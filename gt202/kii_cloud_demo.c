#include "kii_cloud_demo.h"
#include "kii_cloud_demo_setting.h"

#include "gt202_kii_adapter.h"

static char static_ojbect_id_buf[KII_OBJECTID_SIZE + 1];

void parse_response(char* resp_body)
{
    /* TODO: implement */
}

static void init(kii_t* kii, context_t* ctx, char* buff, int buff_length)
{
    memset(kii, 0x00, sizeof(kii_t));
    kii->kii_core.app_id = APP_ID;
    kii->kii_core.app_key = APP_KEY;
    kii->kii_core.app_host = APP_HOST;
    kii->kii_core.http_context.buffer = buff;
    kii->kii_core.http_context.buffer_size = buff_length;

    kii->kii_core.http_context.app_context = ctx;
    kii->kii_core.http_set_request_line_cb = request_line_cb;
    kii->kii_core.http_set_header_cb = header_cb;
    kii->kii_core.http_set_body_cb = body_cb;
    kii->kii_core.http_execute_cb = execute_cb;
    kii->kii_core.logger_cb = logger_cb;

    memset(ctx, 0x00, sizeof(context_t));
    /* share the request and response buffer.*/
    ctx->buff = buff;
    ctx->buff_size = buff_length;
}

static void set_author(kii_t* kii)
{
    strcpy(kii->kii_core.author.author_id, THING_ID);
    strcpy(kii->kii_core.author.access_token, ACCESS_TOKEN);
}

static void init_bucket(kii_bucket_t* bucket) {
    memset(bucket, 0x00, sizeof(kii_bucket_t));
    bucket->scope = KII_SCOPE_THING;
    bucket->scope_id = THING_ID;
    bucket->bucket_name = BUCKET_NAME;
}

static void init_topic(kii_topic_t* topic) {
    memset(topic, 0x00, sizeof(kii_topic_t));
    topic->scope = KII_SCOPE_THING;
    topic->scope_id = THING_ID;
    topic->topic_name = TOPIC_NAME;
}

static void print_response(kii_t* kii)
{
    printf("========response========\n");
    printf("%s\n", kii->kii_core.http_context.buffer);
    printf("========response========\n");
    printf("response_code: %d\n", kii->kii_core.response_code);
    printf("response_body:\n%s\n", kii->kii_core.response_body);
}

static int register_thing(kii_t* kii)
{
    int ret = 0;
    char venderId[22];

    sprintf(venderId, "%d", time(NULL));
    ret = kii_thing_register(kii, venderId, "my_type", "1234");

    print_response(kii);
    parse_response(kii->kii_core.response_body);

    return ret;
}

static int thing_authentication(kii_t* kii)
{
    int ret = 0;

    ret = kii_thing_authenticate(kii, AUTH_VENDOR_ID, AUTH_VENDOR_PASS);

    print_response(kii);
    parse_response(kii->kii_core.response_body);

    return ret;
}

static int create_new_object(kii_t* kii)
{
    int ret = 0;
    char* objectId = static_ojbect_id_buf;

    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    ret = kii_object_create(kii, &bucket, "{}", NULL, objectId);

    print_response(kii);
    printf("ojbectId = %s\n", objectId);
    parse_response(kii->kii_core.response_body);
    return ret;
}

static int create_new_object_with_id(kii_t* kii, const char* id)
{
    int ret = 0;
    
    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    ret = kii_object_create_with_id(kii, &bucket, id, "{}", NULL);

    print_response(kii);
    parse_response(kii->kii_core.response_body);
    return ret;
}

static int patch_object(kii_t* kii, const char* id)
{
    int ret = 0;

    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    ret = kii_object_patch(kii, &bucket, id, "{}", NULL);

    print_response(kii);
    parse_response(kii->kii_core.response_body);
    return ret;
}

static int replace_object(kii_t* kii, const char* id)
{
    int ret = 0;

    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    ret = kii_object_replace(kii, &bucket, id, "{}", NULL);

    print_response(kii);
    parse_response(kii->kii_core.response_body);
    return ret;
}

static int get_object(kii_t* kii, const char* id)
{
    int ret = 0;

    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    ret = kii_object_get(kii, &bucket, id);

    print_response(kii);
    parse_response(kii->kii_core.response_body);
    return ret;
}

static int delete_object(kii_t* kii, const char* id)
{
    int ret = 0;

    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    ret = kii_object_delete(kii, &bucket, id);

    print_response(kii);
    parse_response(kii->kii_core.response_body);
    return ret;
}

static int subscribe_bucket(kii_t* kii, const char* bucket_name)
{
    int ret = 0;

    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    ret = kii_push_subscribe_bucket(kii, &bucket);

    print_response(kii);
    parse_response(kii->kii_core.response_body);
    return ret;
}

static int unsubscribe_bucket(kii_t* kii, const char* bucket_name)
{
    int ret = 0;

    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    ret = kii_push_unsubscribe_bucket(kii, &bucket);

    print_response(kii);
    parse_response(kii->kii_core.response_body);
    return ret;
}

static int create_topic(kii_t* kii)
{
    int ret = 0;

    kii_topic_t topic;
    init_topic(&topic);

    set_author(kii);

    ret = kii_push_create_topic(kii, &topic);

    print_response(kii);
    parse_response(kii->kii_core.response_body);
    return ret;
}

static int delete_topic(kii_t* kii)
{
    int ret = 0;

    kii_topic_t topic;
    init_topic(&topic);

    set_author(kii);

    ret = kii_push_delete_topic(kii, &topic);

    print_response(kii);
    parse_response(kii->kii_core.response_body);
    return ret;
}

static int subscribe_topic(kii_t* kii)
{
    int ret = 0;

    kii_topic_t topic;
    init_topic(&topic);

    set_author(kii);

    ret = kii_push_subscribe_topic(kii, &topic);

    print_response(kii);
    parse_response(kii->kii_core.response_body);
    return ret;
}

static int unsubscribe_topic(kii_t* kii)
{
    int ret = 0;

    kii_topic_t topic;
    init_topic(&topic);

    set_author(kii);

    ret = kii_push_unsubscribe_topic(kii, &topic);

    print_response(kii);
    parse_response(kii->kii_core.response_body);
    return ret;
}

static int install_push(kii_t* kii)
{
    kii_state_t state;
    kii_error_code_t err;

    set_author(kii);

    err = kii_core_install_thing_push(
            &kii->kii_core,
            KII_FALSE);
    printf("request:\n%s\n", kii->kii_core.http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }
    do {
        err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->kii_core.response_body);
    return 0;
}

static int get_endpoint(kii_t* kii)
{
    kii_state_t state;
    kii_error_code_t err;

    set_author(kii);

    err = kii_core_get_mqtt_endpoint(
            &kii->kii_core,
            INSTALLATION_ID);
    printf("request:\n%s\n", kii->kii_core.http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }
    do {
        err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->kii_core.response_body);
    return 0;
}

static void show_help()
{
      printf("commands: \n");
      printf(" help\n\t show this help.\n");
      printf(" register\n\t register new thing.\n");
      printf(" authentication\n\t get access token.\n");
      printf(" new-object\n\t create new object.\n");
      printf(" new-object-with-id\n\t create new object with id.\n");
      printf(" patch-object\n\t patch object.\n");
      printf(" replace-object\n\t replace object.\n");
      printf(" get-object\n\t get object.\n");
      printf(" delete-object\n\t delete object.\n");
      printf(" subscribe-bucket\n\t subscribe bucket.\n");
      printf(" unsubscribe-bucket\n\t unsubscribe bucket.\n");
      printf(" create-topic\n\t create topic.\n");
      printf(" delete-topic\n\t delete topic.\n");
      printf(" subscribe-topic\n\t subscribe to topic.\n");
      printf(" unsubscribe-topic\n\t unsubscribe to topic.\n");
      printf(" install-push\n\t install push.\n");
      printf(" get-endpoint\n\t get endpoint of MQTT.\n");
}

#define CMD_INDEX 1

int kii_main(int argc, char *argv[])
{
    int ret = A_ERROR;
    context_t *ctx = NULL;
    kii_t *kii = NULL;
    kii_state_t state;
    kii_error_code_t err;
    char *buff = NULL;

    if (argc < CMD_INDEX + 1)
    {
        show_help();
        return A_OK;
    }

    kii = malloc(sizeof(kii_t));
    ctx = malloc(sizeof(context_t));
    buff = malloc(4096);

    init(kii, ctx, buff, 4096);

    if(ATH_STRCMP(argv[CMD_INDEX], "register") == 0)
    {
        if (register_thing(kii) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "authentication") == 0)
    {
        if (thing_authentication(kii) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "new-object") == 0)
    {
        if (create_new_object(kii) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "new-object-with-id") == 0)
    {
        if (create_new_object_with_id(kii, OBJECT_NAME) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "patch-object") == 0)
    {
        if (patch_object(kii, OBJECT_NAME) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "replace-object") == 0)
    {
        if (replace_object(kii, OBJECT_NAME) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "get-object") == 0)
    {
        if (get_object(kii, OBJECT_NAME) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "delete-object") == 0)
    {
        if (delete_object(kii, OBJECT_NAME) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "subscribe-bucket") == 0)
    {
        if (subscribe_bucket(kii, BUCKET_NAME) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "unsubscribe-bucket") == 0)
    {
        if (unsubscribe_bucket(kii, BUCKET_NAME) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "create-topic") == 0)
    {
        if (create_topic(kii) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "delete-topic") == 0)
    {
        if (delete_topic(kii) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "subscribe-topic") == 0)
    {
        if (subscribe_topic(kii) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "unsubscribe-topic") == 0)
    {
        if (unsubscribe_topic(kii) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "install-push") == 0)
    {
        if (install_push(kii) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "get-endpoint") == 0)
    {
        if (get_endpoint(kii) == 0)
        {
            ret = A_OK;
        }
    }
    else if(ATH_STRCMP(argv[CMD_INDEX], "help") == 0)
    {
        show_help();
        ret = A_OK;
    }

    free(kii);
    free(buff);
    free(ctx);
    return ret;
}
