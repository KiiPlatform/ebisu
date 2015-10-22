#include "gt202_kii_adapter.h"

#include "prv_kii_setting.h"

#if !ZERO_COPY
#error
#endif

extern kii_socket_code_t socket_close_cb(kii_socket_context_t* socket_context);

    kii_socket_code_t
socket_recv_cb(
        kii_socket_context_t* socket_context,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length)
{
    int res;
    int received;
    int total = 0;
    char *pBuf = NULL;
    context_t* ctx = (context_t*)socket_context->app_context;
    do
    {
        res = t_select((void *)handle, ctx->sock, 1000);
        if (res == A_OK)
        {
#if CONNECT_SSL
            received = SSL_read(ctx->ssl, (void**)&pBuf, length_to_read);
#else
            received = t_recv(handle, ctx->sock, (void**)&pBuf, length_to_read, 0);
#endif
            if(received > 0)
            {
                memcpy(buffer, pBuf, received);
                zero_copy_free(pBuf);
                total = received;
                break;
            }
        }
    } while (res == A_OK);

    if (total >  0) {
        *out_actual_length = total;
        return KII_SOCKETC_OK;
    } else {
        printf("failed to receive:\n");
        // TOOD: could be 0 on success?
        *out_actual_length = 0;
        socket_close_cb(socket_context);
        return KII_SOCKETC_FAIL;
    }
}

