#include "sys_cb_impl.h"
#include "linux-env/task_impl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void delay_ms_cb_impl(unsigned int msec, void* userdata)
{
    delay_ms_cb(msec, userdata);
}

kii_task_code_t task_create_cb_impl(
        const char* name,
        KII_TASK_ENTRY entry,
        void* param,
        void* userdata)
{
    return task_create_cb(name, entry, param, userdata);
}

khc_slist* khc_cb_slist_alloc(const char* str, size_t str_length, void* data) {
    char* copy = (char*)malloc(str_length + 1);
    if (copy == NULL) {
        return NULL;
    }
    khc_slist* node = (khc_slist*)malloc(sizeof(khc_slist));
    if (node == NULL) {
        free(copy);
        return NULL;
    }
    strncpy(copy, str, str_length);
    copy[str_length] = '\0';
    node->data = copy;
    node->next = NULL;
    return node;
}

void khc_cb_slist_free(khc_slist* node, void* data) {
    free(node->data);
    free(node);
}
