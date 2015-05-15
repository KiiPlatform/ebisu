#include <string.h>
#include <stdio.h>

#include "kii_core.h"
#include "kii_mqtt.h"
#include "kii.h"

int kiiMQTT_encode(char* buf, int length)
{
    int rc = 0;
    char d;

    do
    {
        d = length % 128;
        length /= 128;
        if(length > 0)
            d |= 0x80;
        buf[rc++] = d;
    }
    while(length > 0);
    return rc;
}

int kiiMQTT_decode(char* buf, int* value)
{
    int i = 0;
    int multiplier = 1;
    int len = 0;
    *value = 0;
    do
    {
        if(++len > 4)
        {
            return -1;
        }
        *value += (buf[i] & 127) * multiplier;
        multiplier *= 128;
    }
    while((buf[i++] & 128) != 0);

    return len;
}

int kiiMQTT_connect(kii_t* kii, kii_mqtt_endpoint_t* endpoint, unsigned short keepAliveInterval)
{
    char buf[256];
    int i;
    int j;
    int k;
    kii_socket_code_t sock_err;
    size_t actual_length;

    if (kii->mqtt_socket_context.socket > 0) {
        M_KII_LOG(kii->kii_core.logger_cb("closing socket as socket is already created.\r\n"));
        sock_err = kii->mqtt_socket_close_cb(&(kii->mqtt_socket_context));
        if (sock_err != KII_SOCKETC_OK) {
            return -1;
        }
    }
    sock_err = kii->mqtt_socket_connect_cb(&(kii->mqtt_socket_context), endpoint->host, endpoint->port_tcp);
    if (sock_err != KII_SOCKETC_OK) {
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    i = 8; /* reserver 8 bytes for header */
    /* Variable header:Protocol Name bytes */
    buf[i++] = 0x00;
    buf[i++] = 0x06;
    buf[i++] = 'M';
    buf[i++] = 'Q';
    buf[i++] = 'I';
    buf[i++] = 's';
    buf[i++] = 'd';
    buf[i++] = 'p';
    /* Variable header:Protocol Level */
    buf[i++] = 0x03;
    /* Variable header:Connect Flags */
    /*
     * Bit   7                          6                        5                    4  3            2            1 0
     *        User Name Flag     Password Flag    Will Retain        Will QoS     Will Flag   Clean Session   Reserved
     */
    buf[i++] = (char)0xc2;
    /* Variable header:Keep Alive */
    buf[i++] = (keepAliveInterval & 0xff00) >> 8;
    buf[i++] = keepAliveInterval & 0x00ff;
    /* Payload:Client Identifier */
    buf[i++] = (strlen(endpoint->topic) & 0xff00) >> 8;
    buf[i++] = strlen(endpoint->topic) & 0x00ff;
    strcpy(&buf[i], endpoint->topic);
    i += strlen(endpoint->topic);
    /* Payload:User Name */
    buf[i++] = (strlen(endpoint->username) & 0xff00) >> 8;
    buf[i++] = strlen(endpoint->username) & 0x00ff;
    strcpy(&buf[i], endpoint->username);
    i += strlen(endpoint->username);
    /* Payload:Password */
    buf[i++] = (strlen(endpoint->password) & 0xff00) >> 8;
    buf[i++] = strlen(endpoint->password) & 0x00ff;
    strcpy(&buf[i], endpoint->password);
    i += strlen(endpoint->password);

    j = 0;
    /* Fixed header:byte1 */
    buf[j++] = 0x10;
    /*/ Fixed header:Remaining Length*/
    j += kiiMQTT_encode(&buf[j], i - 8);

    /* copy the other tytes */
    for(k = 0; k < i - 8; k++)
    {
        buf[j++] = buf[8 + k];
    }

    M_KII_LOG((kii->kii_core.logger_cb("\r\n----------------MQTT connect send start-------------\r\n")));
    M_KII_LOG(kii->kii_core.logger_cb("j=%d\r\n", j));
    M_KII_LOG(kii->kii_core.logger_cb("\r\n"));
    for (i=0; i<j; i++)
    {
        M_KII_LOG(kii->kii_core.logger_cb("%02x", buf[i]));
    }
    M_KII_LOG(kii->kii_core.logger_cb("\r\n"));

    M_KII_LOG(kii->kii_core.logger_cb("\r\n----------------MQTT connect send end-------------\r\n"));

    sock_err = kii->mqtt_socket_send_cb(&(kii->mqtt_socket_context), buf, j);
    if (sock_err != KII_SOCKETC_OK) {
        M_KII_LOG(kii->kii_core.logger_cb("kii-error: send data fail\r\n"));
        return -1;
    }
    memset(buf, 0, sizeof(buf));
    sock_err = kii->mqtt_socket_recv_cb(&(kii->mqtt_socket_context), buf, sizeof(buf), &actual_length);
    if(sock_err != KII_SOCKETC_OK)
    {
        M_KII_LOG(kii->kii_core.logger_cb("kii-error: recv data fail\r\n"));
        return -1;
    }
    else
    {
        M_KII_LOG(kii->kii_core.logger_cb("\r\n----------------MQTT connect recv start-------------\r\n"));
        M_KII_LOG(kii->kii_core.logger_cb("\r\n"));
        for (i=0; i<actual_length; i++)
        {
            M_KII_LOG(kii->kii_core.logger_cb("%02x", buf[i]));
        }
        M_KII_LOG(kii->kii_core.logger_cb("\r\n"));

        M_KII_LOG(kii->kii_core.logger_cb("\r\n----------------MQTT_connect recv end-------------\r\n"));
        if((actual_length == 4) && (buf[0] == 0x20) && (buf[1] == 0x02) && (buf[2] == 0x00) && (buf[3] == 0x00))
        {
            return 0;
        }
        else
        {
            M_KII_LOG(kii->kii_core.logger_cb("kii-error: invalid data format\r\n"));
            return -1;
        }
    }
}

int kiiMQTT_subscribe(kii_t* kii, const char* topic, enum QoS qos)
{
    char buf[256];
    int i;
    int j;
    int k;
    size_t actual_length = 0;
    kii_socket_code_t sock_err;

    memset(buf, 0, sizeof(buf));
    i = 8;

    /* Variable header:Packet Identifier */
    buf[i++] = 0x00;
    buf[i++] = 0x01;
    /* Payload:topic length */
    buf[i++] = (strlen(topic) & 0xff00) >> 8;
    buf[i++] = strlen(topic) & 0x00ff;
    /* Payload:topic */
    strcpy(&buf[i], topic);
    i += strlen(topic);
    /* Payload: qos*/
    buf[i++] = (char)qos;

    j = 0;
    /* Fixed header: byte1*/
    buf[j++] = (char)0x82;
    /* Fixed header:Remaining Length*/
    j += kiiMQTT_encode(&buf[j], i - 8);

    /* copy the other tytes*/
    for(k = 0; k < i - 8; k++)
    {
        buf[j++] = buf[8 + k];
    }
    M_KII_LOG(kii->kii_core.logger_cb("\r\n----------------MQTT subscribe send start-------------\r\n"));
    M_KII_LOG(kii->kii_core.logger_cb("\r\n"));
    for (i=0; i<j; i++)
    {
        M_KII_LOG(kii->kii_core.logger_cb("%02x", buf[i]));
    }
    M_KII_LOG(kii->kii_core.logger_cb("\r\n"));

    M_KII_LOG(kii->kii_core.logger_cb("\r\n----------------MQTT subscribe send end-------------\r\n"));
    sock_err = kii->mqtt_socket_send_cb(&(kii->mqtt_socket_context), buf, j);
    if(sock_err != KII_SOCKETC_OK)
    {
        M_KII_LOG(kii->kii_core.logger_cb("kii-error: send data fail\r\n"));
        return -1;
    }
    memset(buf, 0, sizeof(buf));
    sock_err = kii->mqtt_socket_recv_cb(&(kii->mqtt_socket_context), buf, sizeof(buf), &actual_length);
    if(sock_err != KII_SOCKETC_OK)
    {
        M_KII_LOG(kii->kii_core.logger_cb("kii-error: recv data fail\r\n"));
        return -1;
    }
    else
    {
        M_KII_LOG(kii->kii_core.logger_cb("\r\n----------------MQTT subscribe recv start-------------\r\n"));
        M_KII_LOG(kii->kii_core.logger_cb("\r\n"));
        for (i=0; i<actual_length; i++)
        {
            M_KII_LOG(kii->kii_core.logger_cb("%02x", buf[i]));
        }
        M_KII_LOG(kii->kii_core.logger_cb("\r\n"));

        M_KII_LOG(kii->kii_core.logger_cb("\r\n----------------MQTT subscribe recv end-------------\r\n"));
        if((actual_length == 5) && ((unsigned char)buf[0] == 0x90) && (buf[1] == 0x03) && (buf[2] == 0x00) &&
                (buf[3] == 0x01))
        {
            return 0;
        }
        else
        {
            M_KII_LOG(kii->kii_core.logger_cb("kii-error: invalid data format\r\n"));
            return -1;
        }
    }
}

int kiiMQTT_pingReq(kii_t* kii)
{
    char buf[2];
    kii_socket_code_t sock_err;

    memset(buf, 0, sizeof(buf));
    buf[0] = (char)0xc0;
    buf[1] = 0x00;
    sock_err = kii->mqtt_socket_send_cb(&(kii->mqtt_socket_context), buf, sizeof(buf));
    if(sock_err != KII_SOCKETC_OK)
    {
        M_KII_LOG(kii->kii_core.logger_cb("kii-error: send data fail\r\n"));
        return -1;
    }
    else
    {
        return 0;
    }
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
