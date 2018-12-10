#ifndef __khc_state_impl
#define __khc_state_impl

#ifdef __cplusplus
extern "C"
{
#endif

#include "khc.h"

#define DEFAULT_RESP_HEADER_BUFF_SIZE 256
#define DEFAULT_STREAM_BUFF_SIZE 1024

void khc_state_idle(khc* khc);
void khc_state_connect(khc* khc);
void khc_state_req_line(khc* khc);
void khc_state_req_host_header(khc* khc);
void khc_state_req_header(khc* khc);
void khc_state_req_header_send(khc* khc);
void khc_state_req_header_send_crlf(khc* khc);
void khc_state_req_header_end(khc* khc);
void khc_state_req_body_read(khc* khc);
void khc_state_req_body_send_size(khc* khc);
void khc_state_req_body_send(khc* khc);
void khc_state_req_body_send_crlf(khc* khc);

void khc_state_resp_status_read(khc* khc);
void khc_state_resp_status_parse(khc* khc);
void khc_state_resp_header_callback(khc* khc);
void khc_state_resp_header_read(khc* khc);
void khc_state_resp_header_skip(khc* khc);
void khc_state_resp_body_fragment(khc* khc);
void khc_state_read_chunk_size_from_header_buff(khc* khc);
void khc_state_read_chunk_body_from_header_buff(khc* khc);

void khc_state_resp_body_read(khc* khc);
void khc_state_resp_body_callback(khc* khc);

void khc_state_resp_body_parse_chunk_size(khc* khc);
void khc_state_resp_body_read_chunk_size(khc* khc);
void khc_state_resp_body_parse_chunk_body(khc* khc);
void khc_state_resp_body_read_chunk_body(khc* khc);
void khc_state_resp_body_skip_chunk_body_crlf(khc* khc);
void khc_state_resp_body_skip_trailers(khc* khc);

void khc_state_close(khc* khc);

typedef void (*KHC_STATE_HANDLER)(khc* khc);

extern const KHC_STATE_HANDLER state_handlers[];

#ifdef __cplusplus
}
#endif

#endif //__khc_state_impl
