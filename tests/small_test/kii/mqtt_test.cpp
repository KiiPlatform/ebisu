#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "kii.h"
#include "kii_mqtt_task.h"
#include "test_callbacks.h"

#include <stdint.h>
#include <sstream>
#include <string>

// A QoS 0 PUBLISH as the broker sends it: the fixed header, then the topic
// length, the topic and the payload, with no packet identifier.
static std::string publish_packet(const std::string& topic, const std::string& payload)
{
    std::string packet("\x30", 1);
    size_t remaining_length = 2 + topic.length() + payload.length();
    do {
        char byte = remaining_length % 128;
        remaining_length /= 128;
        if (remaining_length > 0) {
            byte |= 0x80;
        }
        packet += byte;
    } while (remaining_length > 0);
    packet += (char)(topic.length() >> 8);
    packet += (char)(topic.length() & 0xff);
    return packet + topic + payload;
}

// Serves data as recv on a stream socket does: whatever is left of it, up to
// the size asked for. Once all of it has been read, nothing more arrives, so
// reports what a socket whose receive timeout ran out reports.
static void serve_stream(khct::cb::SockCtx& ctx, const std::string& data, size_t& served)
{
    ctx.on_recv = [data, &served](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        if (served == data.length()) {
            return KHC_SOCK_FAIL;
        }
        *out_actual_length = data.copy(buffer, length_to_read, served);
        served += *out_actual_length;
        return KHC_SOCK_OK;
    };
}

TEST_CASE( "MQTT state test" ) {
    size_t kii_buff_size = 1024;
    char kii_buff[kii_buff_size];
    size_t mqtt_buff_size = 1024;
    char mqtt_buff[kii_buff_size];
    jkii_token_t jkii_tokens[256];
    jkii_resource_t jkii_resource = {jkii_tokens, 256};

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, kii_buff_size);
    kii_set_mqtt_buff(&kii, mqtt_buff, mqtt_buff_size);
    kii_set_json_parser_resource(&kii, &jkii_resource);
    kii._keep_alive_interval = 300;

    khct::cb::SockCtx http_ctx;
    kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
    kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
    kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
    kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);
    khct::cb::SockCtx mqtt_ctx;
    kii_set_cb_mqtt_sock_connect(&kii, khct::cb::mock_connect, &mqtt_ctx);
    kii_set_cb_mqtt_sock_send(&kii, khct::cb::mock_send, &mqtt_ctx);
    kii_set_cb_mqtt_sock_recv(&kii, khct::cb::mock_recv, &mqtt_ctx);
    kii_set_cb_mqtt_sock_close(&kii, khct::cb::mock_close, &mqtt_ctx);

    kii_set_cb_delay_ms(&kii, khct::cb::cb_delay_ms, NULL);

    khct::cb::PushCtx push_ctx;
    kii._cb_push_received = khct::cb::cb_push;
    kii._push_data = &push_ctx;

    mqtt_state_t state;

    _init_mqtt_state(&kii, &state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_INSTALL_PUSH );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );

    int call_connect = 0;
    http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
        ++call_connect;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return KHC_SOCK_OK;
    };

    int call_send = 0;
    http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            const char req_line[] = "POST https://api.kii.com/api/apps/dummyAppID/installations HTTP/1.1\r\n";
            REQUIRE( length == strlen(req_line) );
            REQUIRE( strncmp(buffer, req_line, length) == 0 );
        }
        ++call_send;
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    int call_recv = 0;
    std::stringstream ss;
    ss <<
        "HTTP/1.1 201 Created\r\n"
        "Accept-Ranges: bytes\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Expose-Headers: Content-Type, Authorization, Content-Length, X-Requested-With, ETag, X-Step-Count, X-Environment-version, X-HTTP-Status-Code\r\n"
        "Age: 0\r\n"
        "Cache-Control: max-age=0, no-cache, no-store\r\n"
        "Content-Type: application/vnd.kii.InstallationCreationResponse+json;charset=UTF-8\r\n"
        "Date: Tue, 04 Dec 2018 06:41:17 GMT\r\n"
        "Location: https://api-jp.kii.com/api/apps/dummyAppID/installations/5t842kt0a4d5bvo3g61i97ft6\r\n"
        "Server: openresty\r\n"
        "X-HTTP-Status-Code: 201\r\n"
        "Content-Length: 116\r\n"
        "Connection: Close\r\n"
        "\r\n"
        "{"
        "  \"installationID\" : \"dummyInstallationID\","
        "  \"installationRegistrationID\" : \"56f6bcf7-3b1e-49c0-b625-64f810fe85a0\""
        "}";
    http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++call_recv;
        *out_actual_length = ss.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    int call_close = 0;
    http_ctx.on_close = [=, &call_close](void* socket_ctx) {
        ++call_close;
        return KHC_SOCK_OK;
    };

    _mqtt_state_install_push(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( strcmp(state.ins_id.id, "dummyInstallationID") == 0 );
    REQUIRE( call_connect == 1 );
    REQUIRE( call_send > 1 );
    REQUIRE( call_recv >= 1 );
    REQUIRE( call_close == 1 );

    call_connect = 0;
    http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
        ++call_connect;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return KHC_SOCK_OK;
    };

    call_send = 0;
    http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            const char req_line[] = "GET https://api.kii.com/api/apps/dummyAppID/installations/dummyInstallationID/mqtt-endpoint HTTP/1.1\r\n";
            REQUIRE( length == strlen(req_line) );
            REQUIRE( strncmp(buffer, req_line, length) == 0 );
        }
        ++call_send;
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    call_recv = 0;
    ss.clear();
    ss <<
        "HTTP/1.1 200 OK\r\n"
        "Accept-Ranges: bytes\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Expose-Headers: Content-Type, Authorization, Content-Length, X-Requested-With, ETag, X-Step-Count, X-Environment-version, X-HTTP-Status-Code\r\n"
        "Age: 0\r\n"
        "Cache-Control: max-age=0, no-cache, no-store\r\n"
        "Content-Type: application/vnd.kii.MQTTEndpointResponse+json;charset=UTF-8\r\n"
        "Date: Tue, 04 Dec 2018 08:02:07 GMT\r\n"
        "Server: openresty\r\n"
        "X-HTTP-Status-Code: 200\r\n"
        "X-MQTT-TTL: 2147483647\r\n"
        "Content-Length: 273\r\n"
        "Connection: Close\r\n"
        "\r\n"
        "{"
        "  \"installationID\" : \"dummyInstallationID_2\","
        "  \"username\" : \"dummyUser\","
        "  \"password\" : \"dummyPassword\","
        "  \"mqttTopic\" : \"dummyTopic\","
        "  \"host\" : \"jp-mqtt-dummy.kii.com\","
        "  \"portTCP\" : 1883,"
        "  \"portSSL\" : 8883,"
        "  \"portWS\" : 12470,"
        "  \"portWSS\" : 12473,"
        "  \"X-MQTT-TTL\" : 2147483647"
        "}";
    http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++call_recv;
        *out_actual_length = ss.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    call_close = 0;
    http_ctx.on_close = [=, &call_close](void* socket_ctx) {
        ++call_close;
        return KHC_SOCK_OK;
    };

    _mqtt_state_get_endpoint(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( strcmp(state.endpoint.username, "dummyUser") == 0 );
    REQUIRE( strcmp(state.endpoint.password, "dummyPassword") == 0 );
    REQUIRE( strcmp(state.endpoint.topic, "dummyTopic") == 0 );
    REQUIRE( strcmp(state.endpoint.host, "jp-mqtt-dummy.kii.com") == 0 );
    REQUIRE( state.endpoint.port_tcp == 1883 );
    REQUIRE( state.endpoint.port_ssl == 8883 );
    REQUIRE( state.endpoint.ttl == 2147483647 );
    REQUIRE( call_connect == 1 );
    REQUIRE( call_send > 1 );
    REQUIRE( call_recv >= 1 );
    REQUIRE( call_close == 1 );

    call_connect = 0;
    mqtt_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
        const char* exp_host = "jp-mqtt-dummy.kii.com";
        ++call_connect;
        REQUIRE( strlen(host) == strlen(exp_host) );
        REQUIRE( strncmp(host, exp_host, strlen(exp_host)) == 0 );
        REQUIRE( port == 8883 );
        return KHC_SOCK_OK;
    };

    _mqtt_state_sock_connect(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_SEND_CONNECT );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_connect == 1 );

    call_send = 0;
    mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            const char expect[] = {
                0x10, 0x32, // remaining_size = 50
                0x00, 0x06, 'M', 'Q', 'I', 's', 'd', 'p', 0x03, (char)0xc2,
                0x01, 0x2c, // _keep_alive_interval = 300
                0x00, 0x0a, 'd', 'u', 'm', 'm', 'y', 'T', 'o', 'p', 'i', 'c',
                0x00, 0x09, 'd', 'u', 'm', 'm', 'y', 'U', 's', 'e', 'r',
                0x00, 0x0d, 'd', 'u', 'm', 'm', 'y', 'P', 'a', 's', 's', 'w', 'o', 'r', 'd'
            };
            REQUIRE( length == sizeof(expect) );
            REQUIRE( memcmp(buffer, expect, length) == 0 );
        }
        ++call_send;
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    _mqtt_state_send_connect(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_CONNACK );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_send == 1 );

    call_recv = 0;
    mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        switch (call_recv) {
            case 0: // fixed header - control packet type(2)
                REQUIRE( length_to_read == 1);
                buffer[0] = 0x20;
                *out_actual_length = 1;
                break;
            case 1: // fixed header - remaining length (= 2)
                REQUIRE( length_to_read == 1);
                buffer[0] = 0x02;
                *out_actual_length = 1;
                break;
            case 2: // variable header (2 bytes)
                REQUIRE( length_to_read == 2);
                buffer[0] = 0x00;
                buffer[1] = 0x00;
                *out_actual_length = 2;
                break;
            default:
                FAIL();
                break;
        }
        ++call_recv;
        return KHC_SOCK_OK;
    };

    _mqtt_state_recv_connack(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_SEND_SUBSCRIBE );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_recv == 3 );

    call_send = 0;
    mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            const char expect[] = {
                (char)0x82, 0x0f, // remaining_size = 15
                0x00, 0x01, // packet identifier
                0x00, 0x0a, 'd', 'u', 'm', 'm', 'y', 'T', 'o', 'p', 'i', 'c',
                0x00 // QOS0
            };
            REQUIRE( length == sizeof(expect) );
            REQUIRE( memcmp(buffer, expect, length) == 0 );
        }
        ++call_send;
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    _mqtt_state_send_subscribe(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_SUBACK );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_send == 1 );

    call_recv = 0;
    mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        switch (call_recv) {
            case 0: // fixed header - control packet type(9)
                REQUIRE( length_to_read == 1);
                buffer[0] = (char)0x90;
                *out_actual_length = 1;
                break;
            case 1: // fixed header - remaining length (= 3)
                REQUIRE( length_to_read == 1);
                buffer[0] = 0x03;
                *out_actual_length = 1;
                break;
            case 2: // variable header (3 bytes)
                REQUIRE( length_to_read == 3);
                buffer[0] = 0x00; // identifier MSB.
                buffer[1] = 0x01; // identifier LSB.
                buffer[2] = 0x00; // payload(return code: Success - Maximum QOS 0)
                *out_actual_length = 3;
                break;
            default:
                FAIL();
                break;
        }
        ++call_recv;
        return KHC_SOCK_OK;
    };

    _mqtt_state_recv_suback(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_recv == 3 );

    call_recv = 0;
    mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        switch (call_recv) {
            case 0: // fixed header - Publish
                REQUIRE( length_to_read == 1);
                buffer[0] = 0x30;
                *out_actual_length = 1;
                break;
            case 1: // fixed header - remaining length[0] (= 306)
                REQUIRE( length_to_read == 1);
                buffer[0] = (char)0xb2;
                *out_actual_length = 1;
                break;
            case 2: // fixed header - remaining length[1] (= 306)
                REQUIRE( length_to_read == 1);
                buffer[0] = 0x02;
                *out_actual_length = 1;
                break;
            default:
                FAIL();
                break;
        }
        ++call_recv;
        return KHC_SOCK_OK;
    };

    _mqtt_state_recv_ready(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_recv == 3 );

    call_recv = 0;
    ss.clear();
    ss << (char)0x00 << (char)0x0a << "dummyTopic";
    std::string push_message =
        "{"
        "  \"schema\" : \"\","
        "  \"schemaVersion\" : 0,"
        "  \"target\" : \"dummyTarget\","
        "  \"issuer\" : \"dummyUser\","
        "  \"actions\" : ["
        "    {"
        "      \"AirconAlias\" : ["
        "        {"
        "          \"power\" : true"
        "        },"
        "        {"
        "          \"setPresetTemperature\" : 20"
        "        }"
        "      ]"
        "    }"
        "  ],"
        "  \"title\" : \"dummyTitle\","
        "  \"description\" : \"\""
        "}";
    ss << push_message;
    mqtt_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++call_recv;
        *out_actual_length = ss.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    bool call_push = false;
    push_ctx.on_push = [=, &call_push](const char* message, size_t message_length) {
        call_push = true;
        REQUIRE( message_length == push_message.length() );
        REQUIRE( strncmp(message, push_message.c_str(), message_length) == 0 );
    };

    _mqtt_state_recv_msg(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_recv == 1 );
    REQUIRE( call_push );

    state.elapsed_time_ms = (kii._keep_alive_interval + 1) * 1000;
    _mqtt_state_recv_ready(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_SEND_PINGREQ );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );

    call_send = 0;
    mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            REQUIRE( length == 2 );
            REQUIRE( buffer[0] == (char)0xC0 );
            REQUIRE( buffer[1] == 0x00 );
        }
        ++call_send;
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    _mqtt_state_send_pingreq(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_send == 1 );
    REQUIRE( state.elapsed_time_ms < kii._keep_alive_interval * 1000 );

    call_recv = 0;
    mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        switch (call_recv) {
            case 0: // fixed header - PingResp
                REQUIRE( length_to_read == 1);
                buffer[0] = 0xD0;
                *out_actual_length = 1;
                break;
            case 1: // fixed header - remaining length (= 0)
                REQUIRE( length_to_read == 1);
                buffer[0] = (char)0x00;
                *out_actual_length = 1;
                break;
            default:
                FAIL();
                break;
        }
        ++call_recv;
        return KHC_SOCK_OK;
    };

    _mqtt_state_recv_ready(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_recv == 2 );

    call_recv = 0;
    mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        switch (call_recv) {
            case 0: // fixed header - Other unknown
                REQUIRE( length_to_read == 1);
                buffer[0] = 0xF0;
                *out_actual_length = 1;
                break;
            case 1: // fixed header - remaining length (= 4)
                REQUIRE( length_to_read == 1);
                buffer[0] = (char)0x04;
                *out_actual_length = 1;
                break;
            case 2: // remaining: no more than the packet holds.
                REQUIRE( length_to_read == 4);
                buffer[0] = 'T';
                buffer[1] = 'E';
                buffer[2] = 'S';
                buffer[3] = 'T';
                *out_actual_length = 4;
                break;
            default:
                FAIL();
                break;
        }
        ++call_recv;
        return KHC_SOCK_OK;
    };

    _mqtt_state_recv_ready(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_recv == 3 );
}

// Serves data, then reports the end of the stream as khc_socket_callback.h
// has a callback report that the peer closed the connection: KHC_SOCK_OK with
// 0 bytes. A reader that does not stop there gets KHC_SOCK_FAIL from the
// 100th call on, so that it fails the test's count of calls instead of
// hanging the test.
static void recv_then_end_of_stream(khct::cb::SockCtx& ctx, const std::string& data, int& call_recv)
{
    size_t served = 0;
    ctx.on_recv = [data, served, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) mutable {
        ++call_recv;
        if (call_recv >= 100) {
            return KHC_SOCK_FAIL;
        }
        *out_actual_length = data.copy(buffer, length_to_read, served);
        served += *out_actual_length;
        return KHC_SOCK_OK;
    };
}

// Stands in for a platform's KII_CB_SOCK_RECV_TIMED_OUT: answers as told, and
// records how often it was asked and with which context.
struct RecvTimedOutMock {
    kii_bool_t timed_out;
    int calls;
    void* sock_ctx;
};
static RecvTimedOutMock recv_timed_out_mock;

static kii_bool_t mock_recv_timed_out(void* sock_ctx)
{
    ++recv_timed_out_mock.calls;
    recv_timed_out_mock.sock_ctx = sock_ctx;
    return recv_timed_out_mock.timed_out;
}

TEST_CASE( "MQTT state abnormal tests" ) {
    size_t kii_buff_size = 1024;
    char kii_buff[kii_buff_size];
    size_t mqtt_buff_size = 1024;
    char mqtt_buff[kii_buff_size];
    jkii_token_t jkii_tokens[256];
    jkii_resource_t jkii_resource = {jkii_tokens, 256};

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, kii_buff_size);
    kii_set_mqtt_buff(&kii, mqtt_buff, mqtt_buff_size);
    kii_set_json_parser_resource(&kii, &jkii_resource);
    kii._keep_alive_interval = 300;

    khct::cb::SockCtx http_ctx;
    kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
    kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
    kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
    kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);
    khct::cb::SockCtx mqtt_ctx;
    kii_set_cb_mqtt_sock_connect(&kii, khct::cb::mock_connect, &mqtt_ctx);
    kii_set_cb_mqtt_sock_send(&kii, khct::cb::mock_send, &mqtt_ctx);
    kii_set_cb_mqtt_sock_recv(&kii, khct::cb::mock_recv, &mqtt_ctx);
    kii_set_cb_mqtt_sock_close(&kii, khct::cb::mock_close, &mqtt_ctx);

    kii_set_cb_delay_ms(&kii, khct::cb::cb_delay_ms, NULL);

    khct::cb::PushCtx push_ctx;
    kii._cb_push_received = khct::cb::cb_push;
    kii._push_data = &push_ctx;

    mqtt_state_t state;

    _init_mqtt_state(&kii, &state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_INSTALL_PUSH );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );

    recv_timed_out_mock = RecvTimedOutMock();

    SECTION("install push try again") {
        int call_connect = 0;
        http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
            ++call_connect;
            REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
            REQUIRE( strlen(host) == strlen("api.kii.com") );
            REQUIRE( port == 443 );
            return KHC_SOCK_OK;
        };

        int call_send = 0;
        http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            if (call_send == 0) {
                const char req_line[] = "POST https://api.kii.com/api/apps/dummyAppID/installations HTTP/1.1\r\n";
                REQUIRE( length == strlen(req_line) );
                REQUIRE( strncmp(buffer, req_line, length) == 0 );
            }
            ++call_send;
            *out_sent_length = length;
            return KHC_SOCK_OK;
        };

        int call_recv = 0;
        std::stringstream ss;
        ss <<
            "HTTP/1.1 429 Too Many Requests\r\n"
            "\r\n";
        http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            *out_actual_length = ss.read(buffer, length_to_read).gcount();
            return KHC_SOCK_OK;
        };

        int call_close = 0;
        http_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return KHC_SOCK_OK;
        };

        _mqtt_state_install_push(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_INSTALL_PUSH );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_connect == 1 );
        REQUIRE( call_send > 1 );
        REQUIRE( call_recv == 2 );
        REQUIRE( call_close == 1 );
    }

    SECTION("install push error exit") {
        int call_connect = 0;
        http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
            ++call_connect;
            REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
            REQUIRE( strlen(host) == strlen("api.kii.com") );
            REQUIRE( port == 443 );
            return KHC_SOCK_OK;
        };

        int call_send = 0;
        http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            if (call_send == 0) {
                const char req_line[] = "POST https://api.kii.com/api/apps/dummyAppID/installations HTTP/1.1\r\n";
                REQUIRE( length == strlen(req_line) );
                REQUIRE( strncmp(buffer, req_line, length) == 0 );
            }
            ++call_send;
            *out_sent_length = length;
            return KHC_SOCK_OK;
        };

        int call_recv = 0;
        std::stringstream ss;
        ss <<
            "HTTP/1.1 403 Forbidden\r\n"
            "\r\n";
        http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            *out_actual_length = ss.read(buffer, length_to_read).gcount();
            return KHC_SOCK_OK;
        };

        int call_close = 0;
        http_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return KHC_SOCK_OK;
        };

        _mqtt_state_install_push(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
        REQUIRE( state.info.error == KII_MQTT_ERR_INSTALLATION );
        REQUIRE( call_connect == 1 );
        REQUIRE( call_send > 1 );
        REQUIRE( call_recv == 2 );
        REQUIRE( call_close == 1 );
    }

    SECTION("get endpoint try again") {
        int call_connect = 0;
        http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
            ++call_connect;
            REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
            REQUIRE( strlen(host) == strlen("api.kii.com") );
            REQUIRE( port == 443 );
            return KHC_SOCK_OK;
        };

        int call_send = 0;
        http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            if (call_send == 0) {
                const char req_line[] = "GET https://api.kii.com/api/apps/dummyAppID/installations/dummyInstallationID/mqtt-endpoint HTTP/1.1\r\n";
                REQUIRE( length == strlen(req_line) );
                REQUIRE( strncmp(buffer, req_line, length) == 0 );
            }
            ++call_send;
            *out_sent_length = length;
            return KHC_SOCK_OK;
        };

        int call_recv = 0;
        std::stringstream ss;
        ss <<
            "HTTP/1.1 429 Too Many Requests\r\n"
            "\r\n";
        http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            *out_actual_length = ss.read(buffer, length_to_read).gcount();
            return KHC_SOCK_OK;
        };

        int call_close = 0;
        http_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_GET_ENDPOINT;
        strcpy(state.ins_id.id, "dummyInstallationID");
        _mqtt_state_get_endpoint(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_connect == 1 );
        REQUIRE( call_send > 1 );
        REQUIRE( call_recv == 2 );
        REQUIRE( call_close == 1 );
    }

    SECTION("get endpoint error exit") {
        int call_connect = 0;
        http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
            ++call_connect;
            REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
            REQUIRE( strlen(host) == strlen("api.kii.com") );
            REQUIRE( port == 443 );
            return KHC_SOCK_OK;
        };

        int call_send = 0;
        http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            if (call_send == 0) {
                const char req_line[] = "GET https://api.kii.com/api/apps/dummyAppID/installations/dummyInstallationID/mqtt-endpoint HTTP/1.1\r\n";
                REQUIRE( length == strlen(req_line) );
                REQUIRE( strncmp(buffer, req_line, length) == 0 );
            }
            ++call_send;
            *out_sent_length = length;
            return KHC_SOCK_OK;
        };

        int call_recv = 0;
        std::stringstream ss;
        ss <<
            "HTTP/1.1 403 Forbidden\r\n"
            "\r\n";
        http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            *out_actual_length = ss.read(buffer, length_to_read).gcount();
            return KHC_SOCK_OK;
        };

        int call_close = 0;
        http_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_GET_ENDPOINT;
        strcpy(state.ins_id.id, "dummyInstallationID");
        _mqtt_state_get_endpoint(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
        REQUIRE( state.info.error == KII_MQTT_ERR_GET_ENDPOINT );
        REQUIRE( call_connect == 1 );
        REQUIRE( call_send > 1 );
        REQUIRE( call_recv == 2 );
        REQUIRE( call_close == 1 );
    }

    SECTION("sock connect got KHC_SOCK_AGAIN") {
        int call_connect = 0;
        mqtt_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
            const char* exp_host = "jp-mqtt-dummy.kii.com";
            ++call_connect;
            REQUIRE( strlen(host) == strlen(exp_host) );
            REQUIRE( strncmp(host, exp_host, strlen(exp_host)) == 0 );
            REQUIRE( port == 8883 );
            return KHC_SOCK_AGAIN;
        };

        state.info.task_state = KII_MQTT_ST_SOCK_CONNECT;
        state.endpoint.port_ssl = 8883;
        state.endpoint.port_tcp = 1883;
        strcpy(state.endpoint.host, "jp-mqtt-dummy.kii.com");
        _mqtt_state_sock_connect(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_connect == 1 );
    }

    SECTION("sock connect got KHC_SOCK_FAIL") {
        int call_connect = 0;
        mqtt_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
            const char* exp_host = "jp-mqtt-dummy.kii.com";
            ++call_connect;
            REQUIRE( strlen(host) == strlen(exp_host) );
            REQUIRE( strncmp(host, exp_host, strlen(exp_host)) == 0 );
            REQUIRE( port == 8883 );
            return KHC_SOCK_FAIL;
        };

        state.info.task_state = KII_MQTT_ST_SOCK_CONNECT;
        state.endpoint.port_ssl = 8883;
        state.endpoint.port_tcp = 1883;
        strcpy(state.endpoint.host, "jp-mqtt-dummy.kii.com");
        _mqtt_state_sock_connect(&state);
        // Note: retry connect when connect callback return fail.
        REQUIRE( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_connect == 1 );
    }

    SECTION("send connect no buffer") {
        kii_set_mqtt_buff(&kii, NULL, 0);
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        strcpy(state.endpoint.topic, "dummyTopic");
        strcpy(state.endpoint.username, "dummyUser");
        strcpy(state.endpoint.password, "dummyPassword");
        _mqtt_state_send_connect(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
        REQUIRE( state.info.error == KII_MQTT_ERR_INSUFFICIENT_BUFF );
    }

    SECTION("send connect small buffer") {
        char sbuff[27];
        kii_set_mqtt_buff(&kii, sbuff, 27);
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        strcpy(state.endpoint.topic, "dummyTopic");
        strcpy(state.endpoint.username, "dummyUser");
        strcpy(state.endpoint.password, "dummyPassword");
        _mqtt_state_send_connect(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
        REQUIRE( state.info.error == KII_MQTT_ERR_INSUFFICIENT_BUFF );
    }

    SECTION("send connect got KHC_SOCK_AGAIN") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            return KHC_SOCK_AGAIN;
        };

        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        strcpy(state.endpoint.topic, "dummyTopic");
        strcpy(state.endpoint.username, "dummyUser");
        strcpy(state.endpoint.password, "dummyPassword");
        _mqtt_state_send_connect(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_send == 1 );
    }

    SECTION("send connect got KHC_SOCK_FAIL") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            return KHC_SOCK_FAIL;
        };

        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        strcpy(state.endpoint.topic, "dummyTopic");
        strcpy(state.endpoint.username, "dummyUser");
        strcpy(state.endpoint.password, "dummyPassword");
        _mqtt_state_send_connect(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_send == 1 );
    }

    SECTION("recv connack got KHC_SOCK_AGAIN 1") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    return KHC_SOCK_AGAIN;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 0 );
    }

    SECTION("recv connack got KHC_SOCK_FAIL 1") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    return KHC_SOCK_FAIL;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 0 );
    }

    SECTION("recv connack got KHC_SOCK_AGAIN 2") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x20;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 2)
                    return KHC_SOCK_AGAIN;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv connack got KHC_SOCK_FAIL 2") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x20;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 2)
                    return KHC_SOCK_FAIL;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv connack got KHC_SOCK_AGAIN 3") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x20;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x02;
                    *out_actual_length = 1;
                    break;
                case 2: // variable header (2 bytes)
                    return KHC_SOCK_AGAIN;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 2 );
    }

    SECTION("recv connack got KHC_SOCK_FAIL 3") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x20;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x02;
                    *out_actual_length = 1;
                    break;
                case 2: // variable header (2 bytes)
                    return KHC_SOCK_FAIL;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 2 );
    }

    SECTION("recv connack no CONNACK type") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x90; // type SUBACK.
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x02;
                    *out_actual_length = 1;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 2 );
    }

    SECTION("recv connack receive Failure.") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x20;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x02;
                    *out_actual_length = 1;
                    break;
                case 2: // variable header (2 bytes)
                    REQUIRE( length_to_read == 2);
                    buffer[0] = 0x00;
                    buffer[1] = 0x01; // return code(Connection Refused).
                    *out_actual_length = 2;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 3 );
    }

    // Return codes 4 and 5 refuse the credentials GET_ENDPOINT fetched.
    // RECONNECT would present the same ones again and be refused forever.
    SECTION("recv connack bad user name or password gets a new endpoint") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x20;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x02;
                    *out_actual_length = 1;
                    break;
                case 2: // variable header (2 bytes)
                    REQUIRE( length_to_read == 2);
                    buffer[0] = 0x00;
                    buffer[1] = 0x04; // return code(Bad user name or password).
                    *out_actual_length = 2;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };
        int call_close = 0;
        mqtt_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 3 );
        REQUIRE( call_close == 1 );
    }

    SECTION("recv connack not authorized gets a new endpoint") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x20;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x02;
                    *out_actual_length = 1;
                    break;
                case 2: // variable header (2 bytes)
                    REQUIRE( length_to_read == 2);
                    buffer[0] = 0x00;
                    buffer[1] = 0x05; // return code(Not authorized).
                    *out_actual_length = 2;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };
        int call_close = 0;
        mqtt_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 3 );
        REQUIRE( call_close == 1 );
    }

    // Any other refusal says nothing against the credentials, so they are
    // kept and RECONNECT closes the socket.
    SECTION("recv connack server unavailable reconnects") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x20;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x02;
                    *out_actual_length = 1;
                    break;
                case 2: // variable header (2 bytes)
                    REQUIRE( length_to_read == 2);
                    buffer[0] = 0x00;
                    buffer[1] = 0x03; // return code(Server unavailable).
                    *out_actual_length = 2;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };
        int call_close = 0;
        mqtt_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 3 );
        REQUIRE( call_close == 0 );
    }

    SECTION("send subscribe got KHC_SOCK_AGAIN") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            return KHC_SOCK_AGAIN;
        };

        state.info.task_state = KII_MQTT_ST_SEND_SUBSCRIBE;
        _mqtt_state_send_subscribe(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_send == 1 );
    }

    SECTION("send subscribe got KHC_SOCK_FAIL") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            return KHC_SOCK_FAIL;
        };

        state.info.task_state = KII_MQTT_ST_SEND_SUBSCRIBE;
        _mqtt_state_send_subscribe(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_send == 1 );
    }

    SECTION("recv suback got KHC_SOCK_AGAIN 1") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(9)
                    return KHC_SOCK_AGAIN;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 0 );
    }

    SECTION("recv suback got KHC_SOCK_FAIL 1") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(9)
                    return KHC_SOCK_FAIL;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 0 );
    }

    SECTION("recv suback got KHC_SOCK_AGAIN 2") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(9)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = (char)0x90;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 3)
                    return KHC_SOCK_AGAIN;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv suback got KHC_SOCK_FAIL 2") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(9)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = (char)0x90;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 3)
                    return KHC_SOCK_FAIL;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv suback got KHC_SOCK_AGAIN 3") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(9)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = (char)0x90;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 3)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x03;
                    *out_actual_length = 1;
                    break;
                case 2: // variable header (3 bytes)
                    return KHC_SOCK_AGAIN;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 2 );
    }

    SECTION("recv suback got KHC_SOCK_FAIL 3") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(9)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = (char)0x90;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 3)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x03;
                    *out_actual_length = 1;
                    break;
                case 2: // variable header (3 bytes)
                    return KHC_SOCK_FAIL;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 2 );
    }

    SECTION("recv suback no SUBACK type") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(9)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = (char)0x20;// CONNACK.
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 3)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x03;
                    *out_actual_length = 1;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 2 );
    }

    SECTION("recv suback receive Failure") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(9)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = (char)0x90;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 3)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x03;
                    *out_actual_length = 1;
                    break;
                case 2: // variable header (3 bytes)
                    REQUIRE( length_to_read == 3);
                    buffer[0] = 0x00; // identifier MSB.
                    buffer[1] = 0x01; // identifier LSB.
                    buffer[2] = 0x80; // payload(return code: Failure)
                    *out_actual_length = 3;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 3 );
    }

    SECTION("recv ready got KHC_SOCK_AGAIN 1") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - Publish
                    return KHC_SOCK_AGAIN;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 0 );
    }

    // With no callback set by kii_set_cb_mqtt_sock_recv_timed_out(), a receive
    // that fails before any of a packet has arrived is taken for the receive
    // timeout running out, as it always was.
    SECTION("recv ready got KHC_SOCK_FAIL 1") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - Publish
                    return KHC_SOCK_FAIL;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 0 );
    }

    // In this section and the next, part of the fixed header has been read.
    // Waiting on used to take the rest of the packet for the next one's fixed
    // header.
    SECTION("recv ready got KHC_SOCK_AGAIN 2") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - Publish
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x30;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length[0]
                    return KHC_SOCK_AGAIN;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv ready got KHC_SOCK_FAIL 2") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - Publish
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x30;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length[0]
                    return KHC_SOCK_FAIL;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    // The recv callback reports a receive timeout and a lost connection alike
    // as KHC_SOCK_FAIL. Without the timed-out callback, recv ready can only
    // take it for a timeout, so with keep-alive 0, which sends no PINGREQ to
    // find out, it waits on a lost connection forever.
    SECTION("recv ready without the timed-out callback waits on a failed receive, even with keep-alive 0") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [&call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return KHC_SOCK_FAIL;
        };

        kii._keep_alive_interval = 0;
        kii_set_mqtt_to_sock_recv(&kii, 15);
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        state.elapsed_time_ms = 1000;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( state.elapsed_time_ms == 1000 + 15000 + 1000 );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv ready waits on when the timed-out callback says the receive timed out") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [&call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return KHC_SOCK_FAIL;
        };
        kii_set_cb_mqtt_sock_recv_timed_out(&kii, mock_recv_timed_out, &mqtt_ctx);
        recv_timed_out_mock.timed_out = KII_TRUE;

        kii_set_mqtt_to_sock_recv(&kii, 15);
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        state.elapsed_time_ms = 1000;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( state.elapsed_time_ms == 1000 + 15000 + 1000 );
        REQUIRE( call_recv == 1 );
        REQUIRE( recv_timed_out_mock.calls == 1 );
    }

    SECTION("recv ready reconnects when the timed-out callback says the receive did not time out") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [&call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return KHC_SOCK_FAIL;
        };
        kii_set_cb_mqtt_sock_recv_timed_out(&kii, mock_recv_timed_out, &mqtt_ctx);
        recv_timed_out_mock.timed_out = KII_FALSE;

        // With keep-alive 0 this is the only way the loss is noticed.
        for (unsigned int keep_alive : { 300u, 0u }) {
            INFO("keep-alive " << keep_alive);
            kii._keep_alive_interval = keep_alive;
            call_recv = 0;
            recv_timed_out_mock.calls = 0;
            state.info.task_state = KII_MQTT_ST_RECV_READY;
            state.elapsed_time_ms = 1000;
            _mqtt_state_recv_ready(&state);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( state.info.error == KII_MQTT_ERR_OK );
            CHECK( state.elapsed_time_ms == 0 );
            CHECK( call_recv == 1 );
            CHECK( recv_timed_out_mock.calls == 1 );
        }
    }

    SECTION("recv ready asks the timed-out callback with the context it was given") {
        mqtt_ctx.on_recv = [](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            return KHC_SOCK_FAIL;
        };
        kii_set_cb_mqtt_sock_recv_timed_out(&kii, mock_recv_timed_out, &mqtt_ctx);
        recv_timed_out_mock.timed_out = KII_TRUE;

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( recv_timed_out_mock.calls == 1 );
        REQUIRE( recv_timed_out_mock.sock_ctx == &mqtt_ctx );
    }

    // khc reads the HTTP socket and has no use for it.
    SECTION("the timed-out callback is not asked about the HTTP socket") {
        http_ctx.on_connect = [](void* socket_context, const char* host, unsigned int port) {
            return KHC_SOCK_OK;
        };
        http_ctx.on_send = [](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            *out_sent_length = length;
            return KHC_SOCK_OK;
        };
        int call_recv = 0;
        http_ctx.on_recv = [&call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return KHC_SOCK_FAIL;
        };
        http_ctx.on_close = [](void* socket_context) {
            return KHC_SOCK_OK;
        };
        kii_set_cb_mqtt_sock_recv_timed_out(&kii, mock_recv_timed_out, &mqtt_ctx);

        _mqtt_state_install_push(&state);
        REQUIRE( call_recv == 1 );
        REQUIRE( recv_timed_out_mock.calls == 0 );
    }

    // Waiting for CONNACK, as for SUBACK, a receive that fails or times out
    // means the broker did not answer, and either way the task reconnects.
    SECTION("recv connack reconnects on a failed receive without asking the timed-out callback") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [&call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return KHC_SOCK_FAIL;
        };
        kii_set_cb_mqtt_sock_recv_timed_out(&kii, mock_recv_timed_out, &mqtt_ctx);
        recv_timed_out_mock.timed_out = KII_TRUE;

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( call_recv == 1 );
        REQUIRE( recv_timed_out_mock.calls == 0 );
    }

    // Non-blocking sockets are not supported, but KHC_SOCK_AGAIN says only that
    // nothing has arrived yet, so recv ready waits on as it always did.
    SECTION("recv ready waits on KHC_SOCK_AGAIN without asking the timed-out callback") {
        mqtt_ctx.on_recv = [](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            return KHC_SOCK_AGAIN;
        };
        kii_set_cb_mqtt_sock_recv_timed_out(&kii, mock_recv_timed_out, &mqtt_ctx);
        recv_timed_out_mock.timed_out = KII_FALSE;

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( recv_timed_out_mock.calls == 0 );
    }

    // Once part of a packet has been read, the next read would start inside
    // it, so the task reconnects whether the receive timed out or not.
    SECTION("recv ready reconnects without asking the timed-out callback when the fixed header is cut off") {
        const khc_sock_code_t codes[] = { KHC_SOCK_FAIL, KHC_SOCK_AGAIN };
        for (khc_sock_code_t code : codes) {
            INFO("recv returns " << code << " for the remaining length");
            int call_recv = 0;
            mqtt_ctx.on_recv = [code, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
                if (call_recv++ == 0) { // fixed header - PUBLISH
                    buffer[0] = 0x30;
                    *out_actual_length = 1;
                    return KHC_SOCK_OK;
                }
                return code;
            };
            kii_set_cb_mqtt_sock_recv_timed_out(&kii, mock_recv_timed_out, &mqtt_ctx);
            recv_timed_out_mock.timed_out = KII_TRUE;
            recv_timed_out_mock.calls = 0;

            state.info.task_state = KII_MQTT_ST_RECV_READY;
            state.elapsed_time_ms = 1000;
            _mqtt_state_recv_ready(&state);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( state.info.error == KII_MQTT_ERR_OK );
            CHECK( state.elapsed_time_ms == 0 );
            CHECK( call_recv == 2 );
            CHECK( recv_timed_out_mock.calls == 0 );
        }
    }

    // The end of the stream is no failure the callback could call a timeout.
    SECTION("recv ready reconnects at the end of the stream without asking the timed-out callback") {
        int call_recv = 0;
        recv_then_end_of_stream(mqtt_ctx, "", call_recv);
        kii_set_cb_mqtt_sock_recv_timed_out(&kii, mock_recv_timed_out, &mqtt_ctx);
        recv_timed_out_mock.timed_out = KII_TRUE;

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        state.elapsed_time_ms = 1000;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( state.elapsed_time_ms == 0 );
        REQUIRE( recv_timed_out_mock.calls == 0 );
    }

    SECTION("recv msg got KHC_SOCK_AGAIN from _mqtt_recv_remaining_trash") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return KHC_SOCK_AGAIN;
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = state.kii->_mqtt_buffer_size + 1;
        _mqtt_state_recv_msg(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv msg got KHC_SOCK_FAIL from _mqtt_recv_remaining_trash") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return KHC_SOCK_FAIL;
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = state.kii->_mqtt_buffer_size + 1;
        _mqtt_state_recv_msg(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv msg got KHC_SOCK_AGAIN from _mqtt_recv_remaining") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return KHC_SOCK_AGAIN;
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = state.kii->_mqtt_buffer_size - 1;
        _mqtt_state_recv_msg(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv msg got KHC_SOCK_FAIL from _mqtt_recv_remaining") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return KHC_SOCK_FAIL;
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = state.kii->_mqtt_buffer_size - 1;
        _mqtt_state_recv_msg(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    // The topic length is the first two bytes of the message, so it can claim
    // more than the message holds. Subtracting it from the message size then
    // wrapped around and the callback was handed a length near SIZE_MAX.
    SECTION("recv msg drops a message whose topic is longer than the message") {
        const std::string packet("\x00\x10" "abc", 5); // topic length 16
        size_t served = 0;
        mqtt_ctx.on_recv = [=, &served](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            *out_actual_length = packet.copy(buffer, length_to_read, served);
            served += *out_actual_length;
            return KHC_SOCK_OK;
        };
        bool call_push = false;
        size_t push_length = 0;
        push_ctx.on_push = [&call_push, &push_length](const char* message, size_t message_length) {
            call_push = true;
            push_length = message_length;
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = packet.length();
        _mqtt_state_recv_msg(&state);
        CAPTURE( push_length );
        REQUIRE( call_push == false );
        // Read whole, so the next message starts where it should.
        REQUIRE( served == packet.length() );
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    }

    SECTION("recv msg drops a one-byte message, too short for a topic length") {
        const std::string packet("\x00", 1);
        size_t served = 0;
        mqtt_ctx.on_recv = [=, &served](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            *out_actual_length = packet.copy(buffer, length_to_read, served);
            served += *out_actual_length;
            return KHC_SOCK_OK;
        };
        bool call_push = false;
        size_t push_length = 0;
        push_ctx.on_push = [&call_push, &push_length](const char* message, size_t message_length) {
            call_push = true;
            push_length = message_length;
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = packet.length();
        _mqtt_state_recv_msg(&state);
        CAPTURE( push_length );
        REQUIRE( call_push == false );
        REQUIRE( served == packet.length() );
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    }

    // Nothing to read, which _mqtt_recv_remaining() reports as a failure.
    SECTION("recv msg reconnects on an empty message") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [&call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return KHC_SOCK_FAIL;
        };
        bool call_push = false;
        push_ctx.on_push = [&call_push](const char* message, size_t message_length) {
            call_push = true;
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = 0;
        _mqtt_state_recv_msg(&state);
        REQUIRE( call_push == false );
        REQUIRE( call_recv == 0 );
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    }

    // The boundary: a topic that exactly fills the message is valid.
    SECTION("recv msg passes an empty payload when the topic fills the message") {
        const std::string packet("\x00\x03" "abc", 5);
        size_t served = 0;
        mqtt_ctx.on_recv = [=, &served](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            *out_actual_length = packet.copy(buffer, length_to_read, served);
            served += *out_actual_length;
            return KHC_SOCK_OK;
        };
        bool call_push = false;
        const char* mqtt_buffer = state.kii->_mqtt_buffer;
        push_ctx.on_push = [=, &call_push](const char* message, size_t message_length) {
            call_push = true;
            REQUIRE( message - mqtt_buffer == 5 );
            REQUIRE( message_length == 0 );
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = packet.length();
        _mqtt_state_recv_msg(&state);
        REQUIRE( call_push );
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
    }

    // char is signed on x86, among others: reading the length bytes as char
    // turned 0xC8 into 0xFFFFFFC8, so a topic of 128 bytes or more got the
    // wrong length.
    SECTION("recv msg reads a topic length byte of 0x80 or more as unsigned") {
        std::string packet("\x00\xC8", 2); // topic length 200
        packet.append(200, 't');
        packet.append("{}");
        size_t served = 0;
        mqtt_ctx.on_recv = [=, &served](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            *out_actual_length = packet.copy(buffer, length_to_read, served);
            served += *out_actual_length;
            return KHC_SOCK_OK;
        };
        bool call_push = false;
        const char* mqtt_buffer = state.kii->_mqtt_buffer;
        push_ctx.on_push = [=, &call_push](const char* message, size_t message_length) {
            call_push = true;
            // As an offset: Catch would print a char* by reading through it.
            REQUIRE( message - mqtt_buffer == 2 + 200 );
            REQUIRE( message_length == 2 );
            REQUIRE( strncmp(message, "{}", 2) == 0 );
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = packet.length();
        _mqtt_state_recv_msg(&state);
        REQUIRE( call_push );
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
    }

    // recv on a stream socket returns whatever is there, up to the size asked
    // for. Discarding a packet asked for 256 bytes at a time however little of
    // it was left, so when the next packet had already arrived it took the
    // start of that one too, and then read the middle of it as a fixed header.
    SECTION("recv ready discards an ignored packet without reading into the next") {
        const std::string payload(300, 'p');
        // PUBACK: remaining length 2, the packet identifier.
        const std::string stream = std::string("\x40\x02\x00\x01", 4) + publish_packet("topic", payload);
        size_t served = 0;
        serve_stream(mqtt_ctx, stream, served);
        int call_push = 0;
        push_ctx.on_push = [=, &call_push](const char* message, size_t message_length) {
            ++call_push;
            REQUIRE( std::string(message, message_length) == payload );
        };

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        CHECK( served == 4 );
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );

        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        REQUIRE( state.remaining_message_size == 2 + 5 + payload.length() );

        _mqtt_state_recv_msg(&state);
        REQUIRE( call_push == 1 );
        REQUIRE( served == stream.length() );
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    }

    // The same for a PUBLISH too large for the MQTT buffer: its last part is
    // shorter than 256 bytes.
    SECTION("recv msg discards a message larger than the buffer without reading into the next") {
        const std::string large = publish_packet("topic", std::string(state.kii->_mqtt_buffer_size, 'x'));
        const std::string payload(300, 'p');
        const std::string stream = large + publish_packet("topic", payload);
        size_t served = 0;
        serve_stream(mqtt_ctx, stream, served);
        int call_push = 0;
        push_ctx.on_push = [=, &call_push](const char* message, size_t message_length) {
            ++call_push;
            REQUIRE( std::string(message, message_length) == payload );
        };

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        REQUIRE( state.remaining_message_size > state.kii->_mqtt_buffer_size );

        _mqtt_state_recv_msg(&state);
        CHECK( served == large.length() );
        REQUIRE( call_push == 0 );
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );

        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        REQUIRE( state.remaining_message_size == 2 + 5 + payload.length() );

        _mqtt_state_recv_msg(&state);
        REQUIRE( call_push == 1 );
        REQUIRE( served == stream.length() );
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    }

    SECTION("send pingreq got KHC_SOCK_AGAIN") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            return KHC_SOCK_AGAIN;
        };

        state.info.task_state = KII_MQTT_ST_SEND_PINGREQ;
        _mqtt_state_send_pingreq(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_send == 1 );
        REQUIRE( state.elapsed_time_ms == 0 );
    }

    SECTION("send pingreq got KHC_SOCK_FAIL") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            return KHC_SOCK_FAIL;
        };

        state.info.task_state = KII_MQTT_ST_SEND_PINGREQ;
        _mqtt_state_send_pingreq(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_send == 1 );
        REQUIRE( state.elapsed_time_ms == 0 );
    }

    SECTION("sending pingreq marks the pingresp outstanding") {
        mqtt_ctx.on_send = [=](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            *out_sent_length = length;
            return KHC_SOCK_OK;
        };

        REQUIRE( state.pingresp_received == KII_TRUE );
        state.info.task_state = KII_MQTT_ST_SEND_PINGREQ;
        _mqtt_state_send_pingreq(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.pingresp_received == KII_FALSE );
    }

    SECTION("receiving pingresp clears the outstanding pingresp") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - PINGRESP
                    buffer[0] = (char)0xD0;
                    *out_actual_length = 1;
                    break;
                default: // remaining length (= 0)
                    buffer[0] = 0x00;
                    *out_actual_length = 1;
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        state.pingresp_received = KII_FALSE;
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.pingresp_received == KII_TRUE );
    }

    // The point of the flag: a half-open connection can keep accepting sends
    // for many retransmission timeouts, so waiting for a send error to fail is
    // slow. An unanswered PINGREQ reconnects within one keep-alive interval.
    SECTION("an unanswered pingreq reconnects without sending another") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            *out_sent_length = length;
            return KHC_SOCK_OK; // the socket still looks healthy
        };

        state.pingresp_received = KII_FALSE;
        state.info.task_state = KII_MQTT_ST_SEND_PINGREQ;
        _mqtt_state_send_pingreq(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_send == 0 );
        REQUIRE( state.elapsed_time_ms == 0 );
    }

    // _init_mqtt_state() runs once, before the state loop, and RECONNECT goes
    // to SOCK_CONNECT inside it. If the flag were not cleared when the new
    // connection is established, the first PINGREQ after any reconnect would
    // see the previous connection's KII_FALSE and reconnect again, forever.
    SECTION("a reconnect does not leave the flag set for the new connection") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - CONNACK
                    buffer[0] = (char)0x20;
                    *out_actual_length = 1;
                    break;
                case 1: // remaining length (= 2)
                    buffer[0] = 0x02;
                    *out_actual_length = 1;
                    break;
                default: // variable header: session present + return code 0
                    buffer[0] = 0x00;
                    buffer[1] = 0x00;
                    *out_actual_length = 2;
                    break;
            }
            ++call_recv;
            return KHC_SOCK_OK;
        };

        // As left behind by the connection that died with a PINGREQ in flight.
        state.pingresp_received = KII_FALSE;
        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_SEND_SUBSCRIBE );
        REQUIRE( state.pingresp_received == KII_TRUE );

        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            *out_sent_length = length;
            return KHC_SOCK_OK;
        };
        state.info.task_state = KII_MQTT_ST_SEND_PINGREQ;
        _mqtt_state_send_pingreq(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( call_send == 1 );
    }

    // When the broker closed the connection partway through a packet, reading
    // the rest of it took each 0-byte read for no progress yet and called recv
    // again, forever: the task spun without a delay, never reconnecting and
    // never asking the continue callback whether to stop.
    SECTION("recv connack reconnects when the stream ends in the fixed header") {
        int call_recv = 0;
        recv_then_end_of_stream(mqtt_ctx, "\x20", call_recv);

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        // The type, the remaining length, which the end reads as 0, and the
        // variable header, which is where the end is noticed.
        REQUIRE( call_recv == 3 );
    }

    SECTION("recv msg reconnects when the stream ends partway through a message") {
        int call_recv = 0;
        // Topic length 5, the topic, and 3 of the 10 bytes of the body.
        recv_then_end_of_stream(mqtt_ctx, std::string("\x00\x05" "topic" "abc", 10), call_recv);
        bool call_push = false;
        push_ctx.on_push = [&call_push](const char* message, size_t message_length) {
            call_push = true;
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = 17;
        state.elapsed_time_ms = 1000;
        _mqtt_state_recv_msg(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( state.elapsed_time_ms == 0 );
        REQUIRE( call_push == false );
        REQUIRE( call_recv == 2 );
    }

    SECTION("recv msg reconnects when the stream ends partway through a message it discards") {
        int call_recv = 0;
        // Read 256 bytes at a time, so two reads, then the end.
        recv_then_end_of_stream(mqtt_ctx, std::string(300, 'x'), call_recv);

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = state.kii->_mqtt_buffer_size + 1;
        state.elapsed_time_ms = 1000;
        _mqtt_state_recv_msg(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( state.elapsed_time_ms == 0 );
        REQUIRE( call_recv == 3 );
    }

    SECTION("recv ready reconnects when the stream ends partway through a packet it ignores") {
        int call_recv = 0;
        // PUBACK, remaining length 2, and 1 of those 2 bytes.
        recv_then_end_of_stream(mqtt_ctx, std::string("\x40\x02\x00", 3), call_recv);

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        state.elapsed_time_ms = 1000;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( state.elapsed_time_ms == 0 );
        REQUIRE( call_recv == 4 );
    }

    // Between packets the end reads as a header of type 0 and length 0, and
    // recv ready discards that packet, which with nothing to read reports a
    // failure. So this already reconnected. Were the fixed header to report the
    // end as a failure instead, recv ready would take it for a receive timeout
    // and wait on until the keep-alive interval ran out.
    SECTION("recv ready reconnects when the stream ends between packets") {
        int call_recv = 0;
        recv_then_end_of_stream(mqtt_ctx, "", call_recv);

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        state.elapsed_time_ms = 1000;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( state.elapsed_time_ms == 0 );
        REQUIRE( call_recv == 2 );
    }
}

TEST_CASE( "kii_enable_insecure_mqtt test" ) {
    size_t kii_buff_size = 1024;
    char kii_buff[kii_buff_size];
    size_t mqtt_buff_size = 1024;
    char mqtt_buff[kii_buff_size];
    jkii_token_t jkii_tokens[256];
    jkii_resource_t jkii_resource = {jkii_tokens, 256};

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, kii_buff_size);
    kii_set_mqtt_buff(&kii, mqtt_buff, mqtt_buff_size);
    kii_set_json_parser_resource(&kii, &jkii_resource);
    kii._keep_alive_interval = 300;

    khct::cb::SockCtx http_ctx;
    kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
    kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
    kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
    kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);
    khct::cb::SockCtx mqtt_ctx;
    kii_set_cb_mqtt_sock_connect(&kii, khct::cb::mock_connect, &mqtt_ctx);
    kii_set_cb_mqtt_sock_send(&kii, khct::cb::mock_send, &mqtt_ctx);
    kii_set_cb_mqtt_sock_recv(&kii, khct::cb::mock_recv, &mqtt_ctx);
    kii_set_cb_mqtt_sock_close(&kii, khct::cb::mock_close, &mqtt_ctx);

    kii_set_cb_delay_ms(&kii, khct::cb::cb_delay_ms, NULL);

    khct::cb::PushCtx push_ctx;
    kii._cb_push_received = khct::cb::cb_push;
    kii._push_data = &push_ctx;

    REQUIRE(kii._khc._enable_insecure == 0);
    REQUIRE(kii._insecure_mqtt == KII_FALSE);
    kii_enable_insecure_mqtt(&kii, KII_TRUE);
    REQUIRE(kii._khc._enable_insecure == 0);
    REQUIRE(kii._insecure_mqtt == KII_TRUE);

    mqtt_state_t state;

    _init_mqtt_state(&kii, &state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_INSTALL_PUSH );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );

    int call_connect = 0;
    http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
        ++call_connect;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return KHC_SOCK_OK;
    };

    int call_send = 0;
    http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            const char req_line[] = "POST https://api.kii.com/api/apps/dummyAppID/installations HTTP/1.1\r\n";
            REQUIRE( length == strlen(req_line) );
            REQUIRE( strncmp(buffer, req_line, length) == 0 );
        }
        ++call_send;
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    int call_recv = 0;
    std::stringstream ss;
    ss <<
        "HTTP/1.1 201 Created\r\n"
        "Accept-Ranges: bytes\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Expose-Headers: Content-Type, Authorization, Content-Length, X-Requested-With, ETag, X-Step-Count, X-Environment-version, X-HTTP-Status-Code\r\n"
        "Age: 0\r\n"
        "Cache-Control: max-age=0, no-cache, no-store\r\n"
        "Content-Type: application/vnd.kii.InstallationCreationResponse+json;charset=UTF-8\r\n"
        "Date: Tue, 04 Dec 2018 06:41:17 GMT\r\n"
        "Location: https://api-jp.kii.com/api/apps/dummyAppID/installations/5t842kt0a4d5bvo3g61i97ft6\r\n"
        "Server: openresty\r\n"
        "X-HTTP-Status-Code: 201\r\n"
        "Content-Length: 116\r\n"
        "Connection: Close\r\n"
        "\r\n"
        "{"
        "  \"installationID\" : \"dummyInstallationID\","
        "  \"installationRegistrationID\" : \"56f6bcf7-3b1e-49c0-b625-64f810fe85a0\""
        "}";
    http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++call_recv;
        *out_actual_length = ss.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    int call_close = 0;
    http_ctx.on_close = [=, &call_close](void* socket_ctx) {
        ++call_close;
        return KHC_SOCK_OK;
    };

    _mqtt_state_install_push(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( strcmp(state.ins_id.id, "dummyInstallationID") == 0 );
    REQUIRE( call_connect == 1 );
    REQUIRE( call_send > 1 );
    REQUIRE( call_recv >= 1 );
    REQUIRE( call_close == 1 );

    call_connect = 0;
    http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
        ++call_connect;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return KHC_SOCK_OK;
    };

    call_send = 0;
    http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            const char req_line[] = "GET https://api.kii.com/api/apps/dummyAppID/installations/dummyInstallationID/mqtt-endpoint HTTP/1.1\r\n";
            CHECK( length == strlen(req_line) );
            CHECK( strncmp(buffer, req_line, length) == 0 );
        }
        ++call_send;
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    call_recv = 0;
    ss.clear();
    ss <<
        "HTTP/1.1 200 OK\r\n"
        "Accept-Ranges: bytes\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Expose-Headers: Content-Type, Authorization, Content-Length, X-Requested-With, ETag, X-Step-Count, X-Environment-version, X-HTTP-Status-Code\r\n"
        "Age: 0\r\n"
        "Cache-Control: max-age=0, no-cache, no-store\r\n"
        "Content-Type: application/vnd.kii.MQTTEndpointResponse+json;charset=UTF-8\r\n"
        "Date: Tue, 04 Dec 2018 08:02:07 GMT\r\n"
        "Server: openresty\r\n"
        "X-HTTP-Status-Code: 200\r\n"
        "X-MQTT-TTL: 2147483647\r\n"
        "Content-Length: 273\r\n"
        "Connection: Close\r\n"
        "\r\n"
        "{"
        "  \"installationID\" : \"dummyInstallationID_2\","
        "  \"username\" : \"dummyUser\","
        "  \"password\" : \"dummyPassword\","
        "  \"mqttTopic\" : \"dummyTopic\","
        "  \"host\" : \"jp-mqtt-dummy.kii.com\","
        "  \"portTCP\" : 1883,"
        "  \"portSSL\" : 8883,"
        "  \"portWS\" : 12470,"
        "  \"portWSS\" : 12473,"
        "  \"X-MQTT-TTL\" : 2147483647"
        "}";
    http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++call_recv;
        *out_actual_length = ss.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    call_close = 0;
    http_ctx.on_close = [=, &call_close](void* socket_ctx) {
        ++call_close;
        return KHC_SOCK_OK;
    };

    _mqtt_state_get_endpoint(&state);
    CHECK( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
    CHECK( state.info.error == KII_MQTT_ERR_OK );
    CHECK( strcmp(state.endpoint.username, "dummyUser") == 0 );
    CHECK( strcmp(state.endpoint.password, "dummyPassword") == 0 );
    CHECK( strcmp(state.endpoint.topic, "dummyTopic") == 0 );
    CHECK( strcmp(state.endpoint.host, "jp-mqtt-dummy.kii.com") == 0 );
    CHECK( state.endpoint.port_tcp == 1883 );
    CHECK( state.endpoint.port_ssl == 8883 );
    CHECK( state.endpoint.ttl == 2147483647 );
    CHECK( call_connect == 1 );
    CHECK( call_send > 1 );
    CHECK( call_recv >= 1 );
    CHECK( call_close == 1 );

    call_connect = 0;
    mqtt_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
        const char* exp_host = "jp-mqtt-dummy.kii.com";
        ++call_connect;
        REQUIRE( strlen(host) == strlen(exp_host) );
        REQUIRE( strncmp(host, exp_host, strlen(exp_host)) == 0 );
        REQUIRE( port == 1883 );
        return KHC_SOCK_OK;
    };

    _mqtt_state_sock_connect(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_SEND_CONNECT );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_connect == 1 );
}

// Stands in for the platform's task creation: records the call and returns
// the result the test chose.
struct TaskCreateCtx {
    kii_task_code_t result;
    int calls;
    std::string name;
    void* entry_param;
};

static kii_task_code_t mock_task_create(const char* name, KII_TASK_ENTRY entry, void* entry_param, void* userdata)
{
    TaskCreateCtx* ctx = (TaskCreateCtx*)userdata;
    ++ctx->calls;
    ctx->name = name;
    ctx->entry_param = entry_param;
    return ctx->result;
}

// The task-create callback is what starts the MQTT task, so its result is
// whether push is running. kii_start_push_task() ignored it and returned
// KII_ERR_OK even when no task had been created, and called through the
// callback without checking that one had been set.
TEST_CASE( "kii_start_push_task reports a task it could not create" ) {
    kii_t kii;
    kii_init(&kii);

    SECTION("task created") {
        TaskCreateCtx ctx = { KII_TASKC_OK, 0 };
        kii_set_cb_task_create(&kii, mock_task_create, &ctx);
        CHECK( kii_start_push_task(&kii, 300, khct::cb::cb_push, NULL) == KII_ERR_OK );
        CHECK( ctx.calls == 1 );
        CHECK( ctx.name == KII_TASK_NAME_MQTT );
        CHECK( ctx.entry_param == &kii );
    }

    SECTION("task not created") {
        TaskCreateCtx ctx = { KII_TASKC_FAIL, 0 };
        kii_set_cb_task_create(&kii, mock_task_create, &ctx);
        CHECK( kii_start_push_task(&kii, 300, khct::cb::cb_push, NULL) == KII_ERR_CREATE_TASK );
        CHECK( ctx.calls == 1 );
    }

    SECTION("no task-create callback") {
        CHECK( kii_start_push_task(&kii, 300, khct::cb::cb_push, NULL) == KII_ERR_CREATE_TASK );
    }
}

// A 200 response to the MQTT endpoint request with the given ports and TTL.
static std::string mqtt_endpoint_response(
        const std::string& port_tcp,
        const std::string& port_ssl,
        const std::string& ttl)
{
    const std::string body =
        "{"
        "\"installationID\" : \"dummyInstallationID\","
        "\"username\" : \"dummyUser\","
        "\"password\" : \"dummyPassword\","
        "\"mqttTopic\" : \"dummyTopic\","
        "\"host\" : \"jp-mqtt-dummy.kii.com\","
        "\"portTCP\" : " + port_tcp + ","
        "\"portSSL\" : " + port_ssl + ","
        "\"X-MQTT-TTL\" : " + ttl +
        "}";
    return
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/vnd.kii.MQTTEndpointResponse+json;charset=UTF-8\r\n"
        "Content-Length: " + std::to_string(body.length()) + "\r\n"
        "Connection: Close\r\n"
        "\r\n" + body;
}

// jkii reads the ports as int32_t and the TTL as int64_t, and kii keeps them as
// uint16_t and uint32_t. A value that does not fit is a response kii cannot
// use, as one jkii cannot read already was, rather than one to wrap around: a
// port of -1 became 4294967295.
TEST_CASE( "MQTT endpoint values out of range" ) {
    size_t kii_buff_size = 1024;
    char kii_buff[kii_buff_size];
    size_t mqtt_buff_size = 1024;
    char mqtt_buff[kii_buff_size];
    jkii_token_t jkii_tokens[256];
    jkii_resource_t jkii_resource = {jkii_tokens, 256};

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, kii_buff_size);
    kii_set_mqtt_buff(&kii, mqtt_buff, mqtt_buff_size);
    kii_set_json_parser_resource(&kii, &jkii_resource);

    khct::cb::SockCtx http_ctx;
    kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
    kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
    kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
    kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);
    kii_set_cb_delay_ms(&kii, khct::cb::cb_delay_ms, NULL);

    http_ctx.on_connect = [](void* socket_context, const char* host, uint16_t port) {
        return KHC_SOCK_OK;
    };
    http_ctx.on_send = [](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    http_ctx.on_close = [](void* socket_ctx) {
        return KHC_SOCK_OK;
    };

    const struct {
        const char* port_tcp;
        const char* port_ssl;
        const char* ttl;
        bool ok;
    } cases[] = {
        { "1883", "8883", "2147483647", true },
        { "0", "65535", "0", true },
        { "-1", "8883", "2147483647", false },
        { "1883", "-1", "2147483647", false },
        { "65536", "8883", "2147483647", false },
        { "1883", "65536", "2147483647", false },
        { "1883", "4294967297", "2147483647", false },
        { "1883", "8883", "-1", false },
        { "1883", "8883", "4294967295", true },
        { "1883", "8883", "4294967296", false },
        { "1883", "8883", "9223372036854775808", false },
    };

    for (const auto& c : cases) {
        INFO("portTCP " << c.port_tcp << ", portSSL " << c.port_ssl
                << ", X-MQTT-TTL " << c.ttl);
        std::stringstream ss;
        ss << mqtt_endpoint_response(c.port_tcp, c.port_ssl, c.ttl);
        http_ctx.on_recv = [&ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            *out_actual_length = ss.read(buffer, length_to_read).gcount();
            return KHC_SOCK_OK;
        };

        mqtt_state_t state;
        _init_mqtt_state(&kii, &state);
        state.info.task_state = KII_MQTT_ST_GET_ENDPOINT;
        strcpy(state.ins_id.id, "dummyInstallationID");
        _mqtt_state_get_endpoint(&state);
        if (c.ok) {
            CHECK( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
            CHECK( state.info.error == KII_MQTT_ERR_OK );
            CHECK( std::to_string(state.endpoint.port_tcp) == c.port_tcp );
            CHECK( std::to_string(state.endpoint.port_ssl) == c.port_ssl );
            CHECK( std::to_string(state.endpoint.ttl) == c.ttl );
        } else {
            CHECK( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
            CHECK( state.info.error == KII_MQTT_ERR_GET_ENDPOINT );
        }
    }
}

static kii_task_code_t count_task_create(const char* name, KII_TASK_ENTRY entry, void* entry_param, void* userdata)
{
    ++*(int*)userdata;
    return KII_TASKC_OK;
}

// CONNECT carries the keep-alive in 16 bits, and it was written there with no
// check, so 70000 reached the broker as 4464 while the task pinged every 70000
// seconds, and the broker dropped the connection in between.
TEST_CASE( "MQTT keep-alive interval range" ) {
    char mqtt_buff[1024];

    kii_t kii;
    kii_init(&kii);
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii_set_cb_delay_ms(&kii, khct::cb::cb_delay_ms, NULL);
    int call_task_create = 0;
    kii_set_cb_task_create(&kii, count_task_create, &call_task_create);

    khct::cb::SockCtx mqtt_ctx;
    kii_set_cb_mqtt_sock_send(&kii, khct::cb::mock_send, &mqtt_ctx);
    std::string sent;
    mqtt_ctx.on_send = [&sent](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        sent.append(buffer, length);
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    SECTION("an interval MQTT can carry is sent as given") {
        const struct {
            uint32_t interval;
            std::string bytes;
        } cases[] = {
            { 0, std::string("\x00\x00", 2) },
            { 300, std::string("\x01\x2c", 2) },
            { 65535, std::string("\xff\xff", 2) },
        };

        for (const auto& c : cases) {
            INFO("keep-alive " << c.interval);
            call_task_create = 0;
            CHECK( kii_start_push_task(&kii, c.interval, khct::cb::cb_push, NULL) == KII_ERR_OK );
            CHECK( call_task_create == 1 );

            mqtt_state_t state;
            _init_mqtt_state(&kii, &state);
            strcpy(state.endpoint.topic, "dummyTopic");
            strcpy(state.endpoint.username, "dummyUser");
            strcpy(state.endpoint.password, "dummyPassword");
            state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
            sent.clear();
            _mqtt_state_send_connect(&state);
            REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_CONNACK );
            // After the fixed header, the protocol name, level and flags.
            REQUIRE( sent.length() > 14 );
            CHECK( sent.substr(12, 2) == c.bytes );
        }
    }

    SECTION("an interval MQTT cannot carry does not start the task") {
        // kii_start_push_task() takes a uint32_t rather than a uint16_t for
        // this: the caller's value arrives whole, to be turned away.
        const uint32_t intervals[] = { 65536, 70000, UINT32_MAX };

        for (uint32_t interval : intervals) {
            INFO("keep-alive " << interval);
            kii._keep_alive_interval = 300;
            call_task_create = 0;
            CHECK( kii_start_push_task(&kii, interval, khct::cb::cb_push, NULL) == KII_ERR_FAIL );
            CHECK( call_task_create == 0 );
            CHECK( kii._keep_alive_interval == 300 );
        }
    }
}

// PINGREQ is sent once the keep-alive less the receive timeout has elapsed.
// That was worked out in unsigned int and kept in an int, so a receive timeout
// longer than the keep-alive wrapped round to a time never reached, and no
// PINGREQ was sent. With one as long, the task went to send PINGREQ again
// straight after sending one, before its PINGRESP could be read, and so
// reconnected. A keep-alive of 0, which disables PINGREQ, did the same when
// there was no receive timeout.
TEST_CASE( "MQTT PINGREQ timing" ) {
    kii_t kii;
    kii_init(&kii);
    kii_set_cb_delay_ms(&kii, khct::cb::cb_delay_ms, NULL);

    khct::cb::SockCtx mqtt_ctx;
    kii_set_cb_mqtt_sock_send(&kii, khct::cb::mock_send, &mqtt_ctx);
    kii_set_cb_mqtt_sock_recv(&kii, khct::cb::mock_recv, &mqtt_ctx);
    int call_send = 0;
    mqtt_ctx.on_send = [&call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        ++call_send;
        REQUIRE( std::string(buffer, length) == std::string("\xc0\x00", 2) );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    SECTION("after how long") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [&call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return KHC_SOCK_FAIL; // the receive timed out
        };

        const struct {
            uint16_t keep_alive;
            uint32_t to_recv;
            uint32_t elapsed_ms;
            bool pingreq;
        } cases[] = {
            // Early enough that a receive timing out ends within the keep-alive.
            { 300, 5, 295000, false },
            { 300, 5, 295001, true },
            { 300, 0, 300000, false },
            { 300, 0, 300001, true },
            { 65535, 5, 65530000, false },
            { 65535, 5, 65530001, true },
            // A timeout as long as the keep-alive or longer leaves no time for
            // that, so once the keep-alive is over. Sending PINGREQ and reading
            // PINGRESP count as a second, and a keep-alive is at least that, so
            // the PINGRESP is read before PINGREQ is due again.
            { 30, 30, 30000, false },
            { 30, 30, 30001, true },
            { 10, 30, 10000, false },
            { 10, 30, 10001, true },
            { 1, 5, 1000, false },
            { 1, 5, 1001, true },
            { 65535, UINT32_MAX, 65535000, false },
            { 65535, UINT32_MAX, 65535001, true },
            // Never, as keep-alive is disabled.
            { 0, 0, UINT32_MAX, false },
            { 0, 5, UINT32_MAX, false },
        };

        for (const auto& c : cases) {
            INFO("keep-alive " << c.keep_alive << ", receive timeout " << c.to_recv
                    << ", elapsed " << c.elapsed_ms << " ms");
            kii._keep_alive_interval = c.keep_alive;
            kii_set_mqtt_to_sock_recv(&kii, c.to_recv);
            mqtt_state_t state;
            _init_mqtt_state(&kii, &state);
            state.info.task_state = KII_MQTT_ST_RECV_READY;
            state.elapsed_time_ms = c.elapsed_ms;
            call_recv = 0;
            _mqtt_state_recv_ready(&state);
            if (c.pingreq) {
                CHECK( state.info.task_state == KII_MQTT_ST_SEND_PINGREQ );
                CHECK( call_recv == 0 );
            } else {
                CHECK( state.info.task_state == KII_MQTT_ST_RECV_READY );
                CHECK( call_recv == 1 );
            }
        }
    }

    // The time elapsed is counted in milliseconds in a uint32_t. A receive
    // timeout of more than 4294967 seconds wrapped round there to much less:
    // 4294968 seconds and the wait after it came to 1.704 seconds, UINT32_MAX
    // seconds and the wait to none at all, so PINGREQ was due far later than
    // it should have been, or never. Counting now stops at UINT32_MAX.
    SECTION("with a receive timeout too long to count in milliseconds") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [&call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return KHC_SOCK_FAIL; // the receive timed out
        };
        kii._keep_alive_interval = 65535;

        for (uint32_t to_recv : { (uint32_t)4294968, UINT32_MAX }) {
            INFO("receive timeout " << to_recv);
            kii_set_mqtt_to_sock_recv(&kii, to_recv);
            mqtt_state_t state;
            _init_mqtt_state(&kii, &state);
            state.info.task_state = KII_MQTT_ST_RECV_READY;
            call_recv = 0;
            _mqtt_state_recv_ready(&state);
            REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
            REQUIRE( call_recv == 1 );
            CHECK( state.elapsed_time_ms == UINT32_MAX );

            _mqtt_state_recv_ready(&state);
            CHECK( state.info.task_state == KII_MQTT_ST_SEND_PINGREQ );
            CHECK( call_recv == 1 );
        }
    }

    // A PUBLISH counts as a whole receive timeout, which stops at UINT32_MAX,
    // and reading it then adds no more rather than wrapping round.
    SECTION("with a message after a receive timeout too long to count") {
        kii._keep_alive_interval = 65535;
        kii_set_mqtt_to_sock_recv(&kii, UINT32_MAX);
        mqtt_state_t state;
        _init_mqtt_state(&kii, &state);
        state.info.task_state = KII_MQTT_ST_RECV_READY;

        size_t served = 0;
        // No MQTT buffer is set, so the message is read and dropped.
        serve_stream(mqtt_ctx, publish_packet("t", "message"), served);
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        CHECK( state.elapsed_time_ms == UINT32_MAX );
        _mqtt_state_recv_msg(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        CHECK( state.elapsed_time_ms == UINT32_MAX );

        _mqtt_state_recv_ready(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_SEND_PINGREQ );
    }

    // One PINGREQ after each receive that times out, which is as often as
    // the receive timeout allows, and no reconnecting.
    SECTION("with a receive timeout longer than the keep-alive") {
        size_t served = 0;
        kii._keep_alive_interval = 10;
        kii_set_mqtt_to_sock_recv(&kii, 30);
        mqtt_state_t state;
        _init_mqtt_state(&kii, &state);
        state.info.task_state = KII_MQTT_ST_RECV_READY;

        for (int i = 1; i <= 3; ++i) {
            INFO("PINGREQ " << i);
            served = 0;
            serve_stream(mqtt_ctx, "", served);
            _mqtt_state_recv_ready(&state);
            REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );

            // A receive has timed out, so the keep-alive is over.
            _mqtt_state_recv_ready(&state);
            REQUIRE( state.info.task_state == KII_MQTT_ST_SEND_PINGREQ );
            _mqtt_state_send_pingreq(&state);
            REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
            REQUIRE( call_send == i );

            // The PINGRESP is read before PINGREQ is due again.
            served = 0;
            serve_stream(mqtt_ctx, std::string("\xd0\x00", 2), served);
            _mqtt_state_recv_ready(&state);
            REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
            REQUIRE( state.pingresp_received == KII_TRUE );
            REQUIRE( served == 2 );
        }
    }
}
