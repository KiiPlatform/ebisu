/** \file khc_socket_callback.h
 * Provides socket abstraction.
 */
#ifndef _KHC_SOCKET_CALLBACK
#define _KHC_SOCKET_CALLBACK

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Indicates socket callback result */
typedef enum khc_sock_code_t {
    /** \brief Retrun this code when operation succeed. */
    KHC_SOCK_OK,

    /** \brief Return this code when operation failed. */
    KHC_SOCK_FAIL,

    /** \brief Return this code when operation is in progress.
     *
     * This code is used when the underlying socket is non-bloking mode.
     * khc calls the callback again with the same argument 
     * until the callbacks returns KHC_SOCK_OK or KHC_SOCK_FAIL.
     */
    KHC_SOCK_AGAIN
} khc_sock_code_t;

/** \brief Callback function connects socket to server.
 *
 * Applications must implement this callback in the target enviroment.
 *
 * @param [in] sock_ctx context object.
 * @param [in] host host name.
 * @param [in] port port number.
 * @returns khc_sock_code_t
 */
typedef khc_sock_code_t
    (*KHC_CB_SOCK_CONNECT)
    (void* sock_ctx, const char* host, unsigned int port);

/** \brief Callback function sends data to server.
 *
 * Applications must implement this callback in the target enviroment.
 * This callback is called repeatedly in the HTTP session
 * untill the whole request is sent.
 *
 * @param [in] sock_ctx context object.
 * @param [in] buffer data to send server.
 * @param [in] length length of buffer.
 * @param [out] out_sent_length data size sent to server.
 * @returns khc_sock_code_t
 */
typedef khc_sock_code_t
    (*KHC_CB_SOCK_SEND)
    (void* sock_ctx, const char* buffer, size_t length, size_t* out_sent_length);

/** \brief Callback function receives data from server.
 *
 * Applications must implement this callback in the target enviroment.
 * This callback is called repeatedly in the HTTP session
 * untill out_actual_length value is set to 0.
 *
 * @param [in] sock_ctx context object.
 * @param [out] buffer data read from the socket must be written here.
 * @param [in] length_to_read Maximum size requested to read.
 * @param [out] out_actual_length actual data size read from the socket.
 * @returns khc_sock_code_t
 */
typedef khc_sock_code_t
    (*KHC_CB_SOCK_RECV)
    (void* sock_ctx, char* buffer, size_t length_to_read,
     size_t* out_actual_length);

/** \brief Callback closes socket.
 *
 * Applications must implement this callback in the target enviroment.
 *
 * @param [in] sock_ctx context object.
 * @returns khc_sock_code_t
 */
typedef khc_sock_code_t
    (*KHC_CB_SOCK_CLOSE)(void* sock_ctx);


#ifdef __cplusplus
}
#endif

#endif /* _KHC_SOCKET_CALBACK */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
