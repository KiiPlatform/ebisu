#ifndef LAN9118_NETIF_H
#define LAN9118_NETIF_H

#include <lwip/err.h>
#include <lwip/netif.h>

/* lwIP's interface on the board's LAN9118, for netif_add(), from lwIP's
   thread, as it waits for the controller. Its own task takes what the
   controller receives to netif->input. ERR_OK on success. */
err_t lan9118_netif_init(struct netif* netif);

#endif /* LAN9118_NETIF_H */
