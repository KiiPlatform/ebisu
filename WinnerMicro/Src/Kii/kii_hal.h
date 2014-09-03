#ifndef KII_HAL_H
#define KII_HAL_H

int kiiHAL_Dns(char *host, char *buf);
int kiiHAL_SocketCreate(void);
int kiiHAL_SocketClose(int socket_num);
int kiiHAL_Connect(int socket_num, char *sa_data);
int kiiHAL_SocketSend(int socket_num, unsigned char * buf, int len);
int kiiHAL_SocketRecv(int socket_num, unsigned char * buf, int len);

#endif

