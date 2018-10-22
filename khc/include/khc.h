/**
 * \file khc.h
 * Kii HTTP clinet public API definitions.
 */
#ifndef __khc
#define __khc

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include "khc_socket_callback.h"

/**
 * \brief Callback writes data.
 *
 * \param [in] buffer data to be written.
 * Note that it is not null terminated string. Size must be determined by the product of size and count.
 * \param [in] size block size.
 * \param [in] count number of blocks. buffer size is determined by size * count.
 * \param [in, out] userdata context data passed to khc_set_cb_write(khc*, KHC_CB_WRITE, void*)
 * \returns Size of the bytes written.
 * If the returned value is not equal to requested size,
 * khc aborts HTTP session and khc_perform(khc*) returns KHC_ERR_WRITE_CALLBACK.
 */
typedef size_t (*KHC_CB_WRITE)(char *buffer, size_t size, size_t count, void *userdata);
/**
 * \brief Callback reads data.
 *
 * \param [out] buffer callback must writes data to this buffer.
 * \param [in] size block size.
 * \param [in] count number of blocks. Requested read size is determined by size * count.
 * \param [in, out] userdata context data passed to khc_set_cb_read(khc*, KHC_CB_READ, void*)
 * \returns Size of the bytes read.
 * Returning 0 indicates that the whole data is read.
 * khc repeatedly call this callback untill it returns 0.
 */
typedef size_t (*KHC_CB_READ)(char *buffer, size_t size, size_t count, void *userdata);
/**
 * \brief Callback used to propagate response headers.
 *
 * \param [in] buffer response header data.
 * Note that the buffer is not null terminted and size must be determined by the product of size and count.
 * The buffer does not contains CRLF.
 * \param [in] size block size.
 * \param [in] count number of blocks. Header size is determined by size * count.
 * \param [in, out] userdata context data passed to khc_set_cb_header(khc*, KHC_CB_HEADER, void*)
 * \returns Size of the bytes handled.
 * If it is not equal to Header size determined by size * count,
 * khc aborts HTTP session and khc_perform(khc*) returns KHC_ERR_HEADER_CALLBACK.
 */
typedef size_t (*KHC_CB_HEADER)(char *buffer, size_t size, size_t count, void *userdata);

/**
 * \brief Linked list.
 *
 * Linked list manages c string data.
 */
typedef struct khc_slist {
  char* data; /**< \brief Null terminated string */
  struct khc_slist* next; /**< \brief Pointer to the next item. */
} khc_slist;

/**
 * \brief Add item to the linked list.
 *
 * \param [in, out] slist pointer to the linked list or NULL to create new linked list.
 * \param [in] string data to be appended.
 * \param [in] length of the string.
 * \returns pointer to the linked list (first item).
 */
khc_slist* khc_slist_append(khc_slist* slist, const char* string, size_t length);

/**
 * \brief Free memory used for the entire linked list.
 *
 * \param [in, out] slist pointer to the linked list (first item).
 */
void khc_slist_free_all(khc_slist* slist);

/**
 * \brief Indicate state of khc.
 *
 * No need to reference state of khc to run HTTP session.
 */
typedef enum khc_state {
  KHC_STATE_IDLE,
  KHC_STATE_CONNECT,
  KHC_STATE_REQ_LINE,
  KHC_STATE_REQ_HOST_HEADER,
  KHC_STATE_REQ_HEADER,
  KHC_STATE_REQ_HEADER_SEND,
  KHC_STATE_REQ_HEADER_SEND_CRLF,
  KHC_STATE_REQ_HEADER_END,
  KHC_STATE_REQ_BODY_READ,
  KHC_STATE_REQ_BODY_SEND,
  KHC_STATE_RESP_HEADERS_ALLOC,
  KHC_STATE_RESP_HEADERS_REALLOC,
  KHC_STATE_RESP_HEADERS_READ,
  KHC_STATE_RESP_STATUS_PARSE,
  KHC_STATE_RESP_HEADERS_CALLBACK,
  /* Process flagment of body obtaind when trying to find body boundary. */
  KHC_STATE_RESP_BODY_FLAGMENT,
  KHC_STATE_RESP_BODY_READ,
  KHC_STATE_RESP_BODY_CALLBACK,
  KHC_STATE_CLOSE,
  KHC_STATE_FINISHED,
} khc_state;

/**
 * \brief Error codes.
 */
typedef enum khc_code {
  /**< \brief Operation succeeded. */
  KHC_ERR_OK,
  /**< \brief Failure in connecting to server. */
  KHC_ERR_SOCK_CONNECT,
  /**< \brief Failure in closing connection. */
  KHC_ERR_SOCK_CLOSE,
  /**< \brief Failure in sending data. */
  KHC_ERR_SOCK_SEND,
  /**< \brief Failure in receiving data. */
  KHC_ERR_SOCK_RECV,
  /**< \brief Failure in handling response headers. */
  KHC_ERR_HEADER_CALLBACK,
  /**< \brief Failure in handling response body. */
  KHC_ERR_WRITE_CALLBACK,
  /**< \brief Memory allocation error. */
  KHC_ERR_ALLOCATION,
  /**< \brief Data is too large and doesn't fit to buffers staticaly sized. */
  KHC_ERR_TOO_LARGE_DATA,
  /**< \brief Uncategorized error. */
  KHC_ERR_FAIL,
} khc_code;

/**
 * \brief khc object.
 *
 * Reference/ change members of khc instance must be done by APIs takes
 * pointer of the khc object instance.
 * Do not reference/ change members directly.
 */
typedef struct khc {
  KHC_CB_WRITE _cb_write; /**< \private **/
  void* _write_data; /**< \private **/
  KHC_CB_READ _cb_read; /**< \private **/
  void* _read_data; /**< \private **/
  KHC_CB_HEADER _cb_header; /**< \private **/
  void* _header_data; /**< \private **/

  /** \private Request header list */
  khc_slist* _req_headers;

  char _host[128]; /**< \private **/
  char _path[256]; /**< \private **/
  char _method[16]; /**< \private **/

  /* State machine */
  khc_state _state; /**< \private **/

  /* Socket functions. */
  KHC_CB_SOCK_CONNECT _cb_sock_connect; /**< \private **/
  KHC_CB_SOCK_SEND _cb_sock_send; /**< \private **/
  KHC_CB_SOCK_RECV _cb_sock_recv; /**< \private **/
  KHC_CB_SOCK_CLOSE _cb_sock_close; /**< \private **/
  /* Socket context. */
  void* _sock_ctx_connect; /**< \private **/
  void* _sock_ctx_send; /**< \private **/
  void* _sock_ctx_recv; /**< \private **/
  void* _sock_ctx_close; /**< \private **/

  khc_slist* _current_req_header; /**< \private **/

  char* _stream_buff; /**< \private **/
  size_t _stream_buff_size; /**< \private **/
  int _stream_buff_allocated; /**< \private **/

  size_t _read_size; /**< \private **/
  int _read_req_end; /**< \private **/

  /* Response header buffer (Dynamic allocation) */
  char* _resp_header_buffer; /**< \private **/
  char* _resp_header_buffer_current_pos; /**< \private **/
  size_t _resp_header_buffer_size; /**< \private **/
  size_t _resp_header_read_size; /**< \private **/

  int _status_code; /**< \private **/
  /* Pointer to the double CRLF boundary in the resp_header_buffer */
  char* _body_boundary; /**< \private **/

  /* Header callback */
  char* _cb_header_pos; /**< \private **/
  /* Used to seek for CRFL effectively. */
  size_t _cb_header_remaining_size; /**< \private **/

  char* _body_flagment; /**< \private **/
  size_t _body_flagment_size; /**< \private **/
  int _read_end; /**< \private **/

  size_t _body_read_size; /**< \private **/

  khc_code _result; /**< \private **/
} khc;

/**
 * \brief Set members of khc 0/NULL.
 * 
 * You may use this method when you start new session.
 * \param [out] khc instance.
 * \see khc_set_zero_excl_cb(khc*)
 */
khc_code khc_set_zero(khc* khc);

/**
 * \brief Set members of khc 0/NULL.
 * 
 * However, callbacks/ userdata set by 
 * khc_set_cb_sock_connect(khc*, KHC_CB_SOCK_CONNECT, void*),
 * khc_set_cb_sock_send(khc*, KHC_CB_SOCK_SEND, void*),
 * khc_set_cb_sock_recv(khc*, KHC_CB_SOCK_RECV, void*),
 * khc_set_cb_sock_close(khc*, KHC_CB_SOCK_CLOSE, void*),
 * khc_set_cb_read(khc*, KHC_CB_READ, void*),
 * khc_set_cb_write(khc*, KHC_CB_WRITE, void*) and
 * khc_set_cb_header(khc*, KHC_CB_HEADER, void*)
 * remain untouched.
 * You may use this method when you start new session and reuse same callback and userdata pointer.
 * \param [out] khc instance.
 * \see khc_set_zero(khc*)
 */
khc_code khc_set_zero_excl_cb(khc* khc);

/**
 * \brief Perform the HTTP session
 *
 * During the session, callback functions set by
 * khc_set_cb_sock_connect(khc*, KHC_CB_SOCK_CONNECT, void*),
 * khc_set_cb_sock_send(khc*, KHC_CB_SOCK_SEND, void*),
 * khc_set_cb_sock_recv(khc*, KHC_CB_SOCK_RECV, void*),
 * khc_set_cb_sock_close(khc*, KHC_CB_SOCK_CLOSE, void*),
 * khc_set_cb_read(khc*, KHC_CB_READ, void*),
 * khc_set_cb_write(khc*, KHC_CB_WRITE, void*) and
 * khc_set_cb_header(khc*, KHC_CB_HEADER, void*)
 * called.
 * This method blocks untill the HTTP session ends.
 * \param [in, out] khc instance.
 */
khc_code khc_perform(khc* khc);

/**
 * \brief Set host.
 *
 * Host consists of URL in request line.
 * \param [out] khc instance.
 * \param [in] host must be null terminated.
 */
khc_code khc_set_host(khc* khc, const char* host);

/**
 * \brief Set path.
 *
 * Path consists of URL in request line.
 * \param [out] khc instance.
 * \param [in] path must be null terminated.
 */
khc_code khc_set_path(khc* khc, const char* path);

/**
 * \brief Set HTTP method.
 *
 * \param [out] khc instance.
 * \param [in] method must be null terminated.
 */
khc_code khc_set_method(khc* khc, const char* method);

/**
 * \brief Set request headers.
 *
 * \param [out] khc instance.
 * \param [in] headers list of request headers.
 */
khc_code khc_set_req_headers(khc* khc, khc_slist* headers);

/**
 * \brief Set stream buffer.
 *
 * Set stream buffer pointer used by KHC_CB_READ, KHC_CB_WRITE.
 * If this method is not called or set NULL to the buffer,
 * khc allocates memory of stream buffer when the HTTP session started
 * and free when the HTTP session ends.
 *
 * \param [out] khc instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
khc_code khc_set_stream_buff(khc* khc, char* buffer, size_t buff_size);

/**
 * \brief Set socket connect callback.
 *
 * \param [out] khc instance.
 * \param [in] cb called when socket connection to the server is required.
 * \param [in] userdata context data of the callback.
 */
khc_code khc_set_cb_sock_connect(
  khc* khc,
  KHC_CB_SOCK_CONNECT cb,
  void* userdata);

/**
 * \brief Set socket send callback.
 *
 * Callback would be called several times during the HTTP session depending on the size of the request.
 * \param [out] khc instance.
 * \param [in] cb called when send data to the connected server is required.
 * \param [in] userdata context data of the callback.
 */
khc_code khc_set_cb_sock_send(
  khc* khc,
  KHC_CB_SOCK_SEND cb,
  void* userdata);

/**
 * \brief Set socket recv callback.
 *
 * Callback would be called several times until
 * the lenght of data read by the callback is 0.
 * \param [out] khc instance.
 * \param [in] cb called when receive data from the connected server is required.
 * \param [in] userdata context data of the callback.
 */
khc_code khc_set_cb_sock_recv(
  khc* khc,
  KHC_CB_SOCK_RECV cb,
  void* userdata);

/**
 * \brief Set socket close callback.
 *
 * NOTE: Currently khc uses HTTP 1.0 and close callback is called each HTTP session.
 * \param [out] khc instance.
 * \param [in] cb called when HTTP session ends.
 * \param [in] userdata context data of the callback.
 */
khc_code khc_set_cb_sock_close(
  khc* khc,
  KHC_CB_SOCK_CLOSE cb,
  void* userdata);

/**
 * \brief Set read callback.
 *
 * The callback is called to read request body data.
 * Callback would be called several times until
 * the lenght of data read by the callback is 0.
 * \param [out] khc instance.
 * \param [in] cb reads request body.
 * \param [in] userdata context data of the callback.
 */
khc_code khc_set_cb_read(
  khc* khc,
  KHC_CB_READ cb,
  void* userdata);

/**
 * \brief Set write callback.
 *
 * The callback is called while reading response body.
 * Callback would be called several times untill the whole response body is written.
 * \param [out] khc instance.
 * \param [in] cb writes response body.
 * \param [in] userdata context data of the callback.
 */
khc_code khc_set_cb_write(
  khc* khc,
  KHC_CB_WRITE cb,
  void* userdata);

/**
 * \brief Set header callback.
 *
 * The callback is called while reading response headers.
 * Callback would be called several times untill all response headers are processed.
 * \param [out] khc instance.
 * \param [in] cb response header callback.
 * \param [in] userdata context data of the callback.
 */
khc_code khc_set_cb_header(
  khc* khc,
  KHC_CB_HEADER cb,
  void* userdata);

/**
 * \brief Get HTTP status code.
 *
 * \param [in] khc instance.
 * \returns HTTP status code.
 */
int khc_get_status_code(
  khc* khc
);

#ifdef __cplusplus
}
#endif

#endif //__khc
