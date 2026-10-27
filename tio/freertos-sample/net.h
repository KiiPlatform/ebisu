#ifndef NET_H
#define NET_H

#include <FreeRTOS.h>

/* Starts FreeRTOS-Plus-TCP on the board's Ethernet controller, which asks DHCP
   for an address. Before the scheduler starts. pdPASS on success. */
BaseType_t net_init(void);

/* Waits until the network is up, and logs the address it has. */
void net_wait_until_up(void);

#endif /* NET_H */
