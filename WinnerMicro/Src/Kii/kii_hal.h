#ifndef KII_HAL_H
#define KII_HAL_H

int kiiHAL_dns(char *host, unsigned char *buf);
int kiiHAL_socketCreate(void);
int kiiHAL_socketClose(int socket_num);
int kiiHAL_connect(int socket_num, char *sa_data);
int kiiHAL_socketSend(int socket_num, char * buf, int len);
int kiiHAL_socketRecv(int socket_num, char * buf, int len);
int kiiHal_transfer(void);

#endif

