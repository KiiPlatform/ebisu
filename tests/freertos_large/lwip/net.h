#ifndef NET_H
#define NET_H

#include <FreeRTOS.h>

/* Starts lwIP, which, once the scheduler runs, adds the board's LAN9118 and
   asks DHCP for an address. Before the scheduler starts. pdPASS on success. */
BaseType_t net_init(void);

/* Waits until DHCP has given the board an address, and logs it, and the name
   server's. */
void net_wait_until_up(void);

#endif /* NET_H */
