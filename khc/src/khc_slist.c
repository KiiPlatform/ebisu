#include <string.h>
#include <stdlib.h>
#include "khc.h"

khc_slist* khc_slist_append_using_cb_alloc(
        khc_slist* slist,
        const char* string,
        size_t length,
        KHC_CB_SLIST_ALLOC cb_alloc,
        void* cb_alloc_data) {
    if (cb_alloc == NULL) {
        return NULL;
    }
    khc_slist* next;
    next = cb_alloc(string, length, cb_alloc_data);
    if (next == NULL) {
        return NULL;
    }
    next->next = NULL;

    if (slist == NULL) {
        return next;
    }
    khc_slist* end = slist;
    while (end->next != NULL) {
        end = end->next;
    }
    end->next = next;
    return slist;
}

void khc_slist_free_all_using_cb_free(
        khc_slist* slist,
        KHC_CB_SLIST_FREE cb_free,
        void* cb_free_data) {
    if (cb_free == NULL) {
        return;
    }
    khc_slist *curr;
    curr = slist;
    while (curr != NULL) {
        khc_slist *next = curr->next;
        cb_free(curr, cb_free_data);
        curr = next;
    }
}
