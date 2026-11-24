#ifndef KHC_STATE_IMPL_H
#define KHC_STATE_IMPL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "khc.h"

#define DEFAULT_RESP_HEADER_BUFF_SIZE 256
#define DEFAULT_STREAM_BUFF_SIZE 1024

/* Room for the line that opens a chunk: every hex digit of a size_t, CRLF and
 * the terminator. */
#define KHC_CHUNK_SIZE_LINE_SIZE (sizeof(size_t) * 2 + 3)

/* Writes size in lowercase hex followed by CRLF, terminated, and returns its
 * length without the terminator. */
size_t _khc_chunk_size_line(size_t size, char out[KHC_CHUNK_SIZE_LINE_SIZE]);

void khc_state_idle(khc_t* khc);
void khc_state_connect(khc_t* khc);
void khc_state_req_line(khc_t* khc);
void khc_state_req_body_read(khc_t* khc);
void khc_state_req_body_send(khc_t* khc);

void khc_state_resp_status_read(khc_t* khc);
void khc_state_resp_status_parse(khc_t* khc);
void khc_state_resp_header_callback(khc_t* khc);
void khc_state_resp_header_read(khc_t* khc);
void khc_state_resp_header_skip(khc_t* khc);
void khc_state_resp_body_fragment(khc_t* khc);
void khc_state_read_chunk_size_from_header_buff(khc_t* khc);
void khc_state_read_chunk_body_from_header_buff(khc_t* khc);

void khc_state_resp_body_read(khc_t* khc);
void khc_state_resp_body_callback(khc_t* khc);

void khc_state_resp_body_parse_chunk_size(khc_t* khc);
void khc_state_resp_body_read_chunk_size(khc_t* khc);
void khc_state_resp_body_parse_chunk_body(khc_t* khc);
void khc_state_resp_body_read_chunk_body(khc_t* khc);
void khc_state_resp_body_skip_chunk_body_crlf(khc_t* khc);
void khc_state_resp_body_skip_trailers(khc_t* khc);

void khc_state_close(khc_t* khc);

typedef void (*KHC_STATE_HANDLER)(khc_t* khc);

extern const KHC_STATE_HANDLER state_handlers[];

#ifdef __cplusplus
}
#endif

#endif //KHC_STATE_IMPL_H
