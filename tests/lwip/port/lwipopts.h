/* lwIP for the socket's tests, on a POSIX host, with an OS: its sockets, TCP,
   UDP for DNS, and the loopback interface, which the tests' own servers
   listen on, as lwIP's sockets reach no other host from here. What the two
   configurations differ in, CMakeLists.txt defines as TEST_CONFIG_B:
   - a: timeouts of struct timeval, select, keepalive's TCP options, IPv4;
   - b: timeouts of int milliseconds, LWIP_SO_SNDRCVTIMEO_NONSTANDARD, poll
     without select from lwIP 2.1 on, which 2.0 hasn't, keepalive without its
     TCP options, LWIP_TCP_KEEPALIVE, IPv6 as well as IPv4, and, in lwIP 2.0,
     sockets that set no errno, LWIP_SOCKET_SET_ERRNO at 0, which 2.1 on
     haven't.
   lwIP 1.4, TEST_LWIP_1, has neither IPv6, poll nor a struct timeval for its
   timeouts, whose sockets set errno only with ERRNO: a sets it, and b not.
   See lwIP's src/include/lwip/opt.h for each setting. */
#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#define NO_SYS 0
#define SYS_LIGHTWEIGHT_PROT 1
#define LWIP_TCPIP_CORE_LOCKING 1

#define LWIP_SOCKET 1
#define LWIP_NETCONN 1
/* The socket calls lwIP's functions by their lwip_ names; the tests' own
   servers do too. Without the POSIX names, which the host's headers have. */
#define LWIP_COMPAT_SOCKETS 0
#define LWIP_POSIX_SOCKETS_IO_NAMES 0
#define LWIP_SO_RCVTIMEO 1
#define LWIP_SO_SNDTIMEO 1
/* For the tests' servers to reset a connection. */
#define LWIP_SO_LINGER 1

#define LWIP_IPV4 1
#define LWIP_TCP 1
#define LWIP_UDP 1
#define LWIP_DNS 1
/* The names the tests look up, with dns_local_addhost(). There is no name
   server: a name not among them fails at once. */
#define DNS_LOCAL_HOSTLIST 1
#define DNS_LOCAL_HOSTLIST_IS_DYNAMIC 1
/* server.test, and moving.test, which a test of the socket's moves. */
#define MEMP_NUM_LOCALHOSTLIST 2
#if TEST_LWIP_1
/* No server until a test sets one, where lwIP 1.4 has one of its own, and 2.0
   on none without this. */
#define DNS_SERVER_ADDRESS(ipaddr) ip_addr_set_any(ipaddr)
#endif
#define LWIP_HAVE_LOOPIF 1
#define LWIP_NETIF_LOOPBACK 1
#define LWIP_NETIF_API 1

#if TEST_LWIP_1 && !TEST_CONFIG_B
#define ERRNO 1
#endif

#if TEST_CONFIG_B && TEST_LWIP_1
#define LWIP_TCP_KEEPALIVE 0
#elif TEST_CONFIG_B
#define LWIP_SOCKET_SET_ERRNO 0
#define LWIP_SO_SNDRCVTIMEO_NONSTANDARD 1
#define LWIP_SOCKET_SELECT 0
#define LWIP_SOCKET_POLL 1
#define LWIP_TCP_KEEPALIVE 0
#define LWIP_IPV6 1
#define LWIP_DNS_ADDRTYPE_DEFAULT LWIP_DNS_ADDRTYPE_IPV4_IPV6
/* As lwIP asks on a host whose pointers are wider than 32 bits. */
#define IPV6_FRAG_COPYHEADER 1
#else
#define LWIP_SO_SNDRCVTIMEO_NONSTANDARD 0
#define LWIP_TCP_KEEPALIVE 1
#define LWIP_IPV6 0
#endif

/* Room for the tests' connections, their servers' and those they leave in
   TIME_WAIT, each test making a few. */
#define MEMP_NUM_NETCONN 32
#define MEMP_NUM_TCP_PCB 32
#define MEMP_NUM_TCP_PCB_LISTEN 8
#define MEMP_NUM_UDP_PCB 8
#define MEMP_NUM_SYS_TIMEOUT 32
#define MEM_SIZE (512 * 1024)
#define PBUF_POOL_SIZE 128
#define TCP_MSS 1460
#define TCP_WND (8 * TCP_MSS)
#define TCP_SND_BUF (8 * TCP_MSS)
#define TCP_SND_QUEUELEN (4 * TCP_SND_BUF / TCP_MSS)
#define MEMP_NUM_TCP_SEG TCP_SND_QUEUELEN

#define TCPIP_MBOX_SIZE 64
#define DEFAULT_TCP_RECVMBOX_SIZE 64
#define DEFAULT_UDP_RECVMBOX_SIZE 16
#define DEFAULT_RAW_RECVMBOX_SIZE 16
#define DEFAULT_ACCEPTMBOX_SIZE 16
#define TCPIP_THREAD_STACKSIZE 0
#define DEFAULT_THREAD_STACKSIZE 0

#define LWIP_STATS 0

#endif /* LWIP_LWIPOPTS_H */
