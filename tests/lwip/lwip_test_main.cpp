// The lwIP port's tests: lwIP, started once for them all, with the interface
// that drops all it is given and the names they look up, then Catch's session.
// See README.md.

#define CATCH_CONFIG_RUNNER
#include "lwip_test.h"

namespace {

struct netif silent_netif;

err_t drop(struct netif* netif, struct pbuf* p, const ip4_addr_t* addr)
{
    (void)netif;
    (void)p;
    (void)addr;
    return ERR_OK;
}

#if LWIP_IPV6
err_t drop6(struct netif* netif, struct pbuf* p, const ip6_addr_t* addr)
{
    (void)netif;
    (void)p;
    (void)addr;
    return ERR_OK;
}
#endif

err_t silent_init(struct netif* netif)
{
    netif->name[0] = 's';
    netif->name[1] = 'i';
    netif->mtu = 1500;
    netif->output = drop;
#if LWIP_IPV6
    netif->output_ip6 = drop6;
#endif
    return ERR_OK;
}

void started(void* arg)
{
    sys_sem_signal(static_cast<sys_sem_t*>(arg));
}

// lwIP, started once for all the tests, with the interface that drops, and
// the names they look up.
void start_lwip()
{
    // Before Catch's session, so without its REQUIRE.
    sys_sem_t done;
    if (sys_sem_new(&done, 0) != ERR_OK) {
        fprintf(stderr, "failed to start lwIP\n");
        exit(2);
    }
    tcpip_init(started, &done);
    sys_arch_sem_wait(&done, 0);
    sys_sem_free(&done);

    LOCK_TCPIP_CORE();
    ip4_addr_t addr;
    ip4_addr_t mask;
    ip4_addr_t gw;
    IP4_ADDR(&addr, 10, 255, 0, 1);
    IP4_ADDR(&mask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 0, 0, 0, 0);
    netif_add(&silent_netif, &addr, &mask, &gw, nullptr, silent_init, tcpip_input);
    netif_set_up(&silent_netif);
    netif_set_link_up(&silent_netif);
    ip_addr_t loopback;
    ipaddr_aton("127.0.0.1", &loopback);
    dns_local_addhost("server.test", &loopback);
    UNLOCK_TCPIP_CORE();
}

}

int main(int argc, char* argv[])
{
    start_lwip();
    return Catch::Session().run(argc, argv);
}
