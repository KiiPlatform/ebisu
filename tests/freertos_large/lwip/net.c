/* The network, with lwIP: the board's LAN9118, with an address from DHCP, as
   the FreeRTOS sample's net.c has it with FreeRTOS-Plus-TCP. See net.h. */
#include "net.h"

#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include <lwip/dhcp.h>
#include <lwip/dns.h>
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>

#include "board.h"
#include "lan9118_netif.h"
#include "log.h"

static struct netif netif;

/* In lwIP's thread, once the scheduler has started it. */
static void start(void* parameter)
{
    (void)parameter;
    static ip_addr_t none;
    ip_addr_set_zero(&none);
#if LWIP_VERSION_MAJOR < 2
    struct netif* added = netif_add(&netif, &none, &none, &none, NULL,
            lan9118_netif_init, tcpip_input);
#else
    struct netif* added = netif_add(&netif, ip_2_ip4(&none), ip_2_ip4(&none), ip_2_ip4(&none),
            NULL, lan9118_netif_init, tcpip_input);
#endif
    if (added == NULL) {
        board_fatal("failed to add the LAN9118's interface");
    }
    netif_set_default(&netif);
    netif_set_up(&netif);
    if (dhcp_start(&netif) != ERR_OK) {
        board_fatal("failed to start DHCP");
    }
}

BaseType_t net_init(void)
{
    tcpip_init(start, NULL);
    return pdPASS;
}

void net_wait_until_up(void)
{
    while (netif.ip_addr.addr == 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    /* ipaddr_ntoa()'s buffer is one: the address's, then the server's. */
    char address[16];
    strncpy(address, ipaddr_ntoa(&netif.ip_addr), sizeof(address) - 1);
    address[sizeof(address) - 1] = '\0';
#if LWIP_VERSION_MAJOR < 2
    const ip_addr_t server = dns_getserver(0);
    sample_log("network up: %s, name server %s", address, ipaddr_ntoa(&server));
#else
    sample_log("network up: %s, name server %s", address, ipaddr_ntoa(dns_getserver(0)));
#endif
}
