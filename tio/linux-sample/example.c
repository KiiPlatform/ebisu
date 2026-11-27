#include "example.h"

#include <tio.h>
#include <jkii.h>

#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdatomic.h>
#include <stdbool.h>

#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <ebisu_loggers.h>
#include <ebisu_openssl.h>
#include <ebisu_posix.h>

static void print_help(void) {
    printf("sub commands: [onboard]\n\n");
    printf("to see detail usage of sub command, execute ./exampleapp {subcommand} --help\n\n");

    printf("onboard with vendor-thing-id\n");
    printf("./exampleapp onboard --vendor-thing-id={vendor thing id} --password={password}\n\n");
}

static void print_onboard_usage(void) {
    printf("usage: onboard --vendor-thing-id={ID of the thing} --password={password of the thing}\n");
}

// Using C11 atomic types.
static atomic_bool term_flag = false;
static atomic_bool handler_terminated = false;
static atomic_bool updater_terminated = false;

static bool handler_continue(void* task_info, void* userdata) {
    if (term_flag) {
        return false;
    } else {
        return true;
    }
}

static bool updater_continue(void* task_info, void* userdata) {
    if (term_flag) {
        return false;
    } else {
        return true;
    }
}

static void handler_exit(void* task_info, void* userdata) {
    printf("handler_exit called\n");
    handler_terminated = true;
}

static void updater_exit(void* task_info, void* userdata) {
    printf("updater_exit called\n");
    updater_terminated = true;
}

static void sig_handler(int sig, siginfo_t *info, void *ctx) {
    term_flag = true;
}

static ebisu_posix_lock_t session_lock;

static void author_updated_callback(const kii_author_t* author, void* userdata)
{
    // A device that keeps its tokens across a restart saves the author here.
    // This sample onboards at each start instead.
    printf("author updated: %s\n", author->author_id);
}

static void updater_init(
        tio_updater_t* updater,
        char* buffer,
        int buffer_size,
        ebisu_openssl_socket_t* sock_ssl_ctx)
{
    tio_updater_init(updater);

    tio_updater_set_app(updater, KII_APP_ID, KII_APP_HOST);

    tio_updater_set_system(updater, ebisu_posix_system());

    tio_updater_set_http_buff(updater, buffer, buffer_size);

    tio_updater_set_http_socket(updater, &sock_ssl_ctx->base);

    tio_updater_set_interval(updater, UPDATE_PERIOD_SEC);

    tio_updater_set_cb_task_continue(updater, updater_continue, NULL);
    tio_updater_set_cb_task_exit(updater, updater_exit, NULL);
}

// Read from the directory the sample runs in.
static const char send_file[] = "state.json";

typedef struct {
    size_t file_size;
    size_t file_read;
} updater_file_context_t;

static size_t updater_cb_state_size(void* userdata)
{
    struct stat st;
    updater_file_context_t* ctx = (updater_file_context_t*)userdata;

    printf("Send state\n");
    if (stat(send_file, &st) == 0) {
        ctx->file_size = st.st_size;
        ctx->file_read = 0;
        return st.st_size;
    } else {
        printf("failed to get stat\n");
    }
    return 0;
}

static size_t updater_cb_read(char *buffer, size_t size, void *userdata)
{
    updater_file_context_t* ctx = (updater_file_context_t*)userdata;
    FILE* fp;

    fp = fopen(send_file, "rb");
    if (fp == NULL) {
        printf("fopen error.\n");
        return 0;
    }

    if (fseek(fp, ctx->file_read, SEEK_SET) != 0) {
        printf("fseek error.\n");
        fclose(fp);
        return 0;
    }

    size_t read_size = fread(buffer, 1, size, fp);
    if (read_size > 0) {
        ctx->file_read += read_size;
    }

    fclose(fp);

    printf("updater_cb_read: %zu / %zu\n", ctx->file_read, ctx->file_size);
    return read_size;
}

static bool pushed_message_callback(const char* message, size_t message_length, void* userdata)
{
    printf("pushed_message_callback called,\n");
    printf("%.*s\n", (int)message_length, message);
    return false;
}

static void handler_init(
        tio_handler_t* handler,
        char* http_buffer,
        int http_buffer_size,
        ebisu_openssl_socket_t* http_ssl_ctx,
        char* mqtt_buffer,
        int mqtt_buffer_size,
        ebisu_openssl_socket_t* mqtt_ssl_ctx)
{
    tio_handler_init(handler);

    tio_handler_set_app(handler, KII_APP_ID, KII_APP_HOST);

    tio_handler_set_cb_push(handler, pushed_message_callback, NULL);

    tio_handler_set_system(handler, ebisu_posix_system());

    tio_handler_set_http_socket(handler, &http_ssl_ctx->base);
    tio_handler_set_mqtt_socket(handler, &mqtt_ssl_ctx->base);

    tio_handler_set_mqtt_to_sock_recv(handler, TO_RECV_SEC);

    tio_handler_set_http_buff(handler, http_buffer, http_buffer_size);
    tio_handler_set_mqtt_buff(handler, mqtt_buffer, mqtt_buffer_size);

    tio_handler_set_keep_alive_interval(handler, HANDLER_KEEP_ALIVE_SEC);

    tio_handler_set_cb_task_continue(handler, handler_continue, NULL);
    tio_handler_set_cb_task_exit(handler, handler_exit, NULL);
}

static bool tio_action_handler(tio_action_t* action, tio_action_err_t* err, tio_action_result_data_t* data, void* userdata)
{
    printf("tio_action_handler called\n");
    printf("%.*s: %.*s\n", (int)action->alias_length, action->alias,(int)action->action_name_length, action->action_name);
    return true;
}

int main(int argc, char** argv)
{
    // Each line as it is printed, to a file or a service's log too, which
    // would otherwise have them only as the buffer fills.
    setvbuf(stdout, NULL, _IOLBF, 0);

    // Ctrl-C, or the service being stopped, ends the tasks, and then the
    // program. SIGPIPE needn't be ignored: the sockets never raise it for a
    // peer that has gone.
    struct sigaction sa_term;
    memset(&sa_term, 0, sizeof(sa_term));
    sa_term.sa_sigaction = sig_handler;
    sa_term.sa_flags = SA_SIGINFO;

    if (sigaction(SIGINT, &sa_term, NULL) < 0 || sigaction(SIGTERM, &sa_term, NULL) < 0) {
        printf("failed to register sigaction\n");
        exit(1);
    }

    // The time the server's certificate is checked at: the system's, which
    // keeps it. A device without a real-time clock takes it from the server
    // instead, with an ebisu_server_clock_t, as the FreeRTOS sample does.
    ebisu_posix_lock_init(&session_lock);
    ebisu_clock_t* clock = ebisu_system_clock(ebisu_posix_system());

    // Why a connection failed, to stderr: the sockets log nothing without a
    // logger.
    ebisu_stdio_logger_t to_stderr;
    ebisu_stdio_logger_init(&to_stderr, stderr, EBISU_LOG_WARNING);

    tio_updater_t updater;

    ebisu_openssl_socket_t updater_http_ctx;
    ebisu_openssl_socket_init(&updater_http_ctx, TO_RECV_SEC, TO_SEND_SEC);
    ebisu_openssl_socket_set_clock(&updater_http_ctx, clock);
    ebisu_openssl_socket_set_logger(&updater_http_ctx, &to_stderr.base);

    updater_file_context_t updater_file_ctx;

    char updater_buff[UPDATER_HTTP_BUFF_SIZE];
    memset(updater_buff, 0x00, sizeof(char) * UPDATER_HTTP_BUFF_SIZE);
    updater_init(
            &updater,
            updater_buff,
            UPDATER_HTTP_BUFF_SIZE,
            &updater_http_ctx);

    tio_handler_t handler;

    ebisu_openssl_socket_t handler_http_ctx;
    ebisu_openssl_socket_init(&handler_http_ctx, TO_RECV_SEC, TO_SEND_SEC);
    ebisu_openssl_socket_set_clock(&handler_http_ctx, clock);
    ebisu_openssl_socket_set_logger(&handler_http_ctx, &to_stderr.base);

    ebisu_openssl_socket_t handler_mqtt_ctx;
    ebisu_openssl_socket_init(&handler_mqtt_ctx, TO_RECV_SEC, TO_SEND_SEC);
    ebisu_openssl_socket_set_clock(&handler_mqtt_ctx, clock);
    ebisu_openssl_socket_set_logger(&handler_mqtt_ctx, &to_stderr.base);

    char handler_http_buff[HANDLER_HTTP_BUFF_SIZE];
    memset(handler_http_buff, 0x00, sizeof(char) * HANDLER_HTTP_BUFF_SIZE);

    char handler_mqtt_buff[HANDLER_MQTT_BUFF_SIZE];
    memset(handler_mqtt_buff, 0x00, sizeof(char) * HANDLER_MQTT_BUFF_SIZE);

    handler_init(
            &handler,
            handler_http_buff,
            HANDLER_HTTP_BUFF_SIZE,
            &handler_http_ctx,
            handler_mqtt_buff,
            HANDLER_MQTT_BUFF_SIZE,
            &handler_mqtt_ctx);

    // The handler and the updater share their tokens through the session, which
    // renews them as they expire.
    kii_session_t session;
    kii_session_init(&session);
    kii_session_set_lock(&session, &session_lock.base);
    kii_session_set_system(&session, ebisu_posix_system());
    kii_session_set_cb_author_updated(&session, author_updated_callback, NULL);
    if (tio_handler_set_session(&handler, &session) != TIO_ERR_OK ||
            tio_updater_set_session(&updater, &session) != TIO_ERR_OK) {
        printf("failed to set the session.\n");
        exit(1);
    }

    if (argc < 2) {
        printf("too few arguments.\n");
        print_help();
        exit(1);
    }

    /* Parse command. */
    const char* subc = argv[1];
    if (strcmp(subc, "onboard") == 0) {
        char* vendorThingID = NULL;
        char* password = NULL;
        while(1) {
            struct option longOptions[] = {
                {"vendor-thing-id", required_argument, 0, 0},
                {"password", required_argument, 0, 1},
                {"help", no_argument, 0, 2},
                {0, 0, 0, 0}
            };
            int c = getopt_long(argc, argv, "", longOptions, NULL);
            if (c == -1) {
                if (vendorThingID == NULL) {
                    printf("vendor-thing-id is not specified.\n");
                    exit(1);
                }
                if (password == NULL) {
                    printf("password is not specified.\n");
                    exit(1);
                }
                // Used to onboard again when the server refuses the token, or the broker
                // the MQTT credentials.
                kii_session_set_password(&session, vendorThingID, password);
                tio_code_t result = tio_handler_onboard(
                        &handler,
                        vendorThingID,
                        password,
                        NULL,
                        NULL,
                        NULL,
                        NULL);
                if (result != TIO_ERR_OK) {
                    printf("failed to onboard.\n");
                    exit(1);
                }
                printf("Onboarding succeeded!\n");
                break;
            }
            switch(c) {
                case 0:
                    printf("option vendor-thing-id : %s\n", optarg);
                    vendorThingID = optarg;
                    break;
                case 1:
                    // Not the password, which a terminal's scrollback and a
                    // service's log keep.
                    printf("option password : (given)\n");
                    password = optarg;
                    break;
                case 2:
                    print_onboard_usage();
                    exit(0);
                default:
                    // An option it doesn't have, or one without its value,
                    // which getopt_long() has said.
                    print_onboard_usage();
                    exit(1);
            }
        }
    } else {
        printf("unknown sub command: %s\n", subc);
        print_help();
        exit(1);
    }

    // Both take the author from the session. A task that doesn't start never
    // ends either, for the loop below to wait for.
    if (tio_handler_start(&handler, NULL, tio_action_handler, NULL) != TIO_ERR_OK) {
        printf("failed to start the handler.\n");
        exit(1);
    }
    if (tio_updater_start(
            &updater,
            NULL,
            updater_cb_state_size,
            &updater_file_ctx,
            updater_cb_read,
            &updater_file_ctx) != TIO_ERR_OK) {
        printf("failed to start the updater.\n");
        // The handler's task ends at its next turn.
        term_flag = true;
        while (!handler_terminated) {
            sleep(1);
        }
        exit(1);
    }

    bool end = false;
    bool disp_msg = false;
    while(!end){
        sleep(1);
        if (term_flag && !disp_msg) {
            printf("Waiting for exiting tasks...\n");
            disp_msg = true;
        }
        if (handler_terminated && updater_terminated) {
            end = true;
        }
    }
}

/* vim: set ts=4 sts=4 sw=4 et fenc=utf-8 ff=unix: */

