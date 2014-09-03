/**************************************************************************
 * File Name                    : wm_wifi_oneshot.c
 * Author                       : WinnerMicro
 * Version                      :
 * Date                         : 05/30/2014
 * Description                  : Wifi one shot sample(UDP, PROBEREUEST)
 *
 * Copyright (C) 2014 Beijing Winner Micro Electronics Co.,Ltd.
 * All rights reserved.
 *
 ***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wm_type_def.h"
#include "wm_os.h"
#if (GCC_COMPILE == 1)
#include "wm_ieee80211_gcc.h"
#else
#include "wm_ieee80211.h"
#endif
#include "wm_wifi.h"
#include "wm_wifi_oneshot.h"
#include "utils.h"
#include "wm_params.h"


#define CONFIG_ONE_SHOT_UDP 1
#define CONFIG_ONE_SHOT_PROBE_REQ 2

#ifdef CONFIG_UDP_ONE_SHOT
static OS_TMR *gWifiSwitchChanTim = NULL;
#endif

static u8 guconeshotflag = 0;
#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif
static u8 gSrcMac[ETH_ALEN] = {0,0,0,0,0,0};//{0x68,0xdf,0xdd,0xff,0xec,0x67};

static u8 gucbssidData[ETH_ALEN] = {0};
static u8 gucbssidokflag = 0;

#ifndef le_to_host16
#define le_to_host16(n) (n)
#define host_to_le16(n) (n)
#endif


int tls_wifi_bssid_resolve(u8 *data, u8 len, u8 seqnum, u8 seqtotal, u8 configtype)
{
	static u8 bssid_seq = 0;
	static u8 seq_total_tmp = 0;
	static u8 bssidData[7] = {0};/*保存完整的BSSID信息*/
	static u8 *DatabSsid[2]; /*保存每包的BSSID数据*/

	u8 i = 0, j = 0;;	

	if (seq_total_tmp == 0){
		seq_total_tmp = seqtotal;
	}

	/*clear save ssid's buf because total packet number changed*/
	if (seq_total_tmp != seqtotal){
		for (i= 0; i < seq_total_tmp; i++){
			if (DatabSsid[i] != NULL){
				free(DatabSsid[i]);
				DatabSsid[i] = NULL;
			}
		}
		bssid_seq = 0;
		seq_total_tmp = seqtotal;		
	}

	if (DatabSsid[seqnum] == NULL){
		DatabSsid[seqnum] = malloc(len+1);
		if (DatabSsid[seqnum] == NULL){
			return 1;
		}
		if (configtype == CONFIG_ONE_SHOT_PROBE_REQ){
			for (i = 0,j = 0; i< len; i+=2, j++){
				DatabSsid[seqnum][j] = (((data[i]-0x40)&0xF) <<4)|((data[i+1]-0x40)&0xF);
			}
			DatabSsid[seqnum][j] = '\0';
		}else if (configtype == CONFIG_ONE_SHOT_UDP){
			memcpy(DatabSsid[seqnum], data, len);
			DatabSsid[seqnum][len] = '\0';
		}

		bssid_seq++;
		if (bssid_seq == seqtotal){
			gucbssidokflag = 1;
			for (i = 0; i< seqtotal; i++){
				strcat((char *)bssidData, (char *)DatabSsid[i]);
				free(DatabSsid[i]);
				DatabSsid[i] = NULL;
			}
			memcpy(gucbssidData,bssidData, ETH_ALEN);
			memset(bssidData, 0 ,7);
			bssid_seq = 0;
			seq_total_tmp = 0;
		}
		return 0;
	}
	return 1;
}

static u8 gucpwdokflag = 0;
static u8 gucpwdData[65] ={0};
int tls_wifi_pwd_resolve(u8 *data, u8 len, u8 seqnum, u8 seqtotal, u8 configtype)
{
	static u8 pwd_seq = 0;
	static u8 seq_total_tmp = 0;
	static u8 pwdData[65] ={0};
	static u8 *DataPwd[32];	

	u8 i = 0, j = 0;	

	if (seq_total_tmp == 0){
		seq_total_tmp = seqtotal;
	}
	/*clear save pwd's buf because total packet number changed*/
	if (seq_total_tmp != seqtotal){
		for (i= 0; i < seq_total_tmp; i++){
			if (DataPwd[i] != NULL){
				free(DataPwd[i]);
				DataPwd[i] = NULL;
			}
		}
		seq_total_tmp = seqtotal;
		pwd_seq = 0;
	}

	if (DataPwd[seqnum] == NULL){
		DataPwd[seqnum] = malloc(len+1);
		if (DataPwd[seqnum] == NULL){
			return 1;
		}
		if (configtype == CONFIG_ONE_SHOT_PROBE_REQ){
			for (i = 0,j = 0; i< len; i+=2, j++){
				DataPwd[seqnum][j] = (((data[i]-0x40)&0xF) <<4)|((data[i+1]-0x40)&0xF);
			}
			DataPwd[seqnum][j] = '\0';
		}else if (configtype == CONFIG_ONE_SHOT_UDP){
			memcpy(DataPwd[seqnum], data, len);
			DataPwd[seqnum][len] = '\0';			
		}

		pwd_seq++;
		if (pwd_seq == seqtotal){
			gucpwdokflag = 1;			
			for (i = 0; i< seqtotal; i++){
				if (DataPwd[i][0] != '\0'){
					strcat((char *)pwdData, (char *)DataPwd[i]);
				}
				free(DataPwd[i]);
				DataPwd[i] = NULL;
			}
			
			strcpy((char *)gucpwdData,(char *)pwdData);
			memset(pwdData, 0 ,65);
			pwd_seq = 0;
			seq_total_tmp = 0;
		}
		return 0;		
	}
	return 1;
}

u8 gucssidokflag = 0;
static u8 gucssidData[32] = {0};
int tls_wifi_ssid_resolve(u8 *data, u8 len, u8 seqnum, u8 seqtotal, u8 configtype)
{
	static u8 ssid_seq = 0;
	static u8 seq_total_tmp = 0;
	static u8 ssidData[33] = {0};/*保存完整的SSID信息*/
	static u8 *DataSsid[8]; /*保存每包的SSID数据*/
	u8 i = 0, j = 0;	

	if (seq_total_tmp == 0){
		seq_total_tmp = seqtotal;
	}

	/*clear save ssid's buf because total packet number changed*/
	if (seq_total_tmp != seqtotal){
		for (i= 0; i < seq_total_tmp; i++){
			if (DataSsid[i] != NULL){
				free(DataSsid[i]);
				DataSsid[i] = NULL;
			}
		}
		ssid_seq = 0;
		seq_total_tmp = seqtotal;		
	}

	if (DataSsid[seqnum] == NULL){
		DataSsid[seqnum] = malloc(len/2+1);
		if (DataSsid[seqnum] == NULL){
			return 1;
		}
		if (configtype == CONFIG_ONE_SHOT_PROBE_REQ){
			for (i = 0,j = 0; i< len; i+=2, j++){
				DataSsid[seqnum][j] = (((data[i]-0x40)&0xF) <<4)|((data[i+1]-0x40)&0xF);
			}
			DataSsid[seqnum][j] = '\0';
		}else if (configtype == CONFIG_ONE_SHOT_UDP){
			memcpy(DataSsid[seqnum], data, len);
			DataSsid[seqnum][len] = '\0';
		}

		ssid_seq++;
		if (ssid_seq == seqtotal){
			gucssidokflag = 1;			
			for (i = 0; i< seqtotal; i++){
				strcat((char *)ssidData, (char *)DataSsid[i]);
				free(DataSsid[i]);
				DataSsid[i] = NULL;
			}

			strcpy((char *)gucssidData,(char *)ssidData);
			memset(ssidData, 0 ,33);
			ssid_seq = 0;
			seq_total_tmp = 0;
		}
		return 0;
	}
	return 1;
}

/**
 * compare_ether_addr - Compare two Ethernet addresses
 * @addr1: Pointer to a six-byte array containing the Ethernet address
 * @addr2: Pointer other six-byte array containing the Ethernet address
 *
 * Compare two ethernet addresses, returns 0 if equal
 */
static u8 tls_compare_ether_addr(const u8 *addr1, const u8 *addr2)
{
	return !((addr1[0] == addr2[0]) && (addr1[1] == addr2[1]) && (addr1[2] == addr2[2]) &&   \
		(addr1[3] == addr2[3]) && (addr1[4] == addr2[4]) && (addr1[5] == addr2[5]));
}

static u8 tls_wifi_compare_mac_addr(u8 *macaddr){
	u8 tmpmacaddr[ETH_ALEN] = {0, 0,0,0,0,0};	

	if (macaddr == NULL){
		return 0;
	}

	if (tls_compare_ether_addr(gSrcMac, tmpmacaddr) == 0){
		memcpy(gSrcMac, macaddr, ETH_ALEN);
		return 0;
	}

	if (tls_compare_ether_addr(gSrcMac, macaddr) == 0){
		return 1;
	}
	return 0;
}

u8 tls_wifi_oneshot_connect_by_bssid(u8 *bssid, u8 *pwd)
{
	struct tls_param_ip ip_param;

	tls_wifi_set_listen_mode(0);	
	tls_param_get(TLS_PARAM_ID_IP, &ip_param, FALSE);
	ip_param.dhcp_enable = true;
	tls_param_set(TLS_PARAM_ID_IP, &ip_param, FALSE);
	
	return tls_wifi_connect_by_bssid(bssid, pwd, (pwd == NULL) ? 0 : strlen((char *)pwd));
	
}

u8 tls_wifi_oneshot_connect(u8 *ssid, u8 *pwd)
{
	struct tls_param_ip ip_param;

	tls_wifi_set_listen_mode(0);	

	tls_param_get(TLS_PARAM_ID_IP, &ip_param, FALSE);
	ip_param.dhcp_enable = true;
	tls_param_set(TLS_PARAM_ID_IP, &ip_param, FALSE);

	return tls_wifi_connect(ssid, strlen((char *)ssid), pwd, (pwd==NULL) ? 0 : strlen((char *)pwd));
}

#ifdef CONFIG_UDP_ONE_SHOT
u8 smtcfg1array[4] = {61/*open*/,69/*wep*/,77/*aes*/,81/*tkip*/};
u8 smtcfgArray[16] = {0};

#define HANDSHAKE_CNT 20
u8 guchandshakeflag = 0;
static u8 handshakecnt[4];

static u8 chanswitchflag = 0;
static u8 istimerstart = 0;

static u8* tls_wifi_get_tods_addr(struct ieee80211_hdr *hdr){
	if (ieee80211_has_tods(hdr->frame_control)){
		return hdr->addr2;
	}
	return NULL;
}

static u8* tls_wifi_get_fromds_addr(struct ieee80211_hdr *hdr){
	if (ieee80211_has_fromds(hdr->frame_control)){
		return hdr->addr2;
	}
	return NULL;
}

int tls_wifi_oneshot_packet_head_resolve(u8 *datasave)
{
	u8 seqNum = 0;
	u8 seqTotal  = 0;
	u8 len = 0;
	u8 tagId = 0xFF;

	tagId = (datasave[0]>>4)&0xF;
	if (tagId > 1){
		return 1;
	}

	seqTotal = (datasave[0]&0xF)+1;/*4bit 0-1,1-2*/
	seqNum = (datasave[1]>>4)&0xF;
	if (seqNum >= seqTotal){
		return 1;
	}

	len = datasave[1]&0xF;
	if (len > 4){
		return 1;
	}

	return 0;
}

int tls_wifi_oneshotinfo_resolve(u8 *datasave)
{
	u8 crc, calcrc;
	int ret = 1;
	u8 seqNum = 0;
	u8 seqTotal  = 0;
	u8 len = 0;
	u8 tagId = 0xFF;

	tagId = (datasave[0]>>4)&0xF;
	if (tagId > 1){
		return 1;
	}
	seqTotal = (datasave[0]&0xF)+1;/*4bit 0-1,1-2*/
	seqNum = (datasave[1]>>4)&0xF;
	if (seqNum > seqTotal){
		return 1;
	}

	len = datasave[1]&0xF;
	if (len > 4){
		return 1;
	}

	crc = *(datasave + 2 + len);
	calcrc = get_crc8(datasave,(2+len));
	if (crc == calcrc){
		switch (tagId){
			case 0:
			    if (gucbssidokflag == 0){
					ret = tls_wifi_bssid_resolve((datasave+2), len, seqNum, seqTotal, CONFIG_ONE_SHOT_UDP);
				}
				break;
			case 1:
				if (gucpwdokflag == 0){
					ret = tls_wifi_pwd_resolve((datasave+2), len, seqNum, seqTotal, CONFIG_ONE_SHOT_UDP);
				}
				break;
			default:
				break;
		}
	}

	if ((1 == gucbssidokflag) && (1 == gucpwdokflag)){
		guconeshotflag = 1;
		istimerstart = 0;
		printf("BSSID:%x:%x:%x:%x:%x:%x\n",  gucbssidData[0],  gucbssidData[1],  gucbssidData[2],  gucbssidData[3],  gucbssidData[4],  gucbssidData[5]);		
		printf("PASSWORD:%s\n", gucpwdData);
		tls_wifi_oneshot_connect_by_bssid(gucbssidData, gucpwdData);
		memset(gucbssidData, 0, ETH_ALEN);	
		memset(gucpwdData, 0, 65);

		gucpwdokflag = gucbssidokflag = 2;
		memset(gSrcMac, 0, ETH_ALEN);
	}

	return ret;
}

void tls_wifi_switch_channel_tim(void *ptmr, void *parg){
	static u8 chanid = 0;
	static u8 chanarray[13] = {0,2,4,6,8,10,12,1,3,5,7,9,11};
	if (tls_wifi_get_oneshot_flag()== 1){
		if (0 == chanswitchflag){
			memset(smtcfgArray, 0, sizeof(smtcfgArray));
			memset(handshakecnt, 0, 4);
			memset(gSrcMac, 0, ETH_ALEN);
			tls_wifi_change_chanel(chanarray[chanid]);
			chanid = (++chanid)%13;
		}
	}
}

static __inline int is_broadcast_ether_addr(const u8 *a)
{
	return (a[0] & a[1] & a[2] & a[3] & a[4] & a[5]) == 0xff;
}

u8 tls_wifi_dataframe_recv(struct ieee80211_hdr *hdr, u32 data_len)
{
	int multicast3 = is_broadcast_ether_addr(hdr->addr3);
	u32 frm_len = 0;
	u8 i = 0;
	u8 j = 0;

	static s8 databitcnt = 4;
	static u8 datasave[7];
	static u8 datacnt = 0;
	static u8 syncnum = 0;
	static u8 isave = 0;

	u8 *SrcMacAddr = NULL;

	int ret = 0;

	u8 err;

	if (0 == ieee80211_is_data(hdr->frame_control)){
		return 1;
	}

	if ((tls_wifi_get_oneshot_flag()== 0)||(guconeshotflag)){
		return 0;
	}

	if (ieee80211_is_data_qos(hdr->frame_control)){
		frm_len = data_len - 2;
	}else{
		frm_len = data_len;	
	}

	SrcMacAddr = tls_wifi_get_tods_addr(hdr);
	if (0 == guchandshakeflag){	/*hand shake*/
		if (multicast3){
			for (i = isave; i < 4; i++){
				if (frm_len == smtcfg1array[i]){
					if (tls_wifi_compare_mac_addr(SrcMacAddr)){
						++handshakecnt[i];
						if (handshakecnt[i] > HANDSHAKE_CNT){
							for (j = 1; j < 17; j++){
								smtcfgArray[j-1] = smtcfg1array[i]+j;
							}
							syncnum = 0;
							datacnt = 0;
							isave = i;
							guchandshakeflag = 1;
							if (chanswitchflag == 0){
								memcpy(gucbssidData, hdr->addr1, ETH_ALEN);
								gucbssidokflag = 1;
								OSTmrDel(gWifiSwitchChanTim, &err);
								gWifiSwitchChanTim = NULL;
								chanswitchflag = 1;
							}
							break;
						}
					}
				}else{
					if (tls_wifi_compare_mac_addr(SrcMacAddr)){
						handshakecnt[i] = 0;
						memset(smtcfgArray, 0, sizeof(smtcfgArray));
						isave = 0;
					}
				}
			}
		}
	}else{	/*recv data*/
		if (multicast3){
			if (tls_wifi_compare_mac_addr(SrcMacAddr)){
				if (syncnum == 0){
					if(smtcfgArray[1] == frm_len){
						datasave[datacnt] = 1<<4;
						databitcnt = 0;
						datacnt = 0;
						syncnum = 1;
					}
					return 1;
				}

				for (j = 0; j < 16; j++){
					if (smtcfgArray[j] == frm_len){
						datasave[datacnt] |=  j<<(databitcnt);
						databitcnt -= 4;
						break;
					}
				}

				if (databitcnt < 0){
					databitcnt = 4;
					datacnt++;
					if (datacnt > 1){
						ret = tls_wifi_oneshot_packet_head_resolve(datasave);
						if (ret){
							datasave[0] = datasave[1] = 0;
							datacnt = 0;
							syncnum = 0;
							return 1;
						}
					}
					if (datacnt > 6){
						syncnum = 0;
						/*resolve data*/
						tls_wifi_oneshotinfo_resolve(datasave);
						memset(datasave, 0, 7);
						datacnt = 0;
						isave = 0;
						guchandshakeflag = 0;
						handshakecnt[0] = handshakecnt[1] =handshakecnt[2] =handshakecnt[3] =0;
					}
				}
			}
		}
	}
	return 1;
}
/*END CONFIG_UDP_ONE_SHOT*/
#elif defined(CONFIG_PROBE_REQUEST_ONE_SHOT)
u16 tls_wifi_checksum_cal(s8 *data, u8 len){
	s8 i = 0;
	char checksum = 0;
	if (len == 0){
		return 0xABCD;
	}

	for ( i = 0; i < len; i++){
		checksum += *(data+i);
	}

	return ((checksum&0x1F)+0x40);
}

/*确认SSID和Password信息是否完全解析出来的标志,0:未解析，1:已解析，2:信息解析已完成，并触发加网动作*/
int tls_wifi_oneshotinfo_resolve(u8 *ssid, u8 len, u8 *srcMac)
{
	struct stOneKey stInfo;
	u16 checksum;
	int ret = 1;
	u8 oneshotflag = 0;

	oneshotflag = tls_wifi_get_oneshot_flag();
	if (oneshotflag == 0){
		return ret;
	}

	memcpy((char*)(&stInfo), ssid,7);
	stInfo.data = (ssid+7);	
	stInfo.checksum = *(ssid + (len - 1));
	if (len != 0){
		checksum = tls_wifi_checksum_cal((s8 *)ssid, (len-1));
	}else{
		checksum = 0xABCD;
	}

	stInfo.len = stInfo.len-0x40;
	stInfo.seq_total = stInfo.seq_total - 0x40;
	stInfo.seq_num = stInfo.seq_num - 0x40;

	if ((checksum == 0xABCD)||(stInfo.len != len)){
		goto EXIT;
	}

	if ((stInfo.header[0] == 0x5D)&&(stInfo.header[1] == 0x5E)&&(stInfo.header[2] == 0x5F)){
		if ((u8)checksum == stInfo.checksum){
			if (0 == tls_wifi_compare_mac_addr(srcMac)){
				goto EXIT;				
			}

			if (stInfo.tag_Id == 0x41){ /*SSID*/
				if (gucssidokflag == 0){
					ret = tls_wifi_ssid_resolve(stInfo.data, (len - 8), stInfo.seq_num, stInfo.seq_total, CONFIG_ONE_SHOT_PROBE_REQ);
				}
			}else if (stInfo.tag_Id == 0x42){/*PWD*/
				if (gucpwdokflag == 0){
					ret = tls_wifi_pwd_resolve(stInfo.data, (len - 8), stInfo.seq_num, stInfo.seq_total, CONFIG_ONE_SHOT_PROBE_REQ);
				}
			}else if (stInfo.tag_Id == 0x40){
			    if (gucbssidokflag == 0){
					ret = tls_wifi_bssid_resolve(stInfo.data, (len - 8), stInfo.seq_num, stInfo.seq_total, CONFIG_ONE_SHOT_PROBE_REQ);
				}				
			}
		}		
	}

	if (((1 == gucssidokflag) ||(1 == gucbssidokflag)) && (1 == gucpwdokflag)){
		guconeshotflag = 1; 
		if (gucssidokflag == 1){
			tls_wifi_oneshot_connect(gucssidData, gucpwdData);
			memset(gucssidData, 0, ETH_ALEN);				
			memset(gucpwdData, 0, 65);	
		}else if (1 == gucbssidokflag){
			printf("BSSID:%x:%x:%x:%x:%x:%x\n",  gucbssidData[0],  gucbssidData[1],  gucbssidData[2],  gucbssidData[3],  gucbssidData[4],  gucbssidData[5]);		
			printf("PASSWORD:%s\n", gucpwdData);
			tls_wifi_oneshot_connect_by_bssid(gucbssidData, gucpwdData);
			memset(gucbssidData, 0, ETH_ALEN);				
			memset(gucpwdData, 0, 65);			
		}
		gucssidokflag = gucbssidokflag = gucpwdokflag = 2;
		memset(gSrcMac, 0, ETH_ALEN);
	}

EXIT:
	return ret; 
}

void tls_wifi_probereq_recv(struct ieee80211_hdr *hdr, u32 data_len)
{
	u8 *pos = NULL;
	int ret = 0;
	struct ieee80211_mgmt *mgmt =(struct ieee80211_mgmt *)hdr;
	u32 left;
	u16 fc = 0;

	if ((tls_wifi_get_oneshot_flag()== 0)||(guconeshotflag)){
		return;
	}

	fc = le_to_host16(mgmt->frame_control);
	if ((fc&IEEE80211_FCTL_STYPE) != IEEE80211_STYPE_PROBE_REQ){
		return;
	}

	left = data_len - ((u8 *)mgmt->u.probe_req.variable - (u8 *)mgmt);
	pos = mgmt->u.probe_req.variable;
	while (left >= 2){
		u8 id, elen;

		id = *pos++;
		elen = *pos++;
		left -= 2;

		if (elen > left) {
			return;
		}

		switch (id) {
		case WLAN_EID_SSID:
			if (elen){
				tls_wifi_oneshotinfo_resolve(pos, elen, mgmt->sa);
			}
			return;
		default:
			break;
		}
	}
}
#endif /*CONFIG_PROBE_REQUEST_ONE_SHOT*/

void tls_wifi_one_shot_init(void){
	u8 err;
/*Register wifi data recv callback function*/
	tls_wifi_set_oneshot_flag(0);
#ifdef CONFIG_UDP_ONE_SHOT
	tls_wifi_data_recv_cb_register(tls_wifi_dataframe_recv);
	gWifiSwitchChanTim = OSTmrCreate(0, 15, 2,tls_wifi_switch_channel_tim,NULL,NULL,&err);
	if (gWifiSwitchChanTim){
		OSTmrStart(gWifiSwitchChanTim,&err);
	}
#elif defined(CONFIG_PROBE_REQUEST_ONE_SHOT)
	tls_wifi_data_recv_cb_register(tls_wifi_probereq_recv);
#endif
}

