// How jkii reads a double: with the C library's strtod(), or, built with
// EBISU_NO_STRTOD, with code of its own, which these compare with strtod().
#include "catch.hpp"
#include <jkii.h>
#include <jkii_utils.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include <string>

namespace {

// The bits of a double as an integer that orders like it, for positive ones.
int64_t ordered(double d) {
    int64_t i;
    memcpy(&i, &d, sizeof(i));
    return i;
}

// How many doubles apart two of the same sign are.
uint64_t ulps_apart(double a, double b) {
    if (a < 0) {
        a = -a;
        b = -b;
    }
    int64_t ia = ordered(a);
    int64_t ib = ordered(b);
    return ia > ib ? (uint64_t)(ia - ib) : (uint64_t)(ib - ia);
}

jkii_primitive_err_t read_double(const std::string& s, double* out) {
    jkii_primitive_t r;
    jkii_primitive_err_t res = jkii_parse_primitive(s.c_str(), s.size(), &r);
    if (res == JKII_PRIMITIVE_ERR_OK) {
        REQUIRE(r.type == JKII_FIELD_TYPE_DOUBLE);
        *out = r.value.double_value;
    }
    return res;
}

#ifdef EBISU_NO_STRTOD
// Where the digits fit in 2^53 and the power of ten is within 10^22, jkii's
// result is strtod()'s. On a 32-bit x86 that computes on the x87, which rounds
// twice, it can be a unit off.
#if defined(FLT_EVAL_METHOD) && FLT_EVAL_METHOD != 0
const uint64_t exact_ulps = 1;
#else
const uint64_t exact_ulps = 0;
#endif
// Beyond that, a few units at most.
const uint64_t beyond_ulps = 8;
#else
// strtod() itself.
const uint64_t exact_ulps = 0;
const uint64_t beyond_ulps = 0;
#endif

void check_as_strtod(const std::string& s, uint64_t max_ulps) {
    INFO("number " << s);
    double expected = strtod(s.c_str(), nullptr);
    double got = 0;
    REQUIRE(read_double(s, &got) == JKII_PRIMITIVE_ERR_OK);
    CHECK(ulps_apart(got, expected) <= max_ulps);
}

}

TEST_CASE("jkii reads a double as strtod() does, or within a few units in the last place") {
    SECTION("numbers a device sends") {
        for (const char* s : { "25.5", "-0.1", "1.5e3", "2.25E-1", "1e2", "0.1e1", "1E+2",
                "3.14159", "-273.15", "0.000001", "65535.0", "123456789012345.6" }) {
            check_as_strtod(s, exact_ulps);
        }
    }
    SECTION("the longest and the most extreme") {
        for (const char* s : { "3.141592653589793238462643383279",
                "123456789012345678901234567890.5", "1.797693e308",
                "2.2250738585072014e-308", "4.9406564584124654e-300", "9007199254740993.0",
                "1e22", "1e23", "1e-22", "1e-23", "8.98846567431158e307" }) {
            check_as_strtod(s, beyond_ulps);
        }
    }
    SECTION("the largest double") {
        double d = 0;
        jkii_primitive_err_t res = read_double("1.7976931348623157e308", &d);
#ifdef EBISU_NO_STRTOD
        // A few units in the last place above it is beyond a double, which
        // jkii then says.
        CHECK((res == JKII_PRIMITIVE_ERR_OK || res == JKII_PRIMITIVE_ERR_OVERFLOW));
#else
        REQUIRE(res == JKII_PRIMITIVE_ERR_OK);
        CHECK(d == DBL_MAX);
#endif
    }
    SECTION("zero, with its sign") {
        double d = 1;
        REQUIRE(read_double("0.0", &d) == JKII_PRIMITIVE_ERR_OK);
        CHECK(d == 0);
        CHECK_FALSE(signbit(d));
        REQUIRE(read_double("-0.0", &d) == JKII_PRIMITIVE_ERR_OK);
        CHECK(d == 0);
        CHECK(signbit(d));
        REQUIRE(read_double("0e99999", &d) == JKII_PRIMITIVE_ERR_OK);
        CHECK(d == 0);
    }
    SECTION("a point with no digits after it, as strtod() takes it") {
        check_as_strtod("1.", exact_ulps);
        check_as_strtod("1.e5", exact_ulps);
    }
}

TEST_CASE("jkii says a double too large or too small for one is so") {
    double d = 0;
    CHECK(read_double("1e309", &d) == JKII_PRIMITIVE_ERR_OVERFLOW);
    CHECK(read_double("-1e309", &d) == JKII_PRIMITIVE_ERR_OVERFLOW);
    CHECK(read_double("1.5e100000", &d) == JKII_PRIMITIVE_ERR_OVERFLOW);
    CHECK(read_double("1e-400", &d) == JKII_PRIMITIVE_ERR_UNDERFLOW);
    CHECK(read_double("-1e-99999", &d) == JKII_PRIMITIVE_ERR_UNDERFLOW);
    CHECK(read_double("0.5e", &d) == JKII_PRIMITIVE_ERR_INVALID);
    CHECK(read_double("0.5e+", &d) == JKII_PRIMITIVE_ERR_INVALID);
}

TEST_CASE("jkii reads random doubles as strtod() does, or within a few units") {
    std::mt19937_64 rng(20261108);
    char buff[64];

    SECTION("up to 15 digits, and a power of ten within 10^22: as strtod() does") {
        std::uniform_int_distribution<uint64_t> mantissa(1, UINT64_C(999999999999999));
        std::uniform_int_distribution<int> power(-22, 22);
        for (int n = 0; n < 20000; ++n) {
            uint64_t m = mantissa(rng);
            int e = power(rng);
            snprintf(buff, sizeof(buff), "%llue%d", (unsigned long long)m, e);
            check_as_strtod(buff, exact_ulps);
            // The same, with a point: 123e-2 as 1.23, 123e2 as 12300.0.
            std::string digits = std::to_string(m);
            int point = (int)digits.size() + e;
            std::string with_point;
            if (point <= 0) {
                with_point = "0." + std::string(-point, '0') + digits;
            } else if (point >= (int)digits.size()) {
                with_point = digits + std::string(point - digits.size(), '0') + ".0";
            } else {
                with_point = digits.substr(0, point) + "." + digits.substr(point);
            }
            if (with_point.size() < 40) {
                check_as_strtod(with_point, exact_ulps);
            }
        }
    }
    SECTION("any double, written with 17 digits, or fewer") {
        std::uniform_real_distribution<double> unit(1.0, 10.0);
        std::uniform_int_distribution<int> power(-300, 300);
        std::uniform_int_distribution<int> precision(1, 17);
        for (int n = 0; n < 20000; ++n) {
            double v = unit(rng) * pow(10.0, power(rng));
            snprintf(buff, sizeof(buff), "%.*e", precision(rng) - 1, n % 2 ? -v : v);
            check_as_strtod(buff, beyond_ulps);
        }
    }
}
