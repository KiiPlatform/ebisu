#include <catch.hpp>
#include <ebisu_mem.h>
#include <string.h>
#include <string>

TEST_CASE("ebisu_secure_zero clears what it is given, and nothing else") {
    char buff[16];
    memset(buff, '#', sizeof(buff));
    ebisu_secure_zero(&buff[2], 10);
    CHECK(std::string(buff, 2) == "##");
    CHECK(std::string(&buff[2], 10) == std::string(10, '\0'));
    CHECK(std::string(&buff[12], 4) == "####");

    SECTION("nothing, at NULL") {
        ebisu_secure_zero(nullptr, 0);
        ebisu_secure_zero(buff, 0);
        CHECK(buff[0] == '#');
    }
}
