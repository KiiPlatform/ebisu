#include <string.h>
#include "wm_include.h"
#include "api.h"

#include "kii_def.h"


int kiiHAL_dns(char *host, unsigned char *buf)
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

int kiiHAL_socketCreate(void)
{
    int socketNum;
	
    socketNum = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (socketNum < 0)
    {
        socketNum = -1;
    }
    return socketNum;
}

int kiiHAL_socketClose(int socketNum)
{
    int ret = 0;
	
    if (closesocket(socketNum) !=  0)
    	{
    	ret = -1;
    	}
	return ret;
}


int kiiHAL_connect(int socketNum, char *saData)
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


int kiiHAL_socketSend(int socketNum, char * buf, int len)
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


int kiiHAL_socketRecv(int socketNum, char * buf, int len)
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

