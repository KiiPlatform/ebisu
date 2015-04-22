#ifndef _KII_SOCKET_IMPL
#define _KII_SOCKET_IMPL

#include "kii-core/kii_socket_callback.h"

#ifdef __cplusplus
extern "C" {
#endif

kii_socket_code_t
    connect_cb(kii_socket_context_t* socket_context, const char* host);

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


#ifdef __cplusplus
}
#endif

#endif /* _KII_SOCKET_IMPL */
