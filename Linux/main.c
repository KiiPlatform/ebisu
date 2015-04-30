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
    printf("buffer_size: %lu\n", buffer_size);
    printf("recieve message: %s\n", copy);
}

int main() {
    kii_t kii;
    char buffer[4096];
    char mqtt_buffer[2048];
    
    size_t buffer_size = 4096;
    size_t mqtt_buffer_size = 2048;
    char installation_id[128];
    kii_topic_t topic;
    char scope_id[128];
    int ret;
    kii_bucket_t bucket;
	char object_data[512];
	char object_id[KII_OBJECTID_SIZE+1];

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

    /* create object */
    memset(&bucket, 0x00, sizeof(kii_bucket_t));
    bucket.scope = KII_SCOPE_THING;
    memset(scope_id, 0x00, sizeof(scope_id));
    sprintf(scope_id, "VENDOR_THING_ID:%s", kii.author.author_id);
	bucket.scope_id = scope_id;
    bucket.bucket_name = "my_bucket";
	memset(object_data, 0x00, sizeof(object_data));
	strcpy(object_data, "{""score"": 2300, ""name"": ""game1""}");
	memset(object_id, 0x00, sizeof(object_id));
	ret = kiiObj_create(&kii, &bucket, object_data, NULL, object_id);
	assert(ret == 0);

    /* create object with id */
	memset(object_id, 0x00, sizeof(object_id));
	strcpy(object_id, "my_object");
	ret = kiiObj_createWithID(&kii, &bucket, object_id, object_data, NULL);
	assert(ret == 0);

    /* patch object */
	memset(object_data, 0x00, sizeof(object_data));
	strcpy(object_data, "{""score"": 5000}");
	ret = kiiObj_patch(&kii, &bucket, object_id, object_data, NULL);
	assert(ret == 0);

    /* replace object */
	memset(object_data, 0x00, sizeof(object_data));
	strcpy(object_data, "{""score1"": 2000, ""name1"": ""game1""}");
	ret = kiiObj_replace(&kii, &bucket, object_id, object_data, NULL);
	assert(ret == 0);

    /* get object */
	memset(object_data, 0x00, sizeof(object_data));
	ret = kiiObj_get(&kii, &bucket, object_id, object_data, sizeof(object_data));
	assert(ret == 0);
	printf("object_data:%s\n", object_data);

	/* delete object */
	ret = kiiObj_delete(&kii, &bucket, object_id);
	assert(ret == 0);

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
