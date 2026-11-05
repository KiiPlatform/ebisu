#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "kii.h"
#include "kii_mqtt_task.h"
#include "http_script.h"
#include "task_needs.h"

#include <stdint.h>
#include <string.h>
#include <deque>
#include <functional>
#include <sstream>
#include <set>
#include <string>
#include <vector>

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

// A QoS 1 PUBLISH, which carries a packet identifier after the topic. byte1 is
// the fixed header's first byte, 0x32 unless the DUP or RETAIN flag is wanted.
static std::string publish_qos1_packet(const std::string& topic, uint16_t id,
        const std::string& payload, char byte1 = 0x32)
{
    std::string id_bytes;
    id_bytes += (char)(id >> 8);
    id_bytes += (char)(id & 0xff);
    // Built as QoS 0's with the identifier at the payload's start, whose
    // remaining length counts it.
    std::string packet = publish_packet(topic, id_bytes + payload);
    packet[0] = byte1;
    return packet;
}

// Serves data as recv on a stream socket does: whatever is left of it, up to
// the size asked for. Once all of it has been read, nothing more arrives, so
// reports what a socket whose receive timeout ran out reports.
static void serve_stream(khct::cb::SockCtx& ctx, const std::string& data, size_t& served)
{
    ctx.on_recv = [data, &served](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        if (served == data.length()) {
            return EBISU_SOCK_FAIL;
        }
        *out_actual_length = data.copy(buffer, length_to_read, served);
        served += *out_actual_length;
        return EBISU_SOCK_OK;
    };
}

TEST_CASE( "MQTT state test" ) {
    size_t kii_buff_size = 1024;
    char kii_buff[kii_buff_size];
    size_t mqtt_buff_size = 1024;
    char mqtt_buff[kii_buff_size];

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, kii_buff_size);
    kii_set_mqtt_buff(&kii, mqtt_buff, mqtt_buff_size);
    kii._keep_alive_interval = 300;

    khct::cb::SockCtx http_ctx;
    kii_set_http_socket(&kii, http_ctx.socket());
    khct::cb::SockCtx mqtt_ctx;
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());

    kii_set_system(&kii, khct::cb::delay_only_system());

    khct::cb::PushCtx push_ctx;
    kii_set_cb_mqtt_message(&kii, khct::cb::cb_push, &push_ctx);

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
        return EBISU_SOCK_OK;
    };

    int call_send = 0;
    http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            const char req_line[] = "POST /api/apps/dummyAppID/installations HTTP/1.1\r\n";
            REQUIRE( length == strlen(req_line) );
            REQUIRE( strncmp(buffer, req_line, length) == 0 );
        }
        ++call_send;
        *out_sent_length = length;
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
    };

    int call_close = 0;
    http_ctx.on_close = [=, &call_close](void* socket_ctx) {
        ++call_close;
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
    };

    call_send = 0;
    http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            const char req_line[] = "GET /api/apps/dummyAppID/installations/dummyInstallationID/mqtt-endpoint HTTP/1.1\r\n";
            REQUIRE( length == strlen(req_line) );
            REQUIRE( strncmp(buffer, req_line, length) == 0 );
        }
        ++call_send;
        *out_sent_length = length;
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
    };

    call_close = 0;
    http_ctx.on_close = [=, &call_close](void* socket_ctx) {
        ++call_close;
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
    };

    _mqtt_state_sock_connect(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_SEND_CONNECT );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_connect == 1 );

    call_send = 0;
    mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            const char expect[] = {
                0x10, 0x30, // remaining_size = 48
                0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, (char)0xc2,
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
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
    };

    bool call_push = false;
    push_ctx.on_push = [=, &call_push](const kii_mqtt_message_t* message) {
        call_push = true;
        REQUIRE( message->payload_length == push_message.length() );
        REQUIRE( strncmp(message->payload, push_message.c_str(), message->payload_length) == 0 );
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
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
    };

    _mqtt_state_recv_ready(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_recv == 2 );

    call_recv = 0;
    mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        switch (call_recv) {
            case 0: // fixed header - a reserved type
                REQUIRE( length_to_read == 1);
                buffer[0] = 0xF0;
                *out_actual_length = 1;
                break;
            case 1: // fixed header - remaining length (= 4)
                REQUIRE( length_to_read == 1);
                buffer[0] = (char)0x04;
                *out_actual_length = 1;
                break;
            default:
                // Refused without reading the rest.
                FAIL();
                break;
        }
        ++call_recv;
        return EBISU_SOCK_OK;
    };

    _mqtt_state_recv_ready(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_recv == 2 );
}

// Serves data, then reports the end of the stream as ebisu_socket.h
// has a callback report that the peer closed the connection: EBISU_SOCK_OK with
// 0 bytes. A reader that does not stop there gets EBISU_SOCK_FAIL from the
// 100th call on, so that it fails the test's count of calls instead of
// hanging the test.
static void recv_then_end_of_stream(khct::cb::SockCtx& ctx, const std::string& data, int& call_recv)
{
    size_t served = 0;
    ctx.on_recv = [data, served, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) mutable {
        ++call_recv;
        if (call_recv >= 100) {
            return EBISU_SOCK_FAIL;
        }
        *out_actual_length = data.copy(buffer, length_to_read, served);
        served += *out_actual_length;
        return EBISU_SOCK_OK;
    };
}

// Stands in for a platform socket's recv_timed_out: answers as told, and
// records how often it was asked and with which context.
struct RecvTimedOutMock {
    bool timed_out;
    int calls;
    void* sock_ctx;
};
static RecvTimedOutMock recv_timed_out_mock;

static bool mock_recv_timed_out(void* sock_ctx)
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

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, kii_buff_size);
    kii_set_mqtt_buff(&kii, mqtt_buff, mqtt_buff_size);
    kii._keep_alive_interval = 300;

    khct::cb::SockCtx http_ctx;
    kii_set_http_socket(&kii, http_ctx.socket());
    khct::cb::SockCtx mqtt_ctx;
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());

    kii_set_system(&kii, khct::cb::delay_only_system());

    khct::cb::PushCtx push_ctx;
    kii_set_cb_mqtt_message(&kii, khct::cb::cb_push, &push_ctx);

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
            return EBISU_SOCK_OK;
        };

        int call_send = 0;
        http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            if (call_send == 0) {
                const char req_line[] = "POST /api/apps/dummyAppID/installations HTTP/1.1\r\n";
                REQUIRE( length == strlen(req_line) );
                REQUIRE( strncmp(buffer, req_line, length) == 0 );
            }
            ++call_send;
            *out_sent_length = length;
            return EBISU_SOCK_OK;
        };

        int call_recv = 0;
        std::stringstream ss;
        ss <<
            "HTTP/1.1 429 Too Many Requests\r\n"
            "\r\n";
        http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            *out_actual_length = ss.read(buffer, length_to_read).gcount();
            return EBISU_SOCK_OK;
        };

        int call_close = 0;
        http_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return EBISU_SOCK_OK;
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
            return EBISU_SOCK_OK;
        };

        int call_send = 0;
        http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            if (call_send == 0) {
                const char req_line[] = "POST /api/apps/dummyAppID/installations HTTP/1.1\r\n";
                REQUIRE( length == strlen(req_line) );
                REQUIRE( strncmp(buffer, req_line, length) == 0 );
            }
            ++call_send;
            *out_sent_length = length;
            return EBISU_SOCK_OK;
        };

        int call_recv = 0;
        std::stringstream ss;
        ss <<
            "HTTP/1.1 403 Forbidden\r\n"
            "\r\n";
        http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            *out_actual_length = ss.read(buffer, length_to_read).gcount();
            return EBISU_SOCK_OK;
        };

        int call_close = 0;
        http_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return EBISU_SOCK_OK;
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
            return EBISU_SOCK_OK;
        };

        int call_send = 0;
        http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            if (call_send == 0) {
                const char req_line[] = "GET /api/apps/dummyAppID/installations/dummyInstallationID/mqtt-endpoint HTTP/1.1\r\n";
                REQUIRE( length == strlen(req_line) );
                REQUIRE( strncmp(buffer, req_line, length) == 0 );
            }
            ++call_send;
            *out_sent_length = length;
            return EBISU_SOCK_OK;
        };

        int call_recv = 0;
        std::stringstream ss;
        ss <<
            "HTTP/1.1 429 Too Many Requests\r\n"
            "\r\n";
        http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            *out_actual_length = ss.read(buffer, length_to_read).gcount();
            return EBISU_SOCK_OK;
        };

        int call_close = 0;
        http_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return EBISU_SOCK_OK;
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
            return EBISU_SOCK_OK;
        };

        int call_send = 0;
        http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            if (call_send == 0) {
                const char req_line[] = "GET /api/apps/dummyAppID/installations/dummyInstallationID/mqtt-endpoint HTTP/1.1\r\n";
                REQUIRE( length == strlen(req_line) );
                REQUIRE( strncmp(buffer, req_line, length) == 0 );
            }
            ++call_send;
            *out_sent_length = length;
            return EBISU_SOCK_OK;
        };

        int call_recv = 0;
        std::stringstream ss;
        ss <<
            "HTTP/1.1 403 Forbidden\r\n"
            "\r\n";
        http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            *out_actual_length = ss.read(buffer, length_to_read).gcount();
            return EBISU_SOCK_OK;
        };

        int call_close = 0;
        http_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return EBISU_SOCK_OK;
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

    SECTION("a failure on the network is tried again") {
        enum FailAt { CONNECT, SEND, RECV };
        const FailAt fail_ats[] = { CONNECT, SEND, RECV };
        const kii_mqtt_task_state task_states[] = {
            KII_MQTT_ST_INSTALL_PUSH, KII_MQTT_ST_GET_ENDPOINT
        };
        for (FailAt fail_at : fail_ats) {
            for (kii_mqtt_task_state task_state : task_states) {
                INFO("fail at " << fail_at << ", state " << task_state);
                http_ctx.on_connect = [=](void* socket_context, const char* host, uint16_t port) {
                    return fail_at == CONNECT ? EBISU_SOCK_FAIL : EBISU_SOCK_OK;
                };
                http_ctx.on_send = [=](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
                    *out_sent_length = length;
                    return fail_at == SEND ? EBISU_SOCK_FAIL : EBISU_SOCK_OK;
                };
                http_ctx.on_recv = [=](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
                    *out_actual_length = 0;
                    return EBISU_SOCK_FAIL;
                };
                http_ctx.on_close = [=](void* socket_ctx) {
                    return EBISU_SOCK_OK;
                };

                state.info.task_state = task_state;
                strcpy(state.ins_id.id, "dummyInstallationID");
                if (task_state == KII_MQTT_ST_INSTALL_PUSH) {
                    _mqtt_state_install_push(&state);
                } else {
                    _mqtt_state_get_endpoint(&state);
                }
                CHECK( state.info.task_state == task_state );
                CHECK( state.info.error == KII_MQTT_ERR_OK );
            }
        }
    }

    SECTION("sock connect got EBISU_SOCK_AGAIN") {
        int call_connect = 0;
        mqtt_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
            const char* exp_host = "jp-mqtt-dummy.kii.com";
            ++call_connect;
            REQUIRE( strlen(host) == strlen(exp_host) );
            REQUIRE( strncmp(host, exp_host, strlen(exp_host)) == 0 );
            REQUIRE( port == 8883 );
            return EBISU_SOCK_AGAIN;
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

    SECTION("sock connect got EBISU_SOCK_FAIL") {
        int call_connect = 0;
        mqtt_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, uint16_t port) {
            const char* exp_host = "jp-mqtt-dummy.kii.com";
            ++call_connect;
            REQUIRE( strlen(host) == strlen(exp_host) );
            REQUIRE( strncmp(host, exp_host, strlen(exp_host)) == 0 );
            REQUIRE( port == 8883 );
            return EBISU_SOCK_FAIL;
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

    SECTION("send connect got EBISU_SOCK_AGAIN") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            return EBISU_SOCK_AGAIN;
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

    SECTION("send connect got EBISU_SOCK_FAIL") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            return EBISU_SOCK_FAIL;
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

    SECTION("recv connack got EBISU_SOCK_AGAIN 1") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    return EBISU_SOCK_AGAIN;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 0 );
    }

    SECTION("recv connack got EBISU_SOCK_FAIL 1") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    return EBISU_SOCK_FAIL;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 0 );
    }

    SECTION("recv connack got EBISU_SOCK_AGAIN 2") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x20;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 2)
                    return EBISU_SOCK_AGAIN;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv connack got EBISU_SOCK_FAIL 2") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(2)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x20;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 2)
                    return EBISU_SOCK_FAIL;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv connack got EBISU_SOCK_AGAIN 3") {
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
                    return EBISU_SOCK_AGAIN;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 2 );
    }

    SECTION("recv connack got EBISU_SOCK_FAIL 3") {
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
                    return EBISU_SOCK_FAIL;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
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
            return EBISU_SOCK_OK;
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
            return EBISU_SOCK_OK;
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
            return EBISU_SOCK_OK;
        };
        int call_close = 0;
        mqtt_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return EBISU_SOCK_OK;
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
            return EBISU_SOCK_OK;
        };
        int call_close = 0;
        mqtt_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return EBISU_SOCK_OK;
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
            return EBISU_SOCK_OK;
        };
        int call_close = 0;
        mqtt_ctx.on_close = [=, &call_close](void* socket_ctx) {
            ++call_close;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 3 );
        REQUIRE( call_close == 0 );
    }

    SECTION("send subscribe got EBISU_SOCK_AGAIN") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            return EBISU_SOCK_AGAIN;
        };

        state.info.task_state = KII_MQTT_ST_SEND_SUBSCRIBE;
        _mqtt_state_send_subscribe(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_send == 1 );
    }

    SECTION("send subscribe got EBISU_SOCK_FAIL") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            return EBISU_SOCK_FAIL;
        };

        state.info.task_state = KII_MQTT_ST_SEND_SUBSCRIBE;
        _mqtt_state_send_subscribe(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_send == 1 );
    }

    SECTION("recv suback got EBISU_SOCK_AGAIN 1") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(9)
                    return EBISU_SOCK_AGAIN;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 0 );
    }

    SECTION("recv suback got EBISU_SOCK_FAIL 1") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(9)
                    return EBISU_SOCK_FAIL;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 0 );
    }

    SECTION("recv suback got EBISU_SOCK_AGAIN 2") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(9)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = (char)0x90;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 3)
                    return EBISU_SOCK_AGAIN;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv suback got EBISU_SOCK_FAIL 2") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - control packet type(9)
                    REQUIRE( length_to_read == 1);
                    buffer[0] = (char)0x90;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length (= 3)
                    return EBISU_SOCK_FAIL;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv suback got EBISU_SOCK_AGAIN 3") {
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
                    return EBISU_SOCK_AGAIN;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 2 );
    }

    SECTION("recv suback got EBISU_SOCK_FAIL 3") {
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
                    return EBISU_SOCK_FAIL;
                    break;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
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
            return EBISU_SOCK_OK;
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
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 3 );
    }

    SECTION("recv ready got EBISU_SOCK_AGAIN 1") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - Publish
                    return EBISU_SOCK_AGAIN;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 0 );
    }

    // With a socket that has no recv_timed_out, a receive
    // that fails before any of a packet has arrived is taken for the receive
    // timeout running out, as it always was.
    SECTION("recv ready got EBISU_SOCK_FAIL 1") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - Publish
                    return EBISU_SOCK_FAIL;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
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
    SECTION("recv ready got EBISU_SOCK_AGAIN 2") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - Publish
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x30;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length[0]
                    return EBISU_SOCK_AGAIN;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv ready got EBISU_SOCK_FAIL 2") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            switch (call_recv) {
                case 0: // fixed header - Publish
                    REQUIRE( length_to_read == 1);
                    buffer[0] = 0x30;
                    *out_actual_length = 1;
                    break;
                case 1: // fixed header - remaining length[0]
                    return EBISU_SOCK_FAIL;
                default:
                    FAIL();
                    break;
            }
            ++call_recv;
            return EBISU_SOCK_OK;
        };

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    // The recv callback reports a receive timeout and a lost connection alike
    // as EBISU_SOCK_FAIL. Without the timed-out callback, recv ready can only
    // take it for a timeout, so with keep-alive 0, which sends no PINGREQ to
    // find out, it waits on a lost connection forever.
    SECTION("recv ready without the timed-out callback waits on a failed receive, even with keep-alive 0") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [&call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return EBISU_SOCK_FAIL;
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
            return EBISU_SOCK_FAIL;
        };
        mqtt_ctx.on_recv_timed_out = [](void* sock_ctx) {
            return mock_recv_timed_out(sock_ctx) == true;
        };
        recv_timed_out_mock.timed_out = true;

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
            return EBISU_SOCK_FAIL;
        };
        mqtt_ctx.on_recv_timed_out = [](void* sock_ctx) {
            return mock_recv_timed_out(sock_ctx) == true;
        };
        recv_timed_out_mock.timed_out = false;

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
            return EBISU_SOCK_FAIL;
        };
        mqtt_ctx.on_recv_timed_out = [](void* sock_ctx) {
            return mock_recv_timed_out(sock_ctx) == true;
        };
        recv_timed_out_mock.timed_out = true;

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( recv_timed_out_mock.calls == 1 );
        REQUIRE( recv_timed_out_mock.sock_ctx == &mqtt_ctx );
    }

    // khc reads the HTTP socket and has no use for it.
    SECTION("the timed-out callback is not asked about the HTTP socket") {
        http_ctx.on_connect = [](void* socket_context, const char* host, unsigned int port) {
            return EBISU_SOCK_OK;
        };
        http_ctx.on_send = [](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            *out_sent_length = length;
            return EBISU_SOCK_OK;
        };
        int call_recv = 0;
        http_ctx.on_recv = [&call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return EBISU_SOCK_FAIL;
        };
        http_ctx.on_close = [](void* socket_context) {
            return EBISU_SOCK_OK;
        };
        mqtt_ctx.on_recv_timed_out = [](void* sock_ctx) {
            return mock_recv_timed_out(sock_ctx) == true;
        };

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
            return EBISU_SOCK_FAIL;
        };
        mqtt_ctx.on_recv_timed_out = [](void* sock_ctx) {
            return mock_recv_timed_out(sock_ctx) == true;
        };
        recv_timed_out_mock.timed_out = true;

        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( call_recv == 1 );
        REQUIRE( recv_timed_out_mock.calls == 0 );
    }

    // Non-blocking sockets are not supported, but EBISU_SOCK_AGAIN says only that
    // nothing has arrived yet, so recv ready waits on as it always did.
    SECTION("recv ready waits on EBISU_SOCK_AGAIN without asking the timed-out callback") {
        mqtt_ctx.on_recv = [](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            return EBISU_SOCK_AGAIN;
        };
        mqtt_ctx.on_recv_timed_out = [](void* sock_ctx) {
            return mock_recv_timed_out(sock_ctx) == true;
        };
        recv_timed_out_mock.timed_out = false;

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( recv_timed_out_mock.calls == 0 );
    }

    // Once part of a packet has been read, the next read would start inside
    // it, so the task reconnects whether the receive timed out or not.
    SECTION("recv ready reconnects without asking the timed-out callback when the fixed header is cut off") {
        const ebisu_sock_code_t codes[] = { EBISU_SOCK_FAIL, EBISU_SOCK_AGAIN };
        for (ebisu_sock_code_t code : codes) {
            INFO("recv returns " << code << " for the remaining length");
            int call_recv = 0;
            mqtt_ctx.on_recv = [code, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
                if (call_recv++ == 0) { // fixed header - PUBLISH
                    buffer[0] = 0x30;
                    *out_actual_length = 1;
                    return EBISU_SOCK_OK;
                }
                return code;
            };
            mqtt_ctx.on_recv_timed_out = [](void* sock_ctx) {
                return mock_recv_timed_out(sock_ctx) == true;
            };
            recv_timed_out_mock.timed_out = true;
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
        mqtt_ctx.on_recv_timed_out = [](void* sock_ctx) {
            return mock_recv_timed_out(sock_ctx) == true;
        };
        recv_timed_out_mock.timed_out = true;

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        state.elapsed_time_ms = 1000;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( state.elapsed_time_ms == 0 );
        REQUIRE( recv_timed_out_mock.calls == 0 );
    }

    SECTION("recv msg got EBISU_SOCK_AGAIN from _mqtt_recv_remaining_trash") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return EBISU_SOCK_AGAIN;
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = state.kii->_mqtt_buffer_size + 1;
        _mqtt_state_recv_msg(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv msg got EBISU_SOCK_FAIL from _mqtt_recv_remaining_trash") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return EBISU_SOCK_FAIL;
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = state.kii->_mqtt_buffer_size + 1;
        _mqtt_state_recv_msg(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv msg got EBISU_SOCK_AGAIN from _mqtt_recv_remaining") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return EBISU_SOCK_AGAIN;
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = state.kii->_mqtt_buffer_size - 1;
        _mqtt_state_recv_msg(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
    }

    SECTION("recv msg got EBISU_SOCK_FAIL from _mqtt_recv_remaining") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [=, &call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return EBISU_SOCK_FAIL;
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
    // It is malformed, which MQTT has the client close the connection for.
    SECTION("recv msg reconnects on a message whose topic is longer than the message") {
        const std::string packet("\x00\x10" "abc", 5); // topic length 16
        size_t served = 0;
        mqtt_ctx.on_recv = [=, &served](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            *out_actual_length = packet.copy(buffer, length_to_read, served);
            served += *out_actual_length;
            return EBISU_SOCK_OK;
        };
        bool call_push = false;
        size_t push_length = 0;
        push_ctx.on_push = [&call_push, &push_length](const kii_mqtt_message_t* message) {
            call_push = true;
            push_length = message->payload_length;
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = packet.length();
        _mqtt_state_recv_msg(&state);
        CAPTURE( push_length );
        REQUIRE( call_push == false );
        REQUIRE( served == packet.length() );
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    }

    SECTION("recv msg reconnects on a one-byte message, too short for a topic length") {
        const std::string packet("\x00", 1);
        size_t served = 0;
        mqtt_ctx.on_recv = [=, &served](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            *out_actual_length = packet.copy(buffer, length_to_read, served);
            served += *out_actual_length;
            return EBISU_SOCK_OK;
        };
        bool call_push = false;
        size_t push_length = 0;
        push_ctx.on_push = [&call_push, &push_length](const kii_mqtt_message_t* message) {
            call_push = true;
            push_length = message->payload_length;
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = packet.length();
        _mqtt_state_recv_msg(&state);
        CAPTURE( push_length );
        REQUIRE( call_push == false );
        REQUIRE( served == packet.length() );
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    }

    // Nothing to read, which _mqtt_recv_remaining() reports as a failure.
    SECTION("recv msg reconnects on an empty message") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [&call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return EBISU_SOCK_FAIL;
        };
        bool call_push = false;
        push_ctx.on_push = [&call_push](const kii_mqtt_message_t* message) {
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
            return EBISU_SOCK_OK;
        };
        bool call_push = false;
        const char* mqtt_buffer = state.kii->_mqtt_buffer;
        push_ctx.on_push = [=, &call_push](const kii_mqtt_message_t* message) {
            call_push = true;
            REQUIRE( message->payload - mqtt_buffer == 5 );
            REQUIRE( message->payload_length == 0 );
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
            return EBISU_SOCK_OK;
        };
        bool call_push = false;
        const char* mqtt_buffer = state.kii->_mqtt_buffer;
        push_ctx.on_push = [=, &call_push](const kii_mqtt_message_t* message) {
            call_push = true;
            // As an offset: Catch would print a char* by reading through it.
            REQUIRE( message->payload - mqtt_buffer == 2 + 200 );
            REQUIRE( message->payload_length == 2 );
            REQUIRE( strncmp(message->payload, "{}", 2) == 0 );
        };

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = packet.length();
        _mqtt_state_recv_msg(&state);
        REQUIRE( call_push );
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
    }

    // Packets the task can't take used to be read and dropped: a PUBACK of
    // nothing it sent, or one of a reserved type. MQTT has the client close
    // the connection on them, as on any packet it doesn't allow.
    SECTION("recv ready reconnects on a packet it doesn't take, reading only its fixed header") {
        const std::string packets[] = {
            std::string("\x40\x02\x00\x01", 4),
            std::string("\xf0\x00", 2),
        };
        for (const std::string& packet : packets) {
            INFO("first byte " << (int)(unsigned char)packet[0]);
            size_t served = 0;
            serve_stream(mqtt_ctx, packet + publish_packet("topic", "message"), served);
            int call_push = 0;
            push_ctx.on_push = [&call_push](const kii_mqtt_message_t*) { ++call_push; };

            state.info.task_state = KII_MQTT_ST_RECV_READY;
            _mqtt_state_recv_ready(&state);
            CHECK( served == 2 );
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( state.info.error == KII_MQTT_ERR_OK );
            CHECK( call_push == 0 );
        }
    }

    // recv on a stream socket returns whatever is there, up to the size asked
    // for. Discarding a packet asked for 256 bytes at a time however little of
    // it was left, so when the next packet had already arrived it took the
    // start of that one too, and then read the middle of it as a fixed header.
    // A PUBLISH too large for the MQTT buffer is discarded: its last part is
    // shorter than 256 bytes.
    SECTION("recv msg discards a message larger than the buffer without reading into the next") {
        const std::string large = publish_packet("topic", std::string(state.kii->_mqtt_buffer_size, 'x'));
        const std::string payload(300, 'p');
        const std::string stream = large + publish_packet("topic", payload);
        size_t served = 0;
        serve_stream(mqtt_ctx, stream, served);
        int call_push = 0;
        push_ctx.on_push = [=, &call_push](const kii_mqtt_message_t* message) {
            ++call_push;
            REQUIRE( std::string(message->payload, message->payload_length) == payload );
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

    SECTION("send pingreq got EBISU_SOCK_AGAIN") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            return EBISU_SOCK_AGAIN;
        };

        state.info.task_state = KII_MQTT_ST_SEND_PINGREQ;
        _mqtt_state_send_pingreq(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_send == 1 );
        REQUIRE( state.elapsed_time_ms == 0 );
    }

    SECTION("send pingreq got EBISU_SOCK_FAIL") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            return EBISU_SOCK_FAIL;
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
            return EBISU_SOCK_OK;
        };

        REQUIRE( state.pingresp_received == true );
        state.info.task_state = KII_MQTT_ST_SEND_PINGREQ;
        _mqtt_state_send_pingreq(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.pingresp_received == false );
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
            return EBISU_SOCK_OK;
        };

        state.pingresp_received = false;
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.pingresp_received == true );
    }

    // The point of the flag: a half-open connection can keep accepting sends
    // for many retransmission timeouts, so waiting for a send error to fail is
    // slow. An unanswered PINGREQ reconnects within one keep-alive interval.
    SECTION("an unanswered pingreq reconnects without sending another") {
        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            *out_sent_length = length;
            return EBISU_SOCK_OK; // the socket still looks healthy
        };

        state.pingresp_received = false;
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
    // see the previous connection's false and reconnect again, forever.
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
            return EBISU_SOCK_OK;
        };

        // As left behind by the connection that died with a PINGREQ in flight.
        state.pingresp_received = false;
        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_SEND_SUBSCRIBE );
        REQUIRE( state.pingresp_received == true );

        int call_send = 0;
        mqtt_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            ++call_send;
            *out_sent_length = length;
            return EBISU_SOCK_OK;
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
        // The type, then the remaining length, which is where the end is
        // noticed.
        REQUIRE( call_recv == 2 );
    }

    SECTION("recv msg reconnects when the stream ends partway through a message") {
        int call_recv = 0;
        // Topic length 5, the topic, and 3 of the 10 bytes of the body.
        recv_then_end_of_stream(mqtt_ctx, std::string("\x00\x05" "topic" "abc", 10), call_recv);
        bool call_push = false;
        push_ctx.on_push = [&call_push](const kii_mqtt_message_t* message) {
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
        // The topic's length and the topic, then the rest 256 bytes at a
        // time, so two reads of it, then the end.
        recv_then_end_of_stream(mqtt_ctx, std::string("\x00\x05" "topic", 7) + std::string(293, 'x'), call_recv);

        state.info.task_state = KII_MQTT_ST_RECV_MSG;
        state.remaining_message_size = state.kii->_mqtt_buffer_size + 1;
        state.elapsed_time_ms = 1000;
        _mqtt_state_recv_msg(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( state.elapsed_time_ms == 0 );
        REQUIRE( call_recv == 5 );
    }

    // The end of the stream used to read as a header of type 0 and length 0,
    // which recv ready discarded, and discarding nothing reported a failure:
    // it reconnected only by way of that. Now it reconnects on the end itself,
    // without reading on. Reporting the end as a failure instead would make
    // recv ready take it for a receive timeout and wait on until the
    // keep-alive interval ran out.
    SECTION("recv ready reconnects when the stream ends between packets") {
        int call_recv = 0;
        recv_then_end_of_stream(mqtt_ctx, "", call_recv);

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        state.elapsed_time_ms = 1000;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECONNECT );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( state.elapsed_time_ms == 0 );
        REQUIRE( call_recv == 1 );
    }

    // MQTT's remaining length is at most four bytes, the last without the
    // continuation bit.
    SECTION("recv ready reconnects on a remaining length that does not end") {
        int call_recv = 0;
        recv_then_end_of_stream(mqtt_ctx, "\x30\xff\xff\xff\xff\x00", call_recv);

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        state.elapsed_time_ms = 1000;
        _mqtt_state_recv_ready(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
        CHECK( state.info.error == KII_MQTT_ERR_OK );
        CHECK( call_recv == 5 );
    }

    // The end of the stream right after a packet's first byte used to read as
    // a remaining length of 0, so a PINGRESP's type byte counted as the
    // PINGRESP and a PUBLISH's moved on to reading the message.
    SECTION("recv ready reconnects when the stream ends after a packet's type") {
        const struct {
            const char* name;
            char type;
        } packets[] = {
            { "PINGRESP", '\xd0' },
            { "PUBLISH", '\x30' },
            { "PUBACK", '\x40' },
        };
        for (const auto& packet : packets) {
            INFO(packet.name);
            int call_recv = 0;
            recv_then_end_of_stream(mqtt_ctx, std::string(1, packet.type), call_recv);

            state.info.task_state = KII_MQTT_ST_RECV_READY;
            state.pingresp_received = false;
            state.elapsed_time_ms = 1000;
            _mqtt_state_recv_ready(&state);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( state.info.error == KII_MQTT_ERR_OK );
            CHECK( state.elapsed_time_ms == 0 );
            CHECK( state.pingresp_received == false );
            CHECK( call_recv == 2 );
        }
    }
}

// CONNECT names MQTT 3.1.1, where it used to name 3.1, which a broker with
// nothing later can still be given.
TEST_CASE( "The version of MQTT CONNECT names" ) {
    char mqtt_buff[1024];
    kii_t kii;
    kii_init(&kii);
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii._keep_alive_interval = 300;
    std::string sent;
    khct::cb::SockCtx mqtt_ctx;
    mqtt_ctx.on_send = [&sent](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        sent.append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());

    mqtt_state_t state;
    _init_mqtt_state(&kii, &state);
    strcpy(state.endpoint.topic, "dummyTopic");
    strcpy(state.endpoint.username, "dummyUser");
    strcpy(state.endpoint.password, "dummyPassword");
    const std::string payload(
        "\x00\x0a" "dummyTopic" "\x00\x09" "dummyUser" "\x00\x0d" "dummyPassword", 38);
    // The flags, username, password and clean session, and the keep-alive.
    const std::string flags_keep_alive("\xc2\x01\x2c", 3);

    SECTION("3.1.1 by default") {
        CHECK( kii._mqtt_version == KII_MQTT_VERSION_3_1_1 );
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        _mqtt_state_send_connect(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_CONNACK );
        CHECK( sent == std::string("\x10\x30" "\x00\x04" "MQTT" "\x04", 9) +
                flags_keep_alive + payload );
    }

    SECTION("3.1 set") {
        kii_set_mqtt_version(&kii, KII_MQTT_VERSION_3_1);
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        _mqtt_state_send_connect(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_CONNACK );
        CHECK( sent == std::string("\x10\x32" "\x00\x06" "MQIsdp" "\x03", 11) +
                flags_keep_alive + payload );
    }

    SECTION("3.1.1 set again") {
        kii_set_mqtt_version(&kii, KII_MQTT_VERSION_3_1);
        kii_set_mqtt_version(&kii, KII_MQTT_VERSION_3_1_1);
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        _mqtt_state_send_connect(&state);
        CHECK( sent.compare(0, 9, std::string("\x10\x30" "\x00\x04" "MQTT" "\x04", 9)) == 0 );
    }

    // The buffer holds 8 bytes for the fixed header, CONNECT after them, and
    // the terminator the last string is copied with: 57 bytes in 3.1.1, whose
    // protocol name is 2 shorter, and 59 in 3.1.
    SECTION("the buffer CONNECT needs") {
        const kii_mqtt_version_t version = GENERATE(KII_MQTT_VERSION_3_1_1, KII_MQTT_VERSION_3_1);
        const size_t needed = version == KII_MQTT_VERSION_3_1 ? 59 : 57;
        const bool fits = GENERATE(true, false);
        INFO("version " << version << ", buffer " << (fits ? needed : needed - 1));
        kii_set_mqtt_version(&kii, version);
        kii_set_mqtt_buff(&kii, mqtt_buff, fits ? needed : needed - 1);
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        _mqtt_state_send_connect(&state);
        if (fits) {
            CHECK( state.info.task_state == KII_MQTT_ST_RECV_CONNACK );
            CHECK( sent.size() == needed - 8 - 1 + 2 );
        } else {
            CHECK( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
            CHECK( state.info.error == KII_MQTT_ERR_INSUFFICIENT_BUFF );
            CHECK( sent.empty() );
        }
    }
}

// SUBSCRIBE was written into the buffer unchecked, as CONNECT's check counted
// the topic, which was its client identifier. A client identifier of the
// application's took its place there, so a buffer CONNECT fitted in could be
// too small for SUBSCRIBE's topic.
TEST_CASE( "The buffer SUBSCRIBE needs" ) {
    kii_t kii;
    kii_init(&kii);
    kii._keep_alive_interval = 300;
    std::string sent;
    khct::cb::SockCtx mqtt_ctx;
    mqtt_ctx.on_send = [&sent](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        sent.append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());
    REQUIRE( kii_set_mqtt_client_id(&kii, "c") == KII_ERR_OK );

    mqtt_state_t state;
    _init_mqtt_state(&kii, &state);
    const std::string topic(KII_TOPIC_MAX_SIZE - 1, 't');
    strcpy(state.endpoint.topic, topic.c_str());
    strcpy(state.endpoint.username, "u");
    strcpy(state.endpoint.password, "p");

    // 8 bytes for the fixed header, the packet identifier, 2 bytes before the
    // topic and the QoS after it.
    const size_t needed = 8 + 2 + 2 + topic.size() + 1;
    // CONNECT's, as in the test case above, is far less with this topic.
    const size_t connect_needs = 8 + 2 + 4 + 1 + 1 + 2 + (2 + 1) + (2 + 1) + (2 + 1) + 1;
    REQUIRE( connect_needs < needed );

    SECTION("CONNECT fits in what it needs") {
        // On the heap, so that a write past it is one under ASan.
        std::vector<char> buff(connect_needs);
        kii_set_mqtt_buff(&kii, buff.data(), buff.size());
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        _mqtt_state_send_connect(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_CONNACK );
        CHECK( sent.size() == connect_needs - 8 - 1 + 2 );
    }

    SECTION("SUBSCRIBE fails, sending nothing, in a buffer too small for its topic") {
        for (size_t size : { connect_needs, needed - 1 }) {
            INFO("buffer " << size);
            std::vector<char> buff(size);
            kii_set_mqtt_buff(&kii, buff.data(), buff.size());
            state.info.task_state = KII_MQTT_ST_SEND_SUBSCRIBE;
            state.info.error = KII_MQTT_ERR_OK;
            _mqtt_state_send_subscribe(&state);
            CHECK( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
            CHECK( state.info.error == KII_MQTT_ERR_INSUFFICIENT_BUFF );
            CHECK( sent.empty() );
        }
    }

    SECTION("SUBSCRIBE fits in what it needs") {
        std::vector<char> buff(needed);
        kii_set_mqtt_buff(&kii, buff.data(), buff.size());
        state.info.task_state = KII_MQTT_ST_SEND_SUBSCRIBE;
        _mqtt_state_send_subscribe(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_SUBACK );
        // The fixed header, of 2 bytes, then the rest.
        CHECK( sent == std::string("\x82\x44" "\x00\x01" "\x00\x3f", 6) + topic + std::string(1, '\0') );
    }
}

// CONNECT carries the password, which stayed in the buffer until a packet long
// enough came in over it.
TEST_CASE( "CONNECT's password is cleared from the buffer once sent" ) {
    // Defined, as the test looks through all of it.
    char mqtt_buff[256] = { 0 };
    kii_t kii;
    kii_init(&kii);
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii._keep_alive_interval = 300;
    kii_set_system(&kii, khct::cb::delay_only_system());
    std::string sent;
    bool fail = false;
    khct::cb::SockCtx mqtt_ctx;
    mqtt_ctx.on_send = [&sent, &fail](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        if (fail) {
            return EBISU_SOCK_FAIL;
        }
        sent.append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());

    mqtt_state_t state;
    _init_mqtt_state(&kii, &state);
    strcpy(state.endpoint.topic, "dummyTopic");
    strcpy(state.endpoint.username, "dummyUser");
    strcpy(state.endpoint.password, "MQTT-PASSWORD-SECRET");
    const std::string password("MQTT-PASSWORD-SECRET");

    SECTION("sent") {
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        _mqtt_state_send_connect(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_CONNACK );
        CHECK( sent.find(password) != std::string::npos );
        CHECK( std::string(mqtt_buff, sizeof(mqtt_buff)).find(password) == std::string::npos );
    }

    SECTION("not sent") {
        fail = true;
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        _mqtt_state_send_connect(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
        CHECK( std::string(mqtt_buff, sizeof(mqtt_buff)).find(password) == std::string::npos );
    }
}

// CONNECT had no will, so other clients had no way to learn that a device
// had gone. One set by kii_set_mqtt_will() goes in each CONNECT.
TEST_CASE( "CONNECT carries the last will" ) {
    char mqtt_buff[1024];
    kii_t kii;
    kii_init(&kii);
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii._keep_alive_interval = 300;
    std::string sent;
    khct::cb::SockCtx mqtt_ctx;
    mqtt_ctx.on_send = [&sent](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        sent.append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());

    mqtt_state_t state;
    _init_mqtt_state(&kii, &state);
    strcpy(state.endpoint.topic, "dummyTopic");
    strcpy(state.endpoint.username, "dummyUser");
    strcpy(state.endpoint.password, "dummyPassword");
    const char message[] = { 'g', 'o', 'n', 'e', '\0', '!' };
    kii_mqtt_will_t will = { "th/1/status", message, sizeof(message), KII_MQTT_QOS_1, true };

    auto connect = [&state, &sent]() {
        sent.clear();
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        _mqtt_state_send_connect(&state);
    };

    SECTION("byte for byte, after the client identifier") {
        REQUIRE( kii_set_mqtt_will(&kii, &will) == KII_ERR_OK );
        connect();
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_CONNACK );
        const std::string payload = std::string("\x00\x0a" "dummyTopic", 12) +
            std::string("\x00\x0b" "th/1/status", 13) +
            std::string("\x00\x06" "gone\0!", 8) +
            std::string("\x00\x09" "dummyUser" "\x00\x0d" "dummyPassword", 26);
        // Username, password, will retain, will QoS 1, will, clean session.
        const std::string expected = std::string("\x10\x45" "\x00\x04" "MQTT" "\x04" "\xee" "\x01\x2c", 12) +
            payload;
        CHECK( sent == expected );
    }

    SECTION("with its QoS and retain flags in CONNECT's flags") {
        const struct { kii_mqtt_qos_t qos; bool retain; bool clean; unsigned char flags; } cases[] = {
            { KII_MQTT_QOS_0, false, true, 0xc6 },
            { KII_MQTT_QOS_1, false, true, 0xce },
            { KII_MQTT_QOS_0, true, true, 0xe6 },
            { KII_MQTT_QOS_1, true, false, 0xec },
        };
        for (const auto& c : cases) {
            INFO( "qos " << c.qos << ", retain " << c.retain << ", clean " << c.clean );
            will.qos = c.qos;
            will.retain = c.retain;
            kii_set_mqtt_clean_session(&kii, c.clean);
            REQUIRE( kii_set_mqtt_will(&kii, &will) == KII_ERR_OK );
            connect();
            REQUIRE( sent.size() > 9 );
            CHECK( (unsigned char)sent[9] == c.flags );
        }
    }

    SECTION("none, as by default") {
        REQUIRE( kii_set_mqtt_will(&kii, &will) == KII_ERR_OK );
        REQUIRE( kii_set_mqtt_will(&kii, NULL) == KII_ERR_OK );
        connect();
        REQUIRE( sent.size() > 9 );
        CHECK( (unsigned char)sent[9] == 0xc2 );
        CHECK( sent.find("th/1/status") == std::string::npos );
    }

    SECTION("an empty message") {
        kii_mqtt_will_t empty = { "t", NULL, 0, KII_MQTT_QOS_0, false };
        REQUIRE( kii_set_mqtt_will(&kii, &empty) == KII_ERR_OK );
        connect();
        CHECK( sent.find(std::string("\x00\x01" "t" "\x00\x00" "\x00\x09" "dummyUser", 16)) != std::string::npos );
    }

    // 57 bytes for CONNECT without it, and 4 more for each length, the topic
    // and the message.
    SECTION("in the buffer CONNECT needs") {
        REQUIRE( kii_set_mqtt_will(&kii, &will) == KII_ERR_OK );
        const size_t needed = 57 + 4 + strlen(will.topic) + will.message_length;
        kii_set_mqtt_buff(&kii, mqtt_buff, needed);
        connect();
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_CONNACK );
        kii_set_mqtt_buff(&kii, mqtt_buff, needed - 1);
        connect();
        CHECK( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
        CHECK( state.info.error == KII_MQTT_ERR_INSUFFICIENT_BUFF );
        CHECK( sent.empty() );
    }

    SECTION("that CONNECT can carry, or none is set") {
        REQUIRE( kii_set_mqtt_will(&kii, &will) == KII_ERR_OK );
        const std::string long_topic(65536, 't');
        kii_mqtt_will_t too_long = will;
        too_long.topic = long_topic.c_str();
        CHECK( kii_set_mqtt_will(&kii, &too_long) == KII_ERR_TOO_LARGE_DATA );
        too_long = will;
        too_long.message_length = 65536;
        CHECK( kii_set_mqtt_will(&kii, &too_long) == KII_ERR_TOO_LARGE_DATA );
        kii_mqtt_will_t bad = will;
        bad.topic = NULL;
        CHECK( kii_set_mqtt_will(&kii, &bad) == KII_ERR_FAIL );
        bad = will;
        bad.message = NULL;
        CHECK( kii_set_mqtt_will(&kii, &bad) == KII_ERR_FAIL );
        bad = will;
        bad.qos = (kii_mqtt_qos_t)2;
        CHECK( kii_set_mqtt_will(&kii, &bad) == KII_ERR_FAIL );
        CHECK( kii._mqtt_will == &will );
    }
}

TEST_CASE( "kii_enable_insecure_mqtt test" ) {
    size_t kii_buff_size = 1024;
    char kii_buff[kii_buff_size];
    size_t mqtt_buff_size = 1024;
    char mqtt_buff[kii_buff_size];

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, kii_buff_size);
    kii_set_mqtt_buff(&kii, mqtt_buff, mqtt_buff_size);
    kii._keep_alive_interval = 300;

    khct::cb::SockCtx http_ctx;
    kii_set_http_socket(&kii, http_ctx.socket());
    khct::cb::SockCtx mqtt_ctx;
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());

    kii_set_system(&kii, khct::cb::delay_only_system());

    khct::cb::PushCtx push_ctx;
    kii_set_cb_mqtt_message(&kii, khct::cb::cb_push, &push_ctx);

    REQUIRE(kii._khc._enable_insecure == 0);
    REQUIRE(kii._insecure_mqtt == false);
    kii_enable_insecure_mqtt(&kii, true);
    REQUIRE(kii._khc._enable_insecure == 0);
    REQUIRE(kii._insecure_mqtt == true);

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
        return EBISU_SOCK_OK;
    };

    int call_send = 0;
    http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            const char req_line[] = "POST /api/apps/dummyAppID/installations HTTP/1.1\r\n";
            REQUIRE( length == strlen(req_line) );
            REQUIRE( strncmp(buffer, req_line, length) == 0 );
        }
        ++call_send;
        *out_sent_length = length;
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
    };

    int call_close = 0;
    http_ctx.on_close = [=, &call_close](void* socket_ctx) {
        ++call_close;
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
    };

    call_send = 0;
    http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            const char req_line[] = "GET /api/apps/dummyAppID/installations/dummyInstallationID/mqtt-endpoint HTTP/1.1\r\n";
            CHECK( length == strlen(req_line) );
            CHECK( strncmp(buffer, req_line, length) == 0 );
        }
        ++call_send;
        *out_sent_length = length;
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
    };

    call_close = 0;
    http_ctx.on_close = [=, &call_close](void* socket_ctx) {
        ++call_close;
        return EBISU_SOCK_OK;
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
        return EBISU_SOCK_OK;
    };

    _mqtt_state_sock_connect(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_SEND_CONNECT );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( call_connect == 1 );
}

// Stands in for the platform's task creation: records the call and returns
// the result the test chose.
struct TaskCreateCtx {
    ebisu_task_code_t result;
    int calls;
    std::string name;
    void* entry_param;
    EBISU_TASK_ENTRY entry;
};

static ebisu_task_code_t mock_task_create(const char* name, EBISU_TASK_ENTRY entry, void* entry_param, void* userdata)
{
    TaskCreateCtx* ctx = (TaskCreateCtx*)userdata;
    ++ctx->calls;
    ctx->name = name;
    ctx->entry_param = entry_param;
    ctx->entry = entry;
    return ctx->result;
}

// The task-create callback is what starts the MQTT task, so its result is
// whether push is running. kii_start_push_task() ignored it and returned
// KII_ERR_OK even when no task had been created, and called through the
// callback without checking that one had been set.
// The task subscribed at QoS 0 with a clean session, and read every PUBLISH
// as QoS 0's. It now subscribes at the QoS set, acknowledges a QoS 1 message
// with PUBACK, asks the broker to keep its session if told to, and takes the
// messages such a broker sends before SUBACK.
TEST_CASE( "QoS 1 and a session the broker keeps" ) {
    char mqtt_buff[256];
    kii_t kii;
    kii_init(&kii);
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii._keep_alive_interval = 300;
    kii_set_system(&kii, khct::cb::delay_only_system());

    // What was sent, and what had been when the push callback was called.
    std::string sent;
    bool send_ok = true;
    khct::cb::SockCtx mqtt_ctx;
    mqtt_ctx.on_send = [&sent, &send_ok](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        if (!send_ok) {
            return EBISU_SOCK_FAIL;
        }
        sent.append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());
    std::vector<std::string> pushed;
    std::vector<std::string> sent_at_push;
    khct::cb::PushCtx push_ctx;
    push_ctx.on_push = [&pushed, &sent_at_push, &sent](const kii_mqtt_message_t* message) {
        pushed.push_back(std::string(message->payload, message->payload_length));
        sent_at_push.push_back(sent);
    };
    kii_set_cb_mqtt_message(&kii, khct::cb::cb_push, &push_ctx);

    mqtt_state_t state;
    _init_mqtt_state(&kii, &state);
    strcpy(state.endpoint.topic, "dummyTopic");
    strcpy(state.endpoint.username, "dummyUser");
    strcpy(state.endpoint.password, "dummyPassword");
    size_t served = 0;

    const std::string puback_7("\x40\x02\x00\x07", 4);

    // CONNECT's flags come after the fixed header, of 2 bytes here, and the
    // protocol name, whose length is in its second byte, and level.
    auto connect_flags = [&sent]() {
        REQUIRE( sent.size() > 4 );
        const size_t at = 2 + 2 + (unsigned char)sent[3] + 1;
        REQUIRE( sent.size() > at );
        return (unsigned char)sent[at];
    };

    SECTION("CONNECT asks for a clean session by default") {
        CHECK( kii._mqtt_clean_session == true );
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        _mqtt_state_send_connect(&state);
        CHECK( connect_flags() == 0xc2 );
    }

    SECTION("CONNECT asks the broker to keep the session when told to") {
        kii_set_mqtt_clean_session(&kii, false);
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        _mqtt_state_send_connect(&state);
        CHECK( connect_flags() == 0xc0 );
    }

    SECTION("SUBSCRIBE asks for QoS 0 by default, and QoS 1 when set") {
        CHECK( kii._mqtt_qos == KII_MQTT_QOS_0 );
        state.info.task_state = KII_MQTT_ST_SEND_SUBSCRIBE;
        _mqtt_state_send_subscribe(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_SUBACK );
        REQUIRE_FALSE( sent.empty() );
        CHECK( sent.back() == 0x00 );

        sent.clear();
        kii_set_mqtt_qos(&kii, KII_MQTT_QOS_1);
        _mqtt_state_send_subscribe(&state);
        REQUIRE_FALSE( sent.empty() );
        CHECK( sent.back() == 0x01 );
    }

    SECTION("a SUBACK granting QoS 1 is a subscription") {
        kii_set_mqtt_qos(&kii, KII_MQTT_QOS_1);
        serve_stream(mqtt_ctx, std::string("\x90\x03\x00\x01\x01", 5), served);
        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_READY );
    }

    SECTION("a QoS 1 message is handed over, then acknowledged") {
        const std::string packet = publish_qos1_packet("dummyTopic", 7, "{\"a\":1}");
        serve_stream(mqtt_ctx, packet, served);
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        CHECK( state.message_qos == 1 );
        _mqtt_state_recv_msg(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_READY );
        // The body alone, without the packet identifier.
        REQUIRE( pushed.size() == 1 );
        CHECK( pushed[0] == "{\"a\":1}" );
        CHECK( sent_at_push[0].empty() );
        CHECK( sent == puback_7 );
        CHECK( served == packet.size() );
    }

    SECTION("a QoS 1 message sent again with DUP is handed over and acknowledged again") {
        const std::string packet = publish_qos1_packet("dummyTopic", 7, "{}") +
            publish_qos1_packet("dummyTopic", 7, "{}", 0x3a);
        serve_stream(mqtt_ctx, packet, served);
        for (int i = 0; i < 2; ++i) {
            state.info.task_state = KII_MQTT_ST_RECV_READY;
            _mqtt_state_recv_ready(&state);
            REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
            _mqtt_state_recv_msg(&state);
        }
        CHECK( pushed.size() == 2 );
        CHECK( sent == puback_7 + puback_7 );
    }

    SECTION("a QoS 0 message isn't acknowledged") {
        kii_set_mqtt_qos(&kii, KII_MQTT_QOS_1);
        serve_stream(mqtt_ctx, publish_packet("dummyTopic", "{}"), served);
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        CHECK( state.message_qos == 0 );
        _mqtt_state_recv_msg(&state);
        REQUIRE( pushed.size() == 1 );
        CHECK( pushed[0] == "{}" );
        CHECK( sent.empty() );
    }

    SECTION("a QoS 1 message too large for the buffer is dropped, and acknowledged") {
        const std::string large = publish_qos1_packet("dummyTopic", 7, std::string(sizeof(mqtt_buff), 'x'));
        const std::string next = publish_qos1_packet("dummyTopic", 8, "{}");
        serve_stream(mqtt_ctx, large + next, served);
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        REQUIRE( state.remaining_message_size > sizeof(mqtt_buff) );
        _mqtt_state_recv_msg(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_READY );
        CHECK( pushed.empty() );
        CHECK( sent == puback_7 );
        // Read to its end, and no further.
        CHECK( served == large.size() );

        _mqtt_state_recv_ready(&state);
        _mqtt_state_recv_msg(&state);
        CHECK( pushed.size() == 1 );
        CHECK( sent == puback_7 + std::string("\x40\x02\x00\x08", 4) );
        CHECK( served == large.size() + next.size() );
    }

    // Malformed, which MQTT has the client close the connection for.
    SECTION("a QoS 1 message with no room for its packet identifier reconnects, unacknowledged") {
        // The topic's length leaves 1 byte after it, not 2.
        const std::string packet("\x32\x07\x00\x04" "abcd" "\x07", 9);
        serve_stream(mqtt_ctx, packet, served);
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        _mqtt_state_recv_msg(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
        CHECK( pushed.empty() );
        CHECK( sent.empty() );
    }

    SECTION("a QoS 1 message too large for the buffer, with no room for its packet identifier") {
        // A topic as long as the message, which has no room after it.
        std::string packet("\x32", 1);
        const size_t remaining = sizeof(mqtt_buff) + 10;
        packet += (char)(0x80 | (remaining % 128));
        packet += (char)(remaining / 128);
        packet += (char)((remaining - 2) >> 8);
        packet += (char)((remaining - 2) & 0xff);
        packet.append(remaining - 2, 't');
        serve_stream(mqtt_ctx, packet, served);
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        _mqtt_state_recv_msg(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
        CHECK( pushed.empty() );
        CHECK( sent.empty() );
        // Its fixed header and its topic's length, and no more.
        CHECK( served == 3 + 2 );
    }

    SECTION("a PUBACK that can't be sent reconnects") {
        send_ok = false;
        serve_stream(mqtt_ctx, publish_qos1_packet("dummyTopic", 7, "{}"), served);
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        _mqtt_state_recv_msg(&state);
        CHECK( pushed.size() == 1 );
        CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
    }

    SECTION("a QoS 2 message reconnects, unread") {
        serve_stream(mqtt_ctx, publish_qos1_packet("dummyTopic", 7, "{}", 0x34), served);
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        CHECK( state.message_qos == 2 );
        _mqtt_state_recv_msg(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
        CHECK( pushed.empty() );
        CHECK( sent.empty() );
    }

    // A broker that kept the session sends what it kept for the subscription,
    // which may come before SUBACK. The task took any packet but SUBACK for a
    // failure, and reconnected, to be sent the same again.
    SECTION("messages before SUBACK are taken, then SUBACK") {
        kii_set_mqtt_qos(&kii, KII_MQTT_QOS_1);
        const std::string suback("\x90\x03\x00\x01\x01", 5);
        serve_stream(mqtt_ctx, publish_qos1_packet("dummyTopic", 7, "{\"kept\":1}") +
                publish_packet("dummyTopic", "{\"kept\":2}") + suback, served);
        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        std::vector<kii_mqtt_task_state> states;
        for (int i = 0; i < 8 && state.info.task_state != KII_MQTT_ST_RECV_READY &&
                state.info.task_state != KII_MQTT_ST_RECONNECT; ++i) {
            if (state.info.task_state == KII_MQTT_ST_RECV_SUBACK) {
                _mqtt_state_recv_suback(&state);
            } else {
                REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
                _mqtt_state_recv_msg(&state);
            }
            states.push_back(state.info.task_state);
        }
        CHECK( states == std::vector<kii_mqtt_task_state>({
                KII_MQTT_ST_RECV_MSG, KII_MQTT_ST_RECV_SUBACK,
                KII_MQTT_ST_RECV_MSG, KII_MQTT_ST_RECV_SUBACK,
                KII_MQTT_ST_RECV_READY }) );
        CHECK( pushed == std::vector<std::string>({ "{\"kept\":1}", "{\"kept\":2}" }) );
        CHECK( sent == puback_7 );
        CHECK( state.awaiting_suback == false );
    }

    SECTION("a message before SUBACK that is cut off reconnects") {
        const std::string packet = publish_qos1_packet("dummyTopic", 7, "{}");
        serve_stream(mqtt_ctx, packet.substr(0, packet.size() - 1), served);
        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        _mqtt_state_recv_msg(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
        CHECK( pushed.empty() );
    }
}

TEST_CASE( "kii_start_push_task reports a task it could not create" ) {
    char buff[64];
    kii_t kii;
    kii_init(&kii);
    khct::set_push_needs(&kii, buff, sizeof(buff));

    SECTION("task created") {
        TaskCreateCtx ctx = { EBISU_TASKC_OK, 0 };
        khct::cb::SystemCtx kii_sys;
        kii_sys.on_task_create = [&](const char* name, EBISU_TASK_ENTRY entry, void* entry_param) {
            return mock_task_create(name, entry, entry_param, &ctx);
        };
        kii_set_system(&kii, kii_sys.system());
        CHECK( kii_start_push_task(&kii, 300) == KII_ERR_OK );
        CHECK( ctx.calls == 1 );
        CHECK( ctx.name == KII_TASK_NAME_MQTT );
        CHECK( ctx.entry_param == &kii );
    }

    SECTION("task not created") {
        TaskCreateCtx ctx = { EBISU_TASKC_FAIL, 0 };
        khct::cb::SystemCtx kii_sys;
        kii_sys.on_task_create = [&](const char* name, EBISU_TASK_ENTRY entry, void* entry_param) {
            return mock_task_create(name, entry, entry_param, &ctx);
        };
        kii_set_system(&kii, kii_sys.system());
        CHECK( kii_start_push_task(&kii, 300) == KII_ERR_CREATE_TASK );
        CHECK( ctx.calls == 1 );
    }

    SECTION("no task-create callback") {
        CHECK( kii_start_push_task(&kii, 300) == KII_ERR_CREATE_TASK );
    }
}

// The task uses all of this without checking, so one left out crashed it
// whenever it got there, after kii_start_push_task() had returned KII_ERR_OK.
TEST_CASE( "kii_start_push_task needs what the task uses" ) {
    char buff[64];
    kii_t kii;
    kii_init(&kii);
    khct::set_push_needs(&kii, buff, sizeof(buff));
    TaskCreateCtx ctx = { EBISU_TASKC_OK, 0 };
    khct::cb::SystemCtx kii_sys;
    kii_sys.on_task_create = [&](const char* name, EBISU_TASK_ENTRY entry, void* entry_param) {
        return mock_task_create(name, entry, entry_param, &ctx);
    };
    kii_set_system(&kii, kii_sys.system());
    khct::cb::SockCtx http_ctx;
    kii_set_http_socket(&kii, http_ctx.socket());
    khct::cb::SockCtx mqtt_ctx;
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());

    const struct {
        const char* left_out;
        std::function<void()> unset;
    } cases[] = {
        { "delay", [&] { kii_sys.ops.delay_ms = NULL; } },
        { "HTTP socket", [&] { kii_set_http_socket(&kii, NULL); } },
        { "HTTP connect", [&] { http_ctx.ops.connect = NULL; } },
        { "HTTP send", [&] { http_ctx.ops.send = NULL; } },
        { "HTTP recv", [&] { http_ctx.ops.recv = NULL; } },
        { "HTTP close", [&] { http_ctx.ops.close = NULL; } },
        { "MQTT socket", [&] { kii_set_mqtt_socket(&kii, NULL); } },
        { "MQTT connect", [&] { mqtt_ctx.ops.connect = NULL; } },
        { "MQTT send", [&] { mqtt_ctx.ops.send = NULL; } },
        { "MQTT recv", [&] { mqtt_ctx.ops.recv = NULL; } },
        { "MQTT close", [&] { mqtt_ctx.ops.close = NULL; } },
        { "buffer", [&] { kii_set_buff(&kii, NULL, 0); } },
        { "MQTT buffer", [&] { kii_set_mqtt_buff(&kii, NULL, 0); } },
        { "message callback", [&] { kii_set_cb_mqtt_message(&kii, NULL, NULL); } },
    };
    const kii_t complete = kii;
    const ebisu_system_ops_t sys_ops = kii_sys.ops;
    const ebisu_socket_ops_t http_ops = http_ctx.ops;
    const ebisu_socket_ops_t mqtt_ops = mqtt_ctx.ops;
    for (const auto& c : cases) {
        INFO("left out " << c.left_out);
        c.unset();
        CHECK( kii_start_push_task(&kii, 300) == KII_ERR_FAIL );
        CHECK( ctx.calls == 0 );
        kii = complete;
        kii_sys.ops = sys_ops;
        http_ctx.ops = http_ops;
        mqtt_ctx.ops = mqtt_ops;
    }

    CHECK( kii_start_push_task(&kii, 300) == KII_ERR_OK );
    CHECK( ctx.calls == 1 );
}

static bool stop_task(void*, void*) {
    return false;
}

struct RestartCtx {
    kii_t* kii;
    kii_code_t result;
};

static void restart_on_exit(void*, void* userdata) {
    RestartCtx* ctx = (RestartCtx*)userdata;
    ctx->result = kii_start_push_task(ctx->kii, 300);
}

// A second task would share the first's sockets and buffers. Its exit is where
// an application gets to start it again.
TEST_CASE( "kii_start_push_task refuses while its task is running" ) {
    char buff[64];
    kii_t kii;
    kii_init(&kii);
    khct::set_push_needs(&kii, buff, sizeof(buff));
    kii_set_cb_task_continue(&kii, stop_task, NULL);
    TaskCreateCtx ctx = { EBISU_TASKC_OK, 0 };
    khct::cb::SystemCtx kii_sys;
    kii_sys.on_task_create = [&](const char* name, EBISU_TASK_ENTRY entry, void* entry_param) {
        return mock_task_create(name, entry, entry_param, &ctx);
    };
    kii_set_system(&kii, kii_sys.system());

    SECTION("until it has ended") {
        REQUIRE( kii_start_push_task(&kii, 300) == KII_ERR_OK );

        CHECK( kii_start_push_task(&kii, 60) == KII_ERR_FAIL );
        CHECK( ctx.calls == 1 );
        CHECK( kii._keep_alive_interval == 300 );

        ctx.entry(ctx.entry_param);
        CHECK( kii_start_push_task(&kii, 60) == KII_ERR_OK );
        CHECK( ctx.calls == 2 );
        CHECK( kii._keep_alive_interval == 60 );
    }

    SECTION("but not from its exit callback") {
        RestartCtx restart = { &kii, KII_ERR_FAIL };
        kii_set_cb_task_exit(&kii, restart_on_exit, &restart);
        REQUIRE( kii_start_push_task(&kii, 300) == KII_ERR_OK );

        ctx.entry(ctx.entry_param);
        CHECK( restart.result == KII_ERR_OK );
        CHECK( ctx.calls == 2 );
    }

    SECTION("but not after a task that could not be created") {
        ctx.result = EBISU_TASKC_FAIL;
        REQUIRE( kii_start_push_task(&kii, 300) == KII_ERR_CREATE_TASK );

        ctx.result = EBISU_TASKC_OK;
        CHECK( kii_start_push_task(&kii, 300) == KII_ERR_OK );
    }

    SECTION("but not after kii_init()") {
        REQUIRE( kii_start_push_task(&kii, 300) == KII_ERR_OK );

        kii_init(&kii);
        CHECK( kii._push_task_running == false );
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

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, kii_buff_size);
    kii_set_mqtt_buff(&kii, mqtt_buff, mqtt_buff_size);

    khct::cb::SockCtx http_ctx;
    kii_set_http_socket(&kii, http_ctx.socket());
    kii_set_system(&kii, khct::cb::delay_only_system());

    http_ctx.on_connect = [](void* socket_context, const char* host, uint16_t port) {
        return EBISU_SOCK_OK;
    };
    http_ctx.on_send = [](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_close = [](void* socket_ctx) {
        return EBISU_SOCK_OK;
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
            return EBISU_SOCK_OK;
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

static ebisu_task_code_t count_task_create(const char* name, EBISU_TASK_ENTRY entry, void* entry_param, void* userdata)
{
    ++*(int*)userdata;
    return EBISU_TASKC_OK;
}

// CONNECT carries the keep-alive in 16 bits, and it was written there with no
// check, so 70000 reached the broker as 4464 while the task pinged every 70000
// seconds, and the broker dropped the connection in between.
TEST_CASE( "MQTT keep-alive interval range" ) {
    char mqtt_buff[1024];

    kii_t kii;
    kii_init(&kii);
    khct::set_push_needs(&kii, mqtt_buff, sizeof(mqtt_buff));
    int call_task_create = 0;
    khct::cb::SystemCtx kii_sys;
    kii_sys.on_task_create = [&](const char* name, EBISU_TASK_ENTRY entry, void* entry_param) {
        return count_task_create(name, entry, entry_param, &call_task_create);
    };
    kii_set_system(&kii, kii_sys.system());

    khct::cb::SockCtx mqtt_ctx;
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());
    std::string sent;
    mqtt_ctx.on_send = [&sent](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        sent.append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
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
            CHECK( kii_start_push_task(&kii, c.interval) == KII_ERR_OK );
            CHECK( call_task_create == 1 );
            // The callback runs no task, which would have said so when it ended.
            kii._push_task_running = false;

            mqtt_state_t state;
            _init_mqtt_state(&kii, &state);
            strcpy(state.endpoint.topic, "dummyTopic");
            strcpy(state.endpoint.username, "dummyUser");
            strcpy(state.endpoint.password, "dummyPassword");
            state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
            sent.clear();
            _mqtt_state_send_connect(&state);
            REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_CONNACK );
            // After the fixed header, MQTT 3.1.1's protocol name, level and
            // flags.
            REQUIRE( sent.length() > 12 );
            CHECK( sent.substr(10, 2) == c.bytes );
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
            CHECK( kii_start_push_task(&kii, interval) == KII_ERR_FAIL );
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
    kii_set_system(&kii, khct::cb::delay_only_system());

    khct::cb::SockCtx mqtt_ctx;
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());
    int call_send = 0;
    mqtt_ctx.on_send = [&call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        ++call_send;
        REQUIRE( std::string(buffer, length) == std::string("\xc0\x00", 2) );
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };

    SECTION("after how long") {
        int call_recv = 0;
        mqtt_ctx.on_recv = [&call_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            ++call_recv;
            return EBISU_SOCK_FAIL; // the receive timed out
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
            return EBISU_SOCK_FAIL; // the receive timed out
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
            REQUIRE( state.pingresp_received == true );
            REQUIRE( served == 2 );
        }
    }
}

namespace {

struct ExitCtx {
    // Whether the task is to go on in the state it is about to handle.
    std::function<bool(kii_mqtt_task_state)> keep_going;
    int closes = 0;
    bool exited = false;
    int closes_at_exit = -1;
    kii_mqtt_task_info exit_info;
};

bool exit_test_continue(void* task_info, void* userdata)
{
    ExitCtx* ctx = (ExitCtx*)userdata;
    kii_mqtt_task_state state = ((kii_mqtt_task_info*)task_info)->task_state;
    return ctx->keep_going(state);
}

void exit_test_exit(void* task_info, void* userdata)
{
    ExitCtx* ctx = (ExitCtx*)userdata;
    ctx->exited = true;
    ctx->closes_at_exit = ctx->closes;
    ctx->exit_info = *(kii_mqtt_task_info*)task_info;
}

std::string http_response(const std::string& status_line, const std::string& body)
{
    return "HTTP/1.1 " + status_line + "\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body;
}

// Whether what was sent ends with DISCONNECT.
bool ends_with_disconnect(const std::string& sent)
{
    const std::string disconnect("\xe0\x00", 2);
    return sent.size() >= disconnect.size() &&
        sent.compare(sent.size() - disconnect.size(), disconnect.size(), disconnect) == 0;
}

}

// The task used to leave its MQTT connection open when it ended, discontinued
// or on an error, and the application could not close it: it cannot tell
// whether the task has one open. The task now closes it, before calling the
// exit callback, and only if it is open. On one the broker accepted, it says
// DISCONNECT first, which it never used to. This runs the whole task,
// mqtt_start_task(), as the thread made for it does.
TEST_CASE( "The push task closes its MQTT connection when it ends" ) {
    char buff[1024];
    char mqtt_buff[1024];

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, buff, sizeof(buff));
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii_set_system(&kii, khct::cb::delay_only_system());
    strcpy(kii._author.author_id, "th.1");
    strcpy(kii._author.access_token, "tok");

    // Installing push and getting the endpoint, each on a connection of its
    // own, answered in turn.
    std::deque<std::string> http_responses = {
        http_response("201 Created",
                "{\"installationID\":\"ins.1\",\"installationRegistrationID\":\"reg.1\"}"),
        http_response("200 OK",
                "{\"installationID\":\"ins.1\",\"username\":\"user\",\"password\":\"pass\","
                "\"mqttTopic\":\"topic\",\"host\":\"broker\",\"portTCP\":1883,"
                "\"portSSL\":8883,\"X-MQTT-TTL\":2147483647}"),
    };
    std::string http_resp;
    size_t http_resp_pos = 0;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&](void*, const char*, unsigned int) {
        if (http_responses.empty()) {
            return EBISU_SOCK_FAIL;
        }
        http_resp = http_responses.front();
        http_responses.pop_front();
        http_resp_pos = 0;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_recv = [&http_resp, &http_resp_pos](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = http_resp.copy(buffer, length_to_read, http_resp_pos);
        http_resp_pos += *out_actual_length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return EBISU_SOCK_OK; };
    kii_set_http_socket(&kii, http_ctx.socket());

    ExitCtx ctx;
    kii_set_cb_task_continue(&kii, exit_test_continue, &ctx);
    kii_set_cb_task_exit(&kii, exit_test_exit, &ctx);

    // The broker accepts the connection and the subscription, then sends
    // nothing more: each receive after that times out.
    int connects = 0;
    bool connect_ok = true;
    khct::cb::SockCtx mqtt_ctx;
    mqtt_ctx.on_connect = [&connects, &connect_ok](void*, const char*, unsigned int) {
        ++connects;
        return connect_ok ? EBISU_SOCK_OK : EBISU_SOCK_FAIL;
    };
    // Everything sent on the MQTT socket, and what had been at each close.
    std::string sent;
    std::vector<std::string> sent_at_closes;
    mqtt_ctx.on_send = [&sent](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        sent.append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    mqtt_ctx.on_close = [&ctx, &sent, &sent_at_closes](void*) {
        ++ctx.closes;
        sent_at_closes.push_back(sent);
        return EBISU_SOCK_OK;
    };
    const std::string connack_suback("\x20\x02\x00\x00" "\x90\x03\x00\x01\x00", 9);
    size_t served = 0;
    serve_stream(mqtt_ctx, connack_suback, served);
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());

    SECTION("discontinued while connected") {
        int timeouts = 0;
        ctx.keep_going = [&timeouts](kii_mqtt_task_state state) {
            // Ready, and a receive has timed out.
            return !(state == KII_MQTT_ST_RECV_READY && ++timeouts > 1);
        };

        mqtt_start_task(&kii);

        REQUIRE( ctx.exited );
        CHECK( ctx.exit_info.task_state == KII_MQTT_ST_DISCONTINUED );
        CHECK( ctx.exit_info.error == KII_MQTT_ERR_OK );
        CHECK( connects == 1 );
        CHECK( ctx.closes_at_exit == 1 );
        CHECK( ctx.closes == 1 );
        // DISCONNECT, after SUBSCRIBE, before the close, and once.
        REQUIRE( sent_at_closes.size() == 1 );
        CHECK( ends_with_disconnect(sent_at_closes[0]) );
        CHECK( sent == sent_at_closes[0] );
        CHECK( sent.find(std::string("\xe0\x00", 2)) == sent.size() - 2 );
    }

    SECTION("discontinued before connecting") {
        ctx.keep_going = [](kii_mqtt_task_state state) {
            return state != KII_MQTT_ST_SOCK_CONNECT;
        };

        mqtt_start_task(&kii);

        REQUIRE( ctx.exited );
        CHECK( ctx.exit_info.task_state == KII_MQTT_ST_DISCONTINUED );
        CHECK( connects == 0 );
        CHECK( ctx.closes == 0 );
        CHECK( sent.empty() );
    }

    // The connection is closed to reconnect, and connecting again fails: it
    // is not closed a second time.
    SECTION("discontinued while connecting again after closing") {
        served = 0;
        serve_stream(mqtt_ctx, "", served); // no CONNACK
        ctx.keep_going = [&connects, &connect_ok](kii_mqtt_task_state state) {
            connect_ok = connects == 0;
            return connects < 2;
        };

        mqtt_start_task(&kii);

        REQUIRE( ctx.exited );
        CHECK( ctx.exit_info.task_state == KII_MQTT_ST_DISCONTINUED );
        CHECK( connects == 2 );
        CHECK( ctx.closes == 1 );
        // The broker never accepted CONNECT.
        CHECK_FALSE( ends_with_disconnect(sent) );
    }

    // The broker ends a connection it accepted, and the task reconnects. It
    // is discontinued before sending CONNECT again: neither close follows a
    // DISCONNECT, the first as that connection is gone, the second as the
    // broker hasn't accepted this one.
    SECTION("closing a connection the broker ended, or hasn't accepted") {
        mqtt_ctx.on_recv = [&connack_suback, &served](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            // Then the end of the stream.
            *out_actual_length = connack_suback.copy(buffer, length_to_read, served);
            served += *out_actual_length;
            return EBISU_SOCK_OK;
        };
        ctx.keep_going = [&connects](kii_mqtt_task_state state) {
            return !(connects == 2 && state == KII_MQTT_ST_SEND_CONNECT);
        };

        mqtt_start_task(&kii);

        REQUIRE( ctx.exited );
        CHECK( ctx.exit_info.task_state == KII_MQTT_ST_DISCONTINUED );
        CHECK( connects == 2 );
        REQUIRE( sent_at_closes.size() == 2 );
        CHECK_FALSE( ends_with_disconnect(sent_at_closes[0]) );
        CHECK( sent_at_closes[1] == sent_at_closes[0] );
        CHECK( sent == sent_at_closes[1] );
    }

    SECTION("exiting on an error while connected") {
        // Too small for CONNECT, which SEND_CONNECT finds once connected.
        kii_set_mqtt_buff(&kii, mqtt_buff, 16);
        ctx.keep_going = [](kii_mqtt_task_state) { return true; };

        mqtt_start_task(&kii);

        REQUIRE( ctx.exited );
        CHECK( ctx.exit_info.task_state == KII_MQTT_ST_ERR_EXIT );
        CHECK( ctx.exit_info.error == KII_MQTT_ERR_INSUFFICIENT_BUFF );
        CHECK( connects == 1 );
        CHECK( ctx.closes_at_exit == 1 );
        CHECK( sent.empty() );
    }
}

// Onboarding installs push and returns the MQTT endpoint, so the push task need
// not install push and fetch the endpoint from Kii Cloud as well.
TEST_CASE( "The push task connects with the endpoint onboarding returned" ) {
    char kii_buff[2048];
    char mqtt_buff[1024];

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii_set_system(&kii, khct::cb::delay_only_system());
    khct::HttpScript http;
    http.attach(&kii._khc);

    const std::string endpoint =
        "{\"installationID\":\"ins.1\",\"username\":\"user.1\",\"password\":\"pass.1\","
        "\"mqttTopic\":\"topic.1\",\"host\":\"mqtt.example.com\",\"portTCP\":1883,"
        "\"portSSL\":8883,\"portWS\":12470,\"portWSS\":12473,\"ttl\":2147483647}";
    // Onboarding: a single request, authorized with the app's ID.
    auto onboard = [&](const std::string& onboarding_body) {
        http.responses.push_back(khct::http_response(200, onboarding_body));
        return kii_ti_onboard(&kii, "vendor.1", "password", NULL, NULL, NULL, NULL);
    };

    SECTION("it connects with it, to the host and port it gave") {
        REQUIRE( onboard("{\"accessToken\":\"tok\",\"thingID\":\"th.1\",\"mqttEndpoint\":" +
                    endpoint + "}") == KII_ERR_OK );
        CHECK( std::string(kii._author.access_token) == "tok" );

        mqtt_state_t state;
        _init_mqtt_state(&kii, &state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        CHECK( std::string(state.ins_id.id) == "ins.1" );
        CHECK( std::string(state.endpoint.username) == "user.1" );
        CHECK( std::string(state.endpoint.password) == "pass.1" );
        CHECK( std::string(state.endpoint.topic) == "topic.1" );
        CHECK( state.endpoint.port_tcp == 1883 );

        khct::cb::SockCtx mqtt_ctx;
        std::string host;
        uint16_t port = 0;
        mqtt_ctx.on_connect = [&host, &port](void*, const char* h, uint16_t p) {
            host = h;
            port = p;
            return EBISU_SOCK_OK;
        };
        kii_set_mqtt_socket(&kii, mqtt_ctx.socket());
        _mqtt_state_sock_connect(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_SEND_CONNECT );
        CHECK( host == "mqtt.example.com" );
        CHECK( port == 8883 );
        // Only the onboarding went to the server.
        CHECK( http.requests.size() == 1 );
    }

    SECTION("credentials the broker refuses are fetched again for the installation onboarding made") {
        REQUIRE( onboard("{\"accessToken\":\"tok\",\"thingID\":\"th.1\",\"mqttEndpoint\":" +
                    endpoint + "}") == KII_ERR_OK );
        mqtt_state_t state;
        _init_mqtt_state(&kii, &state);
        // Where a refused CONNACK sends the task.
        state.info.task_state = KII_MQTT_ST_GET_ENDPOINT;
        http.responses.push_back(mqtt_endpoint_response("1883", "8883", "2147483647"));
        _mqtt_state_get_endpoint(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        REQUIRE( http.requests.size() == 2 );
        CHECK( khct::contains(http.requests[1],
                    "GET /api/apps/dummyAppID/installations/ins.1/mqtt-endpoint ") );
    }

    SECTION("without an endpoint it installs push and fetches one") {
        const struct {
            const char* what;
            std::string body;
        } cases[] = {
            { "MQTT not enabled", "{\"accessToken\":\"tok\",\"thingID\":\"th.1\"}" },
            { "an endpoint lacking its SSL port",
                "{\"accessToken\":\"tok\",\"thingID\":\"th.1\",\"mqttEndpoint\":{\"installationID\":\"ins.1\","
                "\"username\":\"user.1\",\"password\":\"pass.1\",\"mqttTopic\":\"topic.1\","
                "\"host\":\"mqtt.example.com\",\"portTCP\":1883}}" },
            { "an endpoint with a port out of range",
                "{\"accessToken\":\"tok\",\"thingID\":\"th.1\",\"mqttEndpoint\":{\"installationID\":\"ins.1\","
                "\"username\":\"user.1\",\"password\":\"pass.1\",\"mqttTopic\":\"topic.1\","
                "\"host\":\"mqtt.example.com\",\"portTCP\":1883,\"portSSL\":65536}}" },
        };
        for (const auto& c : cases) {
            INFO( c.what );
            // An earlier onboarding's endpoint does not outlive the next one.
            REQUIRE( onboard("{\"accessToken\":\"tok\",\"thingID\":\"th.1\",\"mqttEndpoint\":" +
                        endpoint + "}") == KII_ERR_OK );
            REQUIRE( onboard(c.body) == KII_ERR_OK );
            CHECK( std::string(kii._author.access_token) == "tok" );
            mqtt_state_t state;
            _init_mqtt_state(&kii, &state);
            CHECK( state.info.task_state == KII_MQTT_ST_INSTALL_PUSH );
        }
    }

    SECTION("acting as another thing, it installs push and fetches an endpoint") {
        REQUIRE( onboard("{\"accessToken\":\"tok\",\"thingID\":\"th.1\",\"mqttEndpoint\":" +
                    endpoint + "}") == KII_ERR_OK );
        strcpy(kii._author.author_id, "th.2");
        mqtt_state_t state;
        _init_mqtt_state(&kii, &state);
        CHECK( state.info.task_state == KII_MQTT_ST_INSTALL_PUSH );
    }

    SECTION("an onboarding response without a token is still refused") {
        CHECK( onboard("{\"thingID\":\"th.1\",\"mqttEndpoint\":" + endpoint + "}") ==
                KII_ERR_PARSE_JSON );
    }
}

namespace {

// Stops the task after the given number of calls, and adds up its waits.
struct RetryCtx {
    int continues_left;
    uint32_t waited_ms;
    uint32_t longest_wait_ms;
    int connects;
};

bool retry_continue(void* task_info, void* userdata) {
    return ((RetryCtx*)userdata)->continues_left-- > 0;
}

void retry_delay(uint32_t ms, void* userdata) {
    RetryCtx* ctx = (RetryCtx*)userdata;
    ctx->waited_ms += ms;
    if (ms > ctx->longest_wait_ms) {
        ctx->longest_wait_ms = ms;
    }
}

ebisu_sock_code_t retry_connect_fail(void* context, const char* host, uint16_t port) {
    ++((RetryCtx*)context)->connects;
    return EBISU_SOCK_FAIL;
}

}

// A request that times out never had an answer, like one that fails on the
// network, so the push task tries it again rather than ending.
TEST_CASE( "A request of the push task that times out is tried again" ) {
    char buff[1024];
    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, buff, sizeof(buff));
    strcpy(kii._author.author_id, "th.1");
    strcpy(kii._author.access_token, "tok");
    uint64_t now = 1000;
    khct::cb::SystemCtx sys;
    sys.on_delay_ms = [&now](uint32_t ms) { now += ms; };
    sys.on_monotonic_ms = [&now]() { return now; };
    kii_set_system(&kii, sys.system());
    REQUIRE( kii_set_request_timeout(&kii, 1) == KII_ERR_OK );

    // Kii Cloud answers a byte every 100 ms.
    khct::HttpScript script;
    script.attach(&kii);
    script.responses = { khct::http_response(201, "{\"installationID\":\"ins.1\"}") };
    auto recv = script.ctx.on_recv;
    script.ctx.on_recv = [&now, recv](void* ctx, char* buffer, size_t, size_t* out_actual_length) {
        now += 100;
        return recv(ctx, buffer, 1, out_actual_length);
    };

    mqtt_state_t state;
    _init_mqtt_state(&kii, &state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_INSTALL_PUSH );
    _mqtt_state_install_push(&state);
    CHECK( state.info.task_state == KII_MQTT_ST_INSTALL_PUSH );
    CHECK( state.info.error == KII_MQTT_ERR_OK );
    CHECK( state.wait_left_ms > 0 );
}

TEST_CASE( "The waits between attempts" ) {
    kii_t kii;
    kii_init(&kii);
    strcpy(kii._author.author_id, "th.1");
    khct::cb::SockCtx mqtt_ctx;
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());
    mqtt_ctx.on_close = [](void* socket_ctx) { return EBISU_SOCK_OK; };

    mqtt_state_t state;
    _init_mqtt_state(&kii, &state);

    SECTION("double from about a second to at most five minutes") {
        uint32_t base_ms = 2000;
        for (int attempt = 0; attempt < 12; ++attempt) {
            INFO("attempt " << attempt);
            _mqtt_state_reconnect(&state);
            CHECK( state.wait_left_ms >= base_ms / 2 );
            CHECK( state.wait_left_ms <= base_ms );
            base_ms = base_ms * 2 > 300000 ? 300000 : base_ms * 2;
        }
    }

    SECTION("double up to the longest wait set") {
        REQUIRE( kii_set_mqtt_retry_wait_max(&kii, 30) == KII_ERR_OK );
        uint32_t base_ms = 2000;
        for (int attempt = 0; attempt < 8; ++attempt) {
            INFO("attempt " << attempt);
            _mqtt_state_reconnect(&state);
            CHECK( state.wait_left_ms >= base_ms / 2 );
            CHECK( state.wait_left_ms <= base_ms );
            base_ms = base_ms * 2 > 30000 ? 30000 : base_ms * 2;
        }
        CHECK( state.retry_wait_ms == 30000 );
    }

    SECTION("double past five minutes to a longer longest wait") {
        REQUIRE( kii_set_mqtt_retry_wait_max(&kii, 3600) == KII_ERR_OK );
        uint32_t base_ms = 2000;
        uint32_t longest_ms = 0;
        for (int attempt = 0; attempt < 14; ++attempt) {
            INFO("attempt " << attempt);
            _mqtt_state_reconnect(&state);
            CHECK( state.wait_left_ms >= base_ms / 2 );
            CHECK( state.wait_left_ms <= base_ms );
            longest_ms = state.wait_left_ms > longest_ms ? state.wait_left_ms : longest_ms;
            base_ms = base_ms * 2 > 3600000 ? 3600000 : base_ms * 2;
        }
        CHECK( longest_ms >= 1800000 );
        CHECK( state.retry_wait_ms == 3600000 );
    }

    SECTION("a longest wait shorter than the first cuts that short too") {
        REQUIRE( kii_set_mqtt_retry_wait_max(&kii, 1) == KII_ERR_OK );
        for (int attempt = 0; attempt < 4; ++attempt) {
            INFO("attempt " << attempt);
            _mqtt_state_reconnect(&state);
            CHECK( state.wait_left_ms >= 500 );
            CHECK( state.wait_left_ms <= 1000 );
        }
    }

    SECTION("the longest wait the setter takes") {
        CHECK( kii_set_mqtt_retry_wait_max(&kii, 4294967) == KII_ERR_OK );
        CHECK( kii._mqtt_retry_wait_max_ms == 4294967000U );
        state.retry_wait_ms = 4294967000U;
        _mqtt_state_reconnect(&state);
        CHECK( state.wait_left_ms >= 4294967000U / 2 );
        CHECK( state.retry_wait_ms == 4294967000U );
    }

    SECTION("a longest wait out of range changes nothing") {
        REQUIRE( kii_set_mqtt_retry_wait_max(&kii, 30) == KII_ERR_OK );
        CHECK( kii_set_mqtt_retry_wait_max(&kii, 0) == KII_ERR_FAIL );
        CHECK( kii_set_mqtt_retry_wait_max(&kii, 4294968) == KII_ERR_FAIL );
        CHECK( kii._mqtt_retry_wait_max_ms == 30000 );
    }

    SECTION("differ from one thing to another") {
        std::set<uint32_t> waits;
        for (int thing = 0; thing < 64; ++thing) {
            strcpy(kii._author.author_id, ("th." + std::to_string(thing)).c_str());
            _init_mqtt_state(&kii, &state);
            state.retry_wait_ms = 300000;
            _mqtt_state_reconnect(&state);
            waits.insert(state.wait_left_ms);
        }
        CHECK( waits.size() > 32 );
    }

    SECTION("do not differ between runs of one thing") {
        _mqtt_state_reconnect(&state);
        uint32_t wait_ms = state.wait_left_ms;
        _init_mqtt_state(&kii, &state);
        _mqtt_state_reconnect(&state);
        CHECK( state.wait_left_ms == wait_ms );
    }
}

namespace {

struct CredentialsCtx {
    int calls = 0;
    bool give = true;
    bool unterminated = false;
};

bool give_credentials(kii_mqtt_endpoint_t* endpoint, void* userdata)
{
    CredentialsCtx* ctx = (CredentialsCtx*)userdata;
    ++ctx->calls;
    if (ctx->unterminated) {
        memset(endpoint->password, 'p', sizeof(endpoint->password));
    } else {
        strcpy(endpoint->password, "newPassword");
    }
    return ctx->give;
}

}

// The task could only start with push's own flow: an application that had a
// broker's credentials some other way had to write the task's private fields.
TEST_CASE( "The MQTT task with an endpoint of the application's" ) {
    char mqtt_buff[1024];
    kii_t kii;
    kii_init(&kii);
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii_set_cb_mqtt_message(&kii, khct::cb::cb_push, NULL);
    strcpy(kii._author.author_id, "th.1");
    int tasks = 0;
    khct::cb::SystemCtx sys;
    sys.on_task_create = [&tasks](const char*, EBISU_TASK_ENTRY, void*) {
        ++tasks;
        return EBISU_TASKC_OK;
    };
    kii_set_system(&kii, sys.system());
    std::string sent;
    bool connect_ok = true;
    khct::cb::SockCtx mqtt_ctx;
    mqtt_ctx.on_connect = [&connect_ok](void*, const char*, uint16_t) {
        return connect_ok ? EBISU_SOCK_OK : EBISU_SOCK_FAIL;
    };
    mqtt_ctx.on_send = [&sent](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        sent.append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    mqtt_ctx.on_close = [](void*) { return EBISU_SOCK_OK; };
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());
    size_t served = 0;

    kii_mqtt_endpoint_t endpoint;
    memset(&endpoint, 0, sizeof(endpoint));
    strcpy(endpoint.host, "broker.example.com");
    strcpy(endpoint.topic, "devices/1");
    strcpy(endpoint.username, "user");
    strcpy(endpoint.password, "pass");
    endpoint.port_ssl = 8883;

    SECTION("starts with neither the HTTP socket nor the buffer, with a copy") {
        REQUIRE( kii_start_mqtt_task_with_endpoint(&kii, &endpoint, 60) == KII_ERR_OK );
        CHECK( tasks == 1 );
        strcpy(endpoint.host, "changed");
        CHECK( strcmp(kii._mqtt_endpoint.host, "broker.example.com") == 0 );
        CHECK( kii._keep_alive_interval == 60 );
    }

    SECTION("refuses an endpoint that isn't one, starting nothing") {
        kii_mqtt_endpoint_t bad = endpoint;
        memset(bad.username, 'u', sizeof(bad.username));
        CHECK( kii_start_mqtt_task_with_endpoint(&kii, &bad, 60) == KII_ERR_TOO_LARGE_DATA );
        bad = endpoint;
        memset(bad.host, 'h', sizeof(bad.host));
        CHECK( kii_start_mqtt_task_with_endpoint(&kii, &bad, 60) == KII_ERR_TOO_LARGE_DATA );
        bad = endpoint;
        bad.host[0] = '\0';
        CHECK( kii_start_mqtt_task_with_endpoint(&kii, &bad, 60) == KII_ERR_FAIL );
        bad = endpoint;
        bad.topic[0] = '\0';
        CHECK( kii_start_mqtt_task_with_endpoint(&kii, &bad, 60) == KII_ERR_FAIL );
        CHECK( kii_start_mqtt_task_with_endpoint(&kii, NULL, 60) == KII_ERR_FAIL );
        CHECK( kii_start_mqtt_task_with_endpoint(&kii, &endpoint, 65536) == KII_ERR_FAIL );
        kii_set_cb_mqtt_message(&kii, NULL, NULL);
        CHECK( kii_start_mqtt_task_with_endpoint(&kii, &endpoint, 60) == KII_ERR_FAIL );
        CHECK( tasks == 0 );
    }

    SECTION("connects with it, whatever onboarding returned") {
        strcpy(kii._onboarded_thing_id, "th.1");
        strcpy(kii._onboarded_endpoint.host, "kii-broker");
        REQUIRE( kii_start_mqtt_task_with_endpoint(&kii, &endpoint, 60) == KII_ERR_OK );
        mqtt_state_t state;
        _init_mqtt_state(&kii, &state);
        CHECK( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        CHECK( strcmp(state.endpoint.host, "broker.example.com") == 0 );

        // A push task started after it uses push's own again.
        kii._push_task_running = false;
        char buff[256];
        kii_set_buff(&kii, buff, sizeof(buff));
        khct::cb::SockCtx http_ctx;
        kii_set_http_socket(&kii, http_ctx.socket());
        REQUIRE( kii_start_push_task(&kii, 60) == KII_ERR_OK );
        _init_mqtt_state(&kii, &state);
        CHECK( strcmp(state.endpoint.host, "kii-broker") == 0 );
    }

    SECTION("with the client identifier set, or the topic") {
        REQUIRE( kii_start_mqtt_task_with_endpoint(&kii, &endpoint, 60) == KII_ERR_OK );
        mqtt_state_t state;
        _init_mqtt_state(&kii, &state);
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        _mqtt_state_send_connect(&state);
        CHECK( sent.find(std::string("\x00\x09" "devices/1" "\x00\x04" "user", 17)) != std::string::npos );

        REQUIRE( kii_set_mqtt_client_id(&kii, "client-7") == KII_ERR_OK );
        sent.clear();
        _mqtt_state_send_connect(&state);
        CHECK( sent.find(std::string("\x00\x08" "client-7" "\x00\x04" "user", 16)) != std::string::npos );
        CHECK( sent.find("devices/1") == std::string::npos );

        const std::string long_id(65536, 'c');
        CHECK( kii_set_mqtt_client_id(&kii, long_id.c_str()) == KII_ERR_TOO_LARGE_DATA );
        CHECK( kii_set_mqtt_client_id(&kii, NULL) == KII_ERR_OK );
        sent.clear();
        _mqtt_state_send_connect(&state);
        CHECK( sent.find("devices/1") != std::string::npos );
    }

    SECTION("asks for new credentials when the broker refuses them") {
        CredentialsCtx creds;
        kii_set_cb_mqtt_credentials(&kii, give_credentials, &creds);
        REQUIRE( kii_start_mqtt_task_with_endpoint(&kii, &endpoint, 60) == KII_ERR_OK );
        mqtt_state_t state;
        _init_mqtt_state(&kii, &state);
        const std::string refused("\x20\x02\x00\x05", 4);

        SECTION("and connects with them") {
            serve_stream(mqtt_ctx, refused, served);
            _mqtt_state_recv_connack(&state);
            CHECK( creds.calls == 1 );
            CHECK( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
            CHECK( state.wait_left_ms > 0 );
            CHECK( strcmp(state.endpoint.password, "newPassword") == 0 );
        }

        SECTION("or ends without") {
            creds.give = false;
            serve_stream(mqtt_ctx, refused, served);
            _mqtt_state_recv_connack(&state);
            CHECK( creds.calls == 1 );
            CHECK( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
            CHECK( state.info.error == KII_MQTT_ERR_CREDENTIALS );
        }

        SECTION("or ends with ones that aren't") {
            creds.unterminated = true;
            serve_stream(mqtt_ctx, refused, served);
            _mqtt_state_recv_connack(&state);
            CHECK( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
            CHECK( state.info.error == KII_MQTT_ERR_CREDENTIALS );
        }

        SECTION("or ends without a callback") {
            kii_set_cb_mqtt_credentials(&kii, NULL, NULL);
            serve_stream(mqtt_ctx, refused, served);
            _mqtt_state_recv_connack(&state);
            CHECK( creds.calls == 0 );
            CHECK( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
            CHECK( state.info.error == KII_MQTT_ERR_CREDENTIALS );
        }
    }

    SECTION("isn't asked of a push task, which gets them from Kii Cloud") {
        CredentialsCtx creds;
        kii_set_cb_mqtt_credentials(&kii, give_credentials, &creds);
        mqtt_state_t state;
        _init_mqtt_state(&kii, &state);
        state.endpoint = endpoint;
        serve_stream(mqtt_ctx, std::string("\x20\x02\x00\x05", 4), served);
        _mqtt_state_recv_connack(&state);
        CHECK( creds.calls == 0 );
        CHECK( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );
    }

    SECTION("goes on trying a broker it can't reach") {
        connect_ok = false;
        REQUIRE( kii_start_mqtt_task_with_endpoint(&kii, &endpoint, 60) == KII_ERR_OK );
        mqtt_state_t state;
        _init_mqtt_state(&kii, &state);
        for (int attempt = 0; attempt < 10; ++attempt) {
            INFO( "attempt " << attempt );
            _mqtt_state_sock_connect(&state);
            CHECK( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        }
    }
}

TEST_CASE( "A broker that cannot be reached" ) {
    kii_t kii;
    kii_init(&kii);
    strcpy(kii._author.author_id, "th.1");
    RetryCtx ctx = {};
    khct::cb::SockCtx mqtt_ctx;
    mqtt_ctx.on_connect = [&ctx](void*, const char* host, uint16_t port) {
        return retry_connect_fail(&ctx, host, port);
    };
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());
    khct::cb::SystemCtx sys;
    sys.on_delay_ms = [&ctx](uint32_t ms) { retry_delay(ms, &ctx); };
    kii_set_system(&kii, sys.system());

    mqtt_state_t state;
    _init_mqtt_state(&kii, &state);
    strcpy(state.endpoint.host, "mqtt.example.com");
    state.info.task_state = KII_MQTT_ST_SOCK_CONNECT;

    for (int attempt = 1; attempt < 8; ++attempt) {
        INFO("attempt " << attempt);
        _mqtt_state_sock_connect(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        CHECK( state.wait_left_ms > 0 );
    }
    _mqtt_state_sock_connect(&state);
    CHECK( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );
    CHECK( state.info.error == KII_MQTT_ERR_OK );
    CHECK( state.connect_failures == 0 );
}

// A wait of minutes in a single delay would leave the application waiting that
// long for a stopped task.
TEST_CASE( "The task can be stopped during a wait" ) {
    kii_t kii;
    kii_init(&kii);
    strcpy(kii._author.author_id, "th.1");
    strcpy(kii._onboarded_thing_id, "th.1");
    strcpy(kii._onboarded_endpoint.host, "mqtt.example.com");

    char kii_buff[512];
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));

    RetryCtx ctx = {};
    ctx.continues_left = 1000;
    khct::cb::SockCtx mqtt_ctx;
    mqtt_ctx.on_connect = [&ctx](void*, const char* host, uint16_t port) {
        return retry_connect_fail(&ctx, host, port);
    };
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());
    khct::cb::SystemCtx sys;
    sys.on_delay_ms = [&ctx](uint32_t ms) { retry_delay(ms, &ctx); };
    kii_set_system(&kii, sys.system());
    kii_set_cb_task_continue(&kii, retry_continue, &ctx);

    // The broker is out of reach, and Kii Cloud says to try later.
    khct::cb::SockCtx http_ctx;
    kii_set_http_socket(&kii, http_ctx.socket());
    http_ctx.on_connect = [](void* socket_context, const char* host, uint16_t port) {
        return EBISU_SOCK_OK;
    };
    http_ctx.on_send = [](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    const std::string resp = "HTTP/1.1 503 Service Unavailable\r\n\r\n";
    size_t resp_pos = 0;
    http_ctx.on_recv = [&](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = resp.copy(buffer, length_to_read, resp_pos);
        resp_pos += *out_actual_length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_close = [&resp_pos](void* socket_ctx) {
        resp_pos = 0;
        return EBISU_SOCK_OK;
    };

    mqtt_start_task(&kii);

    CHECK( ctx.continues_left < 0 );
    CHECK( ctx.longest_wait_ms <= 1000 );
    // The task spent its time waiting, at long intervals by now.
    CHECK( ctx.waited_ms > 900 * 1000 );
    CHECK( ctx.connects == 8 );
}

// The continue callback saw the task's state and the exit callback the error
// that ended it, and nothing said why the task reconnected, waited or dropped
// a message. The event callback is told.
TEST_CASE( "The MQTT task tells the event callback why" ) {
    char buff[1024];
    char mqtt_buff[256];
    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, buff, sizeof(buff));
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii_set_system(&kii, khct::cb::delay_only_system());
    kii._keep_alive_interval = 300;
    strcpy(kii._author.author_id, "th.1");
    strcpy(kii._author.access_token, "tok");

    std::vector<std::pair<kii_mqtt_event_t, uint32_t>> events;
    kii_set_cb_mqtt_event(&kii, [](kii_mqtt_event_t event, uint32_t detail, void* userdata) {
        ((std::vector<std::pair<kii_mqtt_event_t, uint32_t>>*)userdata)->emplace_back(event, detail);
    }, &events);
    auto told = [&events](kii_mqtt_event_t event, uint32_t detail) {
        return events == std::vector<std::pair<kii_mqtt_event_t, uint32_t>>({ { event, detail } });
    };

    // Kii Cloud answers with http_resp, or doesn't connect without one.
    std::string http_resp;
    size_t http_pos = 0;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&http_resp](void*, const char*, uint16_t) {
        return http_resp.empty() ? EBISU_SOCK_FAIL : EBISU_SOCK_OK;
    };
    http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_recv = [&http_resp, &http_pos](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = http_resp.copy(buffer, length_to_read, http_pos);
        http_pos += *out_actual_length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return EBISU_SOCK_OK; };
    kii_set_http_socket(&kii, http_ctx.socket());

    bool send_ok = true;
    khct::cb::SockCtx mqtt_ctx;
    mqtt_ctx.on_connect = [](void*, const char*, uint16_t) { return EBISU_SOCK_OK; };
    mqtt_ctx.on_send = [&send_ok](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return send_ok ? EBISU_SOCK_OK : EBISU_SOCK_FAIL;
    };
    mqtt_ctx.on_close = [](void*) { return EBISU_SOCK_OK; };
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());
    size_t served = 0;

    khct::cb::PushCtx push_ctx;
    push_ctx.on_push = [](const kii_mqtt_message_t*) {};
    kii_set_cb_mqtt_message(&kii, khct::cb::cb_push, &push_ctx);

    mqtt_state_t state;
    _init_mqtt_state(&kii, &state);
    strcpy(state.ins_id.id, "ins.1");
    strcpy(state.endpoint.host, "broker");
    strcpy(state.endpoint.topic, "dummyTopic");
    strcpy(state.endpoint.username, "dummyUser");
    strcpy(state.endpoint.password, "dummyPassword");

    SECTION("installing push refused, with the status") {
        http_resp = http_response("503 Service Unavailable", "{}");
        _mqtt_state_install_push(&state);
        CHECK( told(KII_MQTT_EVENT_INSTALL_FAILED, 503) );
    }

    SECTION("installing push without a response") {
        _mqtt_state_install_push(&state);
        CHECK( told(KII_MQTT_EVENT_INSTALL_FAILED, 0) );
    }

    SECTION("getting the endpoint refused") {
        http_resp = http_response("404 Not Found", "{}");
        _mqtt_state_get_endpoint(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
        CHECK( told(KII_MQTT_EVENT_ENDPOINT_FAILED, 404) );
    }

    SECTION("connecting failed, counted") {
        mqtt_ctx.on_connect = [](void*, const char*, uint16_t) { return EBISU_SOCK_FAIL; };
        _mqtt_state_sock_connect(&state);
        _mqtt_state_sock_connect(&state);
        CHECK( events == std::vector<std::pair<kii_mqtt_event_t, uint32_t>>({
                { KII_MQTT_EVENT_CONNECT_FAILED, 1 }, { KII_MQTT_EVENT_CONNECT_FAILED, 2 } }) );
    }

    SECTION("sending CONNECT failed") {
        send_ok = false;
        _mqtt_state_send_connect(&state);
        CHECK( told(KII_MQTT_EVENT_SEND_FAILED, KII_MQTT_ST_SEND_CONNECT) );
    }

    SECTION("CONNACK accepting, and refusing, with its code") {
        serve_stream(mqtt_ctx, std::string("\x20\x02\x00\x00", 4), served);
        _mqtt_state_recv_connack(&state);
        CHECK( told(KII_MQTT_EVENT_CONNACK, 0) );
        events.clear();
        served = 0;
        serve_stream(mqtt_ctx, std::string("\x20\x02\x00\x05", 4), served);
        _mqtt_state_recv_connack(&state);
        CHECK( told(KII_MQTT_EVENT_CONNACK, 5) );
    }

    SECTION("something else in place of CONNACK") {
        serve_stream(mqtt_ctx, std::string("\x90\x03\x00\x01\x00", 5), served);
        _mqtt_state_recv_connack(&state);
        CHECK( told(KII_MQTT_EVENT_UNEXPECTED_PACKET, 0x90) );
    }

    SECTION("no CONNACK") {
        serve_stream(mqtt_ctx, "", served);
        _mqtt_state_recv_connack(&state);
        CHECK( told(KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_CONNACK) );
    }

    SECTION("sending SUBSCRIBE failed") {
        send_ok = false;
        _mqtt_state_send_subscribe(&state);
        CHECK( told(KII_MQTT_EVENT_SEND_FAILED, KII_MQTT_ST_SEND_SUBSCRIBE) );
    }

    SECTION("SUBACK granting, and refusing, with its code") {
        kii_set_mqtt_qos(&kii, KII_MQTT_QOS_1);
        serve_stream(mqtt_ctx, std::string("\x90\x03\x00\x01\x01", 5), served);
        _mqtt_state_recv_suback(&state);
        CHECK( told(KII_MQTT_EVENT_SUBACK, 1) );
        events.clear();
        served = 0;
        serve_stream(mqtt_ctx, std::string("\x90\x03\x00\x01\x80", 5), served);
        _mqtt_state_recv_suback(&state);
        CHECK( told(KII_MQTT_EVENT_SUBACK, 0x80) );
    }

    SECTION("SUBACK for another subscription") {
        serve_stream(mqtt_ctx, std::string("\x90\x03\x00\x02\x00", 5), served);
        _mqtt_state_recv_suback(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
        CHECK( told(KII_MQTT_EVENT_UNEXPECTED_PACKET, 0x90) );
    }

    SECTION("the broker closing the connection") {
        mqtt_ctx.on_recv = [](void*, char*, size_t, size_t* out_actual_length) {
            *out_actual_length = 0;
            return EBISU_SOCK_OK;
        };
        _mqtt_state_recv_ready(&state);
        CHECK( told(KII_MQTT_EVENT_CLOSED_BY_BROKER, 0) );
    }

    SECTION("a packet cut off while waiting") {
        serve_stream(mqtt_ctx, std::string("\x30", 1), served);
        _mqtt_state_recv_ready(&state);
        CHECK( told(KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_READY) );
    }

    SECTION("the connection lost while waiting") {
        serve_stream(mqtt_ctx, "", served);
        mqtt_ctx.on_recv_timed_out = [](void*) { return false; };
        _mqtt_state_recv_ready(&state);
        CHECK( told(KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_READY) );
    }

    SECTION("a receive timing out while waiting is no event") {
        serve_stream(mqtt_ctx, "", served);
        _mqtt_state_recv_ready(&state);
        CHECK( events.empty() );
    }

    SECTION("an unanswered PINGREQ, with the keep-alive") {
        state.pingresp_received = false;
        _mqtt_state_send_pingreq(&state);
        CHECK( told(KII_MQTT_EVENT_PING_TIMEOUT, 300) );
    }

    SECTION("sending PINGREQ failed") {
        send_ok = false;
        _mqtt_state_send_pingreq(&state);
        CHECK( told(KII_MQTT_EVENT_SEND_FAILED, KII_MQTT_ST_SEND_PINGREQ) );
    }

    SECTION("a message too large, with its length") {
        const std::string packet = publish_packet("dummyTopic", std::string(300, 'x'));
        serve_stream(mqtt_ctx, packet, served);
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        _mqtt_state_recv_msg(&state);
        CHECK( told(KII_MQTT_EVENT_MESSAGE_TOO_LARGE, 2 + 10 + 300) );
    }

    SECTION("a message whose topic doesn't fit, with its length") {
        serve_stream(mqtt_ctx, std::string("\x30\x04\x00\x09" "ab", 6), served);
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        _mqtt_state_recv_msg(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
        CHECK( told(KII_MQTT_EVENT_MESSAGE_MALFORMED, 4) );
    }

    SECTION("a message handed over is no event") {
        serve_stream(mqtt_ctx, publish_packet("dummyTopic", "{}"), served);
        _mqtt_state_recv_ready(&state);
        _mqtt_state_recv_msg(&state);
        CHECK( events.empty() );
    }

    SECTION("a message at QoS 2") {
        serve_stream(mqtt_ctx, publish_qos1_packet("dummyTopic", 7, "{}", 0x34), served);
        _mqtt_state_recv_ready(&state);
        _mqtt_state_recv_msg(&state);
        CHECK( told(KII_MQTT_EVENT_UNEXPECTED_PACKET, 0x34) );
    }

    SECTION("sending PUBACK failed") {
        send_ok = false;
        serve_stream(mqtt_ctx, publish_qos1_packet("dummyTopic", 7, "{}"), served);
        _mqtt_state_recv_ready(&state);
        _mqtt_state_recv_msg(&state);
        CHECK( told(KII_MQTT_EVENT_SEND_FAILED, KII_MQTT_ST_RECV_MSG) );
    }

    SECTION("without a callback, nothing is told") {
        kii_set_cb_mqtt_event(&kii, NULL, NULL);
        send_ok = false;
        _mqtt_state_send_pingreq(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
        CHECK( events.empty() );
    }
}

// The task estimated the time since its last packet by adding up its waits and
// receive timeouts, so time spent elsewhere, such as in the push callback, went
// uncounted, and a receive that returned at once counted for its whole timeout.
// With a monotonic clock in the system, the keep-alive is timed by it.
TEST_CASE( "The MQTT keep-alive is timed by the system's monotonic clock" ) {
    char mqtt_buff[1024];
    kii_t kii;
    kii_init(&kii);
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii._keep_alive_interval = 300;
    kii_set_mqtt_to_sock_recv(&kii, 15);
    // PINGREQ is due 285 seconds after the last packet: the keep-alive less
    // the time a receive may take.
    const uint64_t due_ms = 285 * 1000;

    khct::cb::SockCtx mqtt_ctx;
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());

    uint64_t now_ms = 1000;
    khct::cb::SystemCtx sys;
    sys.on_monotonic_ms = [&now_ms] { return now_ms; };
    kii_set_system(&kii, sys.system());

    khct::cb::PushCtx push_ctx;
    kii_set_cb_mqtt_message(&kii, khct::cb::cb_push, &push_ctx);

    mqtt_state_t state;
    _init_mqtt_state(&kii, &state);

    SECTION("time spent in the push callback counts") {
        const std::string message = std::string("\x00\x05topic", 7) + "{}";
        std::istringstream in(std::string("\x30", 1) + (char)message.size() + message);
        mqtt_ctx.on_recv = [&in](void*, char* buffer, size_t length, size_t* out) {
            *out = in.read(buffer, length).gcount();
            REQUIRE( *out > 0 );
            return EBISU_SOCK_OK;
        };
        push_ctx.on_push = [&now_ms, due_ms](const kii_mqtt_message_t*) { now_ms += due_ms + 1; };

        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        _mqtt_state_recv_msg(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );

        // Due now, rather than after another receive.
        _mqtt_state_recv_ready(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_SEND_PINGREQ );
    }

    SECTION("a receive that returns at once counts for no more than it took") {
        int receives = 0;
        mqtt_ctx.on_recv = [&receives](void*, char*, size_t, size_t*) {
            ++receives;
            return EBISU_SOCK_AGAIN;
        };
        // Each counted for the receive timeout and the wait after it, 16
        // seconds, which 20 of them take past the keep-alive.
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        for (int i = 0; i < 20; ++i) {
            INFO("receive " << i);
            _mqtt_state_recv_ready(&state);
            REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        }
        CHECK( receives == 20 );

        now_ms += due_ms + 1;
        _mqtt_state_recv_ready(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_SEND_PINGREQ );
        CHECK( receives == 20 );
    }

    SECTION("the keep-alive counts from CONNECT") {
        // Waiting between attempts to connect, which the clock goes on through.
        now_ms += 600 * 1000;
        strcpy(state.endpoint.topic, "topic");
        strcpy(state.endpoint.username, "user");
        strcpy(state.endpoint.password, "password");
        mqtt_ctx.on_send = [](void*, const char*, size_t length, size_t* out) {
            *out = length;
            return EBISU_SOCK_OK;
        };
        state.info.task_state = KII_MQTT_ST_SEND_CONNECT;
        _mqtt_state_send_connect(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_CONNACK );

        int receives = 0;
        mqtt_ctx.on_recv = [&receives](void*, char*, size_t, size_t*) {
            ++receives;
            return EBISU_SOCK_AGAIN;
        };
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_READY );
        CHECK( receives == 1 );

        now_ms += due_ms + 1;
        _mqtt_state_recv_ready(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_SEND_PINGREQ );
        CHECK( receives == 1 );
    }
}

// CONNACK was read as 2 bytes after its fixed header, SUBACK as 3 and PINGRESP
// as none, whatever remaining length the fixed header gave. After one that gave
// another, the task read the next packet from the wrong byte, or the next
// packet's bytes as the rest of this one.
TEST_CASE( "CONNACK, SUBACK and PINGRESP of another length are refused" ) {
    char mqtt_buff[256];
    kii_t kii;
    kii_init(&kii);
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii_set_system(&kii, khct::cb::delay_only_system());
    kii._keep_alive_interval = 300;

    std::vector<std::pair<kii_mqtt_event_t, uint32_t>> events;
    kii_set_cb_mqtt_event(&kii, [](kii_mqtt_event_t event, uint32_t detail, void* userdata) {
        ((std::vector<std::pair<kii_mqtt_event_t, uint32_t>>*)userdata)->emplace_back(event, detail);
    }, &events);

    khct::cb::SockCtx mqtt_ctx;
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());
    size_t served = 0;

    int pushes = 0;
    khct::cb::PushCtx push_ctx;
    push_ctx.on_push = [&pushes](const kii_mqtt_message_t*) { ++pushes; };
    kii_set_cb_mqtt_message(&kii, khct::cb::cb_push, &push_ctx);

    mqtt_state_t state;
    _init_mqtt_state(&kii, &state);

    // Each followed by a packet the task would take, and the length of its
    // fixed header, all that is to be read of it.
    struct Packet {
        std::string bytes;
        size_t header;
    };
    const std::string suback("\x90\x03\x00\x01\x00", 5);
    const std::string publish = publish_packet("topic", "message");

    SECTION("CONNACK") {
        const Packet connacks[] = {
            { std::string("\x20\x00", 2), 2 },
            { std::string("\x20\x01\x00", 3), 2 },
            { std::string("\x20\x03\x00\x00\x00", 5), 2 },
            { std::string("\x20\x7f", 2) + std::string(127, '\0'), 2 },
            { std::string("\x20\x80\x01", 3) + std::string(128, '\0'), 3 },
        };
        for (const Packet& connack : connacks) {
            INFO("remaining length " << (int)(unsigned char)connack.bytes[1]);
            events.clear();
            served = 0;
            serve_stream(mqtt_ctx, connack.bytes + suback, served);
            state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
            _mqtt_state_recv_connack(&state);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( state.mqtt_connected == false );
            CHECK( served == connack.header );
            CHECK( events == std::vector<std::pair<kii_mqtt_event_t, uint32_t>>({
                    { KII_MQTT_EVENT_UNEXPECTED_PACKET, 0x20 } }) );
        }
        // One of 2 is still taken, and nothing after it read.
        events.clear();
        served = 0;
        serve_stream(mqtt_ctx, std::string("\x20\x02\x00\x00", 4) + suback, served);
        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_SEND_SUBSCRIBE );
        CHECK( served == 4 );
    }

    SECTION("SUBACK") {
        const Packet subacks[] = {
            { std::string("\x90\x00", 2), 2 },
            { std::string("\x90\x02\x00\x01", 4), 2 },
            { std::string("\x90\x04\x00\x01\x00\x00", 6), 2 },
            // Two return codes, for two topics where the task subscribed one.
            { std::string("\x90\x04\x00\x01\x00\x01", 6), 2 },
            { std::string("\x90\x80\x01", 3) + std::string(128, '\0'), 3 },
        };
        for (const Packet& packet : subacks) {
            INFO("remaining length " << (int)(unsigned char)packet.bytes[1]);
            events.clear();
            served = 0;
            pushes = 0;
            serve_stream(mqtt_ctx, packet.bytes + publish, served);
            state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
            _mqtt_state_recv_suback(&state);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( served == packet.header );
            CHECK( pushes == 0 );
            CHECK( events == std::vector<std::pair<kii_mqtt_event_t, uint32_t>>({
                    { KII_MQTT_EVENT_UNEXPECTED_PACKET, 0x90 } }) );
        }
        events.clear();
        served = 0;
        serve_stream(mqtt_ctx, suback + publish, served);
        state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
        _mqtt_state_recv_suback(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_READY );
        CHECK( served == suback.length() );
    }

    SECTION("PINGRESP") {
        const Packet pingresps[] = {
            // What follows would have been read as a PUBLISH's fixed header.
            { std::string("\xd0\x01\x30", 3), 2 },
            { std::string("\xd0\x02\x00\x00", 4), 2 },
            { std::string("\xd0\x80\x01", 3) + std::string(128, '\0'), 3 },
        };
        for (const Packet& packet : pingresps) {
            INFO("remaining length " << (int)(unsigned char)packet.bytes[1]);
            events.clear();
            served = 0;
            pushes = 0;
            serve_stream(mqtt_ctx, packet.bytes + publish, served);
            state.info.task_state = KII_MQTT_ST_RECV_READY;
            state.pingresp_received = false;
            _mqtt_state_recv_ready(&state);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( state.pingresp_received == false );
            CHECK( served == packet.header );
            CHECK( events == std::vector<std::pair<kii_mqtt_event_t, uint32_t>>({
                    { KII_MQTT_EVENT_UNEXPECTED_PACKET, 0xd0 } }) );
        }
        // One of 0 is still taken, and the message after it read.
        events.clear();
        served = 0;
        pushes = 0;
        serve_stream(mqtt_ctx, std::string("\xd0\x00", 2) + publish, served);
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        state.pingresp_received = false;
        _mqtt_state_recv_ready(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_READY );
        CHECK( state.pingresp_received == true );
        _mqtt_state_recv_ready(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        _mqtt_state_recv_msg(&state);
        CHECK( pushes == 1 );
        CHECK( served == 2 + publish.length() );
        CHECK( events.empty() );
    }
}

namespace {

// Well-formed UTF-8 as the Unicode Standard's table 3-7 has it, written out
// another way than the task checks it, with none of what a PUBLISH's topic may
// not hold: U+0000, and the wildcards + and #.
bool is_publish_topic(const std::string& topic)
{
    if (topic.empty()) {
        return false;
    }
    const struct {
        unsigned char first_min, first_max, second_min, second_max;
        size_t length;
    } rows[] = {
        { 0x01, 0x7f, 0, 0, 1 },
        { 0xc2, 0xdf, 0x80, 0xbf, 2 },
        { 0xe0, 0xe0, 0xa0, 0xbf, 3 },
        { 0xe1, 0xec, 0x80, 0xbf, 3 },
        { 0xed, 0xed, 0x80, 0x9f, 3 },
        { 0xee, 0xef, 0x80, 0xbf, 3 },
        { 0xf0, 0xf0, 0x90, 0xbf, 4 },
        { 0xf1, 0xf3, 0x80, 0xbf, 4 },
        { 0xf4, 0xf4, 0x80, 0x8f, 4 },
    };
    size_t i = 0;
    while (i < topic.size()) {
        const unsigned char c = (unsigned char)topic[i];
        if (c == '+' || c == '#') {
            return false;
        }
        size_t length = 0;
        for (const auto& row : rows) {
            if (row.first_min <= c && c <= row.first_max) {
                if (row.length > 1) {
                    if (topic.size() - i < row.length) {
                        return false;
                    }
                    const unsigned char second = (unsigned char)topic[i + 1];
                    if (second < row.second_min || row.second_max < second) {
                        return false;
                    }
                    for (size_t j = 2; j < row.length; ++j) {
                        const unsigned char next = (unsigned char)topic[i + j];
                        if (next < 0x80 || 0xbf < next) {
                            return false;
                        }
                    }
                }
                length = row.length;
                break;
            }
        }
        if (length == 0) {
            return false;
        }
        i += length;
    }
    return true;
}

}

// The task used to take what MQTT 3.1.1 doesn't allow a broker to send: a
// packet's type with other flags than it has, packets a client that only
// subscribed never gets, a remaining length written longer than it takes,
// CONNACK's reserved flags, a SUBACK granting a reserved or a higher QoS, DUP
// at QoS 0, a packet identifier of 0, and a topic that is empty, a wildcard or
// not UTF-8. It dropped some and went on, and took the rest. MQTT has the
// client close the connection on each.
TEST_CASE( "What MQTT doesn't allow a broker to send is refused" ) {
    char mqtt_buff[256];
    kii_t kii;
    kii_init(&kii);
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii_set_system(&kii, khct::cb::delay_only_system());
    kii._keep_alive_interval = 300;

    std::vector<std::pair<kii_mqtt_event_t, uint32_t>> events;
    kii_set_cb_mqtt_event(&kii, [](kii_mqtt_event_t event, uint32_t detail, void* userdata) {
        ((std::vector<std::pair<kii_mqtt_event_t, uint32_t>>*)userdata)->emplace_back(event, detail);
    }, &events);
    auto told = [&events](kii_mqtt_event_t event, uint32_t detail) {
        return events == std::vector<std::pair<kii_mqtt_event_t, uint32_t>>({ { event, detail } });
    };

    khct::cb::SockCtx mqtt_ctx;
    std::string sent;
    mqtt_ctx.on_send = [&sent](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        sent.append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());
    size_t served = 0;

    std::vector<std::string> pushed;
    khct::cb::PushCtx push_ctx;
    push_ctx.on_push = [&pushed](const kii_mqtt_message_t* message) {
        pushed.emplace_back(message->payload, message->payload_length);
    };
    kii_set_cb_mqtt_message(&kii, khct::cb::cb_push, &push_ctx);

    mqtt_state_t state;
    _init_mqtt_state(&kii, &state);
    auto reset = [&]() {
        events.clear();
        sent.clear();
        pushed.clear();
        served = 0;
    };
    // A message as it comes while the task waits, read to its end.
    auto receive = [&](const std::string& packet) {
        reset();
        serve_stream(mqtt_ctx, packet, served);
        state.info.task_state = KII_MQTT_ST_RECV_READY;
        _mqtt_state_recv_ready(&state);
        if (state.info.task_state == KII_MQTT_ST_RECV_MSG) {
            _mqtt_state_recv_msg(&state);
        }
    };
    const std::string next = publish_packet("topic", "next");

    SECTION("a remaining length written longer than it takes") {
        const std::string lengths[] = {
            std::string("\x80\x00", 2),
            std::string("\x82\x00", 2),
            std::string("\x82\x80\x00", 3),
            std::string("\xff\xff\x80\x00", 4),
        };
        for (const std::string& length : lengths) {
            INFO("length " << (int)(unsigned char)length[0] << " " << length.size());
            receive("\x30" + length + next);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( pushed.empty() );
            CHECK( served == 1 + length.size() );
            CHECK( told(KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_READY) );

            reset();
            serve_stream(mqtt_ctx, "\x20" + length, served);
            state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
            _mqtt_state_recv_connack(&state);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( told(KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_CONNACK) );
        }
        // As short as it can be, in each number of bytes, it is taken.
        receive(publish_packet("t", std::string(125, 'x')));
        CHECK( pushed.size() == 1 );
        receive(publish_packet("t", std::string(126, 'x')));
        CHECK( pushed.size() == 1 );
    }

    SECTION("while waiting, any packet but PUBLISH and PINGRESP, or one with other flags") {
        for (int byte1 = 0; byte1 < 256; ++byte1) {
            if ((byte1 & 0xf0) == 0x30 || byte1 == 0xd0) {
                continue;
            }
            INFO("first byte " << byte1);
            receive(std::string(1, (char)byte1) + std::string("\x02\x00\x01", 3) + next);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( served == 2 );
            CHECK( pushed.empty() );
            CHECK( told(KII_MQTT_EVENT_UNEXPECTED_PACKET, byte1) );
        }
        receive(std::string("\xd0\x00", 2));
        CHECK( state.info.task_state == KII_MQTT_ST_RECV_READY );
        CHECK( events.empty() );
    }

    SECTION("CONNACK and SUBACK with flags") {
        for (int flags = 1; flags < 16; ++flags) {
            INFO("flags " << flags);
            reset();
            serve_stream(mqtt_ctx, std::string(1, (char)(0x20 | flags)) + std::string("\x02\x00\x00", 3), served);
            state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
            _mqtt_state_recv_connack(&state);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( told(KII_MQTT_EVENT_UNEXPECTED_PACKET, 0x20 | flags) );

            reset();
            serve_stream(mqtt_ctx, std::string(1, (char)(0x90 | flags)) + std::string("\x03\x00\x01\x00", 4), served);
            state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
            _mqtt_state_recv_suback(&state);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( told(KII_MQTT_EVENT_UNEXPECTED_PACKET, 0x90 | flags) );
        }
    }

    SECTION("CONNACK's acknowledge flags") {
        // Each with whether the task asks for a clean session.
        const struct {
            const char* name;
            unsigned char flags;
            unsigned char return_code;
            bool clean_session;
            bool taken;
        } cases[] = {
            { "none", 0x00, 0x00, true, true },
            { "a reserved one", 0x02, 0x00, true, false },
            { "all", 0xff, 0x00, false, false },
            { "Session Present for a clean session", 0x01, 0x00, true, false },
            { "Session Present for a session kept", 0x01, 0x00, false, true },
            { "Session Present on a refusal", 0x01, 0x05, false, false },
        };
        for (const auto& c : cases) {
            INFO(c.name);
            reset();
            kii_set_mqtt_clean_session(&kii, c.clean_session);
            serve_stream(mqtt_ctx, std::string("\x20\x02", 2) + (char)c.flags + (char)c.return_code, served);
            state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
            _mqtt_state_recv_connack(&state);
            if (c.taken) {
                CHECK( state.info.task_state == KII_MQTT_ST_SEND_SUBSCRIBE );
                CHECK( told(KII_MQTT_EVENT_CONNACK, c.return_code) );
            } else {
                CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
                CHECK( told(KII_MQTT_EVENT_UNEXPECTED_PACKET, 0x20) );
            }
        }
        // MQTT 3.1 has a byte there that it doesn't use.
        reset();
        kii_set_mqtt_clean_session(&kii, true);
        kii_set_mqtt_version(&kii, KII_MQTT_VERSION_3_1);
        serve_stream(mqtt_ctx, std::string("\x20\x02\xff\x00", 4), served);
        state.info.task_state = KII_MQTT_ST_RECV_CONNACK;
        _mqtt_state_recv_connack(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_SEND_SUBSCRIBE );
    }

    SECTION("SUBACK's return code, a QoS no higher than asked for, or a refusal") {
        for (kii_mqtt_qos_t asked : { KII_MQTT_QOS_0, KII_MQTT_QOS_1 }) {
            kii_set_mqtt_qos(&kii, asked);
            for (int code = 0; code < 256; ++code) {
                INFO("asked " << (int)asked << ", return code " << code);
                reset();
                serve_stream(mqtt_ctx, std::string("\x90\x03\x00\x01", 4) + (char)code, served);
                state.info.task_state = KII_MQTT_ST_RECV_SUBACK;
                _mqtt_state_recv_suback(&state);
                if (code <= (int)asked) {
                    CHECK( state.info.task_state == KII_MQTT_ST_RECV_READY );
                    CHECK( told(KII_MQTT_EVENT_SUBACK, code) );
                } else if (code == 0x80) {
                    CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
                    CHECK( told(KII_MQTT_EVENT_SUBACK, 0x80) );
                } else {
                    CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
                    CHECK( told(KII_MQTT_EVENT_UNEXPECTED_PACKET, 0x90) );
                }
            }
        }
    }

    SECTION("DUP at QoS 0") {
        std::string packet = publish_packet("topic", "{}");
        packet[0] = (char)0x38;
        receive(packet + next);
        CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
        CHECK( pushed.empty() );
        CHECK( told(KII_MQTT_EVENT_UNEXPECTED_PACKET, 0x38) );
    }

    SECTION("a packet identifier of 0 at QoS 1") {
        for (size_t payload_size : { (size_t)2, sizeof(mqtt_buff) }) {
            INFO("payload of " << payload_size);
            const std::string packet = publish_qos1_packet("topic", 0, std::string(payload_size, 'x'));
            receive(packet + next);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( pushed.empty() );
            // Not acknowledged.
            CHECK( sent.empty() );
            CHECK( told(KII_MQTT_EVENT_MESSAGE_MALFORMED, packet.size() - (payload_size > 100 ? 3 : 2)) );
        }
        // Any other is taken, and acknowledged.
        receive(publish_qos1_packet("topic", 0xffff, "{}"));
        CHECK( pushed.size() == 1 );
        CHECK( sent == std::string("\x40\x02\xff\xff", 4) );
    }

    SECTION("an empty topic, in the buffer or too large for it") {
        for (size_t payload_size : { (size_t)2, sizeof(mqtt_buff) }) {
            INFO("payload of " << payload_size);
            receive(publish_packet("", std::string(payload_size, 'x')) + next);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( pushed.empty() );
            REQUIRE( events.size() == 1 );
            CHECK( events[0].first == KII_MQTT_EVENT_MESSAGE_MALFORMED );
        }
    }

    SECTION("a topic MQTT doesn't allow") {
        const std::string topics[] = {
            "a/+", "#", "a/#", std::string("a\0b", 3),
            // Overlong, a surrogate, past U+10FFFF, a lone continuation byte,
            // cut short, and bytes UTF-8 has none of.
            "\xc0\x80", "\xe0\x80\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\x80",
            "\xc3", "\xe2\x82", "\xf0\x9f\x98", "\xfe", "\xff",
        };
        for (const std::string& topic : topics) {
            INFO("topic " << topic.size() << " bytes, first " << (int)(unsigned char)topic[0]);
            receive(publish_qos1_packet(topic, 7, "{}") + next);
            CHECK( state.info.task_state == KII_MQTT_ST_RECONNECT );
            CHECK( pushed.empty() );
            CHECK( sent.empty() );
        }
    }

    SECTION("a topic in UTF-8 is taken") {
        const std::string topics[] = {
            "a", "a/b", "\x7f", "\xc2\x80", "\xc3\xa9", "\xdf\xbf", "\xe0\xa0\x80",
            "\xe2\x82\xac", "\xed\x9f\xbf", "\xee\x80\x80", "\xef\xbf\xbf",
            "\xf0\x90\x80\x80", "\xf0\x9f\x98\x80", "\xf4\x8f\xbf\xbf",
        };
        for (const std::string& topic : topics) {
            INFO("topic " << topic.size() << " bytes, first " << (int)(unsigned char)topic[0]);
            receive(publish_packet(topic, "{}"));
            CHECK( state.info.task_state == KII_MQTT_ST_RECV_READY );
            CHECK( pushed == std::vector<std::string>({ "{}" }) );
        }
    }

    // Every topic of 1 and 2 bytes, and those of 3 and 4 that start as a
    // character of that length does, against the table.
    SECTION("topics, byte by byte") {
        std::vector<std::string> topics;
        for (int a = 0; a < 256; ++a) {
            topics.push_back(std::string(1, (char)a));
            for (int b = 0; b < 256; ++b) {
                topics.push_back(std::string(1, (char)a) + (char)b);
            }
        }
        for (int a = 0xe0; a < 0x100; ++a) {
            for (int b = 0; b < 256; ++b) {
                for (int c : { 0x00, 0x41, 0x7f, 0x80, 0xbf, 0xc0, 0xff }) {
                    topics.push_back(std::string(1, (char)a) + (char)b + (char)c);
                    if (a >= 0xf0) {
                        topics.push_back(std::string(1, (char)a) + (char)b + (char)c + '\x80');
                        topics.push_back(std::string(1, (char)a) + (char)b + (char)c + '\xc0');
                    }
                }
            }
        }
        size_t taken = 0;
        for (const std::string& topic : topics) {
            receive(publish_packet(topic, "{}"));
            const bool expected = is_publish_topic(topic);
            if ((state.info.task_state == KII_MQTT_ST_RECV_READY) != expected) {
                INFO("topic of " << topic.size() << " bytes: " << (int)(unsigned char)topic[0] <<
                        " " << (topic.size() > 1 ? (int)(unsigned char)topic[1] : -1) <<
                        " " << (topic.size() > 2 ? (int)(unsigned char)topic[2] : -1) <<
                        " " << (topic.size() > 3 ? (int)(unsigned char)topic[3] : -1));
                CHECK( expected == !expected );
            }
            taken += expected ? 1 : 0;
        }
        CHECK( taken > 0 );
        CHECK( taken < topics.size() );
    }
}

// The callback got the payload alone: neither the topic, nor the flags, nor the
// packet identifier reached the application.
TEST_CASE( "The message callback gets what the PUBLISH carried" ) {
    char mqtt_buff[64];
    kii_t kii;
    kii_init(&kii);
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));
    kii_set_system(&kii, khct::cb::delay_only_system());
    kii._keep_alive_interval = 300;

    khct::cb::SockCtx mqtt_ctx;
    std::string sent;
    mqtt_ctx.on_send = [&sent](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        sent.append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());
    size_t served = 0;

    // What the callback got, copied, as it is good only until the callback
    // returns, with where its pointers pointed as offsets into the buffer.
    struct Got {
        std::string topic;
        size_t topic_length;
        bool topic_terminated;
        long topic_at;
        std::string payload;
        long payload_at;
        kii_mqtt_qos_t qos;
        bool retain;
        bool dup;
        uint16_t packet_id;
    };
    std::vector<Got> got;
    khct::cb::PushCtx push_ctx;
    push_ctx.on_push = [&got, &mqtt_buff](const kii_mqtt_message_t* message) {
        got.push_back({
            std::string(message->topic, message->topic_length),
            message->topic_length,
            message->topic[message->topic_length] == '\0',
            (long)(message->topic - mqtt_buff),
            std::string(message->payload, message->payload_length),
            (long)(message->payload - mqtt_buff),
            message->qos,
            message->retain,
            message->dup,
            message->packet_id });
    };
    kii_set_cb_mqtt_message(&kii, khct::cb::cb_push, &push_ctx);

    mqtt_state_t state;
    _init_mqtt_state(&kii, &state);
    // A message as it comes while the task waits, read to its end, or while it
    // waits for SUBACK.
    auto receive = [&](const std::string& packet, kii_mqtt_task_state waiting) {
        got.clear();
        sent.clear();
        served = 0;
        serve_stream(mqtt_ctx, packet, served);
        state.info.task_state = waiting;
        if (waiting == KII_MQTT_ST_RECV_SUBACK) {
            _mqtt_state_recv_suback(&state);
        } else {
            _mqtt_state_recv_ready(&state);
        }
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_MSG );
        _mqtt_state_recv_msg(&state);
        CHECK( state.info.task_state == waiting );
        CHECK( served == packet.length() );
        REQUIRE( got.size() == 1 );
        return got[0];
    };
    const kii_mqtt_task_state ready = KII_MQTT_ST_RECV_READY;

    SECTION("at QoS 0") {
        Got g = receive(publish_packet("devices/1", "{\"a\":1}"), ready);
        CHECK( g.topic == "devices/1" );
        CHECK( g.topic_length == 9 );
        CHECK( g.topic_terminated );
        CHECK( g.topic_at == 0 );
        CHECK( g.payload == "{\"a\":1}" );
        CHECK( g.payload_at == 2 + 9 );
        CHECK( g.qos == KII_MQTT_QOS_0 );
        CHECK( g.retain == false );
        CHECK( g.dup == false );
        CHECK( g.packet_id == 0 );
        CHECK( sent.empty() );
    }

    SECTION("at QoS 1") {
        Got g = receive(publish_qos1_packet("devices/1", 0x1234, "{}"), ready);
        CHECK( g.topic == "devices/1" );
        CHECK( g.topic_terminated );
        CHECK( g.payload == "{}" );
        CHECK( g.payload_at == 2 + 9 + 2 );
        CHECK( g.qos == KII_MQTT_QOS_1 );
        CHECK( g.retain == false );
        CHECK( g.dup == false );
        CHECK( g.packet_id == 0x1234 );
        CHECK( sent == std::string("\x40\x02\x12\x34", 4) );
    }

    SECTION("retained, sent again, or both") {
        std::string packet = publish_packet("t", "p");
        packet[0] = 0x31;
        Got g = receive(packet, ready);
        CHECK( g.qos == KII_MQTT_QOS_0 );
        CHECK( g.retain );
        CHECK( g.dup == false );

        g = receive(publish_qos1_packet("t", 7, "p", 0x33), ready);
        CHECK( g.qos == KII_MQTT_QOS_1 );
        CHECK( g.retain );
        CHECK( g.dup == false );

        g = receive(publish_qos1_packet("t", 7, "p", 0x3a), ready);
        CHECK( g.retain == false );
        CHECK( g.dup );
        CHECK( g.packet_id == 7 );

        g = receive(publish_qos1_packet("t", 7, "p", 0x3b), ready);
        CHECK( g.retain );
        CHECK( g.dup );
    }

    SECTION("before SUBACK") {
        Got g = receive(publish_qos1_packet("t", 9, "p", 0x33), KII_MQTT_ST_RECV_SUBACK);
        CHECK( g.topic == "t" );
        CHECK( g.topic_terminated );
        CHECK( g.payload == "p" );
        CHECK( g.qos == KII_MQTT_QOS_1 );
        CHECK( g.retain );
        CHECK( g.dup == false );
        CHECK( g.packet_id == 9 );

        g = receive(publish_qos1_packet("t", 9, "p", 0x3a), KII_MQTT_ST_RECV_SUBACK);
        CHECK( g.retain == false );
        CHECK( g.dup );

        g = receive(publish_packet("t", "p"), KII_MQTT_ST_RECV_SUBACK);
        CHECK( g.qos == KII_MQTT_QOS_0 );
        CHECK( g.retain == false );
        CHECK( g.dup == false );
        CHECK( g.packet_id == 0 );
    }

    // Terminated where its length was, and as far into the buffer as it goes.
    SECTION("topics as short and as long as the buffer has room for") {
        Got g = receive(publish_packet("t", ""), ready);
        CHECK( g.topic == "t" );
        CHECK( g.topic_terminated );
        CHECK( g.payload.empty() );
        CHECK( g.payload_at == 3 );

        const std::string longest(sizeof(mqtt_buff) - 2, 'x');
        g = receive(publish_packet(longest, ""), ready);
        CHECK( g.topic == longest );
        CHECK( g.topic_terminated );
        CHECK( g.payload.empty() );
        CHECK( g.payload_at == (long)sizeof(mqtt_buff) );

        const std::string with_id(sizeof(mqtt_buff) - 4, 'y');
        g = receive(publish_qos1_packet(with_id, 1, ""), ready);
        CHECK( g.topic == with_id );
        CHECK( g.topic_terminated );
        CHECK( g.packet_id == 1 );
        CHECK( g.payload_at == (long)sizeof(mqtt_buff) );
    }

    SECTION("a payload of any byte, with the topic's bytes in it") {
        const std::string payload("\x00\x05topic\xff", 8);
        Got g = receive(publish_packet("topic", payload), ready);
        CHECK( g.topic == "topic" );
        CHECK( g.topic_terminated );
        CHECK( g.payload == payload );
    }
}
