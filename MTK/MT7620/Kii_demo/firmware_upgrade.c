#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <sys/stat.h>

#include "firmware_upgrade.h"
#include "light.h"
#include "light_if.h"

pthread_t m_thread = 0;

int hal_getHostPort(char* url, char* host, int* port)
{
	char* p1;
	char* p2;

	p1 = strstr(url, "http://");
	if(p1 != NULL)
	{
		p1 += strlen("http://");
	}
	else
	{
		printf("url:%s format error\n", url);
		return -1;
	}

	p2 = strchr(p1, '/');
	if(p2 == NULL)
	{
		printf("url:%s format error\n", url);
		return -1;
	}
	else
	{
		memcpy(host, p1, p2 - p1);
		*port = FWUP_DEFAULT_PORT;
		return 0;
	}
}

/*****************************************************************************
*
*  hal_dns
*
*  \param  host - the input of host name
*               buf - the out put of IP address
*
*  \return  0:success; -1: failure
*
*  \brief  Gets host IP address
*
*****************************************************************************/
int hal_dns(char* hostName, unsigned char* buf)
{
	struct hostent* host;

	host = gethostbyname(hostName);
	if(host != NULL)
	{
		memcpy(buf, host->h_addr_list[0], 4);
		return 0;
	}
	else
	{
		return -1;
	}
}

/*****************************************************************************
*
*  hal_socketCreate
*
*  \param  none
*
*  \return  socket handle
*
*  \brief  Creates socket
*
*****************************************************************************/
int hal_socketCreate(void)
{
	int socketNum;

	socketNum = socket(AF_INET, SOCK_STREAM, 0);
	if(socketNum < 0)
	{
		socketNum = -1;
	}
	return socketNum;
}

/*****************************************************************************
*
*  hal_socketClose
*
*  \param  socketNum - socket handle pointer
*
*  \return  0:success; -1: failure
*
*  \brief  Closes a socket
*
*****************************************************************************/
int hal_socketClose(int* socketNum)
{
	int ret = 0;

	if(close(*socketNum) != 0)
	{
		ret = -1;
	}
	*socketNum = -1;

	return ret;
}

/*****************************************************************************
*
*  hal_connect
*
*  \param  socketNum - socket handle
*               saData - address
*               port - port number
*
*  \return  0:success; -1: failure
*
*  \brief  Connects a TCP socket
*
*****************************************************************************/
int hal_connect(int socketNum, char* saData, int port)
{
	int ret = 0;

	struct sockaddr_in pin;

	memset(&pin, 0, sizeof(struct sockaddr));
	pin.sin_family = AF_INET; // use IPv4
	memcpy((char*)&pin.sin_addr.s_addr, saData, 4);
	pin.sin_port = htons(port);
	if(connect(socketNum, (struct sockaddr*)&pin, sizeof(struct sockaddr)) != 0)
	{
		ret = -1;
	}
	return ret;
}

/*****************************************************************************
*
*  hal_socketSend
*
*  \param  socketNum - socket handle
*               buf - date to be sent
*               len - size of data in bytes
*
*  \return  Number of bytes sent
*
*  \brief  Sends data out to the internet
*
*****************************************************************************/
int hal_socketSend(int socketNum, char* buf, int len)
{
	int bytes;
	int sent;

	bytes = 0;
	while(bytes < len)
	{
		sent = send(socketNum, buf + bytes, len - bytes, 0);
		if(sent <= 0)
		{

			return -1;
		}
		else
		{
			bytes += sent;
		}
	}
	return bytes;
}

/*****************************************************************************
*
*  hal_socketRecv
*
*  \param  socketNum - socket handle;
*               buf - data buffer to receive;
*               len - size of buffer in bytes;
*
*  \return Number of bytes received
*
*  \brief  Receives data from the internet
*
*****************************************************************************/
int hal_socketRecv(int socketNum, char* buf, int len)
{
	int ret;

	ret = recv(socketNum, buf, len, 0);
	if(ret <= 0)
	{
		ret = -1;
	}

	return ret;
}

/*****************************************************************************
*
*  hal_transfer
*
*  \param   buf - data buffer;
*               bufLen - size of buffer in bytes;
*               sendLen - the length of data to be sent;
*
*  \return  0:success; -1: failure
*
*  \brief  Sends and receives data from the internet
*
*****************************************************************************/
int hal_transfer(int socketNum, char* buf, int bufLen, int sendLen)
{
	int bytes;
	int len;
	char* p1;
	char* p2;
	unsigned long contentLengh;

	len = hal_socketSend(socketNum, buf, sendLen);
	if(len < 0)
	{

		printf("send data fail\r\n");
		return -1;
	}

	bytes = 0;
	memset(buf, 0, bufLen);
	while(bytes < bufLen)
	{
		len = hal_socketRecv(socketNum, buf + bytes, bufLen - bytes);
		if(len < 0)
		{
			printf("recv data fail\r\n");
			return -1;
		}
		else
		{
			bytes += len;
			p1 = strstr(buf, "\r\n\r\n");
			if(p1 != NULL)
			{
				p2 = strstr(buf, "Content-Length: ");
				if(p2 != NULL)
				{
					p2 += strlen("Content-Length: ");
					contentLengh = strtoul(p2, 0, 0);
					if(contentLengh > 0)
					{
						p1 += strlen("\r\n\r\n");
						if(bytes >= (contentLengh + (p1 - buf)))
						{
							return 0;
						}
					}
					else   // should never get here
					{
						printf("get content lenght failed\r\n");
						return -1;
					}
				}
				else
				{
					return 0; // no content length
				}
			}
		}
	}
	printf("receiving buffer overflow !\r\n");
	return -1; // buffer overflow
}

/*****************************************************************************
*
*  hal_malloc
*
*  \param  size - the size of memory to be allocated
*
*  \return  the address of memory
*
*  \brief  Allocates memory
*
*****************************************************************************/
void* hal_malloc(unsigned long size)
{
	return malloc(size);
}

/*****************************************************************************
*
*  hal_free
*
*  \param  p - the address of memory
*
*  \return  none
*
*  \brief  Frees memory
*
*****************************************************************************/
void hal_free(void* p)
{
	free(p);
}

static int fwup_doUpgrade(char* fileName)
{
	return 0;
}

static int fwup_downloadFile(char* url, char* fileName)
{
	int socketNum;
	unsigned char ipBuf[4];
	char* p1;
	char* p2;
	char* buf;
	int fd;
	int len;
	int currentPosition;
	int totalLen;
	int contentLength;
	char host[FWUP_HOST_SIZE + 1];
	int port;
	int percent = 0;

	memset(host, 0, sizeof(host));
	port = 0;

	if(hal_getHostPort(url, host, &port) < 0)
	{
		printf("get host and port error\r\n");
		return -1;
	}

	if(hal_dns(host, ipBuf) < 0)
	{
		printf("error: dns failed !\r\n");
		return -1;
	}

	socketNum = hal_socketCreate();
	if(socketNum < 0)
	{
		printf("error: create socket failed !\r\n");
		return -1;
	}

	if(hal_connect(socketNum, (char*)ipBuf, port) < 0)
	{
		printf("error: connect to server failed \r\n");
		hal_socketClose(&socketNum);
		return -1;
	}

	fd = open(fileName, O_RDWR);
	if(fd < 0)
	{
		printf("open file:%s failed\r\n", fileName);
		hal_socketClose(&socketNum);
		return -1;
	}
	currentPosition = lseek(fd, 0, SEEK_END);

	buf = hal_malloc(FWUP_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		printf("memory allocation failed !\r\n");
		close(fd);
		hal_socketClose(&socketNum);
		return -1;
	}
	for(;;)
	{
		memset(buf, 0, FWUP_SOCKET_BUF_SIZE);
		strcpy(buf, "GET ");
		// url
		strcat(buf, url);
		strcat(buf, " HTTP/1.1");
		strcat(buf, "\r\n");
		// Connection
		strcat(buf, "Connection: Keep-Alive\r\n");
		// Host
		strcat(buf, "Host: ");
		strcat(buf, host);
		strcat(buf, "\r\n");
		// RANGE
		strcat(buf, "RANGE: bytes=");
		sprintf(buf + strlen(buf), "%d", currentPosition);
		strcat(buf, "-");
		sprintf(buf + strlen(buf), "%d", currentPosition + (FWUP_SOCKET_BUF_SIZE - FWUP_HTTP_HEADER_SIZE) - 1);
		strcat(buf, "\r\n\r\n");

		if(hal_transfer(socketNum, buf, FWUP_SOCKET_BUF_SIZE, strlen(buf)) != 0)
		{
			printf("transfer data error !\r\n");
			hal_free(buf);
			close(fd);
			hal_socketClose(&socketNum);
			return -1;
		}

		if(strstr(buf, "HTTP/1.1 206") == NULL)
		{
			printf("http status error !\r\n");
			hal_free(buf);
			close(fd);
			hal_socketClose(&socketNum);
			return -1;
		}

		p1 = strstr(buf, "Content-Range:");
		if(p1 == NULL)
		{
			printf("can not get Content-Range!\r\n");
			hal_free(buf);
			close(fd);
			hal_socketClose(&socketNum);
			return -1;
		}
		p2 = strstr(p1, "/");
		if(p2 == NULL)
		{
			printf("can not get bytes info!\r\n");
			hal_free(buf);
			close(fd);
			hal_socketClose(&socketNum);
			return -1;
		}
		p2++;

		totalLen = strtoul(p2, 0, 0);

		p1 = strstr(buf, "Content-Length:");
		if(p1 == NULL)
		{
			printf("can not get Content-Length!\r\n");
			hal_free(buf);
			close(fd);
			hal_socketClose(&socketNum);
			return -1;
		}
		p1 += strlen("Content-Length:");
		contentLength = strtoul(p1, 0, 0);
		p1 = strstr(buf, "\r\n\r\n");
		if(p1 == NULL)
		{
			printf("can not get conten data!\r\n");
			hal_free(buf);
			close(fd);
			hal_socketClose(&socketNum);
			return -1;
		}
		p1 += 4;
		len = write(fd, p1, contentLength);
		if(len != contentLength)
		{
			printf("write data error!\r\n");
			hal_free(buf);
			close(fd);
			hal_socketClose(&socketNum);
			return -1;
		}
		currentPosition += contentLength;
		if(totalLen > 0)
		{
			if(percent != (currentPosition * 100) / totalLen)
			{
				percent = (currentPosition * 100) / totalLen;
				printf("%%%d\r\n", percent);
			}
		}
		if(currentPosition == totalLen)
		{
			printf("file download success!\r\n");
			hal_free(buf);
			close(fd);
			hal_socketClose(&socketNum);
			return 0;
		}
		else if(currentPosition > totalLen)
		{
			printf("file size error!\r\n");
			hal_free(buf);
			close(fd);
			if(unlink(fileName) < 0)
			{
				printf("unlink file:%s failed\r\n", fileName);
			}
			else
			{
				printf("file:%s is removed\r\n", fileName);
			}
			hal_socketClose(&socketNum);
			return -1;
		}
		else
		{
			continue;
		}
	}
}

static void* fwup_thread(void* sdata)
{
	int fd;
	int len;
	char url[LIGHT_FIRMWARE_UPGRADE_URL_SIZE + 1];

	fd = open(FWUP_PARAM_FILENAME, O_RDONLY);
	if(fd >= 0)
	{
		memset(url, 0, sizeof(url));
		len = read(fd, url, LIGHT_FIRMWARE_UPGRADE_URL_SIZE + 1);
		close(fd);
		if(len != LIGHT_FIRMWARE_UPGRADE_URL_SIZE + 1)
		{
			printf("read file:%s failed\r\n", FWUP_PARAM_FILENAME);
			pthread_exit((void*)0);
		}
		else
		{
			printf("url:%s\r\n", url);
		}
	}
	else
	{
		pthread_exit((void*)0);
	}

	for(;;)
	{
		if(fwup_downloadFile(url, FWUP_DOWNLOAD_FILENAME) == 0)
		{
			// remove param file;
			if(unlink(FWUP_PARAM_FILENAME) < 0)
			{
				printf("unlink file:%s failed\r\n", FWUP_PARAM_FILENAME);
			}

			fwup_doUpgrade(FWUP_DOWNLOAD_FILENAME);
			pthread_exit((void*)0);
		}
		else
		{
			sleep(10);
		}
	}
}

void fwup_upgrade(char* url)
{
	int fd;
	int errno;
	int len;

	if(m_thread == 0)
	{
		errno = ESRCH;
	}
	else
	{
		errno = pthread_kill(m_thread, 0);
	}
	if(errno == ESRCH)
	{
		if(url == NULL)
		{
			pthread_create(&m_thread, NULL, fwup_thread, NULL);
		}
		else
		{
			fd = open(FWUP_PARAM_FILENAME, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
			if(fd >= 0)
			{
				len = write(fd, url, LIGHT_FIRMWARE_UPGRADE_URL_SIZE + 1);
				close(fd);
				if(len != LIGHT_FIRMWARE_UPGRADE_URL_SIZE + 1)
				{
					printf("write file:%s failed\r\n", FWUP_PARAM_FILENAME);
				}
				else
				{
					fd = open(FWUP_DOWNLOAD_FILENAME, O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
					if(fd >= 0)
					{
						close(fd);
						pthread_create(&m_thread, NULL, fwup_thread, NULL);
					}
					else
					{
						printf("create file:%s failed\r\n", FWUP_DOWNLOAD_FILENAME);
					}
				}
			}
			else
			{
				printf("create file:%s failed\r\n", FWUP_PARAM_FILENAME);
			}
		}
	}
	else if(errno == EINVAL)
	{
		printf("invalid signal!\r\n");
	}
	else
	{
		printf("the firmware upgrade thread is alive!\r\n");
	}
}
