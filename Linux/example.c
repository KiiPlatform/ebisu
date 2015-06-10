#include "kii.h"
#include "example.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "kii_core_impl.h"

void received_callback(void* app_context, char* buffer, size_t buffer_size) {
    char copy[1024];
    memset(copy, 0x00, sizeof(copy));
    strncpy(copy, buffer, sizeof(copy));
    printf("buffer_size: %lu\n", buffer_size);
    printf("recieve message: %s\n", copy);
}

int main(int argc, char** argv)
{
    int optval;
    kii_t kii;
    kii_author_t author;
    char buffer[EX_BUFFER_SIZE];
    char mqtt_buffer[EX_MQTT_BUFFER_SIZE];
    size_t buffer_size = EX_BUFFER_SIZE;
    size_t mqtt_buffer_size = EX_MQTT_BUFFER_SIZE;

    kii_topic_t topic;
    char scope_id[128];
    int ret;
    kii_bucket_t bucket;
    char object_data[512];
    char object_id[KII_OBJECTID_SIZE + 1];
    char upload_id[KII_UPLOADID_SIZE + 1];
    char content_type[128];
    kii_chunk_data_t chunk;
    unsigned int length;
    unsigned int actual_length;
    unsigned int total_length;
    context_t ssl_context;

    memset(buffer, 0x00, buffer_size);
    memset(mqtt_buffer, 0x00, mqtt_buffer_size);
    memset(&ssl_context, 0x00, sizeof(context_t));

    kii_init(&kii, EX_APP_SITE, EX_APP_ID, EX_APP_KEY);
    kii.kii_core.http_context.buffer = buffer;
    kii.kii_core.http_context.buffer_size = buffer_size;
    kii.kii_core.http_context.app_context = &ssl_context;
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

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"register", no_argument, NULL,  0},
            {"authentication", no_argument, NULL, 1},
            {"new-object", no_argument, NULL, 2},
            {"new-object-with-id", no_argument, NULL, 3},
            {"patch-object", no_argument, NULL, 4},
            {"replace-object", no_argument, NULL, 5},
            {"get-object", no_argument, NULL, 6},
            {"upload-body-o", no_argument, NULL, 7},
            {"upload-body-m", no_argument, NULL, 8},
            {"download-body-o", no_argument, NULL, 9},
            {"download-body-m", no_argument, NULL, 10},
            {"delete-object", no_argument, NULL, 11},
            {"subscribe-bucket", no_argument, NULL, 12},
            {"unsubscribe-bucket", no_argument, NULL, 13},
            {"create-topic", no_argument, NULL, 14},
            {"delete-topic", no_argument, NULL, 15},
            {"subscribe-topic", no_argument, NULL, 16},
            {"unsubscribe-topic", no_argument, NULL, 17},
            {"push", no_argument, NULL,  18},
            {"server-code-execute", no_argument, NULL,  19},
            {"help", no_argument, NULL, 1000},
            {0, 0, 0, 0}
        };

        optval = getopt_long(argc, argv, "",
                long_options, &option_index);
        if (optval == -1)
            break;

        switch (optval) {
            case 0:
                printf("register thing\n");
                ret = kii_thing_register(&kii, EX_AUTH_VENDOR_ID, 
                        EX_AUTH_VENDOR_TYPE, EX_AUTH_VENDOR_PASS);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 1:
                printf("authentication\n");
                ret = kii_thing_authenticate(&kii, EX_AUTH_VENDOR_ID, EX_AUTH_VENDOR_PASS);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 2:
                printf("create new object\n");
                memset(object_id, 0x00, sizeof(object_id));
                ret = kii_object_create(&kii, &bucket, EX_OBJECT_DATA, NULL, object_id);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 3:
                printf("create new object with id\n");
                ret = kii_object_create_with_id(&kii, &bucket, EX_OBJECT_ID, EX_OBJECT_DATA, NULL);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 4:
                printf("patch object\n");
                ret = kii_object_patch(&kii, &bucket, EX_OBJECT_ID, EX_OBJECT_DATA, NULL);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 5:
                printf("replace object\n");
                ret = kii_object_replace(&kii, &bucket, EX_OBJECT_ID, EX_OBJECT_DATA, NULL);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 6:
                printf("get object\n");
                ret = kii_object_get(&kii, &bucket, EX_OBJECT_ID);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 7:
                printf("upload body at once\n");
                ret = kii_object_upload_body_at_once(&kii, &bucket, EX_OBJECT_ID, "text/plain", "1234", 4);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 8:
                printf("upload body in multiple peces\n");
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
                break;
            case 9:
                printf("download body at once\n");
                ret = kii_object_download_body_at_once(&kii, &bucket, EX_OBJECT_ID, &length);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 10:
                printf("download body in multiple peces\n");
                ret = kii_object_download_body(&kii, &bucket, EX_OBJECT_ID, 0, 
                        strlen(EX_BODY_DATA), &actual_length, &total_length);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 11:
                printf("delete object\n");
                ret = kii_object_delete(&kii, &bucket, EX_OBJECT_ID);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 12:
                printf("subscribe bucket\n");
                ret = kii_push_subscribe_bucket(&kii, &bucket);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 13:
                printf("unsubscribe bucket\n");
                ret = kii_push_unsubscribe_bucket(&kii, &bucket);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 14:
                printf("create topic\n");
                ret = kii_push_create_topic(&kii, &topic);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 15:
                printf("delete topic\n");
                ret = kii_push_delete_topic(&kii, &topic);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 16:
                printf("subscrie topic\n");
                ret = kii_push_subscribe_topic(&kii, &topic);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 17:
                printf("unsubscrie topic\n");
                ret = kii_push_unsubscribe_topic(&kii, &topic);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 18:
                printf("Initialize push\n");
                kii_push_start_routine(&kii, 0, 0, received_callback);
                while(1) {
                    sleep(1);
                }
                break;
            case 19:
                printf("Server code execute\n");
                ret = kii_server_code_execute(&kii, EX_ENDPOINT_NAME, NULL);
                if(ret == 0) {
                    printf("success!\n");
                } else {
                    printf("failed!\n");
                }
                break;
            case 1000:
                printf("to configure parameters, edit example.h\n\n");
                printf("commands: \n");
                printf("--register\n register new thing.\n");
                printf("--authentication\n get access token.\n");
                printf("--new-object\n create new object.\n");
                printf("--new-object-with-id\n create new object with id.\n");
                printf("--patch-object\n patch object.\n");
                printf("--replace-object\n replace object.\n");
                printf("--get-object\n get object.\n");
                printf("--upload-body-o\n upload body at once.\n"),
                printf("--upload-body-m\n upload body in multiple pieces.\n"),
                printf("--download-body-o\n download body at once.\n"),
                printf("--download-body-m\n download body in multiple pieces.\n"),
                printf("--delete-object\n delete object.\n");
                printf("--subscribe-bucket\n subscribe bucket.\n");
                printf("--unsubscribe-bucket\n unsubscribe bucket.\n");
                printf("--create-topic\n create topic.\n");
                printf("--delete-topic\n delete topic.\n");
                printf("--subscribe-topic\n subscribe to topic.\n");
                printf("--unsubscribe-topic\n unsubscribe to topic.\n");
                printf("--push\n initialize push.\n");
                printf("--server-code-execute\n server code execute.\n");
                break;
            case '?':
                break;
            default:
                printf("?? getopt returned character code 0%o ??\n", optval);
        }
    }

    if (optind < argc) {
        printf("non-option ARGV-elements: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
    }
    return 0;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
