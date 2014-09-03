/***************************************************************************** 
* 
* File Name : main.c
* 
* Description: main 
* 
* Copyright (c) 2014 Winner Micro Electronic Design Co., Ltd. 
* All rights reserved. 
* 
* Author : dave
* 
* Date : 2014-6-14
*****************************************************************************/ 
#include "wm_include.h"

int main(void)
{
    tls_main();
    OSStart();
    return 0;
}

void CreateUserTask(void)
{
	printf("\n user task\n");
#if DEMO_CONSOLE
	CreateDemoTask();
#endif
//用户自己的task
}

void RestoreParamToDefault(void)
{
#if 0
	struct tls_user_param *user_param = NULL;

	user_param = (struct tls_user_param *)malloc(sizeof(struct tls_user_param));
	if(NULL == user_param)
	{
		return -1;
	}
	memset(user_param, 0, sizeof(*user_param));
	user_param->wireless_protocol = 0;	// sta 0; adhoc 1; ap 2
	user_param->auto_mode = 1;
	user_param->baudrate = 9600;
	user_param->user_port_mode = 0;		// LUart 0; HUart 1; HSPI 2; SDIO 3
	user_param->dhcp_enable = 1;
	user_param->auto_powersave = 0;		
	user_param->ip[0] = 192;
	user_param->ip[1] = 168;
	user_param->ip[2] = 1;
	user_param->ip[3] = 1;
	user_param->netmask[0] = 255;
	user_param->netmask[1] = 255;
	user_param->netmask[2] = 255;
	user_param->netmask[3] = 0;
	user_param->gateway[0] = 192;
	user_param->gateway[1] = 168;
	user_param->gateway[2] = 1;
	user_param->gateway[3] = 1;
	user_param->dns[0] = 192;
	user_param->dns[1] = 168;
	user_param->dns[2] = 1;
	user_param->dns[3] = 1;
	
	user_param->socket_protocol = 0;	// TCP 0; UDP 1
	user_param->socket_client_or_server = 0;	// client 0; server 1
	user_param->socket_port_num = 1000;
	memset(user_param->socket_host, 0, 32);
	memset(user_param->PassWord, '0', 6);	
	
	tls_param_save_user(user_param);
	free(user_param);
	
	tls_param_to_default();
#endif	
}
