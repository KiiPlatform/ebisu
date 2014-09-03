/***************************************************************************** 
* 
* File Name : wm_socket_demo.c 
* 
* Description: socket demo function 
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
							
#if DEMO_STD_SOCKET_CLIENT
#define    DEMO_SOCK_C_TASK_SIZE      256
OS_EVENT *demo_sock_c_q = NULL;
void *demo_sock_c_queue[DEMO_QUEUE_SIZE];
static OS_STK DemoSockCTaskStk[DEMO_SOCK_C_TASK_SIZE]; 

extern ST_Demo_Sys gDemoSys;
static OS_EVENT *sock_receive_q;
static void *sock_receive_queue[DEMO_QUEUE_SIZE];
static OS_STK SKRCVTaskStk[DEMO_SOCK_C_TASK_SIZE]; 
extern OS_EVENT *demo_q;
extern u8 RemoteIp[4];

static void demo_sock_c_task(void *sdata);


int create_client_socket_demo(void)
{
	struct sockaddr_in pin;

	memset(&pin, 0, sizeof(struct sockaddr));
	pin.sin_family=AF_INET;                 //AF_INET表示使用IPv4
	gDemoSys.socket_num = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

	memcpy((char *)&pin.sin_addr.s_addr, (char *)RemoteIp, 4);

	pin.sin_port=htons(RemotePort);
	printf("\nserver ip=%d.%d.%d.%d,port=%d\n",RemoteIp[0],RemoteIp[1],RemoteIp[2],RemoteIp[3],RemotePort);

	printf("\nsocket num=%d\n",gDemoSys.socket_num);
	if (connect(gDemoSys.socket_num, (struct sockaddr *)&pin, sizeof(struct sockaddr)) != 0)
	{
		printf("connect failed! socket num=%d\n", gDemoSys.socket_num);
		closesocket(gDemoSys.socket_num);
		OSQPost(demo_sock_c_q,(void *)DEMO_MSG_SOCKET_ERR);	
		return -1;
	}
	else
	{
		gDemoSys.socket_ok = true;
		gDemoSys.is_raw = 0;
	//	OSQPost(demo_q,(void *)DEMO_MSG_OPEN_UART);
	}	

	return 0;
}

int socket_client_demo(void)
{
	struct tls_ethif * ethif;
	
	ethif = tls_netif_get_ethif();
	printf("\nip=%d.%d.%d.%d\n",ip4_addr1(&ethif->ip_addr.addr),ip4_addr2(&ethif->ip_addr.addr),
		ip4_addr3(&ethif->ip_addr.addr),ip4_addr4(&ethif->ip_addr.addr));

	DemoStdSockOneshotSendMac();
	
	return create_client_socket_demo();

}
	
static void sock_client_recv_task(void *sdata)
{
	ST_Demo_Sys *sys = (ST_Demo_Sys *)sdata;
	int ret = 0;
	INT8U error;

	OSTimeDly(100);
	OSQPend(sock_receive_q, 0, &error);

	for(;;) 
	{
		if(sys->socket_ok)
		{
			ret = 0;
			printf("start to recv data from socket num=%d\n", sys->socket_num);
			ret = recv(sys->socket_num, sys->sock_rx, DEMO_BUF_SIZE, 0);
			sys->sock_data_len = ret;
			sys->recvlen += sys->sock_data_len;
			printf("\n%d\n",sys->recvlen);
			if(ret>0)
			{
				OSQPost(demo_sock_c_q,(void *)DEMO_MSG_SOCKET_RECEIVE_DATA);
			}
			else
			{
				sys->socket_ok = false;
				closesocket(sys->socket_num);
				printf("closesocket: %d\n", sys->socket_num);
				sys->socket_num = 0;
				OSQPost(demo_sock_c_q,(void *)DEMO_MSG_SOCKET_ERR);	
			}
			continue;
		}
		OSTimeDly(100);
	}
}

int CreateSockClientDemoTask(char *buf)
{
	demo_sock_c_q = OSQCreate((void **)&demo_sock_c_queue, DEMO_QUEUE_SIZE);
	//用户处理socket相关的消息
	tls_os_task_create(NULL, NULL,
			demo_sock_c_task,
                    (void *)&gDemoSys,
                    (void *)DemoSockCTaskStk,          /* 任务栈的起始地址 */
                    DEMO_SOCK_C_TASK_SIZE * sizeof(u32), /* 任务栈的大小     */
                    DEMO_SOCKET_C_TASK_PRIO,
                    0);

	sock_receive_q = OSQCreate((void **)&sock_receive_queue, DEMO_QUEUE_SIZE);
	//用于socket数据的接收
	tls_os_task_create(NULL, NULL,
			sock_client_recv_task,
                    (void *)&gDemoSys,
                    (void *)SKRCVTaskStk,          /* 任务栈的起始地址 */
                    DEMO_SOCK_C_TASK_SIZE * sizeof(u32), /* 任务栈的大小     */
                    DEMO_SOCKET_RECEIVE_TASK_PRIO,
                    0);

	return WM_SUCCESS;
}


static void sock_c_net_status_changed_event(u8 status )
{
	switch(status)
	{
		case NETIF_WIFI_JOIN_FAILED:
			OSQPost(demo_sock_c_q,(void *)DEMO_MSG_WJOIN_FAILD);
			break;
		case NETIF_WIFI_JOIN_SUCCESS:
			OSQPost(demo_sock_c_q,(void *)DEMO_MSG_WJOIN_SUCCESS);
			break;
		case NETIF_IP_NET_UP:
			OSQPost(demo_sock_c_q,(void *)DEMO_MSG_SOCKET_CREATE);
			break;
		default:
			break;
	}
}

static void demo_sock_c_task(void *sdata)
{
	ST_Demo_Sys *sys = (ST_Demo_Sys *)sdata;
	void *msg;
	INT8U error;
	struct tls_ethif * ethif = tls_netif_get_ethif();

	printf("\nsock c task\n");
//用于socket接收数据使用
	sys->sock_rx = malloc(DEMO_BUF_SIZE);
	if(NULL == sys->sock_rx)
	{
		printf("\nmalloc socket rx fail\n");
		return;
	}
	memset(sys->sock_rx, 0, DEMO_BUF_SIZE);	
//////	

	if(ethif->status)	//已经在网
	{
		OSQPost(demo_sock_c_q,(void *)DEMO_MSG_SOCKET_CREATE);
	}
	else
	{
		tls_wifi_set_oneshot_flag(1);		/*一键配置使能*/
		printf("\nwait one shot......\n");
	}
	tls_netif_add_status_event(sock_c_net_status_changed_event);

	for(;;) 
	{
    		msg = OSQPend(demo_sock_c_q, 0, &error);
		//printf("\n msg =%d\n",msg);
		switch((u32)msg)
		{
			case DEMO_MSG_WJOIN_SUCCESS:
				printf("\njoin net success\n");
				break;
				
			case DEMO_MSG_SOCKET_CREATE:
				//create_raw_socket_client_demo();
				if(0 == socket_client_demo())
					OSQPost(sock_receive_q, (void *)1);
				break;
				
			case DEMO_MSG_WJOIN_FAILD:
				if(sys->socket_num > 0)
				{
					sys->socket_num = 0;
					sys->socket_ok = FALSE;
				}
				printf("join net faild\n");
				break;

			case DEMO_MSG_SOCKET_RECEIVE_DATA:
				/*收到数据，自行处理*/
#if	(TLS_CONFIG_UART)
				tls_uart_tx(sys->sock_rx,sys->sock_data_len);	/*发到串口上显示*/
#endif				
				break;

			case DEMO_MSG_SOCKET_ERR:
				OSTimeDly(200);
				printf("\nsocket err\n");
				if(0 == create_client_socket_demo())
					OSQPost(sock_receive_q, (void *)1);	
				break;

			default:
				break;
		}
	}

}


#endif

int socket_std_send_data_demo(ST_Demo_Sys *sys)
{	
	int err = 0;
#if (DEMO_STD_SOCKET_CLIENT || DEMO_STD_SOCKET_SERVER)
	u16 len,wptr;
	if(NULL == sys)
		return -1;
	
	wptr = sys->wptr;
	if(sys->rptr < wptr)
	{
		len = wptr - sys->rptr;
		memcpy(sys->txbuf,sys->rxbuf + sys->rptr,len);
		sys->rptr += len;		
		//printf("uart receive: %s\n",  sys->txbuf);
		err = send(sys->socket_num, sys->txbuf , len, 0);
		sys->translen += len;
		//printf("\n1=%d\n",sys->translen);
		sys->overflag = 0;
	}
	else if(sys->rptr > wptr ||(sys->rptr == wptr && sys->overflag))
	{
		len = DEMO_BUF_SIZE - sys->rptr;
		memcpy(sys->txbuf,sys->rxbuf + sys->rptr,len);
		memcpy(sys->txbuf + len, sys->rxbuf, wptr);
		len += wptr;
		sys->rptr = wptr;
		//printf("uart receive: %s\n",  sys->txbuf);
		err = send(sys->socket_num, sys->txbuf , len, 0);
		sys->translen += len;
		//printf("\n2=%d\n",sys->translen);
		sys->overflag = 0;
	}
#endif
	return err;
}


