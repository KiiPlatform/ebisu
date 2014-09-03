/***************************************************************************** 
* 
* File Name : wm_socket_fwup_demo.c 
* 
* Description: socket fwup demo function 
* 
* Copyright (c) 2014 Winner Micro Electronic Design Co., Ltd. 
* All rights reserved. 
* 
* Author : wanghf
* 
* Date : 2014-6-11 
*****************************************************************************/ 

#include <string.h>
#include "wm_include.h"
#include "wm_socket_fwup.h"

#if DEMO_SOCKET_FWUP
#define    DEMO_SOCK_FWUP_TASK_SIZE      256
OS_EVENT *demo_sock_fwup_q = NULL;
void *demo_sock_fwup_queue[DEMO_QUEUE_SIZE];
static OS_STK DemoSockFwupTaskStk[DEMO_SOCK_FWUP_TASK_SIZE]; 
static void demo_sock_fwup_task(void *sdata);
extern ST_Demo_Sys gDemoSys;
static int fwup_skt_num = -1;
struct tls_socket_desc sock_desc;
void create_socket_fwup_demo(void)
{
	struct tls_ethif * ethif;
	ethif = tls_netif_get_ethif();
	printf("\nip=%d.%d.%d.%d\n",ip4_addr1(&ethif->ip_addr.addr),ip4_addr2(&ethif->ip_addr.addr),
		ip4_addr3(&ethif->ip_addr.addr),ip4_addr4(&ethif->ip_addr.addr));

	/*oneshot config broadcast mac addr*/
	DemoRawSockOneshotSendMac();
	
	if(fwup_skt_num<0)
	{
		memset(&sock_desc, 0, sizeof(struct tls_socket_desc));
		sock_desc.cs_mode = SOCKET_CS_MODE_SERVER;
		sock_desc.acceptf = socket_fwup_accept;
		sock_desc.recvf = socket_fwup_recv;
		sock_desc.errf = socket_fwup_err;
		sock_desc.pollf = socket_fwup_poll;
		sock_desc.protocol = SOCKET_PROTO_TCP;
		sock_desc.port = SOCKET_FWUP_PORT;
		fwup_skt_num = tls_socket_create(&sock_desc);
	}
}

void close_socket_fwup_demo(void)
{
    if(fwup_skt_num>=0)
    {
        tls_socket_close(fwup_skt_num);
        fwup_skt_num = -1;
    }
}

int CreateSockFwupDemoTask(char *buf)
{
	demo_sock_fwup_q = OSQCreate((void **)&demo_sock_fwup_queue, DEMO_QUEUE_SIZE);
	//用户处理socket相关的消息
	tls_os_task_create(NULL, NULL,
			demo_sock_fwup_task,
                    (void *)&gDemoSys,
                    (void *)DemoSockFwupTaskStk,          /* 任务栈的起始地址 */
                    DEMO_SOCK_FWUP_TASK_SIZE * sizeof(u32), /* 任务栈的大小     */
                    DEMO_SOCK_FWUP_TASK_PRIO,
                    0);
	return WM_SUCCESS;
}


static void sock_fwup_net_status_changed_event(u8 status )
{
	switch(status)
	{
		case NETIF_WIFI_JOIN_FAILED:
			OSQPost(demo_sock_fwup_q,(void *)DEMO_MSG_WJOIN_FAILD);
			break;
		case NETIF_WIFI_JOIN_SUCCESS:
			OSQPost(demo_sock_fwup_q,(void *)DEMO_MSG_WJOIN_SUCCESS);
			break;
		case NETIF_IP_NET_UP:
			OSQPost(demo_sock_fwup_q,(void *)DEMO_MSG_SOCKET_CREATE);
			break;
		default:
			break;
	}
}

static void demo_sock_fwup_task(void *sdata)
{
//	ST_Demo_Sys *sys = (ST_Demo_Sys *)sdata;
	void *msg;
	INT8U error;
	struct tls_ethif * ethif = tls_netif_get_ethif();

	printf("\nsock fwup task\n");
	if(ethif->status)	//已经在网
	{
		OSQPost(demo_sock_fwup_q,(void *)DEMO_MSG_SOCKET_CREATE);
	}
	else
	{
		tls_wifi_set_oneshot_flag(1);		/*一键配置使能*/
		printf("\nwait one shot......\n");
	}
	tls_netif_add_status_event(sock_fwup_net_status_changed_event);

	for(;;) 
	{
    		msg = OSQPend(demo_sock_fwup_q, 0, &error);
		//printf("\n msg =%d\n",msg);
		switch((u32)msg)
		{
			case DEMO_MSG_WJOIN_SUCCESS:
				printf("\njoin net success\n");
				break;
				
			case DEMO_MSG_SOCKET_CREATE:
				create_socket_fwup_demo();
				break;
				
			case DEMO_MSG_WJOIN_FAILD:
				printf("join net faild\n");
				break;

			case DEMO_MSG_SOCKET_ERR:
				printf("\nsocket err\n");
				break;

			default:
				break;
		}
	}

}

#endif

