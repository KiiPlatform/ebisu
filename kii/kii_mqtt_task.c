#include <string.h>
#include <stdio.h>
#include <time.h>

#include "kii_mqtt_task.h"
#include "kii.h"

int _mqtt_encode(char* buf, unsigned long remaining_length)
{
    int rc = 0;
    char d;

    do
    {
        d = remaining_length % 128;
        remaining_length /= 128;
        if(remaining_length > 0)
            d |= 0x80;
        buf[rc++] = d;
    }
    while(remaining_length > 0 && rc < 5);
    return rc;
}

int _mqtt_decode(char* buf, unsigned long* remaining_length)
{
    int i = 0;
    int multiplier = 1;
    *remaining_length = 0;
    do
    {
        if(i > 3)
        {
            return -1;
        }
        *remaining_length += (buf[i] & 127) * multiplier;
        multiplier *= 128;
    }
    while((buf[i++] & 128) != 0);

    return i;
}

khc_sock_code_t _mqtt_send_connect(kii_t* kii, kii_mqtt_endpoint_t* endpoint) {
    unsigned int keep_alive_interval = kii->_keep_alive_interval;
    memset(kii->mqtt_buffer, 0, kii->mqtt_buffer_size);
    int i = 8; /* reserver 8 bytes for header */
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
    kii->mqtt_buffer[i++] = (keep_alive_interval & 0xff00) >> 8;
    kii->mqtt_buffer[i++] = keep_alive_interval & 0x00ff;
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

    int j = 0;
    /* Fixed header:byte1 */
    kii->mqtt_buffer[j++] = 0x10;
    /*/ Fixed header:Remaining Length*/
    j += _mqtt_encode(&(kii->mqtt_buffer[j]), i - 8);

    /* copy the other types */
    for(int k = 0; k < i - 8; k++)
    {
        kii->mqtt_buffer[j++] = kii->mqtt_buffer[8 + k];
    }

    khc_sock_code_t send_err = KHC_SOCK_FAIL;
    size_t total_sent = 0;
    while (total_sent < j) {
        size_t sent_len = 0;
        send_err = kii->mqtt_sock_send_cb(kii->mqtt_sock_send_ctx,
                &kii->mqtt_buffer[total_sent], j - total_sent, &sent_len);
        if (send_err == KHC_SOCK_AGAIN || send_err == KHC_SOCK_FAIL) {
            // Don't accept non-blocking socket when send_err is KHC_SOCK_AGAIN.
            return KHC_SOCK_FAIL;
        }
        total_sent += sent_len;
    }
    return send_err;
}

khc_sock_code_t _mqtt_send_subscribe(kii_t* kii, const char* topic, kii_mqtt_qos qos)
{
    memset(kii->mqtt_buffer, 0, kii->mqtt_buffer_size);
    int i = 8;

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

    int j = 0;
    /* Fixed header: byte1*/
    kii->mqtt_buffer[j++] = (char)0x82;
    /* Fixed header:Remaining Length*/
    j += _mqtt_encode(&(kii->mqtt_buffer[j]), i - 8);

    /* copy the other types*/
    for(int k = 0; k < i - 8; k++)
    {
        kii->mqtt_buffer[j++] = kii->mqtt_buffer[8 + k];
    }

    khc_sock_code_t send_err = KHC_SOCK_FAIL;
    size_t total_sent = 0;
    while (total_sent < j) {
        size_t sent_len = 0;
        send_err = kii->mqtt_sock_send_cb(kii->mqtt_sock_send_ctx,
                &kii->mqtt_buffer[total_sent], j - total_sent, &sent_len);
        if (send_err == KHC_SOCK_AGAIN || send_err == KHC_SOCK_FAIL) {
            // Don't accept non-blocking socket when send_err is KHC_SOCK_AGAIN.
            return KHC_SOCK_FAIL;
        }
        total_sent += sent_len;
    }
    return send_err;
}

khc_sock_code_t _mqtt_send_pingreq(kii_t* kii)
{
    char buff[2];
    buff[0] = (char)0xc0;
    buff[1] = 0x00;
    khc_sock_code_t sock_err = KHC_SOCK_FAIL;
    size_t total_sent = 0;
    while (total_sent < sizeof(buff)) {
        size_t sent_len = 0;
        sock_err = kii->mqtt_sock_send_cb(kii->mqtt_sock_send_ctx, &buff[total_sent], sizeof(buff) - total_sent, &sent_len);
        if (sock_err == KHC_SOCK_AGAIN || sock_err == KHC_SOCK_FAIL) {
            // Don't accept non-blocking socket when send_err is KHC_SOCK_AGAIN.
            return KHC_SOCK_FAIL;
        }
        total_sent += sent_len;
    }
    return sock_err;
}

khc_sock_code_t _mqtt_recv_fixed_header(kii_t* kii, kii_mqtt_fixed_header* fixed_header)
{
    char c = '\0';
    size_t received = 0;
    // Read First byte.
    khc_sock_code_t res = kii->mqtt_sock_recv_cb(kii->mqtt_sock_recv_ctx, &c, 1, &received);
    if (res == KHC_SOCK_FAIL) {
        return KHC_SOCK_FAIL;
    }
    else if (res == KHC_SOCK_AGAIN) {
        // Don't accept non-blocking mode.
        return KHC_SOCK_FAIL;
    }
    fixed_header->byte1 = c;
    
    char buff[4];
    for (int i = 0; i < 4; ++i) {
        char c2 = '\0';
        khc_sock_code_t res = kii->mqtt_sock_recv_cb(kii->mqtt_sock_recv_ctx, &c2, 1, &received);
        if (res == KHC_SOCK_FAIL) {
            return KHC_SOCK_FAIL;
        }
        else if (res == KHC_SOCK_AGAIN) {
            // Don't accept non-blocking mode.
            return KHC_SOCK_FAIL;
        }
        buff[i] = c2;
        if ((c2 & 128) == 0) { // Check continue bit.
            break;
        }
    }

    unsigned long remaining_length = 0;
    _mqtt_decode(buff, &remaining_length);

    fixed_header->remaining_length = remaining_length;
    return KHC_SOCK_OK;
}

// If the MQTT buffer size is insufficient,
// read and trash the message so that the loop can wait for next message.
khc_sock_code_t _mqtt_recv_remaining_trash(kii_t* kii, unsigned long remaining_length) {
    const size_t buff_size = 256;
    char buff[buff_size];
    size_t received = 0;
    size_t total_received = 0;
    khc_sock_code_t res = KHC_SOCK_FAIL;
    while (total_received < remaining_length) {
        khc_sock_code_t res = kii->mqtt_sock_recv_cb(
            kii->mqtt_sock_recv_ctx,
            buff,
            buff_size,
            &received);
        if (res == KHC_SOCK_FAIL) {
            return KHC_SOCK_FAIL;
        }
        else if (res == KHC_SOCK_AGAIN) {
            // Don't accept non-blocking mode.
            return KHC_SOCK_FAIL;
        }
        else if (res == KHC_SOCK_OK) {
            total_received += received;
        }
    }
    return res;
}

khc_sock_code_t _mqtt_recv_remaining(kii_t* kii, unsigned long remaining_length, char* buff)
{
    size_t received = 0;
    size_t total_received = 0;
    khc_sock_code_t res = KHC_SOCK_FAIL;
    while (total_received < remaining_length) {
        res = kii->mqtt_sock_recv_cb(
            kii->mqtt_sock_recv_ctx,
            buff + received,
            remaining_length - received,
            &received);
        if (res == KHC_SOCK_FAIL) {
            return KHC_SOCK_FAIL;
        }
        else if (res == KHC_SOCK_AGAIN) {
            // Don't accept non-blocking mode.
            return KHC_SOCK_FAIL;
        }
        else if (res == KHC_SOCK_OK) {
            total_received += received;
        }
    }
    return res;
}

void* mqtt_start_task(void* sdata)
{
    kii_t* kii = (kii_t*)sdata;
    kii_mqtt_task_state st = KII_MQTT_ST_INSTALL_PUSH;
    const unsigned int wait_ms = 1000;
    kii_installation_id_t ins_id;
    kii_mqtt_endpoint_t endpoint;
    memset(&endpoint, 0x00, sizeof(kii_mqtt_endpoint_t));
    const char* err_msg = NULL;
    unsigned int keep_alive_interval = kii->_keep_alive_interval;
    unsigned int elapsed_time_ms = 0;
    const unsigned int arrived_msg_read_time = 500;
    const unsigned int msg_send_time = 500;
    unsigned long remaining_message_size = 0;
    time_t started;
    time(&started);
    while (st != KII_MQTT_ST_ERR_EXIT) {
        printf("Loop state: %d\n", st);
        switch(st) {
            case KII_MQTT_ST_INSTALL_PUSH: {
                kii_code_t res = kii_install_push(kii, KII_FALSE, &ins_id);
                if (res != KII_ERR_OK) {
                    int status_code = khc_get_status_code(&kii->_khc);
                    if ((500 <= status_code && status_code < 600) || status_code == 429) {
                        // Temporal error. Try again.
                        kii->delay_ms_cb(wait_ms);
                        break;
                    } else {
                        // Permanent error. Exit loop.
                        err_msg = "Failed to install push";
                        st = KII_MQTT_ST_ERR_EXIT;
                        break;
                    }
                }
                st = KII_MQTT_ST_GET_ENDPOINT;
                break;
            }
            case KII_MQTT_ST_GET_ENDPOINT: {
                kii->delay_ms_cb(wait_ms);
                kii_code_t  res = kii_get_mqtt_endpoint(kii, ins_id.id, &endpoint);
                if (res != KII_ERR_OK) {
                    int status_code = khc_get_status_code(&kii->_khc);
                    if ((500 <= status_code && status_code < 600) || status_code == 429) {
                        // Temporal error. Try again.
                        break;
                    } else {
                        err_msg = "Failed to get Endpoint";
                        st = KII_MQTT_ST_ERR_EXIT;
                        break;
                    }
                } else {
                    st = KII_MQTT_ST_SOCK_CONNECT;
                    break;
                }
            }
            case KII_MQTT_ST_SOCK_CONNECT: {
                unsigned int port;
#ifndef KII_MQTT_USE_PORT_TCP
                port = endpoint.port_ssl;
#else
                port = endpoint.port_tcp;
#endif
                khc_sock_code_t con_res = 
                    kii->mqtt_sock_connect_cb(
                        kii->mqtt_sock_connect_ctx,
                        endpoint.host, port);
                if (con_res != KHC_SOCK_OK) {
                    // TODO: Introduce retry count.
                    // Transit to KII_MQTT_ST_GET_ENDPOINT if retroy count > max retry count
                    kii->delay_ms_cb(wait_ms);
                    break;
                } else {
                    st = KII_MQTT_ST_SEND_CONNECT;
                    break;
                }
            }
            case KII_MQTT_ST_SEND_CONNECT: {
                if (kii->mqtt_buffer == NULL || kii->mqtt_buffer_size == 0) {
                    // TODO: check before staring routine.
                    err_msg = "MQTT buffer is not set";
                    st = KII_MQTT_ST_ERR_EXIT;
                    break;
                }
                if (kii->mqtt_buffer_size < 27 + strlen(endpoint.topic) +
                        strlen(endpoint.username) + strlen(endpoint.password)) {
                    // TODO: check before staring routine.
                    err_msg = "Too small MQTT buffer size";
                    st = KII_MQTT_ST_ERR_EXIT;
                    break;
                }
                khc_sock_code_t send_err = _mqtt_send_connect(kii, &endpoint);
                if (send_err == KHC_SOCK_FAIL) {
                    kii->delay_ms_cb(wait_ms);
                    st = KII_MQTT_ST_RECONNECT;
                    break;
                }
                st = KII_MQTT_ST_RECV_CONNACK;
                break;
            }
            case KII_MQTT_ST_RECV_CONNACK: {
                kii_mqtt_fixed_header fixed_header;
                khc_sock_code_t res = _mqtt_recv_fixed_header(kii, &fixed_header);
                if (res != KII_ERR_OK) {
                    kii->delay_ms_cb(wait_ms);
                    st = KII_MQTT_ST_RECONNECT;
                    break;
                }
                char ptype = fixed_header.byte1 & 0xf0;
                if (ptype != 0x20) {
                    // Must not happens. CONNACK must be the first response.
                    kii->delay_ms_cb(wait_ms);
                    st = KII_MQTT_ST_RECONNECT;
                    break;
                }
                // CONNACK variable header LENGTH is always 2.
                const char length = 2;
                char buff[length];
                memset(buff, '\0', length);
                khc_sock_code_t res2 = _mqtt_recv_remaining(kii, length, buff);
                if (res2 != KII_ERR_OK) {
                    kii->delay_ms_cb(wait_ms);
                    st = KII_MQTT_ST_RECONNECT;
                    break;
                }
                char return_code = buff[1];
                if (return_code != 0) {
                    // CONNACK indicates failure.
                    kii->delay_ms_cb(wait_ms);
                    st = KII_MQTT_ST_RECONNECT;
                    break;
                }
                st = KII_MQTT_ST_SEND_SUBSCRIBE;
                break;
            }
            case KII_MQTT_ST_SEND_SUBSCRIBE: {
                // TODO: enable to configure QoS.
                khc_sock_code_t res = _mqtt_send_subscribe(kii, endpoint.topic, QOS0);
                if (res == KHC_SOCK_FAIL) {
                    kii->delay_ms_cb(wait_ms);
                    st = KII_MQTT_ST_RECONNECT;
                    break;
                } else { // KHC_ERR_OK.
                    st = KII_MQTT_ST_RECV_SUBACK;
                    break;
                }
            }
            case KII_MQTT_ST_RECV_SUBACK: {
                kii_mqtt_fixed_header fixed_header;
                khc_sock_code_t res = _mqtt_recv_fixed_header(kii, &fixed_header);
                if (res != KII_ERR_OK) {
                    kii->delay_ms_cb(wait_ms);
                    st = KII_MQTT_ST_RECONNECT;
                    break;
                }
                unsigned char ptype = fixed_header.byte1 & 0xf0;
                if (ptype != 0x90) {
                    kii->delay_ms_cb(wait_ms);
                    st = KII_MQTT_ST_RECONNECT;
                    break;
                }
                const char len = 3;
                char buff[len];
                memset(buff, '\0', len);
                khc_sock_code_t res2 = _mqtt_recv_remaining(kii, len, buff);
                if (res2 != KII_ERR_OK) {
                    kii->delay_ms_cb(wait_ms);
                    st = KII_MQTT_ST_RECONNECT;
                    break;
                }
                if (buff[0] == 0x00 && // Identifier MSB
                    buff[1] == 0x01 && // Identifier LSB
                    (unsigned char)buff[2] != 0x80) // Return Code
                {
                    st = KII_MQTT_ST_RECV_READY;
                    break;
                } else {
                    kii->delay_ms_cb(wait_ms);
                    st = KII_MQTT_ST_RECONNECT;
                    break;
                }
            }
            case KII_MQTT_ST_RECV_READY: {
                const int thresh_ms = (keep_alive_interval - kii->_mqtt_to_recv_sec) * 1000;
                if (elapsed_time_ms > thresh_ms) {
                    st = KII_MQTT_ST_SEND_PINGREQ;
                    break;
                }
                kii_mqtt_fixed_header fh;
                khc_sock_code_t res = _mqtt_recv_fixed_header(kii, &fh);
                if (res == KHC_SOCK_OK) {
                    unsigned char mtype = fh.byte1 & 0xf0;
                    if (mtype == 0x30) { // PUBLISH
                        remaining_message_size = fh.remaining_length;
                        st = KII_MQTT_ST_RECV_MSG;
                        // Estimate worst case.
                        elapsed_time_ms += kii->_mqtt_to_recv_sec * 1000;
                        break;
                    } else if (mtype == 0xD0) { // PINGRESP
                        elapsed_time_ms += arrived_msg_read_time;
                        break;
                    } else { // Ignore other messages.
                        unsigned long size = fh.remaining_length;
                        // Read and ignore.
                        res = _mqtt_recv_remaining_trash(kii, size);
                        if (res != KHC_SOCK_OK) {
                            elapsed_time_ms = 0;
                            kii->delay_ms_cb(wait_ms);
                            st = KII_MQTT_ST_RECONNECT;
                        }
                        break;
                    }
                } else {
                    // Just repeat same state after the wait.
                    elapsed_time_ms += kii->_mqtt_to_recv_sec * 1000 + wait_ms;
                    kii->delay_ms_cb(wait_ms);
                    break;
                }
            }
            case KII_MQTT_ST_RECV_MSG: {
                if (remaining_message_size > kii->mqtt_buffer_size) {
                    // Ignore message.
                    khc_sock_code_t res = _mqtt_recv_remaining_trash(kii, remaining_message_size);
                    if (res != KHC_SOCK_OK) {
                        elapsed_time_ms = 0;
                        kii->delay_ms_cb(wait_ms);
                        st = KII_MQTT_ST_RECONNECT;
                    } else {
                        elapsed_time_ms += arrived_msg_read_time + wait_ms;
                        kii->delay_ms_cb(wait_ms);
                        st = KII_MQTT_ST_RECV_READY;
                    }
                    break;
                } else {
                    memset(kii->mqtt_buffer, '\0', kii->mqtt_buffer_size);
                    khc_sock_code_t res = _mqtt_recv_remaining(kii, remaining_message_size, kii->mqtt_buffer);
                    if (res != KHC_SOCK_OK) {
                        elapsed_time_ms = 0;
                        kii->delay_ms_cb(wait_ms);
                        st = KII_MQTT_ST_RECONNECT;
                    } else {
                        elapsed_time_ms += arrived_msg_read_time;
                        unsigned int topic_size = (unsigned int)kii->mqtt_buffer[0] * 256 + (unsigned int)kii->mqtt_buffer[1];
                        // 2 bytes: topic size. packet identifier is not present since QoS0
                        char* body_ptr = kii->mqtt_buffer + topic_size + 2;
                        size_t body_length = remaining_message_size - topic_size - 2;
                        kii->push_received_cb(body_ptr, body_length, kii->_push_data);
                        st = KII_MQTT_ST_RECV_READY;
                    }
                    break;
                }
            }
            case KII_MQTT_ST_SEND_PINGREQ: {
                time_t ping_requested;
                time(&ping_requested);
                time_t diff = ping_requested - started;
                printf("Send Pingreq after %ld sec.\n", (long)diff);
                time(&started);
                khc_sock_code_t res = _mqtt_send_pingreq(kii);
                if (res != KHC_SOCK_OK) {
                    elapsed_time_ms = 0;
                    kii->delay_ms_cb(wait_ms);
                    st = KII_MQTT_ST_RECONNECT;
                    break;
                }
                elapsed_time_ms = msg_send_time;
                st = KII_MQTT_ST_RECV_READY;
                break;
            }
            case KII_MQTT_ST_RECONNECT: {
                kii->mqtt_sock_close_cb(kii->mqtt_sock_close_ctx);
                st = KII_MQTT_ST_SOCK_CONNECT;
                break;
            }
            default:
                st = KII_MQTT_ST_ERR_EXIT;
                break;
        }
    }
    return NULL;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
