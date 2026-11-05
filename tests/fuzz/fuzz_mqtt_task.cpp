// What a broker sends the push task, in an MQTT buffer of any size, after a
// CONNACK and a SUBACK that let it through or that the input makes up.
#include <kii.h>
#include <kii_mqtt_task.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "fuzz_input.h"

namespace {

const int MAX_STATES = 512;

struct Task {
    // The MQTT socket.
    ebisu_socket_t base;
    FuzzSocket socket;
    int states;
    size_t received;
};

ebisu_sock_code_t http_connect(ebisu_socket_t*, const char*, uint16_t) {
    return EBISU_SOCK_FAIL;
}

ebisu_sock_code_t http_send(ebisu_socket_t*, const char*, size_t, size_t*) {
    return EBISU_SOCK_FAIL;
}

ebisu_sock_code_t http_recv(ebisu_socket_t*, char*, size_t, size_t*) {
    return EBISU_SOCK_FAIL;
}

ebisu_sock_code_t mqtt_connect(ebisu_socket_t*, const char*, uint16_t) {
    return EBISU_SOCK_OK;
}

// The task sends each packet whole. A PUBACK of identifier 0 acknowledges a
// message MQTT doesn't allow, which the task is to refuse.
ebisu_sock_code_t mqtt_send(ebisu_socket_t*, const char* buffer, size_t length, size_t* out_sent_length) {
    if (length == 4 && buffer[0] == 0x40 && buffer[2] == 0 && buffer[3] == 0) {
        abort();
    }
    *out_sent_length = length;
    return EBISU_SOCK_OK;
}

ebisu_sock_code_t mqtt_recv(ebisu_socket_t* self, char* buffer, size_t length, size_t* out_actual_length) {
    Task* task = (Task*)self;
    // The task keeps connecting again when a connection ends, so here reading
    // after it closed is no finding: the number of states ends the run.
    task->socket.reads_after_close = 0;
    *out_actual_length = task->socket.read(buffer, length);
    return EBISU_SOCK_OK;
}

ebisu_sock_code_t sock_close(ebisu_socket_t*) {
    return EBISU_SOCK_OK;
}

void delay_ms(ebisu_system_t*, uint32_t) {
}

const ebisu_socket_ops_t HTTP_OPS = { http_connect, http_send, http_recv, sock_close, NULL };
const ebisu_socket_ops_t MQTT_OPS = { mqtt_connect, mqtt_send, mqtt_recv, sock_close, NULL };
const ebisu_system_ops_t SYSTEM_OPS = { NULL, delay_ms, NULL };

bool task_continue(void*, void* userdata) {
    return ++((Task*)userdata)->states < MAX_STATES;
}

// Reads all it is given, for ASan to see the lengths are right. A message the
// task should have refused, or that it gives wrongly, aborts.
void on_push(const kii_mqtt_message_t* message, void* userdata) {
    if (message->topic_length == 0 ||
            strlen(message->topic) != message->topic_length ||
            strpbrk(message->topic, "+#") != NULL ||
            (message->qos == KII_MQTT_QOS_0) != (message->packet_id == 0) ||
            (message->dup && message->qos != KII_MQTT_QOS_1)) {
        abort();
    }
    size_t sum = 0;
    for (size_t i = 0; i < message->payload_length; ++i) {
        sum += (unsigned char)message->payload[i];
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

    Task task = { { &MQTT_OPS }, { (const uint8_t*)stream.data(), stream.size(), max_read, 0 }, 0, 0 };
    ebisu_socket_t http = { &HTTP_OPS };
    ebisu_system_t system = { &SYSTEM_OPS };
    kii_set_http_socket(&kii, &http);
    kii_set_mqtt_socket(&kii, &task.base);
    kii_set_system(&kii, &system);
    kii_set_cb_task_continue(&kii, task_continue, &task);
    kii_set_cb_mqtt_message(&kii, on_push, &task);
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
