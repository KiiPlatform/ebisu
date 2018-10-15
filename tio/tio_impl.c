#include "tio_impl.h"

tio_code_t _tio_convert_code(kii_code_t code) {
    switch(code) {
    case KII_ERR_OK:
        return TIO_ERR_OK;
    case KII_ERR_SOCK_CONNECT:
        return TIO_ERR_SOCK_CONNECT;
    case KII_ERR_SOCK_CLOSE:
        return TIO_ERR_SOCK_CLOSE;
    case KII_ERR_SOCK_SEND:
        return TIO_ERR_SOCK_SEND;
    case KII_ERR_SOCK_RECV:
        return TIO_ERR_SOCK_RECV;
    case KII_ERR_HEADER_CALLBACK:
        return TIO_ERR_HEADER_CALLBACK;
    case KII_ERR_WRITE_CALLBACK:
        return TIO_ERR_WRITE_CALLBACK;
    case KII_ERR_ALLOCATION:
        return TIO_ERR_ALLOCATION;
    case KII_ERR_TOO_LARGE_DATA:
        return TIO_ERR_TOO_LARGE_DATA;
    case KII_ERR_RESP_STATUS:
        return TIO_ERR_RESP_STATUS;
    case KII_ERR_PARSE_JSON:
        return TIO_ERR_PARSE_JSON;
    case KII_ERR_FAIL:
    default:
        return TIO_ERR_FAIL;
    }
    return TIO_ERR_FAIL;
}
