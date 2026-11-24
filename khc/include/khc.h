/**
 * \file khc.h
 * Kii HTTP clinet public API definitions.
 */
#ifndef KHC_H
#define KHC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>
#include <ebisu_log.h>
#include <ebisu_socket.h>
#include <ebisu_system.h>
/* The library is compiled with hidden visibility, so what this header declares
   is exactly what the shared object exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif


/* Sizes of the fixed request buffers. Each includes the terminating null, so
 * the longest value accepted is one less; the corresponding setter rejects
 * anything longer with KHC_ERR_TOO_LARGE_DATA rather than truncating. */

/** \brief Size of the host buffer written by khc_set_host(). */
#define KHC_HOST_MAX_SIZE 128
/** \brief Size of the path buffer written by khc_set_path(). */
#define KHC_PATH_MAX_SIZE 256
/** \brief Size of the method buffer written by khc_set_method(). */
#define KHC_METHOD_MAX_SIZE 16

/**
 * \brief How long khc waits, in milliseconds, before calling a socket
 * operation that returned EBISU_SOCK_AGAIN again, with the delay_ms of the
 * system set by khc_set_system(khc_t*, ebisu_system_t*).
 */
#ifndef KHC_AGAIN_WAIT_MS
#define KHC_AGAIN_WAIT_MS 10
#endif

/**
 * \brief Callback writes data.
 *
 * \param [in] buffer data to be written.
 * Note that it is not null terminated string.
 * \param [in] size data size to be written.
 * \param [in, out] userdata context data passed to khc_set_cb_write(khc_t*, KHC_CB_WRITE, void*)
 * \returns Size of the bytes written.
 * If the returned value is not equal to requested size,
 * khc aborts HTTP session and khc_perform(khc_t*) returns KHC_ERR_WRITE_CALLBACK.
 */
typedef size_t (*KHC_CB_WRITE)(char *buffer, size_t size, void *userdata);
/**
 * \brief Callback reads data.
 *
 * \param [out] buffer callback must writes data to this buffer.
 * \param [in] size requested read size: what is left of the stream buffer,
 * after the request's head the first time, less, for a chunked body, a few
 * bytes for each chunk's size line before it and CRLF after it. khc asks
 * again until the buffer is full or the body ends, and sends the buffer then:
 * what the callback gives waits until then.
 * \param [in, out] userdata context data passed to khc_set_cb_read(khc_t*, KHC_CB_READ, void*)
 * \returns Size of the bytes read, at most size, or khc_perform(khc_t*)
 * returns KHC_ERR_READ_CALLBACK.
 * Returning 0 indicates that the whole data is read.
 * khc repeatedly call this callback until it returns 0, or, for a body of the
 * length set by khc_set_req_body_length(khc_t*, size_t), until it has given
 * that much.
 */
typedef size_t (*KHC_CB_READ)(char *buffer, size_t size, void *userdata);
/**
 * \brief Callback used to propagate response headers.
 *
 * \param [in] buffer response header data.
 * Note that the buffer is not null terminated.
 * The buffer does not contains CRLF.
 * \param [in] size header data size.
 * \param [in, out] userdata context data passed to khc_set_cb_header(khc_t*, KHC_CB_HEADER, void*)
 * \returns Size of the bytes handled.
 * If it is not equal to Header size determined by size,
 * khc aborts HTTP session and khc_perform(khc_t*) returns KHC_ERR_HEADER_CALLBACK.
 */
typedef size_t (*KHC_CB_HEADER)(char *buffer, size_t size, void *userdata);
/**
 * \brief Callback deciding whether a request goes on.
 *
 * Called between the steps of khc_perform(khc_t*), each a socket operation or a
 * callback, so that another task can cancel a request: the implementation
 * reads a flag that task sets, under whatever lock their system has.
 *
 * \param [in, out] userdata context data passed to
 * khc_set_cb_continue(khc_t*, KHC_CB_CONTINUE, void*).
 * \returns true to go on, false to cancel the request: khc_perform(khc_t*) closes
 * the connection, if one is open or on the way, and returns KHC_ERR_CANCELLED.
 */
typedef bool (*KHC_CB_CONTINUE)(void *userdata);

/**
 * \brief Linked list.
 *
 * Linked list manages c string data.
 */
typedef struct khc_slist_t {
    char* data; /**< \brief Null terminated string */
    struct khc_slist_t* next; /**< \brief Pointer to the next node. */
} khc_slist_t;

/**
 * \brief Custom khc_slist_t node allocator.

 * In this allocator, you need to allocate memory of khc_slist_t struct and it's data char array.
 * data char array must be NULL terminated so it requires str_length + 1 as length.

 * \param [in] str khc_slist_t content. String must be copied to khc_slist_t.data.
 * \param [in] str_length length of the string (exclude NULL termination).
 * \param [in,out] data optional context data pointer. The pointer is given by
 * khc_slist_append_using_cb_alloc(khc_slist_t*, const char*, size_t, KHC_CB_SLIST_ALLOC, void*) method and could be NULL.
 */
typedef khc_slist_t*(*KHC_CB_SLIST_ALLOC)(const char* str, size_t str_length, void* data);

/**
 * \ brief free memory allocated by custom khc_slist_t node allocator.

 * In this callback, implementation must free memory allocated by single khc_slist_t node.
 * Method must be corresponding to alloc method implemented in KHC_CB_SLIST_ALLOC.

 * \param [in] node of the slist.
 * \param [in] data Context data pointer.
 */
typedef void(*KHC_CB_SLIST_FREE)(khc_slist_t* node, void* data);

/**
 * \brief Default implementation of KHC_CB_SLIST_ALLOC.
 */
khc_slist_t* khc_cb_slist_alloc(const char* str, size_t str_len, void* data);

/**
 * \brief Default implementation of KHC_CB_SLIST_FREE.
 */
void khc_cb_slist_free(khc_slist_t* slist, void* data);

/**
 * \brief Add node to the linked list.

 * This method uses default memory allocator uses malloc() for constructing string copy and khc_slist_t.
 * khc_slist_t must be appended by this method if the previous node is appended by this method.
 * khc_slist_free_all(khc_slist_t*) must be called to free all memories used by the list.
 * You can't use different allocate/ free method specified by
 * khc_slist_append_using_cb_alloc(khc_slist_t*, const char*, size_t length, KHC_CB_SLIST_ALLOC, void*)
 * in a single list.
 * \param [in, out] slist pointer to the linked list or NULL to create new linked list.
 * \param [in] string data to be appended. String is copied to new char array in slist.
 * \param [in] length of the string.
 * \returns pointer to the linked list (first node).
 */
khc_slist_t* khc_slist_append(khc_slist_t* slist, const char* string, size_t length);

/**
 * \brief Add node to the linked list. Node is allocated by specified allocator.

 * This method uses custom memory allocator for constructing string copy and khc_slist_t.
 * khc_slist_t must be appended by this method and same allocator if the previous node is appended by this method.
 * khc_slist_free_all_using_cb_free(khc_slist_t*, KHC_CB_SLIST_FREE, void*) and matching free callback
 * must be used to free all memories used by the list.
 * You can't use different allocate/ free method specified by
 * khc_slist_append_using_cb_alloc(khc_slist_t*, const char*, size_t length, KHC_CB_SLIST_ALLOC, void*)
 * in a single list.
 * \param [in, out] slist pointer to the linked list or NULL to create new linked list.
 * \param [in] string data to be appended. String is copied to new char array in slist.
 * \param [in] length of the string.
 * \param [in] cb_alloc allocator callback function.
 * \param [in] cb_alloc_data context data pointer passed to cb_alloc.
 * \returns pointer to the linked list (first node).
 */
khc_slist_t* khc_slist_append_using_cb_alloc(
        khc_slist_t* slist,
        const char* string,
        size_t length,
        KHC_CB_SLIST_ALLOC cb_alloc,
        void* cb_alloc_data);

/**
 * \brief Free memory used for the entire linked list.

 * Linked list constructed by khc_slist_append(khc_slist_t*, const char*, size_t) must be freed by this method.
 * Each node's string is cleared first, as a request header may carry a token.
 * \param [in, out] slist pointer to the linked list (first node).
 */
void khc_slist_free_all(khc_slist_t* slist);

/**
 * \brief Free memory used for the entire linked list constructed by custom allocator.
 *
 * Linked list constructed by khc_slist_append_using_cb_alloc(khc_slist_t*, const char*, size_t length, KHC_CB_SLIST_ALLOC, void*)
 * must be freed by this method and matching free callback. Each node's
 * string, which KHC_CB_SLIST_ALLOC copied, is cleared before cb_free is called
 * with the node, as a request header may carry a token.

 * \param [in, out] slist pointer to the linked list (first node).
 * \param [in] cb_free free callback.
 * \param [in] cb_free_data context object pointer passed to cb_free.
 */
void khc_slist_free_all_using_cb_free(
        khc_slist_t* slist,
        KHC_CB_SLIST_FREE cb_free,
        void* cb_free_data);

/**
 * \brief Indicate state of khc.
 *
 * No need to reference state of khc to run HTTP session.
 */
typedef enum khc_state_t {
    KHC_STATE_IDLE,
    KHC_STATE_CONNECT,
    /* Puts the request's head in the stream buffer, as much as fits. */
    KHC_STATE_REQ_LINE,
    /* No longer entered: the head goes out from the stream buffer, from
     * KHC_STATE_REQ_BODY_SEND. */
    KHC_STATE_REQ_HOST_HEADER,
    KHC_STATE_REQ_HEADER,
    KHC_STATE_REQ_HEADER_SEND,
    KHC_STATE_REQ_HEADER_SEND_CRLF,
    KHC_STATE_REQ_HEADER_END,
    /* Reads what fits of the body into the stream buffer, after what of the
     * head waits there. */
    KHC_STATE_REQ_BODY_READ,
    /* No longer entered: a chunk's size line is sent with it, from
     * KHC_STATE_REQ_BODY_SEND. */
    KHC_STATE_REQ_BODY_SEND_SIZE,
    /* Sends what the stream buffer holds: the head, the body, or both. */
    KHC_STATE_REQ_BODY_SEND,
    /* No longer entered: the CRLF ending a chunk is sent with it. */
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
} khc_state_t;

/**
 * \brief Error codes.
 */
typedef enum khc_code_t {
    /**< \brief Operation succeeded. */
    KHC_ERR_OK,
    /**< \brief Failure in connecting to server. */
    KHC_ERR_SOCK_CONNECT,
    /**< \brief No longer returned: closing doesn't change the result. See
     * khc_get_close_failed(khc_t*). */
    KHC_ERR_SOCK_CLOSE,
    /**< \brief Failure in sending data. */
    KHC_ERR_SOCK_SEND,
    /**< \brief Failure in receiving data, or the connection closed before
     * the response ended. */
    KHC_ERR_SOCK_RECV,
    /**< \brief Failure in handling response headers. */
    KHC_ERR_HEADER_CALLBACK,
    /**< \brief Failure in handling response body. */
    KHC_ERR_WRITE_CALLBACK,
    /**< \brief Memory allocation error. */
    KHC_ERR_ALLOCATION,
    /**< \brief Data is too large and doesn't fit to buffers statically
     * allocated, as a Content-Length or Transfer-Encoding header too long for
     * the buffer set by khc_set_resp_header_buff(khc_t*, char*, size_t) is. */
    KHC_ERR_TOO_LARGE_DATA,
    /**< \brief Other errors. */
    KHC_ERR_FAIL,
    /* Added after KHC_ERR_FAIL so that the codes above keep their values. */
    /**< \brief The read callback gave more than it was asked for, or ended
     * a body of the length set by khc_set_req_body_length(khc_t*, size_t)
     * early. */
    KHC_ERR_READ_CALLBACK,
    /**< \brief The request took longer than khc_set_timeout_ms(khc_t*,
     * uint32_t) allows. */
    KHC_ERR_TIMEOUT,
    /**< \brief The callback set by khc_set_cb_continue(khc_t*,
     * KHC_CB_CONTINUE, void*) cancelled the request. */
    KHC_ERR_CANCELLED,
    /**< \brief The response isn't one khc can read: its status line isn't
     * one, or its Content-Length, Transfer-Encoding or chunks don't say where
     * its body ends. */
    KHC_ERR_INVALID_RESPONSE,
} khc_code_t;

/**
 * \brief khc object.
 *
 * Reference/ change members of khc instance must be done by APIs takes
 * pointer of the khc object instance.
 * Do not reference/ change members directly.
 */
typedef struct khc_t {
    KHC_CB_WRITE _cb_write; /**< \private **/
    void* _write_data; /**< \private **/
    KHC_CB_READ _cb_read; /**< \private **/
    void* _read_data; /**< \private **/
    KHC_CB_HEADER _cb_header; /**< \private **/
    void* _header_data; /**< \private **/
    KHC_CB_CONTINUE _cb_continue; /**< \private **/
    void* _continue_data; /**< \private **/

    ebisu_system_t* _system; /**< \private **/
    /* Where what khc does is logged, or NULL. */
    ebisu_logger_t* _logger; /**< \private **/
    /* How much of the request's body has been read to send. */
    size_t _req_body_sent; /**< \private **/
    /* How long a request may take, or 0 for no limit. */
    uint32_t _timeout_ms; /**< \private **/
    /* When the request being performed started, by the system's clock. */
    uint64_t _started_ms; /**< \private **/
    /* Whether the last step's socket operation returned EBISU_SOCK_AGAIN. */
    bool _again; /**< \private **/
    /* Why the socket failed, as it said right after, and more of it. */
    ebisu_sock_error_t _sock_error; /**< \private **/
    long _sock_error_detail; /**< \private **/

    /** \private Request header list */
    khc_slist_t* _req_headers;

    char _host[KHC_HOST_MAX_SIZE]; /**< \private **/
    char _path[KHC_PATH_MAX_SIZE]; /**< \private **/
    char _method[KHC_METHOD_MAX_SIZE]; /**< \private **/

    /* State machine */
    khc_state_t _state; /**< \private **/

    ebisu_socket_t* _socket; /**< \private **/

    khc_slist_t* _current_req_header; /**< \private **/
    int _req_head_part; /**< \private **/
    size_t _req_head_offset; /**< \private **/

    char* _stream_buff; /**< \private **/
    size_t _stream_buff_size; /**< \private **/
    bool _stream_buff_allocated; /**< \private **/

    bool _read_req_end; /**< \private **/
    /** \private Whether the server answered before the request's body was
     * sent whole, and the rest isn't sent: RFC 9112, section 9.5. An interim
     * 1xx response, with nothing after it, has the rest sent after all. */
    bool _req_body_cut;
    /* The request body's length, or KHC_REQ_BODY_LENGTH_UNKNOWN to send it
     * chunked. */
    size_t _req_body_length; /**< \private **/
    /* How much of a body of known length is still to be read. */
    size_t _req_body_left; /**< \private **/
    /* Where in the stream buffer what is being sent of the body starts, and
     * its size: a chunk with its size line and CRLF, or plain data. */
    size_t _req_frame_start; /**< \private **/
    size_t _req_frame_size; /**< \private **/

    /* Response header buffer */
    char* _resp_header_buff; /**< \private **/
    size_t _resp_header_buff_size; /**< \private **/
    bool _resp_header_buff_allocated; /**< \private **/

    size_t _resp_header_read_size; /**< \private **/

    uint16_t _status_code; /**< \private **/
    /* Whether the response's status line has reached the header callback:
     * the lines after it are headers. */
    bool _resp_status_line_given; /**< \private **/
    /* Whether the status line said HTTP/1.0, which has no chunks. */
    bool _resp_version_1_0; /**< \private **/
    bool _chunked_resp; /**< \private **/
    size_t _chunk_size; /**< \private **/
    size_t _chunk_size_written; /**< \private **/
    size_t _resp_content_length; /**< \private **/
    bool _resp_has_content_length; /**< \private **/
    /* How much of the body has come, outside a chunked one. */
    size_t _resp_body_received; /**< \private **/
    /* How much of the CRLF CRLF that ends a chunked body has come. */
    size_t _trailer_end_matched; /**< \private **/
    /* The response's Date, as seconds since the epoch. -1 when there is none
     * yet, -2 when the response had a malformed one or several that
     * disagree. */
    int64_t _resp_date; /**< \private **/
    bool _read_end; /**< \private **/

    size_t _body_read_size; /**< \private **/

    size_t _sent_length; /**< \private **/

    khc_code_t _result; /**< \private **/
    bool _close_failed; /**< \private **/
    bool _resp_header_skipped; /**< \private **/

    bool _enable_insecure; /**< \private **/
    bool _absolute_target; /**< \private **/
    /* Dates earlier than this are refused: shortly before the source was
     * built. */
    int64_t _resp_date_floor; /**< \private **/
} khc_t;

/**
 * \brief Set initial value to members of khc.
 *
 * You need to call this method when start using khc instance.
 * \param [out] khc instance.
 * \see khc_reset_except_cb(khc_t*)
 */
void khc_init(khc_t* khc);

/**
 * \brief  Set initial value to members of khc other than callbacks.
 * 
 * The socket set by khc_set_socket(khc_t*, ebisu_socket_t*), and the callbacks/
 * userdata set by following methods remain untouched.
 * khc_set_cb_read(khc_t*, KHC_CB_READ, void*),
 * khc_set_cb_write(khc_t*, KHC_CB_WRITE, void*) and
 * khc_set_cb_header(khc_t*, KHC_CB_HEADER, void*)
 
 * You may use this method when you start new session and reuse same callbacks and userdata pointers.
 * \param [out] khc instance.
 * \see khc_init(khc_t*)
 */
void khc_reset_except_cb(khc_t* khc);

/**
 * \brief Prepare to perform the same request again.
 *
 * Resets what sending the last request and reading its response left behind,
 * so that the next khc_perform(khc_t*) sends the request again. The request
 * itself -- host, path, method, headers and the body length set by
 * khc_set_req_body_length(khc_t*, size_t) -- remains, as do callbacks/
 * userdata, the settings of khc_enable_insecure(khc_t*, bool) and
 * khc_enable_absolute_target(khc_t*, bool), and the buffers set
 * by khc_set_stream_buff(khc_t*, char*, size_t) and
 * khc_set_resp_header_buff(khc_t*, char*, size_t).
 *
 * The body is read again through the read callback, which must start it over.
 *
 * khc_perform(khc_t*) does the same when it starts, so the same request can be
 * performed again without this. What this adds is clearing the last
 * response's results, such as khc_get_status_code(khc_t*), before then.
 * \param [out] khc instance.
 * \see khc_reset_except_cb(khc_t*)
 */
void khc_reset_except_request(khc_t* khc);

/**
 * \brief Perform the HTTP session
 *
 * During the session, the socket set by khc_set_socket(khc_t*, ebisu_socket_t*),
 * and callback functions set by
 * khc_set_cb_read(khc_t*, KHC_CB_READ, void*),
 * khc_set_cb_write(khc_t*, KHC_CB_WRITE, void*) and
 * khc_set_cb_header(khc_t*, KHC_CB_HEADER, void*)
 * called.
 *
 * Each call starts from the request alone: what the last one's response left
 * behind is reset first, as by khc_reset_except_request(khc_t*). A body is
 * read through the read callback each time, which must start it over.
 *
 * A server may answer before the request's body is sent whole, with a 401 or
 * a 413 for instance, and close the connection. With a socket that has
 * recv_wait, khc looks for that before each part of the body, and after a
 * send that fails, as RFC 9112, section 9.5, has a client do: it stops
 * sending, reads the response, and returns as for any other, its status code
 * the server's, with the rest of the body neither read nor sent. An interim
 * 1xx response with nothing after it has the rest sent after all. Without
 * recv_wait, khc reads once the request is sent, and a send that fails fails
 * the request.
 *
 * This method blocks until the HTTP session ends.
 * \param [in, out] khc instance.
 * \returns KHC_ERR_OK if the response was read whole. KHC_ERR_FAIL, without
 * connecting, if the socket, one of its operations other than recv_timed_out,
 * the write callback or the header callback is not set. The read callback may be left out for a request with no body.
 * KHC_ERR_FAIL too, without connecting, for a buffer set with a size of 0 by
 * khc_set_resp_header_buff(khc_t*, char*, size_t) or
 * khc_set_stream_buff(khc_t*, char*, size_t).
 */
khc_code_t khc_perform(khc_t* khc);

/**
 * \brief Set the system: its clock times a request, and its delay waits for a
 * socket that isn't ready.
 *
 * With a delay_ms, khc waits KHC_AGAIN_WAIT_MS before calling a socket
 * operation that returned EBISU_SOCK_AGAIN again, rather than calling it again
 * at once, over and over. With a monotonic_ms, the timeout set by
 * khc_set_timeout_ms(khc_t*, uint32_t) can be used. khc_init(khc_t*) sets none,
 * and the resets leave it.
 *
 * \param [out] khc instance.
 * \param [in] system The system, or NULL. It has to outlive the requests.
 */
void khc_set_system(khc_t* khc, ebisu_system_t* system);

/**
 * \brief Set where khc logs what it does.
 *
 * At EBISU_LOG_DEBUG, each request as it goes: when connected, the request
 * line, each header, and how long the body is, then the response's status
 * line, each of its headers, and how long its body is. The headers are logged
 * as they are, credentials and cookies included, so a logger that takes
 * EBISU_LOG_DEBUG should write only where they would be safe. What a body
 * holds is never logged. At EBISU_LOG_WARNING, a request that fails, with its
 * khc_code_t, and a connection that didn't close. khc_init(khc_t*) sets none, and
 * the resets leave it. Built with EBISU_NO_LOG, khc logs nothing.
 *
 * \param [out] khc instance.
 * \param [in] logger The logger, or NULL for none. It has to outlive the
 * requests.
 */
void khc_set_logger(khc_t* khc, ebisu_logger_t* logger);

/**
 * \brief Set how long a request may take.
 *
 * From the start of khc_perform(khc_t*) to the end of the response. Once it has
 * passed, khc_perform(khc_t*) closes the connection, if one is open or on the
 * way, and returns KHC_ERR_TIMEOUT. It is checked between steps, so a socket operation that
 * blocks runs to its own timeout first: it bounds a response that trickles in
 * through receives that each end in time, or a slow handshake made of many.
 *
 * It needs the monotonic_ms of the system set by
 * khc_set_system(khc_t*, ebisu_system_t*): without one, khc_perform(khc_t*)
 * returns KHC_ERR_FAIL without connecting. khc_init(khc_t*) sets none, and the
 * resets leave it.
 *
 * \param [out] khc instance.
 * \param [in] timeout_ms The longest a request may take, in milliseconds, or 0
 * for no limit.
 */
void khc_set_timeout_ms(khc_t* khc, uint32_t timeout_ms);

/**
 * \brief Set the callback deciding whether a request goes on.
 *
 * khc_init(khc_t*) sets none, and the resets leave it.
 *
 * \param [out] khc instance.
 * \param [in] cb The callback, or NULL for none.
 * \param [in] userdata Given to cb.
 */
void khc_set_cb_continue(khc_t* khc, KHC_CB_CONTINUE cb, void* userdata);

/**
 * \brief Set host.
 *
 * Host consists of URL in request line.
 * \param [out] khc instance.
 * \param [in] host must be null terminated.
 * \return KHC_ERR_OK. KHC_ERR_TOO_LARGE_DATA for a host of KHC_HOST_MAX_SIZE
 * bytes or more, and KHC_ERR_FAIL for one with a space or a control character,
 * a CR or LF among them, which would end the header it is sent in: the host
 * is then left as it was.
 */
khc_code_t khc_set_host(khc_t* khc, const char* host);

/**
 * \brief Set path.
 *
 * Path consists of URL in request line. It is sent as given: what a URL can't
 * have as it is, the caller has percent-encoded.
 * \param [out] khc instance.
 * \param [in] path must be null terminated.
 * \return KHC_ERR_OK. KHC_ERR_TOO_LARGE_DATA for a path of KHC_PATH_MAX_SIZE
 * bytes or more, and KHC_ERR_FAIL for one with a space or a control character,
 * a CR or LF among them, which would end the path or the request line: the
 * path is then left as it was.
 */
khc_code_t khc_set_path(khc_t* khc, const char* path);

/**
 * \brief Set HTTP method.
 *
 * \param [out] khc instance.
 * \param [in] method must be null terminated.
 * \return KHC_ERR_OK. KHC_ERR_TOO_LARGE_DATA for a method of
 * KHC_METHOD_MAX_SIZE bytes or more, and KHC_ERR_FAIL for one with a space or
 * a control character: the method is then left as it was.
 */
khc_code_t khc_set_method(khc_t* khc, const char* method);

/**
 * \brief Set request headers.
 *
 * Each is a line, "Name: value", sent as given. khc_perform(khc_t*) returns
 * KHC_ERR_FAIL without connecting if one has a CR or LF in it, which would
 * make it two headers, or the end of them.
 *
 * \param [out] khc instance.
 * \param [in] headers list of request headers.
 */
khc_code_t khc_set_req_headers(khc_t* khc, khc_slist_t* headers);

/**
 * \brief Set response header buffer.
 *
 * Set response header buffer pointer used by KHC_CB_HEADER.
 * If this method is not called or set NULL to the buffer,
 * khc allocates memory of response header buffer when the HTTP session started
 * and free when the HTTP session ends.
 * The buffer allocated by the khc is 256 bytes.

 * The buffer is used to store single HTTP response header.
 * If header is larger than the buffer, the header is skipped and KHC_CB_HEADER is not called,
 * which khc_get_resp_header_skipped(khc_t*) tells.
 * khc needs to parse Status Line, Content-Length, Transfer-Encoding header to process HTTP message.
 * The buffer must have enough size to store those headers: khc_perform(khc_t*) returns
 * KHC_ERR_TOO_LARGE_DATA if it doesn't. 256 bytes would be enough.
 * If you set the buffer by the method, the method must be called befor khc_perform(khc_t*)
 * and memory used by the buffer can be safely freed after khc_perform(khc_t*) returned.

 * \param [out] khc instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer, at least 1 for a buffer that isn't
 * NULL: khc_perform(khc_t*) returns KHC_ERR_FAIL for 0.
 */
void khc_set_resp_header_buff(khc_t* khc, char* buffer, size_t buff_size);

/**
 * \brief Set stream buffer.
 *
 * Set stream buffer pointer used by KHC_CB_READ, KHC_CB_WRITE.
 * If this method is not called or set NULL to the buffer,
 * khc allocates memory of stream buffer when the HTTP session started
 * and free when the HTTP session ends.
 * The buffer allocated by khc is 1024 bytes.
 *
 * The request is sent a buffer at a time, in one write each: over TLS, at
 * least one record each. Its head goes first, and as much of the body as fits
 * after it, so that a small request is one write. 1024 bytes suit bodies of a
 * few KB. For larger
 * ones, on a slow or metered link, 4096 bytes or more send fewer, larger
 * records, and responses are read in larger pieces too. A chunked body needs
 * room for a chunk's size line and CRLF besides its data, a few bytes: with
 * no room for data, khc_perform(khc_t*) returns KHC_ERR_TOO_LARGE_DATA.
 * If you set the buffer by the method, the method must be called befor khc_perform(khc_t*)
 * and memory used by the buffer can be safely freed after khc_perform(khc_t*) returned.
 * khc_perform(khc_t*) clears it before it returns: the request's head goes
 * through it, its credentials among them, as do its body and the response's.
 *
 * \param [out] khc instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer, at least 1 for a buffer that isn't
 * NULL: khc_perform(khc_t*) returns KHC_ERR_FAIL for 0.
 */
void khc_set_stream_buff(khc_t* khc, char* buffer, size_t buff_size);

/**
 * \brief Set the socket requests are sent on.
 *
 * Its send operation is given the stream buffer: once for a request that fits
 * in it, head and body, and more times for a larger one. Its recv operation is
 * called until the length of data it reads is 0. Each request is sent with
 * "Connection: Close", so once connected, its close operation is called at the
 * end of every request. It is also called for a connect that returned
 * EBISU_SOCK_AGAIN, should the request time out or be cancelled before the
 * connect ends; not for one that returned EBISU_SOCK_FAIL.
 * \param [out] khc instance.
 * \param [in] socket The socket. It has to outlive khc, or be replaced first.
 */
void khc_set_socket(khc_t* khc, ebisu_socket_t* socket);

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
khc_code_t khc_set_cb_read(
        khc_t* khc,
        KHC_CB_READ cb,
        void* userdata);

/** \brief The request body's length is unknown: it is sent chunked. */
#define KHC_REQ_BODY_LENGTH_UNKNOWN ((size_t)-1)

/**
 * \brief Set the length of the request body.
 *
 * With the length known, the body is sent with a Content-Length header rather
 * than chunked, which some intermediaries handle poorly. The read callback set
 * by khc_set_cb_read(khc_t*, KHC_CB_READ, void*) is asked for no more than is
 * left of it, and has to give all of it: returning 0 before then fails the
 * request with KHC_ERR_READ_CALLBACK, as the server would wait for the rest.
 *
 * khc_reset_except_cb(khc_t*) sets it back to KHC_REQ_BODY_LENGTH_UNKNOWN, and
 * khc_reset_except_request(khc_t*) keeps it, as part of the request.
 *
 * \param [out] khc instance.
 * \param [in] length The body's length in bytes, or
 * KHC_REQ_BODY_LENGTH_UNKNOWN to send it chunked, as by default.
 */
void khc_set_req_body_length(khc_t* khc, size_t length);

/**
 * \brief Set write callback.
 *
 * The callback is called while reading response body.
 * Callback would be called several times until the whole response body is written.
 * It is not called for a response without a body, such as an empty chunked
 * one: khc_perform(khc_t*) returns as for any other response.
 * \param [out] khc instance.
 * \param [in] cb writes response body.
 * \param [in] userdata context data of the callback.
 */
khc_code_t khc_set_cb_write(
        khc_t* khc,
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
khc_code_t khc_set_cb_header(
        khc_t* khc,
        KHC_CB_HEADER cb,
        void* userdata);

/**
 * \brief Enable insecure connection.
 *
 * By default, khc uses secure connection.
 * If you need to use tcp instead of ssl/tls, you need to call this method.
 * \param [out] khc instance.
 * \param [in] enable_insecure true for an insecure connection, false (the default) for a secure one.
 */
void khc_enable_insecure(
        khc_t* khc,
        bool enable_insecure);

/**
 * \brief Send the whole URL in the request line.
 *
 * By default, khc sends the path, as in GET /v1/users HTTP/1.1, with the host in
 * the Host header, which is what a server expects. A forward proxy that a
 * request goes through in plain HTTP takes the URL instead, as in
 * GET http://api.example.com/v1/users HTTP/1.1, and needs this method.
 * \param [out] khc instance.
 * \param [in] enable_absolute_target true for the whole URL, false (the default)
 * for the path.
 */
void khc_enable_absolute_target(
        khc_t* khc,
        bool enable_absolute_target);

/**
 * \brief Get HTTP status code.
 *
 * \param [in] khc instance.
 * \returns the three-digit HTTP status code of the last khc_perform(khc_t*)'s
 * response, or 0 if it parsed none, or none has been performed since
 * khc_init(khc_t*), khc_reset_except_cb(khc_t*) or
 * khc_reset_except_request(khc_t*).
 */
uint16_t khc_get_status_code(
        khc_t* khc
);

/**
 * \brief Whether closing the socket failed.
 *
 * Closing comes last and doesn't change what khc_perform(khc_t*) returns: a
 * response read whole is still KHC_ERR_OK, and a request that failed keeps the
 * failure that ended it. The socket may still be open, though, so this is for
 * releasing it some other way.
 *
 * \param [in] khc instance.
 * \returns true if the close callback returned EBISU_SOCK_FAIL in the last
 * khc_perform(khc_t*), or EBISU_SOCK_AGAIN once the request had timed out or been
 * cancelled, when it isn't called again; false if it didn't, or none has been
 * performed since khc_init(khc_t*), khc_reset_except_cb(khc_t*) or
 * khc_reset_except_request(khc_t*).
 */
bool khc_get_close_failed(khc_t* khc);

/**
 * \brief Why the socket failed, in the last khc_perform(khc_t*).
 *
 * After KHC_ERR_SOCK_CONNECT, KHC_ERR_SOCK_SEND or KHC_ERR_SOCK_RECV, what the
 * socket's last_error said, right after the operation failed and before the
 * socket was closed: a host that didn't resolve, a certificate refused, a
 * timeout. EBISU_SOCK_ERROR_NONE for a socket without last_error, or when
 * nothing failed. EBISU_SOCK_ERROR_CLOSED, from khc itself, for a connection
 * that closed before the response ended.
 *
 * \param [in] khc instance.
 * \param [out] out_detail More of it, as the socket gave it, or 0. May be NULL.
 * \returns Why.
 */
ebisu_sock_error_t khc_get_sock_error(khc_t* khc, long* out_detail);

/**
 * \brief Whether a response header was skipped.
 *
 * A header too long for the buffer set by
 * khc_set_resp_header_buff(khc_t*, char*, size_t) doesn't reach KHC_CB_HEADER.
 * The response is otherwise read as usual, so this is how to tell that a header
 * is missing because of that and not from the response.
 *
 * \param [in] khc instance.
 * \returns true if the last khc_perform(khc_t*) skipped a header, false if it
 * didn't, or none has been performed since khc_init(khc_t*),
 * khc_reset_except_cb(khc_t*) or khc_reset_except_request(khc_t*).
 */
bool khc_get_resp_header_skipped(khc_t* khc);

/**
 * \brief Get the server's time, from the response's Date header.
 *
 * Servers state their time, to the second, in their responses. A device without
 * a real-time clock can take its time from there, to put timestamps in what it
 * reports for instance, or to check the validity of certificates once it has
 * talked to the server. The time is as trustworthy as the connection it came
 * through, so the socket callbacks must verify the server's certificate.
 *
 * The three forms of RFC 9110, section 5.6.7, are taken: the one servers must
 * send, "Sun, 06 Nov 1994 08:49:37 GMT", and the obsolete RFC 850 and asctime()
 * ones, "Sunday, 06-Nov-94 08:49:37 GMT" and "Sun Nov  6 08:49:37 1994". UTC is
 * taken for GMT. RFC 850's two-digit year is read as RFC 9110 asks, against
 * the year the library's source dates from. Anything else counts as no Date
 * header, and so does a response with several that disagree or one in another
 * form, a date earlier than the source of the library, which no server running
 * properly can send, or one more than EBISU_DATE_CEILING_YEARS after it, 15
 * unless the build sets another, 0 for no limit: a server's clock that far
 * ahead is wrong too.
 *
 * The date is cleared when khc_perform(khc_t*) starts, at each response's status
 * line, so that an interim 1xx response's never stands for the final one's, and
 * by khc_reset_except_cb(khc_t*) and khc_reset_except_request(khc_t*). It is when
 * the server sent the response: read it right after khc_perform(khc_t*), and count
 * from there with the device's own clock.
 *
 * \param [in] khc instance.
 * \returns Seconds since 1970-01-01T00:00:00Z when the response had a Date
 * header, -1 otherwise.
 */
int64_t khc_get_resp_date(khc_t* khc);

/**
 * \brief Check if the configured request method can have a request body.
 *
 * \param [in] khc instance.
 * \returns true if it can, false if it cannot.
 */
bool khc_method_can_have_body(khc_t* khc);

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif

#endif //KHC_H
