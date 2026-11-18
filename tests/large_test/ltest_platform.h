#ifndef EBISU_LTEST_PLATFORM_H
#define EBISU_LTEST_PLATFORM_H

// What the large tests take from the platform they run on, in one place: its
// socket, with TLS; the threads the tasks they test run on, with the mutexes
// and condition variables the tests and those tasks share; sleeping; and the
// clocks. The tests use these, never the standard library's threads and
// clocks themselves. On a host they are the standard library's, with
// platform's Schannel port on Windows, or else its OpenSSL port. On a board,
// with EBISU_LTEST_BOARD, they are its ltest_board.h's, as
// tests/freertos_large has them.

#ifdef EBISU_LTEST_BOARD
#include "ltest_board.h"
#else

#include <stdint.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <ebisu_socket.h>
#ifdef _WIN32
#include "ebisu_schannel.h"
#else
#include "ebisu_openssl.h"
#endif

namespace ebisu {
namespace ltest {

// The platform's socket, with TLS: platform's Schannel port, Winsock and
// Schannel, or its OpenSSL port, BSD sockets and OpenSSL. Its base is the
// socket.
#ifdef _WIN32
typedef ebisu_schannel_socket_t platform_socket_t;

inline void init_platform_socket(platform_socket_t* sock, unsigned int to_recv,
        unsigned int to_send)
{
    ebisu_schannel_socket_init(sock, to_recv, to_send);
}

inline void set_platform_socket_clock(platform_socket_t* sock, ebisu_clock_t* clock)
{
    ebisu_schannel_socket_set_clock(sock, clock);
}
#else
typedef ebisu_openssl_socket_t platform_socket_t;

inline void init_platform_socket(platform_socket_t* sock, unsigned int to_recv,
        unsigned int to_send)
{
    ebisu_openssl_socket_init(sock, to_recv, to_send);
}

inline void set_platform_socket_clock(platform_socket_t* sock, ebisu_clock_t* clock)
{
    ebisu_openssl_socket_set_clock(sock, clock);
}
#endif

// A thread, started with a function and its argument, which is joined, or
// moved; a mutex; and a condition variable, waited on with a
// std::unique_lock of the mutex.
typedef std::thread thread;
typedef std::mutex mutex;
typedef std::condition_variable condition_variable;

// Sleeps for at least the duration.
template <class Rep, class Period>
inline void sleep_for(const std::chrono::duration<Rep, Period>& duration)
{
    std::this_thread::sleep_for(duration);
}

// The monotonic clock, in milliseconds.
inline uint64_t steady_ms()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The time of day, in microseconds since the epoch.
inline int64_t wall_us()
{
    return (int64_t)std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}

}
}

#endif /* EBISU_LTEST_BOARD */

#endif /* EBISU_LTEST_PLATFORM_H */
