#include "kii_core_impl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define BUFF_SIZE 256

typedef enum prv_ssl_state_t {
    PRV_SSL_STATE_IDLE,
    PRV_SSL_STATE_CONNECT,
    PRV_SSL_STATE_SEND,
    PRV_SSL_STATE_RECV,
    PRV_SSL_STATE_CLOSE
} prv_ssl_state_t;
    
typedef struct context_t
{
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    int sock;
    char host[256];
    prv_ssl_state_t state;
    int last_chunk;
    int sent_size;
    int received_size;
} context_t;

context_t g_ctx;

kii_http_client_code_t prv_ssl_connect(kii_http_context_t* http_context,
        const char* host)
{
    int sock, ret;
    struct hostent *servhost;
    struct sockaddr_in server;
    struct servent *service;
    context_t* ctx = (context_t*)http_context->app_context;
    SSL *ssl;
    SSL_CTX *ssl_ctx;

    printf("host: %s\n", host);
    
    servhost = gethostbyname(host);
    if (servhost == NULL) {
        printf("failed to get host.\n");
        return KII_HTTPC_FAIL;
    }
    memset(&server, 0x00, sizeof(server));
    server.sin_family = AF_INET;
    /* More secure. */
    memcpy(&(server.sin_addr), servhost->h_addr, servhost->h_length);

    /* Get Port number */
    service = getservbyname("https", "tcp");
    if (service != NULL) {
        server.sin_port = service->s_port;
    } else {
        server.sin_port = htons(443);
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("failed to init socket.\n");
        return KII_HTTPC_FAIL;
    }

    if (connect(sock, (struct sockaddr*) &server, sizeof(server)) == -1 ){
        printf("failed to connect socket.\n");
        return KII_HTTPC_FAIL;
    }

    SSL_library_init();
    ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    if (ssl_ctx == NULL){
        printf("failed to init ssl context.\n");
        return KII_HTTPC_FAIL;
    }

    ssl = SSL_new(ssl_ctx);
    if (ssl == NULL){
        printf("failed to init ssl.\n");
        return KII_HTTPC_FAIL;
    }

    ret = SSL_set_fd(ssl, sock);
    if (ret == 0){
        printf("failed to set fd.\n");
        return KII_HTTPC_FAIL;
    }

    ret = SSL_connect(ssl);
    if (ret != 1) {
        int sslErr= SSL_get_error(ssl, ret);
        char sslErrStr[120];
        ERR_error_string_n(sslErr, sslErrStr, 120);
        printf("failed to connect: %s\n", sslErrStr);
        return KII_HTTPC_FAIL;
    }
    ctx->sock = sock;
    ctx->ssl = ssl;
    ctx->ssl_ctx = ssl_ctx;
    return KII_HTTPC_OK;
}

kii_http_client_code_t prv_ssl_send(kii_http_context_t* http_context, const char* send_buff, int buff_length)
{
    context_t* ctx = (context_t*)http_context->app_context;
    int ret = SSL_write(ctx->ssl, send_buff, buff_length);
    if (ret > 0) {
        return KII_HTTPC_OK;
    } else {
        printf("failed to send\n");
        return KII_HTTPC_FAIL;
    }
}

kii_http_client_code_t prv_ssl_recv(kii_http_context_t* http_context, char* recv_buff, int length_to_read, int* out_actual_length)
{
    context_t* ctx = (context_t*)http_context->app_context;
    int ret = SSL_read(ctx->ssl, recv_buff, length_to_read);
    if (ret > 0) {
        *out_actual_length = ret;
        return KII_HTTPC_OK;
    } else {
        printf("failed to receive:\n");
        /* TOOD: could be 0 on success? */
        *out_actual_length = 0;
        return KII_HTTPC_FAIL;
    }
}


kii_http_client_code_t prv_ssl_close(kii_http_context_t* http_context)
{
    context_t* ctx = (context_t*)http_context->app_context;
    int ret = SSL_shutdown(ctx->ssl);
    if (ret != 1) {
        int sslErr = SSL_get_error(ctx->ssl, ret);
        if (sslErr == SSL_ERROR_SYSCALL) {
            /* This is OK.*/
            /* See https://www.openssl.org/docs/ssl/SSL_shutdown.html */
            ret = 1;
        } else {
            char sslErrStr[120];
            ERR_error_string_n(sslErr, sslErrStr, 120);
            printf("failed to shutdown: %s\n", sslErrStr);
        }
    }
    close(ctx->sock);
    SSL_free(ctx->ssl);
    SSL_CTX_free(ctx->ssl_ctx);
    if (ret != 1) {
        printf("failed to close:\n");
        return KII_HTTPC_FAIL;
    }
    return KII_HTTPC_OK;
}

/* HTTP Callback functions */
kii_http_client_code_t
    request_line_cb(
        kii_http_context_t* http_context,
        const char* method,
        const char* host,
        const char* path)
{
    char* reqBuff;
    
    memset(&g_ctx, 0x00, sizeof(context_t));
    http_context->app_context = &g_ctx;
    reqBuff = http_context->buffer;
    strncpy(g_ctx.host, host, strlen(host));
    sprintf(reqBuff, "%s https://%s/%s HTTP/1.1\r\n", method, host, path);

    return KII_HTTPC_OK;
}

kii_http_client_code_t
    header_cb(
        kii_http_context_t* http_context,
        const char* key,
        const char* value)
{
    char* reqBuff = http_context->buffer;
    strcat(reqBuff, key);
    strcat(reqBuff, ":");
    strcat(reqBuff, value);
    strcat(reqBuff, "\r\n");
    return KII_HTTPC_OK;
}

kii_http_client_code_t
    body_cb(
        kii_http_context_t* http_context,
        const char* body_data)
{
    char* reqBuff = http_context->buffer;
    strcat(reqBuff, "\r\n");
    if (body_data != NULL) {
        strcat(reqBuff, body_data);
    }
    return KII_HTTPC_OK;
}

kii_http_client_code_t
    execute_cb(
        kii_http_context_t* http_context,
        int* response_code,
        char** response_body)
{
    context_t* ctx;

    ctx = &g_ctx;
    
    kii_http_client_code_t res;

    printf("client state: %d\n", ctx->state);
    switch (ctx->state) {
        case PRV_SSL_STATE_IDLE:
            ctx->sent_size = 0;
            ctx->last_chunk = 0;
            ctx->received_size = 0;
            ctx->state = PRV_SSL_STATE_CONNECT;
            return KII_HTTPC_AGAIN;
        case PRV_SSL_STATE_CONNECT:
            res = prv_ssl_connect(http_context, ctx->host);
            if (res == KII_HTTPC_OK) {
                ctx->state = PRV_SSL_STATE_SEND;
                return KII_HTTPC_AGAIN;
            } else if (res == KII_HTTPC_AGAIN) {
                return KII_HTTPC_AGAIN;
            } else {
                ctx->state = PRV_SSL_STATE_IDLE;
                return KII_HTTPC_FAIL;
            }
        case PRV_SSL_STATE_SEND:
        {
            char* sendBuff = NULL;
            int size = BUFF_SIZE;
            int remain = strlen(http_context->buffer) - ctx->sent_size;
            if (remain < size) {
                size = remain;
                ctx->last_chunk = 1;
            }
            sendBuff = http_context->buffer + ctx->sent_size;
            res = prv_ssl_send(
                    http_context,
                    sendBuff,
                    size);
            if (res == KII_HTTPC_OK) {
                ctx->sent_size += size;
                if (ctx->last_chunk > 0) {
                    ctx->state = PRV_SSL_STATE_RECV;
                }
                return KII_HTTPC_AGAIN;
            } else if(res == KII_HTTPC_AGAIN) {
                return KII_HTTPC_AGAIN;
            } else {
                ctx->state = PRV_SSL_STATE_IDLE;
                return KII_HTTPC_FAIL;
            }
        }
        case PRV_SSL_STATE_RECV:
        {
            int actualLength = 0;
            char* buffPtr = http_context->buffer + ctx->received_size;
            if (ctx->received_size == 0) {
                memset(http_context->buffer, 0x00, http_context->buffer_size);
            }
            res = prv_ssl_recv(http_context, buffPtr, BUFF_SIZE, &actualLength);
            if (res == KII_HTTPC_OK) {
                ctx->received_size += actualLength;
                if (actualLength < BUFF_SIZE) {
                    ctx->state = PRV_SSL_STATE_CLOSE;
                }
                return KII_HTTPC_AGAIN;
            } else if (res == KII_HTTPC_AGAIN) {
                return KII_HTTPC_AGAIN;
            } else {
                ctx->state = PRV_SSL_STATE_IDLE;
                return KII_HTTPC_FAIL;
            }
        }
        case PRV_SSL_STATE_CLOSE:
        {
            res = prv_ssl_close(http_context);
            if (res == KII_HTTPC_OK) {
                /* parse status code */
                char* statusPtr = strstr(http_context->buffer, "HTTP/1.1 ");
                int numCode = 0;
                char* bodyPtr = NULL;
                if (statusPtr != NULL) {
                    char c_status_code[4];
                    c_status_code[3] = '\0';
                    statusPtr += strlen("HTTP/1.1 ");
                    memcpy(c_status_code, statusPtr, 3);
                    numCode = atoi(c_status_code);
                    *response_code = numCode;
                }
                /* set body pointer */
                bodyPtr = strstr(http_context->buffer, "\r\n\r\n");
                if (bodyPtr != NULL) {
                    bodyPtr += 4;
                }
                *response_body = bodyPtr;
                ctx->state = PRV_SSL_STATE_IDLE;
                return KII_HTTPC_OK;
            } else if (res == KII_HTTPC_AGAIN) {
                return KII_HTTPC_AGAIN;
            } else {
                ctx->state = PRV_SSL_STATE_IDLE;
                return KII_HTTPC_FAIL;
            }
        }
    }
    return KII_HTTPC_OK;
}

void logger_cb(const char* format, ...)
{
    va_list list;
    va_start(list, format);
    vprintf(format, list);
    va_end(list);
}

