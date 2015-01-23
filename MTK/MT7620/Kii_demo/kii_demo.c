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


#include "kii.h"
#include "light.h"
#include "light_if.h"
#include "kii_demo.h"

volatile int toStop = 0;

void cfinish(int sig)
{
    signal(SIGINT, NULL);
    toStop = 1;
}


int main(int argc, char** argv)
{
    int initState = 0;
    //light_struct light;
	
    signal(SIGINT, cfinish);
    signal(SIGTERM, cfinish);

    while (!toStop)
    {
        if (initState == 0)
        {
            if (light_init() < 0)
            {
                printf("Initialize light failed\r\n");
                sleep(10);	
                continue;
            }
            else
            {
		printf("Initialize light success\r\n");
                initState = 1;
            }
        }
        
	//lightIf_status(&light);
        //lightIf_updateStatus(light);
	sleep(5);	
    }

    return 0;
}


