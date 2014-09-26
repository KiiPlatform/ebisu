#ifndef KII_HAL_H
#define KII_HAL_H

int kiiHal_dns(char *host, unsigned char *buf);
int kiiHal_socketCreate(void);
int kiiHal_socketClose(int socket_num);
int kiiHal_connect(int socket_num, char *sa_data);
int kiiHal_socketSend(int socket_num, char * buf, int len);
int kiiHal_socketRecv(int socket_num, char * buf, int len);
int kiiHal_transfer(void);
void kiiHal_delayMs(unsigned int ms);

#endif

