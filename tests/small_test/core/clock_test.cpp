// The clock's interface: what the calls over its operations do, and what a
// clock over a time of day kept elsewhere gives.
#include "catch_amalgamated.hpp"
#include <ebisu_clock.h>
#include <ebisu_object.h>
#include <ebisu_version.h>

#include <stdint.h>
#include <string>
#include <vector>

namespace {

// What the operations were called with, in order.
std::vector<std::string> calls;

// A clock giving the time the test sets, and whether it is exact.
struct TestClock {
    ebisu_clock_t base;
    int64_t now;
    bool exact;
};

bool test_now(ebisu_clock_t* self, int64_t* out_now)
{
    TestClock* clock = EBISU_CONTAINER_OF(self, TestClock, base);
    calls.push_back("now");
    *out_now = clock->now;
    return clock->exact;
}

void test_doubt(ebisu_clock_t* self)
{
    calls.push_back("doubt");
    EBISU_CONTAINER_OF(self, TestClock, base)->exact = false;
}

const ebisu_clock_ops_t with_doubt = { test_now, nullptr, test_doubt };

// A clock's operations with now alone, positionally.
const ebisu_clock_ops_t now_only = { test_now };

}

TEST_CASE( "ebisu_clock_get_time() gives what the clock's now does" ) {
    calls.clear();
    TestClock clock = { { &now_only }, 1700000000, true };
    int64_t now = 0;
    CHECK( ebisu_clock_get_time(&clock.base, &now) );
    CHECK( now == 1700000000 );
    clock.now = 1600000000;
    clock.exact = false;
    CHECK_FALSE( ebisu_clock_get_time(&clock.base, &now) );
    CHECK( now == 1600000000 );
    CHECK( calls == std::vector<std::string>({ "now", "now" }) );
}

TEST_CASE( "ebisu_clock_forget_time() calls the clock's doubt, and leaves a clock without one as it is" ) {
    calls.clear();
    TestClock doubting = { { &with_doubt }, 1700000000, true };
    ebisu_clock_forget_time(&doubting.base);
    CHECK( calls == std::vector<std::string>({ "doubt" }) );
    CHECK_FALSE( doubting.exact );

    calls.clear();
    TestClock sure = { { &now_only }, 1700000000, true };
    CHECK( now_only.doubt == nullptr );
    ebisu_clock_forget_time(&sure.base);
    CHECK( calls.empty() );
    CHECK( sure.exact );
}

TEST_CASE( "ebisu_clock_check_validity() checks both ends at the time, and the end only at a lower bound" ) {
    TestClock clock = { { &now_only }, 1700000000, true };
    const int64_t t = clock.now;
    CHECK( ebisu_clock_check_validity(&clock.base, 0, t - 1) == EBISU_VALIDITY_EXPIRED );
    CHECK( ebisu_clock_check_validity(&clock.base, t + 1, t + 86400) == EBISU_VALIDITY_NOT_YET_VALID );
    // Both ends inclusive.
    CHECK( ebisu_clock_check_validity(&clock.base, t, t) == EBISU_VALIDITY_OK );

    // A period starting later cannot be told from one that has started.
    clock.exact = false;
    CHECK( ebisu_clock_check_validity(&clock.base, t + 1, t + 86400) == EBISU_VALIDITY_OK );
    CHECK( ebisu_clock_check_validity(&clock.base, 0, t - 1) == EBISU_VALIDITY_EXPIRED );
    CHECK( ebisu_clock_check_validity(&clock.base, 0, t) == EBISU_VALIDITY_OK );
}

TEST_CASE( "A time of day earlier than the source gives the floor, a lower bound" ) {
    const int64_t floor = EBISU_SOURCE_TIME - 7 * 86400;
    int64_t now = 0;
    CHECK( ebisu_clock_from_system_time(EBISU_SOURCE_TIME + 86400, &now) );
    CHECK( now == EBISU_SOURCE_TIME + 86400 );
    CHECK( ebisu_clock_from_system_time(floor, &now) );
    CHECK( now == floor );

    // Not set: 1970, 2000, or a second short of the floor.
    CHECK_FALSE( ebisu_clock_from_system_time(floor - 1, &now) );
    CHECK( now == floor );
    now = 0;
    CHECK_FALSE( ebisu_clock_from_system_time(946684800, &now) );
    CHECK( now == floor );
    now = 0;
    CHECK_FALSE( ebisu_clock_from_system_time(0, &now) );
    CHECK( now == floor );
}
