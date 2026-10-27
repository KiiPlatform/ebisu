idf_component_register(
    SRCS app_main.c sock_cb_esp32.c sys_cb_esp32.c
    INCLUDE_DIRS .
    REQUIRES ebisu esp-tls mbedtls esp_timer esp_netif esp_event nvs_flash
        protocol_examples_common)
