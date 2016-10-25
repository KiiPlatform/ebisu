#include "kii_task_impl.h"
#include "wm_include.h"
#include "kii.h"

#define  DEMO_KII_PUSH_RECV_MSG_TASK_PRIO	42
#define  DEMO_KII_PUSH_PINGREQ_TASK_PRIO	43

#define RECVMSG_TASK_STK_SIZE 2048
static unsigned int RecvMsgTaskStk[RECVMSG_TASK_STK_SIZE];
#ifdef KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS
#define PINGREQ_TASK_STK_SIZE 256
static unsigned int PingReqTaskStk[PINGREQ_TASK_STK_SIZE];
#endif

kii_task_code_t task_create_cb
    (const char* name,
     KII_TASK_ENTRY entry,
     void* param)
{
    int ret;
    unsigned char* stk_start = NULL;
    unsigned int stk_size = 0;
    unsigned int priority = 0;

    if (strcmp(name, KII_TASK_NAME_RECV_MSG) == 0) {
        stk_start = RecvMsgTaskStk;
        stk_size = RECVMSG_TASK_STK_SIZE;
        priority = DEMO_KII_PUSH_RECV_MSG_TASK_PRIO;
#ifdef KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS
    } else if (strcmp(name, KII_TASK_NAME_PING_REQ) == 0) {
        stk_start = PingReqTaskStk;
        stk_size = PINGREQ_TASK_STK_SIZE;
        priority = DEMO_KII_PUSH_PINGREQ_TASK_PRIO;
#endif
    }

	ret = tls_os_task_create(NULL,name, entry, param, stk_start, stk_size, priority, 0);

    if(ret == TLS_OS_SUCCESS)
    {
        return KII_TASKC_OK;
    }
    else
    {
        return KII_TASKC_FAIL;
    }
}

void delay_ms_cb(unsigned int msec)
{
	tls_os_time_delay(msec*HZ/1000);
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
