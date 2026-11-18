// The socket callbacks of secure_socket_impl.h on the board: TLS through
// platform's mbedTLS port, over its FreeRTOS-Plus-TCP socket, so the large
// tests run them against the live server, and PLAIN_HTTP through that socket
// alone. SOCKET_LOG isn't: what a test sends and receives stays off the
// console.

#include "secure_socket_impl.h"

namespace {

ebisu_socket_t* tls_socket(ebisu_socket_t* self)
{
    return &ebisu::ltest::ssl::data_of(self)->platform.base;
}

ebisu_socket_t* tcp_socket(ebisu_socket_t* self)
{
    return &ebisu::ltest::ssl::data_of(self)->platform.tcp.base;
}

ebisu_sock_code_t connect_tls(ebisu_socket_t* self, const char* host, uint16_t port)
{
    return tls_socket(self)->ops->connect(tls_socket(self), host, port);
}

ebisu_sock_code_t send_tls(ebisu_socket_t* self, const char* buffer, size_t length,
        size_t* out_sent_length)
{
    return tls_socket(self)->ops->send(tls_socket(self), buffer, length, out_sent_length);
}

ebisu_sock_code_t recv_tls(ebisu_socket_t* self, char* buffer, size_t length_to_read,
        size_t* out_actual_length)
{
    return tls_socket(self)->ops->recv(tls_socket(self), buffer, length_to_read,
            out_actual_length);
}

ebisu_sock_code_t close_tls(ebisu_socket_t* self)
{
    return tls_socket(self)->ops->close(tls_socket(self));
}

ebisu_sock_code_t connect_tcp(ebisu_socket_t* self, const char* host, uint16_t port)
{
    return tcp_socket(self)->ops->connect(tcp_socket(self), host, port);
}

ebisu_sock_code_t send_tcp(ebisu_socket_t* self, const char* buffer, size_t length,
        size_t* out_sent_length)
{
    return tcp_socket(self)->ops->send(tcp_socket(self), buffer, length, out_sent_length);
}

ebisu_sock_code_t recv_tcp(ebisu_socket_t* self, char* buffer, size_t length_to_read,
        size_t* out_actual_length)
{
    return tcp_socket(self)->ops->recv(tcp_socket(self), buffer, length_to_read,
            out_actual_length);
}

ebisu_sock_code_t close_tcp(ebisu_socket_t* self)
{
    return tcp_socket(self)->ops->close(tcp_socket(self));
}

}

const ebisu_socket_ops_t ebisu::ltest::ssl::ops = {
    connect_tls, send_tls, recv_tls, close_tls, NULL
};

const ebisu_socket_ops_t ebisu::ltest::tcp::ops = {
    connect_tcp, send_tcp, recv_tcp, close_tcp, NULL
};
