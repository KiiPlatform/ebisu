#ifndef FREERTOS_IP_CONFIG_H
#define FREERTOS_IP_CONFIG_H

/* FreeRTOS-Plus-TCP for the sample: IPv4, an address from DHCP, DNS, and TCP.
   What is not set here has the default of FreeRTOSIPConfigDefaults.h. See
   https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/TCP_IP_Configuration.html */

#define ipconfigBYTE_ORDER pdFREERTOS_LITTLE_ENDIAN

#define ipconfigUSE_IPv4 1
#define ipconfigUSE_IPv6 0
#define ipconfigUSE_DHCP 1
#define ipconfigUSE_DHCP_HOOK 0
#define ipconfigUSE_DNS 1
#define ipconfigUSE_DNS_CACHE 1
#define ipconfigDNS_CACHE_ENTRIES 4
#define ipconfigDNS_CACHE_NAME_LENGTH 64
#define ipconfigUSE_TCP 1

/* vApplicationIPNetworkEventHook_Multi() tells the application the network is
   up. */
#define ipconfigUSE_NETWORK_EVENT_HOOK 1

/* Above the tasks that use it. */
#define ipconfigIP_TASK_PRIORITY (configMAX_PRIORITIES - 2)
#define ipconfigIP_TASK_STACK_SIZE_WORDS (configMINIMAL_STACK_SIZE * 4)

/* The LAN9118 driver copies frames to and from the stack's buffers, which
   BufferAllocation_2.c allocates as needed. */
#define ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS 60
#define ipconfigEVENT_QUEUE_LENGTH (ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS + 5)
#define ipconfigNETWORK_MTU 1500
/* The 32-bit fields of the IP header aligned. */
#define ipconfigPACKET_FILLER_SIZE 2

/* The driver offloads neither checking checksums nor filtering frames: the
   stack does both. */
#define ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM 0
#define ipconfigETHERNET_DRIVER_FILTERS_FRAME_TYPES 0

#endif /* FREERTOS_IP_CONFIG_H */
