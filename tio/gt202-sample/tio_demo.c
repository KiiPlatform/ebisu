#include "tio_demo.h"

#include <stdio.h>

#include "main.h"
#include "tio.h"
#include "tio_task_impl.h"
#include "tio_socket_impl.h"

#if READ_HOST_MEMORY
extern int_32 __END_BSS;
extern int_32 __START_BSS;
extern int_32 __END_TEXT;
extern int_32 __START_TEXT;
extern int_32 __END_DATA;
extern int_32 __START_DATA;
extern int_32 __END_RODATA;
extern int_32 __START_RODATA;
#endif

const char KII_APP_ID[] = "fj9xy2fsp0ld";
const char KII_APP_HOST[] = "api-jp.kii.com";
const char VENDOR_ID[] = "test-gt202";
const char VENDOR_PASS[] = "1234";

#define HANDLER_HTTP_BUFF_SIZE 2048
#define HANDLER_MQTT_BUFF_SIZE 2048
#define HANDLER_KEEP_ALIVE_SEC 300

#define UPDATER_HTTP_BUFF_SIZE 2048
#define UPDATE_PERIOD_SEC 60

#define TO_RECV_SEC 15
#define TO_SEND_SEC 15

bool term_flag = false;
bool handler_terminated = false;
bool updater_terminated = false;

tio_bool_t _handler_continue(void* task_info, void* userdata) {
    if (term_flag == true) {
        return KII_FALSE;
    } else {
        return KII_TRUE;
    }
}

tio_bool_t _updater_continue(void* task_info, void* userdata) {
    if (term_flag == true) {
        return KII_FALSE;
    } else {
        return KII_TRUE;
    }
}

void _handler_exit(void* task_info, void* userdata) {
    printf("_handler_exit called\n");
    handler_terminated = true;
}

void _updater_exit(void* task_info, void* userdata) {
    printf("_updater_exit called\n");
    updater_terminated = true;
}

void updater_init(
        tio_updater_t* updater,
        char* buffer,
        int buffer_size,
        void* sock_ssl_ctx,
        jkii_resource_t* resource)
{
    tio_updater_init(updater);

    tio_updater_set_app(updater, KII_APP_ID, KII_APP_HOST);

    tio_updater_set_cb_task_create(updater, cb_task_create, NULL);
    tio_updater_set_cb_delay_ms(updater, cb_delay_ms, NULL);

    tio_updater_set_buff(updater, buffer, buffer_size);

    tio_updater_set_cb_sock_connect(updater, sock_cb_connect, sock_ssl_ctx);
    tio_updater_set_cb_sock_send(updater, sock_cb_send, sock_ssl_ctx);
    tio_updater_set_cb_sock_recv(updater, sock_cb_recv, sock_ssl_ctx);
    tio_updater_set_cb_sock_close(updater, sock_cb_close, sock_ssl_ctx);

    tio_updater_set_interval(updater, UPDATE_PERIOD_SEC);

    tio_updater_set_json_parser_resource(updater, resource);

    tio_updater_set_cb_task_continue(updater, _updater_continue, NULL);
    tio_updater_set_cb_task_exit(updater, _updater_exit, NULL);

#if CONNECT_SSL
    // no need to set value, this is default.
    //tio_updater_enable_insecure_http(updater, KII_FALSE);
#else
    tio_updater_enable_insecure_http(updater, KII_TRUE);
#endif
}

const char send_state[] = "{\"AirconAlias\":{\"RoomTemperature\":17,\"PresetTemperature\":17}}";

typedef struct {
    size_t max_size;
    size_t read_size;
} updater_context_t;

size_t updater_cb_state_size(void* userdata)
{
    updater_context_t* ctx = (updater_context_t*)userdata;
    ctx->max_size = strlen(send_state);
    ctx->read_size = 0;
    printf("state_size: %d\n", ctx->max_size);
    return ctx->max_size;
}

size_t updater_cb_read(char *buffer, size_t size, void *userdata)
{
    updater_context_t* ctx = (updater_context_t*)userdata;
    size_t read_size = sprintf(buffer, "%.*s", size, &send_state[ctx->read_size]);
    ctx->read_size += read_size;
    printf("state_read: %d\n", read_size);
    return read_size;
}

tio_bool_t pushed_message_callback(const char* message, size_t message_length, void* userdata)
{
    printf("pushed_message_callback called,\n");
    printf("%.*s\n", (int)message_length, message);
    return KII_FALSE;
}

void handler_init(
        tio_handler_t* handler,
        char* http_buffer,
        int http_buffer_size,
        void* http_ssl_ctx,
        char* mqtt_buffer,
        int mqtt_buffer_size,
        void* mqtt_ssl_ctx,
        jkii_resource_t* resource)
{
    tio_handler_init(handler);

    tio_handler_set_app(handler, KII_APP_ID, KII_APP_HOST);

    tio_handler_set_cb_push(handler, pushed_message_callback, NULL);

    tio_handler_set_cb_task_create(handler, cb_task_create, NULL);
    tio_handler_set_cb_delay_ms(handler, cb_delay_ms, NULL);

    tio_handler_set_cb_sock_connect_http(handler, sock_cb_connect, http_ssl_ctx);
    tio_handler_set_cb_sock_send_http(handler, sock_cb_send, http_ssl_ctx);
    tio_handler_set_cb_sock_recv_http(handler, sock_cb_recv, http_ssl_ctx);
    tio_handler_set_cb_sock_close_http(handler, sock_cb_close, http_ssl_ctx);

    tio_handler_set_cb_sock_connect_mqtt(handler, sock_cb_connect, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_send_mqtt(handler, sock_cb_send, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_recv_mqtt(handler, mqtt_cb_recv, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_close_mqtt(handler, sock_cb_close, mqtt_ssl_ctx);

    tio_handler_set_mqtt_to_sock_recv(handler, TO_RECV_SEC);
    tio_handler_set_mqtt_to_sock_send(handler, TO_SEND_SEC);

    tio_handler_set_http_buff(handler, http_buffer, http_buffer_size);
    tio_handler_set_mqtt_buff(handler, mqtt_buffer, mqtt_buffer_size);

    tio_handler_set_keep_alive_interval(handler, HANDLER_KEEP_ALIVE_SEC);

    tio_handler_set_json_parser_resource(handler, resource);

    tio_handler_set_cb_task_continue(handler, _handler_continue, NULL);
    tio_handler_set_cb_task_exit(handler, _handler_exit, NULL);

#if CONNECT_SSL
    // no need to set value, this is default.
    //tio_handler_enable_insecure_http(handler, KII_FALSE);
    //tio_handler_enable_insecure_mqtt(handler, KII_FALSE);
#else
    tio_handler_enable_insecure_http(handler, KII_TRUE);
    tio_handler_enable_insecure_mqtt(handler, KII_TRUE);
#endif
}

tio_bool_t tio_action_handler(tio_action_t* action, tio_action_err_t* err, void* userdata)
{
    printf("tio_action_handler called\n");
    printf("%.*s: %.*s\n", (int)action->alias_length, action->alias,(int)action->action_name_length, action->action_name);
#if READ_HOST_MEMORY
    printf("text   : %d bytes\n", (int_32)&__END_TEXT - (int_32)&__START_TEXT);
    printf("bss    : %d bytes\n", (int_32)&__END_BSS - (int_32)&__START_BSS);
    printf("data   : %d bytes\n", (int_32)&__END_DATA - (int_32)&__START_DATA);
    printf("rodata : %d bytes\n", (int_32)&__END_RODATA - (int_32)&__START_RODATA);
#endif
    return KII_TRUE;
}

static void show_help()
{
    printf("commands: \n");
    printf(" help\n\t show this help.\n");
    printf(" onboard\n\t onboard this.\n");
#if READ_HOST_MEMORY
    printf("text   : %d bytes\n", (int_32)&__END_TEXT - (int_32)&__START_TEXT);
    printf("bss    : %d bytes\n", (int_32)&__END_BSS - (int_32)&__START_BSS);
    printf("data   : %d bytes\n", (int_32)&__END_DATA - (int_32)&__START_DATA);
    printf("rodata : %d bytes\n", (int_32)&__END_RODATA - (int_32)&__START_RODATA);
#endif
}

#define CMD_INDEX 1

int tio_main(int argc, char *argv[])
{
    if (argc < CMD_INDEX + 1 || ATH_STRCMP(argv[CMD_INDEX], "help") == 0)
    {
        show_help();
        return A_OK;
    }

    if(ATH_STRCMP(argv[CMD_INDEX], "onboard") == 0)
    {
#if CONNECT_SSL
        ssl_ctx_init();
#endif

        tio_updater_t* updater = malloc(sizeof(tio_updater_t));

        socket_context_t updater_http_ctx;
        updater_http_ctx.to_recv = TO_RECV_SEC;
        updater_http_ctx.to_send = TO_SEND_SEC;
        updater_http_ctx.show_debug = 0;

        jkii_token_t* updater_tokens = malloc(sizeof(jkii_token_t) * 256);
        jkii_resource_t updater_resource = { updater_tokens, 256};

        updater_context_t updater_ctx;

        char* updater_buff = malloc(UPDATER_HTTP_BUFF_SIZE);
        memset(updater_buff, 0x00, sizeof(char) * UPDATER_HTTP_BUFF_SIZE);
        updater_init(
                updater,
                updater_buff,
                UPDATER_HTTP_BUFF_SIZE,
                &updater_http_ctx,
                &updater_resource);

        tio_handler_t* handler = malloc(sizeof(tio_handler_t));

        socket_context_t handler_http_ctx;
        handler_http_ctx.to_recv = TO_RECV_SEC;
        handler_http_ctx.to_send = TO_SEND_SEC;
        handler_http_ctx.show_debug = 0;

        socket_context_t handler_mqtt_ctx;
        handler_mqtt_ctx.to_recv = TO_RECV_SEC;
        handler_mqtt_ctx.to_send = TO_SEND_SEC;
        handler_mqtt_ctx.show_debug = 0;

        char* handler_http_buff = malloc(HANDLER_HTTP_BUFF_SIZE);
        memset(handler_http_buff, 0x00, sizeof(char) * HANDLER_HTTP_BUFF_SIZE);

        char* handler_mqtt_buff = malloc(HANDLER_MQTT_BUFF_SIZE);
        memset(handler_mqtt_buff, 0x00, sizeof(char) * HANDLER_MQTT_BUFF_SIZE);

        jkii_token_t* handler_tokens = malloc(sizeof(jkii_token_t) * 256);
        jkii_resource_t handler_resource = {handler_tokens, 256};

        handler_init(
                handler,
                handler_http_buff,
                HANDLER_HTTP_BUFF_SIZE,
                &handler_http_ctx,
                handler_mqtt_buff,
                HANDLER_MQTT_BUFF_SIZE,
                &handler_mqtt_ctx,
                &handler_resource);

        tio_code_t result = tio_handler_onboard(
                handler,
                VENDOR_ID,
                VENDOR_PASS,
                NULL,
                NULL,
                NULL,
                NULL);
        if (result != TIO_ERR_OK) {
            printf("[%d] failed to onboard.\n", result);
            return A_OK;
        } else {
            printf("succeed to onboard.\n");
        }

        const kii_author_t* author = tio_handler_get_author(handler);
        tio_handler_start(handler, author, tio_action_handler, NULL);
        tio_updater_start(
                updater,
                author,
                updater_cb_state_size,
                &updater_ctx,
                updater_cb_read,
                &updater_ctx);
        while(1) {
            _time_delay(1000);
        }

        free(updater);
        free(updater_tokens);
        free(updater_buff);
        free(handler);
        free(handler_http_buff);
        free(handler_mqtt_buff);
        free(handler_tokens);

#if CONNECT_SSL
        ssl_ctx_close();
#endif
    }

    return A_OK;
}

