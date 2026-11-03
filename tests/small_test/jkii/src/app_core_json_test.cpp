// An application with a coreJSON of its own, as FreeRTOS and AWS IoT
// applications often have: this suite links coreJSON's core_json.c, under its
// own names, beside jkii's copy, under jkii's. Were jkii's under coreJSON's
// names, the suite would not link, for two definitions of each function.
#include <catch.hpp>
#include <jkii.h>
#include <string.h>
#include "core_json.h"

TEST_CASE("jkii and an application's own coreJSON link together") {
    const char json[] = "{\"a\":{\"b\":[1,2]}}";

    // The application's.
    CHECK(JSON_Validate(json, strlen(json)) == JSONSuccess);
    const char* value = NULL;
    size_t value_length = 0;
    JSONTypes_t type = JSONInvalid;
    REQUIRE(JSON_SearchConst(json, strlen(json), "a.b[1]", 6, &value, &value_length, &type) == JSONSuccess);
    CHECK(std::string(value, value_length) == "2");

    // jkii's.
    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));
    fields[0].path = "/a/b/[1]";
    fields[0].type = JKII_FIELD_TYPE_INTEGER;
    CHECK(jkii_parse(json, strlen(json), fields) == JKII_ERR_OK);
    CHECK(fields[0].field_copy.int_value == 2);
}
