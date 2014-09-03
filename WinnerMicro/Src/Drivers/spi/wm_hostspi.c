/***************************************************************************** 
* 
* File Name : wm_hostspi.c 
* 
* Description: host spi Driver Module 
* 
* Copyright (c) 2014 Winner Microelectronics Co., Ltd. 
* All rights reserved. 
* 
* Author : dave
* 
* Date : 2014-6-6
*****************************************************************************/ 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "wm_regs.h"
#include "wm_hostspi.h"
#include "wm_gpio.h"
#include "wm_dma.h"
#include "wm_debug.h"

static struct tls_spi_port *spi_port = NULL;

static void SpiMasterInit(u8 mode, u8 cs_active, u32 fclk)
{

	/* pin config */
	IOCTRL_PIN_CONFIG_REG3 = 0;//(IOCTRL_LSSPI_MS_DO_PIN | IOCTRL_LSSPI_MS_DI_PIN | IOCTRL_LSSPI_MS_CS_PIN | IOCTRL_LSSPI_MS_CLK_PIN);
	
	SPIM_CHCFG_REG = SPI_CLEAR_FIFOS;
	while(SPIM_CHCFG_REG & SPI_CLEAR_FIFOS);
	
	SPIM_CLKCFG_REG = SPI_SCLK_DIVIDER(SPI_GET_SCLK_DIVIDER(fclk));
	SPIM_SPICFG_REG = 0;
	SPIM_SPICFG_REG = SPI_FRAME_FORMAT_MOTO | SPI_SET_MASTER_SLAVE(SPI_MASTER) | mode;/* 0x0004 - FRAM FORMAT : motorola 00 | Master | Rising | IDL LOW */
	SPIM_INTEN_REG = 0xff;/* Disable INT */

	tlsDmaInit();
}

 int spiWaitIdle(void)
{	
	unsigned long regVal;	
	unsigned long timeout=0;	

	do
	{
		timeout ++;
		if(timeout > 0x4FFFFF)		//5s	
			return TLS_SPI_STATUS_EBUSY;
		regVal = SPIM_SPISTATUS_REG;
	}while(regVal & (1<<12));

	return TLS_SPI_STATUS_OK;
}

static int SpiDmaBlockWrite(u8 *data,u32 len,u8 ifusecmd,u32 cmd)
{
	unsigned char dmaCh = 0;
	TLS_DMA_DESCRIPTOR DmaDesc;
	u32 txlen;
	u32 i,blocknum,blocksize;
	int ret = TLS_SPI_STATUS_OK;

	if(NULL == data)
	{
		return TLS_SPI_STATUS_EINVAL;
	}
	if(spiWaitIdle())
		return  TLS_SPI_STATUS_EBUSY;
	spi_port->spibusy = TRUE;
 	SPIM_CHCFG_REG = SPI_CLEAR_FIFOS;
	while(SPIM_CHCFG_REG & SPI_CLEAR_FIFOS);
	
//	printf("\nblock write cmd=%x,len=%d\n",cmd,len);
	if(ifusecmd)
		SPIM_TXDATA_REG = cmd;

	if(len%4)
	{
		txlen = ((len+3)/4)<<2;
	}
	else
	{
		txlen = len;
	}
	blocknum = txlen/SPI_DMA_MAX_TRANS_SIZE;
	
	/* Request DMA Channel */
	dmaCh = tlsDmaRequest(1, TLS_DMA_FLAGS_CHANNEL_SEL(TLS_DMA_SEL_LSSPI_TX) | TLS_DMA_FLAGS_HARD_MODE);
	for(i = 0;i <= blocknum; i ++)
	{
	//	printf("\ni=%d\n",i);
		DmaDesc.src_addr = (int)(data + i*SPI_DMA_MAX_TRANS_SIZE);
		DmaDesc.dest_addr = HR_SPI_TXDATA_REG;
		blocksize = (txlen > SPI_DMA_MAX_TRANS_SIZE)? SPI_DMA_MAX_TRANS_SIZE:txlen;
	//	printf("\nblocksize=%d\n",blocksize);
		if(0 == blocksize)
			break;
		DmaDesc.dma_ctrl = TLS_DMA_DESC_CTRL_SRC_ADD_INC | TLS_DMA_DESC_CTRL_DATA_SIZE_WORD | (blocksize << 5);
		DmaDesc.valid = TLS_DMA_DESC_VALID;
		DmaDesc.next = NULL;
		tlsDmaStart(dmaCh, &DmaDesc, 0);
		SPIM_MODECFG_REG = SPI_RX_TRIGGER_LEVEL(0) | SPI_TX_TRIGGER_LEVEL(0) | SPI_TX_DMA_ON;/* Enable SPI TX DMA */
		SPIM_SPITIMEOUT_REG = SPI_TIMER_EN |SPI_TIME_OUT((u32)0xffff);/* spi_timer_en | 0xffff */
		if(ifusecmd && 0 == i)
			SPIM_CHCFG_REG = SPI_FORCE_SPI_CS_OUT | SPI_TX_CHANNEL_ON | SPI_CONTINUE_MODE | SPI_START | SPI_VALID_CLKS_NUM(((blocksize + 4) * 8));
		else
			SPIM_CHCFG_REG = SPI_FORCE_SPI_CS_OUT | SPI_TX_CHANNEL_ON | SPI_CONTINUE_MODE | SPI_START | SPI_VALID_CLKS_NUM((blocksize * 8));

		if(spiWaitIdle())
		{
			ret = TLS_SPI_STATUS_EBUSY;
			break;
		}
		/* Wait Dma Channel Complete and Free Dma channel */
		if(tlsDmaWaitComplt(dmaCh))
		{
			ret = TLS_SPI_STATUS_EBUSY;
			break;
		}
		txlen -= blocksize;
	//	printf("\ntxlen=%d\n",txlen);
	}
	tlsDmaFree(dmaCh);

	SPIM_CHCFG_REG = 0x00000000;
	SPIM_MODECFG_REG = 0x00000000;
	SPIM_SPITIMEOUT_REG = 0x00000000;
	spi_port->spibusy = FALSE;
//	printf("\nwrite block over\n");
	return ret;
}

static int SpiDmaBlockRead(u8 *data,u32 len, u8 *txdata, u8 txlen)
{
	unsigned char dmaCh = 0;
	TLS_DMA_DESCRIPTOR DmaDesc;
	u32 word32 = 0;
	u32 i;
	u32 rxlen;
	u8 blocknum;
	u32 blocksize;
	int ret = TLS_SPI_STATUS_OK;
	
//	printf("\nentry SpiDmaBlockRead\n");
	if( NULL == data || NULL == txdata )
	{
		return TLS_SPI_STATUS_EINVAL;
	}
	if(spiWaitIdle())
	{
		return  TLS_SPI_STATUS_EBUSY;
	}
	spi_port->spibusy = TRUE;
 	SPIM_CHCFG_REG = SPI_CLEAR_FIFOS;
	while(SPIM_CHCFG_REG & SPI_CLEAR_FIFOS);
//	printf("\nblock read len = %d,txlen=%d,txdata=%x\n",len,txlen,*(u32 *)txdata);
	if(len%4)
	{
		rxlen = ((len+3)/4)<<2;
		
	}
	else
	{
		rxlen = len;
	}
	
	blocknum = rxlen/SPI_DMA_MAX_TRANS_SIZE;
//	printf("\nrxlen=%d,blocknum=%d\n",rxlen,blocknum);
	if(txlen > 0 && txlen <= 32)
	{
		for(i = 0;i < txlen;i ++)
		{
			if((i > 0) && (0 == i%4))
			{
				SPIM_TXDATA_REG = word32;
				word32 = 0;
			}
			word32 |= (txdata[i]<<((i%4)*8));
		}
		SPIM_TXDATA_REG = word32;
	}

	/* Request DMA Channel */
	dmaCh = tlsDmaRequest(1, TLS_DMA_FLAGS_CHANNEL_SEL(TLS_DMA_SEL_LSSPI_RX) | TLS_DMA_FLAGS_HARD_MODE);
	DmaDesc.src_addr = HR_SPI_RXDATA_REG;
	for(i = 0;i <= blocknum; i ++)
	{
	//	printf("\ni =%d\n",i);
		DmaDesc.dest_addr = (int)(data + i*SPI_DMA_MAX_TRANS_SIZE);
		blocksize = (rxlen > SPI_DMA_MAX_TRANS_SIZE) ? SPI_DMA_MAX_TRANS_SIZE:rxlen;
		if(0 == blocksize)
			break;
	//	printf("\nblocksize= %d\n",blocksize);
		DmaDesc.dma_ctrl = TLS_DMA_DESC_CTRL_DEST_ADD_INC | TLS_DMA_DESC_CTRL_BURST_SIZE1 | TLS_DMA_DESC_CTRL_DATA_SIZE_WORD | TLS_DMA_DESC_CTRL_TOTAL_BYTES(blocksize);
		//	word32 = DmaDesc.dma_ctrl;
		//	printf("\ndma ctrl = %x\n",DmaDesc.dma_ctrl);
		DmaDesc.valid = TLS_DMA_DESC_VALID;
		DmaDesc.next = NULL;
		tlsDmaStart(dmaCh, &DmaDesc, 0);
		
		SPIM_MODECFG_REG = SPI_RX_TRIGGER_LEVEL(0) | SPI_TX_TRIGGER_LEVEL(0) | SPI_RX_DMA_ON;/* Enable SPI RX DMA */
		SPIM_SPITIMEOUT_REG = SPI_TIMER_EN |SPI_TIME_OUT((u32)0xffff);/* spi_timer_en | 0xffff */
		if(0 == i)		//第一次需要打开TX
			SPIM_CHCFG_REG = SPI_FORCE_SPI_CS_OUT | SPI_RX_CHANNEL_ON | SPI_TX_CHANNEL_ON | SPI_CONTINUE_MODE | SPI_START | SPI_VALID_CLKS_NUM(((blocksize + txlen) * 8)) | SPI_RX_INVALID_BITS(txlen*8);
		else
			SPIM_CHCFG_REG = SPI_FORCE_SPI_CS_OUT | SPI_RX_CHANNEL_ON | SPI_CONTINUE_MODE | SPI_START | SPI_VALID_CLKS_NUM(blocksize * 8);			
		if(spiWaitIdle())
		{
			ret = TLS_SPI_STATUS_EBUSY;
			break;
		}
		/* Wait Dma Channel Complete and Free Dma channel */
		if(tlsDmaWaitComplt(dmaCh))
		{
			ret = TLS_SPI_STATUS_EBUSY;
			break;
		}
		rxlen -= blocksize;
	//	printf("\nrxlen = %d\n",rxlen);
	}
	tlsDmaFree(dmaCh);

	SPIM_CHCFG_REG = 0x00000000;
	SPIM_MODECFG_REG = 0x00000000;
	SPIM_SPITIMEOUT_REG = 0x00000000;
	spi_port->spibusy = FALSE;
	return ret;
}

/**********************************************************************************************************
* Description: 	This function is used to set SPI transfer mode.
*
* Arguments  : 	type					is the transfer type.
*				type == SPI_BYTE_TRANSFER	byte transfer
*				type == SPI_WORD_TRANSFER	word transfer
*				type == SPI_DMA_TRANSFER	DMA transfer
* Returns    :
* Notes	    :		This is an empty function.the system use DMA transfer default.
**********************************************************************************************************/
void tls_spi_trans_type(u8 type)
{
//系统现在只支持DMA方式，其他的方式已经去掉，
//留下该接口，是因为之前该接口已经开放给客户使用了
}


/**********************************************************************************************************
* Description: 	This function is used to setup the spi CPOL,CPHA,cs signal and clock.
*
* Arguments  : 	mode					is CPOL and CPHA type defined in TLS_SPI_MODE_0 to TLS_SPI_MODE_3
* 		cs_active				is cs mode, defined as follow:
* 							cs_active == TLS_SPI_CS_LOW 	:low is active
* 							cs_active == TLS_SPI_CS_HIGH 	:high is active
* 		fclk					is spi clock,the unit is HZ.
*
*
* Returns    : 	TLS_SPI_STATUS_OK			if initialize success
* 		TLS_SPI_STATUS_EBUSY			if SPI is already initialized
* 		TLS_SPI_STATUS_ENOMEM			if malloc SPI memory fail
**********************************************************************************************************/
int tls_spi_setup(u8 mode, u8 cs_active, u32 fclk)
{
	if ((spi_port->mode == mode) && (spi_port->cs_active == cs_active) && (spi_port->speed_hz == fclk)) {
		TLS_DBGPRT_WARNING("@mode, @cs_activer, @fclk is the same as settings of the current spi master driver!\n");
		return TLS_SPI_STATUS_OK;
	}

	switch (mode) {
		case TLS_SPI_MODE_0:		
		case TLS_SPI_MODE_1:
		case TLS_SPI_MODE_2:	
		case TLS_SPI_MODE_3:
			spi_port->mode = mode;
			break;

		default:
			TLS_DBGPRT_ERR("@mode is invalid!\n");
			return TLS_SPI_STATUS_EMODENOSUPPORT;
	}

	if((cs_active != TLS_SPI_CS_HIGH) && (cs_active != TLS_SPI_CS_LOW)) {
		TLS_DBGPRT_ERR("@cs_active  is invalid!\n");
		return TLS_SPI_STATUS_EINVAL;
	} else {spi_port->cs_active = cs_active;}

	if((fclk < TLS_SPI_FCLK_MIN) || (fclk > TLS_SPI_FCLK_MAX)) {
		TLS_DBGPRT_ERR("@fclk  is invalid!\n");
		return TLS_SPI_STATUS_ECLKNOSUPPORT;
	} else 
	{
		spi_port->speed_hz = fclk;
	}

	SpiMasterInit(mode, TLS_SPI_CS_LOW, fclk);
	
	return TLS_SPI_STATUS_OK;
}

/**********************************************************************************************************
* Description: 	This function is used to synchronous write data then read data by SPI.
*
* Arguments  : 	txbuf					is the write data buffer.
* 		n_tx					is the write data length.
*		rxbuf					is the read data buffer.
*		n_rx					is the read data length.
* Returns    : 	TLS_SPI_STATUS_OK			if write success.
* 		TLS_SPI_STATUS_EINVAL			if argument is invalid.
* 		TLS_SPI_STATUS_ENOMEM			if there is no enough memory.
* 		TLS_SPI_STATUS_ESHUTDOWN		if SPI driver does not installed.
**********************************************************************************************************/
int tls_spi_write_then_read(const u8 *txbuf, u32 n_tx, u8 *rxbuf, u32 n_rx)
{
	if ((txbuf == NULL) || (n_tx == 0) || (rxbuf == NULL) || (n_rx == 0)) 
	{
		return TLS_SPI_STATUS_EINVAL;
	}
	
	if(n_rx > SPI_DMA_BUF_MAX_SIZE || n_tx > SPI_DMA_CMD_MAX_SIZE)
	{
		TLS_DBGPRT_ERR("\nread length too long\n");
		return TLS_SPI_STATUS_EINVAL;
	}
	if(spi_port->spibusy)
		return TLS_SPI_STATUS_EBUSY;
	
	memcpy((u8 *)SPI_DMA_CMD_ADDR, txbuf, n_tx);	
	SpiDmaBlockRead((u8 *)SPI_DMA_BUF_ADDR, n_rx, (u8 *)SPI_DMA_CMD_ADDR, n_tx);
	memcpy(rxbuf, (u8 *)SPI_DMA_BUF_ADDR, n_rx);

	return TLS_SPI_STATUS_OK;
}

/**********************************************************************************************************
* Description: 	This function is used to synchronous read data by SPI. 
*
* Arguments  : 	buf					is the buffer for saving SPI data.
* 		len					is the data length.
*
* Returns    : 	TLS_SPI_STATUS_OK			if write success.
* 		TLS_SPI_STATUS_EINVAL			if argument is invalid.
* 		TLS_SPI_STATUS_ENOMEM			if there is no enough memory.
* 		TLS_SPI_STATUS_ESHUTDOWN		if SPI driver does not installed.
**********************************************************************************************************/
int tls_spi_read(u8 *buf, u32 len)
{
	if ((buf == NULL) || (len == 0)) 
	{
		return TLS_SPI_STATUS_EINVAL;
	}

	if(len > SPI_DMA_BUF_MAX_SIZE)
	{
		TLS_DBGPRT_ERR("\nread len too long\n");
		return TLS_SPI_STATUS_EINVAL;
	}
	if(spi_port->spibusy)
		return TLS_SPI_STATUS_EBUSY;
	
	SpiDmaBlockRead((u8 *)SPI_DMA_BUF_ADDR, len, NULL, 0);
	memcpy(buf, (u8 *)SPI_DMA_BUF_ADDR, len);
	
	return TLS_SPI_STATUS_OK;
}

/**********************************************************************************************************
* Description: 	This function is used to synchronous write data by SPI. 
*
* Arguments  : 	buf					is the user data.
* 		len					is the data length.
*
* Returns    : 	TLS_SPI_STATUS_OK			if write success.
* 		TLS_SPI_STATUS_EINVAL			if argument is invalid.
* 		TLS_SPI_STATUS_ENOMEM			if there is no enough memory.
* 		TLS_SPI_STATUS_ESHUTDOWN		if SPI driver does not installed.
**********************************************************************************************************/
int tls_spi_write(const u8 *buf, u32 len)
{
	u32 data32 = 0;
	u16 txBitLen;
	u32 rdval1;
	u32 i;

	if ((buf == NULL) || (len == 0)) 
	{
		return TLS_SPI_STATUS_EINVAL;
	}
	if(spi_port->spibusy)
		return TLS_SPI_STATUS_EBUSY;
	if(len <= 4)		//直接传输
	{
		spi_port->spibusy = TRUE;
		SPIM_CHCFG_REG    = SPI_CLEAR_FIFOS;
		while(SPIM_CHCFG_REG & SPI_CLEAR_FIFOS);
		for(i = 0;i < len;i ++)
		{
			data32 |= (((u8)(buf[i]))<<(i*8));
		}
		SPIM_TXDATA_REG  =  data32;
	
		txBitLen = 8*len;
		rdval1 = 0;
		rdval1 |= SPI_TX_CHANNEL_ON | SPI_CONTINUE_MODE | SPI_START | SPI_VALID_CLKS_NUM(txBitLen);
		SPIM_CHCFG_REG = rdval1;
	
		spiWaitIdle();
		spi_port->spibusy = FALSE;
	}
	else				//DMA传输
	{
		if(len > SPI_DMA_BUF_MAX_SIZE)
		{
			TLS_DBGPRT_ERR("\nwrite len too long\n");
			return TLS_SPI_STATUS_EINVAL;
		}
		memcpy((u8 *)SPI_DMA_BUF_ADDR, buf, len);
		SpiDmaBlockWrite((u8 *)SPI_DMA_BUF_ADDR, len,0,0);
	}
	return 	TLS_SPI_STATUS_OK;
}

/**********************************************************************************************************
* Description: 	This function is used to synchronous write 32bit command then write data by SPI.
*
* Arguments  : 	buf32					is the command data.
* 		txbuf					is the write data buffer.
*		n_tx					is the write data length.
* Returns    : 	TLS_SPI_STATUS_OK			if write success.
* 		TLS_SPI_STATUS_EINVAL			if argument is invalid.
* 		TLS_SPI_STATUS_ENOMEM			if there is no enough memory.
* 		TLS_SPI_STATUS_ESHUTDOWN		if SPI driver does not installed.
**********************************************************************************************************/
int tls_spi_write32_then_writen(const u32 *buf32, const u8 *txbuf, u32 n_tx)
{
	if ((buf32 == NULL) || (txbuf == NULL) || (n_tx == 0)) 
	{
		return TLS_SPI_STATUS_EINVAL;
	}
	if(spi_port->spibusy)
		return TLS_SPI_STATUS_EBUSY;
	if(n_tx > SPI_DMA_BUF_MAX_SIZE)
	{
		TLS_DBGPRT_ERR("\nwriten len too long\n");
		return TLS_SPI_STATUS_EINVAL;
	}
	memcpy((u8 *)SPI_DMA_CMD_ADDR, (u8 *)buf32, 4);
	memcpy((u8 *)SPI_DMA_BUF_ADDR, txbuf, n_tx);
	SpiDmaBlockWrite((u8 *)SPI_DMA_BUF_ADDR, n_tx, 1, *(u32 *)SPI_DMA_CMD_ADDR);
	
	return TLS_SPI_STATUS_OK;
}

/**********************************************************************************************************
* Description: 	This function is used to initialize the SPI master driver.
*
* Arguments  : 	
*
* Returns    : 	TLS_SPI_STATUS_OK			if initialize success
* 		TLS_SPI_STATUS_EBUSY			if SPI is already initialized
* 		TLS_SPI_STATUS_ENOMEM			if malloc SPI memory fail*		
**********************************************************************************************************/
 int tls_spi_init(void)
{
	struct tls_spi_port *port;
	
	if(spi_port != NULL) {
		TLS_DBGPRT_ERR("spi driver module has been installed!\n");
		return TLS_SPI_STATUS_EBUSY;
	}

	TLS_DBGPRT_SPI("initialize spi master driver module.\n");

	port = (struct tls_spi_port *)malloc(sizeof(struct tls_spi_port));
	if (port == NULL) {
		TLS_DBGPRT_ERR("allocate \"struct tls_spi_port\" fail!\n");
		return TLS_SPI_STATUS_ENOMEM;
	}
	memset(port,0,sizeof(struct tls_spi_port));

	port->speed_hz = SPI_DEFAULT_SPEED;	/*默认2M*/
	port->cs_active = SPI_CS_ACTIVE_MODE;
	port->mode = SPI_DEFAULT_MODE;	/*CPHA = 0,CPOL = 0*/
	port->spibusy = false;
	spi_port = port;

	tls_reg_write32(HR_IOCTL_GP_SPI, 0);		/*设置复用IO为SPI模式*/
	SpiMasterInit(spi_port->mode, TLS_SPI_CS_LOW, spi_port->speed_hz);

	TLS_DBGPRT_SPI("spi master driver module initialization finish.\n");

	return TLS_SPI_STATUS_OK;
}

/**
 * @brief 
 *
 * @return 
 */
int tls_spi_exit(void)
{
	TLS_DBGPRT_SPI("Not support spi master driver module uninstalled!\n");
	return TLS_SPI_STATUS_EPERM;
}

/**********************************************************************************************************
* Description: 	This function is used to select SPI slave type.
*
* Arguments  : 	slave					is the slave type,defined as follow:
* 							slave == SPI_SLAVE_FLASH	:flash
* 							slave == SPI_SLAVE_CARD		: sd card
*
* Returns    : 	Before communicate with different SPI device, must call the function.
**********************************************************************************************************/
void tls_spi_slave_sel(u16 slave)
{
	//u16 ret;
/*gpio0控制cs信号*/
	tls_gpio_cfg(SPI_SLAVE_CONTROL_PIN, TLS_GPIO_DIR_OUTPUT, TLS_GPIO_ATTR_FLOATING);
	if(SPI_SLAVE_FLASH == slave)
	{
		tls_gpio_write(SPI_SLAVE_CONTROL_PIN, 0);
	//	ret = tls_gpio_read(SPI_SLAVE_CONTROL_PIN);
	//	printf("\nflash gpio 0 ===%d\n",ret);
	}
	else if(SPI_SLAVE_CARD == slave)
	{
		tls_gpio_write(SPI_SLAVE_CONTROL_PIN, 1);
	//	ret = tls_gpio_read(SPI_SLAVE_CONTROL_PIN);
	//	printf("\ncard gpio 0 ===%d\n",ret);
	}
	
}

