#include "catch.hpp"
#include "sock_cb_linux.h"

#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <string>
#include <thread>

namespace {

EVP_PKEY* generate_key()
{
    EVP_PKEY* key = nullptr;
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    REQUIRE( EVP_PKEY_keygen_init(ctx) == 1 );
    REQUIRE( EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) == 1 );
    REQUIRE( EVP_PKEY_keygen(ctx, &key) == 1 );
    EVP_PKEY_CTX_free(ctx);
    return key;
}

X509* issue_certificate(EVP_PKEY* key)
{
    X509* cert = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 3600);
    X509_set_pubkey(cert, key);
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
            (const unsigned char*)"localhost", -1, -1, 0);
    X509_set_issuer_name(cert, name);
    REQUIRE( X509_sign(cert, key, EVP_sha256()) > 0 );
    return cert;
}

// A TLS connection over a socketpair: the sample's callbacks at one end, as
// sock_cb_connect() would have left them, and the test at the other. Generating
// the key is slow, under valgrind above all, so the tests share one.
struct Connection {
    socket_context_t client;
    SSL_CTX* server_ctx;
    SSL* server;
    int server_fd;
    bool client_closed;

    Connection() : client_closed(false) {
        // As the sample does: writes go to a peer that has gone.
        signal(SIGPIPE, SIG_IGN);
        // As sock_cb_connect() does, which these tests don't go through.
        SSL_library_init();

        static EVP_PKEY* key = generate_key();
        static X509* cert = issue_certificate(key);

        int fds[2];
        REQUIRE( socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0 );
        server_fd = fds[0];

        server_ctx = SSL_CTX_new(SSLv23_server_method());
        REQUIRE( SSL_CTX_use_certificate(server_ctx, cert) == 1 );
        REQUIRE( SSL_CTX_use_PrivateKey(server_ctx, key) == 1 );
        server = SSL_new(server_ctx);
        SSL_set_fd(server, server_fd);

        sock_cb_init_context(&client, 0, 0);
        client.socket = fds[1];
        client.ssl_ctx = SSL_CTX_new(SSLv23_client_method());
#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
        // As sock_cb_connect() has it.
        SSL_CTX_set_options(client.ssl_ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
#endif
        client.ssl = SSL_new(client.ssl_ctx);
        SSL_set_fd(client.ssl, client.socket);

        std::thread accepting([this] { SSL_accept(server); });
        int connected = SSL_connect(client.ssl);
        accepting.join();
        REQUIRE( connected == 1 );

        struct timeval timeout = { 0, 100 * 1000 };
        REQUIRE( setsockopt(client.socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 );
        ERR_clear_error();
    }

    ~Connection() {
        if (!client_closed) {
            sock_cb_close(&client);
        }
        SSL_free(server);
        SSL_CTX_free(server_ctx);
        if (server_fd >= 0) {
            close(server_fd);
        }
    }

    // The transport goes, with no close_notify.
    void close_server_socket() {
        close(server_fd);
        server_fd = -1;
    }

    ebisu_sock_code_t close_client() {
        client_closed = true;
        return sock_cb_close(&client);
    }
};

// A failure earlier on the thread, as of a connection that didn't work out,
// leaves its entries in the thread's error queue. Not a system call's, which
// SSL_get_error() would take for SSL_ERROR_SYSCALL, and go on to look at errno.
void leave_an_error_in_the_queue()
{
    const unsigned char not_a_certificate[] = { 0xff, 0xff, 0xff, 0xff };
    const unsigned char* pos = not_a_certificate;
    REQUIRE( d2i_X509(nullptr, &pos, sizeof(not_a_certificate)) == nullptr );
    REQUIRE( ERR_peek_error() != 0 );
}

}

TEST_CASE( "sock_cb_recv reads what the peer sent" ) {
    Connection c;
    REQUIRE( SSL_write(c.server, "hello", 5) == 5 );

    char buffer[16];
    size_t received = 0;
    CHECK( sock_cb_recv(&c.client, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK );
    REQUIRE( received == 5 );
    CHECK( std::string(buffer, received) == "hello" );
}

// The push task waits for a message with this timeout, over and over: a
// timeout taken for a failure has it drop a connection nothing is wrong with.
TEST_CASE( "sock_cb_recv times out whatever an earlier failure left behind" ) {
    Connection c;
    leave_an_error_in_the_queue();

    char buffer[16];
    size_t received = 0;
    CHECK( sock_cb_recv(&c.client, buffer, sizeof(buffer), &received) == EBISU_SOCK_FAIL );
    CHECK( received == 0 );
    CHECK( sock_cb_recv_timed_out(&c.client) == KII_TRUE );
    CHECK( c.client.ssl_failed == KII_FALSE );

    // And the connection is as good as it was.
    REQUIRE( SSL_write(c.server, "hello", 5) == 5 );
    CHECK( sock_cb_recv(&c.client, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK );
    CHECK( received == 5 );
}

TEST_CASE( "sock_cb_recv takes a close_notify for the end of the stream" ) {
    Connection c;
    leave_an_error_in_the_queue();
    SSL_shutdown(c.server);

    char buffer[16];
    size_t received = 1;
    CHECK( sock_cb_recv(&c.client, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK );
    CHECK( received == 0 );
    CHECK( c.close_client() == EBISU_SOCK_OK );
}

// Servers answering "Connection: close" do this. errno is whatever the last
// call to fail on this thread left there, until SSL_read() has run.
TEST_CASE( "sock_cb_recv takes a closed transport for the end of the stream" ) {
    Connection c;
    c.close_server_socket();
    leave_an_error_in_the_queue();
    errno = EIO;

    char buffer[16];
    size_t received = 1;
    CHECK( sock_cb_recv(&c.client, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK );
    CHECK( received == 0 );
    CHECK( c.close_client() == EBISU_SOCK_OK );
}

// OpenSSL's documentation says SSL_shutdown() must not be called after this,
// and when it is, it fails.
TEST_CASE( "A connection that failed is closed without SSL_shutdown" ) {
    Connection c;
    const char not_a_record[] = "this is no TLS record, by a long way";
    REQUIRE( write(c.server_fd, not_a_record, sizeof(not_a_record)) > 0 );

    char buffer[16];
    size_t received = 0;
    CHECK( sock_cb_recv(&c.client, buffer, sizeof(buffer), &received) == EBISU_SOCK_FAIL );
    CHECK( sock_cb_recv_timed_out(&c.client) == KII_FALSE );
    CHECK( c.client.ssl_failed == KII_TRUE );
    CHECK( c.close_client() == EBISU_SOCK_OK );
}

TEST_CASE( "sock_cb_send reports a connection that has gone" ) {
    Connection c;
    c.close_server_socket();

    // The first may fit in the socket's buffer, on the way to no one.
    const char data[] = "hello";
    size_t sent = 0;
    ebisu_sock_code_t res = EBISU_SOCK_OK;
    for (int i = 0; i < 2 && res == EBISU_SOCK_OK; ++i) {
        res = sock_cb_send(&c.client, data, sizeof(data), &sent);
    }
    CHECK( res == EBISU_SOCK_FAIL );
    CHECK( c.client.ssl_failed == KII_TRUE );
    CHECK( c.close_client() == EBISU_SOCK_OK );
}

// The first attempt failing must not be held against the next.
TEST_CASE( "sock_cb_send sends with an earlier failure left behind" ) {
    Connection c;
    leave_an_error_in_the_queue();

    size_t sent = 0;
    CHECK( sock_cb_send(&c.client, "hello", 5, &sent) == EBISU_SOCK_OK );
    CHECK( sent == 5 );
    char buffer[16];
    CHECK( SSL_read(c.server, buffer, sizeof(buffer)) == 5 );
}
