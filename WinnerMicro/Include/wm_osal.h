/***************************************************************************** 
* 
* File Name : wm_osal.h
* 
* Description: os  Module 
* 
* Copyright (c) 2014 Winner Microelectronics Co., Ltd. 
* All rights reserved. 
* 
* Author : dave 
* 
* Date : 2014-6-13
*****************************************************************************/ 

#ifndef WM_OSAL_H
#define WM_OSAL_H

#include "wm_config.h"

#if (TLS_OS_UCOS)
#include "wm_osal_ucos.h"
#elif (OS_RT_THREAD)
#include "wm_osal_rtthread.h"
#endif

#endif /* end of WM_OSAL_H */


