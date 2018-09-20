#include <tio.h>

#include <stdio.h>
#include <string.h>

#include <gtest/gtest.h>

typedef struct expected_action_t {
    const char* alias;
    const char* action_name;
    const char* action_value;
} expected_action_t;

static int handle_command_expected_index;
static expected_action_t handle_command_expected[3];

static kii_bool_t action_handler_returning_true(
        const char* alias,
        const char* action_name,
        const char* action_value,
        char error[EMESSAGE_SIZE + 1])
{
    EXPECT_STREQ(
        handle_command_expected[handle_command_expected_index].alias,
        alias);
    EXPECT_STREQ(
        handle_command_expected[handle_command_expected_index].action_name,
        action_name);
    EXPECT_STREQ(
        handle_command_expected[handle_command_expected_index].action_value,
        action_value);
    handle_command_expected_index++;
    return KII_TRUE;
}

static kii_bool_t state_handler_returning_true(
        kii_t *kii,
        TIO_WRITER writer)
{
    return (*writer)(kii, "{\"power\":true}");
}

TEST(kiiThingIfTest, handle_command)
{
    tio_t tio;
    tio_command_handler_resource_t command_handler_resource;
    tio_state_updater_resource_t state_updater_resource;
    char command_handler_buff[1024];
    char state_updater_buff[1024];
    char mqtt_buff[1024];
    int i;

    // FIXME: mock response.
    char command_payload[] = "{\"commandID\":\"XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX\",\"actions\":[{\"AirConditionerAlias\":[{\"turnPower\":true},{\"setPresetTemperature\":25}]},{\"HumidityAlias\":[{\"setPresetHumidity\":45}]}],\"issuer\":\"user:XXXXXXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXX\"}";

    const char* receiving_requests[2];
    receiving_requests[0] = "HTTP/1.1 201\r\n\r\n";
    receiving_requests[1] = "HTTP/1.1 201\r\n\r\n";

    command_handler_resource.buffer = command_handler_buff;
    command_handler_resource.buffer_size =
        sizeof(command_handler_buff) / sizeof(command_handler_buff[0]);
    command_handler_resource.mqtt_buffer = mqtt_buff;
    command_handler_resource.mqtt_buffer_size =
        sizeof(mqtt_buff) / sizeof(mqtt_buff[0]);
    command_handler_resource.action_handler = action_handler_returning_true;
    command_handler_resource.state_handler = state_handler_returning_true;

    state_updater_resource.buffer = state_updater_buff;
    state_updater_resource.buffer_size =
        sizeof(state_updater_buff) / sizeof(state_updater_buff[0]);
    state_updater_resource.period = 10;
    state_updater_resource.state_handler = state_handler_returning_true;

    tio_system_cb_t sys_cb;

    // FIXME: Setup callbacks
    // sys_cb.task_create_cb = task_create_cb_impl;
    // sys_cb.delay_ms_cb = delay_ms_cb_impl;

    // FIXME: Setup mock socket callbacks.
    // sys_cb.socket_connect_cb = socket_connect_cb_impl;
    // sys_cb.socket_send_cb = socket_send_cb_impl;
    // sys_cb.socket_recv_cb = socket_recv_cb_impl;
    // sys_cb.socket_close_cb = socket_close_cb_impl;
    // sys_cb.mqtt_socket_connect_cb = mqtt_connect_cb_impl;
    // sys_cb.mqtt_socket_send_cb = mqtt_send_cb_impl;
    // sys_cb.mqtt_socket_recv_cb = mqtt_recv_cb_impl;
    // sys_cb.mqtt_socket_close_cb = mqtt_close_cb_impl;

    ASSERT_EQ(
        KII_TRUE,
        init_tio(
            &tio,
            "app_id",
            "app_key",
            "JP",
            &command_handler_resource,
            &state_updater_resource,
            &sys_cb
            )
    );

    strcpy(tio.command_handler._author.author_id, "owenr");
    strcpy(tio.state_updater._author.author_id, "owenr");
    strcpy(tio.command_handler._author.access_token, "token");
    strcpy(tio.state_updater._author.access_token, "token");

    handle_command_expected_index = 0;
    handle_command_expected[0].alias = "AirConditionerAlias";
    handle_command_expected[0].action_name = "turnPower";
    handle_command_expected[0].action_value = "true";
    handle_command_expected[1].alias = "AirConditionerAlias";
    handle_command_expected[1].action_name = "setPresetTemperature";
    handle_command_expected[1].action_value = "25";
    handle_command_expected[2].alias = "HumidityAlias";
    handle_command_expected[2].action_name = "setPresetHumidity";
    handle_command_expected[2].action_value = "45";

    // FIXME: Execute internal command handler.
    // test_handle_command(&tio, command_payload, strlen(command_payload));

    // FIXME: Compare requests.
    char* req1;
    char* req2;
    ASSERT_STREQ(
        req1,
        "PUT https://api-jp.kii.com/thing-if/apps/app_id/targets/thing:owenr/commands/XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX/action-results HTTP/1.0\r\n"
        "host:api-jp.kii.com\r\n"
        "x-kii-appid:app_id\r\n"
        "x-kii-appkey:app_key\r\n"
        "x-kii-sdk:sn=tic;sv=1.0.1\r\n"
        "content-type:application/json\r\n"
        "authorization:bearer token\r\n"
        "content-length:137\r\n"
        "\r\n"
        "{\"actionResults\":[{\"turnPower\":{\"succeeded\":true}},{\"setPresetTemperature\":{\"succeeded\":true}},{\"setPresetHumidity\":{\"succeeded\":true}}]}");
    ASSERT_STREQ(
        req2,
        "PUT https://api-jp.kii.com/thing-if/apps/app_id/targets/thing:owenr/states HTTP/1.0\r\n"
        "host:api-jp.kii.com\r\n"
        "x-kii-appid:app_id\r\n"
        "x-kii-appkey:app_key\r\n"
        "x-kii-sdk:sn=tic;sv=1.0.1\r\n"
        "content-type:application/vnd.kii.MultipleTraitState+json\r\n"
        "authorization:bearer token\r\n"
        "content-length:14\r\n"
        "\r\n"
        "{\"power\":true}");
}
