#include <stdlib.h>
#include <string.h>
#include "catch.hpp"
#include "test_callbacks.h"
#include "khc.h"

TEST_CASE( "slist append (1)" ) {
  khc_slist list;
  khc_slist_init(&list, khct::cb::cb_alloc, NULL, khct::cb::cb_free, NULL);
  int res = khc_slist_append(&list, "aaaaa", 5);
  REQUIRE( res == 0 );
  _khc_slist_node* appended = list.top;
  REQUIRE( appended != NULL );
  REQUIRE( strlen(appended->data) == 5 );
  REQUIRE( strncmp(appended->data, "aaaaa", 5) == 0 );
  REQUIRE ( appended->next == NULL );
  khc_slist_free_all(&list);
}

TEST_CASE( "slist append (2)" ) {
  khc_slist list;
  khc_slist_init(&list, khct::cb::cb_alloc, NULL, khct::cb::cb_free, NULL);
  int res = khc_slist_append(&list, "aaaaa", 5);
  REQUIRE( res == 0 );
  _khc_slist_node* appended = list.top;
  REQUIRE( appended != NULL );
  REQUIRE( strlen(appended->data) == 5 );
  REQUIRE( strncmp(appended->data, "aaaaa", 5) == 0 );
  REQUIRE( appended->next == NULL );
  res = khc_slist_append(&list, "bbbb", 4);
  REQUIRE( res == 0 );
  _khc_slist_node* next = appended->next;
  REQUIRE ( next != NULL );
  REQUIRE( strlen(next->data) == 4 );
  REQUIRE( strncmp(next->data, "bbbb", 4) == 0 );
  REQUIRE( next->next == NULL );
  khc_slist_free_all(&list);
}
