#include <stdio.h>
#include <string.h>

#include <tio.h>
#include <jkii.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <protocol_examples_common.h>

#include "sdkconfig.h"
#include "sock_cb_esp32.h"
#include "sys_cb_esp32.h"

static const char* TAG = "ebisu_sample";

#define HANDLER_HTTP_BUFF_SIZE 4096
#define HANDLER_MQTT_BUFF_SIZE 2048
#define HANDLER_KEEP_ALIVE_SEC 300
#define UPDATER_HTTP_BUFF_SIZE 4096
#define TOKENS 256

#define TO_RECV_SEC 15
#define TO_SEND_SEC 15

/* Large, so not on app_main's stack. */
static char handler_http_buff[HANDLER_HTTP_BUFF_SIZE];
static char handler_mqtt_buff[HANDLER_MQTT_BUFF_SIZE];
static char updater_http_buff[UPDATER_HTTP_BUFF_SIZE];
static jkii_token_t handler_tokens[TOKENS];
static jkii_token_t updater_tokens[TOKENS];

static tio_handler_t handler;
static tio_updater_t updater;
static kii_session_t session;
static kii_clock_t device_clock;
static socket_context_t handler_http_ctx;
static socket_context_t handler_mqtt_ctx;
static socket_context_t updater_http_ctx;

static const char state[] = CONFIG_EBISU_SAMPLE_STATE;
static size_t state_read;

static void log_time(void)
{
    int64_t now = 0;
    kii_bool_t exact = kii_clock_get_time(&device_clock, &now);
    ESP_LOGI(TAG, "the time: %lld, %s", (long long)now, exact == KII_TRUE ? "exact" : "a lower bound");
}

static const char* or_null(const char* s)
{
    return s[0] == '\0' ? NULL : s;
}

static void author_updated(const kii_author_t* author, void* userdata)
{
    /* A device that keeps its tokens across a restart saves the author here.
       This sample onboards at each start instead. */
    ESP_LOGI(TAG, "author updated: %s", author->author_id);
}

static kii_bool_t handler_continue(void* task_info, void* userdata)
{
    static kii_mqtt_task_state last = KII_MQTT_ST_DISCONTINUED;
    kii_mqtt_task_state task_state = ((tio_handler_task_info_t*)task_info)->task_state;
    if (task_state != last && task_state == KII_MQTT_ST_RECV_READY) {
        ESP_LOGI(TAG, "handler ready: waiting for commands");
    }
    last = task_state;
    return KII_TRUE;
}

static kii_bool_t updater_continue(void* task_info, void* userdata)
{
    return KII_TRUE;
}

static void task_exit(void* task_info, void* userdata)
{
    ESP_LOGE(TAG, "%s task exited", (const char*)userdata);
}

static void task_error(tio_code_t code, const char* message, void* userdata)
{
    ESP_LOGW(TAG, "%s: error %d: %s", (const char*)userdata, code, message);
}

static tio_bool_t action_handler(tio_action_t* action, tio_action_err_t* err,
        tio_action_result_data_t* data, void* userdata)
{
    const tio_action_value_t* value = &action->action_value;
    if (value->type == TIO_TYPE_STRING) {
        ESP_LOGI(TAG, "action %.*s %.*s: %.*s",
                (int)action->alias_length, action->alias,
                (int)action->action_name_length, action->action_name,
                (int)value->opaque_value_length, value->param.opaque_value);
    } else {
        ESP_LOGI(TAG, "action %.*s %.*s",
                (int)action->alias_length, action->alias,
                (int)action->action_name_length, action->action_name);
    }
    return KII_TRUE;
}

static size_t state_size(void* userdata)
{
    ESP_LOGI(TAG, "uploading the state");
    state_read = 0;
    return strlen(state);
}

static size_t state_read_cb(char* buffer, size_t size, void* userdata)
{
    size_t left = strlen(state) - state_read;
    size_t n = size < left ? size : left;
    memcpy(buffer, state + state_read, n);
    state_read += n;
    return n;
}

static void init_handler(jkii_resource_t* resource)
{
    tio_handler_init(&handler);
    tio_handler_set_app(&handler, CONFIG_EBISU_SAMPLE_APP_ID, CONFIG_EBISU_SAMPLE_APP_HOST);
    tio_handler_set_cb_task_create(&handler, task_create_cb_impl, NULL);
    tio_handler_set_cb_delay_ms(&handler, delay_ms_cb_impl, NULL);
    tio_handler_set_cb_task_continue(&handler, handler_continue, NULL);
    tio_handler_set_cb_task_exit(&handler, task_exit, "handler");
    tio_handler_set_cb_err(&handler, task_error, "handler");

    tio_handler_set_cb_sock_connect_http(&handler, sock_cb_connect, &handler_http_ctx);
    tio_handler_set_cb_sock_send_http(&handler, sock_cb_send, &handler_http_ctx);
    tio_handler_set_cb_sock_recv_http(&handler, sock_cb_recv, &handler_http_ctx);
    tio_handler_set_cb_sock_close_http(&handler, sock_cb_close, &handler_http_ctx);

    tio_handler_set_cb_sock_connect_mqtt(&handler, sock_cb_connect, &handler_mqtt_ctx);
    tio_handler_set_cb_sock_send_mqtt(&handler, sock_cb_send, &handler_mqtt_ctx);
    tio_handler_set_cb_sock_recv_mqtt(&handler, sock_cb_recv, &handler_mqtt_ctx);
    tio_handler_set_cb_sock_recv_timed_out_mqtt(&handler, sock_cb_recv_timed_out, &handler_mqtt_ctx);
    tio_handler_set_cb_sock_close_mqtt(&handler, sock_cb_close, &handler_mqtt_ctx);
    tio_handler_set_mqtt_to_sock_recv(&handler, TO_RECV_SEC);
    tio_handler_set_mqtt_to_sock_send(&handler, TO_SEND_SEC);

    tio_handler_set_http_buff(&handler, handler_http_buff, sizeof(handler_http_buff));
    tio_handler_set_mqtt_buff(&handler, handler_mqtt_buff, sizeof(handler_mqtt_buff));
    tio_handler_set_keep_alive_interval(&handler, HANDLER_KEEP_ALIVE_SEC);
    tio_handler_set_json_parser_resource(&handler, resource);
}

static void init_updater(jkii_resource_t* resource)
{
    tio_updater_init(&updater);
    tio_updater_set_app(&updater, CONFIG_EBISU_SAMPLE_APP_ID, CONFIG_EBISU_SAMPLE_APP_HOST);
    tio_updater_set_cb_task_create(&updater, task_create_cb_impl, NULL);
    tio_updater_set_cb_delay_ms(&updater, delay_ms_cb_impl, NULL);
    tio_updater_set_cb_task_continue(&updater, updater_continue, NULL);
    tio_updater_set_cb_task_exit(&updater, task_exit, "updater");
    tio_updater_set_cb_error(&updater, task_error, "updater");

    tio_updater_set_cb_sock_connect(&updater, sock_cb_connect, &updater_http_ctx);
    tio_updater_set_cb_sock_send(&updater, sock_cb_send, &updater_http_ctx);
    tio_updater_set_cb_sock_recv(&updater, sock_cb_recv, &updater_http_ctx);
    tio_updater_set_cb_sock_close(&updater, sock_cb_close, &updater_http_ctx);

    tio_updater_set_buff(&updater, updater_http_buff, sizeof(updater_http_buff));
    tio_updater_set_interval(&updater, CONFIG_EBISU_SAMPLE_UPDATE_PERIOD);
    tio_updater_set_json_parser_resource(&updater, resource);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* Wi-Fi or Ethernet, as configured. */
    ESP_ERROR_CHECK(example_connect());

    /* The time the server's certificate is checked at, which this sample takes
       from the server, the device having no real-time clock: until the first
       response, the clock has only a floor, a time it is known not to be
       earlier than. The clock has a lock of its own: the socket callbacks use
       it while kii may hold the session's. */
    kii_clock_init(&device_clock, monotonic_ms_cb_impl, NULL);
    kii_clock_set_cb_lock(&device_clock, lock_cb_impl, unlock_cb_impl, xSemaphoreCreateMutex());
    if (CONFIG_EBISU_SAMPLE_CLOCK_FLOOR > 0) {
        kii_clock_set_floor(&device_clock, CONFIG_EBISU_SAMPLE_CLOCK_FLOOR);
    }
    log_time();

    sock_cb_init_context(&handler_http_ctx, TO_RECV_SEC, TO_SEND_SEC, &device_clock);
    sock_cb_init_context(&handler_mqtt_ctx, TO_RECV_SEC, TO_SEND_SEC, &device_clock);
    sock_cb_init_context(&updater_http_ctx, TO_RECV_SEC, TO_SEND_SEC, &device_clock);

    static jkii_resource_t handler_resource = { handler_tokens, TOKENS };
    static jkii_resource_t updater_resource = { updater_tokens, TOKENS };
    init_handler(&handler_resource);
    init_updater(&updater_resource);

    /* The handler and the updater share their tokens through the session,
       which renews them as they expire, and each response's date sets the
       clock. */
    kii_session_init(&session);
    kii_session_set_cb_lock(&session, lock_cb_impl, unlock_cb_impl, xSemaphoreCreateMutex());
    kii_session_set_cb_monotonic_ms(&session, monotonic_ms_cb_impl, NULL);
    kii_session_set_cb_author_updated(&session, author_updated, NULL);
    if (tio_handler_set_session(&handler, &session) != TIO_ERR_OK ||
            tio_updater_set_session(&updater, &session) != TIO_ERR_OK ||
            tio_handler_set_clock(&handler, &device_clock) != TIO_ERR_OK ||
            tio_updater_set_clock(&updater, &device_clock) != TIO_ERR_OK) {
        ESP_LOGE(TAG, "failed to set the session and the clock");
        return;
    }

    /* Used to onboard again when the server refuses the token, or the broker
       the MQTT credentials. */
    kii_session_set_password(&session, CONFIG_EBISU_SAMPLE_VENDOR_THING_ID,
            CONFIG_EBISU_SAMPLE_THING_PASSWORD);
    for (;;) {
        tio_code_t result = tio_handler_onboard(&handler,
                CONFIG_EBISU_SAMPLE_VENDOR_THING_ID,
                CONFIG_EBISU_SAMPLE_THING_PASSWORD,
                or_null(CONFIG_EBISU_SAMPLE_THING_TYPE),
                or_null(CONFIG_EBISU_SAMPLE_FIRMWARE_VERSION),
                NULL,
                NULL);
        if (result == TIO_ERR_OK) {
            break;
        }
        ESP_LOGW(TAG, "failed to onboard: %d; trying again in 10 seconds", result);
        log_time();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    ESP_LOGI(TAG, "onboarded");
    log_time();

    /* Both take the author from the session. */
    tio_handler_start(&handler, NULL, action_handler, NULL);
    tio_updater_start(&updater, NULL, state_size, NULL, state_read_cb, NULL);
}
