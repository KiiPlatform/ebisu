#include <string.h>
#include <stdio.h>
#include<stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <ctype.h>  


#include "kii.h"
#include "light.h"
#include "kii_demo.h"

volatile int toStop = 0;

void cfinish(int sig)
{
	signal(SIGINT, NULL);
	toStop = 1;
}


int main(int argc, char** argv)
{
	signal(SIGINT, cfinish);
	signal(SIGTERM, cfinish);

    if (kii_init(STR_SITE, STR_APPID, STR_APPKEY) < 0)
    {
       printf("Initialize kii failed\r\n");
       return 0;
    }

    if (light_onBoarding() != 0)
    {
    	    printf("Device onbording failed\r\n");
	    return 0;
    }

    if (light_initPush() < 0)
    {
        printf("Initialize push failed\r\n");
    }
    else
    {
	printf("Initialize push success\r\n");
    }

    while (!toStop)
    {
        sleep(1);	
    }

    return 0;
}


