#include <string.h>
#include<stdlib.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_hal.h"

#include "simplelink.h"
#include "osi.h"
#include "rom_map.h"

extern kii_data_struct g_kii_data;

/*****************************************************************************
 *
 *  kiiHal_dns
 *
 *  \param  host - the input of host name
 *               buf - the out put of IP address
 *
 *  \return  0:success; -1: failure
 *
 *  \brief  Gets host IP address
 *
 *****************************************************************************/
int kiiHal_dns(char *hostName, unsigned char *buf) 
{
	int ret = 0;
	unsigned long addr;

    if (sl_NetAppDnsGetHostByName((signed char*)hostName, strlen(hostName),
                                                &addr, SL_AF_INET) == 0)
    {
		buf[3] = addr & 0xff;
		buf[2] = (addr >> 8) & 0xff;
		buf[1] = (addr >> 16) & 0xff;
		buf[0] = (addr >> 24) & 0xff;
    }
	else
	{
	    ret = -1;
	}
    return ret;
}

/*****************************************************************************
 *
 *  kiiHal_socketCreate
 *
 *  \param  none
 *
 *  \return  socket handle
 *
 *  \brief  Creates socket
 *
 *****************************************************************************/
int kiiHal_socketCreate(void) 
{
	int socketNum;

	socketNum = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, 0);
	if (socketNum < 0) 
	{
		socketNum = -1;
	}
	return socketNum;
}

/*****************************************************************************
 *
 *  kiiHal_socketClose
 *
 *  \param  socketNum - socket handle pointer
 *
 *  \return  0:success; -1: failure
 *
 *  \brief  Closes a socket
 *
 *****************************************************************************/
int kiiHal_socketClose(int *socketNum) 
{
	int ret = 0;

	if (sl_Close(*socketNum) != 0) 
	{
		ret = -1;
	}
	*socketNum = -1;
	return ret;
}

/*****************************************************************************
 *
 *  kiiHal_connect
 *
 *  \param  socketNum - socket handle
 *               saData - address
 *               port - port number
 *
 *  \return  0:success; -1: failure
 *
 *  \brief  Connects a TCP socket
 *
 *****************************************************************************/
int kiiHal_connect(int socketNum, char *saData, int port) 
{
	int ret = 0;

	struct SlSockAddrIn_t sAddr;

	memset(&sAddr, 0, sizeof(struct SlSockAddrIn_t));

	sAddr.sin_family = SL_AF_INET;
	sAddr.sin_port = sl_Htons((unsigned short) port);
	memcpy((char *) &sAddr.sin_addr.s_addr, saData, 4);
	if (sl_Connect(socketNum, (SlSockAddr_t *) &sAddr,
			sizeof(struct SlSockAddrIn_t)) < 0) 
	{
		return -1;
	}
	return ret;
}

/*****************************************************************************
 *
 *  kiiHal_socketSend
 *
 *  \param  socketNum - socket handle
 *               buf - date to be sent
 *               len - size of data in bytes
 *
 *  \return  Number of bytes sent
 *
 *  \brief  Sends data out to the internet
 *
 *****************************************************************************/
int kiiHal_socketSend(int socketNum, char * buf, int len) 
{
	int bytes;
	int sent;

	bytes = 0;
	while (bytes < len) 
	{
		sent = sl_Send(socketNum, buf + bytes, len - bytes, 0);
		if (sent < 0) 
		{

			return -1;
		} else 
		{
			bytes += sent;
		}
	}
	return bytes;
}

/*****************************************************************************
 *
 *  kiiHal_socketRecv
 *
 *  \param  socketNum - socket handle; 
 *               buf - data buffer to receive;
 *               len - size of buffer in bytes;
 *
 *  \return Number of bytes received
 *
 *  \brief  Receives data from the internet
 *
 *****************************************************************************/
int kiiHal_socketRecv(int socketNum, char * buf, int len) 
{
	int ret;

	ret = sl_Recv(socketNum, buf, len, 0);
	if (ret <= 0) 
	{
		ret = -1;
	}

	return ret;
}

/*****************************************************************************
*
*  kiiHal_transfer
*
*  \param   buf - data buffer;
*               bufLen - size of buffer in bytes;
*               sendLen - the length of data to be sent;
*
*  \return  0:success; -1: failure
*
*  \brief  Sends and receives data from the internet
*
*****************************************************************************/
int kiiHal_transfer(char *buf, int bufLen, int sendLen)
{
    int socketNum;
    unsigned char ipBuf[4];
    int bytes;
    int len;
    char * p1;
    char * p2;
    unsigned long contentLengh;

    //KII_DEBUG("kii-info: host ""%s""\r\n", g_kii_data.host);
    if (kiiHal_dns(g_kii_data.host, ipBuf) < 0)
    {
        KII_DEBUG("kii-error: dns failed !\r\n");
        return -1;
    }
    //KII_DEBUG("Host ip:%d.%d.%d.%d\r\n", ipBuf[0], ipBuf[1], ipBuf[2], ipBuf[3]);
		
    socketNum = kiiHal_socketCreate();
    if (socketNum < 0)
    {
        KII_DEBUG("kii-error: create socket failed !\r\n");
        return -1;
    }
	
    if (kiiHal_connect(socketNum, (char*)ipBuf, KII_DEFAULT_PORT) < 0)
    {
        KII_DEBUG("kii-error: connect to server failed \r\n");
	 kiiHal_socketClose(&socketNum);
        return -1;
    }
	
    len = kiiHal_socketSend(socketNum, buf, sendLen);
    if (len < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
        kiiHal_socketClose(&socketNum);
        return -1;
    }
	
    bytes = 0;
    memset(buf, 0, bufLen);
    while(bytes < bufLen )
    {
        len = kiiHal_socketRecv(socketNum, buf+bytes, bufLen-bytes);
        if (len  < 0)
        {
            KII_DEBUG("kii-error: recv data fail\r\n");
	    kiiHal_socketClose(&socketNum);
            return -1;
        }
	else
	{
	     bytes +=len;
	    p1 = strstr(buf, STR_CRLFCRLF);
	    if (p1  != NULL)
            {
                 p2 = strstr(buf, STR_CONTENT_LENGTH); 
	         if ( p2  != NULL)
	         {
	              p2 +=strlen(STR_CONTENT_LENGTH);
                     contentLengh = strtoul(p2, 0 , 0);
		     if (contentLengh > 0)
		     {
		         p1 +=strlen(STR_CRLFCRLF);
			if (bytes >= (contentLengh + (p1-buf)))
			{
  			    kiiHal_socketClose(&socketNum);
                            return 0;
			}
		     }
		     else //should never get here
		     {
			 KII_DEBUG("kii-error: get content lenght failed\r\n");
  		         kiiHal_socketClose(&socketNum);
		         return -1;
		     }
	         }
	         else
	         {
		     kiiHal_socketClose(&socketNum);
		     return 0; //no content length
                 }
	    }
        }
    }
    KII_DEBUG("kii-error: receiving buffer overflow !\r\n");
    kiiHal_socketClose(&socketNum);
    return -1; //buffer overflow
}


/*****************************************************************************
 *
 *  kiiHal_delayMs
 *
 *  \param  ms - the millisecond to delay
 *
 *  \return  none
 *
 *  \brief  Delay ms
 *
 *****************************************************************************/
void kiiHal_delayMs(unsigned int ms) 
{
	osi_Sleep(ms);
}

/*****************************************************************************
 *
 *  kiiHal_taskCreate
 *
 *  \param  name - task name
 *              pEntry - the entry function
 *              param - an optional data area which can be used to pass parameters to
 *                           the task when the task first executes
 *              stk_start -  the pointer to the task's bottom of stack
 *              stk_size - stack size
 *              prio - the priority of the task
 *
 *  \return 0:success; -1: failure
 *
 *  \brief  Creates task
 *
 *****************************************************************************/
int kiiHal_taskCreate(const char* name, KiiHal_taskEntry pEntry, void* param, unsigned char *stk_start, unsigned int stk_size, unsigned int prio)
{
	long lRetVal = 0;
	lRetVal = osi_TaskCreate(pEntry, (const signed char*) name, stk_size, param, prio, NULL);
	if (lRetVal < 0) 
	{
		return -1;
	}

	return 0;

}

/*****************************************************************************
 *
 *  kiiHal_malloc
 *
 *  \param  size - the size of memory to be allocated
 *
 *  \return  the address of memory
 *
 *  \brief  Allocates memory
 *
 *****************************************************************************/
void *kiiHal_malloc(unsigned long size) 
{
	return mem_Malloc(size);
}

/*****************************************************************************
 *
 *  kiiHal_free
 *
 *  \param  p - the address of memory
 *
 *  \return  none
 *
 *  \brief  Frees memory
 *
 *****************************************************************************/
void kiiHal_free(void *p) 
{
	mem_Free(p);
}

