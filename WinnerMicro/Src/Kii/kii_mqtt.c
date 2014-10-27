#include <string.h>
#include <stdio.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_hal.h"
#include "kii_push.h"
#include "kii_mqtt.h"

extern kii_push_struct g_kii_push;

int KiiMQTT_encode(char* buf, int length)
{
	int rc = 0;

	do
	{
		char d = length % 128;
		length /= 128;
		/* if there are more digits to encode, set the top bit of this digit */
		if (length > 0)
			d |= 0x80;
		buf[rc++] = d;
	} while (length > 0);
	return rc;
}


int KiiMQTT_connect(unsigned short keepAliveInterval)
{
    unsigned char ipBuf[4];
    char buf[256];
    int i;
    int j;

    if (kiiHal_dns(g_kii_push.host, ipBuf) < 0)
    {
        KII_DEBUG("kii-error: dns failed !\r\n");
        return -1;
    }
    KII_DEBUG("broker ip::%d.%d.%d.%d\r\n", ipBuf[0], ipBuf[1], ipBuf[2], ipBuf[3]);
		
    g_kii_push.mqttSocket= kiiHal_socketCreate();
    if (g_kii_push.mqttSocket < 0)
    {
        KII_DEBUG("kii-error: create socket failed !\r\n");
        return -1;
    }
	
	
    if (kiiHal_connect(g_kii_push.mqttSocket, (char*)ipBuf, KII_MQTT_DEFAULT_PORT) < 0)
    {
        KII_DEBUG("kii-error: connect to server failed \r\n");
	 kiiHal_socketClose(g_kii_push.mqttSocket);
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    i = 0;
    buf[i++] = 0x00;
    buf[i++] = 0x06;
    buf[i++] = 'M';
    buf[i++] = 'Q';
    buf[i++] = 'I';
    buf[i++] = 's';
    buf[i++] = 'd';
    buf[i++] = 'p';
    buf[i++] = 0x03;
    buf[i++] = 0xc2;
    buf[i++] = (keepAliveInterval&0xff00) >> 8;
    buf[i++] = keepAliveInterval&0x00ff;
    buf[i++] = (strlen(g_kii_push.mqttTopic) & 0xff00) >> 8;
    buf[i++] = strlen(g_kii_push.mqttTopic) & 0x00ff;
    strcpy(&buf[i], g_kii_push.mqttTopic);
    i +=strlen(g_kii_push.mqttTopic);
    buf[i++] = (strlen(g_kii_push.username) & 0xff00) >> 8;
    buf[i++] = strlen(g_kii_push.username) & 0x00ff;
    strcpy(&buf[i], g_kii_push.username);
    i +=strlen(g_kii_push.username);
    buf[i++] = (strlen(g_kii_push.password) & 0xff00) >> 8;
    buf[i++] = strlen(g_kii_push.password) & 0x00ff;
    strcpy(&buf[i], g_kii_push.password);
    i +=strlen(g_kii_push.password);

    j = 0;
    memset(g_kii_push.sendBuf, 0, KII_PUSH_SEND_BUF_SIZE);
    g_kii_push.sendBuf[j++] = 0x10;  //fixed header
    j +=KiiMQTT_encode(&g_kii_push.sendBuf[j], i);
    memcpy(&g_kii_push.sendBuf[j], buf, i);
    j +=i;

    printf("\r\----------------KiiMQTT_connect send start-------------\r\n");    
	printf("\r\n");
    for (i=0; i<j; i++)
    {
        printf("%02x", g_kii_push.sendBuf[i]);
    }
	printf("\r\n");

    printf("\r\----------------KiiMQTT_connect send end-------------\r\n");    

    if (kiiHal_socketSend(g_kii_push.mqttSocket, g_kii_push.sendBuf, j) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
	 kiiHal_socketClose(g_kii_push.mqttSocket);
        return -1;
    }

    memset(g_kii_push.rcvdBuf, 0, KII_PUSH_RECV_BUF_SIZE);
    g_kii_push.rcvdCounter = kiiHal_socketRecv(g_kii_push.mqttSocket, g_kii_push.rcvdBuf, KII_PUSH_RECV_BUF_SIZE);
    if (g_kii_push.rcvdCounter <= 0)
    {
        KII_DEBUG("kii-error: recv data fail\r\n");
	 kiiHal_socketClose(g_kii_push.mqttSocket);
        return -1;
    }
    else
    {
    printf("\r\----------------KiiMQTT_connect recv start-------------\r\n");    
	printf("\r\n");
    for (i=0; i<g_kii_push.rcvdCounter; i++)
    {
        printf("%02x", g_kii_push.rcvdBuf[i]);
    }
	printf("\r\n");

    printf("\r\----------------KiiMQTT_connect recv end-------------\r\n");    

	if (g_kii_push.rcvdCounter == 4 && g_kii_push.rcvdBuf[0] == 0x20 && g_kii_push.rcvdBuf[1] == 0x02 && g_kii_push.rcvdBuf[2] == 0x00 && g_kii_push.rcvdBuf[3] == 0x00)
	{
            return 0;
	}
       else
       {
        KII_DEBUG("kii-error: invalid data format\r\n");
	 kiiHal_socketClose(g_kii_push.mqttSocket);
        return -1;
       }
    }

}

int KiiMQTT_subscribe(enum QoS qos)
{
    char buf[256];
    int i;
    int j;

    memset(buf, 0, sizeof(buf));
    i = 0;
    buf[i++] = 0x00;
    buf[i++] = 0x01;
    buf[i++] = (strlen(g_kii_push.mqttTopic) & 0xff00) >> 8;
    buf[i++] = strlen(g_kii_push.mqttTopic) & 0x00ff;
    strcpy(&buf[i], g_kii_push.mqttTopic);
    i +=strlen(g_kii_push.mqttTopic);
    buf[i++] = qos;

    j = 0;
    memset(g_kii_push.sendBuf, 0, KII_PUSH_SEND_BUF_SIZE);
    g_kii_push.sendBuf[j++] = 0x82;  //fixed header
    j +=KiiMQTT_encode(&g_kii_push.sendBuf[j], i);
    memcpy(&g_kii_push.sendBuf[j], buf, i);
    j +=i;

    printf("\r\----------------KiiMQTT_subscribe send start-------------\r\n");    
	printf("\r\n");
    for (i=0; i<j; i++)
    {
        printf("%02x", g_kii_push.sendBuf[i]);
    }
	printf("\r\n");

    printf("\r\----------------KiiMQTT_subscribe send end-------------\r\n");    

    if (kiiHal_socketSend(g_kii_push.mqttSocket, g_kii_push.sendBuf, j) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
	 kiiHal_socketClose(g_kii_push.mqttSocket);
        return -1;
    }

    memset(g_kii_push.rcvdBuf, 0, KII_PUSH_RECV_BUF_SIZE);
    g_kii_push.rcvdCounter = kiiHal_socketRecv(g_kii_push.mqttSocket, g_kii_push.rcvdBuf, KII_PUSH_RECV_BUF_SIZE);
    if (g_kii_push.rcvdCounter <= 0)
    {
        KII_DEBUG("kii-error: recv data fail\r\n");
	 kiiHal_socketClose(g_kii_push.mqttSocket);
        return -1;
    }
    else
    {
    printf("\r\----------------KiiMQTT_subscribe recv start-------------\r\n");    
	printf("\r\n");
    for (i=0; i<g_kii_push.rcvdCounter; i++)
    {
        printf("%02x", g_kii_push.rcvdBuf[i]);
    }
	printf("\r\n");

    printf("\r\----------------KiiMQTT_subscribe recv end-------------\r\n");    

	if (g_kii_push.rcvdCounter == 5 && g_kii_push.rcvdBuf[0] == 0x90 && g_kii_push.rcvdBuf[1] == 0x03 && g_kii_push.rcvdBuf[2] == 0x00 && g_kii_push.rcvdBuf[3] == 0x01)
	{
            return 0;
	}
       else
       {
        KII_DEBUG("kii-error: invalid data format\r\n");
	 kiiHal_socketClose(g_kii_push.mqttSocket);
        return -1;
       }

	   
    }

}




int KiiMQTT_pingReq(void)
{
	    int i;
    memset(g_kii_push.sendBuf, 0, KII_PUSH_SEND_BUF_SIZE);
    g_kii_push.sendBuf[0] = 0xc0;
    g_kii_push.sendBuf[1] = 0x00;
    if (kiiHal_socketSend(g_kii_push.mqttSocket, g_kii_push.sendBuf, 2) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
	 kiiHal_socketClose(g_kii_push.mqttSocket);
        return -1;
    }

    memset(g_kii_push.rcvdBuf, 0, KII_PUSH_RECV_BUF_SIZE);
    g_kii_push.rcvdCounter = kiiHal_socketRecv(g_kii_push.mqttSocket, g_kii_push.rcvdBuf, KII_PUSH_RECV_BUF_SIZE);
    if (g_kii_push.rcvdCounter <= 0)
    {
        KII_DEBUG("kii-error: recv data fail\r\n");
	 kiiHal_socketClose(g_kii_push.mqttSocket);
        return -1;
    }
    else
    {
    printf("\r\----------------KiiMQTT_pingReq recv start-------------\r\n");    
	printf("\r\n");
    for (i=0; i<g_kii_push.rcvdCounter; i++)
    {
        printf("%02x", g_kii_push.rcvdBuf[i]);
    }
	printf("\r\n");

    printf("\r\----------------KiiMQTT_pingReq recv end-------------\r\n");    

	if (g_kii_push.rcvdCounter == 2 && g_kii_push.rcvdBuf[0] == 0xd0 && g_kii_push.rcvdBuf[1] == 0x00)
	{
            return 0;
	}
       else
       {
        KII_DEBUG("kii-error: invalid data format\r\n");
	 kiiHal_socketClose(g_kii_push.mqttSocket);
        return -1;
       }

	   
    }

}

