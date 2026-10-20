#include "catch.hpp"
#include "kii.h"
#include "test_callbacks.h"

#include <cstdio>
#include <string>
#include <string.h>

static std::string response(const std::string& status_line, const std::string& body) {
    return "HTTP/1.1 " + status_line + "\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body;
}

TEST_CASE( "kii_send_push_message posts the message to the topic" ) {
    char kii_buff[1024];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_json_parser_resource(&kii, &resource);
    strcpy(kii._author.author_id, "th.1");
    strcpy(kii._author.access_token, "tok");

    // The one request is answered with resp. Everything sent is captured.
    std::string resp;
    size_t resp_pos = 0;
    int connected = 0;
    std::string sent;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&connected](void*, const char*, unsigned int) {
        ++connected;
        return KHC_SOCK_OK;
    };
    http_ctx.on_send = [&sent](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        sent.append(buffer, length);
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    http_ctx.on_recv = [&resp, &resp_pos](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        size_t n = resp.size() - resp_pos;
        if (n > length_to_read) { n = length_to_read; }
        memcpy(buffer, resp.data() + resp_pos, n);
        resp_pos += n;
        *out_actual_length = n;
        return KHC_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return KHC_SOCK_OK; };
    kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
    kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
    kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
    kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);

    const std::string message = "{\"data\":{\"key\":\"value\"},\"mqtt\":{\"enabled\":true}}";
    kii_topic_t topic;
    topic.scope = KII_SCOPE_THING;
    topic.scope_id = "th.1";
    topic.topic_name = "my-topic";

    SECTION("to a thing scope topic") {
        resp = response("201 Created", "{\"pushMessageID\":\"pm.1\"}");

        REQUIRE( kii_send_push_message(&kii, &topic, message.c_str()) == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 201 );

        const std::string request_line = "POST https://api.kii.com"
            "/api/apps/dummyAppID/things/th.1/topics/my-topic/push/messages HTTP/1.1\r\n";
        REQUIRE( sent.compare(0, request_line.size(), request_line) == 0 );
        CHECK( sent.find("\r\nContent-Type: application/vnd.kii.SendPushMessageRequest+json\r\n") != std::string::npos );
        CHECK( sent.find("\r\nAuthorization: Bearer tok\r\n") != std::string::npos );
        CHECK( sent.find("\r\nX-Kii-Appid: dummyAppID\r\n") != std::string::npos );
        // The body is the message as it was given, in one chunk.
        size_t headers_end = sent.find("\r\n\r\n");
        REQUIRE( headers_end != std::string::npos );
        char chunk_size[16];
        snprintf(chunk_size, sizeof(chunk_size), "%x", (unsigned int)message.size());
        REQUIRE( sent.substr(headers_end + 4) ==
                std::string(chunk_size) + "\r\n" + message + "\r\n0\r\n\r\n" );
    }

    SECTION("to an app scope topic") {
        topic.scope = KII_SCOPE_APP;
        topic.scope_id = NULL;
        topic.topic_name = "app-topic";
        resp = response("201 Created", "{\"pushMessageID\":\"pm.1\"}");

        REQUIRE( kii_send_push_message(&kii, &topic, message.c_str()) == KII_ERR_OK );
        const std::string request_line = "POST https://api.kii.com"
            "/api/apps/dummyAppID/topics/app-topic/push/messages HTTP/1.1\r\n";
        REQUIRE( sent.compare(0, request_line.size(), request_line) == 0 );
    }

    SECTION("to a user scope topic") {
        topic.scope = KII_SCOPE_USER;
        topic.scope_id = "user.1";
        resp = response("201 Created", "{\"pushMessageID\":\"pm.1\"}");

        REQUIRE( kii_send_push_message(&kii, &topic, message.c_str()) == KII_ERR_OK );
        const std::string request_line = "POST https://api.kii.com"
            "/api/apps/dummyAppID/users/user.1/topics/my-topic/push/messages HTTP/1.1\r\n";
        REQUIRE( sent.compare(0, request_line.size(), request_line) == 0 );
    }

    // Kii's guide documents 201 and its API reference 200: either is success.
    SECTION("200 is accepted too") {
        resp = response("200 OK", "{\"pushMessageID\":\"pm.1\"}");

        REQUIRE( kii_send_push_message(&kii, &topic, message.c_str()) == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );
    }

    SECTION("a refusal is a response status error") {
        resp = response("404 Not Found", "{\"errorCode\":\"TOPIC_NOT_FOUND\"}");

        REQUIRE( kii_send_push_message(&kii, &topic, message.c_str()) == KII_ERR_RESP_STATUS );
        REQUIRE( khc_get_status_code(&kii._khc) == 404 );
    }

    SECTION("a message larger than the buffer is not sent") {
        const std::string large = "{\"data\":{\"key\":\"" + std::string(sizeof(kii_buff), 'v') +
            "\"},\"mqtt\":{\"enabled\":true}}";

        REQUIRE( kii_send_push_message(&kii, &topic, large.c_str()) == KII_ERR_TOO_LARGE_DATA );
        REQUIRE( connected == 0 );
        REQUIRE( sent.empty() );
    }
}
