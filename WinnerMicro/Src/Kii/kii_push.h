#ifndef KII_PUSH_H
#define KII_PUSH_H

#define KII_PUSH_INSTALLATIONID_SIZE 64
#define KII_PUSH_HOST_SIZE 128
#define KII_PUSH_MQTTTOPIC_SIZE 64
#define KII_PUSH_USERNAME 128
#define KII_PUSH_PASSWORD 128
#define KII_PUSH_RECV_BUF_SIZE 1024
#define KII_PUSH_SEND_BUF_SIZE 1024


#define KII_PUSH_KEEP_ALIVE_INTERVAL_VALUE  30

typedef enum
{
    KIIPUSH_ENDPOINT_READY =  0,
    KIIPUSH_ENDPOINT_UNAVAILABLE = 1,
    KIIPUSH_ENDPOINT_ERROR = 2
}kiiPush_endpointState_e; 


typedef struct {
	char installationID[KII_PUSH_INSTALLATIONID_SIZE+1];
	char mqttTopic[KII_PUSH_MQTTTOPIC_SIZE+1];
	char host[KII_PUSH_HOST_SIZE+1];
	char username[KII_PUSH_USERNAME+1];
	char password[KII_PUSH_PASSWORD+1];
	char rcvdBuf[KII_PUSH_RECV_BUF_SIZE];
	int rcvdCounter;
       char sendBuf[KII_PUSH_SEND_BUF_SIZE];
       int mqttSocket;
       char connected;
} kii_push_struct;



#endif

