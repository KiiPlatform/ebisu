#include <string.h>
#include "stdarg.h"

#include "kii.h"
#include "kii_def.h"
#include "kii_hal.h"
#include "kii_push.h"

#include <LWiFi.h>
#include <LWiFiClient.h>
#include <LTask.h>

#ifdef __cplusplus
 extern "C" {
#endif

static LWiFiClient m_client;

extern kii_push_struct g_kii_push;

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
    return 0;
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
    return 0;
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
    *socketNum = -1;
	
    return 0;
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
    if (m_client.connected() == 1)
    {
        m_client.stop();
    }
	kiiHal_delayMs(500);
	if (m_client.connect(g_kii_push.host, KII_MQTT_DEFAULT_PORT) == 1)
	{
	    return 0;
	}
	else
	{
		return -1;
	}
    return -1;
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
        sent =  m_client.write((uint8_t*)(buf+bytes), len-bytes);
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

	while (m_client.connected() )
	{
	    if (m_client.available()) //issue: one byte will be lost after caling this available api if call "read((uint8_t*)buf, len)"
	    {
	        buf[0] = m_client.read();   
            ret = m_client.read((uint8_t*)(buf+1), len-1);
            return ret+1;
	    }
	}
	return -1;
}

/*****************************************************************************
*
*  kiiHal_socketRecvDataAvailable
*
*  \param None
*
*  \return  0:data available; -1: no data
*
*  \brief  Queries if there are incoming data
*
*****************************************************************************/
int kiiHal_socketRecvDataAvailable(void)
{
	if (m_client.available())
	{
	    return 0;
	}
	else
	{
	    return -1;
	}
}

/*****************************************************************************
*
*  kiiHal_socketConnected
*
*  \param None
*
*  \return  0: connected; -1: not connected
*
*  \brief  Queries if this client object has been connected
*
*****************************************************************************/
int kiiHal_socketConnected(void)
{
	if (m_client.connected())
	{
	    return 0;
	}
	else
	{
	    return -1;
	}
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
    LWiFiClient c;

	int bytes;
	int len;
	char* p1;
	char* p2;
	unsigned long contentLengh;


    if(0 == c.connect(host, KII_DEFAULT_PORT))
    {
        KII_DEBUG("Connect to ""%s"" failed", host);
        return -1;
    }

    bytes = 0;
    while(bytes < sendLen)
    {
        len = (size_t)c.write((const uint8_t *)(buf+bytes), (size_t)(sendLen-bytes));
		if (len <= 0)
        {
            KII_DEBUG("kii-error: send data fail\r\n");
            return -1;
        }
        else
        {
            bytes += len;
        }
    }

    bytes = 0;
    memset(buf, 0, bufLen);
    while(bytes < bufLen )
    {
        len = (size_t)c.read((uint8_t *)(buf+bytes), (size_t)(bufLen-bytes));
        if (len  < 0)
        {
            KII_DEBUG("kii-error: recv data fail\r\n");
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
                            return 0;
			}
		     }
		     else //should never get here
		     {
			 KII_DEBUG("kii-error: get content lenght failed\r\n");
		         return -1;
		     }
	         }
	         else
	         {
		     return 0; //no content length
                 }
	    }
        }
    }
    KII_DEBUG("kii-error: receiving buffer overflow !\r\n");
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
*  \brief  Delay ms, the minimal precision is 10ms for WinnerMicro
*
*****************************************************************************/
void kiiHal_delayMs(unsigned int ms)
{
    delay((uint32_t) ms);
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
int kiiHal_taskCreate(const char* name,
                      KiiHal_taskEntry pEntry,
                      void* param,
                      unsigned char* stk_start,
                      unsigned int stk_size,
                      unsigned int prio)
{
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

static void kiiHal_itoa(char **buf, int i, int base)
{
	char *s;
#define LEN	20
	int rem;
	static char rev[LEN+1];

	rev[LEN] = 0;
	if (i == 0)
		{
		(*buf)[0] = '0';
		++(*buf);
		return;
		}
	s = &rev[LEN];
	while (i)
		{
		rem = i % base;
		if (rem < 10)
			*--s = rem + '0';
		else if (base == 16)
			*--s = "abcdef"[rem - 10];
		i /= base;
		}
	while (*s)
		{
		(*buf)[0] = *s++;
		++(*buf);
		}
}

void kiiHal_debug(const char *fmt,...)
{
  char print_buf[512*2];
   va_list ap;
   double dval;
   int ival;
   char *p, *sval;
   char *bp, cval;
   int fract;

	bp= print_buf;
	*bp= 0;
	
	va_start (ap, fmt);
	for (p= (char*)fmt; *p; p++)
	{
		if (*p != '%')
		{
			*bp++= *p;
			continue;
		}
		switch (*++p) {
		case 'd':
			ival= va_arg(ap, int);
			if (ival < 0){
				*bp++= '-';
			     ival= -ival;
			}
			kiiHal_itoa (&bp, ival, 10);
			break;
			
        	case 'o':
			ival= va_arg(ap, int);
			if (ival < 0){
				*bp++= '-';
			     ival= -ival;
			}
			*bp++= '0';
			kiiHal_itoa (&bp, ival, 8);
			break;
			
		case 'x':
			ival= va_arg(ap, int);
			if (ival < 0){
			     *bp++= '-';
			     ival= -ival;
			}
			*bp++= '0';
			*bp++= 'x';
			kiiHal_itoa (&bp, ival, 16);
			break;
			
		case 'c':
			cval= va_arg(ap, int);
			*bp++= cval;
			break;
			
		case 's':
			for (sval = va_arg(ap, char *) ; *sval ; sval++ )
			    *bp++= *sval;
			break;
		}
	}
	*bp= 0;
	va_end (ap);
	Serial.println(print_buf);
}



#ifdef __cplusplus
}
#endif


