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
 * Note that it is not null terminated string.
 * \param [in] size data size to be written.
 * \param [in, out] userdata context data passed to khc_set_cb_write(khc*, KHC_CB_WRITE, void*)
 * \returns Size of the bytes written.
 * If the returned value is not equal to requested size,
 * khc aborts HTTP session and khc_perform(khc*) returns KHC_ERR_WRITE_CALLBACK.
 */
typedef size_t (*KHC_CB_WRITE)(char *buffer, size_t size, void *userdata);
/**
 * \brief Callback reads data.
 *
 * \param [out] buffer callback must writes data to this buffer.
 * \param [in] size requested read size.
 * \param [in, out] userdata context data passed to khc_set_cb_read(khc*, KHC_CB_READ, void*)
 * \returns Size of the bytes read.
 * Returning 0 indicates that the whole data is read.
 * khc repeatedly call this callback until it returns 0.
 */
typedef size_t (*KHC_CB_READ)(char *buffer, size_t size, void *userdata);
/**
 * \brief Callback used to propagate response headers.
 *
 * \param [in] buffer response header data.
 * Note that the buffer is not null terminated.
 * The buffer does not contains CRLF.
 * \param [in] size header data size.
 * \param [in, out] userdata context data passed to khc_set_cb_header(khc*, KHC_CB_HEADER, void*)
 * \returns Size of the bytes handled.
 * If it is not equal to Header size determined by size,
 * khc aborts HTTP session and khc_perform(khc*) returns KHC_ERR_HEADER_CALLBACK.
 */
typedef size_t (*KHC_CB_HEADER)(char *buffer, size_t size, void *userdata);

/**
 * \brief Linked list.
 *
 * Linked list manages c string data.
 */
typedef struct khc_slist {
    char* data; /**< \brief Null terminated string */
    struct khc_slist* next; /**< \brief Pointer to the next node. */
} khc_slist;

/**
 * \brief Custom khc_slist node allocator.

 * In this allocator, you need to allocate memory of khc_slist struct and it's data char array.
 * data char array must be NULL terminated so it requires str_length + 1 as length.

 * \param [in] str khc_slist content. String must be copied to khc_slist.data.
 * \param [in] str_length length of the string (exclude NULL termination).
 * \param [in,out] data optional context data pointer. The pointer is given by
 * khc_slist_append_using_cb_alloc(khc_slist*, const char*, size_t, KHC_CB_SLIST_ALLOC, void*) method and could be NULL.
 */
typedef khc_slist*(*KHC_CB_SLIST_ALLOC)(const char* str, size_t str_length, void* data);

/**
 * \ brief free memory allocated by custom khc_slist node allocator.

 * In this callback, implementation must free memory allocated by single khc_slist node.
 * Method must be corresponding to alloc method implemented in KHC_CB_SLIST_ALLOC.

 * \param [in] node of the slist.
 * \param [in] data Context data pointer.
 */
typedef void(*KHC_CB_SLIST_FREE)(khc_slist* node, void* data);

/**
 * \brief Default implementation of KHC_CB_SLIST_ALLOC.
 */
khc_slist* khc_cb_slist_alloc(const char* str, size_t str_len, void* data);

/**
 * \brief Default implementation of KHC_CB_SLIST_FREE.
 */
void khc_cb_slist_free(khc_slist* slist, void* data);

/**
 * \brief Add node to the linked list.

 * This method uses default memory allocator uses malloc() for constructing string copy and khc_slist.
 * khc_slist must be appended by this method if the previous node is appended by this method.
 * khc_slist_free_all(khc_slist*) must be called to free all memories used by the list.
 * You can't use different allocate/ free method specified by
 * khc_slist_append_using_cb_alloc(khc_slist*, const char*, size_t length, KHC_CB_SLIST_ALLOC, void*)
 * in a single list.
 * \param [in, out] slist pointer to the linked list or NULL to create new linked list.
 * \param [in] string data to be appended. String is copied to new char array in slist.
 * \param [in] length of the string.
 * \returns pointer to the linked list (first node).
 */
khc_slist* khc_slist_append(khc_slist* slist, const char* string, size_t length);

/**
 * \brief Add node to the linked list. Node is allocated by specified allocator.

 * This method uses custom memory allocator for constructing string copy and khc_slist.
 * khc_slist must be appended by this method and same allocator if the previous node is appended by this method.
 * khc_slist_free_all_using_cb_free(khc_slist*, KHC_CB_SLIST_FREE, void*) and matching free callback
 * must be used to free all memories used by the list.
 * You can't use different allocate/ free method specified by
 * khc_slist_append_using_cb_alloc(khc_slist*, const char*, size_t length, KHC_CB_SLIST_ALLOC, void*)
 * in a single list.
 * \param [in, out] slist pointer to the linked list or NULL to create new linked list.
 * \param [in] string data to be appended. String is copied to new char array in slist.
 * \param [in] length of the string.
 * \param [in] cb_alloc allocator callback function.
 * \param [in] cb_alloc_data context data pointer passed to cb_alloc.
 * \returns pointer to the linked list (first node).
 */
khc_slist* khc_slist_append_using_cb_alloc(
        khc_slist* slist,
        const char* string,
        size_t length,
        KHC_CB_SLIST_ALLOC cb_alloc,
        void* cb_alloc_data);

/**
 * \brief Free memory used for the entire linked list.

 * Linked list constructed by khc_slist_append(khc_slist*, const char*, size_t) must be freed by this method.
 * \param [in, out] slist pointer to the linked list (first node).
 */
void khc_slist_free_all(khc_slist* slist);

/**
 * \brief Free memory used for the entire linked list constructed by custom allocator.
 *
 * Linked list constructed by khc_slist_append_using_cb_alloc(khc_slist*, const char*, size_t length, KHC_CB_SLIST_ALLOC, void*)
 * must be freed by this method and matching free callback.

 * \param [in, out] slist pointer to the linked list (first node).
 * \param [in] cb_free free callback.
 * \param [in] cb_free_data context object pointer passed to cb_free.
 */
void khc_slist_free_all_using_cb_free(
        khc_slist* slist,
        KHC_CB_SLIST_FREE cb_free,
        void* cb_free_data);

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
    KHC_STATE_REQ_BODY_SEND_SIZE,
    KHC_STATE_REQ_BODY_SEND,
    KHC_STATE_REQ_BODY_SEND_CRLF,

    KHC_STATE_RESP_STATUS_READ,
    KHC_STATE_RESP_STATUS_PARSE,
    KHC_STATE_RESP_HEADER_CALLBACK,
    KHC_STATE_RESP_HEADER_READ,
    KHC_STATE_RESP_HEADER_SKIP,
    KHC_STATE_RESP_BODY_FRAGMENT,
    KHC_STATE_READ_CHUNK_SIZE_FROM_HEADER_BUFF,
    KHC_STATE_READ_CHUNK_BODY_FROM_HEADER_BUFF,

    /* Process fragment of body obtained when trying to find body boundary. */
    KHC_STATE_RESP_BODY_READ,
    KHC_STATE_RESP_BODY_CALLBACK,

    KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE,
    KHC_STATE_RESP_BODY_READ_CHUNK_SIZE,
    KHC_STATE_RESP_BODY_PARSE_CHUNK_BODY,
    KHC_STATE_RESP_BODY_READ_CHUNK_BODY,
    KHC_STATE_RESP_BODY_SKIP_CHUNK_BODY_CRLF,
    KHC_STATE_RESP_BODY_SKIP_TRAILERS,

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
    /**< \brief Data is too large and doesn't fit to buffers statically allocated. */
    KHC_ERR_TOO_LARGE_DATA,
    /**< \brief Other errors. */
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

    /* Response header buffer */
    char* _resp_header_buff; /**< \private **/
    size_t _resp_header_buff_size; /**< \private **/
    int _resp_header_buff_allocated; /**< \private **/

    size_t _resp_header_read_size; /**< \private **/

    int _status_code; /**< \private **/
    /* Pointer to the double CRLF boundary in the resp_header_buffer */
    char* _body_boundary; /**< \private **/

    /* Header callback */
    char* _cb_header_pos; /**< \private **/
    /* Used to seek for CRFL effectively. */
    size_t _cb_header_remaining_size; /**< \private **/

    char* _body_fragment; /**< \private **/
    size_t _body_fragment_size; /**< \private **/
    int _chunked_resp; /**< \private **/
    long _chunk_size; /**< \private **/
    long _chunk_size_written; /**< \private **/
    size_t _resp_content_length; /**< \private **/
    int _read_end; /**< \private **/

    size_t _body_read_size; /**< \private **/

    size_t _sent_length; /**< \private **/

    khc_code _result; /**< \private **/
} khc;

/**
 * \brief Set initial value to members of khc.
 *
 * You need to call this method when start using khc instance.
 * \param [out] khc instance.
 * \see khc_reset_except_cb(khc*)
 */
void khc_init(khc* khc);

/**
 * \brief  Set initial value to members of khc other than callbacks.
 * 
 * Callbacks/ userdata set by following methods remain untouched.
 * khc_set_cb_sock_connect(khc*, KHC_CB_SOCK_CONNECT, void*),
 * khc_set_cb_sock_send(khc*, KHC_CB_SOCK_SEND, void*),
 * khc_set_cb_sock_recv(khc*, KHC_CB_SOCK_RECV, void*),
 * khc_set_cb_sock_close(khc*, KHC_CB_SOCK_CLOSE, void*),
 * khc_set_cb_read(khc*, KHC_CB_READ, void*),
 * khc_set_cb_write(khc*, KHC_CB_WRITE, void*) and
 * khc_set_cb_header(khc*, KHC_CB_HEADER, void*)
 
 * You may use this method when you start new session and reuse same callbacks and userdata pointers.
 * \param [out] khc instance.
 * \see khc_init(khc*)
 */
void khc_reset_except_cb(khc* khc);

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
 * This method blocks until the HTTP session ends.
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
 * \brief Set response header buffer.
 *
 * Set response header buffer pointer used by KHC_CB_HEADER.
 * If this method is not called or set NULL to the buffer,
 * khc allocates memory of response header buffer when the HTTP session started
 * and free when the HTTP session ends.
 * The buffer allocated by the khc is 256 bytes.

 * The buffer is used to store single HTTP response header.
 * If header is larger than the buffer, the header is skipped and KHC_CB_HEADER is not called.
 * khc needs to parse Status Line, Content-Length, Transfer-Encoding header to process HTTP message.
 * The buffer must have enough size to store those headers. 256 bytes would be enough.
 * If you set the buffer by the method, the method must be called befor khc_perform(khc*)
 * and memory used by the buffer can be safely freed after khc_perform(khc*) returned.

 * \param [out] khc instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
void khc_set_resp_header_buff(khc* khc, char* buffer, size_t buff_size);

/**
 * \brief Set stream buffer.
 *
 * Set stream buffer pointer used by KHC_CB_READ, KHC_CB_WRITE.
 * If this method is not called or set NULL to the buffer,
 * khc allocates memory of stream buffer when the HTTP session started
 * and free when the HTTP session ends.
 * The buffer allocated by khc is 1024 bytes.

 * You can change the size of buffer depending on your request/ response size.
 * It must be enough large to store size line in chunked encoded message.
 * However, you may use much larger buffer since size line might require very small buffer
 * as it consists of HEX size and CRLF for the better performance.
 * If you set the buffer by the method, the method must be called befor khc_perform(khc*)
 * and memory used by the buffer can be safely freed after khc_perform(khc*) returned.
 *
 * \param [out] khc instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
void khc_set_stream_buff(khc* khc, char* buffer, size_t buff_size);

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
 * the length of data read by the callback is 0.
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
 * the length of data read by the callback is 0.
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
 * Callback would be called several times until the whole response body is written.
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
 * Callback would be called several times until all response headers are processed.
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
