#include <string.h>
#include <stdio.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_push.h"
#include "kii_mqtt.h"
#include "kii-core/kii.h"


#define KIIPUSH_TASK_STK_SIZE 8
static unsigned int mKiiPush_taskStk[KIIPUSH_TASK_STK_SIZE];
#if(KII_PUSH_PING_ENABLE)
#define KIIPUSH_PINGREQ_TASK_STK_SIZE 8
static unsigned int mKiiPush_pingReqTaskStk[KIIPUSH_PINGREQ_TASK_STK_SIZE];
#endif

int kiiPush_install(kii_t* kii, kii_bool_t development, char* installation_id)
{
	char* p1;
	char* p2;
	char* buf;
	int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    buf = kii->http_context.buffer;
    core_err = kii_install_thing_push(kii, development);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_run(kii);
        state = kii_get_state(kii);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->logger_cb("resp: %s\n", kii->response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->response_code < 200 || 300 <= kii->response_code)
    {
		goto exit;
    }
	p1 = strstr(buf, "\"installationID\"");
	if(p1 == NULL)
	{
		goto exit;
	}
	p1 = strstr(p1, ":");
	if(p1 == NULL)
	{
		goto exit;
	}
	p1 = strstr(p1, "\"");
	if(p1 == NULL)
	{
		goto exit;
	}
	p1 += 1;
	p2 = strstr(p1, "\"");
	if(p2 == NULL)
	{
		goto exit;
	}
	memcpy(installation_id, p1, p2 - p1);
	ret = 0;
	
exit:
	return ret;
}

static kiiPush_endpointState_e kiiPush_retrieveEndpoint(kii_t* kii, const char* installation_id, kii_mqtt_endpoint_t* endpoint)
{
	char* p1;
	char* p2;
	char* buf;
	kiiPush_endpointState_e ret;
    kii_error_code_t core_err;
    kii_state_t state;

    buf = kii->http_context.buffer;
    core_err = kii_get_mqtt_endpoint(kii, installation_id);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_run(kii);
        state = kii_get_state(kii);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->logger_cb("resp: %s\n", kii->response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->response_code == 503)
    {
		ret = KIIPUSH_ENDPOINT_UNAVAILABLE;
		goto exit;
    }
    if(kii->response_code < 200 || 300 <= kii->response_code)
    {
		ret = KIIPUSH_ENDPOINT_ERROR;
		goto exit;
    }

    /* get username*/
    p1 = strstr(buf, "\"username\"");
    if(p1 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    p1 = strstr(p1, ":");
    if(p1 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    p1 = strstr(p1, "\"");
    if(p1 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    p1 += 1;
    p2 = strstr(p1, "\"");
    if(p2 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    memcpy(endpoint->username, p1, p2 - p1);

    /* get password*/
    p1 = strstr(buf, "\"password\"");
    if(p1 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    p1 = strstr(p1, ":");
    if(p1 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    p1 = strstr(p1, "\"");
    if(p1 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    p1 += 1;
    p2 = strstr(p1, "\"");
    if(p2 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    memcpy(endpoint->password, p1, p2 - p1);

    /* get host*/
    p1 = strstr(buf, "\"host\"");
    if(p1 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    p1 = strstr(p1, ":");
    if(p1 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    p1 = strstr(p1, "\"");
    if(p1 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    p1 += 1;
    p2 = strstr(p1, "\"");
    if(p2 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    memcpy(endpoint->host, p1, p2 - p1);

    /* get mqttTopic*/
    p1 = strstr(buf, "\"mqttTopic\"");
    if(p1 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    p1 = strstr(p1, ":");
    if(p1 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    p1 = strstr(p1, "\"");
    if(p1 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    p1 += 1;
    p2 = strstr(p1, "\"");
    if(p2 == NULL)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    memcpy(endpoint->topic, p1, p2 - p1);
    /* TODO: parse from response */
    endpoint->port_tcp = 1883;
    ret = KIIPUSH_ENDPOINT_READY;
	
exit:
	return ret;
}


int kiiPush_subscribeBucket(kii_t* kii, kii_bucket_t* bucket)
{
	int ret = -1;

    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_subscribe_bucket(kii, bucket);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_run(kii);
        state = kii_get_state(kii);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->logger_cb("resp: %s\n", kii->response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if (kii->response_code == 204 || kii->response_code == 409) {
        ret = 0;
    }
exit:
	return ret;
}

int kiiPush_subscribeTopic(kii_t* kii, kii_topic_t* topic)
{
	int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_subscribe_topic(kii, topic);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_run(kii);
        state = kii_get_state(kii);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->logger_cb("resp: %s\n", kii->response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if (kii->response_code == 204 || kii->response_code == 409) {
        ret = 0;
    }
exit:
	return ret;
}

int kiiPush_createTopic(kii_t* kii, kii_topic_t* topic)
{
	int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_create_topic(kii, topic);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_run(kii);
        state = kii_get_state(kii);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->logger_cb("resp: %s\n", kii->response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if (kii->response_code == 204 || kii->response_code == 409) {
        ret = 0;
    }
exit:
	return ret;
}

static void* kiiPush_recvMsgTask(void* sdata)
{
	int remainingLen;
	int byteLen;
	int topicLen;
	int totalLen;
	char* p;
	int bytes = 0;
	size_t rcvdCounter = 0;
	KII_PUSH_RECEIVED_CB callback;
	kiiPush_endpointState_e endpointState;
    kii_t* kii;
    kii_mqtt_endpoint_t endpoint;
    char installation_id[KII_PUSH_INSTALLATIONID_SIZE + 1];

    memset(installation_id, 0x00, sizeof(installation_id));
    memset(&endpoint, 0x00, sizeof(kii_mqtt_endpoint_t));

    kii = (kii_t*) sdata;
	callback = kii->push_received_cb;
	for(;;)
	{
		if(kii->_mqtt_endpoint_ready == 0)
		{
			if(kiiPush_install(kii, KII_FALSE, installation_id) != 0)
			{
				kii->delay_ms_cb(1000);
				continue;
			}

			do
			{
				kii->delay_ms_cb(1000);
				endpointState = kiiPush_retrieveEndpoint(kii, installation_id, &endpoint);
			}
			while((endpointState == KIIPUSH_ENDPOINT_UNAVAILABLE));

			if(endpointState != KIIPUSH_ENDPOINT_READY)
			{
				continue;
			}
			M_KII_LOG(kii->logger_cb("installationID:%s\r\n", installation_id));
			M_KII_LOG(kii->logger_cb("mqttTopic:%s\r\n", endpoint.topic));
			M_KII_LOG(kii->logger_cb("host:%s\r\n", endpoint.host));
			M_KII_LOG(kii->logger_cb("username:%s\r\n", endpoint.username));
			M_KII_LOG(kii->logger_cb("password:%s\r\n", endpoint.password));
			if(kiiMQTT_connect(kii, &endpoint, KII_PUSH_KEEP_ALIVE_INTERVAL_VALUE) < 0)
			{
				continue;
			}
			else if(kiiMQTT_subscribe(kii, endpoint.topic, QOS0) < 0)
			{
				continue;
			}
			else
			{
				kii->_mqtt_endpoint_ready = 1;
			}
		}
		else
		{
			memset(kii->mqtt_buffer, 0, kii->mqtt_buffer_size);
            M_KII_LOG(kii->logger_cb("readPointer: %d\r\n", kii->mqtt_buffer + bytes));
            
            rcvdCounter = 0;
			kii->socket_recv_cb(&kii->socket_context, kii->mqtt_buffer, 2, &rcvdCounter);
			if(rcvdCounter == 2)
			{
				if((kii->mqtt_buffer[0] & 0xf0) == 0x30)
				{
                    rcvdCounter = 0;
					kii->socket_recv_cb(&kii->socket_context, kii->mqtt_buffer+2, KII_PUSH_TOPIC_HEADER_SIZE, &rcvdCounter);
					if(rcvdCounter == KII_PUSH_TOPIC_HEADER_SIZE)
					{
					    byteLen = kiiMQTT_decode(&kii->mqtt_buffer[1], &remainingLen);
					}
					else
					{
						M_KII_LOG(kii->logger_cb("kii-error: mqtt decode error\r\n"));
						kii->_mqtt_endpoint_ready = 0;
						continue;
					}
					if(byteLen > 0)
					{
						totalLen =
						    remainingLen + byteLen + 1; /* fixed head byte1+remaining length bytes + remaining bytes*/
					}
					else
					{
						M_KII_LOG(kii->logger_cb("kii-error: mqtt decode error\r\n"));
						kii->_mqtt_endpoint_ready = 0;
						continue;
					}
					if(totalLen > kii->mqtt_buffer_size)
					{
						M_KII_LOG(kii->logger_cb("kii-error: mqtt buffer overflow\r\n"));
						kii->_mqtt_endpoint_ready = 0;
						continue;
					}

					M_KII_LOG(kii->logger_cb("decode byteLen=%d, remainingLen=%d\r\n", byteLen, remainingLen));
					bytes = rcvdCounter + 2;
                    M_KII_LOG(kii->logger_cb("totalLen: %d, bytes: %d\r\n", totalLen, bytes));
					while(bytes < totalLen)
					{
                        M_KII_LOG(kii->logger_cb("totalLen: %d, bytes: %d\r\n", totalLen, bytes));
                        M_KII_LOG(kii->logger_cb("lengthToLead: %d\r\n", totalLen - bytes));
                        M_KII_LOG(kii->logger_cb("readPointer: %d\r\n", kii->mqtt_buffer + bytes));
                        /*kii->socket_recv_cb(&(kii->socket_context), kii->mqtt_buffer + bytes, totalLen - bytes, &rcvdCounter);*/
                        rcvdCounter = 0;
                        kii->socket_recv_cb(&(kii->socket_context), kii->mqtt_buffer + bytes, totalLen - bytes, &rcvdCounter);
                        M_KII_LOG(kii->logger_cb("totalLen: %d, bytes: %d\r\n", totalLen, bytes));
						if(rcvdCounter > 0)
						{
							bytes += rcvdCounter;
                            M_KII_LOG(kii->logger_cb("success read. totalLen: %d, bytes: %d\r\n", totalLen, bytes));
						}
						else
						{
							bytes = -1;
                            M_KII_LOG(kii->logger_cb("failed to read. totalLen: %d, bytes: %d\r\n", totalLen, bytes));
							break;
						}
					}
					M_KII_LOG(kii-logger_cb("bytes:%d, totalLen:%d\r\n", bytes, totalLen));
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
                            M_KII_LOG(kii-logger_cb("Successfully Recieved Push %s\n", p));
						    callback(p, remainingLen - 2 - topicLen);
						}
						else
						{
							M_KII_LOG(kii-logger_cb("kii-error: mqtt topic length error\r\n"));
						    kii->_mqtt_endpoint_ready = 0;
							continue;
						}
					}
					else
					{
						M_KII_LOG(kii->logger_cb("kii_error: mqtt receive data error\r\n"));
                        kii->_mqtt_endpoint_ready = 0;
						continue;
					}
				}
#if(KII_PUSH_PING_ENABLE)
				else if((kii->mqtt_buffer[0] & 0xf0) == 0xd0)
				{
					M_KII_LOG(kii->logger_cb("ping resp\r\n"));
				}
#endif
			}
			else
			{
                M_KII_LOG(kii->logger_cb("kii-error: mqtt receive data error\r\n"));
                kii->_mqtt_endpoint_ready = 0;
			}
		}
	}
	return NULL;
}

#if(KII_PUSH_PING_ENABLE)
static void* kiiPush_pingReqTask(void* sdata)
{
    kii_t* kii;

    kii = (kii_t*)sdata;
	for(;;)
	{
		if(kii->_mqtt_endpoint_ready == 1)
		{
			kiiMQTT_pingReq(kii);
		}
		kii->delay_ms_cb(KII_PUSH_KEEP_ALIVE_INTERVAL_VALUE * 1000);
	}
	return NULL;
}
#endif

int kiiPush_init(kii_t* kii, unsigned int recvMsgtaskPrio, unsigned int pingReqTaskPrio, KII_PUSH_RECEIVED_CB callback)
{
    kii->push_received_cb = callback;
    kii->task_create_cb(NULL,
            kiiPush_recvMsgTask,
            (void*)kii,
            (void*)mKiiPush_taskStk,
            KIIPUSH_TASK_STK_SIZE * sizeof(unsigned char),
            recvMsgtaskPrio);
#if(KII_PUSH_PING_ENABLE)
    kii->task_create_cb(NULL,
            kiiPush_pingReqTask,
            (void*)kii,
            (void*)mKiiPush_pingReqTaskStk,
            KIIPUSH_PINGREQ_TASK_STK_SIZE * sizeof(unsigned char),
            pingReqTaskPrio);
#endif
	return 0;
}
