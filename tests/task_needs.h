#ifndef EBISU_TESTS_TASK_NEEDS_H
#define EBISU_TESTS_TASK_NEEDS_H

#include "kii.h"
#include "http_script.h"

namespace khct {

// What starting the updater's task checks is set, for a test that doesn't run
// it, or runs it without its making a request: none of it is called.
inline void set_updater_needs(kii_t* kii, char* buff, size_t buff_size) {
    kii_set_buff(kii, buff, buff_size);
    kii_set_cb_delay_ms(kii, cb::cb_delay_ms, NULL);
    kii_set_cb_http_sock_connect(kii, cb::mock_connect, NULL);
    kii_set_cb_http_sock_send(kii, cb::mock_send, NULL);
    kii_set_cb_http_sock_recv(kii, cb::mock_recv, NULL);
    kii_set_cb_http_sock_close(kii, cb::mock_close, NULL);
}

// The same for the push task, which has MQTT's as well.
inline void set_push_needs(kii_t* kii, char* buff, size_t buff_size) {
    set_updater_needs(kii, buff, buff_size);
    kii_set_mqtt_buff(kii, buff, buff_size);
    kii_set_cb_mqtt_sock_connect(kii, cb::mock_connect, NULL);
    kii_set_cb_mqtt_sock_send(kii, cb::mock_send, NULL);
    kii_set_cb_mqtt_sock_recv(kii, cb::mock_recv, NULL);
    kii_set_cb_mqtt_sock_close(kii, cb::mock_close, NULL);
}

}

#endif
