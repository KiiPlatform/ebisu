#ifndef __WM_CONFIG_H__
#define __WM_CONFIG_H__

#define	CFG_ON								1
#define CFG_OFF								0


#define TLS_OS_UCOS                     	CFG_ON  /*OS Config*/
#define TLS_CONFIG_THUMB                	CFG_ON  /*Config THUMB instruction */
#define TLS_CONFIG_ARM                  	CFG_OFF /*Config ARM instruction */

#define TLS_CONFIG_TLS_DEBUG            	CFG_OFF /*DEBUG Config**/

/**Driver Support**/
#define TLS_CONFIG_VIRTUAL_EFUSE      		CFG_ON  /*Efuse*/
#define TLS_CONFIG_HS_SPI          			CFG_OFF /*High Speed SPI*/
#define TLS_CONFIG_UART						CFG_ON  /*UART*/


/**Host Interface&Command**/
#define TLS_CONFIG_HOSTIF 					CFG_ON
#define TLS_CONFIG_AT_CMD					(CFG_ON && TLS_CONFIG_HOSTIF)
#define TLS_CONFIG_RI_CMD					(CFG_OFF && TLS_CONFIG_HOSTIF)


/**FLASH MAP**/
#define TLS_FLASH_BOOT_FOOTER_ADDR			(0x00000000)
#define TLS_FLASH_MBR_ADDR					(0x00000000)
#define TLS_FLASH_PARAM1_ADDR				(0x00001000)
#define TLS_FLASH_PARAM2_ADDR				(0x00001800)
#define TLS_FLASH_PARAM_DEFAULT				(0x00002000)
#define TLS_FLASH_FIRMWARE1_ADDR			(0x00003000)
#define TLS_FLASH_FIRMWARE2_ADDR			(0x0004e000)
#define TLS_FLASH_DBR_ADDR					(0x00099000)
#define TLS_FLASH_FSINFO_ADDR				(0x00099200)
#define TLS_FLASH_FAT1_ADDR					(0x0009a000)
#define TLS_FLASH_FAT2_ADDR					(0x0009b000)
#define TLS_FLASH_DATA_END					(0x00800000)

/**EXEC ADDR**/
#define TLS_SRAM_FIRMWARE_EXEC_ADDR			(0x00000400)

/** SOCKET CONFIG **/
#define TLS_CONFIG_SOCKET_STD				CFG_ON
#define TLS_CONFIG_SOCKET_RAW				CFG_ON

/** HTTP CLIENT **/
#define TLS_CONFIG_HTTP_CLIENT				(CFG_ON && TLS_CONFIG_SOCKET_STD)
#define TLS_CONFIG_HTTP_CLIENT_PROXY		CFG_OFF
#define TLS_CONFIG_HTTP_CLIENT_AUTH_BASIC	CFG_OFF
#define TLS_CONFIG_HTTP_CLIENT_AUTH_DIGEST	CFG_OFF
#define TLS_CONFIG_HTTP_CLIENT_AUTH			(TLS_CONFIG_HTTP_CLIENT_AUTH_BASIC || TLS_CONFIG_HTTP_CLIENT_AUTH_DIGEST)
#define TLS_CONFIG_HTTP_CLIENT_SECURE		CFG_OFF

/*******************WIFI INFO**************************
  			Below Switch Only for Reference!!!
********************************************************/
#define TLS_CONFIG_SUPPLICANT_DEBUG     	CFG_OFF
#define TLS_CONFIG_WL_DEBUG             	CFG_OFF

#define  TLS_CONFIG_AP        				CFG_ON  
#define  TLS_CONFIG_IBSS		 			CFG_OFF  

#define  TLS_CONFIG_WPS       				CFG_ON  /* WPS&EAPOL should be enabled together */
#define  TLS_IEEE8021X_EAPOL   				CFG_ON

/*******************************************************
         TASK PRIORITY for Current  CONFIG
********************************************************/
#define TLS_WL_TASK_PRIO_MAX       			12

#define TLS_SUPPLICANT_TASK_PRIO      		(TLS_WL_TASK_PRIO_MAX + 1)
#define TLS_SUPPLICANT_TIMER_TASK_PRIO 		(TLS_WL_TASK_PRIO_MAX + 2)
#define TLS_LWIP_TASK_PRIO            		(TLS_WL_TASK_PRIO_MAX + 3)
#define TLS_HSPI_TX_TASK_PRIO         		(TLS_WL_TASK_PRIO_MAX + 6)
#define TLS_HSPI_RX_TASK_PRIO         		(TLS_WL_TASK_PRIO_MAX + 7)
#define TLS_SYS_TASK_PRIO             		(TLS_WL_TASK_PRIO_MAX + 8)
#define TLS_UART0_TX_TASK_PRIO        		(TLS_WL_TASK_PRIO_MAX + 9)
#define TLS_UART0_RX_TASK_PRIO        		(TLS_WL_TASK_PRIO_MAX + 10)
#define TLS_UART1_TX_TASK_PRIO        		(TLS_WL_TASK_PRIO_MAX + 11)
#define TLS_UART1_RX_TASK_PRIO        		(TLS_WL_TASK_PRIO_MAX + 12)
#define TLS_SPI_SCHEDULER_TASK_PRIO   		(TLS_WL_TASK_PRIO_MAX + 13)
#define TLS_FWUP_TASK_PRIO            		(TLS_WL_TASK_PRIO_MAX + 14)

#endif /*__WM_CONFIG_H__*/

