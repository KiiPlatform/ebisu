#ifndef KII_PUSH_H
#define KII_PUSH_H

#define KII_PUSH_INSTALLATIONID_SIZE 20
#define KII_PUSH_HOST_SIZE 64
#define KII_PUSH_TOPIC_SIZE 30
#define KII_PUSH_USERNAME 64
#define KII_PUSH_PASSWORD 64
#define KII_PUSH_RECV_BUF_SIZE 1024


typedef enum
{
    KIIPUSH_ENDPOINT_READY =  0,
    KIIPUSH_ENDPOINT_UNAVAILABLE = 1,
    KIIPUSH_ENDPOINT_ERROR = 2
}kiiPush_endpointState_e; 


typedef struct {
	char installationID[KII_PUSH_INSTALLATIONID_SIZE+1];
	char topic[KII_PUSH_TOPIC_SIZE+1];
	char host[KII_PUSH_HOST_SIZE+1];
	char username[KII_PUSH_USERNAME+1];
	char password[KII_PUSH_PASSWORD+1];
	char rcvdBuf[KII_PUSH_RECV_BUF_SIZE];
} kii_push_struct;



#endif

