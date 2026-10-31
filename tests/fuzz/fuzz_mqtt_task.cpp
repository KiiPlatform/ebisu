// What a broker sends the push task, in an MQTT buffer of any size, after a
// CONNACK and a SUBACK that let it through or that the input makes up.
#include <kii.h>
#include <kii_mqtt_task.h>
#include <string>
#include <vector>
#include "fuzz_input.h"

namespace {

const int MAX_STATES = 512;

struct Task {
    FuzzSocket socket;
    int states;
    size_t received;
};

khc_sock_code_t http_connect(void*, const char*, uint16_t) {
    return KHC_SOCK_FAIL;
}

khc_sock_code_t mqtt_connect(void*, const char*, uint16_t) {
    return KHC_SOCK_OK;
}

khc_sock_code_t mqtt_send(void*, const char*, size_t length, size_t* out_sent_length) {
    *out_sent_length = length;
    return KHC_SOCK_OK;
}

khc_sock_code_t mqtt_recv(void* context, char* buffer, size_t length, size_t* out_actual_length) {
    Task* task = (Task*)context;
    // The task keeps connecting again when a connection ends, so here reading
    // after it closed is no finding: the number of states ends the run.
    task->socket.reads_after_close = 0;
    *out_actual_length = task->socket.read(buffer, length);
    return KHC_SOCK_OK;
}

khc_sock_code_t sock_close(void*) {
    return KHC_SOCK_OK;
}

void delay_ms(uint32_t, void*) {
}

kii_bool_t task_continue(void*, void* userdata) {
    return ++((Task*)userdata)->states < MAX_STATES ? KII_TRUE : KII_FALSE;
}

// Reads all it is given, for ASan to see the message's length is right.
void on_push(const char* message, size_t length, void* userdata) {
    size_t sum = 0;
    for (size_t i = 0; i < length; ++i) {
        sum += (unsigned char)message[i];
    }
    ((Task*)userdata)->received += sum;
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzInput input = { data, size };
    std::vector<char> http_buff(1024);
    // Exactly this size, so that ASan sees one byte past it.
    std::vector<char> mqtt_buff(32 + input.take() * 4);
    size_t max_read = 1 + input.take();

    std::string stream;
    if (input.take() & 1) {
        // Accepted and subscribed, to reach messages from the first run on.
        stream.assign("\x20\x02\x00\x00\x90\x03\x00\x01\x00", 9);
    }
    stream.append((const char*)input.data, input.size);

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "app");
    kii_set_buff(&kii, http_buff.data(), http_buff.size());
    kii_set_mqtt_buff(&kii, mqtt_buff.data(), mqtt_buff.size());

    Task task = { { (const uint8_t*)stream.data(), stream.size(), max_read, 0 }, 0, 0 };
    kii_set_cb_http_sock_connect(&kii, http_connect, NULL);
    kii_set_cb_http_sock_close(&kii, sock_close, NULL);
    kii_set_cb_mqtt_sock_connect(&kii, mqtt_connect, NULL);
    kii_set_cb_mqtt_sock_send(&kii, mqtt_send, NULL);
    kii_set_cb_mqtt_sock_recv(&kii, mqtt_recv, &task);
    kii_set_cb_mqtt_sock_close(&kii, sock_close, NULL);
    kii_set_cb_delay_ms(&kii, delay_ms, NULL);
    kii_set_cb_task_continue(&kii, task_continue, &task);
    kii._cb_push_received = on_push;
    kii._push_data = &task;
    kii._keep_alive_interval = 60;

    // As onboarding leaves it, so that the task goes straight to the broker.
    strcpy(kii._author.author_id, "th.1");
    strcpy(kii._onboarded_thing_id, "th.1");
    strcpy(kii._onboarded_endpoint.host, "mqtt.example.com");
    strcpy(kii._onboarded_endpoint.username, "user");
    strcpy(kii._onboarded_endpoint.password, "password");
    strcpy(kii._onboarded_endpoint.topic, "topic");
    kii._onboarded_endpoint.port_ssl = 8883;

    mqtt_start_task(&kii);
    return 0;
}
