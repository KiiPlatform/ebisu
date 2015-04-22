#include "kii.h"
#include "kii-core/kii.h"
#include "kii-core/kii_mqtt.h"
#include "kii_mqtt.h"

int main() {
    kii_t kii;
    size_t buffer_size = 4096;
    char buffer[buffer_size];

    memset(buffer, 0x00, buffer_size);
    kii_init(&kii, "JP", "9ab34d8b", "7a950d78956ed39f3b0815f0f001b43b");
    kii.http_context.buffer = buffer;
    kii.http_context.buffer_size = buffer_size;

    kiiDev_getToken(&kii, "98477", "1234");
    printf("vendor thing id: %s\n", kii.author.author_id);
    printf("thing token: %s\n", kii.author.access_token);

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

}
