#ifndef NET_H
#define NET_H

/* Waits for the SDK to start the Wi-Fi, joins the network the build is given,
   EBISU_SAMPLE_WIFI_SSID, or, without one, waits for one joined from the
   SDK's console, and returns once the station has an address from DHCP. */
void net_wait_until_up(void);

#endif /* NET_H */
