// The MQTT task, started with kii_start_mqtt_task_with_endpoint(), against a
// Mosquitto broker of the run's own, which with_broker.py starts: what Kii
// Cloud's broker can't be made to do, or does only now and then. Topics with
// wildcards, subscriptions the broker refuses, retained messages, messages sent
// again as duplicates, sessions the broker keeps, MQTT 3.1, refused
// credentials, the will, the keep-alive, messages too large for the buffer,
// the broker closing the connection, and messages the task publishes with
// kii_mqtt_publish().
//
// The task connects over the platform's socket, trusting the run's CA:
// platform's Schannel port on Windows, and its OpenSSL port elsewhere. Most
// messages are published by a client of the test's own, below, so that what
// the task receives doesn't depend on what it sends.
//
// Each test case has topics and client identifiers of its own, under one the
// run is named by, so that none sees another's messages or session.

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <kii.h>
#ifdef _WIN32
#include <ebisu_schannel.h>
#include <wincrypt.h>
#else
#include <ebisu_openssl.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <stdlib.h>
#include <string.h>

namespace {

// Seconds. A short receive timeout, so that a task asked to stop ends soon.
const unsigned int TO_RECV = 1;
const unsigned int TO_SEND = 10;
const uint32_t KEEP_ALIVE = 30;

std::string env(const char* name)
{
    const char* value = getenv(name);
    return value != nullptr ? value : "";
}

// The broker, as with_broker.py gives it.
struct Broker {
    std::string host;
    uint16_t port;
    std::string ca;
    std::string username;
    std::string password;
};

const Broker& broker()
{
    static const Broker b = {
        env("EBISU_BROKER_HOST"),
        (uint16_t)atoi(env("EBISU_BROKER_PORT").c_str()),
        env("EBISU_BROKER_CA"),
        env("EBISU_BROKER_USERNAME"),
        env("EBISU_BROKER_PASSWORD"),
    };
    if (b.host.empty() || b.port == 0 || b.ca.empty() || b.username.empty() || b.password.empty()) {
        FAIL( "no broker: run the broker tests with tests/broker_test/with_broker.py" );
    }
    return b;
}

// The platform's socket, trusting the run's CA: platform's Schannel port on
// Windows, and its OpenSSL port elsewhere.
#ifdef _WIN32
typedef ebisu_schannel_socket_t platform_socket_t;

// The Schannel port trusts the roots the Windows root store holds, so the
// run's CA is added to it for the run, and removed afterwards. Only under wine,
// whose prefix the tests run in, never to a real machine's root store. Winsock
// is started too, which has to be before the first connection.
//
// The root store's registry, rather than the system store: under wine 10, a
// certificate deleted through the system store stays in the registry. It is
// closed straight after each change, as under wine 10 the socket sees a change
// only once the store is closed.
struct Trust {
    bool winsock = false;
    std::vector<BYTE> der;
    bool added = false;
    std::string error;

    static HCERTSTORE open_root() {
        return CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_A, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, "ROOT");
    }

    Trust() {
        WSADATA data;
        winsock = WSAStartup(MAKEWORD(2, 2), &data) == 0;
        if (!winsock) {
            error = "WSAStartup failed";
            return;
        }
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (ntdll == nullptr || GetProcAddress(ntdll, "wine_get_version") == nullptr) {
            error = "on Windows, the broker tests run only under wine: they add the run's CA to the root store";
            return;
        }
        std::ifstream file(broker().ca, std::ios::binary);
        const std::string pem((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        DWORD length = 0;
        if (pem.empty() || !CryptStringToBinaryA(pem.c_str(), (DWORD)pem.size(),
                    CRYPT_STRING_BASE64HEADER, nullptr, &length, nullptr, nullptr)) {
            error = "the CA's certificate couldn't be loaded";
            return;
        }
        der.resize(length);
        if (!CryptStringToBinaryA(pem.c_str(), (DWORD)pem.size(),
                    CRYPT_STRING_BASE64HEADER, der.data(), &length, nullptr, nullptr)) {
            error = "the CA's certificate couldn't be loaded";
            return;
        }
        HCERTSTORE store = open_root();
        added = store != nullptr && CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
                der.data(), (DWORD)der.size(), CERT_STORE_ADD_REPLACE_EXISTING, nullptr);
        if (store != nullptr) {
            CertCloseStore(store, 0);
        }
        if (!added) {
            error = "the CA's certificate couldn't be added to the root store";
        }
    }
    ~Trust() {
        if (added) {
            HCERTSTORE store = open_root();
            PCCERT_CONTEXT ours = CertCreateCertificateContext(X509_ASN_ENCODING, der.data(), (DWORD)der.size());
            if (store != nullptr && ours != nullptr) {
                // Freed by deleting it.
                PCCERT_CONTEXT found = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0,
                        CERT_FIND_EXISTING, ours, nullptr);
                if (found != nullptr) {
                    CertDeleteCertificateFromStore(found);
                }
            }
            if (ours != nullptr) {
                CertFreeCertificateContext(ours);
            }
            if (store != nullptr) {
                CertCloseStore(store, 0);
            }
        }
        if (winsock) {
            WSACleanup();
        }
    }
};

void init_platform_socket(platform_socket_t* sock, unsigned int to_recv, unsigned int to_send)
{
    static Trust t;
    if (!t.error.empty()) {
        FAIL( t.error );
    }
    ebisu_schannel_socket_init(sock, to_recv, to_send);
}
#else
typedef ebisu_openssl_socket_t platform_socket_t;

// The TLS configuration every connection shares.
struct Tls {
    ebisu_openssl_tls_t tls;
    bool ok;
    Tls() : ok(ebisu_openssl_tls_init(&tls, broker().ca.c_str(), nullptr)) {}
    ~Tls() {
        if (ok) {
            ebisu_openssl_tls_free(&tls);
        }
    }
};

void init_platform_socket(platform_socket_t* sock, unsigned int to_recv, unsigned int to_send)
{
    static Tls t;
    if (!t.ok) {
        FAIL( "the CA's certificate couldn't be loaded" );
    }
    ebisu_openssl_socket_init(sock, to_recv, to_send);
    ebisu_openssl_socket_set_tls(sock, &t.tls);
}
#endif

// What the test case's topics are under, and its client identifiers start
// with: named by the time the run started.
std::string run_name()
{
    static const std::string name = std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    return name;
}

std::string topic_of(const std::string& test)
{
    return "ebisu/" + run_name() + "/" + test;
}

std::string client_id_of(const std::string& test)
{
    return "ebisu-" + run_name() + "-" + test;
}

// The system the task runs with: a std::thread, given to keep, delays that
// sleep, and a steady clock.
struct ThreadSystem {
    struct Link {
        ebisu_system_t base;
        ThreadSystem* owner;
    } link;
    std::function<void(std::thread)> keep;

    ThreadSystem() {
        static const ebisu_system_ops_t ops = {
            [](ebisu_system_t* self, const char*, EBISU_TASK_ENTRY entry, void* entry_param) {
                ((Link*)self)->owner->keep(std::thread(entry, entry_param));
                return EBISU_TASKC_OK;
            },
            [](ebisu_system_t*, uint32_t msec) {
                std::this_thread::sleep_for(std::chrono::milliseconds(msec));
            },
            [](ebisu_system_t*) {
                return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
            },
        };
        link.base.ops = &ops;
        link.owner = this;
    }
    ThreadSystem(const ThreadSystem&) = delete;
    ThreadSystem& operator=(const ThreadSystem&) = delete;

    ebisu_system_t* get() {
        return &link.base;
    }
};

// The fixed header of a CONNECT, of two bytes for a remaining length below 128,
// the protocol's name, with its length, and its level.
const size_t CONNECT_START = 2 + 2 + 6 + 1;

// What the message callback was given, copied.
struct Received {
    std::string topic;
    size_t topic_length;
    // Whether the topic ended at its terminator.
    bool terminated;
    std::string payload;
    kii_mqtt_qos_t qos;
    bool retain;
    bool dup;
    uint16_t packet_id;
    // How many SUBACKs the task had had when it came.
    int subacks_before;
};

// The task, on a kii_t of its own, subscribing to topic as client_id, with what
// it does recorded.
struct Task {
    platform_socket_t sock;
    // What kii is given: sock, through the callbacks below.
    struct Link {
        ebisu_socket_t base;
        Task* t;
    } link;
    ThreadSystem system;
    std::thread thread;
    std::vector<char> buff;
    const std::string client_id;
    kii_mqtt_endpoint_t endpoint;
    kii_t kii;
    // Asked for new credentials, if set before start().
    std::function<bool(kii_mqtt_endpoint_t* endpoint)> on_credentials;

    std::mutex mutex;
    std::condition_variable changed;
    // Guarded by mutex.
    // The task's states, each time it entered one other than the last.
    std::vector<kii_mqtt_task_state> states;
    std::vector<std::pair<kii_mqtt_event_t, uint32_t>> events;
    std::vector<Received> received;
    int credentials_asked = 0;
    bool stop = false;
    bool exited = false;
    kii_mqtt_task_info exit_info;

    std::atomic<int> connects{0};
    // The start of each CONNECT it sent, up to the protocol's level: the rest
    // carries the password.
    std::vector<std::string> connect_packets;
    // Makes the next receive fail, as a lost connection does.
    std::atomic<bool> fail_next_recv{false};
    // Makes the next PUBACK fail to be sent.
    std::atomic<bool> fail_next_puback{false};
    // Makes the receive after the next PUBLISH at QoS 1 fail, so that the
    // connection is lost before its PUBACK.
    std::atomic<bool> fail_recv_after_publish{false};
    // The first byte of each PUBLISH it sent, with its flags.
    std::vector<unsigned char> publishes;
    // On the task's thread only: whether the last receive was made to fail.
    bool recv_made_to_fail = false;

    // The lock kii_mqtt_publish() shares with the task.
    std::mutex publish_mutex;
    struct LockLink {
        ebisu_lock_t base;
        Task* t;
    } lock_link;

    Task(const std::string& topic, const std::string& client_id, size_t buff_size = 1024,
            unsigned int to_recv = TO_RECV)
        : buff(buff_size), client_id(client_id) {
        const Broker& b = broker();
        init_platform_socket(&sock, to_recv, TO_SEND);
        static const ebisu_socket_ops_t ops = {
            cb_connect, cb_send, cb_recv, cb_close, cb_recv_timed_out, nullptr, cb_recv_wait
        };
        link.base.ops = &ops;
        link.t = this;
        static const ebisu_lock_ops_t lock_ops = {
            [](ebisu_lock_t* self) { ((LockLink*)self)->t->publish_mutex.lock(); },
            [](ebisu_lock_t* self) { ((LockLink*)self)->t->publish_mutex.unlock(); },
        };
        lock_link.base.ops = &lock_ops;
        lock_link.t = this;
        system.keep = [this](std::thread task) { thread = std::move(task); };

        memset(&endpoint, 0, sizeof(endpoint));
        REQUIRE( b.host.size() < sizeof(endpoint.host) );
        REQUIRE( topic.size() < sizeof(endpoint.topic) );
        REQUIRE( b.username.size() < sizeof(endpoint.username) );
        REQUIRE( b.password.size() < sizeof(endpoint.password) );
        strcpy(endpoint.host, b.host.c_str());
        endpoint.port_ssl = b.port;
        strcpy(endpoint.topic, topic.c_str());
        strcpy(endpoint.username, b.username.c_str());
        strcpy(endpoint.password, b.password.c_str());

        kii_init(&kii);
        kii_set_mqtt_buff(&kii, buff.data(), buff.size());
        kii_set_mqtt_socket(&kii, &link.base);
        kii_set_mqtt_to_sock_recv(&kii, to_recv);
        kii_set_system(&kii, system.get());
        // So that a reconnection takes a second or two.
        REQUIRE( kii_set_mqtt_retry_wait_max(&kii, 2) == KII_ERR_OK );
        REQUIRE( kii_set_mqtt_client_id(&kii, this->client_id.c_str()) == KII_ERR_OK );
        kii_set_cb_task_continue(&kii, cb_continue, this);
        kii_set_cb_task_exit(&kii, cb_exit, this);
        kii_set_cb_mqtt_message(&kii, cb_message, this);
        kii_set_cb_mqtt_event(&kii, cb_event, this);
        kii_set_mqtt_lock(&kii, &lock_link.base);
    }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // Ends the task, whichever way the test leaves, so that it never outlives
    // what it uses.
    ~Task() {
        end();
    }

    kii_code_t start(uint32_t keep_alive = KEEP_ALIVE) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = false;
            exited = false;
            states.clear();
        }
        if (on_credentials) {
            kii_set_cb_mqtt_credentials(&kii, cb_credentials, this);
        }
        return kii_start_mqtt_task_with_endpoint(&kii, &endpoint, keep_alive);
    }

    // Asks the task to stop, and waits for it to have ended.
    void end() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
        }
        if (thread.joinable()) {
            thread.join();
        }
    }

    // Waits up to seconds for done(), called with the mutex held.
    bool wait_for(int seconds, std::function<bool()> done) {
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock, std::chrono::seconds(seconds), done);
    }

    int count(kii_mqtt_task_state state) {
        return (int)std::count(states.begin(), states.end(), state);
    }

    int count(kii_mqtt_event_t event) {
        return (int)std::count_if(events.begin(), events.end(),
                [event](const std::pair<kii_mqtt_event_t, uint32_t>& e) {
                    return e.first == event;
                });
    }

    int count(kii_mqtt_event_t event, uint32_t detail) {
        return (int)std::count(events.begin(), events.end(), std::make_pair(event, detail));
    }

    // Waits for the task to be ready to receive, or to end.
    bool wait_ready() {
        return wait_for(30, [this] { return count(KII_MQTT_ST_RECV_READY) > 0 || exited; }) &&
            !exited_now();
    }

    // Waits for the task to be ready to receive again after it last
    // reconnected. A PINGREQ goes back to ready too.
    bool wait_ready_after_reconnect() {
        return wait_for(30, [this] {
            auto reconnect = std::find(states.rbegin(), states.rend(), KII_MQTT_ST_RECONNECT);
            return exited || (reconnect != states.rend() &&
                std::find(states.rbegin(), reconnect, KII_MQTT_ST_RECV_READY) != reconnect);
        }) && !exited_now();
    }

    // Waits for n messages in all to have been received.
    bool wait_received(size_t n) {
        return wait_for(30, [this, n] { return received.size() >= n || exited; }) &&
            !exited_now();
    }

    bool exited_now() {
        std::lock_guard<std::mutex> lock(mutex);
        return exited;
    }

    static Task* of(ebisu_socket_t* self) {
        return ((Link*)self)->t;
    }

    static ebisu_sock_code_t cb_connect(ebisu_socket_t* self, const char* host, uint16_t port) {
        Task* t = of(self);
        ++t->connects;
        return t->sock.base.ops->connect(&t->sock.base, host, port);
    }

    static ebisu_sock_code_t cb_send(ebisu_socket_t* self, const char* buffer, size_t length,
            size_t* out_sent_length) {
        Task* t = of(self);
        // Each packet is sent from its start, so a send starting with a
        // PUBACK's first byte is one.
        if (length > 0 && (unsigned char)buffer[0] == 0x40 && t->fail_next_puback.exchange(false)) {
            *out_sent_length = 0;
            return EBISU_SOCK_FAIL;
        }
        if (length > 0 && (unsigned char)buffer[0] == 0x10) {
            std::lock_guard<std::mutex> lock(t->mutex);
            t->connect_packets.push_back(std::string(buffer, std::min(length, (size_t)CONNECT_START)));
        }
        // The tests' messages fit in the buffer, so a PUBLISH takes one send.
        if (length > 0 && ((unsigned char)buffer[0] & 0xf0) == 0x30) {
            {
                std::lock_guard<std::mutex> lock(t->mutex);
                t->publishes.push_back((unsigned char)buffer[0]);
            }
            if (((unsigned char)buffer[0] & 0x06) == 0x02 && t->fail_recv_after_publish.exchange(false)) {
                t->fail_next_recv = true;
            }
        }
        return t->sock.base.ops->send(&t->sock.base, buffer, length, out_sent_length);
    }

    static ebisu_sock_code_t cb_recv(ebisu_socket_t* self, char* buffer, size_t length_to_read,
            size_t* out_actual_length) {
        Task* t = of(self);
        t->recv_made_to_fail = t->fail_next_recv.exchange(false);
        if (t->recv_made_to_fail) {
            *out_actual_length = 0;
            return EBISU_SOCK_FAIL;
        }
        return t->sock.base.ops->recv(&t->sock.base, buffer, length_to_read, out_actual_length);
    }

    // A receive made to fail is one to make, as a lost connection has a
    // socket's be.
    static bool cb_recv_wait(ebisu_socket_t* self, uint32_t timeout_ms) {
        Task* t = of(self);
        if (t->fail_next_recv) {
            return true;
        }
        return t->sock.base.ops->recv_wait(&t->sock.base, timeout_ms);
    }

    static bool cb_recv_timed_out(ebisu_socket_t* self) {
        Task* t = of(self);
        if (t->recv_made_to_fail) {
            return false;
        }
        return t->sock.base.ops->recv_timed_out(&t->sock.base);
    }

    static ebisu_sock_code_t cb_close(ebisu_socket_t* self) {
        Task* t = of(self);
        return t->sock.base.ops->close(&t->sock.base);
    }

    static bool cb_continue(void* task_info, void* userdata) {
        Task* t = (Task*)userdata;
        kii_mqtt_task_state state = ((kii_mqtt_task_info*)task_info)->task_state;
        std::lock_guard<std::mutex> lock(t->mutex);
        if (t->states.empty() || t->states.back() != state) {
            t->states.push_back(state);
            t->changed.notify_all();
        }
        return !t->stop;
    }

    static void cb_exit(void* task_info, void* userdata) {
        Task* t = (Task*)userdata;
        std::lock_guard<std::mutex> lock(t->mutex);
        t->exit_info = *(kii_mqtt_task_info*)task_info;
        t->exited = true;
        t->changed.notify_all();
    }

    static void cb_message(const kii_mqtt_message_t* message, void* userdata) {
        Task* t = (Task*)userdata;
        std::lock_guard<std::mutex> lock(t->mutex);
        Received r;
        r.topic = message->topic;
        r.topic_length = message->topic_length;
        r.terminated = strlen(message->topic) == message->topic_length;
        r.payload = std::string(message->payload, message->payload_length);
        r.qos = message->qos;
        r.retain = message->retain;
        r.dup = message->dup;
        r.packet_id = message->packet_id;
        r.subacks_before = t->count(KII_MQTT_EVENT_SUBACK);
        t->received.push_back(r);
        t->changed.notify_all();
    }

    static void cb_event(kii_mqtt_event_t event, uint32_t detail, void* userdata) {
        Task* t = (Task*)userdata;
        std::lock_guard<std::mutex> lock(t->mutex);
        t->events.push_back(std::make_pair(event, detail));
        t->changed.notify_all();
    }

    static bool cb_credentials(kii_mqtt_endpoint_t* endpoint, void* userdata) {
        Task* t = (Task*)userdata;
        {
            std::lock_guard<std::mutex> lock(t->mutex);
            ++t->credentials_asked;
        }
        return t->on_credentials(endpoint);
    }
};

// A client of the test's own, for what the task doesn't do: it publishes, and
// it connects as another client would.
struct Client {
    platform_socket_t sock;
    bool connected = false;
    uint16_t next_id = 1;

    Client() {
        init_platform_socket(&sock, 10, TO_SEND);
    }
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    ~Client() {
        if (connected) {
            sock.base.ops->close(&sock.base);
        }
    }

    static std::string with_length(const std::string& s) {
        return std::string(1, (char)(s.size() >> 8)) + (char)(s.size() & 0xff) + s;
    }

    static std::string packet(unsigned char byte1, const std::string& rest) {
        std::string p(1, (char)byte1);
        size_t length = rest.size();
        do {
            unsigned char digit = length % 128;
            length /= 128;
            p += (char)(length > 0 ? digit | 0x80 : digit);
        } while (length > 0);
        return p + rest;
    }

    bool send(const std::string& data) {
        size_t sent = 0;
        while (sent < data.size()) {
            size_t n = 0;
            if (sock.base.ops->send(&sock.base, data.data() + sent, data.size() - sent, &n) != EBISU_SOCK_OK) {
                return false;
            }
            sent += n;
        }
        return true;
    }

    bool recv(char* buffer, size_t length) {
        size_t got = 0;
        while (got < length) {
            size_t n = 0;
            if (sock.base.ops->recv(&sock.base, buffer + got, length - got, &n) != EBISU_SOCK_OK || n == 0) {
                return false;
            }
            got += n;
        }
        return true;
    }

    // Connects as client_id, asking for a clean session. CONNACK's return
    // code, or -1 if there was none.
    int connect(const std::string& client_id) {
        const Broker& b = broker();
        if (sock.base.ops->connect(&sock.base, b.host.c_str(), b.port) != EBISU_SOCK_OK) {
            return -1;
        }
        connected = true;
        // MQTT 3.1.1, with a user name, a password and a clean session, and a
        // keep-alive longer than any test case.
        std::string rest = with_length("MQTT") + '\x04' + '\xc2' + '\x02' + '\x58' +
            with_length(client_id) + with_length(b.username) + with_length(b.password);
        char connack[4];
        if (!send(packet(0x10, rest)) || !recv(connack, sizeof(connack)) ||
                connack[0] != 0x20 || connack[1] != 0x02) {
            return -1;
        }
        return (unsigned char)connack[3];
    }

    // Publishes, and at QoS 1 waits for the broker's PUBACK.
    bool publish(const std::string& topic, const std::string& payload, int qos = 0, bool retain = false) {
        std::string rest = with_length(topic);
        uint16_t id = 0;
        if (qos == 1) {
            id = next_id++;
            rest += std::string(1, (char)(id >> 8)) + (char)(id & 0xff);
        }
        rest += payload;
        if (!send(packet(0x30 | (qos << 1) | (retain ? 0x01 : 0x00), rest))) {
            return false;
        }
        if (qos == 0) {
            return true;
        }
        char puback[4];
        return recv(puback, sizeof(puback)) && (unsigned char)puback[0] == 0x40 &&
            puback[1] == 0x02 && (uint16_t)((unsigned char)puback[2] * 256 + (unsigned char)puback[3]) == id;
    }

    void disconnect() {
        send(std::string("\xe0\x00", 2));
        sock.base.ops->close(&sock.base);
        connected = false;
    }
};

// A publisher, connected.
struct Publisher : Client {
    explicit Publisher(const std::string& test) {
        REQUIRE( connect(client_id_of(test) + "-publisher") == 0 );
    }
};

void check_message(const Received& m, const std::string& topic, const std::string& payload,
        kii_mqtt_qos_t qos, bool retain, bool dup)
{
    CHECK( m.topic == topic );
    CHECK( m.topic_length == topic.size() );
    CHECK( m.terminated );
    CHECK( m.payload == payload );
    CHECK( m.qos == qos );
    CHECK( m.retain == retain );
    CHECK( m.dup == dup );
    if (qos == KII_MQTT_QOS_0) {
        CHECK( m.packet_id == 0 );
    } else {
        CHECK( m.packet_id != 0 );
    }
}

}

TEST_CASE("A subscription with wildcards gets each message with its topic")
{
    const std::string base = topic_of("wildcards");
    Task t(base + "/#", client_id_of("wildcards"));
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );

    Publisher p("wildcards");
    const std::string binary("\0\x01\xff#+/", 6);
    REQUIRE( p.publish(base + "/a", "one") );
    REQUIRE( p.publish(base + "/b/c", "two") );
    // The parent level, which a filter ending in /# matches too.
    REQUIRE( p.publish(base, "parent") );
    REQUIRE( p.publish(topic_of("elsewhere") + "/a", "not for it") );
    REQUIRE( p.publish(base + "/\xc3\xb1/\xe6\x97\xa5\xe6\x9c\xac", binary) );
    REQUIRE( p.publish(base + "/empty", "") );

    // One publisher's messages come in the order it published them, so the
    // one on another topic would have come before the last.
    REQUIRE( t.wait_received(5) );
    std::lock_guard<std::mutex> lock(t.mutex);
    REQUIRE( t.received.size() == 5 );
    check_message(t.received[0], base + "/a", "one", KII_MQTT_QOS_0, false, false);
    check_message(t.received[1], base + "/b/c", "two", KII_MQTT_QOS_0, false, false);
    check_message(t.received[2], base, "parent", KII_MQTT_QOS_0, false, false);
    check_message(t.received[3], base + "/\xc3\xb1/\xe6\x97\xa5\xe6\x9c\xac", binary,
            KII_MQTT_QOS_0, false, false);
    check_message(t.received[4], base + "/empty", "", KII_MQTT_QOS_0, false, false);
    CHECK( t.count(KII_MQTT_EVENT_CONNACK, 0) == 1 );
    CHECK( t.count(KII_MQTT_EVENT_SUBACK, 0) == 1 );
}

TEST_CASE("A subscription the broker refuses is tried again, until the broker grants it")
{
    // Under ebisu/+/refused, which with_broker.py has the broker refuse.
    const std::string topic = topic_of("refused");
    Task t(topic, client_id_of("refused"));
    REQUIRE( t.start() == KII_ERR_OK );
    // The task reconnects and subscribes again each time.
    REQUIRE( t.wait_for(30, [&t] { return t.count(KII_MQTT_EVENT_SUBACK, 0x80) >= 2 || t.exited; }) );
    {
        std::lock_guard<std::mutex> lock(t.mutex);
        CHECK( !t.exited );
        CHECK( t.count(KII_MQTT_EVENT_SUBACK) == t.count(KII_MQTT_EVENT_SUBACK, 0x80) );
        CHECK( t.count(KII_MQTT_EVENT_CONNACK, 0) >= 2 );
        CHECK( t.count(KII_MQTT_ST_RECV_READY) == 0 );
    }

    // Told to, through its dynamic security plugin's topic, the broker grants
    // the next SUBSCRIBE. It closes the connections of the user's clients as it
    // does, the administrator's too, so the message is published by another,
    // connected once the task's subscription is granted.
    {
        Publisher admin("refused-admin");
        REQUIRE( admin.publish("$CONTROL/dynamic-security/v1",
                "{\"commands\":[{\"command\":\"addRoleACL\",\"rolename\":\"tests\","
                "\"acltype\":\"subscribePattern\",\"topic\":\"" + topic + "\","
                "\"priority\":20,\"allow\":true}]}", 1) );
    }
    REQUIRE( t.wait_ready() );
    Publisher p("refused");
    REQUIRE( p.publish(topic, "granted") );
    REQUIRE( t.wait_received(1) );
    std::lock_guard<std::mutex> lock(t.mutex);
    check_message(t.received[0], topic, "granted", KII_MQTT_QOS_0, false, false);
    CHECK( t.count(KII_MQTT_EVENT_SUBACK, 0) == 1 );
    CHECK( t.count(KII_MQTT_EVENT_SUBACK, 0x80) >= 2 );
    CHECK( t.connects.load() >= 3 );
}

TEST_CASE("A retained message is flagged when it comes with the subscription, and only then")
{
    const std::string topic = topic_of("retained");
    Publisher p("retained");
    // At QoS 1, so that the broker has it once it is acknowledged.
    REQUIRE( p.publish(topic, "before", 1, true) );

    Task t(topic, client_id_of("retained"));
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_received(1) );
    // Published with the flag, as the one the broker keeps, but sent as it
    // was published, to a subscription made already.
    REQUIRE( p.publish(topic, "after", 1, true) );
    // An empty one, which the broker keeps no more, is sent all the same.
    REQUIRE( p.publish(topic, "", 1, true) );
    REQUIRE( t.wait_received(3) );
    {
        std::lock_guard<std::mutex> lock(t.mutex);
        // At the subscription's QoS, the lower.
        check_message(t.received[0], topic, "before", KII_MQTT_QOS_0, true, false);
        check_message(t.received[1], topic, "after", KII_MQTT_QOS_0, false, false);
        check_message(t.received[2], topic, "", KII_MQTT_QOS_0, false, false);
    }

    // Subscribed again, it gets no message the broker no longer keeps.
    t.end();
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );
    REQUIRE( p.publish(topic, "live") );
    REQUIRE( t.wait_received(4) );
    std::lock_guard<std::mutex> lock(t.mutex);
    REQUIRE( t.received.size() == 4 );
    check_message(t.received[3], topic, "live", KII_MQTT_QOS_0, false, false);
}

TEST_CASE("At QoS 1, with a session the broker keeps, what is sent while the task is away arrives when it is back")
{
    const std::string topic = topic_of("session");
    Task t(topic, client_id_of("session"));
    kii_set_mqtt_qos(&t.kii, KII_MQTT_QOS_1);
    kii_set_mqtt_clean_session(&t.kii, false);
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );
    {
        std::lock_guard<std::mutex> lock(t.mutex);
        CHECK( t.count(KII_MQTT_EVENT_SUBACK, 1) == 1 );
    }

    Publisher p("session");
    REQUIRE( p.publish(topic, "present", 1) );
    REQUIRE( t.wait_received(1) );

    // Stopped, the task ends its connection with DISCONNECT, and the broker
    // keeps its session.
    t.end();
    REQUIRE( p.publish(topic, "away", 1) );
    // Not kept for a session at QoS 0.
    REQUIRE( p.publish(topic, "away at QoS 0") );
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_received(2) );
    REQUIRE( t.wait_ready() );
    // Time for anything else the broker kept to come: the first had been
    // acknowledged.
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::lock_guard<std::mutex> lock(t.mutex);
    REQUIRE( t.received.size() == 2 );
    check_message(t.received[0], topic, "present", KII_MQTT_QOS_1, false, false);
    check_message(t.received[1], topic, "away", KII_MQTT_QOS_1, false, false);
    // The broker sends what it kept as soon as it has accepted the connection,
    // before it answers the task's SUBSCRIBE.
    CHECK( t.received[1].subacks_before == 1 );
    CHECK( t.connects.load() == 2 );
}

TEST_CASE("A message the task didn't acknowledge is sent again, as a duplicate")
{
    const std::string topic = topic_of("duplicate");
    Task t(topic, client_id_of("duplicate"));
    kii_set_mqtt_qos(&t.kii, KII_MQTT_QOS_1);
    kii_set_mqtt_clean_session(&t.kii, false);
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );

    // The connection is lost as the task acknowledges it: it reconnects, and
    // the broker, which kept the session, sends the message again.
    t.fail_next_puback = true;
    Publisher p("duplicate");
    REQUIRE( p.publish(topic, "again", 1) );
    REQUIRE( t.wait_received(2) );
    REQUIRE( t.wait_ready_after_reconnect() );
    // Acknowledged the second time, it isn't sent a third.
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::lock_guard<std::mutex> lock(t.mutex);
    REQUIRE( t.received.size() == 2 );
    check_message(t.received[0], topic, "again", KII_MQTT_QOS_1, false, false);
    check_message(t.received[1], topic, "again", KII_MQTT_QOS_1, false, true);
    CHECK( t.received[1].packet_id == t.received[0].packet_id );
    CHECK( t.count(KII_MQTT_EVENT_SEND_FAILED, KII_MQTT_ST_RECV_MSG) == 1 );
    CHECK( t.connects.load() == 2 );
}

TEST_CASE("The task speaks MQTT 3.1 when set to")
{
    const std::string topic = topic_of("mqtt31");
    Task t(topic, client_id_of("mqtt31"));
    kii_set_mqtt_version(&t.kii, KII_MQTT_VERSION_3_1);
    kii_set_mqtt_qos(&t.kii, KII_MQTT_QOS_1);
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );
    Publisher p("mqtt31");
    REQUIRE( p.publish(topic, "three one", 1) );
    REQUIRE( t.wait_received(1) );

    std::lock_guard<std::mutex> lock(t.mutex);
    check_message(t.received[0], topic, "three one", KII_MQTT_QOS_1, false, false);
    CHECK( t.count(KII_MQTT_EVENT_CONNACK, 0) == 1 );
    // The protocol's name, MQIsdp, and its level, 3.
    REQUIRE( t.connect_packets.size() == 1 );
    CHECK( t.connect_packets[0].substr(2) == std::string("\x00\x06MQIsdp\x03", 9) );
}

TEST_CASE("When the broker refuses the credentials, the task asks for others")
{
    const std::string topic = topic_of("credentials");
    Task t(topic, client_id_of("credentials"));
    strcpy(t.endpoint.password, "not-the-password");

    SECTION("and connects with those it is given") {
        const std::string username = broker().username;
        const std::string password = broker().password;
        t.on_credentials = [&username, &password](kii_mqtt_endpoint_t* endpoint) {
            // Given what it connected with, to change.
            CHECK( std::string(endpoint->username) == username );
            CHECK( std::string(endpoint->topic) == topic_of("credentials") );
            strcpy(endpoint->password, password.c_str());
            return true;
        };
        REQUIRE( t.start() == KII_ERR_OK );
        REQUIRE( t.wait_ready() );
        Publisher p("credentials");
        REQUIRE( p.publish(topic, "let in") );
        REQUIRE( t.wait_received(1) );

        std::lock_guard<std::mutex> lock(t.mutex);
        check_message(t.received[0], topic, "let in", KII_MQTT_QOS_0, false, false);
        CHECK( t.credentials_asked == 1 );
        // Mosquitto refuses a password as not authorized.
        CHECK( t.count(KII_MQTT_EVENT_CONNACK, 5) == 1 );
        CHECK( t.count(KII_MQTT_EVENT_CONNACK, 0) == 1 );
        CHECK( t.connects.load() == 2 );
    }

    SECTION("and ends if it is given none") {
        t.on_credentials = [](kii_mqtt_endpoint_t*) { return false; };
        REQUIRE( t.start() == KII_ERR_OK );
        REQUIRE( t.wait_for(30, [&t] { return t.exited; }) );

        std::lock_guard<std::mutex> lock(t.mutex);
        CHECK( t.credentials_asked == 1 );
        CHECK( t.exit_info.task_state == KII_MQTT_ST_ERR_EXIT );
        CHECK( t.exit_info.error == KII_MQTT_ERR_CREDENTIALS );
        CHECK( t.count(KII_MQTT_EVENT_CONNACK, 5) == 1 );
        CHECK( t.connects.load() == 1 );
    }

    SECTION("and ends without a callback to ask") {
        REQUIRE( t.start() == KII_ERR_OK );
        REQUIRE( t.wait_for(30, [&t] { return t.exited; }) );

        std::lock_guard<std::mutex> lock(t.mutex);
        CHECK( t.exit_info.task_state == KII_MQTT_ST_ERR_EXIT );
        CHECK( t.exit_info.error == KII_MQTT_ERR_CREDENTIALS );
        CHECK( t.connects.load() == 1 );
    }
}

TEST_CASE("The broker publishes the will when the connection is lost, not when the task stops")
{
    const std::string will_topic = topic_of("will") + "/gone";
    Task watcher(will_topic, client_id_of("will-watcher"));
    REQUIRE( watcher.start() == KII_ERR_OK );
    REQUIRE( watcher.wait_ready() );

    kii_mqtt_will_t will;
    will.topic = will_topic.c_str();
    will.message = "gone";
    will.message_length = 4;
    will.qos = KII_MQTT_QOS_1;
    will.retain = false;
    Task t(topic_of("will") + "/in", client_id_of("will"));
    REQUIRE( kii_set_mqtt_will(&t.kii, &will) == KII_ERR_OK );
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );

    // A receive that fails as on a lost connection: the task closes it, without
    // DISCONNECT, and reconnects.
    t.fail_next_recv = true;
    REQUIRE( watcher.wait_received(1) );
    REQUIRE( t.wait_ready_after_reconnect() );
    {
        std::lock_guard<std::mutex> lock(watcher.mutex);
        check_message(watcher.received[0], will_topic, "gone", KII_MQTT_QOS_0, false, false);
    }
    CHECK( t.connects.load() == 2 );

    // Stopped, it says DISCONNECT.
    t.end();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::lock_guard<std::mutex> lock(watcher.mutex);
    CHECK( watcher.received.size() == 1 );
}

TEST_CASE("Idle, the task keeps its connection with PINGREQ")
{
    // The broker closes a connection that is silent for one and a half times
    // the keep-alive: here 4.5 seconds.
    const uint32_t keep_alive = 3;
    const std::string topic = topic_of("keepalive");
    Task t(topic, client_id_of("keepalive"));
    REQUIRE( t.start(keep_alive) == KII_ERR_OK );
    REQUIRE( t.wait_ready() );
    std::this_thread::sleep_for(std::chrono::seconds(4 * keep_alive));

    Publisher p("keepalive");
    REQUIRE( p.publish(topic, "still there") );
    REQUIRE( t.wait_received(1) );
    std::lock_guard<std::mutex> lock(t.mutex);
    CHECK( t.count(KII_MQTT_ST_SEND_PINGREQ) >= 3 );
    CHECK( t.count(KII_MQTT_ST_RECONNECT) == 0 );
    CHECK( t.events.size() == 2 );
    CHECK( t.connects.load() == 1 );
}

TEST_CASE("A message too large for the buffer is dropped, and acknowledged")
{
    const std::string topic = topic_of("large");
    const size_t buff_size = 200;
    Task t(topic, client_id_of("large"), buff_size);
    kii_set_mqtt_qos(&t.kii, KII_MQTT_QOS_1);
    kii_set_mqtt_clean_session(&t.kii, false);
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );

    // After the fixed header, the topic, with its length, the packet
    // identifier, and the payload.
    const size_t fits = buff_size - 2 - topic.size() - 2;
    Publisher p("large");
    REQUIRE( p.publish(topic, std::string(fits + 1, 'o'), 1) );
    REQUIRE( p.publish(topic, std::string(1000, 'x'), 1) );
    REQUIRE( p.publish(topic, std::string(fits, 'f'), 1) );
    REQUIRE( t.wait_received(1) );
    {
        std::lock_guard<std::mutex> lock(t.mutex);
        REQUIRE( t.received.size() == 1 );
        check_message(t.received[0], topic, std::string(fits, 'f'), KII_MQTT_QOS_1, false, false);
        CHECK( t.count(KII_MQTT_EVENT_MESSAGE_TOO_LARGE) == 2 );
        CHECK( t.count(KII_MQTT_EVENT_MESSAGE_TOO_LARGE, (uint32_t)buff_size + 1) == 1 );
        CHECK( t.count(KII_MQTT_EVENT_MESSAGE_TOO_LARGE, (uint32_t)(2 + topic.size() + 2 + 1000)) == 1 );
    }

    // Reconnected, the task isn't sent them again: they were acknowledged.
    t.fail_next_recv = true;
    REQUIRE( t.wait_ready_after_reconnect() );
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::lock_guard<std::mutex> lock(t.mutex);
    CHECK( t.received.size() == 1 );
    CHECK( t.count(KII_MQTT_EVENT_MESSAGE_TOO_LARGE) == 2 );
}

TEST_CASE("A client taking the task's identifier closes the task's connection, and the task takes it back")
{
    const std::string topic = topic_of("takeover");
    Task t(topic, client_id_of("takeover"));
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );

    // The broker closes the connection of the client it had with the
    // identifier, which is the task's, and the task reconnects, closing the
    // other's in turn.
    Client other;
    REQUIRE( other.connect(client_id_of("takeover")) == 0 );
    REQUIRE( t.wait_for(30, [&t] { return t.count(KII_MQTT_EVENT_CLOSED_BY_BROKER) > 0 || t.exited; }) );
    REQUIRE( t.wait_ready_after_reconnect() );

    Publisher p("takeover");
    REQUIRE( p.publish(topic, "back") );
    REQUIRE( t.wait_received(1) );
    std::lock_guard<std::mutex> lock(t.mutex);
    check_message(t.received[0], topic, "back", KII_MQTT_QOS_0, false, false);
    CHECK( t.count(KII_MQTT_EVENT_CLOSED_BY_BROKER) == 1 );
    CHECK( t.connects.load() == 2 );
}

TEST_CASE("The task publishes at QoS 0 and 1, with MQTT 3.1.1 and 3.1")
{
    const std::string base = topic_of("publish");
    Task sub(base + "/out/#", client_id_of("publish-sub"));
    kii_set_mqtt_qos(&sub.kii, KII_MQTT_QOS_1);
    REQUIRE( sub.start() == KII_ERR_OK );
    REQUIRE( sub.wait_ready() );

    Task t(base + "/in", client_id_of("publish"));
    std::string protocol;
    SECTION("MQTT 3.1.1") {
        protocol = std::string("\x00\x04MQTT\x04", 7);
    }
    SECTION("MQTT 3.1") {
        kii_set_mqtt_version(&t.kii, KII_MQTT_VERSION_3_1);
        protocol = std::string("\x00\x06MQIsdp\x03", 9);
    }
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );

    const std::string binary("\0\x01\xff", 3);
    CHECK( kii_mqtt_publish(&t.kii, (base + "/out/zero").c_str(), "zero", 4, KII_MQTT_QOS_0, false, 10000) == KII_ERR_OK );
    CHECK( kii_mqtt_publish(&t.kii, (base + "/out/one").c_str(), "one", 3, KII_MQTT_QOS_1, false, 10000) == KII_ERR_OK );
    CHECK( kii_mqtt_publish(&t.kii, (base + "/out/binary").c_str(), binary.data(), binary.size(), KII_MQTT_QOS_1, false, 10000) == KII_ERR_OK );
    CHECK( kii_mqtt_publish(&t.kii, (base + "/out/empty").c_str(), nullptr, 0, KII_MQTT_QOS_0, false, 10000) == KII_ERR_OK );
    // What it publishes to its own subscription comes back to it, as anyone's
    // would.
    CHECK( kii_mqtt_publish(&t.kii, (base + "/in").c_str(), "self", 4, KII_MQTT_QOS_0, false, 10000) == KII_ERR_OK );

    REQUIRE( sub.wait_received(4) );
    REQUIRE( t.wait_received(1) );
    {
        std::lock_guard<std::mutex> lock(sub.mutex);
        REQUIRE( sub.received.size() == 4 );
        // At the lower of the two QoS, the publisher's and the subscription's.
        check_message(sub.received[0], base + "/out/zero", "zero", KII_MQTT_QOS_0, false, false);
        check_message(sub.received[1], base + "/out/one", "one", KII_MQTT_QOS_1, false, false);
        check_message(sub.received[2], base + "/out/binary", binary, KII_MQTT_QOS_1, false, false);
        check_message(sub.received[3], base + "/out/empty", "", KII_MQTT_QOS_0, false, false);
    }
    std::lock_guard<std::mutex> lock(t.mutex);
    check_message(t.received[0], base + "/in", "self", KII_MQTT_QOS_0, false, false);
    CHECK( t.publishes == std::vector<unsigned char>({ 0x30, 0x32, 0x32, 0x30, 0x30 }) );
    REQUIRE( t.connect_packets.size() == 1 );
    CHECK( t.connect_packets[0].compare(2, protocol.size(), protocol) == 0 );
    CHECK( t.count(KII_MQTT_ST_RECONNECT) == 0 );
    CHECK( t.connects.load() == 1 );
}

TEST_CASE("A retained message the task publishes is kept for those who subscribe later")
{
    const std::string topic = topic_of("publish-retained");
    Task t(topic + "/in", client_id_of("publish-retained"));
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );
    REQUIRE( kii_mqtt_publish(&t.kii, topic.c_str(), "kept", 4, KII_MQTT_QOS_1, true, 10000) == KII_ERR_OK );

    Task later(topic, client_id_of("publish-retained-later"));
    REQUIRE( later.start() == KII_ERR_OK );
    REQUIRE( later.wait_received(1) );
    {
        std::lock_guard<std::mutex> lock(later.mutex);
        check_message(later.received[0], topic, "kept", KII_MQTT_QOS_0, true, false);
    }
    // An empty one, which has the broker keep it no more.
    REQUIRE( kii_mqtt_publish(&t.kii, topic.c_str(), nullptr, 0, KII_MQTT_QOS_1, true, 10000) == KII_ERR_OK );
    std::lock_guard<std::mutex> lock(t.mutex);
    CHECK( t.publishes == std::vector<unsigned char>({ 0x33, 0x33 }) );
}

TEST_CASE("Messages published from several threads at once are each sent once")
{
    const std::string base = topic_of("publish-threads");
    Task sub(base + "/out/#", client_id_of("publish-threads-sub"));
    kii_set_mqtt_qos(&sub.kii, KII_MQTT_QOS_1);
    REQUIRE( sub.start() == KII_ERR_OK );
    REQUIRE( sub.wait_ready() );
    Task t(base + "/in", client_id_of("publish-threads"));
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );

    const int threads = 3;
    const int each = 3;
    std::vector<std::string> expected;
    std::vector<kii_code_t> results(threads * each, KII_ERR_FAIL);
    std::vector<std::thread> publishers;
    for (int i = 0; i < threads; ++i) {
        for (int j = 0; j < each; ++j) {
            expected.push_back(std::to_string(i) + "-" + std::to_string(j));
        }
        publishers.emplace_back([&t, &base, &results, i] {
            for (int j = 0; j < each; ++j) {
                const std::string payload = std::to_string(i) + "-" + std::to_string(j);
                // Each waits for the others' before it, one at a time.
                results[i * each + j] = kii_mqtt_publish(&t.kii, (base + "/out/" + std::to_string(i)).c_str(),
                        payload.data(), payload.size(), KII_MQTT_QOS_1, false, 60000);
            }
        });
    }
    for (std::thread& p : publishers) {
        p.join();
    }
    CHECK( std::count(results.begin(), results.end(), KII_ERR_OK) == threads * each );

    REQUIRE( sub.wait_received(threads * each) );
    // Time for anything sent twice to come.
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::vector<std::string> got;
    {
        std::lock_guard<std::mutex> lock(sub.mutex);
        for (const Received& r : sub.received) {
            got.push_back(r.payload);
            // Each thread's in the order it published them.
            CHECK( r.topic == base + "/out/" + r.payload.substr(0, 1) );
        }
    }
    std::sort(got.begin(), got.end());
    std::sort(expected.begin(), expected.end());
    CHECK( got == expected );
    std::lock_guard<std::mutex> lock(t.mutex);
    CHECK( t.publishes == std::vector<unsigned char>(threads * each, 0x32) );
    CHECK( t.count(KII_MQTT_ST_RECONNECT) == 0 );
}

TEST_CASE("A message at QoS 1 whose PUBACK the lost connection took is sent again, with DUP")
{
    const std::string base = topic_of("publish-again");
    Task sub(base + "/out", client_id_of("publish-again-sub"));
    kii_set_mqtt_qos(&sub.kii, KII_MQTT_QOS_1);
    REQUIRE( sub.start() == KII_ERR_OK );
    REQUIRE( sub.wait_ready() );
    Task t(base + "/in", client_id_of("publish-again"));
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );

    t.fail_recv_after_publish = true;
    REQUIRE( kii_mqtt_publish(&t.kii, (base + "/out").c_str(), "again", 5, KII_MQTT_QOS_1, false, 30000) == KII_ERR_OK );
    {
        std::lock_guard<std::mutex> lock(t.mutex);
        CHECK( t.publishes == std::vector<unsigned char>({ 0x32, 0x3a }) );
        CHECK( t.count(KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_READY) == 1 );
        CHECK( t.connects.load() == 2 );
    }
    // The broker had it before the connection was lost, and sends it on as
    // often as it gets it.
    REQUIRE( sub.wait_received(1) );
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::lock_guard<std::mutex> lock(sub.mutex);
    CHECK( sub.received.size() <= 2 );
    for (const Received& r : sub.received) {
        check_message(r, base + "/out", "again", KII_MQTT_QOS_1, false, false);
    }
}

TEST_CASE("A message the broker doesn't let the task publish is acknowledged all the same, and dropped")
{
    // Outside ebisu/, where with_broker.py's user may not publish.
    const std::string topic = "elsewhere/" + run_name();
    Task t(topic_of("publish-denied") + "/in", client_id_of("publish-denied"));
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );
    CHECK( kii_mqtt_publish(&t.kii, topic.c_str(), "no", 2, KII_MQTT_QOS_1, false, 10000) == KII_ERR_OK );
    CHECK( kii_mqtt_publish(&t.kii, topic.c_str(), "no", 2, KII_MQTT_QOS_0, false, 10000) == KII_ERR_OK );
    std::lock_guard<std::mutex> lock(t.mutex);
    CHECK( t.count(KII_MQTT_ST_RECONNECT) == 0 );
    CHECK( t.connects.load() == 1 );
}

TEST_CASE("Without a task running, a message isn't published")
{
    Task t(topic_of("publish-stopped"), client_id_of("publish-stopped"));
    CHECK( kii_mqtt_publish(&t.kii, topic_of("publish-stopped").c_str(), "x", 1, KII_MQTT_QOS_0, false, 1000) == KII_ERR_FAIL );
    REQUIRE( t.start() == KII_ERR_OK );
    REQUIRE( t.wait_ready() );
    t.end();
    CHECK( kii_mqtt_publish(&t.kii, topic_of("publish-stopped").c_str(), "x", 1, KII_MQTT_QOS_0, false, 1000) == KII_ERR_FAIL );
    std::lock_guard<std::mutex> lock(t.mutex);
    CHECK( t.publishes.empty() );
}

TEST_CASE("With a long receive timeout, the task publishes what it is given at once")
{
    // A receive with nothing arriving would last 10 seconds, and the message
    // wait for it. The task waits for a packet a little at a time instead.
    const unsigned int to_recv = 10;
    const std::string base = topic_of("publish-soon");
    Task sub(base + "/out", client_id_of("publish-soon-sub"));
    REQUIRE( sub.start() == KII_ERR_OK );
    REQUIRE( sub.wait_ready() );
    Task t(base + "/in", client_id_of("publish-soon"), 1024, to_recv);
    REQUIRE( t.start(60) == KII_ERR_OK );
    REQUIRE( t.wait_ready() );

    for (int i = 0; i < 4; ++i) {
        // Idle, so that the task is waiting.
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        const std::string payload = std::to_string(i);
        const auto start = std::chrono::steady_clock::now();
        CHECK( kii_mqtt_publish(&t.kii, (base + "/out").c_str(), payload.data(), payload.size(),
                    i % 2 == 0 ? KII_MQTT_QOS_0 : KII_MQTT_QOS_1, false, 30000) == KII_ERR_OK );
        const auto took = std::chrono::steady_clock::now() - start;
        INFO( "message " << i << " took " <<
                std::chrono::duration_cast<std::chrono::milliseconds>(took).count() << " ms" );
        CHECK( took < std::chrono::seconds(2) );
    }
    REQUIRE( sub.wait_received(4) );
    std::lock_guard<std::mutex> lock(t.mutex);
    CHECK( t.count(KII_MQTT_ST_RECONNECT) == 0 );
}
