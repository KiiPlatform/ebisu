#include "catch.hpp"
#include "kii.h"
#include "http_script.h"

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <string.h>

using khct::http_response;

// kii clears what held a password, a token or MQTT credentials once done with
// it: the buffers set by kii_set_buff() and kii_set_stream_buff() after the
// calls that send or receive one, and the header lines as they are freed. They
// were left as they were, for whatever read that memory next.
namespace {

const char PASSWORD[] = "PASSWORD-SECRET";

// Each header line as allocated, and as it was when freed.
struct Headers {
    std::map<const khc_slist*, std::string> live;
    std::vector<std::pair<std::string, std::string>> freed;
};

khc_slist* cb_alloc(const char* str, size_t len, void* data) {
    khc_slist* node = khc_cb_slist_alloc(str, len, NULL);
    if (node != NULL) {
        ((Headers*)data)->live[node] = std::string(str, len);
    }
    return node;
}

void cb_free(khc_slist* node, void* data) {
    Headers* h = (Headers*)data;
    const std::string allocated = h->live[node];
    h->live.erase(node);
    h->freed.push_back(std::make_pair(allocated, std::string(node->data, allocated.size())));
    khc_cb_slist_free(node, NULL);
}

struct Client {
    char buff[2048];
    char stream[512];
    jkii_token_t tokens[128];
    jkii_resource_t resource;
    kii_t kii;
    khct::HttpScript http;
    Headers headers;

    Client() {
        // Defined, as holds() looks through all of both.
        memset(buff, 0, sizeof(buff));
        memset(stream, 0, sizeof(stream));
        resource.tokens = tokens;
        resource.tokens_num = 128;
        kii_init(&kii);
        kii_set_site(&kii, "api.kii.com");
        kii_set_app_id(&kii, "app");
        kii_set_buff(&kii, buff, sizeof(buff));
        kii_set_stream_buff(&kii, stream, sizeof(stream));
        kii_set_json_parser_resource(&kii, &resource);
        kii_set_cb_slist_resource(&kii, cb_alloc, cb_free, &headers, &headers);
        http.attach(&kii);
    }

    // Whether s is anywhere in either buffer.
    bool holds(const std::string& s) const {
        return std::string(buff, sizeof(buff)).find(s) != std::string::npos ||
            std::string(stream, sizeof(stream)).find(s) != std::string::npos;
    }

    bool all_zero(const char* field, size_t size) const {
        return std::string(field, size) == std::string(size, '\0');
    }
};

} // namespace

TEST_CASE( "kii clears the password and the tokens of a thing's authentication" ) {
    Client c;
    // One held from before, longer than the cut a '\0' at its start made.
    strcpy(c.kii._author.refresh_token, "OLD-REFRESH-TOKEN-SECRET");

    SECTION("accepted") {
        c.http.responses = { http_response(200,
                "{\"id\":\"th.1\",\"access_token\":\"ACCESS-TOKEN-SECRET\",\"expires_in\":3600}") };
        REQUIRE( kii_auth_thing(&c.kii, "vendor-1", PASSWORD) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 1 );
        REQUIRE( khct::contains(c.http.requests[0], PASSWORD) );

        CHECK( std::string(c.kii._author.access_token) == "ACCESS-TOKEN-SECRET" );
        CHECK_FALSE( c.holds(PASSWORD) );
        CHECK_FALSE( c.holds("ACCESS-TOKEN-SECRET") );
        CHECK( kii_get_resp_body_length(&c.kii) == 0 );
        // The new pair has no refresh token, and nothing is left of the old.
        CHECK( c.all_zero(c.kii._author.refresh_token, sizeof(c.kii._author.refresh_token)) );
    }

    SECTION("refused, which says why and stays") {
        const std::string why = "{\"errorCode\":\"invalid_grant\"}";
        c.http.responses = { http_response(400, why) };
        REQUIRE( kii_auth_thing(&c.kii, "vendor-1", PASSWORD) == KII_ERR_RESP_STATUS );
        REQUIRE( kii_get_resp_body_length(&c.kii) == why.size() );
        CHECK( std::string(c.buff, why.size()) == why );
        CHECK_FALSE( c.holds(PASSWORD) );
    }
}

TEST_CASE( "kii clears the password and the token of a thing's registration" ) {
    Client c;
    strcpy(c.kii._author.refresh_token, "OLD-REFRESH-TOKEN-SECRET");
    c.http.responses = { http_response(201,
            "{\"_accessToken\":\"ACCESS-TOKEN-SECRET\",\"_thingID\":\"th.2\"}") };
    REQUIRE( kii_register_thing(&c.kii, "vendor-2", "type", PASSWORD) == KII_ERR_OK );
    REQUIRE( khct::contains(c.http.requests[0], PASSWORD) );

    CHECK( std::string(c.kii._author.access_token) == "ACCESS-TOKEN-SECRET" );
    CHECK_FALSE( c.holds(PASSWORD) );
    CHECK_FALSE( c.holds("ACCESS-TOKEN-SECRET") );
    CHECK( c.all_zero(c.kii._author.refresh_token, sizeof(c.kii._author.refresh_token)) );
}

TEST_CASE( "kii clears the password, the token and the MQTT credentials of onboarding" ) {
    Client c;
    strcpy(c.kii._author.refresh_token, "OLD-REFRESH-TOKEN-SECRET");
    c.http.responses = { http_response(200,
            "{\"accessToken\":\"ACCESS-TOKEN-SECRET\",\"thingID\":\"th.3\","
            "\"mqttEndpoint\":{\"installationID\":\"inst\",\"username\":\"mqtt-user\","
            "\"password\":\"MQTT-PASSWORD-SECRET\",\"mqttTopic\":\"topic\","
            "\"host\":\"mqtt.example\",\"portTCP\":1883,\"portSSL\":8883}}") };
    REQUIRE( kii_ti_onboard(&c.kii, "vendor-3", PASSWORD, NULL, NULL, NULL, NULL) == KII_ERR_OK );
    REQUIRE( khct::contains(c.http.requests[0], PASSWORD) );

    // Kept where kii uses them.
    CHECK( std::string(c.kii._author.access_token) == "ACCESS-TOKEN-SECRET" );
    CHECK( std::string(c.kii._onboarded_endpoint.password) == "MQTT-PASSWORD-SECRET" );
    // And nowhere else.
    CHECK_FALSE( c.holds(PASSWORD) );
    CHECK_FALSE( c.holds("ACCESS-TOKEN-SECRET") );
    CHECK_FALSE( c.holds("MQTT-PASSWORD-SECRET") );
    CHECK( c.all_zero(c.kii._author.refresh_token, sizeof(c.kii._author.refresh_token)) );
}

TEST_CASE( "kii clears the MQTT credentials it fetched, and the token's header" ) {
    Client c;
    strcpy(c.kii._author.author_id, "th.4");
    strcpy(c.kii._author.access_token, "ACCESS-TOKEN-SECRET");
    c.http.responses = { http_response(200,
            "{\"username\":\"mqtt-user\",\"password\":\"MQTT-PASSWORD-SECRET\","
            "\"host\":\"mqtt.example\",\"mqttTopic\":\"topic\",\"portTCP\":1883,"
            "\"portSSL\":8883,\"X-MQTT-TTL\":3600}") };
    kii_mqtt_endpoint_t endpoint;
    memset(&endpoint, 0, sizeof(endpoint));
    REQUIRE( kii_get_mqtt_endpoint(&c.kii, "inst", &endpoint) == KII_ERR_OK );
    REQUIRE( khct::contains(c.http.requests[0], "Authorization: Bearer ACCESS-TOKEN-SECRET") );

    CHECK( std::string(endpoint.password) == "MQTT-PASSWORD-SECRET" );
    CHECK_FALSE( c.holds("MQTT-PASSWORD-SECRET") );
    CHECK_FALSE( c.holds("ACCESS-TOKEN-SECRET") );

    // Every header line, the token's among them, was cleared before it was
    // freed.
    CHECK( c.headers.live.empty() );
    bool had_token = false;
    for (const auto& f : c.headers.freed) {
        INFO("header " << f.first);
        had_token = had_token || f.first.find("ACCESS-TOKEN-SECRET") != std::string::npos;
        CHECK( f.second == std::string(f.first.size(), '\0') );
    }
    CHECK( had_token );
}

// The token's header line is built in the buffer, where a shorter body went
// over it, and a response with no body, which clears the buffer as it comes,
// left the rest.
TEST_CASE( "An API call leaves the token's header line in neither buffer" ) {
    Client c;
    strcpy(c.kii._author.author_id, "th.5");
    strcpy(c.kii._author.access_token, "ACCESS-TOKEN-SECRET");
    c.http.responses = { http_response(204, "") };
    REQUIRE( kii_api_call_start(&c.kii, "PUT", "/api/apps/app/things/th.5/x",
                "application/json", true) == KII_ERR_OK );
    REQUIRE( kii_api_call_append_body(&c.kii, "{}", 2) == KII_ERR_OK );
    REQUIRE( kii_api_call_run(&c.kii) == KII_ERR_OK );
    REQUIRE( khct::contains(c.http.requests[0], "Authorization: Bearer ACCESS-TOKEN-SECRET") );
    CHECK_FALSE( c.holds("ACCESS-TOKEN-SECRET") );
}
