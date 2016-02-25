#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "kii_socket_impl.h"
#include "kii_task_impl.h"

#include <kii.h>
#include <kii_core.h>
#include <kii_core_secure_socket.h>

static void logger_cb(const char* format, ...)
{
    va_list list;
    va_start(list, format);
    vprintf(format, list);
    va_end(list);
}

int kii_impl_init(
        kii_t* kii,
        const char* site,
        const char* app_id,
        const char* app_key)
{
    kii_init(kii, site, app_id, app_key);

    /* setting http socket callbacks */
    kii->kii_core.http_context.connect_cb = s_connect_cb;
    kii->kii_core.http_context.send_cb = s_send_cb;
    kii->kii_core.http_context.recv_cb = s_recv_cb;
    kii->kii_core.http_context.close_cb = s_close_cb;

    /* setting logger callbacks. */
    kii->kii_core.logger_cb = logger_cb;

    /* setting mqtt socket callbacks. */
    kii->mqtt_socket_connect_cb = mqtt_connect_cb;
    kii->mqtt_socket_send_cb = mqtt_send_cb;
    kii->mqtt_socket_recv_cb = mqtt_recv_cb;
    kii->mqtt_socket_close_cb = mqtt_close_cb;

    /* setting task callbacks. */
    kii->task_create_cb = task_create_cb;
    kii->delay_ms_cb = delay_ms_cb;

    return 0;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */

