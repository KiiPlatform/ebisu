#include <string.h>

#include "kii.h"
#include "light.h"
#include "light_if.h"
#include "kii_demo.h"

volatile int toStop = 0;

int kiiDemo_test(void)
{
	light_init();
	return 0;
}
