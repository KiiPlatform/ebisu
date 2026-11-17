#ifndef EBISU_LTEST_BOARD_H
#define EBISU_LTEST_BOARD_H

// What tests/large_test/ltest_platform.h has, on the board: platform's mbedTLS
// port over its FreeRTOS-Plus-TCP socket, and FreeRTOS's tasks, mutexes and
// ticks, as newlib's C++ library has no threads.

#include <stdint.h>
#include <time.h>

#include <chrono>
#include <mutex>
#include <utility>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include <ebisu_freertos_tcp.h>
#include <ebisu_mbedtls.h>
#include <ebisu_socket.h>
#include <ebisu_system.h>

namespace ebisu {
namespace ltest {

// The TLS configuration every socket shares, with the CA certificates of the
// sample's ca.pem: large_main.cpp's.
ebisu_mbedtls_tls_t* board_tls();

// The mbedTLS port's socket, whose base is the socket, over a socket of the
// FreeRTOS-Plus-TCP port's, which plain TCP uses alone.
struct platform_socket_t : ebisu_mbedtls_socket_t {
    ebisu_freertos_tcp_socket_t tcp;
};

inline void init_platform_socket(platform_socket_t* sock, unsigned int to_recv,
        unsigned int to_send)
{
    ebisu_freertos_tcp_socket_init(&sock->tcp, to_recv, to_send);
    ebisu_mbedtls_socket_init(sock, &sock->tcp.base, board_tls());
}

inline void set_platform_socket_clock(platform_socket_t* sock, ebisu_clock_t* clock)
{
    ebisu_mbedtls_socket_set_clock(sock, clock);
}

// Ticks for a duration, rounded up, and a tick more, for the tick under way:
// at least the duration.
template <class Rep, class Period>
inline TickType_t ticks_for(const std::chrono::duration<Rep, Period>& duration)
{
    const int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    if (ms <= 0) {
        return 0;
    }
    return (TickType_t)(((uint64_t)ms * configTICK_RATE_HZ + 999) / 1000 + 1);
}

template <class Rep, class Period>
inline void sleep_for(const std::chrono::duration<Rep, Period>& duration)
{
    vTaskDelay(ticks_for(duration));
}

// The tick count, in milliseconds, from a count of 64 bits: one of 32 at
// 1 kHz goes round after 49 days, which a run never lasts.
inline uint64_t steady_ms()
{
    return (uint64_t)xTaskGetTickCount() * 1000 / configTICK_RATE_HZ;
}

// The time of day: the host's, in seconds, through semihosting, when it is
// first asked for, and the ticks since. Microseconds, in steps of the ticks',
// for the names the tests make of it to differ.
inline int64_t wall_us()
{
    static const time_t start = time(NULL);
    static const uint64_t start_ms = steady_ms();
    return (int64_t)start * 1000000 + (int64_t)(steady_ms() - start_ms) * 1000;
}

// A FreeRTOS mutex.
class mutex {
public:
    mutex() : handle_(xSemaphoreCreateMutex()) {
        configASSERT(handle_ != NULL);
    }
    ~mutex() {
        vSemaphoreDelete(handle_);
    }
    mutex(const mutex&) = delete;
    mutex& operator=(const mutex&) = delete;

    void lock() {
        (void)xSemaphoreTake(handle_, portMAX_DELAY);
    }
    void unlock() {
        (void)xSemaphoreGive(handle_);
    }

private:
    SemaphoreHandle_t handle_;
};

// A condition variable whose waiters check their predicate at each
// notification, and every 10 ms without: a notification wakes one of them at
// once, the others at their next check.
class condition_variable {
public:
    condition_variable() : signal_(xSemaphoreCreateBinary()) {
        configASSERT(signal_ != NULL);
    }
    ~condition_variable() {
        vSemaphoreDelete(signal_);
    }
    condition_variable(const condition_variable&) = delete;
    condition_variable& operator=(const condition_variable&) = delete;

    void notify_one() {
        (void)xSemaphoreGive(signal_);
    }
    void notify_all() {
        (void)xSemaphoreGive(signal_);
    }

    template <class Predicate>
    void wait(std::unique_lock<mutex>& lock, Predicate done) {
        while (!done()) {
            pause(lock, pdMS_TO_TICKS(10));
        }
    }

    template <class Rep, class Period, class Predicate>
    bool wait_for(std::unique_lock<mutex>& lock,
            const std::chrono::duration<Rep, Period>& duration, Predicate done) {
        const TickType_t start = xTaskGetTickCount();
        const TickType_t length = ticks_for(duration);
        while (!done()) {
            const TickType_t gone = xTaskGetTickCount() - start;
            if (gone >= length) {
                return done();
            }
            const TickType_t left = length - gone;
            pause(lock, left < pdMS_TO_TICKS(10) ? left : pdMS_TO_TICKS(10));
        }
        return true;
    }

private:
    void pause(std::unique_lock<mutex>& lock, TickType_t ticks) {
        lock.unlock();
        (void)xSemaphoreTake(signal_, ticks);
        lock.lock();
    }

    SemaphoreHandle_t signal_;
};

// A FreeRTOS task, started with an ebisu task's entry and its argument, and
// joined, as a std::thread is. Each has a stack of THREAD_STACK_WORDS: a task
// that makes TLS connections needs the most, for the handshake.
class thread {
public:
    static const configSTACK_DEPTH_TYPE THREAD_STACK_WORDS = 8192;

    thread() : state_(NULL) {}
    thread(EBISU_TASK_ENTRY entry, void* entry_param) : state_(new State) {
        state_->entry = entry;
        state_->entry_param = entry_param;
        state_->done = xSemaphoreCreateBinary();
        configASSERT(state_->done != NULL);
        BaseType_t made = xTaskCreate(run, "ltest", THREAD_STACK_WORDS, state_,
                uxTaskPriorityGet(NULL), NULL);
        configASSERT(made == pdPASS);
        (void)made;
    }
    ~thread() {
        // As std::thread, which ends the program.
        configASSERT(state_ == NULL);
    }
    thread(thread&& other) : state_(other.state_) {
        other.state_ = NULL;
    }
    thread& operator=(thread&& other) {
        configASSERT(state_ == NULL);
        std::swap(state_, other.state_);
        return *this;
    }
    thread(const thread&) = delete;
    thread& operator=(const thread&) = delete;

    bool joinable() const {
        return state_ != NULL;
    }
    void join() {
        configASSERT(state_ != NULL);
        (void)xSemaphoreTake(state_->done, portMAX_DELAY);
        vSemaphoreDelete(state_->done);
        delete state_;
        state_ = NULL;
    }

private:
    struct State {
        EBISU_TASK_ENTRY entry;
        void* entry_param;
        SemaphoreHandle_t done;
    };

    static void run(void* param) {
        State* state = (State*)param;
        (void)state->entry(state->entry_param);
        // join() may free the state from here on.
        (void)xSemaphoreGive(state->done);
        vTaskDelete(NULL);
    }

    State* state_;
};

}
}

#endif /* EBISU_LTEST_BOARD_H */
