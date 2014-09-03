/***************************************************************************** 
* 
* File Name : wm_timer.c 
* 
* Description: Timer Driver Module 
* 
* Copyright (c) 2014 Winner Microelectronics Co., Ltd. 
* All rights reserved. 
* 
* Author : dave 
* 
* Date : 2014-6-2 
*****************************************************************************/ 
#include "wm_timer.h"
#include "wm_regs.h"
#include "wm_irq.h"

/**********************************************************************************************************
* Description: 	This function is used to stop the timer.
*
* Arguments  : 	
*
* Returns    : 	
**********************************************************************************************************/
void tls_timer_stop(void)
{
	*(unsigned int *)HR_TIMER1_CSR = TLS_TIMER_INT_CLR;
}

/**********************************************************************************************************
* Description: 	This function is used to set the timer and start the timer, the unit is us.
*
* Arguments  : 	timeout 		Timer number
*
* Returns    : 	
**********************************************************************************************************/
void tls_timer_start(int timeout)
{
	int timer_csr;	
	
	*(unsigned int *)HR_TIMER1_CSR = TLS_TIMER_INT_CLR;
	if (timeout)
	{
    		timer_csr = (timeout << TLS_TIMER_VALUE_S) | TLS_TIMER_ONE_TIME ;
		*(unsigned int *)HR_TIMER1_CSR = timer_csr;
	}
	*(unsigned int *)HR_TIMER1_CSR = *(unsigned int *)HR_TIMER1_CSR |TLS_TIMER_INT_EN| TLS_TIMER_EN;
}


/**********************************************************************************************************
* Description: 	This function is used to register timer1 interrupt.
*
* Arguments  : 	
*
* Returns    : 	
**********************************************************************************************************/
void tls_timer_irq_register(void (*callback)(void))
{
    tls_irq_register_handler(TIMER1_INT, (intr_handler_func)callback, 0);
    tls_irq_enable(TIMER1_INT);
}

