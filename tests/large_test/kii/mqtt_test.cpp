// The MQTT push task against Kii Cloud's broker: it connects, subscribes,
// receives what the thing sends to its own topic and straight to itself, stays
// connected while idle
// across receive timeouts and keep-alive pings, and reconnects after losing
// the connection.
//
// The MQTT connection goes through the samples' socket callbacks, the Linux
// sample's or the win32 sample's, so this runs them against the broker too.
//
// A kii_t is not thread-safe, and the push task makes HTTP requests of its own
// through it. So once the task is running, every request this test makes is
// made on the task's thread, by the continue callback, which the task calls
// between its steps, and only while it is ready to receive. tio does the
// same: it posts command results from the push callback.

#include "catch.hpp"
#include <kii.h>
#include "large_test.h"

#ifdef _WIN32
#include "sock_cb_win32.h"
#else
#include "sock_cb_linux.h"
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <string.h>

namespace {

// Seconds. Short, so that the task goes through several receive timeouts and
// keep-alive pings in well under a minute: with these it sends a PINGREQ about
// every 8 seconds while idle.
const unsigned int TO_RECV = 3;
const unsigned int TO_SEND = 10;
const uint32_t KEEP_ALIVE = 10;

struct MqttTest {
    socket_context_t sock;
    std::thread task;

    std::mutex mutex;
    std::condition_variable changed;
    // Guarded by mutex.
    // The task's states, each time it entered one other than the last.
    std::vector<kii_mqtt_task_state> states;
    std::vector<std::string> received;
    // Run on the task's thread, then cleared.
    std::function<void()> step;
    int steps_done = 0;
    // What the last kii_send_push_message() on the task's thread returned.
    kii_code_t send_res = KII_ERR_FAIL;
    int send_status = 0;
    bool stop = false;
    bool exited = false;
    kii_mqtt_task_info exit_info;

    std::atomic<int> connects{0};
    std::atomic<int> closes{0};
    // Makes the next receive fail as a lost connection does.
    std::atomic<bool> fail_next_recv{false};
    // On the task's thread only: whether the last receive was made to fail.
    bool recv_made_to_fail = false;

    int count(kii_mqtt_task_state state) {
        return (int)std::count(states.begin(), states.end(), state);
    }

    // Whether the task has been ready to receive since it last reconnected.
    bool ready_after_reconnect() {
        auto reconnect = std::find(states.rbegin(), states.rend(), KII_MQTT_ST_RECONNECT);
        return reconnect != states.rend() &&
            std::find(states.rbegin(), reconnect, KII_MQTT_ST_RECV_READY) != reconnect;
    }
};

khc_sock_code_t cb_connect(void* ctx, const char* host, uint16_t port)
{
    MqttTest* t = (MqttTest*)ctx;
    ++t->connects;
    return sock_cb_connect(&t->sock, host, port);
}

khc_sock_code_t cb_send(void* ctx, const char* buffer, size_t length,
        size_t* out_sent_length)
{
    MqttTest* t = (MqttTest*)ctx;
    return sock_cb_send(&t->sock, buffer, length, out_sent_length);
}

khc_sock_code_t cb_recv(void* ctx, char* buffer, size_t length_to_read,
        size_t* out_actual_length)
{
    MqttTest* t = (MqttTest*)ctx;
    t->recv_made_to_fail = t->fail_next_recv.exchange(false);
    if (t->recv_made_to_fail) {
        *out_actual_length = 0;
        return KHC_SOCK_FAIL;
    }
    return sock_cb_recv(&t->sock, buffer, length_to_read, out_actual_length);
}

kii_bool_t cb_recv_timed_out(void* ctx)
{
    MqttTest* t = (MqttTest*)ctx;
    if (t->recv_made_to_fail) {
        return KII_FALSE;
    }
    return sock_cb_recv_timed_out(&t->sock);
}

khc_sock_code_t cb_close(void* ctx)
{
    MqttTest* t = (MqttTest*)ctx;
    ++t->closes;
    return sock_cb_close(&t->sock);
}

kii_task_code_t cb_task_create(const char* name, KII_TASK_ENTRY entry,
        void* entry_param, void* userdata)
{
    MqttTest* t = (MqttTest*)userdata;
    t->task = std::thread(entry, entry_param);
    return KII_TASKC_OK;
}

void cb_delay_ms(uint32_t msec, void* userdata)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}

kii_bool_t cb_continue(void* task_info, void* userdata)
{
    MqttTest* t = (MqttTest*)userdata;
    kii_mqtt_task_state state = ((kii_mqtt_task_info*)task_info)->task_state;
    std::function<void()> step;
    {
        std::lock_guard<std::mutex> lock(t->mutex);
        if (t->states.empty() || t->states.back() != state) {
            t->states.push_back(state);
            t->changed.notify_all();
        }
        if (t->stop) {
            return KII_FALSE;
        }
        if (state == KII_MQTT_ST_RECV_READY) {
            step.swap(t->step);
        }
    }
    if (step) {
        step();
        std::lock_guard<std::mutex> lock(t->mutex);
        ++t->steps_done;
        t->changed.notify_all();
    }
    return KII_TRUE;
}

void cb_exit(void* task_info, void* userdata)
{
    MqttTest* t = (MqttTest*)userdata;
    std::lock_guard<std::mutex> lock(t->mutex);
    t->exit_info = *(kii_mqtt_task_info*)task_info;
    t->exited = true;
    t->changed.notify_all();
}

void cb_push(const char* message, size_t message_length, void* userdata)
{
    MqttTest* t = (MqttTest*)userdata;
    std::lock_guard<std::mutex> lock(t->mutex);
    t->received.push_back(std::string(message, message_length));
    t->changed.notify_all();
}

// Waits up to seconds for done(), called with the mutex held.
bool wait_for(MqttTest& t, int seconds, std::function<bool()> done)
{
    std::unique_lock<std::mutex> lock(t.mutex);
    return t.changed.wait_for(lock, std::chrono::seconds(seconds), done);
}

// Runs step on the task's thread the next time it is ready to receive, and
// waits for it to have run. A step that has not started when this gives up is
// dropped, so it never runs after the test has moved on; step must only refer
// to what outlives the task.
bool run_on_task(MqttTest& t, std::function<void()> step)
{
    int done_before;
    {
        std::lock_guard<std::mutex> lock(t.mutex);
        t.step = step;
        done_before = t.steps_done;
    }
    wait_for(t, 60, [&t, done_before] {
        return t.steps_done > done_before || t.exited;
    });
    std::lock_guard<std::mutex> lock(t.mutex);
    t.step = nullptr;
    return t.steps_done > done_before;
}

bool received(MqttTest& t, const std::string& nonce)
{
    return wait_for(t, 30, [&t, &nonce] {
        for (const std::string& message : t.received) {
            if (message.find(nonce) != std::string::npos) {
                return true;
            }
        }
        return false;
    });
}

// Stops the task and waits for it to end, whichever way the test leaves, so
// that it never outlives what it uses.
struct StopTask {
    MqttTest& t;
    ~StopTask() {
        {
            std::lock_guard<std::mutex> lock(t.mutex);
            t.stop = true;
        }
        if (t.task.joinable()) {
            t.task.join();
        }
    }
};

}

TEST_CASE("MQTT push from the broker")
{
    // To Avoid 429 Too Many Requests
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const size_t buff_size = 4096;
    char buff[buff_size];
    char mqtt_buff[2048];
    kii_t kii;
    ebisu::ltest::ssl::SSLData http_ssl_ctx;
    ebisu::ltest::ssl::SSLData unused_mqtt_ssl_ctx;
    jkii_token_t tokens[256];
    jkii_resource_t resource = {tokens, 256};
    kiiltest::init(&kii, buff, buff_size, &http_ssl_ctx, &unused_mqtt_ssl_ctx, &resource);

    MqttTest t;
#ifdef _WIN32
    sock_cb_init_context(&t.sock, TO_RECV, TO_SEND);
#else
    memset(&t.sock, 0, sizeof(t.sock));
    t.sock.to_recv = TO_RECV;
    t.sock.to_send = TO_SEND;
#endif
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii_set_cb_mqtt_sock_connect(&kii, cb_connect, &t);
    kii_set_cb_mqtt_sock_send(&kii, cb_send, &t);
    kii_set_cb_mqtt_sock_recv(&kii, cb_recv, &t);
    kii_set_cb_mqtt_sock_recv_timed_out(&kii, cb_recv_timed_out, &t);
    kii_set_cb_mqtt_sock_close(&kii, cb_close, &t);
    kii_set_mqtt_to_sock_recv(&kii, TO_RECV);
    kii_set_mqtt_to_sock_send(&kii, TO_SEND);
    kii_set_cb_task_create(&kii, cb_task_create, &t);
    kii_set_cb_task_continue(&kii, cb_continue, &t);
    kii_set_cb_task_exit(&kii, cb_exit, &t);
    kii_set_cb_delay_ms(&kii, cb_delay_ms, NULL);

    // A thing of its own, so that no other run, such as the other CI cell's,
    // receives what this one sends.
    std::string vid = "ltest_mqtt_" + std::to_string(kiiltest::current_time());
    REQUIRE( kii_ti_onboard(&kii, vid.c_str(), "1234", NULL, NULL, NULL, NULL) == KII_ERR_OK );

    std::string topic_name = "mqtt-" + std::to_string(kiiltest::current_time());
    kii_topic_t topic;
    topic.scope = KII_SCOPE_THING;
    topic.scope_id = kii._author.author_id;
    topic.topic_name = topic_name.c_str();
    REQUIRE( kii_put_topic(&kii, &topic) == KII_ERR_OK );
    REQUIRE( khc_get_status_code(&kii._khc) == 204 );
    REQUIRE( kii_subscribe_topic(&kii, &topic) == KII_ERR_OK );
    REQUIRE( khc_get_status_code(&kii._khc) == 204 );

    // Sends a message carrying nonce on the task's thread: to the topic, or,
    // with to_thing, straight to the thing, the only one its own token may
    // send to.
    auto send = [&kii, &topic, &t](const std::string& nonce, bool to_thing) {
        std::string message = "{\"data\":{\"nonce\":\"" + nonce + "\"},\"mqtt\":{\"enabled\":true}}";
        REQUIRE( run_on_task(t, [&kii, &topic, &t, message, to_thing] {
            kii_code_t res = to_thing ?
                kii_send_push_message_to_thing(&kii, kii._author.author_id, message.c_str()) :
                kii_send_push_message(&kii, &topic, message.c_str());
            int status = khc_get_status_code(&kii._khc);
            std::lock_guard<std::mutex> lock(t.mutex);
            t.send_res = res;
            t.send_status = status;
        }) );
        std::lock_guard<std::mutex> lock(t.mutex);
        INFO( "status " << t.send_status );
        REQUIRE( t.send_res == KII_ERR_OK );
    };
    const std::string run_id = std::to_string(kiiltest::current_time());

    REQUIRE( kii_start_push_task(&kii, KEEP_ALIVE, cb_push, &t) == KII_ERR_OK );
    StopTask stop_task{t};

    // It connects with the endpoint onboarding returned, and subscribes:
    // onboarding installed push, so the task neither installs it nor fetches
    // the endpoint.
    REQUIRE( wait_for(t, 120, [&t] {
        return t.count(KII_MQTT_ST_RECV_READY) > 0 || t.exited;
    }) );
    {
        std::lock_guard<std::mutex> lock(t.mutex);
        REQUIRE_FALSE( t.exited );
        CHECK( t.count(KII_MQTT_ST_INSTALL_PUSH) == 0 );
        CHECK( t.count(KII_MQTT_ST_GET_ENDPOINT) == 0 );
    }
    CHECK( t.connects.load() == 1 );

    send("first-" + run_id, false);
    REQUIRE( received(t, "first-" + run_id) );

    // Straight to the thing, without the topic.
    send("direct-" + run_id, true);
    REQUIRE( received(t, "direct-" + run_id) );

    // Idle across several receive timeouts and keep-alive intervals. A timeout
    // taken for a lost connection, or a PINGREQ whose PINGRESP was not read,
    // would reconnect.
    std::this_thread::sleep_for(std::chrono::seconds(25));
    send("idle-" + run_id, false);
    REQUIRE( received(t, "idle-" + run_id) );
    {
        std::lock_guard<std::mutex> lock(t.mutex);
        CHECK( t.count(KII_MQTT_ST_SEND_PINGREQ) >= 2 );
        CHECK( t.count(KII_MQTT_ST_RECONNECT) == 0 );
    }
    CHECK( t.connects.load() == 1 );

    // A receive that fails, as on a lost connection, reconnects, and the
    // task subscribes again. Ready again means after the reconnect: a PINGREQ
    // goes back to ready too.
    REQUIRE( run_on_task(t, [&t] { t.fail_next_recv = true; }) );
    REQUIRE( wait_for(t, 60, [&t] {
        return t.ready_after_reconnect() || t.exited;
    }) );
    {
        std::lock_guard<std::mutex> lock(t.mutex);
        REQUIRE_FALSE( t.exited );
    }
    CHECK( t.connects.load() == 2 );
    {
        std::lock_guard<std::mutex> lock(t.mutex);
        CHECK( t.count(KII_MQTT_ST_GET_ENDPOINT) == 0 );
    }
    send("reconnected-" + run_id, false);
    REQUIRE( received(t, "reconnected-" + run_id) );

    // Asked to stop, it ends within a receive timeout, and closes the
    // connection it has open: each of the two has been closed.
    {
        std::lock_guard<std::mutex> lock(t.mutex);
        t.stop = true;
    }
    REQUIRE( wait_for(t, TO_RECV * 3 + 10, [&t] { return t.exited; }) );
    t.task.join();
    {
        std::lock_guard<std::mutex> lock(t.mutex);
        CHECK( t.exit_info.task_state == KII_MQTT_ST_DISCONTINUED );
        CHECK( t.exit_info.error == KII_MQTT_ERR_OK );
    }
    CHECK( t.closes.load() == 2 );

    // The task has ended, so the kii_t is this thread's again.
    CHECK( kii_delete_topic(&kii, &topic) == KII_ERR_OK );
}
