#include "kii.h"
#include "kii-core/kii.h"
#include "kii-core/kii_mqtt.h"
#include "kii_mqtt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void received_callback(char* buffer, size_t buffer_size) {
    char copy[1024];
    memset(copy, 0x00, sizeof(copy));
    strncpy(copy, buffer, sizeof(copy));
    printf("recieve message\n%s", copy);
}
kii_t kii;
char buffer[4096];
char mqtt_buffer[2048];

int main() {
    size_t buffer_size = 4096;
    size_t mqtt_buffer_size = 2048;
    char installation_id[128];
    kii_topic_t topic;
    char scope_id[128];
    int ret;

    memset(buffer, 0x00, buffer_size);
    memset(mqtt_buffer, 0x00, mqtt_buffer_size);

    kii_init(&kii, "JP", "9ab34d8b", "7a950d78956ed39f3b0815f0f001b43b");
    kii.http_context.buffer = buffer;
    kii.http_context.buffer_size = buffer_size;
    kii.mqtt_buffer = mqtt_buffer;
    kii.mqtt_buffer_size = mqtt_buffer_size;

    ret = kiiDev_getToken(&kii, "98477", "1234");
    assert(ret == 0);
    printf("vendor thing id: %s\n", kii.author.author_id);
    printf("thing token: %s\n", kii.author.access_token);


    memset(&topic, 0x00, sizeof(kii_topic_t));
    memset(scope_id, 0x00, sizeof(scope_id));
    sprintf(scope_id, "VENDOR_THING_ID:%s", kii.author.author_id);

    topic.scope = KII_SCOPE_THING;
    topic.scope_id = scope_id;
    topic.topic_name = "myTopic";

#if 0
    ret = kiiPush_createTopic(&kii, &topic);
    assert(ret == 0);

    ret = kiiPush_subscribeTopic(&kii, &topic);
    assert(ret == 0);
#endif
    ret = kiiPush_init(&kii, 0, 0, received_callback);
    assert(ret == 0);
    while(1) {
        sleep(5);
    }

#if 0
    memset(installation_id, 0x00, 128);
    kiiPush_install(&kii, KII_FALSE, installation_id);
    printf("installation_id: %s\n", installation_id);
    kii_mqtt_endpoint_t endpoint;
    strcpy(endpoint.username, "9ab34d8b-GUYXr9ckHpNBZLLQhUMdsxN");
    strcpy(endpoint.password, "ZsSqnnAREOpcRsqzAfTAVwKikeQFcdCmAZTlcWlISwqFUYTDINlBeLwQSYCrIIKd");
    strcpy(endpoint.topic, "sFJGc3o0fmvSK8d3KRSMYZM");
    strcpy(endpoint.host, "jp-mqtt-0a0bfd4468f3.kii.com");
    endpoint.port_tcp = 1883;
    endpoint.port_ssl = 8883;
    endpoint.ttl = 2147483647;

    kiiMQTT_connect(&kii, &endpoint, 0);
    kiiMQTT_subscribe(&kii, endpoint.topic, QOS0);
    kiiMQTT_pingReq(&kii);
#endif

}
