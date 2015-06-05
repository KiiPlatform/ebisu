#include <string.h>
#include "stdarg.h"

#include <LWiFi.h>
#include <LWiFiClient.h>
#include <LTask.h>

#include "kii_socket_impl.h"


static LWiFiClient m_core_client;
static LWiFiClient m_mqtt_client;

kii_socket_code_t
    connect_cb(kii_socket_context_t* socket_context, const char* host,
            unsigned int port)
{
    if (m_core_client.connected() == 1) {
        m_core_client.stop();
    }
    delay(500);
	if (m_core_client.connect(host, port) == 1)	{
        socket_context->socket = 1;
        return KII_SOCKETC_OK;
	} else {
        socket_context->socket = -1;
        return KII_SOCKETC_FAIL;
	}
}

kii_socket_code_t
    send_cb(kii_socket_context_t* socket_context,
            const char* buffer,
            size_t length)
{
    int ret;
	
    ret = m_core_client.write((uint8_t*)buffer, length);
    if (ret > 0) {
        return KII_SOCKETC_OK;
    } else {
        return KII_SOCKETC_FAIL;
    }
}

kii_socket_code_t
    recv_cb(kii_socket_context_t* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length)
{
	int ret;

	while (m_core_client.connected() ) {
        /* One byte will be lost after caling this available api if call "read((uint8_t*)buf, len)"	*/
	    if (m_core_client.available()) {
	        buffer[0] = m_core_client.read();   
            ret = m_core_client.read((uint8_t*)(buffer+1), length_to_read-1);
            *out_actual_length = ret + 1;
            return KII_SOCKETC_OK;
	    }
	}
    return KII_SOCKETC_FAIL;
}

kii_socket_code_t
    close_cb(kii_socket_context_t* socket_context)
{
    if (m_core_client.connected() == 1) {
        m_core_client.stop();
    }
    socket_context->socket = -1;
    return KII_SOCKETC_OK;
}

kii_socket_code_t
    mqtt_connect_cb(kii_socket_context_t* socket_context, const char* host,
            unsigned int port)
{
    if (m_mqtt_client.connected() == 1) {
        m_mqtt_client.stop();
    }
    delay(500);
	if (m_mqtt_client.connect(host, port) == 1)	{
        socket_context->socket = 1;
        return KII_SOCKETC_OK;
	} else {
        socket_context->socket = -1;
        return KII_SOCKETC_FAIL;
	}
}

kii_socket_code_t
    mqtt_send_cb(kii_socket_context_t* socket_context,
            const char* buffer,
            size_t length)
{
    int ret;
	
    ret = m_mqtt_client.write((uint8_t*)buffer, length);
    if (ret > 0) {
        return KII_SOCKETC_OK;
    } else {
        return KII_SOCKETC_FAIL;
    }
}

kii_socket_code_t
    mqtt_recv_cb(kii_socket_context_t* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length)
{
	int ret;

	while (m_mqtt_client.connected() ) {
        /* One byte will be lost after caling this available api if call "read((uint8_t*)buf, len)"	*/
	    if (m_mqtt_client.available()) {
	        buffer[0] = m_mqtt_client.read();   
            ret = m_mqtt_client.read((uint8_t*)(buffer+1), length_to_read-1);
            *out_actual_length = ret + 1;
            return KII_SOCKETC_OK;
	    }
	}
    return KII_SOCKETC_FAIL;
}

kii_socket_code_t
    mqtt_close_cb(kii_socket_context_t* socket_context)
{
    if (m_mqtt_client.connected() == 1) {
        m_mqtt_client.stop();
    }
    socket_context->socket = -1;
    return KII_SOCKETC_OK;
}

int mqtt_data_available(void)
{
	if (m_mqtt_client.available())
	{
	    return 0;
	}
	else
	{
	    return -1;
	}
}

int mqtt_connected(void)
{
	if (m_mqtt_client.connected())
	{
	    return 0;
	}
	else
	{
	    return -1;
	}
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
