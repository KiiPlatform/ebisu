#ifndef KII_HAL_H
#define KII_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*KiiHal_taskEntry)(void* pValue);

int kiiHal_dns(char* hostName, unsigned char* buf);
int kiiHal_socketCreate(void);
int kiiHal_socketClose(int* socket_num);
int kiiHal_connect(int socket_num, char* sa_data, int port);
int kiiHal_socketSend(int socket_num, char* buf, int len);
int kiiHal_socketRecv(int socket_num, char* buf, int len);
int kiiHal_socketRecvDataAvailable(void);
int kiiHal_socketConnected(void);
int kiiHal_transfer(char* host, char* buf, int bufLen, int sendLen);
void kiiHal_delayMs(unsigned int ms);
int kiiHal_taskCreate(const char* name,
                      KiiHal_taskEntry pEntry,
                      void* param,
                      unsigned char* stk_start,
                      unsigned int stk_size,
                      unsigned int prio);
void* kiiHal_malloc(unsigned long size);
void kiiHal_free(void* p);
void kiiHal_debug(const char *fmt,...);

#ifdef __cplusplus
}
#endif


#endif

