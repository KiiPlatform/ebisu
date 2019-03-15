#include "wiced.h"
#include "wiced_log.h"

#include <command_console_commands.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "tio.h"
#include "tio_task_impl.h"
#include "tio_socket_impl.h"

const char KII_APP_ID[] = "fj9xy2fsp0ld";
const char KII_APP_HOST[] = "api-jp.kii.com";
const char VENDOR_ID[] = "test1";
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
    wiced_log_printf("_handler_exit called\n");
    handler_terminated = true;
}

void _updater_exit(void* task_info, void* userdata) {
    wiced_log_printf("_updater_exit called\n");
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

#if CONNECT_INSECURE
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
    wiced_log_printf("state_size: %d\n", ctx->max_size);
    return ctx->max_size;
}

size_t updater_cb_read(char *buffer, size_t size, void *userdata)
{
    updater_context_t* ctx = (updater_context_t*)userdata;
    size_t read_size = sprintf(buffer, "%.*s", size, &send_state[ctx->read_size]);
    ctx->read_size += read_size;
    wiced_log_printf("state_read: %d\n", read_size);
    return read_size;
}

tio_bool_t pushed_message_callback(const char* message, size_t message_length, void* userdata)
{
    wiced_log_printf("pushed_message_callback called,\n");
    wiced_log_printf("%.*s\n", (int)message_length, message);
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

#if CONNECT_INSECURE
    tio_handler_enable_insecure_http(handler, KII_TRUE);
    tio_handler_enable_insecure_mqtt(handler, KII_TRUE);
#endif
}

tio_bool_t tio_action_handler(tio_action_t* action, tio_action_err_t* err, tio_action_result_data_t* data, void* userdata)
{
    wiced_log_printf("tio_action_handler called\n");
    wiced_log_printf("%.*s: %.*s\n", (int)action->alias_length, action->alias,(int)action->action_name_length, action->action_name);
    return KII_TRUE;
}

static tio_updater_t updater;
static char updater_buff[UPDATER_HTTP_BUFF_SIZE];
static jkii_token_t updater_tokens[256];
static socket_context_t updater_http_ctx;
static updater_context_t updater_ctx;
static jkii_resource_t updater_resource = { updater_tokens, 256 };

static tio_handler_t handler;
static socket_context_t handler_http_ctx;
static socket_context_t handler_mqtt_ctx;
static char handler_http_buff[HANDLER_HTTP_BUFF_SIZE];
static char handler_mqtt_buff[HANDLER_MQTT_BUFF_SIZE];
static jkii_token_t handler_tokens[256];
static jkii_resource_t handler_resource = {handler_tokens, 256};

static int tio_main(int argc, char *argv[])
{
    updater_http_ctx.show_debug = 1;

    memset(updater_buff, 0x00, sizeof(char) * UPDATER_HTTP_BUFF_SIZE);
    updater_init(
            &updater,
            updater_buff,
            UPDATER_HTTP_BUFF_SIZE,
            &updater_http_ctx,
            &updater_resource);

    handler_http_ctx.show_debug = 1;

    handler_mqtt_ctx.show_debug = 1;

    memset(handler_http_buff, 0x00, sizeof(char) * HANDLER_HTTP_BUFF_SIZE);

    memset(handler_mqtt_buff, 0x00, sizeof(char) * HANDLER_MQTT_BUFF_SIZE);

    handler_init(
            &handler,
            handler_http_buff,
            HANDLER_HTTP_BUFF_SIZE,
            &handler_http_ctx,
            handler_mqtt_buff,
            HANDLER_MQTT_BUFF_SIZE,
            &handler_mqtt_ctx,
            &handler_resource);

    tio_code_t result = tio_handler_onboard(
            &handler,
            VENDOR_ID,
            VENDOR_PASS,
            NULL,
            NULL,
            NULL,
            NULL);
    if (result != TIO_ERR_OK) {
        wiced_log_printf("[%d] failed to onboard.\n", result);
        return ERR_CMD_OK;
    } else {
        wiced_log_printf("succeed to onboard.\n");
    }

    const kii_author_t* author = tio_handler_get_author(&handler);
    tio_handler_start(&handler, author, tio_action_handler, NULL);
    tio_updater_start(
            &updater,
            author,
            updater_cb_state_size,
            &updater_ctx,
            updater_cb_read,
            &updater_ctx);

    return ERR_CMD_OK;
}

static int wiced_log_output_handler(WICED_LOG_LEVEL_T level, char *logmsg)
{
    write(STDOUT_FILENO, logmsg, strlen(logmsg));

    return 0;
}

static char line_buffer[MAX_LINE_LENGTH];
static char history_buffer_storage[MAX_LINE_LENGTH * MAX_HISTORY_LENGTH];

static const command_t commands[] =
{
    //ALL_COMMANDS
    {"tio", tio_main, 0, NULL, NULL, "", ""},
    CMD_TABLE_END
};

/******************************************************
 *               Function Definitions
 ******************************************************/
void application_start( void )
{
    wiced_result_t ret = WICED_SUCCESS;

    ret = wiced_init();
    if ( ret != WICED_SUCCESS )
    {
        wiced_log_printf("wiced_init failed.\n\n");
        return;
    }

    wiced_log_init(WICED_LOG_PRINTF, wiced_log_output_handler, NULL);

    /* Disable roaming to other access points */
    wiced_wifi_set_roam_trigger( -99 ); /* -99dBm ie. extremely low signal level */

    /* Bringup the network interface */
    ret = wiced_network_up( WICED_STA_INTERFACE, WICED_USE_EXTERNAL_DHCP_SERVER, NULL );
    if ( ret != WICED_SUCCESS )
    {
        wiced_log_printf("\nNot able to join the requested AP\n\n");
        return;
    }

    /* Run the main application function */
    command_console_init( STDIO_UART, MAX_LINE_LENGTH, line_buffer, MAX_HISTORY_LENGTH, history_buffer_storage, " " );
    console_add_cmd_table( commands );
}
