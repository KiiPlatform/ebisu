#include <string.h>
#include "catch.hpp"
#include "khc.h"

TEST_CASE( "slist append (1)" ) {
    khc_slist* list = NULL;
    khc_slist* appended = khc_slist_append(list, "aaaaa", 5);
    REQUIRE( appended != NULL );
    REQUIRE( strlen(appended->data) == 5 );
    REQUIRE( strncmp(appended->data, "aaaaa", 5) == 0 );
    REQUIRE ( appended->next == NULL );
    khc_slist_free_all(appended);
}

TEST_CASE( "slist append (2)" ) {
    khc_slist* list = NULL;
    khc_slist* appended = khc_slist_append(list, "aaaaa", 5);
    REQUIRE( appended != NULL );
    REQUIRE( strlen(appended->data) == 5 );
    REQUIRE( strncmp(appended->data, "aaaaa", 5) == 0 );
    appended = khc_slist_append(appended, "bbbb", 4);
    khc_slist* next = appended->next;
    REQUIRE ( next != NULL );
    REQUIRE( strlen(next->data) == 4 );
    REQUIRE( strncmp(next->data, "bbbb", 4) == 0 );
    REQUIRE( next->next == NULL );
    khc_slist_free_all(appended);
}

typedef struct {
    const char* alloc_requested_str;
    size_t alloc_requested_str_len;
    khc_slist* free_requested_node;
} slist_alloc_ctx;

khc_slist* cb_alloc(const char* str, size_t len, void* data) {
    slist_alloc_ctx* ctx = (slist_alloc_ctx*)data;
    ctx->alloc_requested_str = str;
    ctx->alloc_requested_str_len = len;

    char* copy = (char*)malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    khc_slist* node = (khc_slist*)malloc(sizeof(khc_slist));
    if (node == NULL) {
        free(copy);
        return NULL;
    }
    strncpy(copy, str, len);
    copy[len] = '\0';
    node->data = copy;
    node->next = NULL;
    return node;
}

void cb_free(khc_slist* node, void* data) {
    slist_alloc_ctx* ctx = (slist_alloc_ctx*)data;
    ctx->free_requested_node = node;
    free(node->data);
    free(node);
}

TEST_CASE( "slist append (3)" ) {
    khc_slist* list = NULL;
    slist_alloc_ctx ctx;
    memset(&ctx, 0, sizeof(slist_alloc_ctx));
    const char str[] = "aaaaa";
    khc_slist* appended = khc_slist_append_using_cb_alloc(list, str, 5, cb_alloc, &ctx);
    REQUIRE( appended != NULL );
    REQUIRE( strlen(appended->data) == 5 );
    REQUIRE( strncmp(appended->data, str, 5) == 0 );
    REQUIRE ( appended->next == NULL );

    REQUIRE( ctx.alloc_requested_str == str );
    REQUIRE( ctx.alloc_requested_str_len == 5 );

    khc_slist_free_all_using_cb_free(appended, cb_free, &ctx);
    REQUIRE( ctx.free_requested_node == appended );
}
