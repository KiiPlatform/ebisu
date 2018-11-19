#include <string.h>
#include "catch.hpp"
#include "khc.h"
#include "test_callbacks.h"

TEST_CASE( "slist append (1)" ) {
  khc_slist* list = NULL;
  khc_slist* appended = khc_slist_append(list, "aaaaa", 5, NULL);
  REQUIRE( appended != NULL );
  REQUIRE( strlen(appended->data) == 5 );
  REQUIRE( strncmp(appended->data, "aaaaa", 5) == 0 );
  REQUIRE ( appended->next == NULL );
  khc_slist_free_all(appended, NULL);
}

TEST_CASE( "slist append (2)" ) {
  khc_slist* list = NULL;
  khc_slist* appended = khc_slist_append(list, "aaaaa", 5, NULL);
  REQUIRE( appended != NULL );
  REQUIRE( strlen(appended->data) == 5 );
  REQUIRE( strncmp(appended->data, "aaaaa", 5) == 0 );
  appended = khc_slist_append(appended, "bbbb", 4, NULL);
  khc_slist* next = appended->next;
  REQUIRE ( next != NULL );
  REQUIRE( strlen(next->data) == 4 );
  REQUIRE( strncmp(next->data, "bbbb", 4) == 0 );
  REQUIRE( next->next == NULL );
  khc_slist_free_all(appended, NULL);
}

TEST_CASE( "slist append (3)" ) {
  khct::cb::MemCtx ctx;
  khc_slist_memory_callbacks mc;
  khc_slist_memory_callbacks_init(&mc, khct::cb::cb_alloc, &ctx, khct::cb::cb_free, &ctx);
  int on_alloc_called = 0;
  ctx.on_alloc = [=, &on_alloc_called](size_t size, void* userdata) {
    ++on_alloc_called;
    return malloc(size);
  };
  int on_free_called = 0;
  ctx.on_free = [=, &on_free_called](void* ptr, void* userdata) {
    ++on_free_called;
    free(ptr);
  };

  khc_slist* list = NULL;
  khc_slist* appended = khc_slist_append(list, "aaaaa", 5, &mc);
  REQUIRE( appended != NULL );
  REQUIRE( strlen(appended->data) == 5 );
  REQUIRE( strncmp(appended->data, "aaaaa", 5) == 0 );
  REQUIRE ( appended->next == NULL );
  khc_slist_free_all(appended, &mc);

  REQUIRE( on_alloc_called == 2 );
  REQUIRE( on_free_called == 2 );
}

