#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_hal.h"

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
int kiiHal_dns(char* hostName, unsigned char* buf)
{
#if 0
	struct hostent* host;

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
#else
	int ret = -1;
	struct sockaddr_in address;
	struct addrinfo *result = NULL;
	struct addrinfo hints;

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(hostName, NULL, &hints, &result) == 0)
	{
		struct addrinfo* res = result;

		/* prefer ip4 addresses */
		while (res)
		{
			if (res->ai_family == AF_INET)
			{
				result = res;
				break;
			}
			res = res->ai_next;
		}

		if (result->ai_family == AF_INET)
		{
			address.sin_addr = ((struct sockaddr_in*)(result->ai_addr))->sin_addr;
			memcpy(buf, (char*)&address.sin_addr.s_addr, 4);
			ret = 0;
		}
		freeaddrinfo(result);
	}

	return ret;
#endif	
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
	if(socketNum < 0)
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
int kiiHal_socketClose(int* socketNum)
{
	int ret = 0;

	if(close(*socketNum) != 0)
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
int kiiHal_connect(int socketNum, char* saData, int port)
{
	int ret = 0;

	struct sockaddr_in pin;

	memset(&pin, 0, sizeof(struct sockaddr));
	pin.sin_family = AF_INET; // use IPv4
	memcpy((char*)&pin.sin_addr.s_addr, saData, 4);
	pin.sin_port = htons(port);
	if(connect(socketNum, (struct sockaddr*)&pin, sizeof(struct sockaddr)) != 0)
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
int kiiHal_socketSend(int socketNum, char* buf, int len)
{
	int bytes;
	int sent;

	bytes = 0;
	while(bytes < len)
	{
		sent = send(socketNum, buf + bytes, len - bytes, 0);
		if(sent <= 0)
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
int kiiHal_socketRecv(int socketNum, char* buf, int len)
{
	int ret;

	ret = recv(socketNum, buf, len, 0);
	if(ret <= 0)
	{
		ret = -1;
	}

	return ret;
}

/*****************************************************************************
*
*  kiiHal_transfer
*
*  \param   host - the host name
*               buf - data buffer;
*               bufLen - size of buffer in bytes;
*               sendLen - the length of data to be sent;
*
*  \return  0:success; -1: failure
*
*  \brief  Sends and receives data from the internet
*
*****************************************************************************/
int kiiHal_transfer(char* host, char* buf, int bufLen, int sendLen)
{
	int socketNum;
	unsigned char ipBuf[4];
	int bytes;
	int len;
	char* p1;
	char* p2;
	unsigned long contentLengh;
	int ret = -1;

	// KII_DEBUG("kii-info: host ""%s""\r\n", host);
	if(kiiHal_dns(host, ipBuf) < 0)
	{
		KII_DEBUG("kii-error: dns failed !\r\n");
		goto exit;
	}
	// KII_DEBUG("Host ip:%d.%d.%d.%d\r\n", ipBuf[0], ipBuf[1], ipBuf[2], ipBuf[3]);

	socketNum = kiiHal_socketCreate();
	if(socketNum < 0)
	{
		KII_DEBUG("kii-error: create socket failed !\r\n");
		goto exit;
	}

	if(kiiHal_connect(socketNum, (char*)ipBuf, KII_DEFAULT_PORT) < 0)
	{
		KII_DEBUG("kii-error: connect to server failed \r\n");
		goto close_socket;
	}

	len = kiiHal_socketSend(socketNum, buf, sendLen);
	if(len < 0)
	{

		KII_DEBUG("kii-error: send data fail\r\n");
		goto close_socket;
	}

	bytes = 0;
	memset(buf, 0, bufLen);
	while(bytes < bufLen)
	{
		len = kiiHal_socketRecv(socketNum, buf + bytes, bufLen - bytes);
		if(len < 0)
		{
			KII_DEBUG("kii-error: recv data fail\r\n");
			goto close_socket;
		}
		else
		{
			bytes += len;
			p1 = strstr(buf, STR_CRLFCRLF);
			if(p1 != NULL)
			{
				p2 = strstr(buf, STR_CONTENT_LENGTH);
				if(p2 != NULL)
				{
					p2 += strlen(STR_CONTENT_LENGTH);
					contentLengh = strtoul(p2, 0, 0);
					if(contentLengh > 0)
					{
						p1 += strlen(STR_CRLFCRLF);
						if(bytes >= (contentLengh + (p1 - buf)))
						{
							ret = 0;
							goto close_socket;
						}
					}
					else   // should never get here
					{
						KII_DEBUG("kii-error: get content lenght failed\r\n");
						goto close_socket;
					}
				}
				else
				{
					ret = 0; // no content length
					goto close_socket;
				}
			}
		}
	}
	KII_DEBUG("kii-error: receiving buffer overflow !\r\n");
close_socket:
	kiiHal_socketClose(&socketNum);
exit:
	return ret;
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
	usleep(ms * 1000);
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
#if 0
static pthread_t m_pthid1;
static pthread_t m_pthid2;

int kiiHal_taskCreate(const char* name,
                      KiiHal_taskEntry pEntry,
                      void* param,
                      unsigned char* stk_start,
                      unsigned int stk_size,
                      unsigned int prio)
{
	int ret;
	pthread_t *pthid;
    static int index = 0;

    if (index == 0)
    {
        pthid = &m_pthid1;
        index++;
    }
	else if(index == 1)
	{
        index++;
        pthid = &m_pthid2;
	}
	else
	{
	    return -1;
	}
		
	ret = pthread_create(pthid, NULL, pEntry, param);

	if(ret == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}
#else
int kiiHal_taskCreate(const char* name,
                      KiiHal_taskEntry pEntry,
                      void* param,
                      unsigned char* stk_start,
                      unsigned int stk_size,
                      unsigned int prio)
{
	int ret;
	pthread_t pthid;

	ret = pthread_create(&pthid, NULL, pEntry, param);

	if(ret == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}
#endif

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
void* kiiHal_malloc(unsigned long size)
{
	return malloc(size);
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
void kiiHal_free(void* p)
{
	free(p);
}
