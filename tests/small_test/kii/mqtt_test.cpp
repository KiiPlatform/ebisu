#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "kii.h"
#include "kii_mqtt_task.h"
#include "test_callbacks.h"

#include <sstream>

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
    http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
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
    http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
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
    mqtt_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
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
    mqtt_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
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
    mqtt_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
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
    mqtt_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
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
            case 2: // remaining.
                REQUIRE( length_to_read == 256);
                buffer[0] = 'T';
                buffer[1] = 'E';
                buffer[2] = 'S';
                buffer[4] = 'T';
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

    SECTION("install push try again") {
        int call_connect = 0;
        http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
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
        http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
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
        http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
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
        http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
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
        mqtt_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
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
        mqtt_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
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
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
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
        REQUIRE( state.info.task_state == KII_MQTT_ST_RECV_READY );
        REQUIRE( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( call_recv == 1 );
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
    http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
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
    http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
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
    mqtt_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
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
