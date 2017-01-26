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
    size_t i;
    size_t j;
    size_t k;
    kii_socket_code_t sock_err;
    size_t actual_length;
    unsigned int port;

    if (kii->mqtt_socket_context.socket > 0) {
        M_KII_LOG(kii->kii_core.logger_cb("closing socket as socket is already created.\r\n"));
        sock_err = kii->mqtt_socket_close_cb(&(kii->mqtt_socket_context));
        if (sock_err != KII_SOCKETC_OK) {
            M_KII_LOG(kii->kii_core.logger_cb("closing socket is failed.\r\n"));
            return -1;
        }
    }
#ifndef KII_MQTT_USE_PORT_TCP
    port = endpoint->port_ssl;
#else
    port = endpoint->port_tcp;
#endif
    sock_err = kii->mqtt_socket_connect_cb(&(kii->mqtt_socket_context), endpoint->host, port);
    if (sock_err != KII_SOCKETC_OK) {
        M_KII_LOG(kii->kii_core.logger_cb("connecting socket is failed.\r\n"));
        return -1;
    }

    if (kii->mqtt_buffer == NULL || kii->mqtt_buffer_size == 0) {
        M_KII_LOG(kii->kii_core.logger_cb("mqtt_buffer must not be NULL.\r\n"));
        return -1;
    }
    if (kii->mqtt_buffer_size < 27 + strlen(endpoint->topic) +
            strlen(endpoint->username) + strlen(endpoint->password)) {
        M_KII_LOG(kii->kii_core.logger_cb("mqtt_buffer is too short.\r\n"));
        return -1;
    }
    memset(kii->mqtt_buffer, 0, kii->mqtt_buffer_size);
    i = 8; /* reserver 8 bytes for header */
    /* Variable header:Protocol Name bytes */
    kii->mqtt_buffer[i++] = 0x00;
    kii->mqtt_buffer[i++] = 0x06;
    kii->mqtt_buffer[i++] = 'M';
    kii->mqtt_buffer[i++] = 'Q';
    kii->mqtt_buffer[i++] = 'I';
    kii->mqtt_buffer[i++] = 's';
    kii->mqtt_buffer[i++] = 'd';
    kii->mqtt_buffer[i++] = 'p';
    /* Variable header:Protocol Level */
    kii->mqtt_buffer[i++] = 0x03;
    /* Variable header:Connect Flags */
    /*
     * Bit   7                          6                        5                    4  3            2            1 0
     *        User Name Flag     Password Flag    Will Retain        Will QoS     Will Flag   Clean Session   Reserved
     */
    kii->mqtt_buffer[i++] = (char)0xc2;
    /* Variable header:Keep Alive */
    kii->mqtt_buffer[i++] = (keepAliveInterval & 0xff00) >> 8;
    kii->mqtt_buffer[i++] = keepAliveInterval & 0x00ff;
    /* Payload:Client Identifier */
    kii->mqtt_buffer[i++] = (strlen(endpoint->topic) & 0xff00) >> 8;
    kii->mqtt_buffer[i++] = strlen(endpoint->topic) & 0x00ff;
    strcpy(&(kii->mqtt_buffer[i]), endpoint->topic);
    i += strlen(endpoint->topic);
    /* Payload:User Name */
    kii->mqtt_buffer[i++] = (strlen(endpoint->username) & 0xff00) >> 8;
    kii->mqtt_buffer[i++] = strlen(endpoint->username) & 0x00ff;
    strcpy(&(kii->mqtt_buffer[i]), endpoint->username);
    i += strlen(endpoint->username);
    /* Payload:Password */
    kii->mqtt_buffer[i++] = (strlen(endpoint->password) & 0xff00) >> 8;
    kii->mqtt_buffer[i++] = strlen(endpoint->password) & 0x00ff;
    strcpy(&(kii->mqtt_buffer[i]), endpoint->password);
    i += strlen(endpoint->password);

    j = 0;
    /* Fixed header:byte1 */
    kii->mqtt_buffer[j++] = 0x10;
    /*/ Fixed header:Remaining Length*/
    j += kiiMQTT_encode(&(kii->mqtt_buffer[j]), i - 8);

    /* copy the other tytes */
    for(k = 0; k < i - 8; k++)
    {
        kii->mqtt_buffer[j++] = kii->mqtt_buffer[8 + k];
    }

    M_KII_LOG((kii->kii_core.logger_cb("\r\n----------------MQTT connect send start-------------\r\n")));
    M_KII_LOG(kii->kii_core.logger_cb("j=%d\r\n", j));
    M_KII_LOG(kii->kii_core.logger_cb("\r\n"));
    for (i=0; i<j; i++)
    {
        M_KII_LOG(kii->kii_core.logger_cb("%02x\r\n", kii->mqtt_buffer[i]));
    }
    M_KII_LOG(kii->kii_core.logger_cb("\r\n"));

    M_KII_LOG(kii->kii_core.logger_cb("\r\n----------------MQTT connect send end-------------\r\n"));

    sock_err = kii->mqtt_socket_send_cb(&(kii->mqtt_socket_context),
            kii->mqtt_buffer, j);
    if (sock_err != KII_SOCKETC_OK) {
        M_KII_LOG(kii->kii_core.logger_cb("kii-error: send data fail\r\n"));
        return -1;
    }
    memset(kii->mqtt_buffer, 0, kii->mqtt_buffer_size);
    sock_err = kii->mqtt_socket_recv_cb(&(kii->mqtt_socket_context),
            kii->mqtt_buffer, kii->mqtt_buffer_size, &actual_length);
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
            M_KII_LOG(kii->kii_core.logger_cb("%02x", kii->mqtt_buffer[i]));
        }
        M_KII_LOG(kii->kii_core.logger_cb("\r\n"));

        M_KII_LOG(kii->kii_core.logger_cb("\r\n----------------MQTT_connect recv end-------------\r\n"));
        if((actual_length == 4) &&
                (kii->mqtt_buffer[0] == 0x20) &&
                (kii->mqtt_buffer[1] == 0x02) &&
                (kii->mqtt_buffer[2] == 0x00) &&
                (kii->mqtt_buffer[3] == 0x00))
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
    size_t i;
    size_t j;
    size_t k;
    size_t actual_length = 0;
    kii_socket_code_t sock_err;

    if (kii->mqtt_buffer == NULL || kii->mqtt_buffer_size == 0) {
        M_KII_LOG(kii->kii_core.logger_cb("mqtt_buffer must not be NULL.\r\n"));
        return -1;
    }
    if (kii->mqtt_buffer_size < 14 + strlen(topic)) {
        M_KII_LOG(kii->kii_core.logger_cb("mqtt_buffer is too short.\r\n"));
        return -1;
    }
    memset(kii->mqtt_buffer, 0, kii->mqtt_buffer_size);
    i = 8;

    /* Variable header:Packet Identifier */
    kii->mqtt_buffer[i++] = 0x00;
    kii->mqtt_buffer[i++] = 0x01;
    /* Payload:topic length */
    kii->mqtt_buffer[i++] = (strlen(topic) & 0xff00) >> 8;
    kii->mqtt_buffer[i++] = strlen(topic) & 0x00ff;
    /* Payload:topic */
    strcpy(&kii->mqtt_buffer[i], topic);
    i += strlen(topic);
    /* Payload: qos*/
    kii->mqtt_buffer[i++] = (char)qos;

    j = 0;
    /* Fixed header: byte1*/
    kii->mqtt_buffer[j++] = (char)0x82;
    /* Fixed header:Remaining Length*/
    j += kiiMQTT_encode(&(kii->mqtt_buffer[j]), i - 8);

    /* copy the other tytes*/
    for(k = 0; k < i - 8; k++)
    {
        kii->mqtt_buffer[j++] = kii->mqtt_buffer[8 + k];
    }
    M_KII_LOG(kii->kii_core.logger_cb("\r\n----------------MQTT subscribe send start-------------\r\n"));
    M_KII_LOG(kii->kii_core.logger_cb("\r\n"));
    for (i=0; i<j; i++)
    {
        M_KII_LOG(kii->kii_core.logger_cb("%02x\r\n", kii->mqtt_buffer[i]));
    }
    M_KII_LOG(kii->kii_core.logger_cb("\r\n"));

    M_KII_LOG(kii->kii_core.logger_cb("\r\n----------------MQTT subscribe send end-------------\r\n"));
    sock_err = kii->mqtt_socket_send_cb(&(kii->mqtt_socket_context),
            kii->mqtt_buffer, j);
    if(sock_err != KII_SOCKETC_OK)
    {
        M_KII_LOG(kii->kii_core.logger_cb("kii-error: send data fail\r\n"));
        return -1;
    }
    memset(kii->mqtt_buffer, 0, kii->mqtt_buffer_size);
    sock_err = kii->mqtt_socket_recv_cb(&(kii->mqtt_socket_context),
            kii->mqtt_buffer, kii->mqtt_buffer_size, &actual_length);
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
            M_KII_LOG(kii->kii_core.logger_cb("%02x\r\n", kii->mqtt_buffer[i]));
        }
        M_KII_LOG(kii->kii_core.logger_cb("\r\n"));

        M_KII_LOG(kii->kii_core.logger_cb("\r\n----------------MQTT subscribe recv end-------------\r\n"));
        if((actual_length == 5) &&
                ((unsigned char)kii->mqtt_buffer[0] == 0x90) &&
                (kii->mqtt_buffer[1] == 0x03) &&
                (kii->mqtt_buffer[2] == 0x00) &&
                (kii->mqtt_buffer[3] == 0x01))
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
