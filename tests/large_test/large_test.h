#ifndef __large_test__
#define __large_test__

#include "kii.h"
#include "secure_socket_impl.h"

namespace kiiltest {

const char DEFAULT_SITE[] = "api-jp.kii.com";
// APP Owner: satoshi.kumano@kii.com
const char APP_ID[] = "b6t9ai81zb3s";

inline void init(
        kii_t* kii,
        char* buffer,
        int buffer_size,
        void* http_ssl_ctx,
        void* mqtt_ssl_ctx)
{
    kii_init(kii, DEFAULT_SITE, APP_ID);

    kii_set_buff(kii, buffer, buffer_size);

    kii_set_http_cb_sock_connect(kii, ssl::cb_connect, http_ssl_ctx);
    kii_set_http_cb_sock_send(kii, ssl::cb_send, http_ssl_ctx);
    kii_set_http_cb_sock_recv(kii, ssl::cb_recv, http_ssl_ctx);
    kii_set_http_cb_sock_close(kii, ssl::cb_close, http_ssl_ctx);

    kii_set_mqtt_cb_sock_connect(kii, ssl::cb_connect, mqtt_ssl_ctx);
    kii_set_mqtt_cb_sock_send(kii, ssl::cb_send, mqtt_ssl_ctx);
    kii_set_mqtt_cb_sock_recv(kii, ssl::cb_recv, mqtt_ssl_ctx);
    kii_set_mqtt_cb_sock_close(kii, ssl::cb_close, mqtt_ssl_ctx);

    kii->_author.author_id[0] = '\0';
    kii->_author.access_token[0] = '\0';
}

} // namespace kiiltest

#endif