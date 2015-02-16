#include <string.h>
#include "wm_include.h"

#include "kii.h"
#include "light.h"
#include "light_if.h"
#include "kii_demo.h"

volatile int toStop = 0;

int kiiDemo_test(char *buf)
{
    (void) buf;
	
    while (1)
    {
        if (light_init() < 0)
        {
            printf("Initialize light failed\r\n");
            OSTimeDly (10*OS_TICKS_PER_SEC);
            continue;
        }
        else
        {
            printf("Initialize light success\r\n");
	    return 0;
        }
    }
}


