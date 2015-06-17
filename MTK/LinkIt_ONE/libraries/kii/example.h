#ifndef __example
#define __example


#ifdef __cplusplus
extern "C" {
#endif
const char EX_AUTH_VENDOR_ID[] = "4649";
const char EX_AUTH_VENDOR_PASS[] = "1234";
const char EX_AUTH_VENDOR_TYPE[] = "my_type";

const char EX_APP_SITE[] = "JP";
const char EX_APP_ID[] = "9ab34d8b";
const char EX_APP_KEY[] = "7a950d78956ed39f3b0815f0f001b43b";

#define EX_BUFFER_SIZE 2048
#define EX_MQTT_BUFFER_SIZE 1024

const char EX_THING_ID[] = "th.396587a00022-51e9-4e11-5eec-07846c59";
const char EX_ACCESS_TOKEN[] = "PlRI8O54j74Og7OtRnYLHu-ftQYINQwgDRuyhi3rGlQ";
const char EX_OBJECT_ID[] = "my_object";
const char EX_BUCKET_NAME[] = "my_bucket";
const char EX_TOPIC_NAME[] = "my_topic";
const char EX_OBJECT_DATA[] = "{}";
const char EX_BODY_DATA[] = "Hello world !\n";
const char EX_ENDPOINT_NAME[] = "test_topic";

extern int kiiDemo_test(char *buf);

#ifdef __cplusplus
}
#endif

#endif
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
