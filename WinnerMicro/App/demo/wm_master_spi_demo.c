/***************************************************************************** 
* 
* File Name : wm_master_spi_demo.c 
* 
* Description: SPI master demo function 
* 
* Copyright (c) 2014 Winner Micro Electronic Design Co., Ltd. 
* All rights reserved. 
* 
* Author : dave
* 
* Date : 2014-6-2 
*****************************************************************************/ 
#include <string.h>
#include "wm_include.h"

#if DEMO_MASTER_SPI

#define TEST_SPI_SPEED_SIZE		2*1024

void TestSPIReceiveData(void)
{
	u8 cmd[40];
	u8 buf[40];
	u16 temp = 0;
	u16 i;

	memset(cmd,0,32);
	memset(buf,0,32);
	cmd[0] = 0x06;
	tls_spi_write_then_read(cmd, 1, buf, 2);
	printf("\nrx buf[%x][%x]\n",buf[0],buf[1]);	

	if(buf[0] & 0x01)	//数据或命令已经准备好
	{
		temp = 0;
		cmd[0] = 0x02;
		tls_spi_write_then_read(cmd,1,buf,2);
		temp |= buf[0];
		temp |= buf[1] << 8;
		printf("\ntemp=%d\n",temp);
		if(temp > 0)
		{
//这里最好判断一下数据长度是否4的整数倍，留4个byte通过0x10命令接收
			cmd[0] = 0;
			tls_spi_write_then_read(cmd,1,buf,temp - 1);

			cmd[0] = 0x10;
			tls_spi_write_then_read(cmd,1,&buf[temp - 1],1);
			for(i = 0;i < temp; i++)
			{
				printf("[%d]=[%x]\r\n",i,buf[i]);
			}
		}
	}
}

u8 TestSPITransferData(void)
{
	u32 i = 0;
	u8 cmd[40];
	u8 buf[40];
	u8 *TXBuf;
	int time;
//	int ret;

	TXBuf = malloc(TEST_SPI_SPEED_SIZE);
	if(NULL == TXBuf)
		return 0;
	memset(TXBuf,0xaa,TEST_SPI_SPEED_SIZE);
	
	memset(cmd,0,32);
	memset(buf,0,32);
	cmd[0] = 0x03;
	while(1)
	{		
		tls_spi_write_then_read(cmd, 1, buf, 2);
		if(buf[0] & 0x01)
			break;
		OSTimeDly(1);
		printf("\ncan not tx data\n");
	}

	cmd[0] =0x00;
	*TXBuf = 0x90;	//命令字
	time = OSTimeGet();
	printf("\ntime1 = %d\n",time);
	for(i = 0;i < 1000;i ++)
	{
		tls_spi_write(TXBuf,TEST_SPI_SPEED_SIZE);
	}
	time = OSTimeGet();
	printf("\ntime2 = %d\n",time);
	printf("\ntx cnt =%d\n",i);	
	free(TXBuf);
	
	return	0;
}

u8 TestSPITransferCMD(void)
{
	u8 buf[40];
	u8 cmd[100];
	u16 ret;

	memset(buf,0,32);
	memset(cmd,0,100);

	cmd[0] = 0x03;	//命令字一个byte之后，不能跟0，否则命令下面识别不对

	tls_spi_write_then_read(cmd, 1, buf, 2);
	printf("\n[%x][%x]\n",buf[0],buf[1]);
	//OSTimeDly(10);

#if 1
	if(buf[0]&0x02)
	{

		cmd[0] = 0x91;
		cmd[1] = 0xaa;
		cmd[2] = 0x01;	//TYPE 命令端口传输 jj
		cmd[3] = 0x00;	//
		cmd[4] = 0x04;	//数据长度 4byte
		cmd[5] = 0x00;	//序号0
		cmd[6] = 0x00;	//FLG
		cmd[7] = 0x00;	//DA
		cmd[8] = 0X05;	//CHK
/*获取版本号*/

		cmd[9] = 0X01;	//精简指令获取版本号
		cmd[10] = 0X07;	
		cmd[11] = 0X00;	
		cmd[12] = 0X00;	
		cmd[13] = 0X08;	//CHK	
		cmd[14] = 0;
		cmd[15] = 0;
		cmd[16] = 0;
		//while(1)
			{
		//	temp = *((u32 *)cmd);
		//	printf("\ntemp = %x\n",temp);
			ret = tls_spi_write(cmd,50);	//写精简指令
			printf("\nret =%d\n",ret);
		//	OSTimeDly(10);
			}

		
	}
#endif
return 0;
}


int spi_demo(char *buf)
{
//	int time;
	
	tls_spi_slave_sel(SPI_SLAVE_CARD);
		
	tls_spi_trans_type(2);
	tls_spi_setup(TLS_SPI_MODE_0, TLS_SPI_CS_LOW, 10000000);
//	TestSPIReceiveData();	
//	TestSPITransferCMD();
#if 0
	time = OSTimeGet();
	printf("\ntime1 = %d\n",time);
	for(i = 0;i < 1000;i ++)
#endif		
		{
	TestSPITransferData();
		}
#if 0
	time = OSTimeGet();
	printf("\ntime1 = %d\n",time);
#endif	

	return WM_SUCCESS;
}


#endif
