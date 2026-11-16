#include "catch.hpp"
#include "kii.h"
#include "http_script.h"

#include <string>
#include <vector>
#include <string.h>

namespace {

std::string response(uint16_t status, const std::string& headers, const std::string& body) {
    return "HTTP/1.1 " + std::to_string(status) + " Status\r\n" + headers +
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

// The headers of a 2xx response, as Kii's documentation shows them.
const char OK_HEADERS[] =
    "X-Step-count: 3\r\n"
    "X-Environment-version: 0\r\n"
    "Content-Type: application/json;charset=UTF-8\r\n";

std::string kii_error_type(const std::string& name) {
    return "Content-Type: application/vnd.kii." + name + "+json\r\n";
}

std::string span(const jkii_member_t& member) {
    return (member.value != NULL) ?
        std::string(member.value, member.value_length) : std::string("(none)");
}

std::string copied(const jkii_member_t& member) {
    std::vector<char> buff(member.value_length + 1);
    REQUIRE( jkii_member_string(&member, buff.data(), buff.size()) == JKII_FIELD_ERR_OK );
    return buff.data();
}

void check_absent(const jkii_member_t& member) {
    CHECK( member.key == nullptr );
    CHECK( member.value == nullptr );
    CHECK( member.value_length == 0 );
    CHECK( member.type == JKII_FIELD_TYPE_ANY );
}

struct Client {
    char buff[4096];
    kii_t kii;
    khct::HttpScript http;
    kii_server_code_result_t result;

    Client() {
        memset(buff, 0, sizeof(buff));
        memset(&result, 0xff, sizeof(result));
        kii_init(&kii);
        kii_set_site(&kii, "api.kii.com");
        kii_set_app_id(&kii, "app");
        kii_set_buff(&kii, buff, sizeof(buff));
        strcpy(kii._author.author_id, "th.1");
        strcpy(kii._author.access_token, "thing-token");
        http.attach(&kii);
    }

    // Runs an endpoint that answers resp, checking what the call itself
    // returns, and reads the result.
    kii_code_t run(const std::string& resp) {
        http.responses.push_back(resp);
        kii_code_t executed = kii_execute_server_code(&kii, "ep", "{}");
        uint16_t status = kii_get_resp_status(&kii);
        CHECK( executed == ((200 <= status && status < 300) ? KII_ERR_OK : KII_ERR_RESP_STATUS) );
        memset(&result, 0xff, sizeof(result));
        return kii_get_server_code_result(&kii, &result);
    }

    std::string body() const {
        return std::string(result.body, result.body_length);
    }
};

}

TEST_CASE( "Server code's returnedValue is given, whatever its type" ) {
    struct Case {
        std::string body;
        jkii_field_type_t type;
        std::string value;
    };
    const std::vector<Case> cases = {
        { "{\"returnedValue\":\"name_of_my_friend\"}", JKII_FIELD_TYPE_STRING, "name_of_my_friend" },
        { "{\"returnedValue\":1}", JKII_FIELD_TYPE_INTEGER, "1" },
        { "{\"returnedValue\":2.5}", JKII_FIELD_TYPE_DOUBLE, "2.5" },
        { "{\"returnedValue\":true}", JKII_FIELD_TYPE_BOOLEAN, "true" },
        { "{\"returnedValue\":null}", JKII_FIELD_TYPE_NULL, "null" },
        { "{\"returnedValue\":{\"name\":\"Kii\",\"zip\":\"123456\"}}",
            JKII_FIELD_TYPE_OBJECT, "{\"name\":\"Kii\",\"zip\":\"123456\"}" },
        { "{\"returnedValue\":[\"one\", \"two\"]}", JKII_FIELD_TYPE_ARRAY, "[\"one\", \"two\"]" },
        { "{ \"returnedValue\" : \"\" }", JKII_FIELD_TYPE_STRING, "" },
        // What an endpoint that took too long returns, when it is set to.
        { "{\"returnedValue\":{\"_timeoutResponse\":{\"customField\": \"my_custom_message\"}}}",
            JKII_FIELD_TYPE_OBJECT, "{\"_timeoutResponse\":{\"customField\": \"my_custom_message\"}}" },
    };
    for (const Case& c : cases) {
        CAPTURE( c.body );
        Client client;
        REQUIRE( client.run(response(200, OK_HEADERS, c.body)) == KII_ERR_OK );
        CHECK( client.result.status == 200 );
        CHECK( client.body() == c.body );
        CHECK( client.result.body == client.buff );
        CHECK( client.result.returned_value.key == nullptr );
        CHECK( client.result.returned_value.type == c.type );
        CHECK( span(client.result.returned_value) == c.value );
        check_absent(client.result.error_code);
        check_absent(client.result.message);
        check_absent(client.result.details);
        CHECK( client.result.step_count == 3 );
        CHECK( client.result.environment_version == 0 );
    }

    // Read on with jkii.
    Client client;
    REQUIRE( client.run(response(201, "",
            "{\"returnedValue\":{\"interval\":30,\"name\":\"a\\\"b\"}}")) == KII_ERR_OK );
    jkii_member_t member;
    const jkii_member_t& value = client.result.returned_value;
    REQUIRE( jkii_find_member(value.value, value.value_length, "/interval", &member) == JKII_ERR_OK );
    int64_t interval = 0;
    CHECK( jkii_member_integer(&member, 1, 3600, &interval) == JKII_FIELD_ERR_OK );
    CHECK( interval == 30 );
    REQUIRE( jkii_find_member(value.value, value.value_length, "/name", &member) == JKII_ERR_OK );
    CHECK( copied(member) == "a\"b" );
}

// For an endpoint whose response isn't wrapped, or one that went wrong.
TEST_CASE( "A 2xx body that isn't returnedValue's envelope is left as it came" ) {
    const std::vector<std::string> bodies = {
        "",
        "{}",
        "{\"returnedvalue\":1}",
        "{\"returned\\u0056alue\":1}",
        "{\"value\":{\"returnedValue\":1}}",
        "[{\"returnedValue\":1}]",
        "\"returnedValue\"",
        "returnedValue",
        "{\"returnedValue\":}",
        "{\"returnedValue\":1",
        "<html>ok</html>",
    };
    for (const std::string& body : bodies) {
        CAPTURE( body );
        Client client;
        REQUIRE( client.run(response(200, OK_HEADERS, body)) == KII_ERR_PARSE_JSON );
        CHECK( client.result.status == 200 );
        CHECK( client.body() == body );
        check_absent(client.result.returned_value);
        check_absent(client.result.error_code);
        check_absent(client.result.message);
        check_absent(client.result.details);
        CHECK( client.result.step_count == 3 );
    }

    // Even with Kii's type for errors.
    Client client;
    REQUIRE( client.run(response(200, kii_error_type("ValidationException"),
            "{\"errorCode\":\"E\",\"message\":\"m\"}")) == KII_ERR_PARSE_JSON );
    check_absent(client.result.error_code);
}

TEST_CASE( "An error of Kii's own gives its errorCode, message and details" ) {
    SECTION( "an endpoint that threw" ) {
        Client client;
        REQUIRE( client.run(response(400, kii_error_type("EndpointInvocationErrorException"),
                "{\"errorCode\":\"ENDPOINT_INVOCATION_ERROR\","
                "\"message\":\"Error found while executing the developer-defined code\","
                "\"details\":{\"errorCode\":\"RUNTIME_ERROR\",\"message\":\"x is not defined\"}}"))
                == KII_ERR_RESP_STATUS );
        CHECK( client.result.status == 400 );
        CHECK( client.result.error_code.type == JKII_FIELD_TYPE_STRING );
        CHECK( copied(client.result.error_code) == "ENDPOINT_INVOCATION_ERROR" );
        CHECK( copied(client.result.message) ==
                "Error found while executing the developer-defined code" );
        const jkii_member_t& details = client.result.details;
        REQUIRE( details.type == JKII_FIELD_TYPE_OBJECT );
        jkii_member_t member;
        REQUIRE( jkii_find_member(details.value, details.value_length, "/errorCode", &member) == JKII_ERR_OK );
        CHECK( copied(member) == "RUNTIME_ERROR" );
        REQUIRE( jkii_find_member(details.value, details.value_length, "/message", &member) == JKII_ERR_OK );
        CHECK( copied(member) == "x is not defined" );
        check_absent(client.result.returned_value);
    }
    SECTION( "an endpoint that took too long, as Kii's documentation lays it out" ) {
        Client client;
        REQUIRE( client.run(response(400, kii_error_type("EndpointInvocationErrorException"),
                "{\n"
                "  \"errorCode\" : \"ENDPOINT_INVOCATION_ERROR\",\n"
                "  \"message\" : \"Error found while executing the developer-defined code\",\n"
                "  \"details\" : {\n"
                "    \"errorCode\" : \"EXECUTION_TIMEOUT\",\n"
                "    \"message\" : \"execution timeout. limit=20000ms\"\n"
                "  }\n"
                "}\n")) == KII_ERR_RESP_STATUS );
        CHECK( copied(client.result.error_code) == "ENDPOINT_INVOCATION_ERROR" );
        const jkii_member_t& details = client.result.details;
        jkii_member_t member;
        REQUIRE( jkii_find_member(details.value, details.value_length, "/errorCode", &member) == JKII_ERR_OK );
        CHECK( copied(member) == "EXECUTION_TIMEOUT" );
    }
    SECTION( "a version that doesn't exist, without details" ) {
        Client client;
        REQUIRE( client.run(response(404, kii_error_type("ServerCodeVersionNotFoundException"),
                "{\"errorCode\":\"SERVER_CODE_VERSION_NOT_FOUND\","
                "\"message\":\"The version v9 in app app was not found\"}")) == KII_ERR_RESP_STATUS );
        CHECK( client.result.status == 404 );
        CHECK( copied(client.result.error_code) == "SERVER_CODE_VERSION_NOT_FOUND" );
        CHECK( copied(client.result.message) == "The version v9 in app app was not found" );
        check_absent(client.result.details);
    }
    SECTION( "details of any type, and a message with escapes" ) {
        Client client;
        REQUIRE( client.run(response(400, kii_error_type("ValidationException"),
                "{\"details\":\"no such endpoint\",\"message\":\"bad \\\"ep\\\"\\n\","
                "\"errorCode\":\"INVALID_INPUT_DATA\"}")) == KII_ERR_RESP_STATUS );
        CHECK( copied(client.result.error_code) == "INVALID_INPUT_DATA" );
        CHECK( copied(client.result.message) == "bad \"ep\"\n" );
        CHECK( client.result.details.type == JKII_FIELD_TYPE_STRING );
        CHECK( copied(client.result.details) == "no such endpoint" );
    }
    SECTION( "a message that isn't a string" ) {
        Client client;
        REQUIRE( client.run(response(403, kii_error_type("WrongTokenException"),
                "{\"errorCode\":\"WRONG_TOKEN\",\"message\":null,\"details\":[1]}")) == KII_ERR_RESP_STATUS );
        CHECK( copied(client.result.error_code) == "WRONG_TOKEN" );
        check_absent(client.result.message);
        CHECK( client.result.details.type == JKII_FIELD_TYPE_ARRAY );
        CHECK( span(client.result.details) == "[1]" );
    }
    SECTION( "Kii's type, with parameters, in any case" ) {
        const std::vector<std::string> types = {
            "Content-Type: application/vnd.kii.ValidationException+json; charset=UTF-8\r\n",
            "Content-Type:application/vnd.kii.ValidationException+json;charset=UTF-8\r\n",
            "Content-Type: Application/VND.Kii.ValidationException+JSON\r\n",
            "content-type: application/vnd.kii.V+json  \r\n",
            "CONTENT-TYPE: application/vnd.kii.V+json ;\r\n",
            // The last one counts.
            "Content-Type: text/plain\r\nContent-Type: application/vnd.kii.V+json\r\n",
        };
        for (const std::string& type : types) {
            CAPTURE( type );
            Client client;
            REQUIRE( client.run(response(400, type, "{\"errorCode\":\"E\"}")) == KII_ERR_RESP_STATUS );
            CHECK( copied(client.result.error_code) == "E" );
            check_absent(client.result.message);
            check_absent(client.result.details);
        }
    }
}

TEST_CASE( "Only a 2xx response is read as returnedValue's envelope" ) {
    for (uint16_t status : { 300, 302, 399, 400, 500 }) {
        CAPTURE( status );
        Client client;
        REQUIRE( client.run(response(status, OK_HEADERS, "{\"returnedValue\":1}")) == KII_ERR_RESP_STATUS );
        CHECK( client.result.status == status );
        check_absent(client.result.returned_value);
        CHECK( client.body() == "{\"returnedValue\":1}" );
    }
    for (uint16_t status : { 200, 201, 202, 299 }) {
        CAPTURE( status );
        Client client;
        REQUIRE( client.run(response(status, OK_HEADERS, "{\"returnedValue\":1}")) == KII_ERR_OK );
        CHECK( span(client.result.returned_value) == "1" );
    }
}

// From a proxy on the way, say, or a body that isn't one.
TEST_CASE( "Any other error is left as it came" ) {
    const std::string error = "{\"errorCode\":\"E\",\"message\":\"m\",\"details\":{}}";
    struct Case {
        std::string headers;
        std::string body;
    };
    const std::vector<Case> cases = {
        { "", error },
        { "Content-Type: application/json\r\n", error },
        { "Content-Type: text/html\r\n", "<html>Unavailable</html>" },
        { "Content-Type: application/vnd.kii.+json\r\n", error },
        { "Content-Type: application/vnd.kiiV+json\r\n", error },
        { "Content-Type: application/vnd.kii.V+xml\r\n", error },
        { "Content-Type: application/vnd.kii.V+jsonx\r\n", error },
        { "Content-Type: text/vnd.kii.V+json\r\n", error },
        { "Content-Type: x; application/vnd.kii.V+json\r\n", error },
        { "Content-Type: application/vnd.kii.V+json\r\nContent-Type: application/json\r\n", error },
        { "X-Content-Type: application/vnd.kii.V+json\r\n", error },
        { "Content-Type-X: application/vnd.kii.V+json\r\n", error },
        { kii_error_type("V"), "" },
        { kii_error_type("V"), "not json" },
        { kii_error_type("V"), "{\"errorCode\":\"E\"" },
        { kii_error_type("V"), "[\"E\"]" },
        { kii_error_type("V"), "{\"message\":\"m\",\"details\":{}}" },
        { kii_error_type("V"), "{\"errorCode\":1,\"message\":\"m\"}" },
        { kii_error_type("V"), "{\"errorCode\":null,\"message\":\"m\"}" },
        { kii_error_type("V"), "{\"details\":{\"errorCode\":\"E\"}}" },
    };
    for (uint16_t status : { 400, 401, 404, 502 }) {
        for (const Case& c : cases) {
            CAPTURE( status, c.headers, c.body );
            Client client;
            REQUIRE( client.run(response(status, c.headers, c.body)) == KII_ERR_RESP_STATUS );
            CHECK( client.result.status == status );
            CHECK( client.body() == c.body );
            check_absent(client.result.returned_value);
            check_absent(client.result.error_code);
            check_absent(client.result.message);
            check_absent(client.result.details);
        }
    }
}

TEST_CASE( "X-Step-Count and X-Environment-Version come from each response" ) {
    Client client;
    REQUIRE( client.run(response(200,
            "X-Step-Count: 12\r\nX-Environment-Version: 16\r\n",
            "{\"returnedValue\":1}")) == KII_ERR_OK );
    CHECK( client.result.step_count == 12 );
    CHECK( client.result.environment_version == 16 );

    // Not left from the last.
    REQUIRE( client.run(response(200, "", "{\"returnedValue\":1}")) == KII_ERR_OK );
    CHECK( client.result.step_count == -1 );
    CHECK( client.result.environment_version == -1 );

    // An error has them too.
    REQUIRE( client.run(response(400, kii_error_type("EndpointInvocationErrorException") +
            "x-step-count:5\r\nX-ENVIRONMENT-VERSION:  6 \r\n",
            "{\"errorCode\":\"ENDPOINT_INVOCATION_ERROR\"}")) == KII_ERR_RESP_STATUS );
    CHECK( client.result.step_count == 5 );
    CHECK( client.result.environment_version == 6 );

    struct Case {
        std::string value;
        int64_t count;
    };
    const std::vector<Case> cases = {
        { "0", 0 },
        { "007", 7 },
        { "9223372036854775807", INT64_MAX },
        { "9223372036854775808", -1 },
        { "99999999999999999999", -1 },
        { "", -1 },
        { "-1", -1 },
        { "+3", -1 },
        { "3x", -1 },
        { "3 4", -1 },
        { "0x10", -1 },
        { "1.5", -1 },
        { "16.20.2", -1 },
    };
    for (const Case& c : cases) {
        CAPTURE( c.value );
        REQUIRE( client.run(response(200,
                "X-Step-Count: " + c.value + "\r\nX-Environment-Version: " + c.value + "\r\n",
                "{\"returnedValue\":1}")) == KII_ERR_OK );
        CHECK( client.result.step_count == c.count );
        CHECK( client.result.environment_version == c.count );
    }

    // Names that are like them, and the last of two.
    REQUIRE( client.run(response(200,
            "X-Step-Counts: 1\r\nX-Step: 2\r\nStep-Count: 3\r\n"
            "X-Environment-Version: 4\r\nX-Environment-Version: 5\r\n",
            "{\"returnedValue\":1}")) == KII_ERR_OK );
    CHECK( client.result.step_count == -1 );
    CHECK( client.result.environment_version == 5 );
}

TEST_CASE( "Without a response, there is none to read" ) {
    Client client;
    REQUIRE( kii_get_server_code_result(&client.kii, &client.result) == KII_ERR_FAIL );
    CHECK( client.result.status == 0 );
    CHECK( client.result.body_length == 0 );
    CHECK( client.result.step_count == -1 );
    CHECK( client.result.environment_version == -1 );
    check_absent(client.result.returned_value);

    REQUIRE( client.run(response(200, OK_HEADERS, "{\"returnedValue\":1}")) == KII_ERR_OK );

    // A request not made leaves nothing of the last response.
    const std::string endpoint(4096, 'e');
    REQUIRE( kii_execute_server_code(&client.kii, endpoint.c_str(), "{}") == KII_ERR_TOO_LARGE_DATA );
    REQUIRE( kii_get_server_code_result(&client.kii, &client.result) == KII_ERR_FAIL );
    CHECK( client.result.status == 0 );
    CHECK( client.result.body_length == 0 );
    CHECK( client.result.step_count == -1 );
    CHECK( client.result.environment_version == -1 );
    check_absent(client.result.returned_value);

    // Nor does one whose connection was refused.
    REQUIRE( client.run(response(200, OK_HEADERS, "{\"returnedValue\":1}")) == KII_ERR_OK );
    client.http.responses.push_back("");
    REQUIRE( kii_execute_server_code(&client.kii, "ep", "{}") == KII_ERR_SOCK_CONNECT );
    REQUIRE( kii_get_server_code_result(&client.kii, &client.result) == KII_ERR_FAIL );
    CHECK( client.result.status == 0 );
    CHECK( client.result.step_count == -1 );

    // A kii_t without a buffer has no body to give.
    kii_t bare;
    kii_init(&bare);
    kii_server_code_result_t result;
    REQUIRE( kii_get_server_code_result(&bare, &result) == KII_ERR_FAIL );
    CHECK( result.body == nullptr );
    CHECK( result.body_length == 0 );
}
