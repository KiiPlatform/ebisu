/* The Wi-Fi station, with an address from DHCP, through the SDK's Wi-Fi and
   lwIP glue. */
#include "net.h"

#include <stdbool.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include <lwip/ip_addr.h>
#include <lwip/netif.h>
#include <lwip_netconf.h>
#include <wifi_conf.h>

#include "log.h"
#include "sample_config.h"

/* The station's interface, as the SDK's glue numbers them. */
#define STATION 0

/* The SDK's interfaces, of its lwip_netconf.c, which no header of its
   declares. */
extern struct netif xnetif[];

/* Whether the station has an IPv4 address. Read without lwIP's lock: a
   32-bit word that DHCP sets. */
static bool has_address(void)
{
    return !ip_addr_isany(&xnetif[STATION].ip_addr);
}

static void join(void)
{
    /* wifi_connect() takes them as char*. */
    static char ssid[] = EBISU_SAMPLE_WIFI_SSID;
    static char password[] = EBISU_SAMPLE_WIFI_PASSWORD;
    rtw_security_t security = password[0] == '\0' ?
        RTW_SECURITY_OPEN : RTW_SECURITY_WPA2_AES_PSK;
    for (;;) {
        int ret;
        sample_log("joining %s", ssid);
        ret = wifi_connect(ssid, security, password, (int)strlen(ssid), (int)strlen(password),
                -1, NULL);
        if (ret == RTW_SUCCESS) {
            if (LwIP_DHCP(STATION, DHCP_START) == DHCP_ADDRESS_ASSIGNED) {
                return;
            }
            sample_log("no address from DHCP");
        } else {
            sample_log("failed to join %s: %d", ssid, ret);
        }
        sample_log("trying again in 10 seconds");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void net_wait_until_up(void)
{
    /* wlan_network()'s task starts lwIP, then the Wi-Fi. */
    while (!wifi_is_up(RTW_STA_INTERFACE)) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (EBISU_SAMPLE_WIFI_SSID[0] != '\0') {
        join();
    } else {
        sample_log("no Wi-Fi network in the build: join one from the console, "
                "with ATW0=<ssid>, ATW1=<password> and ATWC");
        while (!has_address()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    {
        /* lwIP 1.4's: IPv4's, in ip_addr_t. */
        const ip_addr_t* address = &xnetif[STATION].ip_addr;
        sample_log("address %u.%u.%u.%u",
                ip4_addr1(address), ip4_addr2(address), ip4_addr3(address), ip4_addr4(address));
    }
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
