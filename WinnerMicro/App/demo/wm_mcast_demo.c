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

#if DEMO_UDP_MULTI_CAST
#define    DEMO_MCAST_TASK_SIZE      256
OS_EVENT *demo_mcast_q = NULL;
void *demo_mcast_queue[DEMO_QUEUE_SIZE];
static OS_STK DemoMCastTaskStk[DEMO_MCAST_TASK_SIZE]; 

extern ST_Demo_Sys gDemoSys;
static void demo_mcast_task(void *sdata);

#define MPORT 5100
u8 MCASTIP[4] = {224,1,2,1};
#define MCAST_BUF_SIZE	1024
int mcastsendrcv(bool snd)
{
  int s;
  int ret;
  int size;
  int ttl=10;
  int loop=0;
  int times=0;
  int i=65;
  char *buffer = NULL; 
  socklen_t socklen;
  struct sockaddr_in localaddr,fromaddr,Multi_addr;
 struct ip_mreq mreq;
  s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if(s<0){
   printf("socket error");
    return -1;
  }
  printf("multi case socket num=%d\n", s);
  Multi_addr.sin_family=AF_INET;
 Multi_addr.sin_port=htons(MPORT);
  memcpy((char *)&Multi_addr.sin_addr, (char *)MCASTIP, 4);

  localaddr.sin_family=AF_INET;
  localaddr.sin_port=htons(MPORT);
  localaddr.sin_addr.s_addr=htonl(0x00000000UL);
  fromaddr.sin_family=AF_INET;
  fromaddr.sin_port=htons(MPORT);
  fromaddr.sin_addr.s_addr=htonl(0x00000000UL);
 ret=bind(s,(struct sockaddr*)&localaddr,sizeof(localaddr));
if(ret<0){
  printf("bind error");
  return -1;
}
//设置多播TTL值
 if(setsockopt(s,IPPROTO_IP,IP_MULTICAST_TTL,&ttl,sizeof(ttl))<0){
    printf("IP_MULTICAST_TTL");
    return -1;
  }
//设置数据是否发送到本地回环接口
if(setsockopt(s,IPPROTO_IP,IP_MULTICAST_LOOP,&loop,sizeof(loop))<0){
  printf("IP_MULTICAST_LOOP");
  return -1;
 }
 memcpy((char *)&mreq.imr_multiaddr.s_addr, (char *)MCASTIP, 4);
 mreq.imr_interface.s_addr=htonl(0x00000000UL);
 //客户端加入多播组
if(setsockopt(s,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq))<0){
  printf("IP_ADD_MEMBERSHIP");
  return -1;
}
buffer = malloc(MCAST_BUF_SIZE);
if(NULL == buffer)
	return -1;
memset(buffer,0,MCAST_BUF_SIZE);
//循环接收或发送数据
if(snd)
{
for(times=0;times<20;times++){
  memset(buffer,i,MCAST_BUF_SIZE);
  size=sendto(s,buffer,MCAST_BUF_SIZE,0,(struct sockaddr*)&Multi_addr,sizeof(Multi_addr));
  printf("multi case sendto buf=%s, size=%d\n", buffer, size);
  OSTimeDly(50);
  i++;
  memset(buffer,0,MCAST_BUF_SIZE);
}
}
else
{
for(times=0;times<5;times++)
{
	socklen = sizeof(fromaddr);
	recvfrom(s,buffer,MCAST_BUF_SIZE,0,(struct sockaddr*)&fromaddr, &socklen);
	printf("multi case recvfrom buf=%s, socklen=%d\n", buffer, socklen);
	memset(buffer,0,MCAST_BUF_SIZE);
}
}
free(buffer);
//离开多播组
ret=setsockopt(s,IPPROTO_IP,IP_DROP_MEMBERSHIP,&mreq,sizeof(mreq));
if(ret<0){
  printf("IP_DROP_MEMBERSHIP");
  return -1;
}
closesocket(s);
return 0;
}

int CreateMCastDemoTask(char *buf)
{
	demo_mcast_q = OSQCreate((void **)&demo_mcast_queue, DEMO_QUEUE_SIZE);
	//用户处理socket相关的消息
	tls_os_task_create(NULL, NULL,
			demo_mcast_task,
                    (void *)&gDemoSys,
                    (void *)DemoMCastTaskStk,          /* 任务栈的起始地址 */
                    DEMO_MCAST_TASK_SIZE * sizeof(u32), /* 任务栈的大小     */
                    DEMO_MCAST_TASK_PRIO,
                    0);

	return WM_SUCCESS;
}


static void mcast_net_status_changed_event(u8 status )
{
	switch(status)
	{
		case NETIF_WIFI_JOIN_FAILED:
			OSQPost(demo_mcast_q,(void *)DEMO_MSG_WJOIN_FAILD);
			break;
		case NETIF_WIFI_JOIN_SUCCESS:
			OSQPost(demo_mcast_q,(void *)DEMO_MSG_WJOIN_SUCCESS);
			break;
		case NETIF_IP_NET_UP:
			OSQPost(demo_mcast_q,(void *)DEMO_MSG_SOCKET_CREATE);
			break;
		default:
			break;
	}
}

static void demo_mcast_task(void *sdata)
{
//	ST_Demo_Sys *sys = (ST_Demo_Sys *)sdata;
	void *msg;
	INT8U error;
	struct tls_ethif * ethif = tls_netif_get_ethif();

	printf("\nmcast task\n");
	
	if(ethif->status)	//已经在网
	{
		OSQPost(demo_mcast_q,(void *)DEMO_MSG_SOCKET_CREATE);
	}
	else
	{
		tls_wifi_set_oneshot_flag(1);		/*一键配置使能*/
		printf("\nwait one shot......\n");
	}
	tls_netif_add_status_event(mcast_net_status_changed_event);

	for(;;) 
	{
    		msg = OSQPend(demo_mcast_q, 0, &error);
		//printf("\n msg =%d\n",msg);
		switch((u32)msg)
		{
			case DEMO_MSG_WJOIN_SUCCESS:
				printf("\njoin net success\n");
				break;
				
			case DEMO_MSG_SOCKET_CREATE:
				mcastsendrcv(1);
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

