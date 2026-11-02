#include "catch.hpp"
#include "ebisu_openssl.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <chrono>
#include <string>
#include <thread>
#include <netdb.h>

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
        REQUIRE( ebisu_openssl_tls_init(&tls, nullptr, nullptr) );
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
struct LocalServer {
    int family;
    int listen_fd;
    bool listening;
    uint16_t port;
    SSL_CTX* ctx;
    bool stopping;
    std::thread thread;

    explicit LocalServer(int family = AF_INET) : family(family), listening(false), port(0),
            ctx(nullptr), stopping(false) {
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

// A file of CA certificates with the server's, removed at the end.
struct CaFile {
    char path[64];

    CaFile() {
        strcpy(path, "/tmp/ebisu_ca_XXXXXX");
        int fd = mkstemp(path);
        REQUIRE( fd >= 0 );
        FILE* f = fdopen(fd, "w");
        REQUIRE( PEM_write_X509(f, server_certificate()) == 1 );
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
        REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr) );
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
        REQUIRE( ebisu_openssl_tls_init(&tls, nullptr, ca.dir) );
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

TEST_CASE( "A TLS configuration whose CA certificates can't be loaded fails" ) {
    ebisu_openssl_tls_t tls;
    CHECK_FALSE( ebisu_openssl_tls_init(&tls, "/nonexistent/ca.pem", nullptr) );
    CHECK( tls.ssl_ctx == nullptr );
}

// The CA certificates were parsed again for every connection, each with an
// SSL_CTX of its own.
TEST_CASE( "The OpenSSL sockets of a configuration share its SSL_CTX" ) {
    LocalServer server;
    REQUIRE( server.listening );
    CaFile ca;
    ebisu_openssl_tls_t tls;
    REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr) );

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
        REQUIRE( ebisu_openssl_tls_init(&tls, ca.path, nullptr) );
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
