#include <catch.hpp>
#include <ebisu_retry.h>
#include <stdint.h>
#include <set>

namespace {

// What the MQTT task waited before ebisu_retry.h, from its wait and the
// thing's ID: half to all of the wait, rounded down.
uint32_t task_wait_before(uint32_t wait, const char* id) {
    uint32_t hash = 2166136261U;
    for (const char* c = id; *c != '\0'; ++c) {
        hash = (hash ^ (unsigned char)*c) * 16777619U;
    }
    hash = (hash ^ wait) * 16777619U;
    return wait / 2 + hash % (wait / 2 + 1);
}

// min(first * 2^(n - 1), max), n from 1, in 64 bits: any first of 1 or more
// doubled 32 times is past every max.
uint32_t doubled(uint32_t first, uint32_t max, uint32_t n) {
    uint32_t shift = n <= 1 ? 0 : n - 1;
    if (first == 0) {
        return 0;
    }
    if (shift >= 32) {
        return max;
    }
    uint64_t wait = (uint64_t)first << shift;
    return wait < max ? (uint32_t)wait : max;
}

const uint32_t waits[] = {
    0, 1, 2, 3, 4, 5, 999, 1000, 1001, 2000, 300000, 2147483647U, 2147483648U, 4294967000U, 4294967294U,
    4294967295U
};

}  // namespace

TEST_CASE( "ebisu_retry_seed() is 32-bit FNV-1a" ) {
    // The test vectors of the FNV reference code.
    CHECK( ebisu_retry_seed("") == 0x811c9dc5U );
    CHECK( ebisu_retry_seed("a") == 0xe40c292cU );
    CHECK( ebisu_retry_seed("foobar") == 0xbf9cf968U );
    CHECK( ebisu_retry_seed(NULL) == ebisu_retry_seed("") );
    // Bytes above 0x7f as unsigned, whatever char is.
    CHECK( ebisu_retry_seed("\xff") == ((0x811c9dc5U ^ 0xffU) * 16777619U) );
}

TEST_CASE( "ebisu_retry_spread() gives half, rounded up, to all of the wait" ) {
    const char* ids[] = { "", "th.1", "th.2", "a-much-longer-thing-id-0123456789" };
    for (const char* id : ids) {
        uint32_t seed = ebisu_retry_seed(id);
        for (uint32_t wait : waits) {
            INFO("id " << id << ", wait " << wait);
            uint32_t got = ebisu_retry_spread(wait, seed);
            CHECK( got >= wait - wait / 2 );
            CHECK( got <= wait );
        }
    }
    CHECK( ebisu_retry_spread(0, 12345) == 0 );
    CHECK( ebisu_retry_spread(1, 12345) == 1 );
}

TEST_CASE( "ebisu_retry_spread() of an even wait is what the MQTT task waited before it" ) {
    const char* ids[] = { "", "th.1", "th.2", "a-much-longer-thing-id-0123456789" };
    for (const char* id : ids) {
        uint32_t seed = ebisu_retry_seed(id);
        for (uint32_t wait = 2000; wait <= 300000; wait *= 2) {
            INFO("id " << id << ", wait " << wait);
            CHECK( ebisu_retry_spread(wait, seed) == task_wait_before(wait, id) );
        }
        for (uint32_t wait = 0; wait < 20000; wait += 2) {
            INFO("id " << id << ", wait " << wait);
            CHECK( ebisu_retry_spread(wait, seed) == task_wait_before(wait, id) );
        }
        CHECK( ebisu_retry_spread(4294967000U, seed) == task_wait_before(4294967000U, id) );
        CHECK( ebisu_retry_spread(4294967294U, seed) == task_wait_before(4294967294U, id) );
    }
}

TEST_CASE( "ebisu_retry_spread() spreads devices, and a device's share changes from wait to wait" ) {
    uint32_t one = ebisu_retry_seed("th.1");
    uint32_t other = ebisu_retry_seed("th.2");
    int differ = 0;
    std::set<uint32_t> shares;
    for (uint32_t wait = 1000; wait < 2000; ++wait) {
        if (ebisu_retry_spread(wait, one) != ebisu_retry_spread(wait, other)) {
            ++differ;
        }
        shares.insert(ebisu_retry_spread(wait, one) - (wait - wait / 2));
    }
    // Two devices wait the same at most now and then, and one doesn't wait
    // the same part of every wait.
    CHECK( differ > 990 );
    CHECK( shares.size() > 100 );
}

TEST_CASE( "ebisu_retry_wait() doubles the first wait up to the longest" ) {
    uint32_t seed = ebisu_retry_seed("th.1");

    SECTION("in milliseconds, as the MQTT task's") {
        for (uint32_t n = 1; n <= 20; ++n) {
            INFO("n " << n);
            CHECK( ebisu_retry_wait(2000, 300000, n, seed) ==
                   ebisu_retry_spread(doubled(2000, 300000, n), seed) );
        }
        CHECK( doubled(2000, 300000, 8) == 256000 );
        CHECK( doubled(2000, 300000, 9) == 300000 );
    }

    SECTION("in seconds") {
        for (uint32_t n = 1; n <= 20; ++n) {
            INFO("n " << n);
            uint32_t got = ebisu_retry_wait(360, 3600, n, seed);
            CHECK( got == ebisu_retry_spread(doubled(360, 3600, n), seed) );
            CHECK( got >= 180 );
            CHECK( got <= 3600 );
        }
    }

    SECTION("0 failures as 1") {
        CHECK( ebisu_retry_wait(2000, 300000, 0, seed) == ebisu_retry_wait(2000, 300000, 1, seed) );
    }

    SECTION("a longest wait below the first cuts that short too") {
        for (uint32_t n = 0; n <= 3; ++n) {
            INFO("n " << n);
            CHECK( ebisu_retry_wait(2000, 1000, n, seed) == ebisu_retry_spread(1000, seed) );
        }
    }

    SECTION("a first wait of 0 stays 0, however many failures") {
        CHECK( ebisu_retry_wait(0, 300000, 1, seed) == 0 );
        CHECK( ebisu_retry_wait(0, 300000, 4294967295U, seed) == 0 );
        CHECK( ebisu_retry_wait(5, 0, 7, seed) == 0 );
    }

    SECTION("no overflow, whatever the count and the waits") {
        const uint32_t counts[] = { 1, 2, 31, 32, 33, 64, 4294967295U };
        const uint32_t firsts[] = { 1, 3, 2000, 2147483647U, 2147483648U, 4294967295U };
        for (uint32_t n : counts) {
            for (uint32_t first : firsts) {
                INFO("n " << n << ", first " << first);
                CHECK( ebisu_retry_wait(first, 4294967295U, n, seed) ==
                       ebisu_retry_spread(doubled(first, 4294967295U, n), seed) );
            }
        }
        CHECK( doubled(3, 4294967295U, 31) == 3221225472U );
        CHECK( doubled(3, 4294967295U, 32) == 4294967295U );
    }
}
