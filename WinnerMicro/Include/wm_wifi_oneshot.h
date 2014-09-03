/**************************************************************************
 * File Name                    : wm_wifi_oneshot.h
 * Author                       : WinnerMicro
 * Version                      :
 * Date                         : 05/30/2014
 * Description                  :
 *
 * Copyright (C) 2014 Beijing Winner Micro Electronics Co., Ltd.
 * All rights reserved.
 *
 ***************************************************************************/
#ifndef WM_WIFI_ONESHOT_H
#define WM_WIFI_ONESHOT_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wm_type_def.h>
#if (GCC_COMPILE==1)
#include "wm_ieee80211_gcc.h"
#else
#include <wm_ieee80211.h>
#endif
/*UDP ONE SHOT*/
#define CONFIG_UDP_ONE_SHOT
/*PROBE REQUEST ONE SHOT*/
//#define CONFIG_PROBE_REQUEST_ONE_SHOT
#ifdef CONFIG_PROBE_REQUEST_ONE_SHOT
typedef struct stOneKey{
	s8 header[3];
	s8 len;
	s8 tag_Id;
	u8 seq_total;
	u8 seq_num;
	u8 *data;
	u8 checksum;
}OneKey;
#endif
u8 tls_wifi_dataframe_recv(struct ieee80211_hdr *hdr, u32 data_len);

void tls_wifi_probereq_recv(struct ieee80211_hdr *hdr, u32 data_len);
/**********************************************************************************************************
* Description: 	This function is used to init wm oneshot config function
*
* Arguments  : 	
* 				
* Returns    : 	
*              
**********************************************************************************************************/
void tls_wifi_one_shot_init(void);
#endif /*WM_WIFI_ONESHOT_H*/
