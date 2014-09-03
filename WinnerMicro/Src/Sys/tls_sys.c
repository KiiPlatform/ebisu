/**************************************************************************
 * File Name                   : tls_sys.c
 * Author                       :
 * Version                      :
 * Date                          :
 * Description                 :
 *
 * Copyright (c) 2014 Winner Microelectronics Co., Ltd. 
 * All rights reserved.
 *
 ***************************************************************************/

#include "wm_osal.h"
#include "tls_sys.h"
#include "wm_mem.h"
#include "wm_debug.h"
#include "wm_params.h"
#include "wm_regs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wm_wifi.h"
#include "wm_netif.h"


#define      SYS_TASK_STK_SIZE          256 
tls_os_queue_t *msg_queue;
#if TLS_CONFIG_TLS_DEBUG
void TLS_DBGPRT_DUMP(char *p, u32 len)
{
    int i;
	if (tls_debug_level & DBG_DUMP)
    {
        printf("dump length : %d\n", len);
        for (i=0;i<len;i++) {
            printf("%02X ", p[i]);
            if ((i+1)%16 == 0 && (i+1)%32 != 0) {
                printf("- ");
            }
            if ((i+1)%32 == 0) {
                printf("\n");
            }
			if (i==2000) {
				printf("\n");
				break;
			}
				
        }
        printf("\n");
    }
}
#endif
#define ip_addr_set_zero(ipaddr)      ((ipaddr)->addr = 0)
static void sys_net_up()
{
    struct ip_addr ip_addr, net_mask, gateway, dns1, dns2;
    struct tls_param_ip ip_param;
	u8 mode;
#if TLS_CONFIG_AP
	u8 dnsname[32];
#endif
	
    tls_param_get(TLS_PARAM_ID_IP, &ip_param, FALSE);
	tls_param_get(TLS_PARAM_ID_WPROTOCOL, (void* )&mode, FALSE);
    if (ip_param.dhcp_enable && 
            (mode != IEEE80211_MODE_AP)) {
        ip_addr_set_zero(&ip_addr);
        ip_addr_set_zero(&net_mask);
        ip_addr_set_zero(&gateway); 
        tls_netif_set_addr(
                &ip_addr, &net_mask, &gateway);
         tls_dhcp_start();
    } else {
        tls_dhcp_stop();
		memcpy((char *)&ip_addr.addr, &ip_param.ip, 4);
        memcpy((char *)&net_mask.addr, &ip_param.netmask, 4);
        memcpy((char *)&gateway.addr, &ip_param.gateway, 4);
        tls_netif_set_addr(
                &ip_addr, &net_mask, &gateway); 
        memcpy((char *)&dns1.addr, &ip_param.dns1, 4);
        memcpy((char *)&dns2.addr, &ip_param.dns2, 4);
        tls_netif_dns_setserver(0, &dns1);
        tls_netif_dns_setserver(1, &dns2);
        tls_netif_set_up();	
    }
	
#if TLS_CONFIG_AP
    if (mode == IEEE80211_MODE_AP){	
        tls_dhcps_start();
        tls_param_get(TLS_PARAM_ID_DNSNAME, dnsname, 0);
        tls_dnss_start(dnsname);
    }
#endif	
}

static void sys_net_down()
{
#if TLS_CONFIG_AP
	u8 mode;
#endif

    tls_netif_set_down();

	/* Try to reconnect if auto_connect is set*/
	tls_auto_reconnect();
	
#if TLS_CONFIG_AP
	tls_param_get(TLS_PARAM_ID_WPROTOCOL, (void* )&mode, FALSE);
	if (mode == IEEE80211_MODE_AP){
		DHCPS_Stop();
	}
#endif
}	

void tls_auto_reconnect(void)
{
	struct tls_param_key  key;
	struct tls_param_ssid ssid;
	struct tls_param_bssid bssid;
	u8 auto_reconnect=0xff;

	tls_wifi_auto_connect_flag(WIFI_AUTO_CNT_FLAG_GET, &auto_reconnect);
	if(auto_reconnect == WIFI_AUTO_CNT_OFF)
		return;
	else if(auto_reconnect == WIFI_AUTO_CNT_TMP_OFF){
		auto_reconnect = WIFI_AUTO_CNT_ON;
		tls_wifi_auto_connect_flag(WIFI_AUTO_CNT_FLAG_SET, &auto_reconnect);
		return; //tmparary return, for active "DISCONNECT" , such as AT CMD
	}
	
	tls_param_get(TLS_PARAM_ID_BSSID, (void *)&bssid, true);
	
	if(bssid.bssid_enable){
		tls_param_get(TLS_PARAM_ID_KEY, (void *)&key, true);
		tls_wifi_connect_by_bssid(bssid.bssid, key.psk, key.key_length);
	}else{
		tls_param_get(TLS_PARAM_ID_SSID, (void *)&ssid, true);
		tls_param_get(TLS_PARAM_ID_KEY, (void *)&key, true);
		
		tls_wifi_connect(ssid.ssid, ssid.ssid_len, key.psk, key.key_length);
	}
	
	return; 
}


/*
 * sys task stack
 */
u32      sys_task_stk[SYS_TASK_STK_SIZE];     

void tls_sys_task(void *data)
{
    u8 err;
    u32 *msg ;
    u8 oneshotflag = 0;
    u8 auto_mode = 0;
    for (;;) {
        err = tls_os_queue_receive(msg_queue, (void **)&msg, 0, 0);
        if (!err) {
            switch((u32)msg) {
                case SYS_MSG_NET_UP:
                    sys_net_up();
                    break;

                case SYS_MSG_NET_DOWN:
                    sys_net_down();
                    break;
				case SYS_MSG_CONNECT_FAILED:
					tls_auto_reconnect();
					break;
               case SYS_MSG_AUTO_MODE_RUN:	
			   		tls_auto_reconnect();
			   break;				
		        default:
		            break;
            }
        } else {

        }
    }
}

void tls_sys_auto_mode_run(void)
{
    tls_os_queue_send(msg_queue, (void *)SYS_MSG_AUTO_MODE_RUN, 0); 
}

static void tls_sys_net_up(void)
{
    tls_os_queue_send(msg_queue, 
            (void *)SYS_MSG_NET_UP, 0);
}
static void tls_sys_net_down(void)
{
    tls_os_queue_send(msg_queue, 
            (void *)SYS_MSG_NET_DOWN, 0);
}
static void tls_sys_connect_failed(void)
{
    tls_os_queue_send(msg_queue, 
            (void *)SYS_MSG_CONNECT_FAILED, 0);
}

static void sys_net_status_changed(u8 status)
{
    switch(status)
    {
        case NETIF_WIFI_JOIN_SUCCESS:
            tls_sys_net_up();
            break;
		case NETIF_WIFI_JOIN_FAILED:
			tls_sys_connect_failed();
			break;
        case NETIF_WIFI_DISCONNECTED:
            tls_sys_net_down();
            break;
        default:
            break;
    }
}
int tls_sys_init()
{
    int err;
    void *msg;

    /* create messge queue */
#define SYS_MSG_SIZE     20
    msg  = tls_mem_alloc(SYS_MSG_SIZE * sizeof(void *));
    if (!msg)
        return -1;

    err = tls_os_queue_create(&msg_queue,
            msg,
            SYS_MSG_SIZE, 0);
    if (err)
        return -1;

    /* create task */
    tls_os_task_create(NULL, NULL,
            tls_sys_task,
            (void *)0,
            (void *)&sys_task_stk,          /* 任务栈的起始地址 */
            SYS_TASK_STK_SIZE * sizeof(u32), /* 任务栈的大小     */
            TLS_SYS_TASK_PRIO,
            0);

    tls_netif_add_status_event(sys_net_status_changed);

    return 0;

}

