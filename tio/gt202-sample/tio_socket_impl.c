#include "tio_socket_impl.h"

static SSL_CTX *_ssl_ctx = NULL;

void ssl_ctx_init() {
#if CONNECT_SSL
    if (_ssl_ctx == NULL) {
        _ssl_ctx = SSL_ctx_new(SSL_CLIENT, 4500, 2000, 0);
    }
#endif
}

void ssl_ctx_close() {
#if CONNECT_SSL
    if (_ssl_ctx != NULL)
    {
        SSL_ctx_free(_ssl_ctx);
        _ssl_ctx = NULL;
    }
#endif
}

    khc_sock_code_t
sock_cb_connect(
        void* sock_ctx,
        const char* host,
        unsigned int port)
{
    int ret;
    DNC_CFG_CMD dnsCfg;
    DNC_RESP_INFO dnsRespInfo;
    SOCKADDR_T hostAddr;
    A_UINT32 sock;
    SSL *ssl = NULL;
    socket_context_t *ctx = (socket_context_t*)sock_ctx;

    memset(&dnsRespInfo, 0, sizeof(dnsRespInfo));
    /*Check if driver is loaded*/
    if(IS_DRIVER_READY != A_OK){
        return KHC_SOCK_FAIL;
    }

    // resolve the IP address of the host
    if (0 == ath_inet_aton(host, &dnsRespInfo.ipaddrs_list[0]))
    {
        if (strlen(host) >= sizeof(dnsCfg.ahostname))
        {
            printf("GetERROR: host name too long\n");
            return KHC_SOCK_FAIL;
        }
        strcpy((char*)dnsCfg.ahostname, host);
        dnsCfg.domain = ATH_AF_INET;
        dnsCfg.mode =  RESOLVEHOSTNAME;
        if (A_OK != custom_ip_resolve_hostname(handle, &dnsCfg, &dnsRespInfo))
        {
            printf("GetERROR: Unable to resolve host name\r\n");
            return KHC_SOCK_FAIL;
        }
        dnsRespInfo.ipaddrs_list[0] = A_BE2CPU32(dnsRespInfo.ipaddrs_list[0]);
    }

#if CONNECT_SSL
    if (_ssl_ctx == NULL){
        printf("failed to init ssl context.\n");
        return KHC_SOCK_FAIL;
    }
#endif

    sock = t_socket((void *)handle, ATH_AF_INET, SOCK_STREAM_TYPE, 0);
    if (sock < 0) {
        printf("failed to init socket.\n");
        return KHC_SOCK_FAIL;
    }

    memset(&hostAddr, 0x00, sizeof(hostAddr));
    hostAddr.sin_family = ATH_AF_INET;
    hostAddr.sin_addr = dnsRespInfo.ipaddrs_list[0];
    hostAddr.sin_port = port;

    if (t_connect((void *)handle, sock, &hostAddr, sizeof(hostAddr)) == A_ERROR){
        printf("failed to connect socket.\n");
        return KHC_SOCK_FAIL;
    }

#if CONNECT_SSL
    ssl = SSL_new(_ssl_ctx);
    if (ssl == NULL){
        printf("failed to init ssl.\n");
        return KHC_SOCK_FAIL;
    }

    ret = SSL_set_fd(ssl, sock);
    if (ret < 0){
        printf("failed to set fd: %d\n", ret);
        return KHC_SOCK_FAIL;
    }

    ret = SSL_connect(ssl);
    if (ret < 0) {
        printf("failed to connect: %d\n", ret);
        return KHC_SOCK_FAIL;
    }
#endif

    if (ctx->show_debug != 0) {
        printf("Connect socket: %d\n", sock);
    }
    ctx->sock = sock;
    ctx->ssl = ssl;
    return KHC_SOCK_OK;
}

    khc_sock_code_t
 sock_cb_send(
        void* sock_ctx,
        const char* buffer,
        size_t length,
        size_t* out_sent_length)
{
    int ret;
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    char* custom = CUSTOM_ALLOC(length);
    memcpy(custom, buffer, length);
#if CONNECT_SSL
    ret = SSL_write(ctx->ssl, custom, length);
#else
    ret = t_send(handle, ctx->sock, (uint_8 *)custom, length, 0);
#endif
    CUSTOM_FREE(custom);
    if (ret > 0) {
        if (ctx->show_debug != 0) {
            printf("%.*s", ret, buffer);
        }
        *out_sent_length = ret;
        return KHC_SOCK_OK;
    } else {
        printf("failed to send\n");
        return KHC_SOCK_FAIL;
    }
}

khc_sock_code_t sock_cb_recv(
        void* sock_ctx,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    *out_actual_length = 0;
    size_t received = 0;
    int res = A_OK;
    do
    {
        res = t_select(handle, ctx->sock, 1000);
        if (res == A_OK)
        {
            char* ptr = NULL;
#if CONNECT_SSL
            received = SSL_read(ctx->ssl, (void**)&ptr, length_to_read);
#else
            received = t_recv(handle, ctx->sock, (void**)&ptr, length_to_read, 0);
#endif
//            printf("t_recv: %d\n", received);
            if(received > 0)
            {
                memcpy(buffer, ptr, received);
                zero_copy_free(ptr);
            }
            break;
        } else if (res == A_ERROR) {
//            printf("t_select: TimeOut\n");
        } else {
            printf("t_select: other error %d\n", res);
        }
    } while (res == A_OK);

    if (received > 0) {
        if (ctx->show_debug != 0) {
            printf("%.*s", received, buffer);
        }
        *out_actual_length = received;
        return KHC_SOCK_OK;
    } else if (received == 0) {
        return KHC_SOCK_OK;
        /*
        int ssl_error = SSL_get_error(ctx->ssl, ret);
        if (ssl_error == SSL_ERROR_ZERO_RETURN) {
            return KHC_SOCK_OK;
        } else if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
            return KHC_SOCK_AGAIN;
        } else if (ssl_error == SSL_ERROR_SYSCALL){
            if (errno == 0){
                return KHC_SOCK_OK;
            } else {
                printf("SSL_ERROR_SYSCALL: errno=%d: %s\n", errno, strerror(errno));
                return KHC_SOCK_FAIL;
            }
        } else {
            return KHC_SOCK_FAIL;
        }
        return KHC_SOCK_FAIL;
        */
    } else {
        return KHC_SOCK_FAIL;
    }
}

khc_sock_code_t mqtt_cb_recv(
        void* sock_ctx,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    *out_actual_length = 0;
    size_t received = 0;
    int res = A_OK;
    do
    {
        res = t_select(handle, ctx->sock, 1000);
        if (res == A_OK)
        {
            char* ptr = NULL;
#if CONNECT_SSL
            received = SSL_read(ctx->ssl, (void**)&ptr, length_to_read);
#else
            received = t_recv(handle, ctx->sock, (void**)&ptr, length_to_read, 0);
#endif
            if(received > 0)
            {
                memcpy(buffer, ptr, received);
                zero_copy_free(ptr);
            }
            break;
        }
    } while (res != A_SOCK_INVALID);/* A_ERROR is timeout, retry */

    if (received > 0) {
        *out_actual_length = received;
        return KHC_SOCK_OK;
    } else if (received == 0) {
        return KHC_SOCK_OK;
        /*
        int ssl_error = SSL_get_error(ctx->ssl, ret);
        if (ssl_error == SSL_ERROR_ZERO_RETURN) {
            return KHC_SOCK_OK;
        } else if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
            return KHC_SOCK_AGAIN;
        } else if (ssl_error == SSL_ERROR_SYSCALL){
            if (errno == 0){
                return KHC_SOCK_OK;
            } else {
                printf("SSL_ERROR_SYSCALL: errno=%d: %s\n", errno, strerror(errno));
                return KHC_SOCK_FAIL;
            }
        } else {
            return KHC_SOCK_FAIL;
        }
        return KHC_SOCK_FAIL;
        */
    } else {
        return KHC_SOCK_FAIL;
    }
}

khc_sock_code_t sock_cb_close(void* sock_ctx)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
#if CONNECT_SSL
    if (ctx->ssl != NULL)
    {
        SSL_shutdown(ctx->ssl);
    }
#endif
    if (ctx->sock > 0)
    {
        if (ctx->show_debug != 0) {
            printf("Close socket: %d\n", ctx->sock);
        }
        t_shutdown(handle, ctx->sock);
    }
    return KHC_SOCK_OK;
}
