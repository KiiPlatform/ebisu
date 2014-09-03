/***************************************************************************** 
* 
* File Name : wm_flash_demo.c 
* 
* Description: flash demo function 
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
 
#if DEMO_FLASH

#define TEST_SPEED_SIZE		2*1024
int flash_demo(char *buf)
{
	u8 testbuf[40];
	u8 testbuf2[40];
	u8 *testspeed;
	u32 time;
	u16 i;

	tls_spi_slave_sel(SPI_SLAVE_FLASH);
	//tls_spi_trans_type(0);
	tls_spi_setup(TLS_SPI_MODE_0, TLS_SPI_CS_LOW, 10000000);
#if 1
	memset(testbuf, 0x5a,32);
	tls_fls_write(0xf0000, testbuf, 32);

	memset(testbuf2, 0, 32);
	tls_fls_read(0xf0000, testbuf2, 1);
	printf("\n%x\n",testbuf2[0]);
	
	tls_fls_read(0xf0000, testbuf2,32);
	printf("\n%x\n",testbuf2[0]);
	if(0 == memcmp(testbuf,testbuf2,32))
	{
		printf("\nread write flash ok\n");
	}
	else
	{
		printf("\nread write flash fail\n");
	}
#endif	
#if 1
	testspeed = malloc(TEST_SPEED_SIZE);
	if(NULL == testspeed)
		return -1;
	memset(testspeed,0xa5,TEST_SPEED_SIZE);
	time = OSTimeGet();
	printf("\ntime=%d\n",time);
	//for(i = 0;i < 1;i ++)
	//while(1)
		tls_fls_write(0xf0000, testspeed, TEST_SPEED_SIZE);
	time = OSTimeGet();
	printf("\nw time=%d\n",time);
	memset(testspeed,0,TEST_SPEED_SIZE);
	time = OSTimeGet();
	printf("\ntime=%d\n",time);
	for(i = 0;i < 1000;i ++)
	{
		memset(testspeed,0,TEST_SPEED_SIZE);
		tls_fls_read(0xf0000, testspeed,TEST_SPEED_SIZE);
	}
	time = OSTimeGet();
	printf("\nr time=%d\n",time);
	printf("\nspeed[0]= %x,mid=%x,speed end=%x\n",testspeed[0],testspeed[255], testspeed[TEST_SPEED_SIZE - 1]);
	if(testspeed[0] == 0xa5 && testspeed[TEST_SPEED_SIZE - 1] == 0xa5)
	{
		printf("\nok\n");
	}
	free(testspeed);

#endif	

	return WM_SUCCESS;
}

#endif
