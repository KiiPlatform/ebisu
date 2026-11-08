#include <string.h>

#include <ebisu_version.h>
#include <jkii.h>
#include <tio.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "board.h"
#include "header_pool.h"
#include "log.h"
#include "net.h"
#include "random.h"
#include "sample_config.h"
#include "sock_cb_freertos.h"
#include "sys_cb_freertos.h"

#define HANDLER_HTTP_BUFF_SIZE 4096
#define HANDLER_MQTT_BUFF_SIZE 2048
#define HANDLER_KEEP_ALIVE_SEC 300
#define UPDATER_HTTP_BUFF_SIZE 4096
#define STREAM_BUFF_SIZE 1024
#define RESP_HEADER_BUFF_SIZE 256

#define TO_RECV_SEC 15
#define TO_SEND_SEC 15

/* In words. The task onboards, over TLS. */
#define APP_TASK_STACK_WORDS 3072

/* From EBISU_SAMPLE_CA_FILE, in ca_pem.c, which CMake generates. */
extern const char ca_pem[];
extern const size_t ca_pem_size;

/* Large, so not on the task's stack. The handler and the updater run in tasks
   of their own, so each has its own. */
static char handler_http_buff[HANDLER_HTTP_BUFF_SIZE];
static char handler_mqtt_buff[HANDLER_MQTT_BUFF_SIZE];
static char handler_stream_buff[STREAM_BUFF_SIZE];
static char handler_resp_header_buff[RESP_HEADER_BUFF_SIZE];
static header_pool_t handler_headers;
static char updater_http_buff[UPDATER_HTTP_BUFF_SIZE];
static char updater_stream_buff[STREAM_BUFF_SIZE];
static char updater_resp_header_buff[RESP_HEADER_BUFF_SIZE];
static header_pool_t updater_headers;

static tio_handler_t handler;
static tio_updater_t updater;
static kii_session_t session;
static ebisu_clock_t device_clock;
static freertos_lock_t clock_lock;
static freertos_lock_t session_lock;
static socket_context_t handler_http_ctx;
static socket_context_t handler_mqtt_ctx;
static socket_context_t updater_http_ctx;

static const char state[] = EBISU_SAMPLE_STATE;
static size_t state_read;

static void log_time(void)
{
    int64_t now = 0;
    bool exact = ebisu_clock_get_time(&device_clock, &now);
    sample_log("the time: %lld, %s", (long long)now, exact ? "exact" : "a lower bound");
}

static const char* or_null(const char* s)
{
    return s[0] == '\0' ? NULL : s;
}

/* For the log. */
static const char* sock_error_name(ebisu_sock_error_t error)
{
    switch (error) {
        case EBISU_SOCK_ERROR_NONE: return "none";
        case EBISU_SOCK_ERROR_DNS: return "dns";
        case EBISU_SOCK_ERROR_CONNECT: return "connect";
        case EBISU_SOCK_ERROR_TLS: return "tls";
        case EBISU_SOCK_ERROR_VERIFY: return "verify";
        case EBISU_SOCK_ERROR_TIMEOUT: return "timeout";
        case EBISU_SOCK_ERROR_CLOSED: return "closed";
        case EBISU_SOCK_ERROR_OTHER: return "other";
    }
    return "?";
}

static void author_updated(const kii_author_t* author, void* userdata)
{
    /* A device that keeps its tokens across a restart saves the author here.
       This sample onboards at each start instead. */
    sample_log("author updated: %s", author->author_id);
}

static bool handler_continue(void* task_info, void* userdata)
{
    static kii_mqtt_task_state last = KII_MQTT_ST_DISCONTINUED;
    kii_mqtt_task_state task_state = ((tio_handler_task_info_t*)task_info)->task_state;
    if (task_state != last && task_state == KII_MQTT_ST_RECV_READY) {
        sample_log("handler ready: waiting for commands");
    }
    last = task_state;
    return true;
}

static bool updater_continue(void* task_info, void* userdata)
{
    return true;
}

static void task_exit(void* task_info, void* userdata)
{
    sample_log("%s task exited", (const char*)userdata);
}

static void task_error(tio_code_t code, const char* message, void* userdata)
{
    sample_log("%s: error %d: %s", (const char*)userdata, code, message);
}

static bool action_handler(tio_action_t* action, tio_action_err_t* err,
        tio_action_result_data_t* data, void* userdata)
{
    const tio_action_value_t* value = &action->action_value;
    if (value->type == TIO_TYPE_STRING) {
        sample_log("action %.*s %.*s: %.*s",
                (int)action->alias_length, action->alias,
                (int)action->action_name_length, action->action_name,
                (int)value->opaque_value_length, value->param.opaque_value);
    } else {
        sample_log("action %.*s %.*s",
                (int)action->alias_length, action->alias,
                (int)action->action_name_length, action->action_name);
    }
    return true;
}

static size_t state_size(void* userdata)
{
    sample_log("uploading the state");
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

static void init_handler(void)
{
    tio_handler_init(&handler);
    tio_handler_set_app(&handler, EBISU_SAMPLE_APP_ID, EBISU_SAMPLE_APP_HOST);
    tio_handler_set_system(&handler, freertos_system());
    tio_handler_set_cb_task_continue(&handler, handler_continue, NULL);
    tio_handler_set_cb_task_exit(&handler, task_exit, "handler");
    tio_handler_set_cb_err(&handler, task_error, "handler");

    tio_handler_set_http_socket(&handler, &handler_http_ctx.base);
    tio_handler_set_mqtt_socket(&handler, &handler_mqtt_ctx.base);
    tio_handler_set_mqtt_to_sock_recv(&handler, TO_RECV_SEC);

    tio_handler_set_http_buff(&handler, handler_http_buff, sizeof(handler_http_buff));
    tio_handler_set_mqtt_buff(&handler, handler_mqtt_buff, sizeof(handler_mqtt_buff));
    /* Otherwise each request would take these, and its headers, from the
       heap. */
    tio_handler_set_stream_buff(&handler, handler_stream_buff, sizeof(handler_stream_buff));
    tio_handler_set_resp_header_buff(&handler, handler_resp_header_buff,
            sizeof(handler_resp_header_buff));
    header_pool_init(&handler_headers);
    tio_handler_set_cb_slist_resource(&handler, header_pool_alloc, header_pool_free,
            &handler_headers, &handler_headers);
    tio_handler_set_keep_alive_interval(&handler, HANDLER_KEEP_ALIVE_SEC);
}

static void init_updater(void)
{
    tio_updater_init(&updater);
    tio_updater_set_app(&updater, EBISU_SAMPLE_APP_ID, EBISU_SAMPLE_APP_HOST);
    tio_updater_set_system(&updater, freertos_system());
    tio_updater_set_cb_task_continue(&updater, updater_continue, NULL);
    tio_updater_set_cb_task_exit(&updater, task_exit, "updater");
    tio_updater_set_cb_error(&updater, task_error, "updater");

    tio_updater_set_socket(&updater, &updater_http_ctx.base);

    tio_updater_set_buff(&updater, updater_http_buff, sizeof(updater_http_buff));
    tio_updater_set_stream_buff(&updater, updater_stream_buff, sizeof(updater_stream_buff));
    tio_updater_set_resp_header_buff(&updater, updater_resp_header_buff,
            sizeof(updater_resp_header_buff));
    header_pool_init(&updater_headers);
    tio_updater_set_cb_slist_resource(&updater, header_pool_alloc, header_pool_free,
            &updater_headers, &updater_headers);
    tio_updater_set_interval(&updater, EBISU_SAMPLE_UPDATE_PERIOD);
}

static void app_task(void* arg)
{
    net_wait_until_up();

    int ret = sock_cb_init_tls(ca_pem, ca_pem_size);
    if (ret != 0) {
        board_fatal("failed to set up TLS: -0x%04x", (unsigned int)-ret);
    }

    /* The time the server's certificate is checked at, which this sample takes
       from the server, the board having no real-time clock: until the first
       response, the clock has only a floor, a time it is known not to be
       earlier than. The clock has a lock of its own: the socket callbacks use
       it while kii may hold the session's. */
    freertos_lock_init(&clock_lock);
    ebisu_clock_init(&device_clock, freertos_system());
    ebisu_clock_set_lock(&device_clock, &clock_lock.base);
    if (EBISU_SAMPLE_CLOCK_FLOOR > 0) {
        ebisu_clock_set_floor(&device_clock, EBISU_SAMPLE_CLOCK_FLOOR);
    }
    log_time();

    sock_cb_init_context(&handler_http_ctx, TO_RECV_SEC, TO_SEND_SEC, &device_clock);
    sock_cb_init_context(&handler_mqtt_ctx, TO_RECV_SEC, TO_SEND_SEC, &device_clock);
    sock_cb_init_context(&updater_http_ctx, TO_RECV_SEC, TO_SEND_SEC, &device_clock);

    init_handler();
    init_updater();

    /* The handler and the updater share their tokens through the session,
       which renews them as they expire, and each response's date sets the
       clock. */
    kii_session_init(&session);
    freertos_lock_init(&session_lock);
    kii_session_set_lock(&session, &session_lock.base);
    kii_session_set_system(&session, freertos_system());
    kii_session_set_cb_author_updated(&session, author_updated, NULL);
    if (tio_handler_set_session(&handler, &session) != TIO_ERR_OK ||
            tio_updater_set_session(&updater, &session) != TIO_ERR_OK ||
            tio_handler_set_clock(&handler, &device_clock) != TIO_ERR_OK ||
            tio_updater_set_clock(&updater, &device_clock) != TIO_ERR_OK) {
        board_fatal("failed to set the session and the clock");
    }

    /* Used to onboard again when the server refuses the token, or the broker
       the MQTT credentials. */
    kii_session_set_password(&session, EBISU_SAMPLE_VENDOR_THING_ID,
            EBISU_SAMPLE_THING_PASSWORD);
    for (;;) {
        tio_code_t result = tio_handler_onboard(&handler,
                EBISU_SAMPLE_VENDOR_THING_ID,
                EBISU_SAMPLE_THING_PASSWORD,
                or_null(EBISU_SAMPLE_THING_TYPE),
                or_null(EBISU_SAMPLE_FIRMWARE_VERSION),
                NULL,
                NULL);
        if (result == TIO_ERR_OK) {
            break;
        }
        /* Why, if a connection failed. */
        long detail = 0;
        ebisu_sock_error_t sock_error = tio_handler_get_sock_error(&handler, &detail);
        sample_log("failed to onboard: %d, socket error %s, detail %ld; trying again in 10 seconds",
                result, sock_error_name(sock_error), detail);
        log_time();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    sample_log("onboarded");
    log_time();

    /* Both take the author from the session. */
    tio_handler_start(&handler, NULL, action_handler, NULL);
    tio_updater_start(&updater, NULL, state_size, NULL, state_read_cb, NULL);
    vTaskDelete(NULL);
}

int main(void)
{
    board_init();
    log_init();
    sample_log("ebisu %s", EBISU_VERSION_FULL);

    int ret = random_init();
    if (ret != 0) {
        board_fatal("failed to seed the random generator: -0x%04x", (unsigned int)-ret);
    }
    if (net_init() != pdPASS) {
        board_fatal("failed to start the network");
    }
    if (xTaskCreate(app_task, "app", APP_TASK_STACK_WORDS, NULL,
                tskIDLE_PRIORITY + 2, NULL) != pdPASS) {
        board_fatal("failed to create the application's task");
    }
    vTaskStartScheduler();
    /* It returns only if it could not start. */
    board_fatal("failed to start the scheduler");
}

/* FreeRTOS's hooks. */

void vApplicationMallocFailedHook(void)
{
    board_fatal("out of memory");
}

void vApplicationStackOverflowHook(TaskHandle_t task, char* name)
{
    board_fatal("stack overflow in task %s", name);
}
