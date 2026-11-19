#include <string.h>

#include "header_pool.h"

void header_pool_init(header_pool_t* pool)
{
    memset(pool, 0, sizeof(*pool));
}

khc_slist* header_pool_alloc(const char* str, size_t str_length, void* data)
{
    header_pool_t* pool = (header_pool_t*)data;
    if (pool->nodes_used == HEADER_POOL_NODES ||
            str_length >= sizeof(pool->text) - pool->text_used) {
        return NULL;
    }
    khc_slist* node = &pool->nodes[pool->nodes_used++];
    node->data = pool->text + pool->text_used;
    memcpy(node->data, str, str_length);
    node->data[str_length] = '\0';
    node->next = NULL;
    pool->text_used += str_length + 1;
    ++pool->live;
    return node;
}

void header_pool_free(khc_slist* node, void* data)
{
    header_pool_t* pool = (header_pool_t*)data;
    /* ebisu clears what the header held, a token included, before freeing it.
       Its space is taken again once the request's headers are all freed. */
    (void)node;
    if (--pool->live == 0) {
        pool->nodes_used = 0;
        pool->text_used = 0;
    }
}
