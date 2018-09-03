#include "kii.h"

size_t _cb_write_buff(char *buffer, size_t size, size_t count, void *userdata) {
  kii_t* kii = (kii_t*)userdata;
  size_t remain = kii->_rw_buff_size - kii->_rw_buff_written;
  size_t req_size = size * count;
  if (remain < req_size) {
    // Insufficient buffer size.
    return 0;
  }
  memcpy(kii->_rw_buff + kii->_rw_buff_written, buffer, req_size);
  kii->_rw_buff_written += req_size;
  return req_size;
}

size_t _cb_read_buff(char *buffer, size_t size, size_t count, void *userdata) {
  kii_t* kii = (kii_t*)userdata;
  size_t remain = kii->_rw_buff_req_size - kii->_rw_buff_read;
  if (remain <= 0) {
    return 0;
  }
  size_t to_read = (size * count > remain) ? (remain) : (size * count);
  memcpy(buffer, kii->_rw_buff + kii->_rw_buff_read, to_read);
  kii->_rw_buff_read += to_read;
  return to_read;
}

int kii_api_call(
    kii_t *kii,
    const char *http_method,
    const char *resource_path,
    const void *http_body,
    size_t body_size,
    const char *content_type,
    char *header,
    ...)
{
    // TODO: reimplement it. Equivalent to kii_core_api_call().
    return -1;
}
