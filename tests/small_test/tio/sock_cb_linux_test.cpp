// The Linux sample's OpenSSL socket, against a TLS server of the test's own on
// the loopback, with a CA and certificates made for the run.
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "sock_cb_linux.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace {

EVP_PKEY* new_key()
{
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    EVP_PKEY* params = nullptr;
    EVP_PKEY_paramgen_init(pctx);
    EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1);
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    // A named curve, as 1.1.0 and later make by default.
    EVP_PKEY_CTX_set_ec_param_enc(pctx, OPENSSL_EC_NAMED_CURVE);
#endif
    EVP_PKEY_paramgen(pctx, &params);
    EVP_PKEY_CTX_free(pctx);
    EVP_PKEY_CTX* kctx = EVP_PKEY_CTX_new(params, nullptr);
    EVP_PKEY* key = nullptr;
    EVP_PKEY_keygen_init(kctx);
    EVP_PKEY_keygen(kctx, &key);
    EVP_PKEY_CTX_free(kctx);
    EVP_PKEY_free(params);
    return key;
}

void add_ext(X509* cert, X509V3_CTX* v3, int nid, const char* value)
{
    X509_EXTENSION* ext = X509V3_EXT_conf_nid(nullptr, v3, nid, (char*)value);
    X509_add_ext(cert, ext, -1);
    X509_EXTENSION_free(ext);
}

// A certificate for key, issued by issuer with issuer_key, or self-signed
// without them. san is its subjectAltName, or none.
X509* new_cert(EVP_PKEY* key, const char* cn, const char* san, bool ca,
        X509* issuer, EVP_PKEY* issuer_key, long serial)
{
    X509* cert = X509_new();
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), serial);
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    X509_gmtime_adj(X509_get_notBefore(cert), -3600);
    X509_gmtime_adj(X509_get_notAfter(cert), 24 * 3600);
#else
    X509_gmtime_adj(X509_getm_notBefore(cert), -3600);
    X509_gmtime_adj(X509_getm_notAfter(cert), 24 * 3600);
#endif
    X509_set_pubkey(cert, key);
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)cn, -1, -1, 0);
    X509_set_issuer_name(cert, issuer != nullptr ? X509_get_subject_name(issuer) : name);
    X509V3_CTX v3;
    X509V3_set_ctx_nodb(&v3);
    X509V3_set_ctx(&v3, issuer != nullptr ? issuer : cert, cert, nullptr, nullptr, 0);
    add_ext(cert, &v3, NID_basic_constraints, ca ? "critical,CA:TRUE" : "CA:FALSE");
    if (ca) {
        add_ext(cert, &v3, NID_key_usage, "critical,keyCertSign,cRLSign");
    }
    add_ext(cert, &v3, NID_subject_key_identifier, "hash");
    if (san != nullptr) {
        add_ext(cert, &v3, NID_subject_alt_name, san);
    }
    X509_sign(cert, issuer_key != nullptr ? issuer_key : key, EVP_sha256());
    return cert;
}

struct Pki {
    EVP_PKEY* ca_key;
    X509* ca;
    EVP_PKEY* key;
    // For localhost and the loopback's addresses, issued by the CA.
    X509* localhost;
    // For another name, issued by the CA.
    X509* other;
    // For localhost, but issued by no one trusted.
    EVP_PKEY* stranger_key;
    X509* stranger;
    X509* untrusted;
    std::string ca_file;

    Pki()
    {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
        SSL_library_init();
        SSL_load_error_strings();
#endif
        // The server writes to clients that may have gone.
        signal(SIGPIPE, SIG_IGN);
        ca_key = new_key();
        ca = new_cert(ca_key, "ebisu test CA", nullptr, true, nullptr, nullptr, 1);
        key = new_key();
        localhost = new_cert(key, "localhost", "DNS:localhost,IP:127.0.0.1,IP:::1", false,
                ca, ca_key, 2);
        other = new_cert(key, "other.example", "DNS:other.example", false, ca, ca_key, 3);
        stranger_key = new_key();
        stranger = new_cert(stranger_key, "stranger CA", nullptr, true, nullptr, nullptr, 1);
        untrusted = new_cert(key, "localhost", "DNS:localhost,IP:127.0.0.1", false,
                stranger, stranger_key, 4);

        // The sample trusts what OpenSSL trusts by default, which
        // SSL_CERT_FILE replaces: the CA, and no other.
        const char* tmp = getenv("TMPDIR");
        std::string path = std::string(tmp != nullptr ? tmp : "/tmp") + "/ebisu_sample_ca_XXXXXX";
        std::vector<char> name(path.begin(), path.end());
        name.push_back('\0');
        int fd = mkstemp(name.data());
        FILE* fp = fdopen(fd, "w");
        PEM_write_X509(fp, ca);
        fclose(fp);
        ca_file = name.data();
        setenv("SSL_CERT_FILE", ca_file.c_str(), 1);
    }

    ~Pki()
    {
        unlink(ca_file.c_str());
    }
};

const Pki& pki()
{
    static Pki made;
    return made;
}

// A TLS server for one connection, on 127.0.0.1 or ::1. after_handshake is
// what it does with the connection once it has one. With tls_1_1, it takes
// TLS 1.1 and no later.
struct Server {
    int listen_fd;
    uint16_t port;
    std::thread thread;
    std::string server_name;

    Server(int family, X509* cert, std::function<void(SSL*)> after_handshake,
            bool close_at_once = false, int rcvbuf = 0, bool tls_1_1 = false)
        : listen_fd(-1), port(0)
    {
        listen_fd = socket(family, SOCK_STREAM, 0);
        int on = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        if (rcvbuf > 0) {
            // Inherited by the socket accepted.
            setsockopt(listen_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
        }
        struct sockaddr_storage ss;
        memset(&ss, 0, sizeof(ss));
        socklen_t len;
        if (family == AF_INET6) {
            struct sockaddr_in6* in6 = (struct sockaddr_in6*)&ss;
            in6->sin6_family = AF_INET6;
            in6->sin6_addr = in6addr_loopback;
            len = sizeof(*in6);
        } else {
            struct sockaddr_in* in = (struct sockaddr_in*)&ss;
            in->sin_family = AF_INET;
            in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            len = sizeof(*in);
        }
        if (bind(listen_fd, (struct sockaddr*)&ss, len) != 0 || listen(listen_fd, 1) != 0) {
            close(listen_fd);
            listen_fd = -1;
            return;
        }
        len = sizeof(ss);
        getsockname(listen_fd, (struct sockaddr*)&ss, &len);
        port = ntohs(family == AF_INET6 ? ((struct sockaddr_in6*)&ss)->sin6_port :
                ((struct sockaddr_in*)&ss)->sin_port);

        thread = std::thread([this, cert, after_handshake, close_at_once, tls_1_1]() {
            struct pollfd p;
            p.fd = listen_fd;
            p.events = POLLIN;
            p.revents = 0;
            if (poll(&p, 1, 10000) != 1) {
                return;
            }
            int fd = accept(listen_fd, nullptr, nullptr);
            if (fd < 0) {
                return;
            }
            if (close_at_once) {
                close(fd);
                return;
            }
            struct timeval tv;
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
            SSL_CTX* ctx = SSL_CTX_new(tls_1_1 ? TLSv1_1_server_method() : SSLv23_server_method());
#else
            SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
            if (tls_1_1) {
                SSL_CTX_set_min_proto_version(ctx, TLS1_1_VERSION);
                SSL_CTX_set_max_proto_version(ctx, TLS1_1_VERSION);
                // Which OpenSSL 3 takes only at security level 0.
                SSL_CTX_set_security_level(ctx, 0);
            }
#endif
            SSL_CTX_use_certificate(ctx, cert);
            SSL_CTX_use_PrivateKey(ctx, pki().key);
            SSL* ssl = SSL_new(ctx);
            SSL_set_fd(ssl, fd);
            if (SSL_accept(ssl) == 1) {
                const char* name = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
                server_name = name != nullptr ? name : "";
                after_handshake(ssl);
            }
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(fd);
            ERR_clear_error();
        });
    }

    bool listening() const
    {
        return listen_fd >= 0;
    }

    ~Server()
    {
        if (thread.joinable()) {
            thread.join();
        }
        if (listen_fd >= 0) {
            close(listen_fd);
        }
    }
};

// Until the client closes the connection.
void wait_for_close(SSL* ssl)
{
    char buf[256];
    while (SSL_read(ssl, buf, sizeof(buf)) > 0) {
    }
}

void sleep_ms(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

socket_context_t new_context(unsigned int timeout_s)
{
    socket_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.to_recv = timeout_s;
    ctx.to_send = timeout_s;
    return ctx;
}

// How many descriptors the process has open.
int open_fds()
{
    DIR* dir = opendir("/proc/self/fd");
    int count = 0;
    while (readdir(dir) != nullptr) {
        ++count;
    }
    closedir(dir);
    return count;
}

void on_signal(int)
{
}

// Sends SIGUSR1 to this thread every period_ms, with a handler that has
// SA_RESTART, as a timer, or SIGCHLD from children, would: until it goes, or
// for 10 seconds.
struct Signaller {
    struct sigaction previous;
    std::atomic<bool> stop;
    std::thread thread;

    explicit Signaller(int period_ms) : stop(false)
    {
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = on_signal;
        action.sa_flags = SA_RESTART;
        sigaction(SIGUSR1, &action, &previous);
        pthread_t target = pthread_self();
        thread = std::thread([this, target, period_ms]() {
            for (int t = 0; t < 10000 && !stop; t += period_ms) {
                sleep_ms(period_ms);
                if (!stop) {
                    pthread_kill(target, SIGUSR1);
                }
            }
        });
    }

    ~Signaller()
    {
        stop = true;
        thread.join();
        sigaction(SIGUSR1, &previous, nullptr);
    }
};

long elapsed_ms(std::chrono::steady_clock::time_point start)
{
    return (long)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
}

}

TEST_CASE( "The sample's socket connects to a server trusted for the name" ) {
    const Pki& p = pki();
    Server server(AF_INET, p.localhost, [](SSL* ssl) {
        SSL_write(ssl, "hello", 5);
        wait_for_close(ssl);
    });
    REQUIRE( server.listening() );

    socket_context_t ctx = new_context(5);
    REQUIRE( sock_cb_connect(&ctx, "localhost", server.port) == KHC_SOCK_OK );
    char buf[16];
    size_t got = 0;
    CHECK( sock_cb_recv(&ctx, buf, sizeof(buf), &got) == KHC_SOCK_OK );
    CHECK( std::string(buf, got) == "hello" );
    CHECK( sock_cb_close(&ctx) == KHC_SOCK_OK );
}

TEST_CASE( "The sample's socket tells the server the name it connects to" ) {
    const Pki& p = pki();
    Server server(AF_INET, p.localhost, wait_for_close);
    REQUIRE( server.listening() );

    socket_context_t ctx = new_context(5);
    REQUIRE( sock_cb_connect(&ctx, "localhost", server.port) == KHC_SOCK_OK );
    sock_cb_close(&ctx);
    server.thread.join();
    CHECK( server.server_name == "localhost" );
}

TEST_CASE( "The sample's socket refuses a certificate it can't trust" ) {
    const Pki& p = pki();
    SECTION( "issued by no CA it trusts" ) {
        Server server(AF_INET, p.untrusted, wait_for_close);
        REQUIRE( server.listening() );
        socket_context_t ctx = new_context(5);
        if (sock_cb_connect(&ctx, "localhost", server.port) == KHC_SOCK_OK) {
            FAIL_CHECK( "connected" );
            sock_cb_close(&ctx);
        }
    }
    SECTION( "for another name" ) {
        Server server(AF_INET, p.other, wait_for_close);
        REQUIRE( server.listening() );
        socket_context_t ctx = new_context(5);
        if (sock_cb_connect(&ctx, "localhost", server.port) == KHC_SOCK_OK) {
            FAIL_CHECK( "connected" );
            sock_cb_close(&ctx);
        }
    }
    SECTION( "for no address, connecting to one" ) {
        Server server(AF_INET, p.other, wait_for_close);
        REQUIRE( server.listening() );
        socket_context_t ctx = new_context(5);
        if (sock_cb_connect(&ctx, "127.0.0.1", server.port) == KHC_SOCK_OK) {
            FAIL_CHECK( "connected" );
            sock_cb_close(&ctx);
        }
    }
}

// OpenSSL 1.1 goes down to TLS 1.0 by itself. 1.0.2's client method for the
// sample is TLS 1.2's, and OpenSSL 3 takes nothing older at its default
// security level.
TEST_CASE( "The sample's socket refuses TLS older than 1.2" ) {
    const Pki& p = pki();
    Server server(AF_INET, p.localhost, wait_for_close, false, 0, true);
    REQUIRE( server.listening() );
    socket_context_t ctx = new_context(5);
    if (sock_cb_connect(&ctx, "localhost", server.port) == KHC_SOCK_OK) {
        FAIL_CHECK( "connected" );
        sock_cb_close(&ctx);
    }
}

TEST_CASE( "The sample's socket connects to an address the certificate has" ) {
    const Pki& p = pki();
    SECTION( "IPv4" ) {
        Server server(AF_INET, p.localhost, wait_for_close);
        REQUIRE( server.listening() );
        socket_context_t ctx = new_context(5);
        REQUIRE( sock_cb_connect(&ctx, "127.0.0.1", server.port) == KHC_SOCK_OK );
        sock_cb_close(&ctx);
        server.thread.join();
        // An address is no name to tell the server, as RFC 6066 has it.
        CHECK( server.server_name == "" );
    }
    SECTION( "IPv6" ) {
        Server server(AF_INET6, p.localhost, wait_for_close);
        if (!server.listening()) {
            WARN( "no IPv6 loopback here" );
            return;
        }
        socket_context_t ctx = new_context(5);
        REQUIRE( sock_cb_connect(&ctx, "::1", server.port) == KHC_SOCK_OK );
        sock_cb_close(&ctx);
    }
}

TEST_CASE( "The sample's socket is kept from programs the application runs" ) {
    const Pki& p = pki();
    Server server(AF_INET, p.localhost, wait_for_close);
    REQUIRE( server.listening() );

    socket_context_t ctx = new_context(5);
    REQUIRE( sock_cb_connect(&ctx, "localhost", server.port) == KHC_SOCK_OK );
    CHECK( (fcntl(ctx.socket, F_GETFD) & FD_CLOEXEC) != 0 );
    sock_cb_close(&ctx);
}

TEST_CASE( "The sample's socket times out a receive with nothing coming" ) {
    const Pki& p = pki();
    Server server(AF_INET, p.localhost, wait_for_close);
    REQUIRE( server.listening() );

    socket_context_t ctx = new_context(1);
    REQUIRE( sock_cb_connect(&ctx, "localhost", server.port) == KHC_SOCK_OK );
    char buf[16];
    size_t got = 0;
    auto start = std::chrono::steady_clock::now();
    CHECK( sock_cb_recv(&ctx, buf, sizeof(buf), &got) == KHC_SOCK_FAIL );
    long ms = elapsed_ms(start);
    CHECK( ms >= 900 );
    CHECK( ms < 2500 );
    sock_cb_close(&ctx);
}

// A socket with a timeout fails with EINTR on a signal, SA_RESTART or not,
// which OpenSSL takes for a read or write to try again: SSL_ERROR_WANT_READ or
// SSL_ERROR_WANT_WRITE, as for the timeout.
TEST_CASE( "The sample's socket keeps its timeouts through signals" ) {
    const Pki& p = pki();
    SECTION( "receiving what comes in time" ) {
        Server server(AF_INET, p.localhost, [](SSL* ssl) {
            sleep_ms(1000);
            SSL_write(ssl, "hello", 5);
            wait_for_close(ssl);
        });
        REQUIRE( server.listening() );
        socket_context_t ctx = new_context(5);
        REQUIRE( sock_cb_connect(&ctx, "localhost", server.port) == KHC_SOCK_OK );
        char buf[16];
        size_t got = 0;
        khc_sock_code_t result;
        {
            Signaller signaller(100);
            result = sock_cb_recv(&ctx, buf, sizeof(buf), &got);
        }
        CHECK( result == KHC_SOCK_OK );
        CHECK( std::string(buf, got) == "hello" );
        sock_cb_close(&ctx);
    }
    SECTION( "receiving nothing" ) {
        Server server(AF_INET, p.localhost, wait_for_close);
        REQUIRE( server.listening() );
        socket_context_t ctx = new_context(1);
        REQUIRE( sock_cb_connect(&ctx, "localhost", server.port) == KHC_SOCK_OK );
        char buf[16];
        size_t got = 0;
        khc_sock_code_t result;
        long ms;
        {
            Signaller signaller(200);
            auto start = std::chrono::steady_clock::now();
            result = sock_cb_recv(&ctx, buf, sizeof(buf), &got);
            ms = elapsed_ms(start);
        }
        CHECK( result == KHC_SOCK_FAIL );
        CHECK( ms >= 900 );
        CHECK( ms < 2500 );
        sock_cb_close(&ctx);
    }
    SECTION( "sending, with the peer reading nothing" ) {
        std::atomic<bool> done(false);
        // Small buffers on both ends, for the sends to fill them soon.
        Server server(AF_INET, p.localhost, [&done](SSL*) {
            while (!done) {
                sleep_ms(10);
            }
        }, false, 4096);
        REQUIRE( server.listening() );
        socket_context_t ctx = new_context(1);
        REQUIRE( sock_cb_connect(&ctx, "localhost", server.port) == KHC_SOCK_OK );
        int sndbuf = 4096;
        setsockopt(ctx.socket, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
        const std::vector<char> data(64 * 1024, 'x');
        khc_sock_code_t result = KHC_SOCK_OK;
        long ms = 0;
        {
            Signaller signaller(200);
            for (int i = 0; i < 1000 && result == KHC_SOCK_OK; ++i) {
                size_t sent = 0;
                auto start = std::chrono::steady_clock::now();
                result = sock_cb_send(&ctx, data.data(), data.size(), &sent);
                ms = elapsed_ms(start);
            }
        }
        CHECK( result == KHC_SOCK_FAIL );
        CHECK( ms >= 900 );
        CHECK( ms < 2500 );
        done = true;
        sock_cb_close(&ctx);
    }
}

TEST_CASE( "The sample's socket leaves no descriptor open when it fails to connect" ) {
    const Pki& p = pki();
    SECTION( "refused" ) {
        // A port nothing listens on.
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in in;
        memset(&in, 0, sizeof(in));
        in.sin_family = AF_INET;
        in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(fd, (struct sockaddr*)&in, sizeof(in));
        socklen_t len = sizeof(in);
        getsockname(fd, (struct sockaddr*)&in, &len);
        close(fd);
        uint16_t port = ntohs(in.sin_port);

        socket_context_t ctx = new_context(5);
        // Once first, for what the resolver keeps open from its first use.
        sock_cb_connect(&ctx, "localhost", port);
        int before = open_fds();
        CHECK( sock_cb_connect(&ctx, "localhost", port) == KHC_SOCK_FAIL );
        CHECK( open_fds() == before );
    }
    SECTION( "the server closing at once" ) {
        int before = open_fds();
        {
            Server server(AF_INET, p.localhost, wait_for_close, true);
            REQUIRE( server.listening() );
            socket_context_t ctx = new_context(5);
            CHECK( sock_cb_connect(&ctx, "localhost", server.port) == KHC_SOCK_FAIL );
        }
        CHECK( open_fds() == before );
    }
    SECTION( "a certificate refused" ) {
        int before = open_fds();
        {
            Server server(AF_INET, p.untrusted, wait_for_close);
            REQUIRE( server.listening() );
            socket_context_t ctx = new_context(5);
            if (sock_cb_connect(&ctx, "localhost", server.port) == KHC_SOCK_OK) {
                FAIL_CHECK( "connected" );
                sock_cb_close(&ctx);
            }
        }
        CHECK( open_fds() == before );
    }
}
