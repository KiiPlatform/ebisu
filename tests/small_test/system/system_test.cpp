// The system of the POSIX port, or on Windows the Win32 port's: the locks it
// makes, and the inline calls over its operations.
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#ifdef _WIN32
#include <ebisu_win32.h>
#define port_system ebisu_win32_system
#else
#include <ebisu_posix.h>
#define port_system ebisu_posix_system
#endif

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

TEST_CASE( "The port's system makes a lock that lets one task hold it at a time" ) {
    ebisu_system_t* sys = port_system();
    ebisu_lock_t* lock = ebisu_system_lock_create(sys);
    REQUIRE( lock != nullptr );
    std::atomic<int> inside(0);
    std::atomic<bool> overlapped(false);
    long counter = 0;
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 2000; ++i) {
                ebisu_lock_acquire(lock);
                if (++inside > 1) {
                    overlapped = true;
                }
                long value = counter;
                std::this_thread::yield();
                counter = value + 1;
                --inside;
                ebisu_lock_release(lock);
            }
        });
    }
    for (std::thread& t : threads) {
        t.join();
    }
    CHECK_FALSE( overlapped );
    CHECK( counter == 8000 );
    ebisu_system_lock_destroy(sys, lock);
}

TEST_CASE( "Each lock the port's system makes is a lock of its own" ) {
    ebisu_system_t* sys = port_system();
    ebisu_lock_t* a = ebisu_system_lock_create(sys);
    ebisu_lock_t* b = ebisu_system_lock_create(sys);
    REQUIRE( a != nullptr );
    REQUIRE( b != nullptr );
    CHECK( a != b );
    ebisu_lock_acquire(a);
    // Another task takes b while a is held, and waits for a.
    std::promise<void> took_b;
    std::atomic<bool> took_a(false);
    std::thread other([&]() {
        ebisu_lock_acquire(b);
        took_b.set_value();
        ebisu_lock_acquire(a);
        took_a = true;
        ebisu_lock_release(a);
        ebisu_lock_release(b);
    });
    CHECK( took_b.get_future().wait_for(std::chrono::seconds(10)) == std::future_status::ready );
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK_FALSE( took_a );
    ebisu_lock_release(a);
    other.join();
    CHECK( took_a );
    ebisu_system_lock_destroy(sys, b);
    ebisu_system_lock_destroy(sys, a);
}

TEST_CASE( "The port's system makes and destroys many locks, leaving nothing behind" ) {
    // valgrind and ASan's leak check see any left.
    ebisu_system_t* sys = port_system();
    std::vector<ebisu_lock_t*> locks;
    for (int i = 0; i < 1000; ++i) {
        ebisu_lock_t* lock = ebisu_system_lock_create(sys);
        REQUIRE( lock != nullptr );
        locks.push_back(lock);
    }
    for (ebisu_lock_t* lock : locks) {
        ebisu_lock_acquire(lock);
        ebisu_lock_release(lock);
        ebisu_system_lock_destroy(sys, lock);
    }
}

TEST_CASE( "The port's system delays and keeps time through the inline calls" ) {
    ebisu_system_t* sys = port_system();
    const uint64_t before = ebisu_system_monotonic_ms(sys);
    const auto start = std::chrono::steady_clock::now();
    ebisu_system_delay_ms(sys, 100);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK( elapsed >= std::chrono::milliseconds(100) );
    // GetTickCount64() counts in steps of 10 to 16 ms.
    CHECK( ebisu_system_monotonic_ms(sys) - before >= 80 );
}

TEST_CASE( "The port's system delays at least as long as asked, short delays too" ) {
    ebisu_system_t* sys = port_system();
    // Below a clock tick of Windows, of 1 to 15.6 ms, about one, and past it.
    for (uint32_t ms : { 1u, 2u, 3u, 5u, 10u, 15u, 16u, 17u, 31u, 33u }) {
        INFO( ms );
        const auto start = std::chrono::steady_clock::now();
        ebisu_system_delay_ms(sys, ms);
        CHECK( std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(ms) );
    }
    // No wait at all, which ends.
    ebisu_system_delay_ms(sys, 0);
}
