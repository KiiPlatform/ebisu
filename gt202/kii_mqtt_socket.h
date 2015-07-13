#ifndef _KII_MQTT_SOCKET
#define _KII_MQTT_SOCKET

#include "kii_socket_callback.h"

#ifdef __cplusplus
extern "C" {
#endif

kii_socket_code_t mqtt_socket_close(kii_socket_context_t* socket_context);

    kii_socket_code_t
mqtt_socket_connect(
        kii_socket_context_t* socket_context,
        const char* host,
        unsigned int port);

    kii_socket_code_t
mqtt_socket_send(
        kii_socket_context_t* socket_context,
        const char* buffer,
        size_t length);

    kii_socket_code_t
mqtt_socket_recv(
        kii_socket_context_t* socket_context,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length);


#ifdef __cplusplus
}
#endif

#endif /* _KII_MQTT_SOCKET */
