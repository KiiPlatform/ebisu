#include <string.h>
#include "khc.h"

khc_slist* _alloc_cb(const char* str, size_t str_len, void* data) {
  char* copy = malloc(str_len+1);
  if (copy == NULL) {
    return NULL;
  }
  khc_slist* node = malloc(sizeof(khc_slist));
  if (node == NULL) {
    free(copy);
    return NULL;
  }
  strncpy(copy, str, str_len);
  copy[str_len] = '\0';
  node->data = copy;
  node->next = NULL;
  return node;
}

void _free_cb(khc_slist* slist, void* data) {
  free(slist->data);
  free(slist);
}

khc_slist* khc_slist_append(khc_slist* slist, const char* string, size_t length) {
  return khc_slist_append_using_alloc_cb(slist, string, length, _alloc_cb, NULL);
}

khc_slist* khc_slist_append_using_alloc_cb(
  khc_slist* slist,
  const char* string,
  size_t length,
  KHC_SLIST_ALLOC_CB alloc_cb,
  void* alloc_cb_data) {
  khc_slist* next;
  next = alloc_cb(string, length, alloc_cb_data);
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

void khc_slist_free_all(khc_slist* slist) {
  khc_slist_free_all_using_free_cb(slist, _free_cb, NULL);
}

void khc_slist_free_all_using_free_cb(
  khc_slist* slist,
  KHC_SLIST_FREE_CB free_cb,
  void* free_cb_data) {
  khc_slist *curr;
  curr = slist;
  while (curr != NULL) {
    khc_slist *next = curr->next;
    free_cb(curr, free_cb_data);
    curr = next;
  }
}