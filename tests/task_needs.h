#ifndef EBISU_TESTS_TASK_NEEDS_H
#define EBISU_TESTS_TASK_NEEDS_H

#include "kii.h"
#include "http_script.h"

namespace khct {

// What starting the updater's task checks is set, for a test that doesn't run
// it, or runs it without its making a request: none of it is called.
inline void set_updater_needs(kii_t* kii, char* buff, size_t buff_size) {
    kii_set_buff(kii, buff, buff_size);
    kii_set_system(kii, cb::delay_only_system());
    kii_set_http_socket(kii, cb::unused_socket());
}

// The same for the push task, which has MQTT's as well.
inline void set_push_needs(kii_t* kii, char* buff, size_t buff_size) {
    set_updater_needs(kii, buff, buff_size);
    kii_set_mqtt_buff(kii, buff, buff_size);
    kii_set_mqtt_socket(kii, cb::unused_socket());
}

}

#endif
