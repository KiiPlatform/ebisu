#include "tio_socket_impl.h"
#include <string.h>

#if !CONNECT_INSECURE
#include <mbedtls/ssl.h>
#endif

    khc_sock_code_t
sock_cb_connect(
        void* sock_ctx,
        const char* host,
        unsigned int port)
{
    wiced_ip_address_t addr;
    wiced_result_t rc;
    socket_context_t *ctx = (socket_context_t*)sock_ctx;

    rc = wiced_hostname_lookup(host, &addr, 10000, WICED_STA_INTERFACE );
    if(rc != WICED_SUCCESS) {
        wiced_log_printf("wiced_hostname_lookup failed.[%d]\n", rc);
        return KHC_SOCK_FAIL;
    }

    rc = wiced_tcp_create_socket(&(ctx->socket), WICED_STA_INTERFACE);
    if (rc != WICED_SUCCESS) {
        wiced_log_printf("wiced_tcp_create_socket failed.[%d]\n", rc);
        return KHC_SOCK_FAIL;
    }

#if !CONNECT_INSECURE
    wiced_tls_init_context(&(ctx->tls_context), NULL, NULL);
    wiced_tcp_enable_tls(&(ctx->socket), &(ctx->tls_context));
#endif

    rc = wiced_tcp_connect(&(ctx->socket), &addr, port, 10000);
    if (rc != WICED_SUCCESS) {
        wiced_log_printf("wiced_tcp_connect([%s],[%d]) failed.[%d]\n", host, port, rc);
        wiced_tcp_disconnect(&(ctx->socket));
        wiced_tcp_delete_socket(&(ctx->socket));
        return KHC_SOCK_FAIL;
    }

    if (ctx->show_debug != 0) {
        wiced_log_printf("Connect socket: [%s]:[%d]\n", host, port);
    }
    return KHC_SOCK_OK;
}

    khc_sock_code_t
 sock_cb_send(
        void* sock_ctx,
        const char* buffer,
        size_t length,
        size_t* out_sent_length)
{
    wiced_result_t ret;
    socket_context_t* ctx = (socket_context_t*)sock_ctx;

    ret = wiced_tcp_send_buffer(&(ctx->socket), buffer, length);
    if (ret == WICED_SUCCESS) {
        if (ctx->show_debug != 0) {
            wiced_log_printf("%.*s", length, buffer);
        }
        *out_sent_length = length;
        return KHC_SOCK_OK;
    } else {
        wiced_log_printf("failed to send\n");
        return KHC_SOCK_FAIL;
    }
}

khc_sock_code_t sock_cb_recv(
        void* sock_ctx,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length)
{
    wiced_result_t ret = WICED_SUCCESS;
    socket_context_t* ctx = (socket_context_t*)sock_ctx;

    wiced_packet_t *packet = ctx->packet;
    int offset = ctx->packet_offset;

    if (packet == NULL) {
        ret = wiced_tcp_receive(&(ctx->socket), &packet, 10000);
        offset = 0;
    }

    if (ret == WICED_SUCCESS) {
        uint16_t        total;
        uint16_t        length;
        uint8_t*        data;

        wiced_packet_get_data(packet, offset, &data, &length, &total);
#if !CONNECT_INSECURE
        if (total == 2 && length == 2 && data[0] == 0x1 && data[1] == 0x0) {
            // for WICED SDK v4.
            // Got MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY result on WICED SDK v6.
            // data[0] == MBEDTLS_SSL_ALERT_LEVEL_WARNING
            // data[1] == MBEDTLS_SSL_ALERT_MSG_CLOSE_NOTIFY
            wiced_log_printf("Got tls alert packet!\n");
            *out_actual_length = 0;
            wiced_packet_delete(packet);
            ctx->packet = NULL;
            ctx->packet_offset = 0;
            return KHC_SOCK_OK;
        }
#endif
        *out_actual_length = MIN(length, length_to_read);
        memcpy(buffer, data, *out_actual_length);
        offset += *out_actual_length;
        if (*out_actual_length < total) {
            ctx->packet = packet;
            ctx->packet_offset = offset;
        } else {
            wiced_packet_delete(packet);
            ctx->packet = NULL;
            ctx->packet_offset = 0;
        }
        if (ctx->show_debug != 0) {
            wiced_log_printf("%.*s", *out_actual_length, buffer);
        }
        return KHC_SOCK_OK;
    } else if (ret == WICED_TCPIP_SOCKET_CLOSED) {
        if (ctx->show_debug != 0) {
            wiced_log_printf("Socket closed by server. This is 'Connection: Close' reaction.\n");
        }
        *out_actual_length = 0;
        return KHC_SOCK_OK;
#if !CONNECT_INSECURE
    } else if (ret == (uint16_t)MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        if (ctx->show_debug != 0) {
            wiced_log_printf("Socket closed by tls alert.\n");
        }
        *out_actual_length = 0;
        return KHC_SOCK_OK;
#endif
    } else if (ret == WICED_TIMEOUT) {
        if (ctx->show_debug != 0) {
            wiced_log_printf("Timeout\n");
        }
        *out_actual_length = 0;
        return KHC_SOCK_OK;
    } else {
        if (ctx->show_debug != 0) {
            wiced_log_printf("sock_cb_recv fail. [%x]\n", ret);
        }
        return KHC_SOCK_FAIL;
    }
}

khc_sock_code_t mqtt_cb_recv(
        void* sock_ctx,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length)
{
    wiced_result_t ret = WICED_SUCCESS;
    socket_context_t* ctx = (socket_context_t*)sock_ctx;

    wiced_packet_t *packet = ctx->packet;
    int offset = ctx->packet_offset;

    if (packet == NULL) {
        ret = wiced_tcp_receive(&(ctx->socket), &packet, WICED_NEVER_TIMEOUT);
        offset = 0;
    }

    if (ret == WICED_SUCCESS) {
        uint16_t        total;
        uint16_t        length;
        uint8_t*        data;

        wiced_packet_get_data(packet, offset, &data, &length, &total);
        *out_actual_length = MIN(length, length_to_read);
        memcpy(buffer, data, *out_actual_length);
        offset += *out_actual_length;
        if (*out_actual_length < total) {
            ctx->packet = packet;
            ctx->packet_offset = offset;
        } else {
            wiced_packet_delete(packet);
            ctx->packet = NULL;
            ctx->packet_offset = 0;
        }
        if (ctx->show_debug != 0) {
            wiced_log_printf("Length: %d\n", *out_actual_length);
            for (int i = 0; i < *out_actual_length; ++i) {
                wiced_log_printf("0x%x,", buffer[i]);
            }
            wiced_log_printf("\n");
        }
        return KHC_SOCK_OK;
    } else {
        if (ctx->show_debug != 0) {
            wiced_log_printf("mqtt_cb_recv fail. [%d]\n", ret);
        }
        return KHC_SOCK_FAIL;
    }
}

khc_sock_code_t sock_cb_close(void* sock_ctx)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;

    if (ctx->packet != NULL) {
        wiced_packet_delete(ctx->packet);
    }
    if (ctx->show_debug != 0) {
        wiced_log_printf("Close socket\n");
    }
    wiced_tcp_disconnect(&(ctx->socket));
    wiced_tcp_delete_socket(&(ctx->socket));
#if !CONNECT_INSECURE
    wiced_tls_deinit_context(&(ctx->tls_context));
#endif
    return KHC_SOCK_OK;
}
