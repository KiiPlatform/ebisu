#include "example.h"

#include <tio.h>
#include <jkii.h>

#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <pthread.h>
#include <unistd.h>
#include "sys_cb_impl.h"
#include "sock_cb_linux.h"

static void print_help() {
    printf("sub commands: [onboard|update]\n\n");
    printf("to see detail usage of sub command, execute ./exampleapp {subcommand} --help\n\n");

    printf("onboard with vendor-thing-id\n");
    printf("./exampleapp onboard --vendor-thing-id={vendor thing id} --password={password}\n\n");
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

    tio_updater_set_cb_task_create(updater, task_create_cb_impl);
    tio_updater_set_cb_delay_ms(updater, delay_ms_cb_impl);

    tio_updater_set_buff(updater, buffer, buffer_size);

    tio_updater_set_cb_sock_connect(updater, sock_cb_connect, sock_ssl_ctx);
    tio_updater_set_cb_sock_send(updater, sock_cb_send, sock_ssl_ctx);
    tio_updater_set_cb_sock_recv(updater, sock_cb_recv, sock_ssl_ctx);
    tio_updater_set_cb_sock_close(updater, sock_cb_close, sock_ssl_ctx);

    tio_updater_set_interval(updater, UPDATE_PERIOD_SEC);

    tio_updater_set_json_parser_resource(updater, resource);
}

const char send_file[] = "state.json";

typedef struct {
    size_t file_size;
    size_t file_read;
} updater_file_context_t;

size_t updater_cb_state_size(void* userdata)
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

size_t updater_cb_read(char *buffer, size_t size, size_t count, void *userdata)
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

    size_t read_size = fread(buffer, 1, size * count, fp);
    if (read_size > 0) {
        ctx->file_read += read_size;
    }

    fclose(fp);

    printf("updater_cb_read: %ld / %ld\n", ctx->file_read, ctx->file_size);
    return read_size;
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

    tio_handler_set_cb_task_create(handler, task_create_cb_impl);
    tio_handler_set_cb_delay_ms(handler, delay_ms_cb_impl);

    tio_handler_set_cb_sock_connect_http(handler, sock_cb_connect, http_ssl_ctx);
    tio_handler_set_cb_sock_send_http(handler, sock_cb_send, http_ssl_ctx);
    tio_handler_set_cb_sock_recv_http(handler, sock_cb_recv, http_ssl_ctx);
    tio_handler_set_cb_sock_close_http(handler, sock_cb_close, http_ssl_ctx);

    tio_handler_set_cb_sock_connect_mqtt(handler, sock_cb_connect, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_send_mqtt(handler, sock_cb_send, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_recv_mqtt(handler, sock_cb_recv, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_close_mqtt(handler, sock_cb_close, mqtt_ssl_ctx);

    tio_handler_set_mqtt_to_sock_recv(handler, TO_RECV_SEC);
    tio_handler_set_mqtt_to_sock_send(handler, TO_SEND_SEC);

    tio_handler_set_http_buff(handler, http_buffer, http_buffer_size);
    tio_handler_set_mqtt_buff(handler, mqtt_buffer, mqtt_buffer_size);

    tio_handler_set_keep_alive_interval(handler, HANDLER_KEEP_ALIVE_SEC);

    tio_handler_set_json_parser_resource(handler, resource);
}

tio_bool_t tio_action_handler(tio_action_t* action, tio_action_err_t* err, void* userdata)
{
    printf("tio_action_handler called\n");
    printf("%.*s: %.*s\n", (int)action->alias_length, action->alias,(int)action->action_name_length, action->action_name);
    return KII_TRUE;
}

int main(int argc, char** argv)
{
    char* subc = argv[1];

    tio_updater_t updater;

    socket_context_t updater_http_ctx;
    updater_http_ctx.to_recv = TO_RECV_SEC;
    updater_http_ctx.to_send = TO_SEND_SEC;

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
    handler_http_ctx.to_recv = TO_RECV_SEC;
    handler_http_ctx.to_send = TO_SEND_SEC;

    socket_context_t handler_mqtt_ctx;
    handler_mqtt_ctx.to_recv = TO_RECV_SEC;
    handler_mqtt_ctx.to_send = TO_SEND_SEC;

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

    if (argc < 2) {
        printf("too few arguments.\n");
        print_help();
        exit(1);
    }

    /* Parse command. */
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
            int optIndex = 0;
            int c = getopt_long(argc, argv, "", longOptions, &optIndex);
            const char* optName = longOptions[optIndex].name;
            if (c == -1) {
                if (vendorThingID == NULL) {
                    printf("neither vendor-thing-id is specified.\n");
                    exit(1);
                }
                if (password == NULL) {
                    printf("password is not specifeid.\n");
                    exit(1);
                }
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
            printf("option %s : %s\n", optName, optarg);
            switch(c) {
                case 0:
                    vendorThingID = optarg;
                    break;
                case 1:
                    password = optarg;
                    break;
                case 2:
                    printf("usage: \n");
                    printf("onboard --vendor-thing-id={ID of the thing} --password={password of the thing}\n");
                    break;
                default:
                    printf("unexpected usage.\n");
            }
            if (strcmp(optName, "help") == 0) {
                exit(0);
            }
        }
    } else {
        print_help();
        exit(0);
    }

    const kii_author_t* author = tio_handler_get_author(&handler);
    tio_handler_start(&handler, author, tio_action_handler, NULL);
    tio_updater_start(
            &updater,
            author,
            updater_cb_state_size,
            &updater_file_ctx,
            updater_cb_read,
            &updater_file_ctx);

    /* run forever. TODO: Convert to daemon. */
    while(1){ sleep(1); };

}

/* vim: set ts=4 sts=4 sw=4 et fenc=utf-8 ff=unix: */

