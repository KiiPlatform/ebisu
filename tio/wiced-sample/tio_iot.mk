#

#==============================================================================
# Global defines
#==============================================================================
GLOBAL_DEFINES += STDIO_BUFFER_SIZE=128

NAME := App_tio_iot_demo

$(NAME)_SOURCES := ./tio_demo.c \
                   ./tio_socket_impl.c \
                   ./tio_task_impl.c \
                   ./tio/command_parser.c \
                   ./tio/jkii.c \
                   ./tio/jsmn.c \
                   ./tio/khc.c \
                   ./tio/khc_impl.c \
                   ./tio/khc_slist.c \
                   ./tio/khc_state_impl.c \
                   ./tio/kii.c \
                   ./tio/kii_api_call.c \
                   ./tio/kii_json_wrapper.c \
                   ./tio/kii_mqtt_task.c \
                   ./tio/kii_object.c \
                   ./tio/kii_object_impl.c \
                   ./tio/kii_push.c \
                   ./tio/kii_push_impl.c \
                   ./tio/kii_req_impl.c \
                   ./tio/kii_server_code.c \
                   ./tio/kii_thing.c \
                   ./tio/kii_thing_impl.c \
                   ./tio/kii_ti.c \
                   ./tio/kii_ti_impl.c \
                   ./tio/tio.c \
                   ./tio/tio_impl.c

$(NAME)_INCLUDES := ./tio/

$(NAME)_COMPONENTS := protocols/MQTT \
                      utilities/wiced_log \
                      utilities/command_console \
                      utilities/command_console/wps \
                      utilities/command_console/wifi \
                      utilities/command_console/thread \
                      utilities/command_console/ping \
                      utilities/command_console/platform \
                      utilities/command_console/tracex \
                      utilities/command_console/mallinfo

WIFI_CONFIG_DCT_H := wifi_config_dct.h

VALID_PLATFORMS := BCM94343W_AVN
