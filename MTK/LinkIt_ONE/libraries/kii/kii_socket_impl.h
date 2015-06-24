#ifndef _KII_SOCKET_IMPL
#define _KII_SOCKET_IMPL

#include "kii_socket_callback.h"

#ifdef __cplusplus
extern "C" {
#endif

kii_socket_code_t
    connect_cb(kii_socket_context_t* socket_context, const char* host,
            unsigned int port);

kii_socket_code_t
    send_cb(kii_socket_context_t* socket_context,
            const char* buffer,
            size_t length);

kii_socket_code_t
    recv_cb(kii_socket_context_t* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length);

kii_socket_code_t
    close_cb(kii_socket_context_t* socket_context);

kii_socket_code_t
    mqtt_connect_cb(kii_socket_context_t* socket_context, const char* host,
            unsigned int port);

kii_socket_code_t
    mqtt_send_cb(kii_socket_context_t* socket_context,
            const char* buffer,
            size_t length);

kii_socket_code_t
    mqtt_recv_cb(kii_socket_context_t* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length);

kii_socket_code_t
    mqtt_close_cb(kii_socket_context_t* socket_context);

int mqtt_data_available(void);
int mqtt_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* _KII_SOCKET_IMPL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
