#ifndef __KHC_CORE_SOCKET
#define __KHC_CORE_SOCKET

#include <ebisu_socket.h>
#include "kii.h"
#ifdef _WIN32
#include "sock_cb_win32.h"
#else
#include "sock_cb_linux.h"
#endif

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace ebisu {
namespace ltest {

// The large tests' monotonic clock.
inline uint64_t steady_ms()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
}

// A lock that is a std::mutex.
struct MutexLock {
    struct Link {
        ebisu_lock_t base;
        MutexLock* owner;
    } link;
    std::mutex mutex;

    MutexLock() {
        static const ebisu_lock_ops_t ops = {
            [](ebisu_lock_t* self) { ((Link*)self)->owner->mutex.lock(); },
            [](ebisu_lock_t* self) { ((Link*)self)->owner->mutex.unlock(); },
        };
        link.base.ops = &ops;
        link.owner = this;
    }
    MutexLock(const MutexLock&) = delete;
    MutexLock& operator=(const MutexLock&) = delete;

    ebisu_lock_t* get() {
        return &link.base;
    }
};

// The system the large tests run tasks with: each task is a std::thread, given
// to keep, delays sleep, and the monotonic clock is steady_ms().
struct ThreadSystem {
    struct Link {
        ebisu_system_t base;
        ThreadSystem* owner;
    } link;
    std::function<void(std::thread)> keep;

    ThreadSystem() {
        static const ebisu_system_ops_t ops = {
            [](ebisu_system_t* self, const char*, EBISU_TASK_ENTRY entry, void* entry_param) {
                ((Link*)self)->owner->keep(std::thread(entry, entry_param));
                return EBISU_TASKC_OK;
            },
            [](ebisu_system_t*, uint32_t msec) {
                std::this_thread::sleep_for(std::chrono::milliseconds(msec));
            },
            [](ebisu_system_t*) { return steady_ms(); },
        };
        link.base.ops = &ops;
        link.owner = this;
    }
    ThreadSystem(const ThreadSystem&) = delete;
    ThreadSystem& operator=(const ThreadSystem&) = delete;

    ebisu_system_t* get() {
        return &link.base;
    }
};

// The time the server's certificate is checked at, as a device without a
// real-time clock has it: the floor until the first response to a kii_t it is
// set on, the time the server stated from then on. One for every test, with a
// lock, for the push task's thread.
inline ebisu_clock_t* test_clock()
{
    struct Clock {
        ThreadSystem system;
        MutexLock lock;
        ebisu_clock_t clock;
        Clock() {
            ebisu_clock_init(&clock, system.get());
            ebisu_clock_set_lock(&clock, lock.get());
        }
    };
    static Clock c;
    return &c.clock;
}

namespace ssl {
// TLS, through the platform sample's socket.
extern const ebisu_socket_ops_t ops;
}

namespace tcp {
// Plain TCP, on the platform sample's socket alone.
extern const ebisu_socket_ops_t ops;
}

namespace ssl {

// One connection, as TLS or as plain TCP.
struct SSLData
{
    struct Link {
        ebisu_socket_t base;
        SSLData* owner;
    };
#ifdef _WIN32
    // The win32 sample's context: Winsock, with Schannel for TLS. A plain TCP
    // connection uses only its socket.
    socket_context_t win32;
#else
    // The Linux sample's context: BSD sockets, with OpenSSL for TLS. A plain
    // TCP connection uses only its socket.
    socket_context_t linux_ctx;
#endif
    Link tls_link;
    Link tcp_link;

    SSLData() {
        // No timeouts, as with OpenSSL.
#ifdef _WIN32
        sock_cb_init_context(&win32, 0, 0);
        win32.clock = test_clock();
#else
        sock_cb_init_context(&linux_ctx, 0, 0);
        linux_ctx.clock = test_clock();
#endif
        tls_link.base.ops = &ssl::ops;
        tls_link.owner = this;
        tcp_link.base.ops = &tcp::ops;
        tcp_link.owner = this;
    }
    SSLData(const SSLData&) = delete;
    SSLData& operator=(const SSLData&) = delete;

    // The sample's socket, with its TLS.
    ebisu_socket_t* sample() {
#ifdef _WIN32
        return &win32.base;
#else
        return &linux_ctx.base;
#endif
    }
    ebisu_socket_t* tls() {
        return &tls_link.base;
    }
    ebisu_socket_t* tcp() {
        return &tcp_link.base;
    }
    // What HTTP requests go on: plain TCP in a PLAIN_HTTP build.
    ebisu_socket_t* http() {
#ifdef PLAIN_HTTP
        return tcp();
#else
        return tls();
#endif
    }
};

inline SSLData* data_of(ebisu_socket_t* self)
{
    return ((SSLData::Link*)self)->owner;
}

}

// A socket passing everything on to another, and giving each send's data to
// on_sent.
struct ObservedSocket {
    struct Link {
        ebisu_socket_t base;
        ObservedSocket* owner;
    } link;
    ebisu_socket_t* inner;
    std::function<void(const char* data, size_t length)> on_sent;

    explicit ObservedSocket(ebisu_socket_t* inner) : inner(inner) {
        static const ebisu_socket_ops_t ops = {
            [](ebisu_socket_t* self, const char* host, uint16_t port) {
                ebisu_socket_t* in = ((Link*)self)->owner->inner;
                return in->ops->connect(in, host, port);
            },
            [](ebisu_socket_t* self, const char* buffer, size_t length, size_t* out_sent_length) {
                ObservedSocket* o = ((Link*)self)->owner;
                ebisu_sock_code_t res = o->inner->ops->send(o->inner, buffer, length, out_sent_length);
                if (res == EBISU_SOCK_OK && o->on_sent) {
                    o->on_sent(buffer, *out_sent_length);
                }
                return res;
            },
            [](ebisu_socket_t* self, char* buffer, size_t length_to_read, size_t* out_actual_length) {
                ebisu_socket_t* in = ((Link*)self)->owner->inner;
                return in->ops->recv(in, buffer, length_to_read, out_actual_length);
            },
            [](ebisu_socket_t* self) {
                ebisu_socket_t* in = ((Link*)self)->owner->inner;
                return in->ops->close(in);
            },
            NULL,
        };
        link.base.ops = &ops;
        link.owner = this;
    }
    ObservedSocket(const ObservedSocket&) = delete;
    ObservedSocket& operator=(const ObservedSocket&) = delete;

    ebisu_socket_t* get() {
        return &link.base;
    }
};

}
}

#endif /* __KHC_CORE_SOCKET */
