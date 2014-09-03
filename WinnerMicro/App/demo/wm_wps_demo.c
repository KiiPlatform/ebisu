/***************************************************************************** 
* 
* File Name : wm_mcast_demo.c 
* 
* Description: mcast demo function 
* 
* Copyright (c) 2014 Winner Micro Electronic Design Co., Ltd. 
* All rights reserved. 
* 
* Author : wanghaifang
* 
* Date : 2014-6-2 
*****************************************************************************/ 

#include <string.h>
#include "wm_include.h"

#if DEMO_WPS

static void wps_net_status_changed_event(u8 status )
{
	struct tls_ethif * ethif;
	
	switch(status)
	{
		case NETIF_WIFI_JOIN_FAILED:
			printf("\nWPS : join net failed\n");
			break;
		case NETIF_WIFI_JOIN_SUCCESS:
			printf("\nWPS : join net success\n");
			break;
		case NETIF_IP_NET_UP:
			ethif = tls_netif_get_ethif();
			printf("\nip=%d.%d.%d.%d\n",ip4_addr1(&ethif->ip_addr.addr),ip4_addr2(&ethif->ip_addr.addr),
					ip4_addr3(&ethif->ip_addr.addr),ip4_addr4(&ethif->ip_addr.addr));
			break;
		default:
			break;
	}
}


int demo_wps_pbc(char *buf)
{
	struct tls_param_ip ip_param;
	int ret = WM_FAILED;
	tls_param_get(TLS_PARAM_ID_IP, &ip_param, FALSE);
	ip_param.dhcp_enable = true;
	tls_param_set(TLS_PARAM_ID_IP, &ip_param, FALSE);

	tls_netif_add_status_event(wps_net_status_changed_event);
#if TLS_CONFIG_WPS
	tls_wifi_set_oneshot_flag(0);

	ret = tls_wps_start_pbc();
#endif
	if(ret == WM_SUCCESS)
		printf("Start WPS pbc mode ... \n");			
	//	else if(ret == WM_WPS_BUSY)
	//		printf("WPS Busy..., waiting for join failed time out\n");			


	return WM_SUCCESS;
}


int demo_wps_pin(char *buf)
{
	int ret = WM_FAILED;
	struct tls_param_ip ip_param;

	tls_param_get(TLS_PARAM_ID_IP, &ip_param, FALSE);
	ip_param.dhcp_enable = true;
	tls_param_set(TLS_PARAM_ID_IP, &ip_param, FALSE);

	tls_netif_add_status_event(wps_net_status_changed_event);
#if TLS_CONFIG_WPS	
	tls_wifi_set_oneshot_flag(0);
	ret = tls_wps_start_pin();
#endif
	if(ret == WM_SUCCESS)
		printf("Start WPS pin mode ... \n");
	//else if(ret == WM_WPS_BUSY)
	//	printf("WPS Busy..., waiting for join failed time out\n");			

	return WM_SUCCESS;
}

int demo_wps_get_pin(char *buf)
{
#if TLS_CONFIG_WPS	
	u8 pin[WPS_PIN_LEN+1];
		
	if(!tls_wps_get_pin(pin))
			printf("Pin code: %s\n", pin);
		
	if(!tls_wps_set_pin(pin, WPS_PIN_LEN))
		printf("Pin set correctly: %s\n", pin);
#endif

	return WM_SUCCESS;
}

#endif 

