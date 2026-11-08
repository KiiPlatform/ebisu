#include "khc.h"
#include "khc_impl.h"

#include <string.h>

void khc_set_logger(khc* khc, ebisu_logger_t* logger)
{
    khc->_logger = logger;
}

#ifndef EBISU_NO_LOG
void _khc_log_lines(khc* khc, char direction, const char* lines)
{
    const char* end;
    if (khc->_logger == NULL) {
        return;
    }
    while ((end = strstr(lines, "\r\n")) != NULL && end != lines) {
        _khc_log_header(khc, direction, lines, (size_t)(end - lines));
        lines = end + 2;
    }
}

const char* _khc_code_name(khc_code code)
{
    switch (code) {
    case KHC_ERR_OK: return "KHC_ERR_OK";
    case KHC_ERR_SOCK_CONNECT: return "KHC_ERR_SOCK_CONNECT";
    case KHC_ERR_SOCK_CLOSE: return "KHC_ERR_SOCK_CLOSE";
    case KHC_ERR_SOCK_SEND: return "KHC_ERR_SOCK_SEND";
    case KHC_ERR_SOCK_RECV: return "KHC_ERR_SOCK_RECV";
    case KHC_ERR_HEADER_CALLBACK: return "KHC_ERR_HEADER_CALLBACK";
    case KHC_ERR_WRITE_CALLBACK: return "KHC_ERR_WRITE_CALLBACK";
    case KHC_ERR_ALLOCATION: return "KHC_ERR_ALLOCATION";
    case KHC_ERR_TOO_LARGE_DATA: return "KHC_ERR_TOO_LARGE_DATA";
    case KHC_ERR_FAIL: return "KHC_ERR_FAIL";
    case KHC_ERR_READ_CALLBACK: return "KHC_ERR_READ_CALLBACK";
    case KHC_ERR_TIMEOUT: return "KHC_ERR_TIMEOUT";
    case KHC_ERR_CANCELLED: return "KHC_ERR_CANCELLED";
    case KHC_ERR_INVALID_RESPONSE: return "KHC_ERR_INVALID_RESPONSE";
    }
    return "an unknown khc_code";
}
#endif
