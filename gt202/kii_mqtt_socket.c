#include "gt202_kii_adapter.h"

#include "prv_kii_setting.h"

#define BUFF_SIZE 256

kii_socket_code_t mqtt_socket_close(kii_socket_context_t* socket_context)
{
    context_t* ctx = (context_t*)socket_context->app_context;
    if (ctx->sock > 0)
    {
        t_shutdown(handle, ctx->sock);
    }
    return KII_HTTPC_OK;
}

    kii_socket_code_t
mqtt_socket_connect(
        kii_socket_context_t* socket_context,
        const char* host,
        unsigned int port)
{
    int ret = KII_HTTPC_FAIL;
    DNC_CFG_CMD dnsCfg;
    DNC_RESP_INFO dnsRespInfo;
    SOCKADDR_T hostAddr;
    A_UINT32 sock;
    const char* pHost;
    context_t *ctx = (context_t*)socket_context->app_context;

    memset(&dnsRespInfo, 0, sizeof(dnsRespInfo));
    /*Check if driver is loaded*/
    if(IS_DRIVER_READY != A_OK){
        goto END_FUNC;
    }

    if (strcmp(host, "development-jp-mqtt-12345678-0001.internal.kii.com") == 0) {
        pHost = "52.68.233.214";
    } else {
        pHost = host;
    }
    // resolve the IP address of the host
    if (0 == ath_inet_aton(pHost, &dnsRespInfo.ipaddrs_list[0]))
    {
        if (strlen(pHost) >= sizeof(dnsCfg.ahostname))
        {
            printf("GetERROR: host name too long.(%s)\n", pHost);
            goto END_FUNC;
        }
        strcpy((char*)dnsCfg.ahostname, pHost);
        dnsCfg.domain = ATH_AF_INET;
        dnsCfg.mode =  RESOLVEHOSTNAME;
        if (A_OK != custom_ip_resolve_hostname(handle, &dnsCfg, &dnsRespInfo))
        {
            printf("GetERROR: Unable to resolve host name\r\n");
            goto END_FUNC;
        }
        dnsRespInfo.ipaddrs_list[0] = A_BE2CPU32(dnsRespInfo.ipaddrs_list[0]);
    }

    sock = t_socket((void *)handle, ATH_AF_INET, SOCK_STREAM_TYPE, 0);
    if (sock < 0) {
        printf("failed to init socket.\n");
        goto END_FUNC;
    }

    memset(&hostAddr, 0x00, sizeof(hostAddr));
    hostAddr.sin_family = ATH_AF_INET;
    hostAddr.sin_addr = dnsRespInfo.ipaddrs_list[0];
    hostAddr.sin_port = port;

    if (t_connect((void *)handle, sock, &hostAddr, sizeof(hostAddr)) == A_ERROR){
        printf("failed to connect mqtt socket.\n");
        goto END_FUNC;
    }

    ctx->sock = sock;
    ret = KII_HTTPC_OK;
END_FUNC:
    return ret;
}

    kii_socket_code_t
mqtt_socket_send(
        kii_socket_context_t* socket_context,
        const char* send_buff,
        int buff_length)
{
    int ret;
    context_t* ctx = (context_t*)socket_context->app_context;
    char* buff = CUSTOM_ALLOC(buff_length);

    memcpy(buff, send_buff, buff_length);
    ret = t_send(handle, ctx->sock, (uint_8 *)buff, buff_length, 0);

    CUSTOM_FREE(buff);

    if (ret > 0) {
        return KII_HTTPC_OK;
    } else {
        printf("failed to send\n");
        prv_close(socket_context);
        return KII_HTTPC_FAIL;
    }
}

    kii_http_client_code_t
mqtt_socket_recv(
        kii_socket_context_t* socket_context,
        char* recv_buff,
        int length_to_read,
        int* out_actual_length)
{
    int received;
    char *pBuf = NULL;
    context_t* ctx = (context_t*)socket_context->app_context;
    received = t_recv(handle, ctx->sock, (void**)&pBuf, length_to_read, 0);
    if(received > 0)
    {
        memcpy(recv_buff, pBuf, received);
        zero_copy_free(pBuf);
        *out_actual_length = received;
        return KII_HTTPC_OK;
    } else {
        printf("failed to receive:\n");
        // TOOD: could be 0 on success?
        *out_actual_length = 0;
        mqtt_socket_close(socket_context);
        return KII_HTTPC_FAIL;
    }
}
