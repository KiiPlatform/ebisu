#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "khc.h"
#include "khc_state_impl.h"
#include "khc_socket_callback.h"

void khc_slist_memory_callbacks_init(
    khc_slist_memory_callbacks* mc,
    KHC_SLIST_CB_ALLOC cb_alloc,
    void* alloc_data,
    KHC_SLIST_CB_FREE cb_free,
    void* free_data)
{
  if (mc != NULL) {
    mc->cb_alloc = cb_alloc;
    mc->alloc_data = alloc_data;
    mc->cb_free = cb_free;
    mc->free_data = free_data;
  }
}

void* _slist_default_alloc(size_t size, void* userdata) {
  return malloc(size);
}

void _slist_default_free(void* ptr, void* userdata) {
  free(ptr);
}

khc_slist* khc_slist_append(khc_slist* slist, const char* string, size_t length, khc_slist_memory_callbacks* mc) {
  KHC_SLIST_CB_ALLOC cb_alloc = (mc != NULL && mc->cb_alloc != NULL) ? mc->cb_alloc : _slist_default_alloc;
  void* cb_alloc_data = mc != NULL ? mc->alloc_data : NULL;
  KHC_SLIST_CB_FREE cb_free = (mc != NULL && mc->cb_free != NULL) ? mc->cb_free : _slist_default_free;
  void* cb_free_data = mc != NULL ? mc->free_data : NULL;

  khc_slist* next;
  next = (khc_slist*)cb_alloc(sizeof(khc_slist), cb_alloc_data);
  if (next == NULL) {
    return NULL;
  }
  next->next = NULL;
  void* temp = cb_alloc(length+1, cb_alloc_data);
  if (temp == NULL) {
    cb_free(next, cb_free_data);
    return NULL;
  }
  next->data = (char*)temp;
  strncpy(next->data, string, length);
  next->data[length] = '\0';
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

khc_code khc_set_resp_header_buff(khc* khc, char* buffer, size_t buff_size) {
  khc->_resp_header_buff = buffer;
  khc->_resp_header_buff_size = buff_size;
  return KHC_ERR_OK;
}

khc_code khc_set_stream_buff(khc* khc, char* buffer, size_t buff_size) {
  khc->_stream_buff = buffer;
  khc->_stream_buff_size = buff_size;
  return KHC_ERR_OK;
}

void khc_slist_free_all(khc_slist* slist, khc_slist_memory_callbacks* mc) {
  KHC_SLIST_CB_FREE cb_free = (mc != NULL && mc->cb_free != NULL) ? mc->cb_free : _slist_default_free;
  void* cb_free_data = mc != NULL ? mc->free_data : NULL;

  khc_slist *curr;
  curr = slist;
  while (curr != NULL) {
    khc_slist *next = curr->next;
    cb_free(curr->data, cb_free_data);
    curr->data = NULL;
    cb_free(curr, cb_free_data);
    curr = next;
  }
}

khc_code khc_perform(khc* khc) {
  khc->_state = KHC_STATE_IDLE;
  while(khc->_state != KHC_STATE_FINISHED) {
    state_handlers[khc->_state](khc);
  }
  khc_code res = khc->_result;
  khc->_state = KHC_STATE_IDLE;
  khc->_result = KHC_ERR_OK;
  
  return res;
}

khc_code khc_set_zero(khc* khc) {
  // Callbacks.
  khc->_cb_write = NULL;
  khc->_write_data = NULL;

  khc->_cb_header = NULL;
  khc->_header_data = NULL;

  khc->_cb_read = NULL;
  khc->_read_data = NULL;

  khc->_cb_sock_connect = NULL;
  khc->_sock_ctx_connect = NULL;

  khc->_cb_sock_send = NULL;
  khc->_sock_ctx_send = NULL;

  khc->_cb_sock_recv = NULL;
  khc->_sock_ctx_recv = NULL;

  khc->_cb_sock_close = NULL;
  khc->_sock_ctx_close = NULL;

  khc_set_zero_excl_cb(khc);

  return KHC_ERR_OK;
}

khc_code khc_set_zero_excl_cb(khc* khc) {
  khc->_req_headers = NULL;
  khc->_host[0] = '\0';
  khc->_path[0] = '\0';
  khc->_method[0] = '\0';

  // Internal states.
  khc->_state = KHC_STATE_IDLE;
  khc->_current_req_header = NULL;
  khc->_read_size = 0;
  khc->_read_req_end = 0;
  khc->_resp_header_read_size = 0;
  khc->_status_code =0;
  khc->_body_boundary = NULL;
  khc->_cb_header_pos = NULL;
  khc->_cb_header_remaining_size = 0;
  khc->_body_flagment = NULL;
  khc->_body_flagment_size = 0;
  khc->_chunked_resp = 0;
  khc->_chunk_size = 0;
  khc->_chunk_size_written = 0;
  khc->_resp_content_length = 0;
  khc->_read_end = 0;
  khc->_body_read_size = 0;
  khc->_result = KHC_ERR_OK;

  // Response header Buffer
  khc->_resp_header_buff = NULL;
  khc->_resp_header_buff_size = 0;
  khc->_resp_header_buff_allocated = 0;
  // Stream Buffer
  khc->_stream_buff = NULL;
  khc->_stream_buff_size = 0;
  khc->_stream_buff_allocated = 0;
  return KHC_ERR_OK;
}

int khc_get_status_code(
  khc* khc
) {
  return khc->_status_code;
}
