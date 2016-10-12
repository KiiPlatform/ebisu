#include "wm_include.h"
#include "kii.h"
#include "example.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "kii_core_impl.h"

void received_callback(kii_t* kii, char* buffer, size_t buffer_size) {
    printf("buffer_size: %u\n", buffer_size);
    printf("recieve message: %s\n", buffer);
}

int kiiDemo_test(char *buf)
{
    kii_t kii;
    kii_author_t author;
    char *buffer;
    char *mqtt_buffer;
    size_t buffer_size = EX_BUFFER_SIZE;
    size_t mqtt_buffer_size = EX_MQTT_BUFFER_SIZE;

    kii_topic_t topic;
    char scope_id[128];
    int ret;
    kii_bucket_t bucket;
    char object_data[512];
    char object_id[KII_OBJECTID_SIZE+1];
    char upload_id[128];
    char content_type[128];
    kii_chunk_data_t chunk;
    unsigned int length;
    unsigned int actual_length;
    unsigned int total_length;

    buffer = malloc(EX_BUFFER_SIZE);
	if (buffer == NULL)
	{
	    printf("allocate memory failed\n");
		return -1;
	}

	mqtt_buffer = malloc(EX_MQTT_BUFFER_SIZE);
	if (mqtt_buffer == NULL)
	{
	    printf("allocate memory failed\n");
		free(buffer);
		return -1;
	}
    memset(buffer, 0x00, buffer_size);
    memset(mqtt_buffer, 0x00, mqtt_buffer_size);


    kii_init(&kii, EX_APP_SITE, EX_APP_ID, EX_APP_KEY);
    kii.kii_core.http_context.buffer = buffer;
    kii.kii_core.http_context.buffer_size = buffer_size;
    kii.kii_core.http_context.socket_context.app_context = NULL;
    kii.mqtt_buffer = mqtt_buffer;
    kii.mqtt_buffer_size = mqtt_buffer_size;
    memset(&author, 0x00, sizeof(kii_author_t));
    strncpy(author.author_id, (char*)EX_THING_ID, 128);
    strncpy(author.access_token, (char*)EX_ACCESS_TOKEN, 128);
    kii.kii_core.author = author;

    memset(&bucket, 0x00, sizeof(kii_bucket_t));
    bucket.scope = KII_SCOPE_THING;
    bucket.bucket_name = (char*)EX_BUCKET_NAME;
    memset(scope_id, 0x00, sizeof(scope_id));
    sprintf(scope_id, "VENDOR_THING_ID:%s", EX_AUTH_VENDOR_ID);
    bucket.scope_id = scope_id;

    memset(&topic, 0x00, sizeof(kii_topic_t));
    memset(scope_id, 0x00, sizeof(scope_id));
    sprintf(scope_id, "VENDOR_THING_ID:%s", EX_AUTH_VENDOR_ID);

    topic.scope = KII_SCOPE_THING;
    topic.scope_id = scope_id;
    topic.topic_name = (char*)EX_TOPIC_NAME;

    printf("register\n");
    ret = kii_thing_register(&kii, EX_AUTH_VENDOR_ID, 
            EX_AUTH_VENDOR_TYPE, EX_AUTH_VENDOR_PASS);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("authentication\n");
    ret = kii_thing_authenticate(&kii, EX_AUTH_VENDOR_ID, EX_AUTH_VENDOR_PASS);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("create new object\n");
    memset(object_id, 0x00, sizeof(object_id));
    ret = kii_object_create(&kii, &bucket, EX_OBJECT_DATA, NULL, object_id);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("create new object with id\n");
    ret = kii_object_create_with_id(&kii, &bucket, EX_OBJECT_ID, EX_OBJECT_DATA, NULL);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("patch object\n");
    ret = kii_object_patch(&kii, &bucket, EX_OBJECT_ID, EX_OBJECT_DATA, NULL);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("replace object\n");
    ret = kii_object_replace(&kii, &bucket, EX_OBJECT_ID, EX_OBJECT_DATA, NULL);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("get object\n");
    ret = kii_object_get(&kii, &bucket, EX_OBJECT_ID);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("upload body at once\n");
    ret = kii_object_upload_body_at_once(&kii, &bucket, EX_OBJECT_ID, "text/plain", "1234", 4);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("upload body in multiple peces\n");
    do {
        memset(upload_id, 0x00, sizeof(upload_id));
        ret = kii_object_init_upload_body(&kii, &bucket, EX_OBJECT_ID, upload_id); 
        if (ret != 0) {
            printf("failed!\n");
            break;
        }
        memset(object_data, 0x00, sizeof(object_data));
        strcpy(object_data, EX_BODY_DATA);
        chunk.chunk = object_data;
        memset(content_type, 0x00, sizeof(content_type));
        strcpy(content_type, "text/plain");
        chunk.body_content_type = content_type;
        chunk.length = strlen(object_data);
        chunk.position = 0;
        chunk.total_length = strlen(object_data);
        ret = kii_object_upload_body(&kii, &bucket, EX_OBJECT_ID, upload_id, &chunk);
        if (ret != 0) {
            printf("failed!\n");
            break;
        }
        ret = kii_object_commit_upload(&kii, &bucket, EX_OBJECT_ID, upload_id, 1);
        if(ret == 0) {
            printf("success!\n");
        } else {
            printf("failed!\n");
        }
    }while(0);
    printf("download body at once\n");
    ret = kii_object_download_body_at_once(&kii, &bucket, EX_OBJECT_ID, &length);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("download body in multiple peces\n");
    ret = kii_object_download_body(&kii, &bucket, EX_OBJECT_ID, 0, 
    strlen(EX_BODY_DATA), &actual_length, &total_length);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("subscribe bucket\n");
    ret = kii_push_subscribe_bucket(&kii, &bucket);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("unsubscribe bucket\n");
    ret = kii_push_unsubscribe_bucket(&kii, &bucket);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("delete object\n");
    ret = kii_object_delete(&kii, &bucket, EX_OBJECT_ID);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("create topic\n");
    ret = kii_push_create_topic(&kii, &topic);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("subscrie topic\n");
    ret = kii_push_subscribe_topic(&kii, &topic);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
#if 0	
    printf("unsubscrie topic\n");
    ret = kii_push_unsubscribe_topic(&kii, &topic);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("delete topic\n");
    ret = kii_push_delete_topic(&kii, &topic);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
#endif	
    printf("Server code execute\n");
    ret = kii_server_code_execute(&kii, EX_ENDPOINT_NAME, NULL);
    if(ret == 0) {
        printf("success!\n");
    } else {
        printf("failed!\n");
    }
    printf("Initialize push\n");
    kii_push_start_routine(&kii, received_callback);
    return 0;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
