#ifndef __KHC_CORE_SOCKET
#define __KHC_CORE_SOCKET

#include "khc_socket_callback.h"
#include "kii.h"
#ifdef _WIN32
#include "sock_cb_win32.h"
#else
#include "sock_cb_linux.h"
#endif

#include <chrono>
#include <mutex>

namespace ebisu {
namespace ltest {

// The time the server's certificate is checked at, as a device without a
// real-time clock has it: the floor until the first response to a kii_t it is
// set on, the time the server stated from then on. One for every test, with a
// lock, for the push task's thread.
inline kii_clock_t* test_clock()
{
    static std::mutex mutex;
    struct Clock {
        kii_clock_t clock;
        Clock() {
            kii_clock_init(&clock, [](void*) -> uint64_t {
                return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
            }, nullptr);
            kii_clock_set_cb_lock(&clock,
                    [](void* m) { ((std::mutex*)m)->lock(); },
                    [](void* m) { ((std::mutex*)m)->unlock(); },
                    &mutex);
        }
    };
    static Clock c;
    return &c.clock;
}

namespace ssl {

// One connection, for the callbacks in both namespaces below.
struct SSLData
{
#ifdef _WIN32
    // The win32 sample's context: Winsock, with Schannel for TLS. A plain TCP
    // connection uses only its socket.
    socket_context_t win32;

    SSLData() {
        // No timeouts, as with OpenSSL.
        sock_cb_init_context(&win32, 0, 0);
        win32.clock = test_clock();
    }
#else
    // The Linux sample's context: BSD sockets, with OpenSSL for TLS. A plain
    // TCP connection uses only its socket.
    socket_context_t linux_ctx;

    SSLData() {
        sock_cb_init_context(&linux_ctx, 0, 0);
        linux_ctx.clock = test_clock();
    }
#endif
};

khc_sock_code_t
    cb_connect(void* socket_context, const char* host,
            uint16_t port);

khc_sock_code_t
    cb_send(void* socket_context,
            const char* buffer,
            size_t length,
            size_t* out_sent_length);

khc_sock_code_t
    cb_recv(void* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length);

khc_sock_code_t
    cb_close(void* socket_context);

}

namespace tcp {
khc_sock_code_t
    cb_connect(void* socket_context, const char* host,
            uint16_t port);

khc_sock_code_t
    cb_send(void* socket_context,
            const char* buffer,
            size_t length,
            size_t* out_sent_length);

khc_sock_code_t
    cb_recv(void* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length);

khc_sock_code_t
    cb_close(void* socket_context);
}

}
}

#endif /* __KHC_CORE_SOCKET */
