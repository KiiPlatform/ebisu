#include <string.h>
#include <pthread.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_hal.h"

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

    struct hostent *host;
	
    int portnumber,nbytes;

    host = gethostbyname(hostName);
    if(host != NULL)
    {
        memcpy(buf, host->h_addr_list[0], 4);
        return 0;
    }
    else
    {
        return -1;
    }
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
	
    socketNum = socket(AF_INET, SOCK_STREAM, 0);
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

    if (close(*socketNum) !=  0)
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
	
	struct sockaddr_in pin;

	memset(&pin, 0, sizeof(struct sockaddr));
	pin.sin_family=AF_INET; //use IPv4
	memcpy((char *)&pin.sin_addr.s_addr, saData, 4);
	pin.sin_port=htons(port);
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
*  \brief  Sends data out to the internet
*
*****************************************************************************/
int kiiHal_socketSend(int socketNum, char * buf, int len)
{
    int bytes;
    int sent;
	
    bytes = 0;
    while(bytes < len)
    {
        sent =  send(socketNum, buf+bytes, len-bytes, 0);
        if (sent < 0)
        {
        
            return -1;
        }
	else
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

     ret = recv(socketNum, buf, len, 0);
     if (ret < 0)
     	{
     	    ret =-1;
     	}


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
	
    len = kiiHal_socketSend(socketNum, g_kii_data.sendBuf, g_kii_data.sendDataLen);
    if (len < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
        kiiHal_socketClose(&socketNum);
        return -1;
    }
	
    bytes = 0;
    memset(g_kii_data.rcvdBuf, 0, KII_RECV_BUF_SIZE);
    while(bytes < KII_RECV_BUF_SIZE )
    {
        len = kiiHal_socketRecv(socketNum, g_kii_data.rcvdBuf+bytes, KII_RECV_BUF_SIZE-bytes);
        if (len  < 0)
        {
            KII_DEBUG("kii-error: recv data fail\r\n");
	    kiiHal_socketClose(&socketNum);
            return -1;
        }
	else
	{
	     bytes +=len;
	    p1 = strstr(g_kii_data.rcvdBuf, STR_CRLFCRLF);
	    if (p1  != NULL)
            {
                 p2 = strstr(g_kii_data.rcvdBuf, STR_CONTENT_LENGTH); 
	         if ( p2  != NULL)
	         {
	              p2 +=strlen(STR_CONTENT_LENGTH);
                     contentLengh = strtoul(p2, 0 , 0);
		     if (contentLengh > 0)
		     {
		         p1 +=strlen(STR_CRLFCRLF);
			if (bytes >= (contentLengh + (p1-g_kii_data.rcvdBuf)))
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
	usleep(ms*1000);
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
    int ret;
    pthread_t pthid;
 
    ret = pthread_create(&pthid,NULL, pEntry, param);

    if (ret == 0)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

