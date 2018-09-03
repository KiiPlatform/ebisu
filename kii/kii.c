#include "kii.h"
#include <string.h>

#define KII_SDK_INFO "sn=te;sv=1.2.4"

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

int kii_init(
        kii_t* kii,
        const char* site,
        const char* app_id,
        const char* app_key)
{
    memset(kii, 0x00, sizeof(kii_t));
    kii->_app_id = (char*)app_id;
    kii->_app_key = (char*)app_key;
        if(strcmp(site, "CN") == 0)
    {
        kii->_app_host = "api-cn2.kii.com";
    }
    else if(strcmp(site, "CN3") == 0)
    {
        kii->_app_host = "api-cn3.kii.com";
    }
    else if(strcmp(site, "JP") == 0)
    {
        kii->_app_host = "api-jp.kii.com";
    }
    else if(strcmp(site, "US") == 0)
    {
        kii->_app_host = "api.kii.com";
    }
    else if(strcmp(site, "SG") == 0)
    {
        kii->_app_host = "api-sg.kii.com";
    }
    else if (strcmp(site, "EU") == 0)
    {
        kii->_app_host = "api-eu.kii.com";
    }
    else
    {
        /* Let's enable to set custom host */
        kii->_app_host = (char*)site;
    }
    kii->_sdk_info = KII_SDK_INFO;
    khc_set_zero(&kii->_khc);
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii->_rw_buff);
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii->_rw_buff);
    return 0;
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

int kii_set_buff(kii_t* kii, char* buff, size_t buff_size) {
    kii->_rw_buff = buff;
    kii->_rw_buff_size = buff_size;
    // TODO: review API. It requires \0 terminated request body is already set.
    kii->_rw_buff_req_size = strlen(buff);
    kii->_rw_buff_read = 0;
    kii->_rw_buff_written = 0;
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii);
    return 0;
}