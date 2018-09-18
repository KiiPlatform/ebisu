#include <string.h>
#include <stdio.h>

#include "kii_mqtt.h"
#include "kii.h"

#define KII_PUSH_TOPIC_HEADER_SIZE 8

int _mqtt_encode(char* buf, int length)
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

int _mqtt_decode(char* buf, int* value)
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

int _mqtt_connect(kii_t* kii, kii_mqtt_endpoint_t* endpoint, unsigned short keepAliveInterval)
{
    size_t i;
    size_t j;
    size_t k;
    khc_sock_code_t sock_err;
    size_t actual_length;
    unsigned int port;

    // TODO: Review this logic. This might be used to recover from stale connection.
    if (kii->_mqtt_connected == 1) {
        M_KII_LOG("closing socket as socket is already created.\r\n");
        sock_err = kii->mqtt_sock_close_cb(&(kii->mqtt_sock_close_ctx));
        if (sock_err != KHC_SOCK_OK) {
            M_KII_LOG("closing socket is failed.\r\n");
            return -1;
        }
        kii->_mqtt_connected = 0;
    }
#ifndef KII_MQTT_USE_PORT_TCP
    port = endpoint->port_ssl;
#else
    port = endpoint->port_tcp;
#endif
    sock_err = kii->mqtt_sock_connect_cb(&(kii->mqtt_sock_connect_ctx), endpoint->host, port);
    if (sock_err != KHC_SOCK_OK) {
        M_KII_LOG("connecting socket is failed.\r\n");
        return -1;
    }

    if (kii->mqtt_buffer == NULL || kii->mqtt_buffer_size == 0) {
        M_KII_LOG("mqtt_buffer must not be NULL.\r\n");
        return -1;
    }
    if (kii->mqtt_buffer_size < 27 + strlen(endpoint->topic) +
            strlen(endpoint->username) + strlen(endpoint->password)) {
        M_KII_LOG("mqtt_buffer is too short.\r\n");
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
    j += _mqtt_encode(&(kii->mqtt_buffer[j]), i - 8);

    /* copy the other tytes */
    for(k = 0; k < i - 8; k++)
    {
        kii->mqtt_buffer[j++] = kii->mqtt_buffer[8 + k];
    }

    sock_err = kii->mqtt_sock_send_cb(&(kii->mqtt_sock_send_ctx),
            kii->mqtt_buffer, j);
    if (sock_err != KHC_SOCK_OK) {
        M_KII_LOG("kii-error: send data fail\r\n");
        return -1;
    }
    memset(kii->mqtt_buffer, 0, kii->mqtt_buffer_size);
    sock_err = kii->mqtt_sock_recv_cb(&(kii->mqtt_sock_recv_ctx),
            kii->mqtt_buffer, kii->mqtt_buffer_size, &actual_length);
    if(sock_err != KHC_SOCK_OK)
    {
        M_KII_LOG("kii-error: recv data fail\r\n");
        return -1;
    }
    else
    {
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
            M_KII_LOG("kii-error: invalid data format\r\n");
            return -1;
        }
    }
}

int _mqtt_subscribe(kii_t* kii, const char* topic, enum QoS qos)
{
    size_t i;
    size_t j;
    size_t k;
    size_t actual_length = 0;
    khc_sock_code_t sock_err;

    if (kii->mqtt_buffer == NULL || kii->mqtt_buffer_size == 0) {
        M_KII_LOG("mqtt_buffer must not be NULL.\r\n");
        return -1;
    }
    if (kii->mqtt_buffer_size < 14 + strlen(topic)) {
        M_KII_LOG("mqtt_buffer is too short.\r\n");
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
    j += _mqtt_encode(&(kii->mqtt_buffer[j]), i - 8);

    /* copy the other tytes*/
    for(k = 0; k < i - 8; k++)
    {
        kii->mqtt_buffer[j++] = kii->mqtt_buffer[8 + k];
    }
    sock_err = kii->mqtt_sock_send_cb(&(kii->mqtt_sock_send_ctx),
            kii->mqtt_buffer, j);
    if(sock_err != KHC_SOCK_OK)
    {
        M_KII_LOG("kii-error: send data fail\r\n");
        return -1;
    }
    memset(kii->mqtt_buffer, 0, kii->mqtt_buffer_size);
    sock_err = kii->mqtt_sock_recv_cb(&(kii->mqtt_sock_recv_ctx),
            kii->mqtt_buffer, kii->mqtt_buffer_size, &actual_length);
    if(sock_err != KHC_SOCK_OK)
    {
        M_KII_LOG("kii-error: recv data fail\r\n");
        return -1;
    }
    else
    {
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
            M_KII_LOG("kii-error: invalid data format\r\n");
            return -1;
        }
    }
}

int _mqtt_pingreq(kii_t* kii)
{
    char buf[2];
    khc_sock_code_t sock_err;

    memset(buf, 0, sizeof(buf));
    buf[0] = (char)0xc0;
    buf[1] = 0x00;
    sock_err = kii->mqtt_sock_send_cb(&(kii->mqtt_sock_send_ctx), buf, sizeof(buf));
    if(sock_err != KHC_SOCK_OK)
    {
        M_KII_LOG("kii-error: send data fail\r\n");
        return -1;
    }
    else
    {
        return 0;
    }
}

int _mqtt_recvmsg(
        kii_t* kii,
        kii_mqtt_endpoint_t* endpoint)
{
    int remainingLen;
    int byteLen;
    int topicLen;
    size_t totalLen;
    char* p;
    size_t bytes = 0;
    size_t rcvdCounter = 0;
    KII_PUSH_RECEIVED_CB callback;

    callback = kii->push_received_cb;
    memset(kii->mqtt_buffer, 0, kii->mqtt_buffer_size);

    rcvdCounter = 0;
    kii->mqtt_sock_recv_cb(&kii->mqtt_sock_recv_ctx, kii->mqtt_buffer, 2, &rcvdCounter);
    if(rcvdCounter == 2)
    {
        if((kii->mqtt_buffer[0] & 0xf0) == 0x30)
        {
            rcvdCounter = 0;
            kii->mqtt_sock_recv_cb(&kii->mqtt_sock_recv_ctx, kii->mqtt_buffer+2, KII_PUSH_TOPIC_HEADER_SIZE, &rcvdCounter);
            if(rcvdCounter == KII_PUSH_TOPIC_HEADER_SIZE)
            {
                byteLen = _mqtt_decode(&kii->mqtt_buffer[1], &remainingLen);
            }
            else
            {
                M_KII_LOG("kii-error: mqtt decode error\r\n");
                return -1;
            }
            if(byteLen > 0)
            {
                totalLen =
                  remainingLen + byteLen + 1; /* fixed head byte1+remaining length bytes + remaining bytes*/
            }
            else
            {
                M_KII_LOG("kii-error: mqtt decode error\r\n");
                return -1;
            }
            if(totalLen > kii->mqtt_buffer_size)
            {
                M_KII_LOG("kii-error: mqtt buffer overflow\r\n");
                return -1;
            }
            bytes = rcvdCounter + 2;
            while(bytes < totalLen)
            {
                rcvdCounter = 0;
                kii->mqtt_sock_recv_cb(&(kii->mqtt_sock_recv_ctx), kii->mqtt_buffer + bytes, totalLen - bytes, &rcvdCounter);
                if(rcvdCounter > 0)
                {
                    bytes += rcvdCounter;
                    M_KII_LOG("success read. totalLen: %u, bytes: %u\r\n", (unsigned int)totalLen, (unsigned int)bytes);
                }
                else
                {
                    bytes = -1;
                    M_KII_LOG("failed to read. totalLen: %u, bytes: %d\r\n", (unsigned int)totalLen, (unsigned int)bytes);
                    break;
                }
            }
            if(bytes >= totalLen)
            {
                p = kii->mqtt_buffer;
                p++; /* skip fixed header byte1*/
                p += byteLen; /* skip remaining length bytes*/
                topicLen = p[0] * 256 + p[1]; /* get topic length*/
                p = p + 2; /* skip 2 topic length bytes*/
                p = p + topicLen; /* skip topic*/
                if((remainingLen - 2 - topicLen) > 0)
                {
                    M_KII_LOG("Successfully Recieved Push %s\n", p);
                    callback(kii, p, remainingLen - 2 - topicLen);
                }
                else
                {
                    M_KII_LOG("kii-error: mqtt topic length error\r\n");
                    return -1;
                }
            }
            else
            {
                M_KII_LOG("kii_error: mqtt receive data error\r\n");
                return -1;
            }
        }
        else if((kii->mqtt_buffer[0] & 0xf0) == 0xd0)
        {
            M_KII_LOG("ping resp\r\n");
        }
    }
    else
    {
        M_KII_LOG("kii-error: mqtt receive data error\r\n");
        return -1;
    }
    return 0;
}

void* _mqtt_start_recvmsg_task(void* sdata)
{
    kii_t* kii;
    kii_mqtt_endpoint_t endpoint;
    kii_mqtt_state pushState = KII_MQTT_PREPARING_ENDPOINT;

    memset(&endpoint, 0x00, sizeof(kii_mqtt_endpoint_t));

    kii = (kii_t*) sdata;
    for(;;)
    {
        switch(pushState)
        {
            case KII_MQTT_PREPARING_ENDPOINT:
                {
                    kii_installation_id_t ins_id;

                    if(kii_install_push(kii, KII_FALSE, &ins_id) != KII_ERR_OK)
                    {
                        M_KII_LOG("kii-error: mqtt installation error\r\n");
                        kii->delay_ms_cb(1000);
                        continue;
                    }

                    kii_code_t get_ep_res = KII_ERR_FAIL;
                    int retry = 0;
                    do
                    {
                        kii->delay_ms_cb(1000);
                        get_ep_res = kii_get_mqtt_endpoint(kii, ins_id.id,
                                &endpoint);
                        if (get_ep_res == KII_ERR_OK) {
                            retry = 0;
                        }
                        int status_code = khc_get_status_code(&kii->_khc);
                        if (500 <= status_code && status_code < 600) {
                            retry = 1;
                        }
                    }
                    while(retry);

                    if(get_ep_res != KII_ERR_OK)
                    {
                        M_KII_LOG("kii-error: mqtt retrive error\r\n");
                        kii->delay_ms_cb(1000);
                        continue;
                    }

                    pushState = KII_MQTT_SUBSCRIBING_TOPIC;
                }
                break;
            case KII_MQTT_SUBSCRIBING_TOPIC:
                if (_mqtt_connect(kii, &endpoint,
                                kii->_keep_alive_interval) != 0)
                {
                    M_KII_LOG("kii-error: mqtt connect error\r\n");
                    pushState = KII_MQTT_PREPARING_ENDPOINT;
                    continue;
                }
                kii->_mqtt_connected = 1;

                if(_mqtt_subscribe(kii, endpoint.topic, QOS0) < 0)
                {
                    M_KII_LOG("kii-error: mqtt subscribe error\r\n");
                    kii->_mqtt_connected = 0;
                    pushState = KII_MQTT_PREPARING_ENDPOINT;
                    continue;
                }

                pushState = KII_MQTT_READY;
                break;
            case KII_MQTT_READY:
                if(_mqtt_recvmsg(kii, &endpoint) != 0)
                {
                    /* Receiving notificaiton is failed. Retry subscribing. */
                    kii->_mqtt_connected = 0;
                    pushState = KII_MQTT_SUBSCRIBING_TOPIC;
                }
                break;
        }
    }
    return NULL;
}

void* _mqtt_start_pinreq_task(void* sdata)
{
    kii_t* kii;

    kii = (kii_t*)sdata;
    for(;;)
    {
        if(kii->_mqtt_connected == 1)
        {
            _mqtt_pingreq(kii);
        }
        kii->delay_ms_cb(kii->_keep_alive_interval * 1000);
    }
    return NULL;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
