#include "kii_cloud_demo.h"
#include "kii_cloud_demo_setting.h"

#include "gt202_kii_adapter.h"

void parse_response(char* resp_body)
{
    /* TODO: implement */
}

static void init(kii_core_t* kii, context_t* ctx, char* buff, int buff_length)
{
    memset(kii, 0x00, sizeof(kii_core_t));
    kii->app_id = APP_ID;
    kii->app_key = APP_KEY;
    kii->app_host = APP_HOST;
    kii->http_context.buffer = buff;
    kii->http_context.buffer_size = buff_length;

    kii->http_context.app_context = ctx;
    kii->http_set_request_line_cb = request_line_cb;
    kii->http_set_header_cb = header_cb;
    kii->http_set_body_cb = body_cb;
    kii->http_execute_cb = execute_cb;
    kii->logger_cb = logger_cb;

    memset(ctx, 0x00, sizeof(context_t));
    /* share the request and response buffer.*/
    ctx->buff = buff;
    ctx->buff_size = buff_length;
}

static void set_author(kii_core_t* kii)
{
    strcpy(kii->author.author_id, THING_ID);
    strcpy(kii->author.access_token, ACCESS_TOKEN);
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

static void print_response(kii_core_t* kii)
{
    printf("========response========\n");
    printf("%s\n", kii->http_context.buffer);
    printf("========response========\n");
    printf("response_code: %d\n", kii->response_code);
    printf("response_body:\n%s\n", kii->response_body);
}

static int register_thing(kii_core_t* kii)
{
    int ret = 0;
    kii_state_t state;
    kii_error_code_t err;
    char *thingData = malloc(1024);
    
    /* Prepare Thing Data */
    memset(thingData, 0x00, 1024);
    sprintf(thingData,
            "{\"_vendorThingID\":\"%d\", \"_password\":\"1234\"}",
            time(NULL));
    /* Register Thing */
    err = kii_core_register_thing(kii, thingData);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        ret = 1;
        goto END_FUNC;
    }
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        ret = 1;
        goto END_FUNC;
    }
    print_response(kii);
    parse_response(kii->response_body);

END_FUNC:
    free(thingData);
    return ret;
}

static int thing_authentication(kii_core_t* kii)
{
    kii_state_t state;
    kii_error_code_t err;

    err = kii_core_thing_authentication(kii, AUTH_VENDOR_ID, AUTH_VENDOR_PASS);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);

    return 0;
}

static int create_new_object(kii_core_t* kii)
{
    kii_state_t state;
    kii_error_code_t err;
    
    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    err = kii_core_create_new_object(
            kii,
            &bucket,
            "{}",
            NULL);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }    
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
    return 0;
}

static int create_new_object_with_id(kii_core_t* kii, const char* id)
{
    kii_state_t state;
    kii_error_code_t err;
    
    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    err = kii_core_create_new_object_with_id(
            kii,
            &bucket,
            id,
            "{}",
            NULL);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }    
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
    return 0;
}

static int patch_object(kii_core_t* kii, const char* id)
{
    kii_state_t state;
    kii_error_code_t err;

    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    err = kii_core_patch_object(
            kii,
            &bucket,
            id,
            "{}",
            NULL);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }    
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
    return 0;
}

static int replace_object(kii_core_t* kii, const char* id)
{
    kii_state_t state;
    kii_error_code_t err;

    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    err = kii_core_replace_object(
            kii,
            &bucket,
            id,
            "{}",
            NULL);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }    
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
    return 0;
}

static int get_object(kii_core_t* kii, const char* id)
{
    kii_state_t state;
    kii_error_code_t err;

    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    err = kii_core_get_object(
            kii,
            &bucket,
            id);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
    return 0;
}

static int delete_object(kii_core_t* kii, const char* id)
{
    kii_state_t state;
    kii_error_code_t err;

    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    err = kii_core_delete_object(
            kii,
            &bucket,
            id);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
    return 0;
}

static int subscribe_bucket(kii_core_t* kii, const char* bucket_name)
{
    kii_state_t state;
    kii_error_code_t err;

    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    err = kii_core_subscribe_bucket(
            kii,
            &bucket);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
    return 0;
}

static int unsubscribe_bucket(kii_core_t* kii, const char* bucket_name)
{
    kii_state_t state;
    kii_error_code_t err;

    kii_bucket_t bucket;
    init_bucket(&bucket);

    set_author(kii);

    err = kii_core_unsubscribe_bucket(
            kii,
            &bucket);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
    return 0;
}

static int create_topic(kii_core_t* kii)
{
    kii_state_t state;
    kii_error_code_t err;

    kii_topic_t topic;
    init_topic(&topic);

    set_author(kii);

    err = kii_core_create_topic(
            kii,
            &topic);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
    return 0;
}

static int delete_topic(kii_core_t* kii)
{
    kii_state_t state;
    kii_error_code_t err;

    kii_topic_t topic;
    init_topic(&topic);

    set_author(kii);

    err = kii_core_delete_topic(
            kii,
            &topic);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
    return 0;
}

static int subscribe_topic(kii_core_t* kii)
{
    kii_state_t state;
    kii_error_code_t err;

    kii_topic_t topic;
    init_topic(&topic);

    set_author(kii);

    err = kii_core_subscribe_topic(
            kii,
            &topic);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
    return 0;
}

static int unsubscribe_topic(kii_core_t* kii)
{
    kii_state_t state;
    kii_error_code_t err;

    kii_topic_t topic;
    init_topic(&topic);

    set_author(kii);

    err = kii_core_unsubscribe_topic(
            kii,
            &topic);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
    return 0;
}

static int install_push(kii_core_t* kii)
{
    kii_state_t state;
    kii_error_code_t err;

    set_author(kii);

    err = kii_core_install_thing_push(
            kii,
            KII_FALSE);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
    return 0;
}

static int get_endpoint(kii_core_t* kii)
{
    kii_state_t state;
    kii_error_code_t err;

    set_author(kii);

    err = kii_core_get_mqtt_endpoint(
            kii,
            INSTALLATION_ID);
    printf("request:\n%s\n", kii->http_context.buffer);
    if (err != KIIE_OK) {
        printf("execution failed\n");
        return 1;
    }
    do {
        err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (err != KIIE_OK) {
        return 1;
    }
    print_response(kii);
    parse_response(kii->response_body);
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
    kii_core_t *kii = NULL;
    kii_state_t state;
    kii_error_code_t err;
    char *buff = NULL;

    if (argc < CMD_INDEX + 1)
    {
        show_help();
        return A_OK;
    }

    kii = malloc(sizeof(kii_core_t));
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

