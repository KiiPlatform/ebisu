/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include "WinnerMicro.h"



char expired(Timer* timer) {
long left = timer->end_time - (OSTimeGet()*10);
return (left < 0);
}


void countdown_ms(Timer* timer, unsigned int timeout) {
	timer->end_time = (OSTimeGet()*10) + timeout;
}


void countdown(Timer* timer, unsigned int timeout) {
	timer->end_time = (OSTimeGet()*10) + (timeout * 1000);
}


int left_ms(Timer* timer) {
	long left = timer->end_time - (OSTimeGet()*10);
	return (left < 0) ? 0 : left;
}


void InitTimer(Timer* timer) {
	timer->end_time = 0;
}


int wm_read(Network* n, unsigned char* buffer, int len, int timeout_ms) {
	int ret;
	int i;

	ret = kiiHal_socketRecv(n->my_socket, buffer, len);
	printf("\r\n===============wm read start==========================\r\n");
	for (i= 0; i<len; i++)
	{
	    printf("%02x", buffer[i]);
	}
	printf("\r\n================wm read end=========================\r\n");
	
	return ret;
}


int wm_write(Network* n, unsigned char* buffer, int len, int timeout_ms) {
	int i;
	printf("\r\n=================wm write start========================\r\n");
	for (i= 0; i<len; i++)
	{
	    printf("%02x", buffer[i]);
	}
	printf("\r\n==================wm write end=======================\r\n");
	return kiiHal_socketSend(n->my_socket, buffer,  len);
}


void wm_disconnect(Network* n) {
	kiiHal_socketClose(n->my_socket);
}


void NewNetwork(Network* n) {
	n->my_socket = 0;
	n->mqttread = wm_read;
	n->mqttwrite = wm_write;
	n->disconnect = wm_disconnect;
}

int ConnectNetwork(Network* n, char* addr, int port)
{
    unsigned char ipBuf[4];

    printf("kii-info: host ""%s""\r\n", addr);
    if (kiiHal_dns(addr, ipBuf) < 0)
    {
        printf("kii-error: dns failed !\r\n");
        return -1;
    }
    printf("Host ip:%d.%d.%d.%d\r\n", ipBuf[0], ipBuf[1], ipBuf[2], ipBuf[3]);
		
    n->my_socket = kiiHal_socketCreate();
    if (n->my_socket < 0)
    {
        printf("kii-error: create socket failed !\r\n");
        return -1;
    }
	
    if (kiiHal_connect(n->my_socket, (char*)ipBuf, port) < 0)
    {
        printf("kii-error: connect to server failed \r\n");
	 kiiHal_socketClose(n->my_socket);
        return -1;
    }
	return 0;
}
