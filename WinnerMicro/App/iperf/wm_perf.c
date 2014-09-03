#include "wm_include.h"
#include "wm_config.h"
#include "iperf.h"

#if TLS_CONFIG_WIFI_PERF_TEST

#define THT_QUEUE_SIZE	32
#define THT_TASK_PRIO	32

struct tht_param gThtSys;
OS_EVENT *tht_q = NULL;
void *tht_queue[THT_QUEUE_SIZE];
OS_STK ThtTaskStk[512]; 

void tht_task(void *sdata)
{
	void *tht = (struct tht_param *)sdata;
	void *msg;
	INT8U error;
	for(;;) 
	{
		msg = OSQPend(tht_q, 0, &error);
		//printf("\n msg =%d\n",msg);
		switch((u32)msg)
		{
			case TLS_MSG_WIFI_PERF_TEST_START:
				printf("\nTHT_TEST_START\n");
				tls_perf(tht);
				break;
			default:
				break;
		}
	}

}


void CreateThroughputTask(void)
{
	INT8U error;
	
	memset(&gThtSys, 0 ,sizeof(struct tht_param));
	tht_q = OSQCreate(&tht_queue, THT_QUEUE_SIZE);
	OSTaskCreate(tht_task, (void *)&gThtSys, (void *)&ThtTaskStk[512 - 1], THT_TASK_PRIO);
	
}

#endif 

