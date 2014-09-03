#ifndef __LITEPOINT_H
#define __LITEPOINT_H

typedef struct _ltpt_rx_info
{
	int bprocess;
	int cnt_total;
	int cnt_good;
	int cnt_bad;
	unsigned char valid;
	unsigned char snr;
	unsigned char rcpi;
	unsigned char rate;
}ltpt_rx_info;

typedef struct _ltpt_tx_info
{
	int bprocess;
	int cnt_total;
	/* input parameters */
	int channel;
	int packetcount;	
	int psdulen;    
	int txgain;      
	int datarate;   
}ltpt_tx_info;


enum Rate 
{
	S2M = 0x0000,
	S5M5 = 0x0001,
	S11M = 0x0002,
	L1M = 0x0003,
	L2M = 0x0004,
	L5M5 = 0x0005,
	L11M = 0x0006,
	R06M = 0x0100,
	R09M = 0x0101,
	R12M = 0x0102,
	R18M = 0x0103,
 	R24M = 0x0104,
	R36M = 0x0105,
	R48M = 0x0106,
	R54M = 0x0107,
};

extern int g_ltpt_testmode;
extern ltpt_rx_info g_ltpt_rxinfo;
extern ltpt_tx_info g_ltpt_txinfo;
extern unsigned char hed_rf65_txgainmap[32];
extern const unsigned int hed_bbp_value[];


void tls_tx_send_litepoint(void);

#endif
