#include <string.h>
#include <stdio.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_hal.h"
#include "kii_push.h"
#include "kii_mqtt.h"

extern kii_push_struct g_kii_push;

/*****************************************************************************
*
*  KiiMQTT_encode
*
*  \param  buf - the input of buffer to be encoded
*               length - the buffer length
*
*  \return the number of bytes written to buffer
*
*  \brief  Encodes the message length according to the MQTT algorithm
*
*****************************************************************************/
int KiiMQTT_encode(char* buf, int length)
{
	int rc = 0;
	char d;

	do
	{
		d = length % 128;
		length /= 128;
		if (length > 0)
			d |= 0x80;
		buf[rc++] = d;
	} while (length > 0);
	return rc;
}


/*****************************************************************************
*
*  KiiMQTT_decode
*
*  \param  buf - the input of buffer to be dencoded
*               value - the decoded length returned
*
*  \return the number of bytes written to buffer
*
*  \brief  Decodes the message length according to the MQTT algorithm
*
*****************************************************************************/
int KiiMQTT_decode(char* buf, int *value)
{
	int i = 0;
	int multiplier = 1;
	int len = 0;
	*value = 0;
	do
	{
		if (++len > 4)
		{
			return -1;
		}
		*value += (buf[i] & 127) * multiplier;
		multiplier *= 128;
	} while ((buf[i++] & 128) != 0);

	return len;
}


/*****************************************************************************
*
*  KiiMQTT_connect
*
*  \param  keepAliveInterval -  keep alive in seconds
*
*  \return 0:success; -1: failure
*
*  \brief  Connects to MQTT broker
*
*****************************************************************************/
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
    //KII_DEBUG("broker ip::%d.%d.%d.%d\r\n", ipBuf[0], ipBuf[1], ipBuf[2], ipBuf[3]);
	
    if (kiiHal_connect(g_kii_push.mqttSocket, (char*)ipBuf, KII_MQTT_DEFAULT_PORT) < 0)
    {
        KII_DEBUG("kii-error: connect to server failed \r\n");
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    i = 0;
    //Variable header:Protocol Name bytes
    buf[i++] = 0x00;
    buf[i++] = 0x06;
    buf[i++] = 'M';
    buf[i++] = 'Q';
    buf[i++] = 'I';
    buf[i++] = 's';
    buf[i++] = 'd';
    buf[i++] = 'p';
    //Variable header:Protocol Level
    buf[i++] = 0x03;
    //Variable header:Connect Flags
    /*
    * Bit   7                          6                        5                    4  3            2            1                      0
    *        User Name Flag     Password Flag    Will Retain        Will QoS     Will Flag   Clean Session   Reserved
    */
    buf[i++] = 0xc2;
    //Variable header:Keep Alive
    buf[i++] = (keepAliveInterval&0xff00) >> 8;
    buf[i++] = keepAliveInterval&0x00ff;
    //Payload:Client Identifier
    buf[i++] = (strlen(g_kii_push.mqttTopic) & 0xff00) >> 8;
    buf[i++] = strlen(g_kii_push.mqttTopic) & 0x00ff;
    strcpy(&buf[i], g_kii_push.mqttTopic);
    i +=strlen(g_kii_push.mqttTopic);
    //Payload:User Name
    buf[i++] = (strlen(g_kii_push.username) & 0xff00) >> 8;
    buf[i++] = strlen(g_kii_push.username) & 0x00ff;
    strcpy(&buf[i], g_kii_push.username);
    i +=strlen(g_kii_push.username);
    //Payload:Password
    buf[i++] = (strlen(g_kii_push.password) & 0xff00) >> 8;
    buf[i++] = strlen(g_kii_push.password) & 0x00ff;
    strcpy(&buf[i], g_kii_push.password);
    i +=strlen(g_kii_push.password);

    j = 0;
    memset(g_kii_push.sendBuf, 0, KII_PUSH_SEND_BUF_SIZE);
    //Fixed header:byte1
    g_kii_push.sendBuf[j++] = 0x10;  
    //Fixed header:Remaining Length
    j +=KiiMQTT_encode(&g_kii_push.sendBuf[j], i);

    //copy the other tytes
    memcpy(&g_kii_push.sendBuf[j], buf, i);
    j +=i;

   // KII_DEBUG("\r\n----------------MQTT connect send start-------------\r\n");    
    //KII_DEBUG("\r\n");
    //for (i=0; i<j; i++)
    //{
    //    KII_DEBUG("%02x", g_kii_push.sendBuf[i]);
    //}
    //KII_DEBUG("\r\n");

    //KII_DEBUG("\r\n----------------MQTT connect send end-------------\r\n");    

    if (kiiHal_socketSend(g_kii_push.mqttSocket, g_kii_push.sendBuf, j) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
        return -1;
    }

    memset(g_kii_push.rcvdBuf, 0, KII_PUSH_RECV_BUF_SIZE);
    g_kii_push.rcvdCounter = kiiHal_socketRecv(g_kii_push.mqttSocket, g_kii_push.rcvdBuf, KII_PUSH_RECV_BUF_SIZE);
    if (g_kii_push.rcvdCounter <= 0)
    {
        KII_DEBUG("kii-error: recv data fail\r\n");
        return -1;
    }
    else
    {
    	//KII_DEBUG("\r\n----------------MQTT connect recv start-------------\r\n");    
	//KII_DEBUG("\r\n");
    	//for (i=0; i<g_kii_push.rcvdCounter; i++)
    	//{
        //	KII_DEBUG("%02x", g_kii_push.rcvdBuf[i]);
    	//}
	//KII_DEBUG("\r\n");

    	//KII_DEBUG("\r\n----------------MQTT_connect recv end-------------\r\n");    

	if ((g_kii_push.rcvdCounter == 4) 
        && (g_kii_push.rcvdBuf[0] == 0x20) 
        && (g_kii_push.rcvdBuf[1] == 0x02) 
        && (g_kii_push.rcvdBuf[2] == 0x00)
        && (g_kii_push.rcvdBuf[3] == 0x00))
	{
            return 0;
	}
       else
       {
        KII_DEBUG("kii-error: invalid data format\r\n");
        return -1;
       }
    }
}


/*****************************************************************************
*
*  KiiMQTT_subscribe
*
*  \param  qos -  QoS value, 0, 1 or 2
*
*  \return 0:success; -1: failure
*
*  \brief  Subscribes MQTT topic
*
*****************************************************************************/
int KiiMQTT_subscribe(enum QoS qos)
{
    char buf[256];
    int i;
    int j;

    memset(buf, 0, sizeof(buf));
    i = 0;

    //Variable header:Packet Identifier
    buf[i++] = 0x00;
    buf[i++] = 0x01;
    //Payload:topic length
    buf[i++] = (strlen(g_kii_push.mqttTopic) & 0xff00) >> 8;
    buf[i++] = strlen(g_kii_push.mqttTopic) & 0x00ff;
    //Payload:topic
    strcpy(&buf[i], g_kii_push.mqttTopic);
    i +=strlen(g_kii_push.mqttTopic);
    //Payload: qos
    buf[i++] = qos;

    j = 0;
    memset(g_kii_push.sendBuf, 0, KII_PUSH_SEND_BUF_SIZE);
    //Fixed header: byte1
    g_kii_push.sendBuf[j++] = 0x82;  
    //Fixed header:Remaining Length
    j +=KiiMQTT_encode(&g_kii_push.sendBuf[j], i);

    //copy the other tytes
    memcpy(&g_kii_push.sendBuf[j], buf, i);
    j +=i;

    //KII_DEBUG("\r\n----------------MQTT subscribe send start-------------\r\n");    
    //KII_DEBUG("\r\n");
    //for (i=0; i<j; i++)
    //{
    //    KII_DEBUG("%02x", g_kii_push.sendBuf[i]);
    //}
    //KII_DEBUG("\r\n");

    //KII_DEBUG("\r\n----------------MQTT subscribe send end-------------\r\n");    

    if (kiiHal_socketSend(g_kii_push.mqttSocket, g_kii_push.sendBuf, j) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
        return -1;
    }

    memset(g_kii_push.rcvdBuf, 0, KII_PUSH_RECV_BUF_SIZE);
    g_kii_push.rcvdCounter = kiiHal_socketRecv(g_kii_push.mqttSocket, g_kii_push.rcvdBuf, KII_PUSH_RECV_BUF_SIZE);
    if (g_kii_push.rcvdCounter <= 0)
    {
        KII_DEBUG("kii-error: recv data fail\r\n");
        return -1;
    }
    else
    {
    //KII_DEBUG("\r\n----------------MQTT subscribe recv start-------------\r\n");    
//	KII_DEBUG("\r\n");
    //for (i=0; i<g_kii_push.rcvdCounter; i++)
    //{
     //   KII_DEBUG("%02x", g_kii_push.rcvdBuf[i]);
    //}
//	KII_DEBUG("\r\n");

    //KII_DEBUG("\r\n----------------MQTT subscribe recv end-------------\r\n");    

	if ((g_kii_push.rcvdCounter == 5)
         && ((unsigned char)g_kii_push.rcvdBuf[0] == 0x90)
         && (g_kii_push.rcvdBuf[1] == 0x03)
         && (g_kii_push.rcvdBuf[2] == 0x00)
         && (g_kii_push.rcvdBuf[3] == 0x01))
	{
            return 0;
	}
       else
       {
        KII_DEBUG("kii-error: invalid data format\r\n");
        return -1;
       }
    }
}


/*****************************************************************************
*
*  KiiMQTT_pingReq
*
*  \param  None
*
*  \return 0:success; -1: failure
*
*  \brief  Sends "PINGREQ"
*
*****************************************************************************/
int KiiMQTT_pingReq(void)
{
//	  int i;
    memset(g_kii_push.sendBuf, 0, KII_PUSH_SEND_BUF_SIZE);
    g_kii_push.sendBuf[0] = 0xc0;
    g_kii_push.sendBuf[1] = 0x00;
    if (kiiHal_socketSend(g_kii_push.mqttSocket, g_kii_push.sendBuf, 2) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
        return -1;
    }
    else
    {
        return 0;
    }
}

