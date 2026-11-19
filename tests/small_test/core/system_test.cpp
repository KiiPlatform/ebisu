#include <catch.hpp>
#include <ebisu_system.h>

#include <string>
#include <vector>

namespace {

// What the operations were called with, in order.
std::vector<std::string> calls;

struct TestLock {
    ebisu_lock_t base;
    int id;
};

void test_lock(ebisu_lock_t* self)
{
    calls.push_back("lock " + std::to_string(EBISU_CONTAINER_OF(self, TestLock, base)->id));
}

void test_unlock(ebisu_lock_t* self)
{
    calls.push_back("unlock " + std::to_string(EBISU_CONTAINER_OF(self, TestLock, base)->id));
}

const ebisu_lock_ops_t test_lock_ops = { test_lock, test_unlock };

struct TestSystem {
    ebisu_system_t base;
    TestLock made;
};

void test_delay_ms(ebisu_system_t*, uint32_t msec)
{
    calls.push_back("delay " + std::to_string(msec));
}

uint64_t test_monotonic_ms(ebisu_system_t*)
{
    calls.push_back("monotonic");
    return 1234567890123ULL;
}

ebisu_lock_t* test_lock_create(ebisu_system_t* self)
{
    calls.push_back("create");
    TestSystem* sys = EBISU_CONTAINER_OF(self, TestSystem, base);
    sys->made.base.ops = &test_lock_ops;
    sys->made.id = 7;
    return &sys->made.base;
}

void test_lock_destroy(ebisu_system_t* self, ebisu_lock_t* lock)
{
    TestSystem* sys = EBISU_CONTAINER_OF(self, TestSystem, base);
    calls.push_back(lock == &sys->made.base ? "destroy made" : "destroy another");
}

const ebisu_system_ops_t with_locks = {
    nullptr, test_delay_ms, test_monotonic_ms, test_lock_create, test_lock_destroy
};

// The operations of a system written before lock_create, positionally.
const ebisu_system_ops_t without_locks = { nullptr, test_delay_ms, test_monotonic_ms };

bool test_clock_now(ebisu_clock_t*, int64_t* out_now)
{
    *out_now = 0;
    return false;
}

const ebisu_clock_ops_t test_clock_ops = { test_clock_now, nullptr, nullptr };

ebisu_clock_t test_clock = { &test_clock_ops };

ebisu_clock_t* test_system_clock(ebisu_system_t*)
{
    calls.push_back("clock");
    return &test_clock;
}

const ebisu_system_ops_t with_clock = {
    nullptr, test_delay_ms, test_monotonic_ms, nullptr, nullptr, test_system_clock
};

}

TEST_CASE( "ebisu_lock_acquire() and ebisu_lock_release() call the lock, and do nothing without one" ) {
    calls.clear();
    TestLock a = { { &test_lock_ops }, 1 };
    TestLock b = { { &test_lock_ops }, 2 };
    ebisu_lock_acquire(&a.base);
    ebisu_lock_acquire(&b.base);
    ebisu_lock_release(&b.base);
    ebisu_lock_release(&a.base);
    ebisu_lock_acquire(nullptr);
    ebisu_lock_release(nullptr);
    CHECK( calls == std::vector<std::string>({ "lock 1", "lock 2", "unlock 2", "unlock 1" }) );
}

TEST_CASE( "ebisu_system_delay_ms() and ebisu_system_monotonic_ms() call the system" ) {
    calls.clear();
    TestSystem sys = {};
    sys.base.ops = &with_locks;
    ebisu_system_delay_ms(&sys.base, 4294967295u);
    CHECK( ebisu_system_monotonic_ms(&sys.base) == 1234567890123ULL );
    CHECK( calls == std::vector<std::string>({ "delay 4294967295", "monotonic" }) );
}

TEST_CASE( "ebisu_system_lock_create() gives the system's lock, and ebisu_system_lock_destroy() gives it back" ) {
    calls.clear();
    TestSystem sys = {};
    sys.base.ops = &with_locks;
    ebisu_lock_t* lock = ebisu_system_lock_create(&sys.base);
    REQUIRE( lock == &sys.made.base );
    ebisu_lock_acquire(lock);
    ebisu_lock_release(lock);
    ebisu_system_lock_destroy(&sys.base, lock);
    // NULL, as a lock that couldn't be made, is nothing to destroy.
    ebisu_system_lock_destroy(&sys.base, nullptr);
    CHECK( calls == std::vector<std::string>({ "create", "lock 7", "unlock 7", "destroy made" }) );
}

TEST_CASE( "A system without lock_create makes no lock" ) {
    calls.clear();
    TestSystem sys = {};
    sys.base.ops = &without_locks;
    CHECK( without_locks.lock_create == nullptr );
    CHECK( without_locks.lock_destroy == nullptr );
    ebisu_lock_t* lock = ebisu_system_lock_create(&sys.base);
    CHECK( lock == nullptr );
    // What code given that NULL then does, as it runs in one task.
    ebisu_lock_acquire(lock);
    ebisu_lock_release(lock);
    ebisu_system_lock_destroy(&sys.base, lock);
    CHECK( calls.empty() );
}

TEST_CASE( "ebisu_system_clock() gives the system's clock, and NULL for a system without one" ) {
    calls.clear();
    TestSystem sys = {};
    sys.base.ops = &with_clock;
    CHECK( ebisu_system_clock(&sys.base) == &test_clock );
    CHECK( calls == std::vector<std::string>({ "clock" }) );

    // The operations of a system written before clock, positionally.
    calls.clear();
    sys.base.ops = &with_locks;
    CHECK( with_locks.clock == nullptr );
    CHECK( ebisu_system_clock(&sys.base) == nullptr );
    CHECK( calls.empty() );
}
