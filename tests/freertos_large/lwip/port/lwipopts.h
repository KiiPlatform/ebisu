/* lwIP for the large tests on lwIP, on the board's LAN9118 with DHCP, as
   Realtek's SDKs configure it in their lwipopts.h: the SDK for AmebaZ lwIP
   1.4.1, TEST_LWIP_1, and the SDK for AmebaZ2 lwIP 2.0.2. Their settings
   below, whatever bears on ebisu's socket among them: an OS, no core lock,
   timeouts of int milliseconds, sockets that set no errno, lwIP's own errno
   values, keepalive's TCP options, IPv4 alone, and their pools, which leave a
   connection little room. And LWIP_SO_SNDTIMEO, which the samples add to
   them, as ebisu's socket needs it. See lwIP's src/include/lwip/opt.h for
   each setting. */
#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#define NO_SYS 0
#define SYS_LIGHTWEIGHT_PROT 1
#define LWIP_TCPIP_CORE_LOCKING 0
/* Mutexes as binary semaphores, which sys_arch.c has. */
#define LWIP_COMPAT_MUTEX 1

#define LWIP_NETCONN 1
#define LWIP_SOCKET 1
#define LWIP_SO_RCVTIMEO 1
#define LWIP_SO_SNDTIMEO 1
#define LWIP_SO_SNDRCVTIMEO_NONSTANDARD 1
#define LWIP_TCP_KEEPALIVE 1
#define SO_REUSE 1
/* lwIP's errno values, which SO_ERROR gives, and sockets that set no errno:
   lwIP 1.4 sets it only with ERRNO, which neither SDK defines. */
#define LWIP_PROVIDE_ERRNO 1
#define LWIP_SOCKET_SET_ERRNO 0

#define LWIP_IPV6 0
#define LWIP_ARP 1
#define LWIP_ICMP 1
#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_DHCP 1
#define LWIP_DNS 1
#define ETHARP_TRUST_IP_MAC 0
#define ARP_QUEUEING 0
#define IP_REASSEMBLY 1
#define IP_FRAG 1
#define TCP_TTL 255
#define UDP_TTL 255
#define TCP_QUEUE_OOSEQ 1
#define LWIP_RANDOMIZE_INITIAL_LOCAL_PORTS 1
#define LWIP_TCPIP_TIMEOUT 1

#define MEM_ALIGNMENT 4
#define MEM_SIZE (5 * 1024)
#define MEMP_NUM_PBUF 100
#define MEMP_NUM_UDP_PCB 6
#define MEMP_NUM_TCP_PCB 10
#define MEMP_NUM_TCP_PCB_LISTEN 5
#define MEMP_NUM_TCP_SEG 20
#define MEMP_NUM_SYS_TIMEOUT 10
#define PBUF_POOL_SIZE 20
#define PBUF_POOL_BUFSIZE 500
#define TCP_MSS (1500 - 40)
#define TCP_SND_BUF (5 * TCP_MSS)
#define TCP_SND_QUEUELEN (4 * TCP_SND_BUF / TCP_MSS)
#if TEST_LWIP_1
/* lwIP's own default of 4 connections, which the SDK for AmebaZ keeps. */
#define TCP_WND (4 * TCP_MSS)
#else
#define MEMP_NUM_NETCONN 8
#define TCP_WND (2 * TCP_MSS)
#endif

/* Stacks in FreeRTOS's words, as sys_arch.c makes the threads. */
#define TCPIP_THREAD_NAME "TCP_IP"
#define TCPIP_THREAD_STACKSIZE 1000
#define TCPIP_THREAD_PRIO (configMAX_PRIORITIES - 2)
#define TCPIP_MBOX_SIZE 6
#define DEFAULT_THREAD_STACKSIZE 500
#define DEFAULT_UDP_RECVMBOX_SIZE 6
#define DEFAULT_TCP_RECVMBOX_SIZE 6
#define DEFAULT_RAW_RECVMBOX_SIZE 6
#define DEFAULT_ACCEPTMBOX_SIZE 6

#define LWIP_STATS 0

#endif /* LWIP_LWIPOPTS_H */
