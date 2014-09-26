#include <string.h>
#include "wm_include.h"
#include "api.h"

#include "kii.h"
#include "kii_def.h"

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
*  \brief  get host IP address
*
*****************************************************************************/
int kiiHal_dns(char *host, unsigned char *buf)
{
    int ret = 0;
    ip_addr_t addr;

    if (netconn_gethostbyname(host, &addr) == 0)
    {
        buf[0] = addr.addr& 0xff;
        buf[1] = (addr.addr>> 8) & 0xff;
        buf[2] = (addr.addr>> 16) & 0xff;
        buf[3] = (addr.addr>> 24) & 0xff;
    }
	else
    {
        ret =  -1;
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
*  \brief  create socket
*
*****************************************************************************/
int kiiHal_socketCreate(void)
{
    int socketNum;
	
    socketNum = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
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
*  \param  socketNum - socket handle
*
*  \return  0:success; -1: failure
*
*  \brief  Closes a socket
*
*****************************************************************************/
int kiiHal_socketClose(int socketNum)
{
    int ret = 0;
	
    if (closesocket(socketNum) !=  0)
    	{
    	ret = -1;
    	}
	return ret;
}



/*****************************************************************************
*
*  kiiHal_connect
*
*  \param  socketNum - socket handle
*               saData - address
*
*  \return  0:success; -1: failure
*
*  \brief  connect a TCP socket
*
*****************************************************************************/
int kiiHal_connect(int socketNum, char *saData)
{
    int ret = 0;
	
	struct sockaddr_in pin;

	memset(&pin, 0, sizeof(struct sockaddr));
	pin.sin_family=AF_INET; //use IPv4
	memcpy((char *)&pin.sin_addr.s_addr, saData, 4);
	pin.sin_port=htons(80);
	if (connect(socketNum, (struct sockaddr *)&pin, sizeof(struct sockaddr)) != 0)
	{
		ret = -1;
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
*  \brief  sends data out to the internet
*
*****************************************************************************/
int kiiHal_socketSend(int socketNum, char * buf, int len)
{
    int ret;

      ret = send(socketNum, buf , len, 0);
      if (ret < 0)
      	{
      	    ret = -1;
      	}

    KII_DEBUG("\r\n ========send data start=====\r\n");
    KII_DEBUG("%s", buf);
    KII_DEBUG("\r\n ========send data end=====\r\n");

	  return ret;
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

     ret = recv(socketNum, buf, len, 0);
     if (ret < 0)
     	{
     	    ret =-1;
     	}

    KII_DEBUG("\r\n ========recv data start=====\r\n");
    KII_DEBUG("%s", buf);
    KII_DEBUG("\r\n ========recv data end=====\r\n");

    return ret;
}

/*****************************************************************************
*
*  kiiHal_transfer
*
*  \param  none
*
*  \return  0:success; -1: failure
*
*  \brief  Sends and receives data from the internet
*
*****************************************************************************/
int kiiHal_transfer(void)
{
    int socketNum;
    unsigned char ipBuf[4];

    if (kiiHal_dns(g_kii_data.host, ipBuf) < 0)
    {
        KII_DEBUG("kii-error: dns failed !\r\n");
        return -1;
    }
    KII_DEBUG("Host ip:%d.%d.%d.%d\r\n", ipBuf[3], ipBuf[2], ipBuf[1], ipBuf[0]);
		
    socketNum = kiiHal_socketCreate();
    if (socketNum < 0)
    {
        KII_DEBUG("kii-error: create socket failed !\r\n");
        return -1;
    }
	
	
    if (kiiHal_connect(socketNum, (char*)ipBuf) < 0)
    {
        KII_DEBUG("kii-error: connect to server failed \r\n");
	 kiiHal_socketClose(socketNum);
        return -1;
    }
    
    if (kiiHal_socketSend(socketNum, g_kii_data.sendBuf, g_kii_data.sendDataLen) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
	 kiiHal_socketClose(socketNum);
        return -1;
    }

    memset(g_kii_data.rcvdBuf, 0, KII_RECV_BUF_SIZE);
    g_kii_data.rcvdCounter = kiiHal_socketRecv(socketNum, g_kii_data.rcvdBuf, KII_RECV_BUF_SIZE);
    if (g_kii_data.rcvdCounter < 0)
    {
        KII_DEBUG("kii-error: recv data fail\r\n");
	 kiiHal_socketClose(socketNum);
        return -1;
    }
    else
    {
	 kiiHal_socketClose(socketNum);
        return 0;
    }
}


/*****************************************************************************
*
*  kiiHal_delayMs
*
*  \param  ms - the millisecond to delay
*
*  \return  none
*
*  \brief  delay ms, the minimal precision is 10ms for WinnerMicro
*
*****************************************************************************/
void kiiHal_delayMs(unsigned int ms)
{
	OSTimeDly (ms*OS_TICKS_PER_SEC/1000);
}

int kiiHal_taskCreate(tls_os_task_t *task,
      const char* name,
      void (*entry)(void* param), 
      void* param,
      unsigned char *stk_start,
      unsigned int stk_size,
      unsigned int prio,
      unsigned int flag)
{
	if (tls_os_task_create(task, name, entry, param, stk_start, stk_size, prio, flag) == TLS_OS_SUCCESS)
	{
	    return 0;
	}
        else
        {
            return -1;
        }

}

