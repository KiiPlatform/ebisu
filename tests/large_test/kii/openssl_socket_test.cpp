#include "catch.hpp"
#include "ebisu_openssl.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <netdb.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <pthread.h>
#include <atomic>

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

// Valid from from seconds from now, until to seconds from now.
// For localhost, in its subjectAltName as well as its commonName, unless
// alt_name is false: OpenSSL 4.1 and later match a host against the first
// alone. With alt_names, for those in its subjectAltName instead.
X509* issue_certificate(EVP_PKEY* key, long from = 0, long to = 3600, bool alt_name = true,
        const char* alt_names = "DNS:localhost")
{
    X509* cert = X509_new();
    REQUIRE( X509_set_version(cert, 2) == 1 );
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_get_notBefore(cert), from);
    X509_gmtime_adj(X509_get_notAfter(cert), to);
    X509_set_pubkey(cert, key);
    // A name of its own, which X509_set_subject_name() copies: OpenSSL 4
    // gives the certificate's as const.
    X509_NAME* name = X509_NAME_new();
    REQUIRE( X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
            (const unsigned char*)"localhost", -1, -1, 0) == 1 );
    REQUIRE( X509_set_subject_name(cert, name) == 1 );
    REQUIRE( X509_set_issuer_name(cert, name) == 1 );
    X509_NAME_free(name);
    if (alt_name) {
        X509V3_CTX ctx;
        X509V3_set_ctx_nodb(&ctx);
        X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);
        X509_EXTENSION* san = X509V3_EXT_conf_nid(nullptr, &ctx, NID_subject_alt_name,
                const_cast<char*>(alt_names));
        REQUIRE( san != nullptr );
        REQUIRE( X509_add_ext(cert, san, -1) == 1 );
        X509_EXTENSION_free(san);
    }
    REQUIRE( X509_sign(cert, key, EVP_sha256()) > 0 );
    return cert;
}

// The server's key and its certificate for localhost, self-signed. Generating
// the key is slow, under valgrind above all, so the tests share one.
EVP_PKEY* server_key()
{
    static EVP_PKEY* key = generate_key();
    return key;
}

X509* server_certificate()
{
    static X509* cert = issue_certificate(server_key());
    return cert;
}

// A TLS connection over a socketpair: platform's OpenSSL port at one end, as
// the socket's connect would have left them, and the test at the other.
struct Connection {
    ebisu_openssl_socket_t client;
    // The client's, which the socket's connect would have shared with others.
    SSL_CTX* client_ctx;
    SSL_CTX* server_ctx;
    SSL* server;
    int server_fd;
    bool client_closed;

    Connection() : client_closed(false) {
        // As the sample does: writes go to a peer that has gone.
        signal(SIGPIPE, SIG_IGN);
        // As the socket's connect does, which these tests don't go through.
        // Through the port, which with OpenSSL 1.0.2 also sets the locking
        // callbacks: without them, the handshake below, on two threads at
        // once, races in OpenSSL's shared state.
        ebisu_openssl_tls_t tls;
        REQUIRE( ebisu_openssl_tls_init(&tls, nullptr, nullptr, nullptr) );
        ebisu_openssl_tls_free(&tls);

        EVP_PKEY* key = server_key();
        X509* cert = server_certificate();

        int fds[2];
        REQUIRE( socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0 );
        server_fd = fds[0];

        server_ctx = SSL_CTX_new(SSLv23_server_method());
        REQUIRE( SSL_CTX_use_certificate(server_ctx, cert) == 1 );
        REQUIRE( SSL_CTX_use_PrivateKey(server_ctx, key) == 1 );
        server = SSL_new(server_ctx);
        SSL_set_fd(server, server_fd);

        ebisu_openssl_socket_init(&client, 0, 0);
        client.socket = fds[1];
        client_ctx = SSL_CTX_new(SSLv23_client_method());
#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
        // As the socket's connect has it.
        SSL_CTX_set_options(client_ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
#endif
        client.ssl = SSL_new(client_ctx);
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
            client.base.ops->close(&client.base);
        }
        SSL_CTX_free(client_ctx);
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
        return client.base.ops->close(&client.base);
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

TEST_CASE( "The OpenSSL socket's recv reads what the peer sent" ) {
    Connection c;
    REQUIRE( SSL_write(c.server, "hello", 5) == 5 );

    char buffer[16];
    size_t received = 0;
    CHECK( c.client.base.ops->recv(&c.client.base, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK );
    REQUIRE( received == 5 );
    CHECK( std::string(buffer, received) == "hello" );
}

// The push task waits for a message with this timeout, over and over: a
// timeout taken for a failure has it drop a connection nothing is wrong with.
TEST_CASE( "The OpenSSL socket's recv times out whatever an earlier failure left behind" ) {
    Connection c;
    leave_an_error_in_the_queue();

    char buffer[16];
    size_t received = 0;
    CHECK( c.client.base.ops->recv(&c.client.base, buffer, sizeof(buffer), &received) == EBISU_SOCK_FAIL );
    CHECK( received == 0 );
    CHECK( c.client.base.ops->recv_timed_out(&c.client.base) == true );
    CHECK( c.client.ssl_failed == false );

    // And the connection is as good as it was.
    REQUIRE( SSL_write(c.server, "hello", 5) == 5 );
    CHECK( c.client.base.ops->recv(&c.client.base, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK );
    CHECK( received == 5 );
}

TEST_CASE( "The OpenSSL socket's recv takes a close_notify for the end of the stream" ) {
    Connection c;
    leave_an_error_in_the_queue();
    SSL_shutdown(c.server);

    char buffer[16];
    size_t received = 1;
    CHECK( c.client.base.ops->recv(&c.client.base, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK );
    CHECK( received == 0 );
    CHECK( c.close_client() == EBISU_SOCK_OK );
}

// Servers answering "Connection: close" do this. errno is whatever the last
// call to fail on this thread left there, until SSL_read() has run.
TEST_CASE( "The OpenSSL socket's recv takes a closed transport for the end of the stream" ) {
    Connection c;
    c.close_server_socket();
    leave_an_error_in_the_queue();
    errno = EIO;

    char buffer[16];
    size_t received = 1;
    CHECK( c.client.base.ops->recv(&c.client.base, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK );
    CHECK( received == 0 );
    CHECK( c.close_client() == EBISU_SOCK_OK );
}

// The MQTT task waits with it for a packet a little at a time, so that a
// message given meanwhile is sent soon.
TEST_CASE( "The OpenSSL socket's recv_wait waits for something for recv to return" ) {
    Connection c;
    ebisu_socket_t* sock = &c.client.base;
    char buffer[16];
    size_t received = 0;
    // What a TLS 1.3 server sends after the handshake, which is no data: recv
    // reads it, and times out.
    CHECK( sock->ops->recv(sock, buffer, sizeof(buffer), &received) == EBISU_SOCK_FAIL );
    REQUIRE( sock->ops->recv_timed_out(sock) );

    SECTION("for its timeout, with nothing arriving, which leaves the connection as it was") {
        const auto start = std::chrono::steady_clock::now();
        CHECK( sock->ops->recv_wait(sock, 300) == false );
        const auto waited = std::chrono::steady_clock::now() - start;
        CHECK( waited >= std::chrono::milliseconds(290) );
        CHECK( waited < std::chrono::milliseconds(2000) );

        REQUIRE( SSL_write(c.server, "hello", 5) == 5 );
        CHECK( sock->ops->recv_wait(sock, 10000) == true );
        CHECK( sock->ops->recv(sock, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK );
        CHECK( std::string(buffer, received) == "hello" );
    }

    SECTION("not for what it has decrypted and recv hasn't returned, which the socket no longer has") {
        REQUIRE( SSL_write(c.server, "hello", 5) == 5 );
        CHECK( sock->ops->recv(sock, buffer, 2, &received) == EBISU_SOCK_OK );
        CHECK( received == 2 );
        CHECK( sock->ops->recv_wait(sock, 0) == true );
        CHECK( sock->ops->recv(sock, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK );
        CHECK( std::string(buffer, received) == "llo" );
    }

    SECTION("not for the end of the stream") {
        SSL_shutdown(c.server);
        CHECK( sock->ops->recv_wait(sock, 10000) == true );
        received = 1;
        CHECK( sock->ops->recv(sock, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK );
        CHECK( received == 0 );
    }

    SECTION("not for a transport that closed") {
        c.close_server_socket();
        CHECK( sock->ops->recv_wait(sock, 10000) == true );
        received = 1;
        CHECK( sock->ops->recv(sock, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK );
        CHECK( received == 0 );
    }
}

// OpenSSL's documentation says SSL_shutdown() must not be called after this,
// and when it is, it fails.
TEST_CASE( "A connection that failed is closed without SSL_shutdown" ) {
    Connection c;
    const char not_a_record[] = "this is no TLS record, by a long way";
    REQUIRE( write(c.server_fd, not_a_record, sizeof(not_a_record)) > 0 );

    char buffer[16];
    size_t received = 0;
    CHECK( c.client.base.ops->recv(&c.client.base, buffer, sizeof(buffer), &received) == EBISU_SOCK_FAIL );
    CHECK( c.client.base.ops->recv_timed_out(&c.client.base) == false );
    CHECK( c.client.ssl_failed == true );
    CHECK( c.close_client() == EBISU_SOCK_OK );
}

TEST_CASE( "The OpenSSL socket's send reports a connection that has gone" ) {
    Connection c;
    c.close_server_socket();

    // The first may fit in the socket's buffer, on the way to no one.
    const char data[] = "hello";
    size_t sent = 0;
    ebisu_sock_code_t res = EBISU_SOCK_OK;
    for (int i = 0; i < 2 && res == EBISU_SOCK_OK; ++i) {
        res = c.client.base.ops->send(&c.client.base, data, sizeof(data), &sent);
    }
    CHECK( res == EBISU_SOCK_FAIL );
    CHECK( c.client.ssl_failed == true );
    CHECK( c.close_client() == EBISU_SOCK_OK );
}

namespace {

volatile sig_atomic_t sigpipes_delivered = 0;

void count_sigpipe(int) { sigpipes_delivered = sigpipes_delivered + 1; }

// SIGPIPE as an application that does nothing about it has it, but counted
// rather than the end of the process; as the fixture left it, afterwards.
struct CountingSigpipe {
    CountingSigpipe() {
        sigpipes_delivered = 0;
        signal(SIGPIPE, count_sigpipe);
    }
    ~CountingSigpipe() { signal(SIGPIPE, SIG_IGN); }
};

bool sigpipe_pending()
{
    sigset_t pending;
    REQUIRE( sigpending(&pending) == 0 );
    return sigismember(&pending, SIGPIPE) == 1;
}

// A send to the peer that has gone, which fails.
void send_to_no_one(Connection& c)
{
    const char data[] = "hello";
    size_t sent = 0;
    ebisu_sock_code_t res = EBISU_SOCK_OK;
    for (int i = 0; i < 2 && res == EBISU_SOCK_OK; ++i) {
        res = c.client.base.ops->send(&c.client.base, data, sizeof(data), &sent);
    }
    REQUIRE( res == EBISU_SOCK_FAIL );
}

}

// OpenSSL writes with write(), which raises SIGPIPE on a peer that has gone:
// by default the end of the process.
TEST_CASE( "The OpenSSL socket raises no SIGPIPE on a peer that has gone" ) {
    Connection c;
    CountingSigpipe counting;
    c.close_server_socket();

    SECTION("from a send") {
        send_to_no_one(c);
        CHECK( sigpipes_delivered == 0 );
        CHECK_FALSE( sigpipe_pending() );
        long detail = 0;
        CHECK( c.client.base.ops->last_error(&c.client.base, &detail) == EBISU_SOCK_ERROR_CLOSED );
        CHECK( detail == EPIPE );
        // And the thread's mask is as it was: SIGPIPE not blocked.
        sigset_t mask;
        REQUIRE( pthread_sigmask(SIG_SETMASK, nullptr, &mask) == 0 );
        CHECK( sigismember(&mask, SIGPIPE) == 0 );
    }

    SECTION("from a close, whose close_notify goes to no one") {
        CHECK( c.close_client() == EBISU_SOCK_OK );
        CHECK( sigpipes_delivered == 0 );
        CHECK_FALSE( sigpipe_pending() );
    }

    SECTION("nor leaves one pending for an application that blocks it") {
        sigset_t sigpipe, old_mask;
        sigemptyset(&sigpipe);
        sigaddset(&sigpipe, SIGPIPE);
        REQUIRE( pthread_sigmask(SIG_BLOCK, &sigpipe, &old_mask) == 0 );
        send_to_no_one(c);
        CHECK_FALSE( sigpipe_pending() );
        // The mask is the application's still.
        sigset_t now_mask;
        REQUIRE( pthread_sigmask(SIG_SETMASK, &old_mask, &now_mask) == 0 );
        CHECK( sigismember(&now_mask, SIGPIPE) == 1 );
        CHECK( sigpipes_delivered == 0 );
    }

    SECTION("and leaves one that was pending before, which isn't its own") {
        sigset_t sigpipe, old_mask;
        sigemptyset(&sigpipe);
        sigaddset(&sigpipe, SIGPIPE);
        REQUIRE( pthread_sigmask(SIG_BLOCK, &sigpipe, &old_mask) == 0 );
        REQUIRE( pthread_kill(pthread_self(), SIGPIPE) == 0 );
        REQUIRE( sigpipe_pending() );
        send_to_no_one(c);
        CHECK( sigpipe_pending() );
        CHECK( sigpipes_delivered == 0 );
        // Unblocked, the application's own is delivered to it.
        REQUIRE( pthread_sigmask(SIG_SETMASK, &old_mask, nullptr) == 0 );
        CHECK( sigpipes_delivered == 1 );
    }
}

// The first attempt failing must not be held against the next.
TEST_CASE( "The OpenSSL socket's send sends with an earlier failure left behind" ) {
    Connection c;
    leave_an_error_in_the_queue();

    size_t sent = 0;
    CHECK( c.client.base.ops->send(&c.client.base, "hello", 5, &sent) == EBISU_SOCK_OK );
    CHECK( sent == 5 );
    char buffer[16];
    CHECK( SSL_read(c.server, buffer, sizeof(buffer)) == 5 );
}

namespace {

// The loopback address of family, AF_INET or AF_INET6, with port.
struct sockaddr_storage loopback(int family, uint16_t port)
{
    struct sockaddr_storage addr;
    memset(&addr, 0, sizeof(addr));
    if (family == AF_INET6) {
        struct sockaddr_in6* in6 = (struct sockaddr_in6*)&addr;
        in6->sin6_family = AF_INET6;
        in6->sin6_addr = in6addr_loopback;
        in6->sin6_port = htons(port);
    } else {
        struct sockaddr_in* in = (struct sockaddr_in*)&addr;
        in->sin_family = AF_INET;
        in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        in->sin_port = htons(port);
    }
    return addr;
}

socklen_t length_of(int family)
{
    return family == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
}

// A TLS server on a loopback port of its own, IPv4's or IPv6's, with the
// certificate for localhost, taking connections one after another until the
// test is done. listening is false where the family can't be listened on.
// With greet, it sends a byte once the handshake is done: in TLS 1.3, the
// client reads the server's session tickets on its way to it.
struct LocalServer {
    int family;
    bool greet;
    int listen_fd;
    bool listening;
    uint16_t port;
    SSL_CTX* ctx;
    bool stopping;
    std::thread thread;

    // With version, the only TLS version it takes, and with ciphers, the only
    // TLS 1.2 and earlier suites, older ones included.
    explicit LocalServer(int family = AF_INET, bool greet = false, int version = 0,
            const char* ciphers = nullptr) : family(family),
            greet(greet), listening(false), port(0), ctx(nullptr), stopping(false) {
        signal(SIGPIPE, SIG_IGN);
        listen_fd = socket(family, SOCK_STREAM, 0);
        if (listen_fd < 0) {
            return;
        }
        struct sockaddr_storage addr = loopback(family, 0);
        if (bind(listen_fd, (struct sockaddr*)&addr, length_of(family)) != 0) {
            return;
        }
        REQUIRE( listen(listen_fd, 4) == 0 );
        socklen_t len = sizeof(addr);
        REQUIRE( getsockname(listen_fd, (struct sockaddr*)&addr, &len) == 0 );
        port = ntohs(family == AF_INET6 ?
                ((struct sockaddr_in6*)&addr)->sin6_port : ((struct sockaddr_in*)&addr)->sin_port);
        listening = true;
        // So that it never waits for ever, whatever the test does.
        struct timeval timeout = { 10, 0 };
        setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        SSL_library_init();
        ctx = SSL_CTX_new(SSLv23_server_method());
        REQUIRE( SSL_CTX_use_certificate(ctx, server_certificate()) == 1 );
        REQUIRE( SSL_CTX_use_PrivateKey(ctx, server_key()) == 1 );
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
        // ECDHE, which 1.0.2 has a server offer only when told to, and the
        // port asks for, as servers have it.
        REQUIRE( SSL_CTX_set_ecdh_auto(ctx, 1) == 1 );
#endif
        if (version != 0) {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
            long off = SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2;
            off &= ~(version == TLS1_VERSION ? SSL_OP_NO_TLSv1 :
                    version == TLS1_1_VERSION ? SSL_OP_NO_TLSv1_1 : SSL_OP_NO_TLSv1_2);
            SSL_CTX_set_options(ctx, off);
#else
            REQUIRE( SSL_CTX_set_min_proto_version(ctx, version) == 1 );
            REQUIRE( SSL_CTX_set_max_proto_version(ctx, version) == 1 );
#endif
        }
        if (ciphers != nullptr) {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
            const std::string list = ciphers;
#else
            // Security level 0, where TLS 1.0, 1.1 and SHA-1 still are.
            const std::string list = std::string(ciphers) + ":@SECLEVEL=0";
#endif
            REQUIRE( SSL_CTX_set_cipher_list(ctx, list.c_str()) == 1 );
        }
        thread = std::thread([this] { serve(); });
    }

    ~LocalServer() {
        if (!listening) {
            if (listen_fd >= 0) {
                close(listen_fd);
            }
            return;
        }
        stopping = true;
        // Wakes accept() with a connection that only says to stop.
        int fd = socket(family, SOCK_STREAM, 0);
        struct sockaddr_storage addr = loopback(family, port);
        connect(fd, (struct sockaddr*)&addr, length_of(family));
        thread.join();
        close(fd);
        close(listen_fd);
        SSL_CTX_free(ctx);
    }

    void serve() {
        while (true) {
            int fd = accept(listen_fd, nullptr, nullptr);
            if (fd < 0 || stopping) {
                if (fd >= 0) {
                    close(fd);
                }
                return;
            }
            struct timeval timeout = { 10, 0 };
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            SSL* ssl = SSL_new(ctx);
            SSL_set_fd(ssl, fd);
            if (SSL_accept(ssl) == 1) {
                if (greet) {
                    SSL_write(ssl, "!", 1);
                }
                // Until the client closes.
                char c;
                SSL_read(ssl, &c, 1);
            }
            SSL_free(ssl);
            close(fd);
            ERR_clear_error();
        }
    }
};

// A TLS 1.0 ServerHello, for TLS_RSA_WITH_AES_128_CBC_SHA: what a server of
// TLS 1.0 alone answers a client that doesn't say which versions it takes.
const unsigned char TLS10_SERVER_HELLO[] = {
    0x16, 0x03, 0x01, 0x00, 0x2a,  // a handshake record of TLS 1.0, 42 bytes
    0x02, 0x00, 0x00, 0x26,        // ServerHello, 38 bytes
    0x03, 0x01,                    // TLS 1.0
    0x5c, 0x1f, 0x4e, 0x0b, 0x7a, 0x31, 0x22, 0x90, 0x6d, 0x18, 0x45, 0xe2, 0x03, 0xaa, 0x67, 0x0c,
    0x91, 0x3b, 0x58, 0x2f, 0xd4, 0x10, 0x86, 0x7e, 0x29, 0xc5, 0x33, 0x0a, 0x64, 0xbe, 0x17, 0x4d,
    0x00,                          // no session ID
    0x00, 0x2f,                    // TLS_RSA_WITH_AES_128_CBC_SHA
    0x00,                          // no compression
};

// A fatal alert of TLS 1.2's, of the given description.
std::string fatal_alert(unsigned char description)
{
    const unsigned char alert[] = { 0x15, 0x03, 0x03, 0x00, 0x02, 0x02, description };
    return std::string((const char*)alert, sizeof(alert));
}

// A server on 127.0.0.1 that answers what it reads with response, by default
// an HTTP response: not TLS.
struct PlainServer {
    int listen_fd;
    uint16_t port;
    std::thread thread;

    explicit PlainServer(std::string response =
            "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n") :
            listen_fd(socket(AF_INET, SOCK_STREAM, 0)), port(0) {
        REQUIRE( listen_fd >= 0 );
        struct sockaddr_storage addr = loopback(AF_INET, 0);
        REQUIRE( bind(listen_fd, (struct sockaddr*)&addr, length_of(AF_INET)) == 0 );
        REQUIRE( listen(listen_fd, 1) == 0 );
        socklen_t len = sizeof(addr);
        REQUIRE( getsockname(listen_fd, (struct sockaddr*)&addr, &len) == 0 );
        port = ntohs(((struct sockaddr_in*)&addr)->sin_port);
        thread = std::thread([this, response] {
            int fd = accept(listen_fd, nullptr, nullptr);
            if (fd < 0) {
                return;
            }
            char buffer[2048];
            (void)recv(fd, buffer, sizeof(buffer), 0);
            (void)send(fd, response.data(), response.size(), 0);
            // For the client to close first.
            (void)recv(fd, buffer, sizeof(buffer), 0);
            close(fd);
        });
    }

    ~PlainServer() {
        shutdown(listen_fd, SHUT_RDWR);
        thread.join();
        close(listen_fd);
    }
};


// A file of CA certificates with the server's, removed at the end.
struct CaFile {
    char path[64];

    explicit CaFile(X509* cert = server_certificate()) {
        strcpy(path, "/tmp/ebisu_ca_XXXXXX");
        int fd = mkstemp(path);
        REQUIRE( fd >= 0 );
        FILE* f = fdopen(fd, "w");
        REQUIRE( PEM_write_X509(f, cert) == 1 );
        fclose(f);
    }

    ~CaFile() {
        unlink(path);
    }
};

// A directory of CA certificates named by hash, with the server's.
struct CaPath {
    char dir[64];
    std::string file;

    CaPath() {
        strcpy(dir, "/tmp/ebisu_ca_dir_XXXXXX");
        REQUIRE( mkdtemp(dir) != nullptr );
        char name[32];
        snprintf(name, sizeof(name), "%08lx.0", X509_subject_name_hash(server_certificate()));
        file = std::string(dir) + "/" + name;
        FILE* f = fopen(file.c_str(), "w");
        REQUIRE( f != nullptr );
        REQUIRE( PEM_write_X509(f, server_certificate()) == 1 );
        fclose(f);
    }

    ~CaPath() {
        unlink(file.c_str());
        rmdir(dir);
    }
};

ebisu_sock_code_t connect_to(ebisu_openssl_socket_t* sock, const LocalServer& server)
{
    return sock->base.ops->connect(&sock->base, "localhost", server.port);
}

}

TEST_CASE( "The OpenSSL socket trusts the CA certificates of its configuration" ) {
    LocalServer server;
    REQUIRE( server.listening );

    SECTION("from a file") {
        CaFile ca;
        ebisu_openssl_tls_t tls;
        REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
        ebisu_openssl_socket_t sock;
        ebisu_openssl_socket_init(&sock, 5, 5);
        ebisu_openssl_socket_set_tls(&sock, &tls);
        REQUIRE( connect_to(&sock, server) == EBISU_SOCK_OK );
        CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
        ebisu_openssl_tls_free(&tls);
    }

    SECTION("from a directory") {
        CaPath ca;
        ebisu_openssl_tls_t tls;
        REQUIRE( ebisu_openssl_tls_init(&tls, nullptr, ca.dir, nullptr) );
        ebisu_openssl_socket_t sock;
        ebisu_openssl_socket_init(&sock, 5, 5);
        ebisu_openssl_socket_set_tls(&sock, &tls);
        REQUIRE( connect_to(&sock, server) == EBISU_SOCK_OK );
        CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
        ebisu_openssl_tls_free(&tls);
    }

    SECTION("and the system's, which don't know the server, without one") {
        ebisu_openssl_socket_t sock;
        ebisu_openssl_socket_init(&sock, 5, 5);
        CHECK( connect_to(&sock, server) == EBISU_SOCK_FAIL );
    }
}

namespace {

// Has server speak TLS 1.2 at most, where its suites count, with these only.
void limit_server(LocalServer& server, const char* ciphers)
{
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    REQUIRE( SSL_CTX_set_max_proto_version(server.ctx, TLS1_2_VERSION) == 1 );
#endif
    REQUIRE( SSL_CTX_set_cipher_list(server.ctx, ciphers) == 1 );
}

}

// The port took the suites OpenSSL's build defaults to, and from OpenSSL 1.1
// on, protocol versions from TLS 1.0.
TEST_CASE( "The OpenSSL socket asks for TLS 1.2 or later, with forward secrecy and authenticated encryption" ) {
    LocalServer server;
    REQUIRE( server.listening );
    CaFile ca;
    ebisu_openssl_tls_t tls;
    REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
    ebisu_openssl_socket_t sock;
    ebisu_openssl_socket_init(&sock, 5, 5);
    ebisu_openssl_socket_set_tls(&sock, &tls);

    SECTION("a server with such a suite is connected to with it") {
        limit_server(server, "ECDHE-RSA-AES128-GCM-SHA256");
        REQUIRE( connect_to(&sock, server) == EBISU_SOCK_OK );
        CHECK( std::string(SSL_get_cipher_name(sock.ssl)) == "ECDHE-RSA-AES128-GCM-SHA256" );
        CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
    }

    SECTION("a server with only RSA key exchange is refused") {
        limit_server(server, "AES128-GCM-SHA256:AES128-SHA");
        CHECK( connect_to(&sock, server) == EBISU_SOCK_FAIL );
    }

    SECTION("a server with only CBC suites is refused") {
        limit_server(server, "ECDHE-RSA-AES128-SHA:ECDHE-RSA-AES128-SHA256");
        CHECK( connect_to(&sock, server) == EBISU_SOCK_FAIL );
    }

    SECTION("a server with TLS 1.1 at most is refused") {
        SSL_CTX_set_options(server.ctx, SSL_OP_NO_TLSv1_2);
#ifdef SSL_OP_NO_TLSv1_3
        SSL_CTX_set_options(server.ctx, SSL_OP_NO_TLSv1_3);
#endif
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
        // Which OpenSSL 3 allows only at security level 0.
        SSL_CTX_set_security_level(server.ctx, 0);
#endif
        CHECK( connect_to(&sock, server) == EBISU_SOCK_FAIL );
    }

    ebisu_openssl_tls_free(&tls);
}

TEST_CASE( "A TLS configuration whose CA certificates can't be loaded fails" ) {
    ebisu_openssl_tls_t tls;
    CHECK_FALSE( ebisu_openssl_tls_init(&tls, "/nonexistent/ca.pem", nullptr, nullptr) );
    CHECK( tls.ssl_ctx == nullptr );

    // And says why, to who asks: OpenSSL's first error of the call, whatever
    // an earlier one left in the queue.
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
    ERR_raise(ERR_LIB_USER, 1);
#else
    ERR_put_error(ERR_LIB_USER, 0, 1, __FILE__, __LINE__);
#endif
    unsigned long error = 0;
    CHECK_FALSE( ebisu_openssl_tls_init(&tls, "/nonexistent/ca.pem", nullptr, &error) );
    CHECK( error != 0 );
    CHECK( ERR_GET_LIB(error) != ERR_LIB_USER );
    // With the rest of it still in the queue.
    CHECK( ERR_peek_error() == error );
    ERR_clear_error();

    // 0 when it doesn't fail.
    CaFile ca;
    error = 1;
    REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, &error) );
    CHECK( error == 0 );
    ebisu_openssl_tls_free(&tls);
}

// The CA certificates were parsed again for every connection, each with an
// SSL_CTX of its own.
TEST_CASE( "The OpenSSL sockets of a configuration share its SSL_CTX" ) {
    LocalServer server;
    REQUIRE( server.listening );
    CaFile ca;
    ebisu_openssl_tls_t tls;
    REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );

    ebisu_openssl_socket_t first;
    ebisu_openssl_socket_init(&first, 5, 5);
    ebisu_openssl_socket_set_tls(&first, &tls);
    ebisu_openssl_socket_t second;
    ebisu_openssl_socket_init(&second, 5, 5);
    ebisu_openssl_socket_set_tls(&second, &tls);

    for (ebisu_openssl_socket_t* sock : { &first, &second, &first }) {
        REQUIRE( connect_to(sock, server) == EBISU_SOCK_OK );
        CHECK( SSL_get_SSL_CTX(sock->ssl) == tls.ssl_ctx );
        CHECK( sock->base.ops->close(&sock->base) == EBISU_SOCK_OK );
    }
    ebisu_openssl_tls_free(&tls);
}

namespace {

// Connects sock to server, reads its greeting, and closes, saying whether the
// connection resumed a session.
bool connect_resumed(ebisu_openssl_socket_t* sock, const LocalServer& server)
{
    REQUIRE( connect_to(sock, server) == EBISU_SOCK_OK );
    bool resumed = SSL_session_reused(sock->ssl) == 1;
    char c = 0;
    size_t received = 0;
    CHECK( sock->base.ops->recv(&sock->base, &c, 1, &received) == EBISU_SOCK_OK );
    CHECK( received == 1 );
    CHECK( sock->base.ops->close(&sock->base) == EBISU_SOCK_OK );
    return resumed;
}

}

// Every connection made a full handshake, though each request has one of its
// own: the configuration now keeps the session of the last connection to a
// host, for the next to resume.
TEST_CASE( "The OpenSSL socket resumes the session of the last connection to the host" ) {
    LocalServer server(AF_INET, true);
    REQUIRE( server.listening );
    CaFile ca;
    ebisu_openssl_tls_t tls;
    REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
    ebisu_openssl_socket_t sock;
    ebisu_openssl_socket_init(&sock, 5, 5);
    ebisu_openssl_socket_set_tls(&sock, &tls);

    CHECK_FALSE( connect_resumed(&sock, server) );

    SECTION("the next connection resumes it, and the one after") {
        CHECK( connect_resumed(&sock, server) );
        CHECK( connect_resumed(&sock, server) );
    }

    SECTION("another socket of the configuration resumes it") {
        ebisu_openssl_socket_t other;
        ebisu_openssl_socket_init(&other, 5, 5);
        ebisu_openssl_socket_set_tls(&other, &tls);
        CHECK( connect_resumed(&other, server) );
    }

    SECTION("a socket of another configuration doesn't") {
        ebisu_openssl_tls_t other_tls;
        REQUIRE( ebisu_openssl_tls_init(&other_tls, ca.path, nullptr, nullptr) );
        ebisu_openssl_socket_t other;
        ebisu_openssl_socket_init(&other, 5, 5);
        ebisu_openssl_socket_set_tls(&other, &other_tls);
        CHECK_FALSE( connect_resumed(&other, server) );
        ebisu_openssl_tls_free(&other_tls);
    }

    SECTION("a connection to another port doesn't, and each keeps its own") {
        LocalServer other(AF_INET, true);
        REQUIRE( other.listening );
        CHECK_FALSE( connect_resumed(&sock, other) );
        CHECK( connect_resumed(&sock, server) );
        CHECK( connect_resumed(&sock, other) );
    }

    SECTION("more hosts than it keeps push the oldest out") {
        std::vector<std::unique_ptr<LocalServer>> others;
        for (int i = 0; i < 4; ++i) {
            others.emplace_back(new LocalServer(AF_INET, true));
            REQUIRE( others.back()->listening );
            CHECK_FALSE( connect_resumed(&sock, *others.back()) );
        }
        CHECK_FALSE( connect_resumed(&sock, server) );
        CHECK( connect_resumed(&sock, *others.back()) );
    }

    // What the configuration kept is freed with it, which valgrind checks.
    ebisu_openssl_tls_free(&tls);
}

namespace {

// Whether host has addresses of both IPv4 and IPv6.
bool has_both_families(const char* host)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* addrs = nullptr;
    if (getaddrinfo(host, "443", &hints, &addrs) != 0) {
        return false;
    }
    bool v4 = false;
    bool v6 = false;
    for (struct addrinfo* ai = addrs; ai != nullptr; ai = ai->ai_next) {
        v4 = v4 || ai->ai_family == AF_INET;
        v6 = v6 || ai->ai_family == AF_INET6;
    }
    freeaddrinfo(addrs);
    return v4 && v6;
}

}

// The port looked hosts up with gethostbyname(), which knows IPv4 only, and
// connected to the first address it gave: a server on another was out of
// reach.
TEST_CASE( "The OpenSSL socket tries each of the host's addresses" ) {
    if (!has_both_families("localhost")) {
        WARN( "localhost has no IPv4 and IPv6 addresses both: skipped" );
        return;
    }
    for (int family : { AF_INET, AF_INET6 }) {
        INFO( "server on " << (family == AF_INET6 ? "IPv6" : "IPv4") );
        LocalServer server(family);
        if (!server.listening) {
            WARN( "can't listen on loopback of this family: skipped" );
            continue;
        }
        CaFile ca;
        ebisu_openssl_tls_t tls;
        REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
        for (unsigned int timeout : { 5u, 0u }) {
            INFO( "connect timeout " << timeout );
            ebisu_openssl_socket_t sock;
            ebisu_openssl_socket_init(&sock, 5, 5);
            ebisu_openssl_socket_set_connect_timeout(&sock, timeout);
            ebisu_openssl_socket_set_tls(&sock, &tls);
            REQUIRE( connect_to(&sock, server) == EBISU_SOCK_OK );
            CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
        }
        ebisu_openssl_tls_free(&tls);
    }
}

// Where nothing answers, connect() waited for TCP to give up, about two
// minutes on Linux, unless a send timeout cut it short.
TEST_CASE( "The OpenSSL socket gives up connecting at its connect timeout" ) {
    ebisu_openssl_socket_t sock;
    ebisu_openssl_socket_init(&sock, 0, 0);
    ebisu_openssl_socket_set_connect_timeout(&sock, 1);
    auto start = std::chrono::steady_clock::now();
    // TEST-NET-1, which nothing answers on.
    CHECK( sock.base.ops->connect(&sock.base, "192.0.2.1", 443) == EBISU_SOCK_FAIL );
    auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK( std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 5 );
}

TEST_CASE( "The OpenSSL socket fails on a host that doesn't resolve" ) {
    for (unsigned int timeout : { 5u, 0u }) {
        INFO( "connect timeout " << timeout );
        ebisu_openssl_socket_t sock;
        ebisu_openssl_socket_init(&sock, 0, 0);
        ebisu_openssl_socket_set_connect_timeout(&sock, timeout);
        CHECK( sock.base.ops->connect(&sock.base, "ebisu-test.invalid", 443) == EBISU_SOCK_FAIL );
    }
}

namespace {

// A loopback port nothing listens on: one just bound and let go.
uint16_t closed_port()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE( fd >= 0 );
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    REQUIRE( bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0 );
    socklen_t len = sizeof(addr);
    REQUIRE( getsockname(fd, (struct sockaddr*)&addr, &len) == 0 );
    close(fd);
    return ntohs(addr.sin_port);
}

ebisu_sock_error_t last_error(ebisu_openssl_socket_t* sock, long* detail)
{
    REQUIRE( sock->base.ops->last_error != nullptr );
    return sock->base.ops->last_error(&sock->base, detail);
}

}

// Every failure to connect was the same EBISU_SOCK_FAIL, a host that didn't
// resolve and a certificate refused alike. last_error tells them apart.
TEST_CASE( "The OpenSSL socket says why it failed" ) {
    ebisu_openssl_socket_t sock;
    ebisu_openssl_socket_init(&sock, 5, 5);
    long detail = -1;

    SECTION("a host that doesn't resolve, with the resolver's error") {
        for (unsigned int timeout : { 5u, 0u }) {
            INFO( "connect timeout " << timeout );
            ebisu_openssl_socket_set_connect_timeout(&sock, timeout);
            REQUIRE( sock.base.ops->connect(&sock.base, "ebisu-test.invalid", 443) == EBISU_SOCK_FAIL );
            CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_DNS );
            CHECK( detail != 0 );
        }
    }

    SECTION("an address that refuses, with errno") {
        REQUIRE( sock.base.ops->connect(&sock.base, "127.0.0.1", closed_port()) == EBISU_SOCK_FAIL );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_CONNECT );
        CHECK( detail == ECONNREFUSED );
    }

    SECTION("an address that doesn't answer in time") {
        ebisu_openssl_socket_set_connect_timeout(&sock, 1);
        // TEST-NET-1, which nothing answers on.
        REQUIRE( sock.base.ops->connect(&sock.base, "192.0.2.1", 443) == EBISU_SOCK_FAIL );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_TIMEOUT );
    }

    SECTION("a certificate refused, with why") {
        LocalServer server;
        REQUIRE( server.listening );
        // The system's CA certificates, which don't know the server's.
        REQUIRE( connect_to(&sock, server) == EBISU_SOCK_FAIL );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( detail == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT );
    }

    SECTION("a handshake that fails for no suite in common, with OpenSSL's error") {
        LocalServer server;
        REQUIRE( server.listening );
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
        REQUIRE( SSL_CTX_set_max_proto_version(server.ctx, TLS1_2_VERSION) == 1 );
#endif
        // Suites the port doesn't offer.
        REQUIRE( SSL_CTX_set_cipher_list(server.ctx, "AES128-SHA") == 1 );
        CaFile ca;
        ebisu_openssl_tls_t tls;
        REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
        ebisu_openssl_socket_set_tls(&sock, &tls);
        REQUIRE( connect_to(&sock, server) == EBISU_SOCK_FAIL );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_TLS_MISMATCH );
        CHECK( detail != 0 );
        ebisu_openssl_tls_free(&tls);
    }

    SECTION("a handshake that fails otherwise, with OpenSSL's error") {
        PlainServer plain;
        REQUIRE( sock.base.ops->connect(&sock.base, "127.0.0.1", plain.port) == EBISU_SOCK_FAIL );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_TLS );
        CHECK( detail != 0 );
    }
}

namespace {

// A server that takes one connection and does with it what it is given to,
// until the client closes or the test ends.
struct RawServer {
    int listen_fd;
    uint16_t port;
    std::atomic<bool> stopping;
    std::thread thread;

    explicit RawServer(std::function<void(int fd, const std::atomic<bool>& stopping)> serve) :
            listen_fd(socket(AF_INET, SOCK_STREAM, 0)), port(0), stopping(false) {
        REQUIRE( listen_fd >= 0 );
        struct sockaddr_storage addr = loopback(AF_INET, 0);
        REQUIRE( bind(listen_fd, (struct sockaddr*)&addr, length_of(AF_INET)) == 0 );
        REQUIRE( listen(listen_fd, 1) == 0 );
        socklen_t len = sizeof(addr);
        REQUIRE( getsockname(listen_fd, (struct sockaddr*)&addr, &len) == 0 );
        port = ntohs(((struct sockaddr_in*)&addr)->sin_port);
        thread = std::thread([this, serve] {
            int fd = accept(listen_fd, nullptr, nullptr);
            if (fd < 0) {
                return;
            }
            serve(fd, stopping);
            close(fd);
        });
    }

    ~RawServer() {
        stopping = true;
        shutdown(listen_fd, SHUT_RDWR);
        thread.join();
        close(listen_fd);
    }
};

// Says nothing, for as long as the client stays.
void say_nothing(int fd, const std::atomic<bool>&)
{
    char buffer[2048];
    while (recv(fd, buffer, sizeof(buffer), 0) > 0) {
    }
}

// The start of a handshake record of 16384 bytes, then a byte of it every
// 100 ms: each wait for the server is short, and there is no end to them.
void drip(int fd, const std::atomic<bool>& stopping)
{
    const char header[] = { 0x16, 0x03, 0x03, 0x40, 0x00 };
    (void)send(fd, header, sizeof(header), MSG_NOSIGNAL);
    for (int i = 0; i < 300 && !stopping; ++i) {
        const char byte = 0x02;
        if (send(fd, &byte, 1, MSG_NOSIGNAL) != 1) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

double seconds_since(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

}

// The receive and send timeouts bound each wait for the server, and not how
// many of them a handshake takes.
TEST_CASE( "The OpenSSL socket gives up a handshake at its connect timeout" ) {
    ebisu_openssl_socket_t sock;
    long detail = -1;

    SECTION("with a server that says nothing, and no receive timeout") {
        RawServer server(say_nothing);
        ebisu_openssl_socket_init(&sock, 0, 0);
        ebisu_openssl_socket_set_connect_timeout(&sock, 1);
        auto start = std::chrono::steady_clock::now();
        CHECK( sock.base.ops->connect(&sock.base, "127.0.0.1", server.port) == EBISU_SOCK_FAIL );
        CHECK( seconds_since(start) >= 0.9 );
        CHECK( seconds_since(start) < 4 );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_TIMEOUT );
    }

    SECTION("with a server that sends a byte at a time, each within the receive timeout") {
        RawServer server(drip);
        ebisu_openssl_socket_init(&sock, 5, 5);
        ebisu_openssl_socket_set_connect_timeout(&sock, 1);
        auto start = std::chrono::steady_clock::now();
        CHECK( sock.base.ops->connect(&sock.base, "127.0.0.1", server.port) == EBISU_SOCK_FAIL );
        CHECK( seconds_since(start) >= 0.9 );
        CHECK( seconds_since(start) < 4 );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_TIMEOUT );
    }

    SECTION("at the receive timeout, if that comes first") {
        RawServer server(say_nothing);
        ebisu_openssl_socket_init(&sock, 1, 1);
        ebisu_openssl_socket_set_connect_timeout(&sock, 30);
        auto start = std::chrono::steady_clock::now();
        CHECK( sock.base.ops->connect(&sock.base, "127.0.0.1", server.port) == EBISU_SOCK_FAIL );
        CHECK( seconds_since(start) >= 0.9 );
        CHECK( seconds_since(start) < 4 );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_TIMEOUT );
    }

    SECTION("without a connect timeout, at the receive timeout alone, as before") {
        RawServer server(say_nothing);
        ebisu_openssl_socket_init(&sock, 1, 1);
        ebisu_openssl_socket_set_connect_timeout(&sock, 0);
        auto start = std::chrono::steady_clock::now();
        CHECK( sock.base.ops->connect(&sock.base, "127.0.0.1", server.port) == EBISU_SOCK_FAIL );
        CHECK( seconds_since(start) >= 0.9 );
        CHECK( seconds_since(start) < 4 );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_TIMEOUT );
    }

    SECTION("and a handshake within it leaves a socket that blocks, with its receive timeout") {
        LocalServer server;
        REQUIRE( server.listening );
        CaFile ca;
        ebisu_openssl_tls_t tls;
        REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
        ebisu_openssl_socket_init(&sock, 1, 5);
        ebisu_openssl_socket_set_connect_timeout(&sock, 5);
        ebisu_openssl_socket_set_tls(&sock, &tls);
        REQUIRE( connect_to(&sock, server) == EBISU_SOCK_OK );
        CHECK( (fcntl(sock.socket, F_GETFL, 0) & O_NONBLOCK) == 0 );
        char buffer[16];
        size_t received = 0;
        auto start = std::chrono::steady_clock::now();
        CHECK( sock.base.ops->recv(&sock.base, buffer, sizeof(buffer), &received) == EBISU_SOCK_FAIL );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_TIMEOUT );
        CHECK( seconds_since(start) >= 0.9 );
        CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
        ebisu_openssl_tls_free(&tls);
    }
}

TEST_CASE( "The OpenSSL socket says a receive timed out, or a peer went" ) {
    Connection c;
    long detail = -1;

    SECTION("a receive that times out") {
        char buffer[16];
        size_t received = 0;
        REQUIRE( c.client.base.ops->recv(&c.client.base, buffer, sizeof(buffer), &received) == EBISU_SOCK_FAIL );
        CHECK( last_error(&c.client, &detail) == EBISU_SOCK_ERROR_TIMEOUT );
        CHECK( detail == 0 );
    }

    SECTION("a send to a peer that has gone") {
        c.close_server_socket();
        const char data[] = "hello";
        size_t sent = 0;
        ebisu_sock_code_t res = EBISU_SOCK_OK;
        for (int i = 0; i < 2 && res == EBISU_SOCK_OK; ++i) {
            res = c.client.base.ops->send(&c.client.base, data, sizeof(data), &sent);
        }
        REQUIRE( res == EBISU_SOCK_FAIL );
        CHECK( last_error(&c.client, &detail) == EBISU_SOCK_ERROR_CLOSED );
        CHECK( detail == EPIPE );
    }
}

namespace {

struct LogCtx;

struct MockLogger {
    ebisu_logger_t base;
    LogCtx* owner;
};

// A logger that keeps what it is sent.
struct LogCtx {
    std::vector<std::pair<ebisu_log_level_t, std::string>> messages;
    MockLogger logger;

    LogCtx();
    ebisu_logger_t* get() {
        return &logger.base;
    }
};

void record_log(ebisu_logger_t* self, ebisu_log_level_t level, const char* fmt, va_list args)
{
    char message[512];
    vsnprintf(message, sizeof(message), fmt, args);
    reinterpret_cast<MockLogger*>(self)->owner->messages.emplace_back(level, message);
}

const ebisu_logger_ops_t record_log_ops = { record_log };

LogCtx::LogCtx()
{
    logger.base.ops = &record_log_ops;
    logger.owner = this;
}

// What f wrote to stream, whose descriptor is fd.
std::string written_to(FILE* stream, int fd, const std::function<void()>& f)
{
    fflush(stream);
    FILE* captured = tmpfile();
    REQUIRE( captured != nullptr );
    int saved = dup(fd);
    dup2(fileno(captured), fd);
    f();
    fflush(stream);
    dup2(saved, fd);
    close(saved);
    std::string out;
    rewind(captured);
    char buff[256];
    size_t n;
    while ((n = fread(buff, 1, sizeof(buff), captured)) > 0) {
        out.append(buff, n);
    }
    fclose(captured);
    return out;
}

// What f wrote to stderr.
std::string stderr_of(const std::function<void()>& f)
{
    return written_to(stderr, 2, f);
}

#ifndef EBISU_NO_LOG
// What f wrote to stdout. Catch writes there too, so f mustn't check.
std::string stdout_of(const std::function<void()>& f)
{
    return written_to(stdout, 1, f);
}

bool starts_with(const std::string& s, const std::string& prefix)
{
    return s.compare(0, prefix.size(), prefix) == 0;
}
#endif

bool has(const std::string& s, const std::string& part)
{
    return s.find(part) != std::string::npos;
}

}

#ifndef EBISU_NO_LOG
// The port printed why it failed with printf(), to stdout, where an
// application running as a daemon had no say in where it went. Now it logs
// only to a logger, as the rest of ebisu does.
TEST_CASE( "The OpenSSL socket says why it failed to its logger, and nowhere without one" ) {
    SECTION("a host that doesn't resolve") {
        for (unsigned int timeout : { 5u, 0u }) {
            INFO( "connect timeout " << timeout );
            LogCtx log;
            ebisu_openssl_socket_t sock;
            ebisu_openssl_socket_init(&sock, 0, 0);
            ebisu_openssl_socket_set_connect_timeout(&sock, timeout);
            ebisu_openssl_socket_set_logger(&sock, log.get());
            std::string err = stderr_of([&sock] {
                CHECK( sock.base.ops->connect(&sock.base, "ebisu-test.invalid", 443) == EBISU_SOCK_FAIL );
            });
            // Not empty, perhaps: under qemu-user, the emulator writes its own
            // warnings to the same stderr.
            CHECK_FALSE( has(err, "failed to") );
            REQUIRE( log.messages.size() == 1 );
            CHECK( log.messages[0].first == EBISU_LOG_ERROR );
            CHECK( starts_with(log.messages[0].second, "failed to look up ebisu-test.invalid: ") );
            CHECK( log.messages[0].second.find('\n') == std::string::npos );
        }
    }

    SECTION("a certificate the socket doesn't trust") {
        LocalServer server;
        REQUIRE( server.listening );
        LogCtx log;
        ebisu_openssl_socket_t sock;
        ebisu_openssl_socket_init(&sock, 5, 5);
        ebisu_openssl_socket_set_logger(&sock, log.get());
        std::string err = stderr_of([&sock, &server] {
            CHECK( connect_to(&sock, server) == EBISU_SOCK_FAIL );
        });
        CHECK_FALSE( has(err, "server certificate") );
        CHECK_FALSE( has(err, "failed to") );
        REQUIRE( log.messages.size() == 1 );
        CHECK( log.messages[0].first == EBISU_LOG_ERROR );
        CHECK( starts_with(log.messages[0].second, "server certificate for localhost rejected: ") );
    }

    SECTION("without a logger, nowhere") {
        LocalServer server;
        REQUIRE( server.listening );
        ebisu_openssl_socket_t unresolved;
        ebisu_openssl_socket_init(&unresolved, 0, 0);
        ebisu_openssl_socket_t untrusted;
        ebisu_openssl_socket_init(&untrusted, 5, 5);
        ebisu_sock_code_t resolve = EBISU_SOCK_OK;
        ebisu_sock_code_t trust = EBISU_SOCK_OK;
        std::string out;
        std::string err = stderr_of([&] {
            out = stdout_of([&] {
                resolve = unresolved.base.ops->connect(&unresolved.base, "ebisu-test.invalid", 443);
                trust = connect_to(&untrusted, server);
            });
        });
        CHECK( resolve == EBISU_SOCK_FAIL );
        CHECK( trust == EBISU_SOCK_FAIL );
        // Not empty, perhaps, as above.
        CHECK_FALSE( has(err, "failed to") );
        CHECK_FALSE( has(err, "server certificate") );
        CHECK_FALSE( has(out, "failed to") );
        CHECK_FALSE( has(out, "server certificate") );
        // Why, last_error still says.
        long detail = 0;
        CHECK( unresolved.base.ops->last_error(&unresolved.base, &detail) == EBISU_SOCK_ERROR_DNS );
        CHECK( untrusted.base.ops->last_error(&untrusted.base, &detail) == EBISU_SOCK_ERROR_VERIFY );
    }
}
#else
// Built without logging, it says why it failed through last_error alone.
TEST_CASE( "The OpenSSL socket built without logging logs nothing, to its logger or to stderr" ) {
    long detail = 0;

    SECTION("a host that doesn't resolve") {
        LogCtx log;
        ebisu_openssl_socket_t sock;
        ebisu_openssl_socket_init(&sock, 0, 0);
        ebisu_openssl_socket_set_logger(&sock, log.get());
        std::string err = stderr_of([&sock] {
            CHECK( sock.base.ops->connect(&sock.base, "ebisu-test.invalid", 443) == EBISU_SOCK_FAIL );
        });
        CHECK_FALSE( has(err, "failed to") );
        CHECK( log.messages.empty() );
        CHECK( sock.base.ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_DNS );
    }

    SECTION("a certificate the socket doesn't trust") {
        LocalServer server;
        REQUIRE( server.listening );
        LogCtx log;
        ebisu_openssl_socket_t sock;
        ebisu_openssl_socket_init(&sock, 5, 5);
        ebisu_openssl_socket_set_logger(&sock, log.get());
        std::string err = stderr_of([&sock, &server] {
            CHECK( connect_to(&sock, server) == EBISU_SOCK_FAIL );
        });
        CHECK_FALSE( has(err, "server certificate") );
        CHECK( log.messages.empty() );
        CHECK( sock.base.ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_VERIFY );
    }

    SECTION("without a logger") {
        ebisu_openssl_socket_t sock;
        ebisu_openssl_socket_init(&sock, 0, 0);
        std::string err = stderr_of([&sock] {
            CHECK( sock.base.ops->connect(&sock.base, "ebisu-test.invalid", 443) == EBISU_SOCK_FAIL );
        });
        CHECK_FALSE( has(err, "failed to") );
    }
}
#endif

namespace {

// The lookups the C library's getaddrinfo() is let do for hosts in this
// domain, which a test holds back while it keeps the gate closed, as a lookup
// waits on name servers that don't answer.
const char GATED_DOMAIN[] = ".lookup-gate.invalid";

pthread_mutex_t gate_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gate_cond = PTHREAD_COND_INITIALIZER;
bool gate_closed = false;
// Lookups of the domain started, and those held back still.
int gated_lookups = 0;
int gated_waiting = 0;

bool is_gated(const char* host)
{
    size_t length = strlen(host);
    size_t domain = sizeof(GATED_DOMAIN) - 1;
    return length > domain && strcmp(host + length - domain, GATED_DOMAIN) == 0;
}

typedef int (*getaddrinfo_t)(const char*, const char*, const struct addrinfo*,
        struct addrinfo**);

getaddrinfo_t libc_getaddrinfo()
{
    static getaddrinfo_t libc = (getaddrinfo_t)dlsym(RTLD_NEXT, "getaddrinfo");
    return libc;
}

// Closes the gate while it lives, and opens it again once the lookups it held
// back have gone through.
struct ClosedGate {
    ClosedGate() { close(); }

    ~ClosedGate() { open(); }

    void close()
    {
        pthread_mutex_lock(&gate_mutex);
        gate_closed = true;
        pthread_mutex_unlock(&gate_mutex);
    }

    void open()
    {
        pthread_mutex_lock(&gate_mutex);
        gate_closed = false;
        pthread_cond_broadcast(&gate_cond);
        while (gated_waiting > 0) {
            pthread_cond_wait(&gate_cond, &gate_mutex);
        }
        pthread_mutex_unlock(&gate_mutex);
    }

    int lookups()
    {
        pthread_mutex_lock(&gate_mutex);
        int count = gated_lookups;
        pthread_mutex_unlock(&gate_mutex);
        return count;
    }
};

ebisu_sock_code_t connect_within(ebisu_openssl_socket_t* sock, unsigned int timeout,
        const char* host, uint16_t port)
{
    ebisu_openssl_socket_init(sock, 0, 0);
    ebisu_openssl_socket_set_connect_timeout(sock, timeout);
    return sock->base.ops->connect(&sock->base, host, port);
}

}

// The one the port calls, rather than the C library's, which it hands what the
// gate doesn't hold back, and the lookups it held back once it opens, for the
// loopback.
extern "C" int getaddrinfo(const char* host, const char* service,
        const struct addrinfo* hints, struct addrinfo** out)
{
    if (host == nullptr || !is_gated(host)) {
        return libc_getaddrinfo()(host, service, hints, out);
    }
    pthread_mutex_lock(&gate_mutex);
    ++gated_lookups;
    ++gated_waiting;
    while (gate_closed) {
        pthread_cond_wait(&gate_cond, &gate_mutex);
    }
    int ret = libc_getaddrinfo()("127.0.0.1", service, hints, out);
    --gated_waiting;
    pthread_cond_broadcast(&gate_cond);
    pthread_mutex_unlock(&gate_mutex);
    return ret;
}

// A connect that gave up on a lookup left it running on its thread, as
// getaddrinfo() can't be stopped, and the next connect started another: where
// the name servers didn't answer, each connect left a thread waiting.
TEST_CASE( "The OpenSSL socket doesn't look a host up again while a lookup of it runs" ) {
    const uint16_t port = closed_port();
    ClosedGate gate;
    const int before = gate.lookups();
    ebisu_openssl_socket_t sock;
    long detail = 0;
    for (int i = 0; i < 3; ++i) {
        INFO( "connect " << i );
        CHECK( connect_within(&sock, 1, "a.lookup-gate.invalid", port) == EBISU_SOCK_FAIL );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_TIMEOUT );
    }
    CHECK( gate.lookups() - before == 1 );

    // Another host, or another port, is looked up by itself.
    CHECK( connect_within(&sock, 1, "b.lookup-gate.invalid", port) == EBISU_SOCK_FAIL );
    CHECK( connect_within(&sock, 1, "a.lookup-gate.invalid", (uint16_t)(port ^ 1)) ==
            EBISU_SOCK_FAIL );
    CHECK( gate.lookups() - before == 3 );

    // Let through, the lookup gives the loopback, where nothing listens.
    gate.open();
    CHECK( connect_within(&sock, 5, "a.lookup-gate.invalid", port) == EBISU_SOCK_FAIL );
    CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_CONNECT );

    // Done, a lookup isn't waited for again: the next is one of its own.
    gate.close();
    const int done = gate.lookups();
    CHECK( connect_within(&sock, 1, "a.lookup-gate.invalid", port) == EBISU_SOCK_FAIL );
    CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_TIMEOUT );
    CHECK( gate.lookups() - done == 1 );
}

TEST_CASE( "OpenSSL sockets waiting on a running lookup take its addresses when it ends" ) {
    const uint16_t port = closed_port();
    ClosedGate gate;
    const int before = gate.lookups();
    ebisu_openssl_socket_t first;
    CHECK( connect_within(&first, 1, "c.lookup-gate.invalid", port) == EBISU_SOCK_FAIL );

    // Sockets of other tasks, which wait for the lookup the first gave up on.
    const int count = 4;
    std::vector<ebisu_sock_code_t> codes(count, EBISU_SOCK_OK);
    std::vector<ebisu_sock_error_t> errors(count, EBISU_SOCK_ERROR_NONE);
    std::vector<std::thread> tasks;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < count; ++i) {
        tasks.emplace_back([&codes, &errors, i, port] {
            ebisu_openssl_socket_t sock;
            codes[i] = connect_within(&sock, 10, "c.lookup-gate.invalid", port);
            long detail = 0;
            errors[i] = sock.base.ops->last_error(&sock.base, &detail);
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    CHECK( gate.lookups() - before == 1 );
    gate.open();
    for (auto& task : tasks) {
        task.join();
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    for (int i = 0; i < count; ++i) {
        INFO( "task " << i );
        CHECK( codes[i] == EBISU_SOCK_FAIL );
        CHECK( errors[i] == EBISU_SOCK_ERROR_CONNECT );
    }
    CHECK( std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 8 );
}

namespace {

// The SHA-256 of cert in DER, or of its public key's SubjectPublicKeyInfo.
std::vector<unsigned char> certificate_pin(X509* cert)
{
    std::vector<unsigned char> pin(EBISU_TLS_PIN_SIZE);
    unsigned int len = 0;
    REQUIRE( X509_digest(cert, EVP_sha256(), pin.data(), &len) == 1 );
    REQUIRE( len == EBISU_TLS_PIN_SIZE );
    return pin;
}

std::vector<unsigned char> public_key_pin(X509* cert)
{
    EVP_PKEY* key = X509_get_pubkey(cert);
    unsigned char* der = nullptr;
    int der_len = i2d_PUBKEY(key, &der);
    REQUIRE( der_len > 0 );
    std::vector<unsigned char> pin(EBISU_TLS_PIN_SIZE);
    unsigned int len = 0;
    REQUIRE( EVP_Digest(der, (size_t)der_len, pin.data(), &len, EVP_sha256(), nullptr) == 1 );
    OPENSSL_free(der);
    EVP_PKEY_free(key);
    return pin;
}

ebisu_sock_code_t connect_by_address(ebisu_openssl_socket_t* sock, const LocalServer& server)
{
    return sock->base.ops->connect(&sock->base, "127.0.0.1", server.port);
}

bool logged(const LogCtx& log, ebisu_log_level_t level, const std::string& part)
{
    for (const auto& m : log.messages) {
        if (m.first == level && m.second.find(part) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}

// The server's certificate is for localhost, self-signed, and the server is
// reached at 127.0.0.1, a name it isn't issued for.
TEST_CASE( "The OpenSSL socket checks the server's certificate as its configuration says" ) {
    LocalServer server;
    REQUIRE( server.listening );
    CaFile ca;
    LogCtx log;
    ebisu_openssl_socket_t sock;
    ebisu_openssl_socket_init(&sock, 5, 5);
    ebisu_openssl_socket_set_logger(&sock, log.get());
    long detail = 0;

    SECTION("in full, by default: its name too") {
        ebisu_openssl_tls_t tls;
        REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
        ebisu_openssl_socket_set_tls(&sock, &tls);
        CHECK( connect_by_address(&sock, server) == EBISU_SOCK_FAIL );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( detail == X509_V_ERR_IP_ADDRESS_MISMATCH );
        REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_FULL, nullptr) );
        CHECK( connect_by_address(&sock, server) == EBISU_SOCK_FAIL );
        ebisu_openssl_tls_free(&tls);
    }

    SECTION("its chain without its name") {
        ebisu_openssl_tls_t tls;
        REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
        REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_CHAIN, nullptr) );
        ebisu_openssl_socket_set_tls(&sock, &tls);
        REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_OK );
        CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
        CHECK_FALSE( logged(log, EBISU_LOG_WARNING, "without verifying") );
        ebisu_openssl_tls_free(&tls);

        // Still to a CA certificate trusted.
        REQUIRE( ebisu_openssl_tls_init(&tls, nullptr, nullptr, nullptr) );
        REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_CHAIN, nullptr) );
        CHECK( connect_by_address(&sock, server) == EBISU_SOCK_FAIL );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( detail == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT );
        ebisu_openssl_tls_free(&tls);
    }

    SECTION("pinned, its certificate or its public key, with no CA certificate of its") {
        for (const std::vector<unsigned char>& pin :
                { certificate_pin(server_certificate()), public_key_pin(server_certificate()) }) {
            ebisu_openssl_tls_t tls;
            REQUIRE( ebisu_openssl_tls_init(&tls, nullptr, nullptr, nullptr) );
            REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_PIN, pin.data()) );
            ebisu_openssl_socket_set_tls(&sock, &tls);
            REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_OK );
            CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
            ebisu_openssl_tls_free(&tls);
        }
    }

    SECTION("pinned, and another certificate refused") {
        std::vector<unsigned char> pin = certificate_pin(server_certificate());
        pin[0] ^= 1;
        ebisu_openssl_tls_t tls;
        REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
        REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_PIN, pin.data()) );
        ebisu_openssl_socket_set_tls(&sock, &tls);
        // At localhost, which its chain and its name would have passed.
        CHECK( connect_to(&sock, server) == EBISU_SOCK_FAIL );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( detail == X509_V_ERR_CERT_REJECTED );
#ifndef EBISU_NO_LOG
        CHECK( logged(log, EBISU_LOG_ERROR, "neither it nor its public key has the pin") );
#endif
        ebisu_openssl_tls_free(&tls);
    }

    SECTION("pinned, whatever its dates") {
        X509* expired = issue_certificate(server_key(), -7200, -3600);
        REQUIRE( SSL_CTX_use_certificate(server.ctx, expired) == 1 );
        ebisu_openssl_tls_t tls;
        REQUIRE( ebisu_openssl_tls_init(&tls, nullptr, nullptr, nullptr) );
        REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_PIN,
                    certificate_pin(expired).data()) );
        ebisu_openssl_socket_set_tls(&sock, &tls);
        REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_OK );
        CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
        ebisu_openssl_tls_free(&tls);
        X509_free(expired);
    }

    SECTION("not at all, with a warning on each connection") {
        ebisu_openssl_tls_t tls;
        REQUIRE( ebisu_openssl_tls_init(&tls, nullptr, nullptr, nullptr) );
        REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_NONE, nullptr) );
        ebisu_openssl_socket_set_tls(&sock, &tls);
        for (int i = 0; i < 2; ++i) {
            REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_OK );
            CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
        }
#ifndef EBISU_NO_LOG
        int warnings = 0;
        for (const auto& m : log.messages) {
            if (m.first == EBISU_LOG_WARNING &&
                    m.second == "connected to 127.0.0.1 without verifying its certificate.") {
                ++warnings;
            }
        }
        CHECK( warnings == 2 );
#endif
        ebisu_openssl_tls_free(&tls);
    }
}

// close used to shut down, free and close what the socket held, whether or
// not it had a connection: NULL on a socket never connected, and the last
// connection's SSL, freed already, and descriptor, maybe another's by then, on
// one used again. A close after a connect that failed was one, or a close
// twice; khc and the MQTT task close only a connection made.
TEST_CASE( "The OpenSSL socket's close does nothing without a connection" ) {
    LocalServer server;
    REQUIRE( server.listening );
    CaFile ca;
    LogCtx log;
    ebisu_openssl_tls_t tls;
    REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
    ebisu_openssl_socket_t sock;
    ebisu_openssl_socket_init(&sock, 5, 5);
    ebisu_openssl_socket_set_logger(&sock, log.get());
    // In full, the server's certificate, which isn't for the address
    // connected to, is refused; its chain alone is taken.
    ebisu_openssl_socket_set_tls(&sock, &tls);
    ebisu_socket_t* base = &sock.base;

    SECTION("never connected") {
        CHECK( base->ops->close(base) == EBISU_SOCK_OK );
    }
    SECTION("a connect that failed, never connected before") {
        REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_FAIL );
        CHECK( base->ops->close(base) == EBISU_SOCK_OK );
    }
    SECTION("a connect that failed, after a connection closed") {
        REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_CHAIN, nullptr) );
        REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_OK );
        CHECK( base->ops->close(base) == EBISU_SOCK_OK );
        // The lowest descriptor free, which the connection's was.
        int other = open("/dev/null", O_RDONLY);
        REQUIRE( other >= 0 );
        REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_FULL, nullptr) );
        REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_FAIL );
        CHECK( base->ops->close(base) == EBISU_SOCK_OK );
        CHECK( fcntl(other, F_GETFD) != -1 );
        close(other);

        // And connects again.
        REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_CHAIN, nullptr) );
        REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_OK );
        CHECK( base->ops->close(base) == EBISU_SOCK_OK );
    }
    SECTION("closed already") {
        REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_CHAIN, nullptr) );
        REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_OK );
        CHECK( base->ops->close(base) == EBISU_SOCK_OK );
        CHECK( base->ops->close(base) == EBISU_SOCK_OK );
    }
    CHECK( sock.ssl == nullptr );
    CHECK( sock.socket == -1 );
    CHECK_FALSE( logged(log, EBISU_LOG_ERROR, "shutdown") );
    CHECK_FALSE( logged(log, EBISU_LOG_ERROR, "failed to close") );
    ebisu_openssl_tls_free(&tls);
}

namespace {

// An option of the socket's connection, as getsockopt() reads it, or -1.
long option_of(const ebisu_openssl_socket_t& sock, int level, int name)
{
    int value = 0;
    socklen_t len = sizeof(value);
    if (getsockopt(sock.socket, level, name, &value, &len) != 0) {
        return -1;
    }
    return value;
}

}

TEST_CASE( "The OpenSSL socket checks an idle connection, and sends at once, when asked" ) {
    LocalServer server;
    REQUIRE( server.listening );
    CaFile ca;
    LogCtx log;
    ebisu_openssl_tls_t tls;
    REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
    REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_CHAIN, nullptr) );
    ebisu_openssl_socket_t sock;
    ebisu_openssl_socket_init(&sock, 5, 5);
    ebisu_openssl_socket_set_logger(&sock, log.get());
    ebisu_openssl_socket_set_tls(&sock, &tls);
#if defined(TCP_KEEPIDLE)
    const int idle_option = TCP_KEEPIDLE;
#elif defined(TCP_KEEPALIVE)
    const int idle_option = TCP_KEEPALIVE;
#endif

    REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_OK );
    CHECK( option_of(sock, SOL_SOCKET, SO_KEEPALIVE) == 0 );
    CHECK( option_of(sock, IPPROTO_TCP, TCP_NODELAY) == 0 );
    // From the next connect.
    ebisu_openssl_socket_set_keepalive(&sock, 30, 5, 4);
    ebisu_openssl_socket_set_nodelay(&sock, true);
    CHECK( option_of(sock, SOL_SOCKET, SO_KEEPALIVE) == 0 );
    REQUIRE( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );

    REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_OK );
    CHECK( option_of(sock, SOL_SOCKET, SO_KEEPALIVE) != 0 );
    CHECK( option_of(sock, IPPROTO_TCP, idle_option) == 30 );
    CHECK( option_of(sock, IPPROTO_TCP, TCP_KEEPINTVL) == 5 );
    CHECK( option_of(sock, IPPROTO_TCP, TCP_KEEPCNT) == 4 );
    CHECK( option_of(sock, IPPROTO_TCP, TCP_NODELAY) != 0 );
    CHECK_FALSE( logged(log, EBISU_LOG_WARNING, "failed to set") );
    REQUIRE( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );

    ebisu_openssl_socket_set_keepalive(&sock, 0, 5, 4);
    ebisu_openssl_socket_set_nodelay(&sock, false);
    REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_OK );
    CHECK( option_of(sock, SOL_SOCKET, SO_KEEPALIVE) == 0 );
    CHECK( option_of(sock, IPPROTO_TCP, TCP_NODELAY) == 0 );
    REQUIRE( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
    ebisu_openssl_tls_free(&tls);
}

// A session resumed has no certificate to check: those kept under a mode that
// checked less would let the next connection skip the check.
TEST_CASE( "A TLS configuration checks in full again once set to, with no session kept" ) {
    LocalServer server(AF_INET, true);
    REQUIRE( server.listening );
    ebisu_openssl_tls_t tls;
    REQUIRE( ebisu_openssl_tls_init(&tls, nullptr, nullptr, nullptr) );
    REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_NONE, nullptr) );
    ebisu_openssl_socket_t sock;
    ebisu_openssl_socket_init(&sock, 5, 5);
    ebisu_openssl_socket_set_tls(&sock, &tls);
    CHECK_FALSE( connect_resumed(&sock, server) );
    CHECK( connect_resumed(&sock, server) );

    REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_FULL, nullptr) );
    CHECK( connect_to(&sock, server) == EBISU_SOCK_FAIL );
    long detail = 0;
    CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_VERIFY );
    ebisu_openssl_tls_free(&tls);
}

TEST_CASE( "A TLS configuration refuses a mode it can't take, as it was" ) {
    LocalServer server;
    REQUIRE( server.listening );
    CaFile ca;
    ebisu_openssl_tls_t tls;
    REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
    CHECK_FALSE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_PIN, nullptr) );
    CHECK_FALSE( ebisu_openssl_tls_set_verify(&tls, (ebisu_tls_verify_t)42, nullptr) );
    // Still in full: by name, at localhost.
    ebisu_openssl_socket_t sock;
    ebisu_openssl_socket_init(&sock, 5, 5);
    ebisu_openssl_socket_set_tls(&sock, &tls);
    REQUIRE( connect_to(&sock, server) == EBISU_SOCK_OK );
    CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
    CHECK( connect_by_address(&sock, server) == EBISU_SOCK_FAIL );
    ebisu_openssl_tls_free(&tls);

    ebisu_openssl_tls_t unset;
    CHECK_FALSE( ebisu_openssl_tls_init(&unset, "/nonexistent/ca.pem", nullptr, nullptr) );
    CHECK_FALSE( ebisu_openssl_tls_set_verify(&unset, EBISU_TLS_VERIFY_NONE, nullptr) );
}


// The port takes TLS 1.2 or later, with ECDHE or DHE and AES-GCM or
// ChaCha20-Poly1305. A server that takes only older ones answered with
// OpenSSL's text alone, such as "tlsv1 alert protocol version".
TEST_CASE( "The OpenSSL socket says when the server has no TLS version or suite in common with it" ) {
    CaFile ca;
    struct Case {
        const char* what;
        int version;
        const char* ciphers;
    };
    const Case cases[] = {
        { "TLS 1.0 only", TLS1_VERSION, "AES128-SHA" },
        { "TLS 1.1 only", TLS1_1_VERSION, "AES128-SHA" },
        { "TLS 1.2 with neither forward secrecy nor AEAD", TLS1_2_VERSION, "AES128-SHA" },
        { "TLS 1.2 without forward secrecy", TLS1_2_VERSION, "AES128-GCM-SHA256" },
        { "TLS 1.2 without AEAD", TLS1_2_VERSION, "ECDHE-RSA-AES128-SHA" },
    };
    for (const Case& c : cases) {
        CAPTURE( c.what );
        LocalServer server(AF_INET, false, c.version, c.ciphers);
        REQUIRE( server.listening );
        LogCtx log;
        ebisu_openssl_tls_t tls;
        REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
        REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_CHAIN, nullptr) );
        ebisu_openssl_socket_t sock;
        ebisu_openssl_socket_init(&sock, 5, 5);
        ebisu_openssl_socket_set_logger(&sock, log.get());
        ebisu_openssl_socket_set_tls(&sock, &tls);
        CHECK( connect_by_address(&sock, server) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_TLS_MISMATCH );
        CHECK( detail != 0 );
        CAPTURE( ERR_GET_REASON((unsigned long)detail) );
#ifdef EBISU_NO_LOG
        CHECK( log.messages.empty() );
#else
        CHECK( logged(log, EBISU_LOG_ERROR, "failed to connect") );
        CHECK( logged(log, EBISU_LOG_ERROR,
                "127.0.0.1 may take only TLS older than 1.2, or TLS 1.2 without ECDHE or DHE "
                "with AES-GCM or ChaCha20-Poly1305, which this port needs.") );
#endif
        CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
        ebisu_openssl_tls_free(&tls);
    }

    LogCtx log;
    ebisu_openssl_tls_t tls;
    REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
    REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_CHAIN, nullptr) );
    ebisu_openssl_socket_t sock;
    ebisu_openssl_socket_init(&sock, 5, 5);
    ebisu_openssl_socket_set_logger(&sock, log.get());
    ebisu_openssl_socket_set_tls(&sock, &tls);
    long detail = 0;

    SECTION("for each alert that says so, and no other") {
        // handshake_failure, protocol_version, insufficient_security; and
        // internal_error, which doesn't.
        for (int description : { 40, 70, 71, 80 }) {
            CAPTURE( description );
            PlainServer alerting(fatal_alert((unsigned char)description));
            CHECK( sock.base.ops->connect(&sock.base, "127.0.0.1", alerting.port) == EBISU_SOCK_FAIL );
            CHECK( last_error(&sock, &detail) ==
                    (description == 80 ? EBISU_SOCK_ERROR_TLS : EBISU_SOCK_ERROR_TLS_MISMATCH) );
            CHECK( ERR_GET_REASON((unsigned long)detail) == 1000 + description );
        }
    }
    SECTION("for a server that answers in TLS 1.0, which OpenSSL refuses itself") {
        PlainServer old(std::string((const char*)TLS10_SERVER_HELLO, sizeof(TLS10_SERVER_HELLO)));
        CHECK( sock.base.ops->connect(&sock.base, "127.0.0.1", old.port) == EBISU_SOCK_FAIL );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_TLS_MISMATCH );
        CAPTURE( ERR_GET_REASON((unsigned long)detail) );
#ifndef EBISU_NO_LOG
        CHECK( logged(log, EBISU_LOG_ERROR, "may take only") );
#endif
    }
    SECTION("but not for a server that isn't TLS") {
        PlainServer plain;
        CHECK( sock.base.ops->connect(&sock.base, "127.0.0.1", plain.port) == EBISU_SOCK_FAIL );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_TLS );
        CAPTURE( ERR_GET_REASON((unsigned long)detail) );
        CHECK( ERR_GET_REASON((unsigned long)detail) != 0 );
        CHECK_FALSE( logged(log, EBISU_LOG_ERROR, "may take only") );
    }
    SECTION("nor for one that has both") {
        LocalServer server(AF_INET, false, TLS1_2_VERSION, nullptr);
        REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_OK );
        CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
    }
    SECTION("nor for a certificate refused") {
        REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_FULL, nullptr) );
        LocalServer server;
        CHECK( connect_by_address(&sock, server) == EBISU_SOCK_FAIL );
        CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK_FALSE( logged(log, EBISU_LOG_ERROR, "may take only") );
    }
    ebisu_openssl_tls_free(&tls);
}

// OpenSSL 4.1 matches the host against the certificate's DNS subjectAltNames
// alone. Earlier versions take its commonName for one that has none.
TEST_CASE( "The OpenSSL socket takes a host's commonName for its name only before OpenSSL 4.1" ) {
    static X509* cn_only = issue_certificate(server_key(), 0, 3600, false);
    LocalServer server;
    REQUIRE( server.listening );
    REQUIRE( SSL_CTX_use_certificate(server.ctx, cn_only) == 1 );
    CaFile ca(cn_only);
    ebisu_openssl_tls_t tls;
    REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
    ebisu_openssl_socket_t sock;
    ebisu_openssl_socket_init(&sock, 5, 5);
    ebisu_openssl_socket_set_tls(&sock, &tls);
    long detail = 0;
#if (OPENSSL_VERSION_NUMBER >= 0x40100000L)
    CHECK( connect_to(&sock, server) == EBISU_SOCK_FAIL );
    CHECK( last_error(&sock, &detail) == EBISU_SOCK_ERROR_VERIFY );
    CHECK( detail == X509_V_ERR_HOSTNAME_MISMATCH );
#else
    CHECK( connect_to(&sock, server) == EBISU_SOCK_OK );
    (void)detail;
#endif
    CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
    ebisu_openssl_tls_free(&tls);
}

namespace {

// The server name of the last ClientHello a server of this file took, or
// "none" for a ClientHello without one.
std::string sni_seen;

int note_sni(SSL* ssl, int*, void*)
{
    const char* name = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    sni_seen = name != nullptr ? name : "none";
    return SSL_TLSEXT_ERR_OK;
}

// What connecting to host at the server gives, in full verification, with
// the server's certificate cert, which is its own CA.
ebisu_sock_error_t connect_in_full(X509* cert, int family, const char* host, long* out_detail)
{
    *out_detail = 0;
    sni_seen = "no handshake";
    LocalServer server(family);
    REQUIRE( server.listening );
    REQUIRE( SSL_CTX_use_certificate(server.ctx, cert) == 1 );
    REQUIRE( SSL_CTX_set_tlsext_servername_callback(server.ctx, note_sni) == 1 );
    CaFile ca(cert);
    ebisu_openssl_tls_t tls;
    REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
    ebisu_openssl_socket_t sock;
    ebisu_openssl_socket_init(&sock, 5, 5);
    ebisu_openssl_socket_set_tls(&sock, &tls);
    ebisu_sock_error_t error = EBISU_SOCK_ERROR_NONE;
    if (sock.base.ops->connect(&sock.base, host, server.port) != EBISU_SOCK_OK) {
        error = last_error(&sock, out_detail);
    }
    CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
    ebisu_openssl_tls_free(&tls);
    return error;
}

}

// A certificate names the addresses it is issued for apart from the names:
// in subjectAltNames of a kind of their own. And a ClientHello's server name
// is a name, never an address (RFC 6066, section 3).
TEST_CASE( "The OpenSSL socket checks a host given as an address against the certificate's addresses" ) {
    long detail = 0;

    SECTION("an IPv4 address the certificate is issued for") {
        static X509* cert = issue_certificate(server_key(), 0, 3600, true, "IP:127.0.0.1");
        CHECK( connect_in_full(cert, AF_INET, "127.0.0.1", &detail) == EBISU_SOCK_ERROR_NONE );
        CHECK( sni_seen == "none" );
        // In the other forms getaddrinfo() takes.
        CHECK( connect_in_full(cert, AF_INET, "127.1", &detail) == EBISU_SOCK_ERROR_NONE );
        CHECK( sni_seen == "none" );
        CHECK( connect_in_full(cert, AF_INET, "0x7f000001", &detail) == EBISU_SOCK_ERROR_NONE );
        CHECK( sni_seen == "none" );
    }
    SECTION("an IPv6 address the certificate is issued for, however it is written") {
        static X509* cert = issue_certificate(server_key(), 0, 3600, true, "IP:::1");
        LocalServer probe(AF_INET6);
        if (!probe.listening) {
            WARN( "no IPv6 loopback: skipped" );
        } else {
            CHECK( connect_in_full(cert, AF_INET6, "::1", &detail) == EBISU_SOCK_ERROR_NONE );
            CHECK( sni_seen == "none" );
            CHECK( connect_in_full(cert, AF_INET6, "0:0:0:0:0:0:0:1", &detail) == EBISU_SOCK_ERROR_NONE );
            CHECK( sni_seen == "none" );
            // With a scope, which a certificate has none of.
            CHECK( connect_in_full(cert, AF_INET6, "::1%1", &detail) == EBISU_SOCK_ERROR_NONE );
            CHECK( sni_seen == "none" );
        }
    }
    SECTION("an address the certificate isn't issued for") {
        static X509* cert = issue_certificate(server_key(), 0, 3600, true, "IP:127.0.0.2");
        CHECK( connect_in_full(cert, AF_INET, "127.0.0.1", &detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( detail == X509_V_ERR_IP_ADDRESS_MISMATCH );
        CHECK( sni_seen == "none" );
    }
    SECTION("an address the certificate has as a name, which is no address") {
        static X509* cert = issue_certificate(server_key(), 0, 3600, true, "DNS:127.0.0.1");
        CHECK( connect_in_full(cert, AF_INET, "127.0.0.1", &detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( detail == X509_V_ERR_IP_ADDRESS_MISMATCH );
    }
    SECTION("a name, as before: sent to the server, and checked against the names") {
        static X509* cert = issue_certificate(server_key(), 0, 3600, true,
                "DNS:localhost,IP:127.0.0.1");
        CHECK( connect_in_full(cert, AF_INET, "localhost", &detail) == EBISU_SOCK_ERROR_NONE );
        CHECK( sni_seen == "localhost" );
        // With a name of another host, for no version of OpenSSL to take the
        // commonName, which is localhost.
        static X509* other = issue_certificate(server_key(), 0, 3600, true,
                "DNS:other.invalid,IP:127.0.0.1");
        CHECK( connect_in_full(other, AF_INET, "localhost", &detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( detail == X509_V_ERR_HOSTNAME_MISMATCH );
        CHECK( sni_seen == "localhost" );
    }
}

TEST_CASE( "The OpenSSL socket gives the TLS version and cipher suite its connection negotiated" ) {
    CaFile ca;
    ebisu_openssl_tls_t tls;
    REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr, nullptr) );
    REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_CHAIN, nullptr) );
    ebisu_openssl_socket_t sock;
    ebisu_openssl_socket_init(&sock, 5, 5);
    ebisu_openssl_socket_set_tls(&sock, &tls);
    CHECK( ebisu_openssl_socket_get_protocol(&sock) == nullptr );
    CHECK( ebisu_openssl_socket_get_cipher(&sock) == nullptr );

    SECTION("TLS 1.2, with one of the port's suites") {
        LocalServer server(AF_INET, false, TLS1_2_VERSION, nullptr);
        REQUIRE( server.listening );
        REQUIRE( SSL_CTX_set_cipher_list(server.ctx, "ECDHE-RSA-AES128-GCM-SHA256") == 1 );
        REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_OK );
        REQUIRE( ebisu_openssl_socket_get_protocol(&sock) != nullptr );
        CHECK( std::string(ebisu_openssl_socket_get_protocol(&sock)) == "TLSv1.2" );
        REQUIRE( ebisu_openssl_socket_get_cipher(&sock) != nullptr );
        CHECK( std::string(ebisu_openssl_socket_get_cipher(&sock)) == "ECDHE-RSA-AES128-GCM-SHA256" );
        CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
    }
#if (OPENSSL_VERSION_NUMBER >= 0x10101000L)
    SECTION("TLS 1.3, where there is TLS 1.3") {
        LocalServer server;
        REQUIRE( server.listening );
        REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_OK );
        REQUIRE( ebisu_openssl_socket_get_protocol(&sock) != nullptr );
        CHECK( std::string(ebisu_openssl_socket_get_protocol(&sock)) == "TLSv1.3" );
        REQUIRE( ebisu_openssl_socket_get_cipher(&sock) != nullptr );
        CHECK( std::string(ebisu_openssl_socket_get_cipher(&sock)).find("TLS_") == 0 );
        CHECK( sock.base.ops->close(&sock.base) == EBISU_SOCK_OK );
    }
#endif
    SECTION("nothing after a connect that failed") {
        REQUIRE( ebisu_openssl_tls_set_verify(&tls, EBISU_TLS_VERIFY_FULL, nullptr) );
        LocalServer server;
        REQUIRE( connect_by_address(&sock, server) == EBISU_SOCK_FAIL );
    }
    // Nor once closed.
    CHECK( ebisu_openssl_socket_get_protocol(&sock) == nullptr );
    CHECK( ebisu_openssl_socket_get_cipher(&sock) == nullptr );
    ebisu_openssl_tls_free(&tls);
}
