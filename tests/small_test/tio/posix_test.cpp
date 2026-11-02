#include "catch.hpp"
#include "ebisu_posix.h"

#include <atomic>
#include <chrono>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

namespace {

void on_alarm(int)
{
}

struct DetachState {
    std::atomic<bool> created;
    std::atomic<int> state;
};

// Waits for task_create to have returned, as what it does after creating
// the thread is what counts.
void* report_detach_state(void* param)
{
    DetachState* d = (DetachState*)param;
    while (!d->created) {
        sched_yield();
    }
    pthread_attr_t attr;
    int state = -1;
    pthread_getattr_np(pthread_self(), &attr);
    pthread_attr_getdetachstate(&attr, &state);
    pthread_attr_destroy(&attr);
    d->state = state;
    return nullptr;
}

}

TEST_CASE( "The POSIX system's delay sleeps on through a signal" ) {
    struct sigaction action;
    struct sigaction previous;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_alarm;
    REQUIRE( sigaction(SIGALRM, &action, &previous) == 0 );
    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));
    timer.it_value.tv_usec = 20 * 1000;
    REQUIRE( setitimer(ITIMER_REAL, &timer, nullptr) == 0 );

    auto start = std::chrono::steady_clock::now();
    ebisu_system_t* system = ebisu_posix_system();
    system->ops->delay_ms(system, 100);
    auto elapsed = std::chrono::steady_clock::now() - start;

    sigaction(SIGALRM, &previous, nullptr);
    CHECK( std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= 100 );
}

// Nothing joins a task, so a joinable thread stays behind when it ends.
TEST_CASE( "The POSIX system's tasks leave nothing behind when they end" ) {
    DetachState d;
    d.created = false;
    d.state = -1;

    ebisu_system_t* system = ebisu_posix_system();
    REQUIRE( system->ops->task_create(system, "task", report_detach_state, &d) == EBISU_TASKC_OK );
    d.created = true;
    while (d.state == -1) {
        sched_yield();
    }

    CHECK( d.state == PTHREAD_CREATE_DETACHED );
}

TEST_CASE( "The POSIX system's monotonic clock goes forward" ) {
    ebisu_system_t* system = ebisu_posix_system();
    uint64_t before = system->ops->monotonic_ms(system);
    system->ops->delay_ms(system, 20);
    uint64_t after = system->ops->monotonic_ms(system);
    CHECK( after >= before + 20 );
    CHECK( after < before + 2000 );
}

TEST_CASE( "The POSIX lock is a mutex" ) {
    ebisu_posix_lock_t lock;
    REQUIRE( ebisu_posix_lock_init(&lock) == 0 );
    lock.base.ops->lock(&lock.base);
    CHECK( pthread_mutex_trylock(&lock.mutex) != 0 );
    lock.base.ops->unlock(&lock.base);
    REQUIRE( pthread_mutex_trylock(&lock.mutex) == 0 );
    pthread_mutex_unlock(&lock.mutex);
    ebisu_posix_lock_destroy(&lock);
}
