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
    int rcvdCounter;
    char buf[256];
    int i;
    int j;
    int k;

    if (kiiHal_dns(g_kii_push.host, ipBuf) < 0)
    {
        KII_DEBUG("kii-error: mqtt dns failed !\r\n");
        return -1;
    }
    //KII_DEBUG("broker ip::%d.%d.%d.%d\r\n", ipBuf[0], ipBuf[1], ipBuf[2], ipBuf[3]);

    if (g_kii_push.mqttSocket >= 0)
    {
        kiiHal_socketClose(&g_kii_push.mqttSocket);
    }
    g_kii_push.mqttSocket= kiiHal_socketCreate();
    if (g_kii_push.mqttSocket < 0)
    {
        KII_DEBUG("kii-error: create mqtt socket failed !\r\n");
        return -1;
    }
	
    if (kiiHal_connect(g_kii_push.mqttSocket, (char*)ipBuf, KII_MQTT_DEFAULT_PORT) < 0)
    {
        KII_DEBUG("kii-error: connect to mqtt server failed \r\n");
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    i = 8;  //reserver 8 bytes for header
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
    //Fixed header:byte1
    buf[j++] = 0x10;  
    //Fixed header:Remaining Length
    j +=KiiMQTT_encode(&buf[j], i-8);

    //copy the other tytes
    for (k=0; k<i-8; k++)
    {
        buf[j++] = buf[8+k];
    }
/*
    KII_DEBUG("\r\n----------------MQTT connect send start-------------\r\n");    
    KII_DEBUG("j=%d\r\n", j);       
    KII_DEBUG("\r\n");
    for (i=0; i<j; i++)
    {
        KII_DEBUG("%02x", buf[i]);
    }
    KII_DEBUG("\r\n");

    KII_DEBUG("\r\n----------------MQTT connect send end-------------\r\n");    
*/
    if (kiiHal_socketSend(g_kii_push.mqttSocket, buf, j) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    rcvdCounter = kiiHal_socketRecv(g_kii_push.mqttSocket, buf, sizeof(buf));
    if (rcvdCounter <= 0)
    {
        KII_DEBUG("kii-error: recv data fail\r\n");
        return -1;
    }
    else
    {
/*
    	KII_DEBUG("\r\n----------------MQTT connect recv start-------------\r\n");    
	KII_DEBUG("\r\n");
    	for (i=0; i<rcvdCounter; i++)
    	{
        	KII_DEBUG("%02x", buf[i]);
    	}
	KII_DEBUG("\r\n");

    	KII_DEBUG("\r\n----------------MQTT_connect recv end-------------\r\n");    
*/
	if ((rcvdCounter == 4) 
        && (buf[0] == 0x20) 
        && (buf[1] == 0x02) 
        && (buf[2] == 0x00)
        && (buf[3] == 0x00))
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
    int rcvdCounter;
    int i;
    int j;
    int k;

    memset(buf, 0, sizeof(buf));
    i = 8;

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
    //Fixed header: byte1
    buf[j++] = 0x82;  
    //Fixed header:Remaining Length
    j +=KiiMQTT_encode(&buf[j], i-8);

    //copy the other tytes
    for (k=0; k<i-8; k++)
    {
        buf[j++] = buf[8+k];
    }
/*
    KII_DEBUG("\r\n----------------MQTT subscribe send start-------------\r\n");    
    KII_DEBUG("\r\n");
    for (i=0; i<j; i++)
    {
        KII_DEBUG("%02x", buf[i]);
    }
    KII_DEBUG("\r\n");

    KII_DEBUG("\r\n----------------MQTT subscribe send end-------------\r\n");    
*/
    if (kiiHal_socketSend(g_kii_push.mqttSocket, buf, j) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    rcvdCounter = kiiHal_socketRecv(g_kii_push.mqttSocket, buf, sizeof(buf));
    if (rcvdCounter <= 0)
    {
        KII_DEBUG("kii-error: recv data fail\r\n");
        return -1;
    }
    else
    {
/*
        KII_DEBUG("\r\n----------------MQTT subscribe recv start-------------\r\n");    
        KII_DEBUG("\r\n");
        for (i=0; i<rcvdCounter; i++)
        {
            KII_DEBUG("%02x", buf[i]);
        }
	  KII_DEBUG("\r\n");

        KII_DEBUG("\r\n----------------MQTT subscribe recv end-------------\r\n");    
*/
	if ((rcvdCounter == 5)
         && ((unsigned char)buf[0] == 0x90)
         && (buf[1] == 0x03)
         && (buf[2] == 0x00)
         && (buf[3] == 0x01))
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
    char buf[2];

    memset(buf, 0, sizeof(buf));
    buf[0] = 0xc0;
    buf[1] = 0x00;
    if (kiiHal_socketSend(g_kii_push.mqttSocket, buf, sizeof(buf)) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
        return -1;
    }
    else
    {
        return 0;
    }
}

