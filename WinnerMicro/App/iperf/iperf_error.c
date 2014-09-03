/*
 * Copyright (c) 2009-2011, The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory (subject to receipt of any
 * required approvals from the U.S. Dept. of Energy).  All rights reserved.
 *
 * This code is distributed under a BSD style license, see the LICENSE file
 * for complete information.
 */

#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include "iperf.h"
#include "iperf_error.h"
#include "wpa_debug.h"

int i_errno;

char *
iperf_strerror(int i_errno)
{
    static char errstr[256];
    int len, perr, herr;
    perr = herr = 0;

    len = sizeof(errstr);
    memset(errstr, 0, len);

    switch (i_errno) {
        case IENONE:
            os_snprintf(errstr, len, "No error");
            break;
        case IESERVCLIENT:
            os_snprintf(errstr, len, "Iperf cannot be both server and client");
            break;
        case IENOROLE:
            os_snprintf(errstr, len, "Iperf instance must either be a client (-c) or server (-s)");
            break;
        case IECLIENTONLY:
            os_snprintf(errstr, len, "Some option you are trying to set is client only");
            break;
        case IEDURATION:
            os_snprintf(errstr, len, "Test duration too long (maximum = %d seconds)", MAX_TIME);
            break;
        case IENUMSTREAMS:
            os_snprintf(errstr, len, "Number of parallel streams too large (maximum = %d)", MAX_STREAMS);
            break;
        case IEBLOCKSIZE:
            os_snprintf(errstr, len, "Block size too large (maximum = %d bytes)", MAX_BLOCKSIZE);
            break;
        case IEBUFSIZE:
            os_snprintf(errstr, len, "Socket buffer size too large (maximum = %d bytes)", MAX_TCP_BUFFER);
            break;
        case IEINTERVAL:
            os_snprintf(errstr, len, "Report interval too large (maximum = %d seconds)", MAX_INTERVAL);
            break;
        case IEMSS:
            os_snprintf(errstr, len, "TCP MSS too large (maximum = %d bytes)", MAX_MSS);
            break;
        case IENEWTEST:
            os_snprintf(errstr, len, "Unable to create a new test");
            perr = 1;
            break;
        case IEINITTEST:
            os_snprintf(errstr, len, "Test initialization failed");
            perr = 1;
            break;
        case IELISTEN:
            os_snprintf(errstr, len, "Unable to start listener for connections");
            perr = 1;
            break;
        case IECONNECT:
            os_snprintf(errstr, len, "Unable to connect to server");
            herr = 1;
            perr = 1;
            break;
        case IEACCEPT:
            os_snprintf(errstr, len, "Unable to accept connection from client");
            herr = 1;
            perr = 1;
            break;
        case IESENDCOOKIE:
            os_snprintf(errstr, len, "Unable to send cookie to server");
            perr = 1;
            break;
        case IERECVCOOKIE:
            os_snprintf(errstr, len, "Unable to receive cookie to server");
            perr = 1;
            break;
        case IECTRLWRITE:
            os_snprintf(errstr, len, "Unable to write to the control socket");
            perr = 1;
            break;
        case IECTRLREAD:
            os_snprintf(errstr, len, "Unable to read from the control socket");
            perr = 1;
            break;
        case IECTRLCLOSE:
            os_snprintf(errstr, len, "Control socket has closed unexpectedly");
            break;
        case IEMESSAGE:
            os_snprintf(errstr, len, "Received an unknown control message");
            break;
        case IESENDMESSAGE:
            os_snprintf(errstr, len, "Unable to send control message");
            perr = 1;
            break;
        case IERECVMESSAGE:
            os_snprintf(errstr, len, "Unable to receive control message");
            perr = 1;
            break;
        case IESENDPARAMS:
            os_snprintf(errstr, len, "Unable to send parameters to server");
            perr = 1;
            break;
        case IERECVPARAMS:
            os_snprintf(errstr, len, "Unable to receive parameters from client");
            perr = 1;
            break;
        case IEPACKAGERESULTS:
            os_snprintf(errstr, len, "Unable to package results");
            perr = 1;
            break;
        case IESENDRESULTS:
            os_snprintf(errstr, len, "Unable to send results");
            perr = 1;
            break;
        case IERECVRESULTS:
            os_snprintf(errstr, len, "Unable to receive results");
            perr = 1;
            break;
        case IESELECT:
            os_snprintf(errstr, len, "Select failed");
            perr = 1;
            break;
        case IECLIENTTERM:
            os_snprintf(errstr, len, "The client has terminated");
            break;
        case IESERVERTERM:
            os_snprintf(errstr, len, "The server has terminated");
            break;
        case IEACCESSDENIED:
            os_snprintf(errstr, len, "The server is busy running a test. try again later");
            break;
        case IESETNODELAY:
            os_snprintf(errstr, len, "Unable to set TCP NODELAY");
            perr = 1;
            break;
        case IESETMSS:
            os_snprintf(errstr, len, "Unable to set TCP MSS");
            perr = 1;
            break;
        case IESETBUF:
            os_snprintf(errstr, len, "Unable to set socket buffer size");
            perr = 1;
            break;
        case IESETTOS:
            os_snprintf(errstr, len, "Unable to set IP TOS");
            perr = 1;
            break;
        case IESETCOS:
            os_snprintf(errstr, len, "Unable to set IPv6 traffic class");
            perr = 1;
            break;
        case IEREUSEADDR:
            os_snprintf(errstr, len, "Unable to reuse address on socket");
            perr = 1;
            break;
        case IENONBLOCKING:
            os_snprintf(errstr, len, "Unable to set socket to non-blocking");
            perr = 1;
            break;
        case IESETWINDOWSIZE:
            os_snprintf(errstr, len, "Unable to set socket window size");
            perr = 1;
            break;
        case IEPROTOCOL:
            os_snprintf(errstr, len, "Protocol does not exist");
            break;
        case IECREATESTREAM:
            os_snprintf(errstr, len, "Unable to create a new stream");
            herr = 1;
            perr = 1;
            break;
        case IEINITSTREAM:
            os_snprintf(errstr, len, "Unable to initialize stream");
            herr = 1;
            perr = 1;
            break;
        case IESTREAMLISTEN:
            os_snprintf(errstr, len, "Unable to start stream listener");
            perr = 1;
            break;
        case IESTREAMCONNECT:
            os_snprintf(errstr, len, "Unable to connect stream");
            herr = 1;
            perr = 1;
            break;
        case IESTREAMACCEPT:
            os_snprintf(errstr, len, "Unable to accept stream connection");
            perr = 1;
            break;
        case IESTREAMWRITE:
            os_snprintf(errstr, len, "Unable to write to stream socket");
            perr = 1;
            break;
        case IESTREAMREAD:
            os_snprintf(errstr, len, "Unable to read from stream socket");
            perr = 1;
            break;
        case IESTREAMCLOSE:
            os_snprintf(errstr, len, "Stream socket has closed unexpectedly");
            break;
        case IESTREAMID:
            os_snprintf(errstr, len, "Stream has an invalid id");
            break;
        case IENEWTIMER:
            os_snprintf(errstr, len, "Unable to create new timer");
            perr = 1;
            break;
        case IEUPDATETIMER:
            os_snprintf(errstr, len, "Unable to update timer");
            perr = 1;
            break;
    }

    if (herr || perr)
        strncat(errstr, ": ", len);
    if (h_errno && herr) {
        //strncat(errstr, hstrerror(h_errno), len);
    } else if (errno && perr) {
        strncat(errstr, strerror(errno), len);
    }

    return (errstr);
}

void
iperf_error(char *estr)
{
    printf("%s: %s\n", estr, iperf_strerror(i_errno));
}

/*
void
iperf_error(char *estr)
{
    printf("%s: ", estr);

    switch (i_errno) {
        case IESERVCLIENT:
            printf("iperf cannot be both server and client\n");
            break;
        case IENOROLE:
            printf("iperf instance must either be a client (-c) or server (-s)\n");
            break;
        case IECLIENTONLY:
            printf("some option you are trying to set is client only\n");
            break;
        case IEDURATION:
            printf("test duration too long (maximum = %d seconds)\n", MAX_TIME);
            break;
        case IENUMSTREAMS:
            printf("number of parallel streams too large (maximum = %d)\n", MAX_STREAMS);
            break;
        case IEBLOCKSIZE:
            printf("block size too large (maximum = %d bytes)\n", MAX_BLOCKSIZE);
            break;
        case IEBUFSIZE:
            printf("socket buffer size too large (maximum = %d bytes)\n", MAX_TCP_BUFFER);
            break;
        case IEINTERVAL:
            printf("report interval too large (maximum = %d seconds)\n", MAX_INTERVAL);
            break;
        case IEMSS:
            printf("TCP MSS too large (maximum = %d bytes)\n", MAX_MSS);
            break;
        case IECTRLWRITE:
            if (errno)
                printf("unable to write to the control socket: %s\n", strerror(errno));
            else
                printf("unable to write to the control socket\n");
            break;
        case IECTRLREAD:
            if (errno)
                fprintf(stderr, "unable to read from the control socket: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to read from the control socket\n");
            break;
        case IECTRLCLOSE:
            fprintf(stderr, "control socket has closed unexpectedly\n");
            break;
        case IESTREAMWRITE:
            if (errno)
                fprintf(stderr, "unable to write to stream socket: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to write to stream socket\n");
            break;
        case IESTREAMREAD:
            if (errno)
                fprintf(stderr, "unable to read from stream socket: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to read from stream socket\n");
            break;
        case IESTREAMCLOSE:
            fprintf(stderr, "stream socket has closed unexpectedly\n");
            break;
        case IENEWTEST:
            if (errno)
                fprintf(stderr, "unable to create a new test: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to create a new test\n");
            break;
        case IECONNECT:
            if (errno)
                fprintf(stderr, "unable to connect to server: %s\n", strerror(errno));
            else if (h_errno)
                fprintf(stderr, "unable to connect to server: %s\n", hstrerror(h_errno));
            else
                fprintf(stderr, "unable to connect to server\n");
            break;
        case IESELECT:
            if (errno)
                fprintf(stderr, "select failed: %s\n", strerror(errno));
            else
                fprintf(stderr, "select failed\n");
            break;
        case IESENDPARAMS:
            if (errno)
                fprintf(stderr, "unable to send parameters to server: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to send parameters to server\n");
            break;
        case IERECVPARAMS:
            if (errno)
                fprintf(stderr, "unable to receive parameters from client: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to receive parameters from client\n");
            break;
        case IECREATESTREAM:
            if (errno)
                fprintf(stderr, "unable to create a new stream: %s\n", strerror(errno));
            else if (h_errno)
                fprintf(stderr, "unable to create a new stream: %s\n", hstrerror(h_errno));
            else
                fprintf(stderr, "unable to create a new stream\n");
            break;
        case IEINITSTREAM:
            if (errno)
                fprintf(stderr, "unable to initialize stream: %s\n", strerror(errno));
            else if (h_errno)
                fprintf(stderr, "unable to initialize stream: %s\n", hstrerror(h_errno));
            else
                fprintf(stderr, "unable to initialize stream\n");
            break;
        case IESETWINDOWSIZE:
            if (errno)
                fprintf(stderr, "unable to set socket window size: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to set socket window size\n");
            break;
        case IEPACKAGERESULTS:
            if (errno)
                fprintf(stderr, "unable to package results: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to package results\n");
            break;
        case IESENDRESULTS:
            if (errno)
                fprintf(stderr, "unable to send results: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to send results\n");
            break;
        case IERECVRESULTS:
            if (errno)
                fprintf(stderr, "unable to receive results: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to receive results\n");
            break;
        case IESTREAMID:
            fprintf(stderr, "stream has an invalid id\n");
            break;
        case IESERVERTERM:
            fprintf(stderr, "the server has terminated\n");
            break;
        case IEACCESSDENIED:
            fprintf(stderr, "the server is busy running a test. try again later.\n");
            break;
        case IEMESSAGE:
            fprintf(stderr, "received an unknown control message\n");
            break;
        case IESENDMESSAGE:
            if (errno)
                fprintf(stderr, "unable to send control message: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to send control message\n");
            break;
        case IERECVMESSAGE:
            if (errno)
                fprintf(stderr, "unable to receive control message: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to receive control message\n");
            break;
        case IESENDCOOKIE:
            if (errno)
                fprintf(stderr, "unable to send cookie to server: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to send cookie to server\n");
            break;
        case IERECVCOOKIE:
            if (errno)
                fprintf(stderr, "unable to receive cookie to server: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to receive cookie to server\n");
            break;
        case IELISTEN:
            if (errno)
                fprintf(stderr, "unable to start listener for connections: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to start listener for connections\n");
            break;
        case IEACCEPT:
            if (errno)
                fprintf(stderr, "unable to accept connection from client: %s\n", strerror(errno));
            else if (h_errno)
                fprintf(stderr, "unable to accept connection from client: %s\n", hstrerror(h_errno));
            else
                fprintf(stderr, "unable to accept connection from client\n");
            break;
        case IESETNODELAY:
            if (errno)
                fprintf(stderr, "unable to set TCP NODELAY: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to set TCP NODELAY\n");
            break;
        case IESETMSS:
            if (errno)
                fprintf(stderr, "unable to set TCP MSS: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to set TCP MSS\n");
            break;
        case IEREUSEADDR:
            if (errno)
                fprintf(stderr, "unable to reuse address on socket: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to reuse address on socket\n");
            break;
        case IESTREAMCONNECT:
            if (errno)
                fprintf(stderr, "unable to connect stream: %s\n", strerror(errno));
            else if (h_errno)
                fprintf(stderr, "unable to connect stream: %s\n", hstrerror(h_errno));
            else
                fprintf(stderr, "unable to connect stream\n");
            break;
        case IESTREAMACCEPT:
            if (errno)
                fprintf(stderr, "unable to accept stream connection: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to accept stream connection\n");
            break;
        case IENONBLOCKING:
            if (errno)
                fprintf(stderr, "unable to set socket to non-blocking: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to set socket to non-blocking\n");
            break;
        case IEUPDATETIMER:
            if (errno)
                fprintf(stderr, "unable to update timer: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to update timer\n");
            break;
        case IENEWTIMER:
            if (errno)
                fprintf(stderr, "unable to create new timer: %s\n", strerror(errno));
            else
                fprintf(stderr, "unable to create new timer\n");
            break;
        case IEINITTEST:
            if (errno)
                fprintf(stderr, "test initialization failed: %s\n", strerror(errno));
            else
                fprintf(stderr, "test initialization failed\n");
            break;
    }
}
*/


