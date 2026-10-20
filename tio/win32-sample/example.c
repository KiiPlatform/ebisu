/* The linux sample, example.c, on the Win32 API: the same tasks, callbacks
   and command line, with a console control handler for Ctrl-C, Winsock and
   Schannel for the connections, and Win32 threads for the tasks. */

#include "example.h"

#include <winsock2.h>
#include <windows.h>

#include <tio.h>
#include <jkii.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sock_cb_win32.h"
#include "sys_cb_win32.h"

static void print_help(void) {
    printf("sub commands: [onboard|update]\n\n");
    printf("to see detail usage of sub command, execute win32app {subcommand} --help\n\n");

    printf("onboard with vendor-thing-id\n");
    printf("win32app onboard --vendor-thing-id={vendor thing id} --password={password}\n\n");
}

static void print_onboard_usage(void) {
    printf("usage: \n");
    printf("onboard --vendor-thing-id={ID of the thing} --password={password of the thing}\n");
}

/* Set by the console control handler, which runs on a thread of its own, and
   by the tasks' exit callbacks, which run on theirs. */
static volatile LONG term_flag = 0;
static volatile LONG handler_terminated = 0;
static volatile LONG updater_terminated = 0;

static void set_flag(volatile LONG* flag) {
    InterlockedExchange(flag, 1);
}

static int is_set(volatile LONG* flag) {
    return InterlockedCompareExchange(flag, 0, 0) != 0;
}

static tio_bool_t _handler_continue(void* task_info, void* userdata) {
    return is_set(&term_flag) ? KII_FALSE : KII_TRUE;
}

static tio_bool_t _updater_continue(void* task_info, void* userdata) {
    return is_set(&term_flag) ? KII_FALSE : KII_TRUE;
}

static void _handler_exit(void* task_info, void* userdata) {
    printf("_handler_exit called\n");
    set_flag(&handler_terminated);
}

static void _updater_exit(void* task_info, void* userdata) {
    printf("_updater_exit called\n");
    set_flag(&updater_terminated);
}

/* Ctrl-C and Ctrl-Break ask the tasks to stop, and main() waits for them.
   Anything else, such as the console window closing, is left to the default
   handler, which ends the process. */
static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            set_flag(&term_flag);
            return TRUE;
        default:
            return FALSE;
    }
}

static void updater_init(
        tio_updater_t* updater,
        char* buffer,
        int buffer_size,
        void* sock_ssl_ctx,
        jkii_resource_t* resource)
{
    tio_updater_init(updater);

    tio_updater_set_app(updater, KII_APP_ID, KII_APP_HOST);

    tio_updater_set_cb_task_create(updater, task_create_cb_impl, NULL);
    tio_updater_set_cb_delay_ms(updater, delay_ms_cb_impl, NULL);

    tio_updater_set_buff(updater, buffer, buffer_size);

    tio_updater_set_cb_sock_connect(updater, sock_cb_connect, sock_ssl_ctx);
    tio_updater_set_cb_sock_send(updater, sock_cb_send, sock_ssl_ctx);
    tio_updater_set_cb_sock_recv(updater, sock_cb_recv, sock_ssl_ctx);
    tio_updater_set_cb_sock_close(updater, sock_cb_close, sock_ssl_ctx);

    tio_updater_set_interval(updater, UPDATE_PERIOD_SEC);

    tio_updater_set_json_parser_resource(updater, resource);

    tio_updater_set_cb_task_continue(updater, _updater_continue, NULL);
    tio_updater_set_cb_task_exit(updater, _updater_exit, NULL);
}

static const char send_file[] = "state.json";

typedef struct {
    size_t file_size;
    size_t file_read;
} updater_file_context_t;

static size_t updater_cb_state_size(void* userdata)
{
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    updater_file_context_t* ctx = (updater_file_context_t*)userdata;

    printf("Send state\n");
    if (GetFileAttributesExA(send_file, GetFileExInfoStandard, &attrs)) {
        ULONGLONG size =
            ((ULONGLONG)attrs.nFileSizeHigh << 32) | attrs.nFileSizeLow;
        ctx->file_size = (size_t)size;
        ctx->file_read = 0;
        return ctx->file_size;
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

    if (fseek(fp, (long)ctx->file_read, SEEK_SET) != 0) {
        printf("fseek error.\n");
        fclose(fp);
        return 0;
    }

    size_t read_size = fread(buffer, 1, size, fp);
    if (read_size > 0) {
        ctx->file_read += read_size;
    }

    fclose(fp);

    /* size_t is 64 bits and long 32 on 64-bit Windows. */
    printf("updater_cb_read: %lu / %lu\n",
            (unsigned long)ctx->file_read, (unsigned long)ctx->file_size);
    return read_size;
}

static tio_bool_t pushed_message_callback(const char* message, size_t message_length, void* userdata)
{
    printf("pushed_message_callback called,\n");
    printf("%.*s\n", (int)message_length, message);
    return KII_FALSE;
}

static void handler_init(
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

    tio_handler_set_cb_task_create(handler, task_create_cb_impl, NULL);
    tio_handler_set_cb_delay_ms(handler, delay_ms_cb_impl, NULL);

    tio_handler_set_cb_sock_connect_http(handler, sock_cb_connect, http_ssl_ctx);
    tio_handler_set_cb_sock_send_http(handler, sock_cb_send, http_ssl_ctx);
    tio_handler_set_cb_sock_recv_http(handler, sock_cb_recv, http_ssl_ctx);
    tio_handler_set_cb_sock_close_http(handler, sock_cb_close, http_ssl_ctx);

    tio_handler_set_cb_sock_connect_mqtt(handler, sock_cb_connect, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_send_mqtt(handler, sock_cb_send, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_recv_mqtt(handler, sock_cb_recv, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_recv_timed_out_mqtt(handler, sock_cb_recv_timed_out, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_close_mqtt(handler, sock_cb_close, mqtt_ssl_ctx);

    tio_handler_set_mqtt_to_sock_recv(handler, TO_RECV_SEC);
    tio_handler_set_mqtt_to_sock_send(handler, TO_SEND_SEC);

    tio_handler_set_http_buff(handler, http_buffer, http_buffer_size);
    tio_handler_set_mqtt_buff(handler, mqtt_buffer, mqtt_buffer_size);

    tio_handler_set_keep_alive_interval(handler, HANDLER_KEEP_ALIVE_SEC);

    tio_handler_set_json_parser_resource(handler, resource);

    tio_handler_set_cb_task_continue(handler, _handler_continue, NULL);
    tio_handler_set_cb_task_exit(handler, _handler_exit, NULL);
}

static tio_bool_t tio_action_handler(tio_action_t* action, tio_action_err_t* err, tio_action_result_data_t* data, void* userdata)
{
    printf("tio_action_handler called\n");
    printf("%.*s: %.*s\n", (int)action->alias_length, action->alias,(int)action->action_name_length, action->action_name);
    return KII_TRUE;
}

/* If argv[*index] is --name=value, or --name followed by the value in the
   next argument, as getopt_long() takes them, returns 1 with the value in
   *value, past which *index is moved. Returns 0 if it is another option, and
   -1 if it is this one without a value. */
static int match_option(int argc, char** argv, int* index, const char* name,
        char** value)
{
    const char* arg = argv[*index];
    size_t name_len = strlen(name);
    if (strncmp(arg, "--", 2) != 0 || strncmp(arg + 2, name, name_len) != 0) {
        return 0;
    }
    const char* rest = arg + 2 + name_len;
    if (*rest == '=') {
        *value = (char*)rest + 1;
        return 1;
    }
    if (*rest != '\0') {
        return 0;
    }
    if (*index + 1 >= argc) {
        printf("option '--%s' requires an argument\n", name);
        return -1;
    }
    *index += 1;
    *value = argv[*index];
    return 1;
}

int main(int argc, char** argv)
{
    /* The tasks print from threads of their own. Make what they print
       appear as they print it, also through a pipe: the Windows C runtime
       buffers fully there, and has no line buffering to ask for. */
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 2) {
        printf("too few arguments.\n");
        print_help();
        return 1;
    }

    /* Parse command. getopt_long() is not part of the Windows C runtime. */
    char* subc = argv[1];
    char* vendorThingID = NULL;
    char* password = NULL;
    if (strcmp(subc, "onboard") == 0) {
        for (int i = 2; i < argc; ++i) {
            int matched;
            if ((matched = match_option(argc, argv, &i, "vendor-thing-id", &vendorThingID)) != 0) {
                if (matched < 0) {
                    return 1;
                }
                printf("option vendor-thing-id : %s\n", vendorThingID);
            } else if ((matched = match_option(argc, argv, &i, "password", &password)) != 0) {
                if (matched < 0) {
                    return 1;
                }
                printf("option password : %s\n", password);
            } else if (strcmp(argv[i], "--help") == 0) {
                print_onboard_usage();
                return 0;
            } else {
                printf("unrecognized option '%s'\n", argv[i]);
                printf("unexpected usage.\n");
                print_onboard_usage();
                return 1;
            }
        }
        if (vendorThingID == NULL) {
            printf("neither vendor-thing-id is specified.\n");
            return 1;
        }
        if (password == NULL) {
            printf("password is not specified.\n");
            return 1;
        }
    } else {
        print_help();
        return strcmp(subc, "--help") == 0 || strcmp(subc, "help") == 0 ? 0 : 1;
    }

    // Ctrl-C, in place of the linux sample's SIGINT handler.
    if (!SetConsoleCtrlHandler(console_ctrl_handler, TRUE)) {
        printf("failed to register the console control handler\n");
        return 1;
    }

    WSADATA wsa_data;
    int wsa_err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_err != 0) {
        printf("WSAStartup failed: %d\n", wsa_err);
        return 1;
    }

    tio_updater_t updater;

    socket_context_t updater_http_ctx;
    sock_cb_init_context(&updater_http_ctx, TO_RECV_SEC, TO_SEND_SEC);

    jkii_token_t updater_tokens[256];
    jkii_resource_t updater_resource = {updater_tokens, 256};

    updater_file_context_t updater_file_ctx;

    char updater_buff[UPDATER_HTTP_BUFF_SIZE];
    memset(updater_buff, 0x00, sizeof(char) * UPDATER_HTTP_BUFF_SIZE);
    updater_init(
            &updater,
            updater_buff,
            UPDATER_HTTP_BUFF_SIZE,
            &updater_http_ctx,
            &updater_resource);

    tio_handler_t handler;

    socket_context_t handler_http_ctx;
    sock_cb_init_context(&handler_http_ctx, TO_RECV_SEC, TO_SEND_SEC);

    socket_context_t handler_mqtt_ctx;
    sock_cb_init_context(&handler_mqtt_ctx, TO_RECV_SEC, TO_SEND_SEC);

    char handler_http_buff[HANDLER_HTTP_BUFF_SIZE];
    memset(handler_http_buff, 0x00, sizeof(char) * HANDLER_HTTP_BUFF_SIZE);

    char handler_mqtt_buff[HANDLER_MQTT_BUFF_SIZE];
    memset(handler_mqtt_buff, 0x00, sizeof(char) * HANDLER_MQTT_BUFF_SIZE);

    jkii_token_t handler_tokens[256];
    jkii_resource_t handler_resource = {handler_tokens, 256};

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
            vendorThingID,
            password,
            NULL,
            NULL,
            NULL,
            NULL);
    if (result != TIO_ERR_OK) {
        printf("failed to onboard.\n");
        WSACleanup();
        return 1;
    }
    printf("Onboarding succeeded!\n");

    const kii_author_t* author = tio_handler_get_author(&handler);
    result = tio_handler_start(&handler, author, tio_action_handler, NULL);
    if (result != TIO_ERR_OK) {
        printf("failed to start the handler task.\n");
        WSACleanup();
        return 1;
    }
    result = tio_updater_start(
            &updater,
            author,
            updater_cb_state_size,
            &updater_file_ctx,
            updater_cb_read,
            &updater_file_ctx);
    int exit_code = 0;
    if (result != TIO_ERR_OK) {
        /* Stop the handler task, which is running, and wait for it below. */
        printf("failed to start the updater task.\n");
        set_flag(&term_flag);
        set_flag(&updater_terminated);
        exit_code = 1;
    }

    int disp_msg = 0;
    while (!(is_set(&handler_terminated) && is_set(&updater_terminated))) {
        Sleep(1000);
        if (is_set(&term_flag) && !disp_msg) {
            printf("Waiting for exiting tasks...\n");
            disp_msg = 1;
        }
    }

    WSACleanup();
    return exit_code;
}
