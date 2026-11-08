#ifndef HEADER_POOL_H
#define HEADER_POOL_H

#include <stddef.h>

#include <khc.h>

/* Request headers without the heap, for tio_handler_set_cb_slist_resource()
   and tio_updater_set_cb_slist_resource(). kii allocates a request's headers
   one at a time and frees them all once the request is done, so an arena that
   starts over when the last is freed is all it takes.

   A request whose token is refused keeps its headers while the session renews
   the token with a request of its own, then takes a new Authorization header.
   With the longest application ID and tokens kii takes, that is 11 headers
   and 900 bytes; with an ID of 8 characters and tokens of 43, about half the
   bytes. Running out fails the request with KII_ERR_ALLOCATION.

   One pool for each tio_handler_t and tio_updater_t: each is used by one task
   at a time, so the pool has no lock. */
#define HEADER_POOL_NODES 16
#define HEADER_POOL_TEXT_SIZE 1024

typedef struct {
    khc_slist nodes[HEADER_POOL_NODES];
    char text[HEADER_POOL_TEXT_SIZE];
    size_t nodes_used;
    size_t text_used;
    size_t live;
} header_pool_t;

void header_pool_init(header_pool_t* pool);

/* KHC_CB_SLIST_ALLOC and KHC_CB_SLIST_FREE, the pool their data. */
khc_slist* header_pool_alloc(const char* str, size_t str_length, void* pool);
void header_pool_free(khc_slist* node, void* pool);

#endif
