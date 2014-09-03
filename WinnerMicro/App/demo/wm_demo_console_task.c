/***************************************************************************** 
* 
* File Name : wm_demo_task.c
* 
* Description: demo task
* 
* Copyright (c) 2014 Winner Micro Electronic Design Co., Ltd. 
* All rights reserved. 
* 
* Author : dave
* 
* Date : 2014-6-14
*****************************************************************************/ 
#include <string.h>
#include "wm_include.h"

#if DEMO_CONSOLE
#define    DEMO_TASK_SIZE      256
OS_EVENT *demo_q = NULL;
void *demo_queue[DEMO_QUEUE_SIZE];
static OS_STK DemoTaskStk[DEMO_TASK_SIZE]; 
ST_Demo_Sys gDemoSys;


void demo_console_task(void *sdata);

extern void demo_uart_malloc(void);
extern void demo_uart_open(void);
extern int demo_uart_cmd_parse(ST_Demo_Sys *sys);
extern int socket_raw_send_data_demo(ST_Demo_Sys *sys);
extern int socket_std_send_data_demo(ST_Demo_Sys *sys);
extern int demo_console_show_help(void);


void CreateDemoTask(void)
{
	memset(&gDemoSys, 0 ,sizeof(ST_Demo_Sys));
	demo_q = OSQCreate((void **)&demo_queue, DEMO_QUEUE_SIZE);
	tls_os_task_create(NULL, NULL,
			demo_console_task,
                    (void *)&gDemoSys,
                    (void *)DemoTaskStk,          /* 任务栈的起始地址 */
                    DEMO_TASK_SIZE * sizeof(u32), /* 任务栈的大小     */
                    DEMO_TASK_PRIO,
                    0);
}



int opentx = 1;

//该任务使用uart1 作为测试控制台，可以输入字符串命令测试其他的demo
//测试socket收发数据时，可以通过该控制台向外传输数据
void demo_console_task(void *sdata)
{
	ST_Demo_Sys *sys = (ST_Demo_Sys *)sdata;
	void *msg;
	INT8U error;
	int ret = 0;
	u8 automode = WIFI_AUTO_CNT_OFF;
	
	demo_console_show_help();
	//起demo之后清除自动联网，和自动工作模式，避免系统自动创建socket和socket demo冲突
	tls_wifi_auto_connect_flag( WIFI_AUTO_CNT_FLAG_SET, &automode);
	tls_param_set(TLS_PARAM_ID_AUTOMODE, (void *)&automode, (bool)FALSE);
	
	demo_uart_malloc();
#if TLS_CONFIG_UART
	tls_user_uart_set_baud_rate(115200);
#endif	
	OSQPost(demo_q,(void *)DEMO_MSG_OPEN_UART);
	for(;;) 
	{
    		msg = OSQPend(demo_q, 0, &error);
		//printf("\n msg =%d\n",msg);
		switch((u32)msg)
		{
			case DEMO_MSG_OPEN_UART:
				demo_uart_open();
				break;
			case DEMO_MSG_UART_RECEIVE_DATA:
				ret = demo_uart_cmd_parse(sys);	//解析其他demo的测试命令字
				if(DEMO_CONSOLE_CMD == ret)	//先进行命令解析，看是否是模拟的命令字
				{
					memset(sys->rxbuf , 0, DEMO_BUF_SIZE);	/*命令传输完成之后清buf*/
					sys->rptr = 0;
					sys->wptr = 0;
				}
				else if(DEMO_CONSOLE_SHORT_CMD == ret)
				{
					//param not passed all, do nothing.
				}
				else if(sys->socket_ok)		//配合socket demo做数据发送
				{
					if(sys->is_raw)
					{
						socket_raw_send_data_demo(sys);
					}
					else
					{
						socket_std_send_data_demo(sys);
					}
				}	
				if(sys->MsgNum)
					sys->MsgNum --;
				break;
			default:
				break;
		}
	}
}

#endif	//DEMO_CONSOLE

