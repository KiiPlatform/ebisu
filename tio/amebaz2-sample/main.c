/* tio on Realtek's AmebaZ2, with the SDK's FreeRTOS, lwIP and mbedTLS: the
   firmware joins the Wi-Fi network, onboards, uploads its state periodically,
   and logs the commands it receives. In place of the SDK's example's main.c,
   which build.sh compiles it instead of. */
#include <string.h>

#include <ebisu_version.h>
#include <tio.h>

#include <ebisu_freertos.h>
#include <ebisu_lwip.h>
#include <ebisu_mbedtls.h>
#include <ebisu_server_clock.h>

#include <FreeRTOS.h>
#include <task.h>

#include <mbedtls/platform.h>
#include <mbedtls/ssl.h>

#include "main.h"

#include "header_pool.h"
#include "log.h"
#include "net.h"
#include "random.h"
#include "sample_config.h"

#define HANDLER_HTTP_BUFF_SIZE 4096
#define HANDLER_MQTT_BUFF_SIZE 2048
#define HANDLER_KEEP_ALIVE_SEC 300
#define UPDATER_HTTP_BUFF_SIZE 4096
#define STREAM_BUFF_SIZE 1024
#define RESP_HEADER_BUFF_SIZE 256

#define TO_RECV_SEC 15
#define TO_SEND_SEC 15
/* The TLS handshake as a whole: each wait in it has the receive timeout, and
   a server that sends a little at a time would otherwise hold it as long as
   it likes. Room for a handshake of software cryptography, at this chip's
   speed. */
#define HANDSHAKE_SEC 30

/* In words. Each task makes TLS connections, whose handshake takes the most
   of it: the application's, which onboards, and the handler's and the
   updater's, which ebisu makes. */
#define TASK_STACK_WORDS 3072
/* Below the SDK's: lwIP's, the Wi-Fi's and the timers'. */
#define TASK_PRIORITY (tskIDLE_PRIORITY + 1)

/* From EBISU_SAMPLE_CA_FILE, in ca_pem.c, which build.sh generates. */
extern const char ca_pem[];
extern const size_t ca_pem_size;

/* The SDK's: the console, and the task that starts lwIP and the Wi-Fi. */
extern void console_init(void);

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
static ebisu_freertos_system_t sample_system;
static ebisu_server_clock_t device_clock;
static ebisu_freertos_lock_t clock_lock;
static ebisu_freertos_lock_t session_lock;
/* Each connection: TLS with ebisu's mbedTLS port, over TCP with its lwIP
   port. The connections share a TLS configuration, of the CA certificates. */
typedef struct {
    ebisu_lwip_tcp_socket_t tcp;
    ebisu_mbedtls_socket_t tls;
} sample_socket_t;
static ebisu_mbedtls_tls_t tls_config;
static sample_socket_t handler_http_socket;
static sample_socket_t handler_mqtt_socket;
static sample_socket_t updater_http_socket;

static const char state[] = EBISU_SAMPLE_STATE;
static size_t state_read;

static void log_time(void)
{
    int64_t now = 0;
    bool exact = ebisu_clock_get_time(&device_clock.base, &now);
    /* In seconds since 1970, which fit in 32 bits until 2106: the SDK's
       printf may not take a long long. */
    sample_log("the time: %lu, %s", (unsigned long)now, exact ? "exact" : "a lower bound");
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
        case EBISU_SOCK_ERROR_TLS_MISMATCH: return "tls mismatch";
    }
    return "?";
}

static void author_updated(const kii_author_t* author, void* userdata)
{
    (void)userdata;
    /* A device that keeps its tokens across a restart saves the author here.
       This sample onboards at each start instead. */
    sample_log("author updated: %s", author->author_id);
}

static bool handler_continue(void* task_info, void* userdata)
{
    static kii_mqtt_task_state_t last = KII_MQTT_ST_DISCONTINUED;
    kii_mqtt_task_state_t task_state = ((tio_handler_task_info_t*)task_info)->task_state;
    (void)userdata;
    if (task_state != last && task_state == KII_MQTT_ST_RECV_READY) {
        sample_log("handler ready: waiting for commands");
    }
    last = task_state;
    return true;
}

static bool updater_continue(void* task_info, void* userdata)
{
    (void)task_info;
    (void)userdata;
    return true;
}

static void task_exit(void* task_info, void* userdata)
{
    (void)task_info;
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
    (void)err;
    (void)data;
    (void)userdata;
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
    (void)userdata;
    sample_log("uploading the state");
    state_read = 0;
    return strlen(state);
}

static size_t state_read_cb(char* buffer, size_t size, void* userdata)
{
    size_t left = strlen(state) - state_read;
    size_t n = size < left ? size : left;
    (void)userdata;
    memcpy(buffer, state + state_read, n);
    state_read += n;
    return n;
}

static void init_handler(void)
{
    tio_handler_init(&handler);
    tio_handler_set_app(&handler, EBISU_SAMPLE_APP_ID, EBISU_SAMPLE_APP_HOST);
    tio_handler_set_system(&handler, &sample_system.base);
    tio_handler_set_cb_task_continue(&handler, handler_continue, NULL);
    tio_handler_set_cb_task_exit(&handler, task_exit, "handler");
    tio_handler_set_cb_err(&handler, task_error, "handler");

    tio_handler_set_http_socket(&handler, &handler_http_socket.tls.base);
    tio_handler_set_mqtt_socket(&handler, &handler_mqtt_socket.tls.base);
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
    tio_updater_set_system(&updater, &sample_system.base);
    tio_updater_set_cb_task_continue(&updater, updater_continue, NULL);
    tio_updater_set_cb_task_exit(&updater, task_exit, "updater");
    tio_updater_set_cb_err(&updater, task_error, "updater");

    tio_updater_set_http_socket(&updater, &updater_http_socket.tls.base);

    tio_updater_set_http_buff(&updater, updater_http_buff, sizeof(updater_http_buff));
    tio_updater_set_stream_buff(&updater, updater_stream_buff, sizeof(updater_stream_buff));
    tio_updater_set_resp_header_buff(&updater, updater_resp_header_buff,
            sizeof(updater_resp_header_buff));
    header_pool_init(&updater_headers);
    tio_updater_set_cb_slist_resource(&updater, header_pool_alloc, header_pool_free,
            &updater_headers, &updater_headers);
    tio_updater_set_interval(&updater, EBISU_SAMPLE_UPDATE_PERIOD);
}

/* mbedTLS's allocations, as the SDK builds it with MBEDTLS_PLATFORM_MEMORY
   and no allocator of its own: from FreeRTOS's heap. */
static void* tls_calloc(size_t count, size_t size)
{
    void* p;
    if (size != 0 && count > (size_t)-1 / size) {
        return NULL;
    }
    p = pvPortMalloc(count * size);
    if (p != NULL) {
        memset(p, 0, count * size);
    }
    return p;
}

/* The connections' TLS configuration: the CA certificates, a random
   generator, and records no larger than mbedTLS takes. */
static void init_tls(void)
{
    mbedtls_ssl_config* conf;
    int ret;

    mbedtls_platform_set_calloc_free(tls_calloc, vPortFree);
    if (!ebisu_mbedtls_tls_init(&tls_config, (const unsigned char*)ca_pem, ca_pem_size, &ret)) {
        sample_fatal("failed to set up TLS: -0x%04x", (unsigned int)-ret);
    }
    conf = ebisu_mbedtls_tls_config(&tls_config);
#if !EBISU_MBEDTLS_SETS_RNG
    /* The SDK builds mbedTLS without a CTR_DRBG: see random.h. */
    mbedtls_ssl_conf_rng(conf, sample_random, NULL);
#endif
#if defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH) && defined(MBEDTLS_SSL_MAX_CONTENT_LEN) && \
    MBEDTLS_SSL_MAX_CONTENT_LEN < 16384
    /* The SDK's mbedTLS takes records of MBEDTLS_SSL_MAX_CONTENT_LEN, 4096
       bytes, where a server may send 16384: the server is asked for records
       no larger, which one that doesn't take the extension ignores. */
#if MBEDTLS_SSL_MAX_CONTENT_LEN >= 4096
    ret = mbedtls_ssl_conf_max_frag_len(conf, MBEDTLS_SSL_MAX_FRAG_LEN_4096);
#elif MBEDTLS_SSL_MAX_CONTENT_LEN >= 2048
    ret = mbedtls_ssl_conf_max_frag_len(conf, MBEDTLS_SSL_MAX_FRAG_LEN_2048);
#elif MBEDTLS_SSL_MAX_CONTENT_LEN >= 1024
    ret = mbedtls_ssl_conf_max_frag_len(conf, MBEDTLS_SSL_MAX_FRAG_LEN_1024);
#else
    ret = mbedtls_ssl_conf_max_frag_len(conf, MBEDTLS_SSL_MAX_FRAG_LEN_512);
#endif
    if (ret != 0) {
        sample_fatal("failed to set the record size: -0x%04x", (unsigned int)-ret);
    }
#else
    (void)conf;
#endif
}

/* TCP with its timeouts, and TLS over it, whose certificates are checked at
   the clock's time, each logging why it fails. */
static void init_socket(sample_socket_t* sock)
{
    ebisu_lwip_tcp_socket_init(&sock->tcp, TO_RECV_SEC, TO_SEND_SEC);
    ebisu_lwip_tcp_socket_set_logger(&sock->tcp, sample_logger());
    ebisu_mbedtls_socket_init(&sock->tls, &sock->tcp.base, &tls_config);
    /* The system's clock: the server clock app_task() sets on it. */
    ebisu_mbedtls_socket_set_clock(&sock->tls, ebisu_system_clock(&sample_system.base));
    ebisu_mbedtls_socket_set_logger(&sock->tls, sample_logger());
    ebisu_mbedtls_socket_set_handshake_timeout(&sock->tls, &sample_system.base, HANDSHAKE_SEC);
}

static void app_task(void* arg)
{
    (void)arg;
    net_wait_until_up();
    init_tls();

    /* The time the server's certificate is checked at, which this sample takes
       from the server, the chip having no clock that keeps the time across a
       restart: until the first response, the clock has only a floor, a time
       it is known not to be earlier than. The clock has a lock of its own: the
       sockets use it while kii may hold the session's. FreeRTOS keeps no time
       of day, so the system gives this one as its clock. */
    if (!ebisu_freertos_lock_init(&clock_lock)) {
        sample_fatal("failed to create the clock's lock");
    }
    ebisu_server_clock_init(&device_clock, &sample_system.base);
    ebisu_server_clock_set_lock(&device_clock, &clock_lock.base);
    if (EBISU_SAMPLE_CLOCK_FLOOR > 0) {
        ebisu_server_clock_set_floor(&device_clock, EBISU_SAMPLE_CLOCK_FLOOR);
    }
    ebisu_freertos_system_set_clock(&sample_system, &device_clock.base);
    log_time();

    init_socket(&handler_http_socket);
    init_socket(&handler_mqtt_socket);
    init_socket(&updater_http_socket);

    init_handler();
    init_updater();

    /* The handler and the updater share their tokens through the session,
       which renews them as they expire, and each response's date sets the
       clock. */
    kii_session_init(&session);
    if (!ebisu_freertos_lock_init(&session_lock)) {
        sample_fatal("failed to create the session's lock");
    }
    kii_session_set_lock(&session, &session_lock.base);
    kii_session_set_system(&session, &sample_system.base);
    kii_session_set_cb_author_updated(&session, author_updated, NULL);
    if (tio_handler_set_session(&handler, &session) != TIO_ERR_OK ||
            tio_updater_set_session(&updater, &session) != TIO_ERR_OK ||
            tio_handler_set_clock(&handler, &device_clock.base) != TIO_ERR_OK ||
            tio_updater_set_clock(&updater, &device_clock.base) != TIO_ERR_OK) {
        sample_fatal("failed to set the session and the clock");
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
        long detail = 0;
        ebisu_sock_error_t sock_error;
        if (result == TIO_ERR_OK) {
            break;
        }
        /* Why, if a connection failed. */
        sock_error = tio_handler_get_sock_error(&handler, &detail);
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
    /* The SDK's console, which the sample logs to, and its commands. */
    console_init();
    log_init();
    sample_log("ebisu %s", EBISU_VERSION_FULL);
    random_init();

    /* The SDK's task that starts lwIP and the Wi-Fi, which app_task() waits
       for. */
    wlan_network();

    /* The tasks ebisu makes, the handler's and the updater's, as the
       application's. */
    ebisu_freertos_system_init(&sample_system, TASK_STACK_WORDS, TASK_PRIORITY);
    if (xTaskCreate(app_task, "app", TASK_STACK_WORDS, NULL, TASK_PRIORITY, NULL) != pdPASS) {
        sample_fatal("failed to create the application's task");
    }
    vTaskStartScheduler();
    /* It returns only if it could not start. */
    sample_fatal("failed to start the scheduler");
    return 0;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
