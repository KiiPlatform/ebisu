#include "gt202_kii_adapter.h"

#include "prv_kii_setting.h"

#define BUFF_SIZE 256

kii_socket_code_t socket_close_cb(kii_socket_context_t* socket_context);

    kii_socket_code_t
socket_connect_cb(
        kii_socket_context_t* socket_context,
        const char* host,
        unsigned int port)
{
    int ret;
    DNC_CFG_CMD dnsCfg;
    DNC_RESP_INFO dnsRespInfo;
    SOCKADDR_T hostAddr;
    A_UINT32 sock;
    SSL *ssl = NULL;
    SSL_CTX *ssl_ctx = NULL;
    context_t *ctx = (context_t*)socket_context->app_context;

    memset(&dnsRespInfo, 0, sizeof(dnsRespInfo));
    /*Check if driver is loaded*/
    if(IS_DRIVER_READY != A_OK){
        return KII_SOCKETC_FAIL;
    }

    // resolve the IP address of the host
    if (0 == ath_inet_aton(host, &dnsRespInfo.ipaddrs_list[0]))
    {
        if (strlen(host) >= sizeof(dnsCfg.ahostname))
        {
            printf("GetERROR: host name too long\n");
            return KII_SOCKETC_FAIL;
        }
        strcpy((char*)dnsCfg.ahostname, host);
        dnsCfg.domain = ATH_AF_INET;
        dnsCfg.mode =  RESOLVEHOSTNAME;
        if (A_OK != custom_ip_resolve_hostname(handle, &dnsCfg, &dnsRespInfo))
        {
            printf("GetERROR: Unable to resolve host name\r\n");
            return KII_SOCKETC_FAIL;
        }
        dnsRespInfo.ipaddrs_list[0] = A_BE2CPU32(dnsRespInfo.ipaddrs_list[0]);
    }

#if CONNECT_SSL
    ssl_ctx = SSL_ctx_new(SSL_CLIENT, 4500, 2000, 0);
    if (ssl_ctx == NULL){
        printf("failed to init ssl context.\n");
        return KII_SOCKETC_FAIL;
    }
#endif

    sock = t_socket((void *)handle, ATH_AF_INET, SOCK_STREAM_TYPE, 0);
    if (sock < 0) {
        printf("failed to init socket.\n");
        return KII_SOCKETC_FAIL;
    }

    memset(&hostAddr, 0x00, sizeof(hostAddr));
    hostAddr.sin_family = ATH_AF_INET;
    hostAddr.sin_addr = dnsRespInfo.ipaddrs_list[0];
    hostAddr.sin_port = port;

    if (t_connect((void *)handle, sock, &hostAddr, sizeof(hostAddr)) == A_ERROR){
        printf("failed to connect socket.\n");
        return KII_SOCKETC_FAIL;
    }

#if CONNECT_SSL
    ssl = SSL_new(ssl_ctx);
    if (ssl == NULL){
        printf("failed to init ssl.\n");
        return KII_SOCKETC_FAIL;
    }

    ret = SSL_set_fd(ssl, sock);
    if (ret < 0){
        printf("failed to set fd: %d\n", ret);
        return KII_SOCKETC_FAIL;
    }

    ret = SSL_connect(ssl);
    if (ret < 0) {
        printf("failed to connect: %d\n", ret);
        return KII_SOCKETC_FAIL;
    }
#endif

    ctx->sock = sock;
    ctx->ssl = ssl;
    ctx->ssl_ctx = ssl_ctx;
    return KII_SOCKETC_OK;
}

    kii_socket_code_t
socket_send_cb(
        kii_socket_context_t* socket_context,
        const char* buffer,
        size_t length)
{
    int ret;
    context_t* ctx = (context_t*)socket_context->app_context;
    char* buff = CUSTOM_ALLOC(length);

    memcpy(buff, buffer, length);
#if CONNECT_SSL
    ret = SSL_write(ctx->ssl, buff, length);
#else
    ret = t_send(handle, ctx->sock, (uint_8 *)buff, length, 0);
#endif

    CUSTOM_FREE(buff);

    if (ret > 0) {
        return KII_SOCKETC_OK;
    } else {
        printf("failed to send\n");
        socket_close_cb(socket_context);
        return KII_SOCKETC_FAIL;
    }
}

kii_socket_code_t socket_close_cb(kii_socket_context_t* socket_context)
{
    context_t* ctx = (context_t*)socket_context->app_context;
#if CONNECT_SSL
    if (ctx->ssl != NULL)
    {
        SSL_shutdown(ctx->ssl);
    }
    if (ctx->ssl_ctx != NULL)
    {
        SSL_ctx_free(ctx->ssl_ctx);
    }
#endif
    if (ctx->sock > 0)
    {
        t_shutdown(handle, ctx->sock);
    }
    return KII_SOCKETC_OK;
}

void logger_cb(const char* format, ...)
{
    va_list list;
    va_start(list, format);
    vprintf(format, list);
    va_end(list);
}
