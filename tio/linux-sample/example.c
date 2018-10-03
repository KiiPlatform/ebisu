#include "example.h"

#include <tio.h>
#include <tio2.h>
#include <kii_json.h>

#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>

#include <pthread.h>
#include <unistd.h>
#include "sys_cb_impl.h"
#include "sock_cb_linux.h"

typedef struct _air_conditioner_t {
    kii_bool_t power;
    int temperature;
} _air_conditioner_t;

static _air_conditioner_t m_air_conditioner;
static pthread_mutex_t m_mutex;

static kii_bool_t _get_air_conditioner_info(
        _air_conditioner_t* air_conditioner)
{
    if (pthread_mutex_lock(&m_mutex) != 0) {
        return KII_FALSE;
    }
    air_conditioner->power = m_air_conditioner.power;
    air_conditioner->temperature = m_air_conditioner.temperature;
    if (pthread_mutex_unlock(&m_mutex) != 0) {
        return KII_FALSE;
    }
    return KII_TRUE;
}

static kii_bool_t _set_air_conditioner_info(
        const _air_conditioner_t* air_conditioner)
{
    if (pthread_mutex_lock(&m_mutex) != 0) {
        return KII_FALSE;
    }
    m_air_conditioner.power = air_conditioner->power;
    m_air_conditioner.temperature = air_conditioner->temperature;
    if (pthread_mutex_unlock(&m_mutex) != 0) {
        return KII_FALSE;
    }
    return KII_TRUE;
}

static kii_bool_t action_handler(
        const char* alias,
        const char* action_name,
        const char* action_params,
        char error[EMESSAGE_SIZE + 1])
{
    _air_conditioner_t air_conditioner;

    printf("alias=%s, action name=%s, action params=%s\n",
            alias, action_name, action_params);

    if (strcmp(alias, "AirConditionerAlias") != 0 &&
            strcmp(alias, "HumidityAlias") != 0) {
        snprintf(error, EMESSAGE_SIZE + 1, "invalid alias: %s", alias);
        return KII_FALSE;
    }

    memset(&air_conditioner, 0, sizeof(air_conditioner));
    if (_get_air_conditioner_info(&air_conditioner) == KII_FALSE) {
        printf("fail to lock.\n");
        strcpy(error, "fail to lock.");
        return KII_FALSE;
    }
    if (strcmp(action_name, "turnPower") == 0) {
        air_conditioner.power =
            strcmp(action_params, "true") == 0 ? KII_TRUE : KII_FALSE;
    }
    if (strcmp(action_name, "setPresetTemperature") == 0) {
        air_conditioner.temperature = atoi(action_name);
    }

    if (_set_air_conditioner_info(&air_conditioner) == KII_FALSE) {
        printf("fail to unlock.\n");
        return KII_FALSE;
    }
    return KII_TRUE;
}

static kii_bool_t state_handler(
        kii_t* kii,
        TIO_WRITER writer)
{
    FILE* fp = fopen("air_conditioner-state.json", "r");
    if (fp != NULL) {
        char buf[256];
        kii_bool_t retval = KII_TRUE;
        while (fgets(buf, sizeof(buf) / sizeof(buf[0]), fp) != NULL) {
            if ((*writer)(kii, buf) == KII_FALSE) {
                retval = KII_FALSE;
                break;
            }
        }
        fclose(fp);
        return retval;
    } else {
        char buf[256];
        _air_conditioner_t air_conditioner;
        memset(&air_conditioner, 0x00, sizeof(air_conditioner));
        if (_get_air_conditioner_info(&air_conditioner) == KII_FALSE) {
            printf("fail to lock.\n");
            return KII_FALSE;
        }
        if ((*writer)(kii, "{\"AirConditionerAlias\":") == KII_FALSE) {
            return KII_FALSE;
        }
        if ((*writer)(kii, "{\"power\":") == KII_FALSE) {
            return KII_FALSE;
        }
        if ((*writer)(kii, (int)air_conditioner.power == (int)KII_JSON_TRUE
                        ? "true," : "false,") == KII_FALSE) {
            return KII_FALSE;
        }
        if ((*writer)(kii, "\"currentTemperature\":") == KII_FALSE) {
            return KII_FALSE;
        }
        snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%d}",
                air_conditioner.temperature);
        if ((*writer)(kii, buf) == KII_FALSE) {
            return KII_FALSE;
        }
        if ((*writer)(kii, "}") == KII_FALSE) {
            return KII_FALSE;
        }
        return KII_TRUE;
    }
}

static kii_bool_t custom_push_handler(
        kii_t *kii,
        const char* message,
        size_t message_length)
{
    kii_bool_t ret = KII_TRUE;
    printf("custom_push_handler:\n%s\n", message);
    if (strncmp(message, "{\"commandID\"", 12) == 0) {
        ret = KII_FALSE;
    }
    // check no error in parsing topic.
    if (strncmp(message, "{\"Item\":\"CheckNoError\"", 22) == 0) {
        ret = KII_FALSE;
    }
    return ret;
}

static void print_help() {
    printf("sub commands: [onboard|onboard-with-token|get|update]\n\n");
    printf("to see detail usage of sub command, execute ./exampleapp {subcommand} --help\n\n");

    printf("onboard with vendor-thing-id\n");
    printf("./exampleapp onboard --vendor-thing-id={vendor thing id} --password={password}\n\n");

    printf("onboard with thing-id\n");
    printf("./exampleapp onboard --thing-id={thing id} --password={password}\n\n");

    printf("onboard-with-token.\n");
    printf("./exampleapp onboard-with-token --thing-id={thing id} --access-token={access token}\n\n");
    printf("to configure app to use, edit example.h\n\n");

    printf("get.\n"
            "./exampleapp get --firmware-version --thing-type --vendor-thing-id={vendor thing id} --password={password} \n\n");

    printf("get.\n"
            "./exampleapp get --firmware-version --thing-type --thing-id={thing id} --password={password} \n\n");

    printf("update.\n"
            "./exampleapp update --firmware-version --thing-type --vendor-thing-id={vendor thing id} --password={password} \n\n");

    printf("update.\n"
            "./exampleapp update --firmware-version --thing-type --thing-id={thing id} --password={password} \n\n");

}

void init(
        tio_handler_t* tio,
        char* kii_buffer,
        int kii_buffer_size,
        void* http_ssl_ctx,
        char* mqtt_buffer,
        int mqtt_buffer_size,
        void* mqtt_ssl_ctx,
        kii_json_resource_t* resource)
{
    tio_handler_set_app(tio, EX_APP_ID, EX_APP_SITE);

    tio_handler_set_cb_task_create(tio, task_create_cb_impl);
    tio_handler_set_cb_delay_ms(tio, delay_ms_cb_impl);

    tio_handler_set_http_buff(tio, kii_buffer, kii_buffer_size);

    tio_handler_set_cb_sock_connect_http(tio, sock_cb_connect, http_ssl_ctx);
    tio_handler_set_cb_sock_send_http(tio, sock_cb_send, http_ssl_ctx);
    tio_handler_set_cb_sock_recv_http(tio, sock_cb_recv, http_ssl_ctx);
    tio_handler_set_cb_sock_close_http(tio, sock_cb_close, http_ssl_ctx);

    tio_handler_set_mqtt_buff(tio, mqtt_buffer, mqtt_buffer_size);

    tio_handler_set_cb_sock_connect_mqtt(tio, mqtt_cb_connect, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_send_mqtt(tio, mqtt_cb_send, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_recv_mqtt(tio, mqtt_cb_recv, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_close_mqtt(tio, mqtt_cb_close, mqtt_ssl_ctx);

    tio_handler_set_keep_alive_interval(tio, 0);

    kii_set_json_parser_resource(&tio->_kii, resource);
    tio->_kii._author.author_id[0] = '\0';
    tio->_kii._author.access_token[0] = '\0';
}

int main(int argc, char** argv)
{
    char* subc = argv[1];
    /*
    tio_command_handler_resource_t command_handler_resource;
    tio_state_updater_resource_t state_updater_resource;
    tio_system_cb_t sys_cb;
    char command_handler_buff[EX_COMMAND_HANDLER_BUFF_SIZE];
    char state_updater_buff[EX_STATE_UPDATER_BUFF_SIZE];
    char mqtt_buff[EX_MQTT_BUFF_SIZE];
    tio_t tio;
    kii_bool_t result;
    */

    tio_handler_t tio;
    char kii_buff[EX_COMMAND_HANDLER_BUFF_SIZE];
    socket_context_t http_ctx;
    char mqtt_buff[EX_MQTT_BUFF_SIZE];
    socket_context_t mqtt_ctx;
    kii_json_token_t tokens[256];
    kii_json_resource_t resource = {tokens, 256};
    kii_code_t result;

    memset(kii_buff, 0x00, sizeof(char) * EX_COMMAND_HANDLER_BUFF_SIZE);
    memset(mqtt_buff, 0x00, sizeof(char) * EX_MQTT_BUFF_SIZE);
    init(
            &tio,
            kii_buff,
            EX_COMMAND_HANDLER_BUFF_SIZE,
            &http_ctx,
            mqtt_buff,
            EX_MQTT_BUFF_SIZE,
            &mqtt_ctx,
            &resource);

    if (pthread_mutex_init(&m_mutex, NULL) != 0) {
        printf("fail to get mutex.\n");
        exit(1);
    }

    if (argc < 2) {
        printf("too few arguments.\n");
        print_help();
        exit(1);
    }

    /* Parse command. */
    if (strcmp(subc, "onboard") == 0) {
        char* vendorThingID = NULL;
        char* password = NULL;
        while(1) {
            struct option longOptions[] = {
                {"vendor-thing-id", required_argument, 0, 0},
                {"password", required_argument, 0, 1},
                {"help", no_argument, 0, 2},
                {0, 0, 0, 0}
            };
            int optIndex = 0;
            int c = getopt_long(argc, argv, "", longOptions, &optIndex);
            const char* optName = longOptions[optIndex].name;
            if (c == -1) {
                if (vendorThingID == NULL) {
                    printf("neither vendor-thing-id is specified.\n");
                    exit(1);
                }
                if (password == NULL) {
                    printf("password is not specifeid.\n");
                    exit(1);
                }
                printf("program successfully started!\n");
                result = kii_ti_onboard(
                        &tio._kii,
                        vendorThingID,
                        password,
                        NULL,
                        NULL,
                        NULL,
                        NULL);
                if (result != KII_ERR_OK) {
                    printf("failed to onboard.\n");
                    exit(1);
                }
                break;
            }
            printf("option %s : %s\n", optName, optarg);
            switch(c) {
                case 0:
                    vendorThingID = optarg;
                    break;
                case 1:
                    password = optarg;
                    break;
                case 2:
                    printf("usage: \n");
                    printf("onboard --vendor-thing-id={ID of the thing} --password={password of the thing}\n");
                    break;
                default:
                    printf("unexpected usage.\n");
            }
            if (strcmp(optName, "help") == 0) {
                break;
            }
        }
    } else {
        print_help();
        exit(0);
    }
/*
    } else if (strcmp(subc, "get") == 0) {
        char* vendorThingID = NULL;
        char* thingID = NULL;
        char* password = NULL;
        int getFirmwareVersion = 0;
        int getThingType = 0;
        while (1) {
            struct option longOptions[] = {
                {"vendor-thing-id", required_argument, 0, 0},
                {"thing-id", required_argument, 0, 1},
                {"password", required_argument, 0, 2},
                {"firmware-version", no_argument, 0, 3},
                {"thing-type", no_argument, 0, 4},
                {"help", no_argument, 0, 5},
                {0, 0, 0, 0}
            };
            int optIndex = 0;
            int c = getopt_long(argc, argv, "", longOptions, &optIndex);
            if (c == -1) {
                break;
            }
            switch (c) {
                case 0:
                    vendorThingID = optarg;
                    break;
                case 1:
                    thingID = optarg;
                    break;
                case 2:
                    password = optarg;
                    break;
                case 3:
                    getFirmwareVersion = 1;
                    break;
                case 4:
                    getThingType = 1;
                    break;
                case 5:
                    printf("usage: \n"
                            "get --vendor-thing-id={ID of the thing} "
                            "--password={password of the thing} "
                            "--thing-type "
                            "--firmware-version\n");
                    exit(0);
                    break;
            }
        }
        if (vendorThingID == NULL && thingID == NULL) {
            printf("neither vendor-thing-id and thing-id are specified.\n");
            exit(1);
        }
        if (password == NULL) {
            printf("password is not specifeid.\n");
            exit(1);
        }
        if (vendorThingID != NULL && thingID != NULL) {
            printf("both vendor-thing-id and thing-id is specified.  either of one should be specified.\n");
            exit(1);
        }
        if (getFirmwareVersion == 0 && getThingType == 0) {
            printf("--firmware-version or --thing-type must be specified.\n");
            exit(1);
        }
        if (init_tio(
                &tio,
                EX_APP_ID,
                EX_APP_KEY,
                EX_APP_SITE,
                &command_handler_resource,
                &state_updater_resource,
                &sys_cb) == KII_FALSE) {
            printf("fail to initialize.\n");
            exit(1);
        }
        if (vendorThingID != NULL) {
            if (kii_ti_onboard(
                    &kii,
                    vendorThingID,
                    password,
                    NULL,
                    NULL,
                    NULL,
                    NULL) != KII_ERR_OK) {
                printf("fail to onboard.\n");
                exit(1);
            }
        } else {
            if (onboard_with_thing_id(
                    &tio,
                    thingID,
                    password,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL) == KII_FALSE) {
                printf("fail to onboard.\n");
                exit(1);
            }
        }

        if (getFirmwareVersion != 0) {
            char firmwareVersion[64];
            tio_error_t error;
            if (get_firmware_version(
                    &tio,
                    firmwareVersion,
                    sizeof(firmwareVersion) / sizeof(firmwareVersion[0]),
                    &error) == KII_FALSE) {
                printf("get_firmware_version is failed: %d\n", error.code);
                if (error.code == TIO_ERROR_HTTP) {
                    printf("status code=%d, error code=%s\n",
                            error.http_status_code,
                            error.error_code);
                }
                exit(0);
            }
            printf("firmware version=%s\n", firmwareVersion);
        }
        if (getThingType != 0) {
            char thingType[64];
            tio_error_t error;
            if (get_thing_type(
                    &tio,
                    thingType,
                    sizeof(thingType) / sizeof(thingType[0]),
                    &error) == KII_FALSE) {
                printf("get_thing_type is failed: %d\n", error.code);
                if (error.code == TIO_ERROR_HTTP) {
                    printf("status code=%d, error code=%s\n",
                            error.http_status_code,
                            error.error_code);
                }
                exit(0);
            }
            printf("thing type=%s\n", thingType);
        }
        exit(0);
    } else if (strcmp(subc, "update") == 0) {
        char* vendorThingID = NULL;
        char* thingID = NULL;
        char* password = NULL;
        char* firmwareVersion = NULL;
        char* thingType = NULL;
        while (1) {
            struct option longOptions[] = {
                {"vendor-thing-id", required_argument, 0, 0},
                {"thing-id", required_argument, 0, 1},
                {"password", required_argument, 0, 2},
                {"firmware-version", required_argument, 0, 3},
                {"thing-type", required_argument, 0, 4},
                {"help", no_argument, 0, 5},
                {0, 0, 0, 0}
            };
            int optIndex = 0;
            int c = getopt_long(argc, argv, "", longOptions, &optIndex);
            if (c == -1) {
                break;
            }
            switch (c) {
                case 0:
                    vendorThingID = optarg;
                    break;
                case 1:
                    thingID = optarg;
                    break;
                case 2:
                    password = optarg;
                    break;
                case 3:
                    firmwareVersion = optarg;
                    break;
                case 4:
                    thingType = optarg;
                    break;
                case 5:
                    printf("usage: \n"
                            "update --vendor-thing-id={ID of the thing} "
                            "--password={password of the thing} "
                            "--thing-type={thing type "
                            "--firmware-version={firmware version}\n");
                    exit(0);
                    break;
            }
        }
        if (vendorThingID == NULL && thingID == NULL) {
            printf("neither vendor-thing-id and thing-id are specified.\n");
            exit(1);
        }
        if (password == NULL) {
            printf("password is not specifeid.\n");
            exit(1);
        }
        if (vendorThingID != NULL && thingID != NULL) {
            printf("both vendor-thing-id and thing-id is specified.  either of one should be specified.\n");
            exit(1);
        }
        if (firmwareVersion == NULL && thingType == NULL) {
            printf("--firmware-version or --thing-type must be specified.\n");
            exit(1);
        }
        if (init_tio(
                &tio,
                EX_APP_ID,
                EX_APP_KEY,
                EX_APP_SITE,
                &command_handler_resource,
                &state_updater_resource,
                &sys_cb) == KII_FALSE) {
            printf("fail to initialize.\n");
            exit(1);
        }
        if (vendorThingID != NULL) {
            if (onboard_with_vendor_thing_id(
                    &tio,
                    vendorThingID,
                    password,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL) == KII_FALSE) {
                printf("fail to onboard.\n");
                exit(1);
            }
        } else {
            if (onboard_with_thing_id(
                    &tio,
                    thingID,
                    password,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL) == KII_FALSE) {
                printf("fail to onboard.\n");
                exit(1);
            }
        }

        if (firmwareVersion != NULL) {
            tio_error_t error;
            if (update_firmware_version(
                    &tio,
                    firmwareVersion,
                    &error) == KII_FALSE) {
                printf("update_firmware_version is failed: %d\n", error.code);
                if (error.code == TIO_ERROR_HTTP) {
                    printf("status code=%d, error code=%s\n",
                            error.http_status_code,
                            error.error_code);
                }
                exit(1);
            }
            printf("firmware version successfully updated.\n");
        }
        if (thingType != NULL) {
            tio_error_t error;
            if (update_thing_type(
                    &tio,
                    thingType,
                    &error) == KII_FALSE) {
                printf("update_thing_type is failed: %d\n", error.code);
                if (error.code == TIO_ERROR_HTTP) {
                    printf("status code=%d, error code=%s\n",
                            error.http_status_code,
                            error.error_code);
                }
                exit(1);
            }
            printf("thing type successfully updated.\n");
        }
        exit(0);
    } else {
        print_help();
        exit(0);
    }

    start(&tio);
    */
    tio_handler_start(&tio, NULL, NULL, NULL, NULL);

    /* run forever. TODO: Convert to daemon. */
    while(1){ sleep(1); };

    /*
     * This sample application keeps mutex from the start to end
     * of the applicatoin process. So we don't implement destry.
     * pthread_mutex_destroy(&m_mutex);
    */
}

/* vim: set ts=4 sts=4 sw=4 et fenc=utf-8 ff=unix: */

