#include "catch.hpp"
#include "task_impl.h"

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

// Waits for task_create_cb() to have returned, as what it does after creating
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

TEST_CASE( "delay_ms_cb sleeps on through a signal" ) {
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
    delay_ms_cb(100, nullptr);
    auto elapsed = std::chrono::steady_clock::now() - start;

    sigaction(SIGALRM, &previous, nullptr);
    CHECK( std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= 100 );
}

// Nothing joins a task, so a joinable thread stays behind when it ends.
TEST_CASE( "task_create_cb leaves nothing behind when the task ends" ) {
    DetachState d;
    d.created = false;
    d.state = -1;

    REQUIRE( task_create_cb("task", report_detach_state, &d, nullptr) == EBISU_TASKC_OK );
    d.created = true;
    while (d.state == -1) {
        sched_yield();
    }

    CHECK( d.state == PTHREAD_CREATE_DETACHED );
}
