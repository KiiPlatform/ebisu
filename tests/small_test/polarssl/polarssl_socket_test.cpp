// platform's PolarSSL port, over the POSIX port's socket of plain TCP,
// against TLS servers of the test's own on the loopback, made with OpenSSL,
// and certificates issued here, by a CA of the test's own: the mbedTLS port's
// tests, mbedtls/mbedtls_socket_test.cpp, but for what is TLS 1.3's, which
// PolarSSL hasn't, and with the application's settings in a setup of its own,
// as PolarSSL has no configuration to set them in.
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

// OpenSSL's first: PolarSSL's headers define macros, ASN1_BOOLEAN and the
// like, of the names of OpenSSL's types.
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <ebisu_server_clock.h>
#include <ebisu_polarssl.h>
#include <ebisu_posix.h>

#include <polarssl/ssl.h>
#include <polarssl/x509.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

const time_t DAY = 24 * 60 * 60;

// EC P-256, quick to make, under valgrind too.
EVP_PKEY* make_key()
{
    EVP_PKEY* key = nullptr;
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    REQUIRE( EVP_PKEY_keygen_init(ctx) == 1 );
    REQUIRE( EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) == 1 );
    REQUIRE( EVP_PKEY_keygen(ctx, &key) == 1 );
    EVP_PKEY_CTX_free(ctx);
    return key;
}

void add_extension(X509* cert, X509V3_CTX* ctx, int nid, const std::string& value)
{
    X509_EXTENSION* extension = X509V3_EXT_conf_nid(nullptr, ctx, nid,
            const_cast<char*>(value.c_str()));
    REQUIRE( extension != nullptr );
    REQUIRE( X509_add_ext(cert, extension, -1) == 1 );
    X509_EXTENSION_free(extension);
}

// A certificate for name, valid from not_before to not_after, issued by the
// issuer with its key, or self-signed without one: a CA's, or a server's.
X509* issue(EVP_PKEY* key, const std::string& name, bool ca, time_t not_before,
        time_t not_after, X509* issuer = nullptr, EVP_PKEY* issuer_key = nullptr,
        bool alt_name = true)
{
    static long serial = 1;
    X509* cert = X509_new();
    REQUIRE( X509_set_version(cert, 2) == 1 );
    ASN1_INTEGER_set(X509_get_serialNumber(cert), serial++);
    REQUIRE( ASN1_TIME_set(X509_get_notBefore(cert), not_before) != nullptr );
    REQUIRE( ASN1_TIME_set(X509_get_notAfter(cert), not_after) != nullptr );
    REQUIRE( X509_set_pubkey(cert, key) == 1 );
    // A name of its own, which X509_set_subject_name() copies: OpenSSL 4
    // gives the certificate's as const.
    X509_NAME* subject = X509_NAME_new();
    REQUIRE( X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC,
            (const unsigned char*)name.c_str(), -1, -1, 0) == 1 );
    REQUIRE( X509_set_subject_name(cert, subject) == 1 );
    REQUIRE( X509_set_issuer_name(cert,
                issuer != nullptr ? X509_get_subject_name(issuer) : subject) == 1 );
    X509_NAME_free(subject);
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, issuer != nullptr ? issuer : cert, cert, nullptr, nullptr, 0);
    if (ca) {
        add_extension(cert, &ctx, NID_basic_constraints, "critical,CA:TRUE");
        add_extension(cert, &ctx, NID_key_usage, "critical,keyCertSign,cRLSign");
    } else {
        add_extension(cert, &ctx, NID_basic_constraints, "CA:FALSE");
        if (alt_name) {
            add_extension(cert, &ctx, NID_subject_alt_name, "DNS:" + name);
        }
        add_extension(cert, &ctx, NID_ext_key_usage, "serverAuth");
    }
    REQUIRE( X509_sign(cert, issuer_key != nullptr ? issuer_key : key, EVP_sha256()) > 0 );
    return cert;
}

std::string to_pem(X509* cert)
{
    BIO* bio = BIO_new(BIO_s_mem());
    REQUIRE( PEM_write_bio_X509(bio, cert) == 1 );
    char* data = nullptr;
    long length = BIO_get_mem_data(bio, &data);
    std::string pem(data, (size_t)length);
    BIO_free(bio);
    return pem;
}

// The test's CA, valid from a year ago for ten years, and a key for the
// servers' certificates, which it issues.
struct Pki {
    EVP_PKEY* ca_key;
    X509* ca;
    EVP_PKEY* key;
    time_t now;

    Pki() : now(time(nullptr)) {
        ca_key = make_key();
        ca = issue(ca_key, "ebisu test CA", true, now - 365 * DAY, now + 3650 * DAY);
        key = make_key();
    }

    // A server's certificate, issued by the CA unless self-signed.
    X509* server(const std::string& name, time_t not_before, time_t not_after,
            bool self_signed = false) {
        return self_signed ? issue(key, name, false, not_before, not_after) :
            issue(key, name, false, not_before, not_after, ca, ca_key);
    }

    // Valid from yesterday to tomorrow.
    X509* server() {
        return server("localhost", now - DAY, now + DAY);
    }
};

Pki& pki()
{
    static Pki instance;
    return instance;
}

#if !EBISU_POLARSSL_SETS_RNG
// The application's random generator, which PolarSSL without a CTR_DRBG of
// its own takes: OpenSSL's, which takes calls from more than one thread at
// once.
int application_rng(void*, unsigned char* output, size_t length)
{
    return RAND_bytes(output, (int)length) == 1 ? 0 : -1;
}
#endif

// The application's random generator, where PolarSSL has none of its own, in
// a connection's setup.
void rng_setup(ssl_context* ssl, void*)
{
#if !EBISU_POLARSSL_SETS_RNG
    ssl_set_rng(ssl, application_rng, nullptr);
#else
    (void)ssl;
#endif
}

// Gives the configuration the application's random generator, where it has
// none of its own, in a setup that sets nothing else.
void give_rng(ebisu_polarssl_tls_t* tls)
{
    ebisu_polarssl_tls_set_setup(tls, rng_setup, nullptr);
}

// A configuration with the test's CA, and a random generator, unless told
// not to give it one it hasn't of its own; and the cipher suites set for a
// test, if any, which its setup sets on each connection, as the
// application's.
struct Config {
    ebisu_polarssl_tls_t tls;
    bool rng;
    const int* suites;
    explicit Config(bool give_rng = true) : rng(give_rng), suites(nullptr) {
        std::string pem = to_pem(pki().ca);
        REQUIRE( ebisu_polarssl_tls_init(&tls, (const unsigned char*)pem.c_str(),
                    pem.size() + 1) == 0 );
        ebisu_polarssl_tls_set_setup(&tls, setup, this);
    }
    ~Config() {
        ebisu_polarssl_tls_free(&tls);
    }
    static void setup(ssl_context* ssl, void* userdata) {
        Config* config = static_cast<Config*>(userdata);
        if (config->rng) {
            rng_setup(ssl, nullptr);
        }
        if (config->suites != nullptr) {
            ssl_set_ciphersuites(ssl, config->suites);
        }
    }
};

// A TLS server on 127.0.0.1, at a port of its own, with OpenSSL, that serves
// each connection on a thread of its own with serve, once the handshake is
// done, or without TLS at all if plain. It takes the certificate, and waits
// 10 seconds at the most for each connection.
struct TlsServer {
    typedef std::function<void(SSL* ssl, int fd)> serve_t;
    SSL_CTX* ctx;
    int listener;
    uint16_t port;
    std::thread accepting;
    std::mutex lock;
    std::vector<std::thread> serving;
    std::atomic<int> handshakes;

    TlsServer(X509* cert, serve_t serve, int connections = 1, int max_version = 0,
            bool plain = false) : ctx(nullptr), listener(-1), port(0), handshakes(0) {
        // A write to a client that has gone.
        signal(SIGPIPE, SIG_IGN);
        ctx = SSL_CTX_new(TLS_server_method());
        REQUIRE( SSL_CTX_use_certificate(ctx, cert) == 1 );
        // The context's now.
        X509_free(cert);
        REQUIRE( SSL_CTX_use_PrivateKey(ctx, pki().key) == 1 );
        if (max_version != 0) {
            REQUIRE( SSL_CTX_set_max_proto_version(ctx, max_version) == 1 );
        }
        listener = socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE( listener >= 0 );
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        REQUIRE( bind(listener, (sockaddr*)&addr, sizeof(addr)) == 0 );
        socklen_t length = sizeof(addr);
        REQUIRE( getsockname(listener, (sockaddr*)&addr, &length) == 0 );
        port = ntohs(addr.sin_port);
        REQUIRE( listen(listener, 16) == 0 );
        accepting = std::thread([this, serve, connections, plain] {
            for (int i = 0; i < connections; ++i) {
                pollfd pfd = { listener, POLLIN, 0 };
                if (poll(&pfd, 1, 10000) != 1) {
                    return;
                }
                int fd = accept(listener, nullptr, nullptr);
                if (fd < 0) {
                    return;
                }
                std::lock_guard<std::mutex> guard(lock);
                serving.emplace_back([this, serve, fd, plain] {
                    if (plain) {
                        serve(nullptr, fd);
                    } else {
                        SSL* ssl = SSL_new(ctx);
                        SSL_set_fd(ssl, fd);
                        if (SSL_accept(ssl) == 1) {
                            ++handshakes;
                            serve(ssl, fd);
                        }
                        SSL_free(ssl);
                    }
                    close(fd);
                });
            }
        });
    }

    ~TlsServer() {
        accepting.join();
        for (std::thread& thread : serving) {
            thread.join();
        }
        close(listener);
        SSL_CTX_free(ctx);
    }
};

// What a server can do with a connection.
void echo(SSL* ssl, int)
{
    char buffer[256];
    int n = SSL_read(ssl, buffer, sizeof(buffer));
    if (n > 0) {
        SSL_write(ssl, buffer, n);
    }
    // For the client to close first.
    SSL_read(ssl, buffer, sizeof(buffer));
}

void nothing(SSL*, int)
{
}

// Waits up to 10 seconds for the flag, so that a server whose test failed
// before setting it still ends.
bool wait_for(const std::atomic<bool>& flag)
{
    for (int i = 0; i < 1000 && !flag; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return flag;
}

// A logger that keeps the lines it is given.
struct Captured {
    ebisu_logger_t base;
    std::mutex lock;
    std::vector<std::string> lines;

    static void log(ebisu_logger_t* self, ebisu_log_level_t, const char* fmt, va_list args) {
        Captured* captured = (Captured*)self;
        char line[512];
        vsnprintf(line, sizeof(line), fmt, args);
        std::lock_guard<std::mutex> guard(captured->lock);
        captured->lines.push_back(line);
    }

    Captured() {
        static const ebisu_logger_ops_t ops = { log };
        base.ops = &ops;
    }

    bool has(const std::string& text) {
        std::lock_guard<std::mutex> guard(lock);
        for (const std::string& line : lines) {
            if (line.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

// The PolarSSL port's socket over the POSIX port's, with the given timeouts.
struct Client {
    ebisu_posix_tcp_socket_t tcp;
    ebisu_polarssl_socket_t tls;
    ebisu_socket_t* sock;
    Captured logger;

    explicit Client(ebisu_polarssl_tls_t* config, unsigned int to_recv = 5,
            unsigned int to_send = 5) {
        ebisu_posix_tcp_socket_init(&tcp, to_recv, to_send);
        ebisu_polarssl_socket_init(&tls, &tcp.base, config);
        ebisu_polarssl_socket_set_logger(&tls, &logger.base);
        sock = &tls.base;
    }

    ~Client() {
        sock->ops->close(sock);
    }

    ebisu_sock_code_t connect(uint16_t port, const char* host = "localhost") {
        return sock->ops->connect(sock, host, port);
    }

    ebisu_sock_error_t error(long* detail = nullptr) {
        long ignored = 0;
        return sock->ops->last_error(sock, detail != nullptr ? detail : &ignored);
    }

    void send(const std::string& data) {
        size_t sent = 0;
        REQUIRE( sock->ops->send(sock, data.data(), data.size(), &sent) == EBISU_SOCK_OK );
        REQUIRE( sent == data.size() );
    }

    // Up to length, with what recv returned.
    std::string recv(size_t length, ebisu_sock_code_t* code = nullptr) {
        std::vector<char> buffer(length);
        size_t received = 0;
        ebisu_sock_code_t result = sock->ops->recv(sock, buffer.data(), length, &received);
        if (code != nullptr) {
            *code = result;
        } else {
            REQUIRE( result == EBISU_SOCK_OK );
        }
        return std::string(buffer.data(), received);
    }
};

} // namespace

TEST_CASE( "The PolarSSL socket connects, sends and receives over TLS" ) {
    Config config;
    int max_version = 0;
    SECTION("TLS 1.3, as the server has it, with the session tickets it sends") {
    }
    SECTION("TLS 1.2, a server's highest") {
        max_version = TLS1_2_VERSION;
    }
    TlsServer server(pki().server(), echo, 1, max_version);
    Client client(&config.tls);
    REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
    client.send("hello");
    CHECK( client.recv(64) == "hello" );
    CHECK( client.error() == EBISU_SOCK_ERROR_NONE );
    CHECK( client.logger.lines.empty() );
}


TEST_CASE( "The PolarSSL socket is used again once closed" ) {
    Config config;
    TlsServer server(pki().server(), echo, 2);
    Client client(&config.tls);
    for (int i = 0; i < 2; ++i) {
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        client.send("again");
        CHECK( client.recv(64) == "again" );
        CHECK( client.sock->ops->close(client.sock) == EBISU_SOCK_OK );
    }
    CHECK( server.handshakes == 2 );
}

TEST_CASE( "The PolarSSL socket refuses a certificate its configuration's CA didn't issue" ) {
    Config config;
    TlsServer server(pki().server("localhost", pki().now - DAY, pki().now + DAY, true), nothing);
    Client client(&config.tls);
    REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
    long detail = 0;
    CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
    CHECK( (detail & BADCERT_NOT_TRUSTED) != 0 );
#ifndef EBISU_NO_LOG
    CHECK( client.logger.has("server certificate for localhost refused") );
#endif
}

TEST_CASE( "The PolarSSL socket refuses a certificate for another host" ) {
    Config config;
    TlsServer server(pki().server("other.example", pki().now - DAY, pki().now + DAY), nothing);
    Client client(&config.tls);
    REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
    long detail = 0;
    CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
    CHECK( (detail & BADCERT_CN_MISMATCH) != 0 );
}

namespace {

// A configuration without CA certificates.
struct NoCaConfig {
    ebisu_polarssl_tls_t tls;
    NoCaConfig() {
        REQUIRE( ebisu_polarssl_tls_init(&tls, nullptr, 0) == 0 );
        give_rng(&tls);
    }
    ~NoCaConfig() {
        ebisu_polarssl_tls_free(&tls);
    }
};

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

// Self-signed, for another name, and expired: nothing a CA would pass.
X509* own_certificate()
{
    return pki().server("other.example", pki().now - 60 * DAY, pki().now - 30 * DAY, true);
}

} // namespace

TEST_CASE( "The PolarSSL socket checks a certificate's chain without its name, when asked" ) {
    Config config;
    REQUIRE( ebisu_polarssl_tls_set_verify(&config.tls, EBISU_TLS_VERIFY_CHAIN, nullptr) );

    SECTION("one the CA issued for another name is taken") {
        TlsServer server(pki().server("other.example", pki().now - DAY, pki().now + DAY), nothing);
        Client client(&config.tls);
        CHECK( client.connect(server.port) == EBISU_SOCK_OK );
    }

    SECTION("one self-signed is still refused") {
        TlsServer server(pki().server("localhost", pki().now - DAY, pki().now + DAY, true), nothing);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( (detail & BADCERT_NOT_TRUSTED) != 0 );
    }

    SECTION("one expired is still refused") {
        TlsServer server(pki().server("other.example", pki().now - 60 * DAY, pki().now - 30 * DAY),
                nothing);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( (detail & BADCERT_EXPIRED) != 0 );
    }
}

TEST_CASE( "The PolarSSL socket takes a pinned certificate, whatever else it is" ) {
    X509* cert = own_certificate();
    NoCaConfig config;
    // Two pins, and another; the server holds the certificate.
    TlsServer server(cert, nothing, 3);

    for (const std::vector<unsigned char>& pin : { certificate_pin(cert), public_key_pin(cert) }) {
        REQUIRE( ebisu_polarssl_tls_set_verify(&config.tls, EBISU_TLS_VERIFY_PIN, pin.data()) );
        Client client(&config.tls);
        CHECK( client.connect(server.port) == EBISU_SOCK_OK );
    }

    SECTION("and refuses another") {
        std::vector<unsigned char> pin = public_key_pin(cert);
        pin[EBISU_TLS_PIN_SIZE - 1] ^= 0x80;
        REQUIRE( ebisu_polarssl_tls_set_verify(&config.tls, EBISU_TLS_VERIFY_PIN, pin.data()) );
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( (detail & BADCERT_OTHER) != 0 );
#ifndef EBISU_NO_LOG
        CHECK( client.logger.has(
                    "server certificate for localhost refused: neither it nor its public key has the pin") );
#endif
    }
}

TEST_CASE( "The PolarSSL socket takes any certificate when asked to check none, with a warning" ) {
    NoCaConfig config;
    TlsServer server(own_certificate(), echo);
    REQUIRE( ebisu_polarssl_tls_set_verify(&config.tls, EBISU_TLS_VERIFY_NONE, nullptr) );
    Client client(&config.tls);
    REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
    client.send("hello");
    CHECK( client.recv(5) == "hello" );
#ifndef EBISU_NO_LOG
    CHECK( client.logger.has("connected to localhost without verifying its certificate.") );
#endif
}

TEST_CASE( "A TLS configuration without CA certificates takes no server's in full" ) {
    NoCaConfig config;
    TlsServer server(pki().server(), nothing);
    Client client(&config.tls);
    REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
    CHECK( client.error() == EBISU_SOCK_ERROR_VERIFY );
}

TEST_CASE( "A TLS configuration refuses a mode it can't take, as it was" ) {
    Config config;
    CHECK_FALSE( ebisu_polarssl_tls_set_verify(&config.tls, EBISU_TLS_VERIFY_PIN, nullptr) );
    CHECK_FALSE( ebisu_polarssl_tls_set_verify(&config.tls, (ebisu_tls_verify_t)42, nullptr) );
    TlsServer server(pki().server("other.example", pki().now - DAY, pki().now + DAY), nothing);
    Client client(&config.tls);
    REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
    long detail = 0;
    CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
    CHECK( (detail & BADCERT_CN_MISMATCH) != 0 );
}

TEST_CASE( "The PolarSSL socket checks the certificates' dates at the clock's time" ) {
    Config config;
    const time_t now = pki().now;
    ebisu_server_clock_t clock;
    REQUIRE( ebisu_server_clock_init(&clock, ebisu_posix_system()) );
    auto expired = [now] { return pki().server("localhost", now - 60 * DAY, now - 30 * DAY); };
    auto not_yet = [now] { return pki().server("localhost", now + 30 * DAY, now + 60 * DAY); };

    SECTION("an expired one, at a time the clock has, is refused") {
        REQUIRE( ebisu_server_clock_set_time(&clock, now) );
        TlsServer server(expired(), nothing);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_clock(&client.tls, &clock.base);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( (detail & BADCERT_EXPIRED) != 0 );
    }
    SECTION("an expired one, before the clock's floor, is refused") {
        ebisu_server_clock_set_floor(&clock, now);
        TlsServer server(expired(), nothing);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_clock(&client.tls, &clock.base);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( (detail & BADCERT_EXPIRED) != 0 );
    }
    SECTION("one not valid yet, at a time the clock has, is refused, and the time forgotten") {
        REQUIRE( ebisu_server_clock_set_time(&clock, now) );
        TlsServer server(not_yet(), nothing);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_clock(&client.tls, &clock.base);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( (detail & BADCERT_FUTURE) != 0 );
        int64_t time_now = 0;
        CHECK_FALSE( ebisu_clock_get_time(&clock.base, &time_now) );
    }
    SECTION("one not valid yet, with only the clock's floor, which it may have been issued since, is taken") {
        ebisu_server_clock_set_floor(&clock, now);
        TlsServer server(not_yet(), echo);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_clock(&client.tls, &clock.base);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        client.send("floor");
        CHECK( client.recv(64) == "floor" );
    }
    SECTION("one not valid yet at the system's time, valid at the clock's, is taken") {
        REQUIRE( ebisu_server_clock_set_time(&clock, now + 45 * DAY) );
        TlsServer server(not_yet(), echo);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_clock(&client.tls, &clock.base);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        client.send("clock");
        CHECK( client.recv(64) == "clock" );
    }
    SECTION("one not valid yet, at the system's clock's time, is refused, and the clock keeps it") {
        ebisu_clock_t* system_clock = ebisu_system_clock(ebisu_posix_system());
        TlsServer server(not_yet(), nothing);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_clock(&client.tls, system_clock);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( (detail & BADCERT_FUTURE) != 0 );
        int64_t time_now = 0;
        CHECK( ebisu_clock_get_time(system_clock, &time_now) );
    }
}

#if defined(POLARSSL_HAVE_TIME)
TEST_CASE( "The PolarSSL socket without a clock checks the dates at the system's time" ) {
    Config config;
    const time_t now = pki().now;
    SECTION("an expired one is refused") {
        TlsServer server(pki().server("localhost", now - 60 * DAY, now - 30 * DAY), nothing);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( (detail & BADCERT_EXPIRED) != 0 );
    }
    SECTION("one not valid yet is refused") {
        TlsServer server(pki().server("localhost", now + 30 * DAY, now + 60 * DAY), nothing);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( (detail & BADCERT_FUTURE) != 0 );
    }
}
#else
TEST_CASE( "The PolarSSL socket without a clock or the system's time doesn't connect" ) {
    Config config;
    TlsServer server(pki().server(), nothing);
    Client client(&config.tls);
    REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
    CHECK( client.error() == EBISU_SOCK_ERROR_TLS );
    CHECK( server.handshakes == 0 );
}
#endif

#if !EBISU_POLARSSL_SETS_RNG
TEST_CASE( "The PolarSSL socket without a random generator doesn't connect, and says why" ) {
    Config config(false);
    TlsServer server(pki().server(), nothing);
    Client client(&config.tls);
    REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
    long detail = 0;
    CHECK( client.error(&detail) == EBISU_SOCK_ERROR_TLS );
    CHECK( detail == POLARSSL_ERR_SSL_NO_RNG );
    CHECK( client.logger.has("no random generator for TLS with localhost") );
    CHECK( server.handshakes == 0 );
}
#endif

TEST_CASE( "The PolarSSL socket's receive times out, and the connection goes on" ) {
    Config config;
    std::atomic<bool> timed_out(false);
    TlsServer server(pki().server(), [&timed_out](SSL* ssl, int) {
        wait_for(timed_out);
        SSL_write(ssl, "late", 4);
        char buffer[16];
        SSL_read(ssl, buffer, sizeof(buffer));
    });
    Client client(&config.tls, 1);
    REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
    ebisu_sock_code_t code;
    client.recv(64, &code);
    REQUIRE( code == EBISU_SOCK_FAIL );
    CHECK( client.sock->ops->recv_timed_out(client.sock) );
    CHECK( client.error() == EBISU_SOCK_ERROR_TIMEOUT );
    // Not a failure to log.
    CHECK( client.logger.lines.empty() );
    timed_out = true;
    CHECK( client.recv(64) == "late" );
    CHECK_FALSE( client.sock->ops->recv_timed_out(client.sock) );
}

TEST_CASE( "The PolarSSL socket takes the end of the stream" ) {
    Config config;
    SECTION("a close_notify") {
        TlsServer server(pki().server(), [](SSL* ssl, int) {
            SSL_write(ssl, "bye", 3);
            SSL_shutdown(ssl);
        });
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        CHECK( client.recv(64) == "bye" );
        ebisu_sock_code_t code;
        CHECK( client.recv(64, &code).empty() );
        CHECK( code == EBISU_SOCK_OK );
    }
    SECTION("the end of the TCP stream, without a close_notify") {
        TlsServer server(pki().server(), [](SSL* ssl, int fd) {
            SSL_write(ssl, "bye", 3);
            shutdown(fd, SHUT_RDWR);
        });
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        CHECK( client.recv(64) == "bye" );
        ebisu_sock_code_t code;
        CHECK( client.recv(64, &code).empty() );
        CHECK( code == EBISU_SOCK_OK );
    }
}

TEST_CASE( "The PolarSSL socket's recv_wait waits for something for recv to return" ) {
    Config config;
    std::atomic<bool> go(false);
    TlsServer server(pki().server(), [&go](SSL* ssl, int) {
        wait_for(go);
        // One record.
        SSL_write(ssl, "abcdef", 6);
        char buffer[16];
        SSL_read(ssl, buffer, sizeof(buffer));
    });
    // A receive that times out soon, for the tickets below.
    Client client(&config.tls, 1);
    REQUIRE( client.sock->ops->recv_wait != nullptr );
    REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
    // Whatever of the handshake is still to come, as TLS 1.3's tickets.
    while (client.sock->ops->recv_wait(client.sock, 200)) {
        ebisu_sock_code_t code;
        REQUIRE( client.recv(64, &code).empty() );
        REQUIRE( code == EBISU_SOCK_FAIL );
    }
    // Nothing for its timeout.
    auto start = std::chrono::steady_clock::now();
    CHECK_FALSE( client.sock->ops->recv_wait(client.sock, 300) );
    CHECK( std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(250) );
    go = true;
    CHECK( client.sock->ops->recv_wait(client.sock, 5000) );
    CHECK( client.recv(2) == "ab" );
    // The rest of the record, decrypted, which the socket under it no longer
    // has: no wait.
    start = std::chrono::steady_clock::now();
    CHECK( client.sock->ops->recv_wait(client.sock, 5000) );
    CHECK( std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1000) );
    CHECK( client.recv(64) == "cdef" );
}

TEST_CASE( "The PolarSSL socket over a socket without recv_wait has none" ) {
    Config config;
    ebisu_posix_tcp_socket_t tcp;
    ebisu_posix_tcp_socket_init(&tcp, 5, 5);
    ebisu_socket_ops_t without = *tcp.base.ops;
    without.recv_wait = nullptr;
    tcp.base.ops = &without;
    ebisu_polarssl_socket_t tls;
    ebisu_polarssl_socket_init(&tls, &tcp.base, &config.tls);
    CHECK( tls.base.ops->recv_wait == nullptr );
}

TEST_CASE( "The PolarSSL socket's send to a peer that has gone fails" ) {
    Config config;
    std::atomic<bool> gone(false);
    TlsServer server(pki().server(), [&gone](SSL*, int fd) {
        // Reset as the server closes it, so that sends fail at once.
        linger reset = { 1, 0 };
        setsockopt(fd, SOL_SOCKET, SO_LINGER, &reset, sizeof(reset));
        gone = true;
    });
    Client client(&config.tls);
    REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
    REQUIRE( wait_for(gone) );
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ebisu_sock_code_t code = EBISU_SOCK_OK;
    for (int i = 0; i < 100 && code == EBISU_SOCK_OK; ++i) {
        size_t sent = 0;
        code = client.sock->ops->send(client.sock, "data", 4, &sent);
    }
    REQUIRE( code == EBISU_SOCK_FAIL );
    CHECK( client.error() == EBISU_SOCK_ERROR_CLOSED );
}

TEST_CASE( "The PolarSSL socket says why connecting failed, as the socket under it says" ) {
    Config config;
    Client client(&config.tls);
    SECTION("an address that refuses") {
        // A port no one listens on: one a server had.
        uint16_t port;
        {
            TlsServer server(pki().server(), nothing, 0);
            port = server.port;
        }
        REQUIRE( client.connect(port, "127.0.0.1") == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_CONNECT );
        CHECK( detail == ECONNREFUSED );
    }
    SECTION("a host that doesn't resolve") {
        REQUIRE( client.connect(443, "no-such-host.invalid") == EBISU_SOCK_FAIL );
        CHECK( client.error() == EBISU_SOCK_ERROR_DNS );
    }
}

TEST_CASE( "The PolarSSL socket fails a handshake with a server that doesn't speak TLS" ) {
    Config config;
    TlsServer server(pki().server(), [](SSL*, int fd) {
        const char reply[] = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        char buffer[512];
        (void)!read(fd, buffer, sizeof(buffer));
        (void)!write(fd, reply, sizeof(reply) - 1);
    }, 1, 0, true);
    Client client(&config.tls);
    REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
    long detail = 0;
    ebisu_sock_error_t error = client.error(&detail);
    CHECK( error == EBISU_SOCK_ERROR_TLS );
    CHECK( detail < 0 );
#ifndef EBISU_NO_LOG
    CHECK( client.logger.has("TLS handshake failed with localhost") );
#endif
}

TEST_CASE( "The PolarSSL sockets of a configuration connect on several threads at once" ) {
    Config config;
    const int threads = 4;
    const int each = 3;
    TlsServer server(pki().server(), echo, threads * each);
    std::atomic<int> echoed(0);
    std::vector<std::thread> clients;
    for (int t = 0; t < threads; ++t) {
        clients.emplace_back([&config, &server, &echoed, t] {
            for (int i = 0; i < each; ++i) {
                ebisu_posix_tcp_socket_t tcp;
                ebisu_polarssl_socket_t tls;
                ebisu_posix_tcp_socket_init(&tcp, 10, 10);
                ebisu_polarssl_socket_init(&tls, &tcp.base, &config.tls);
                if (tls.base.ops->connect(&tls.base, "localhost", server.port) != EBISU_SOCK_OK) {
                    continue;
                }
                std::string message = "thread " + std::to_string(t) + " " + std::to_string(i);
                size_t sent = 0;
                char buffer[64];
                size_t received = 0;
                if (tls.base.ops->send(&tls.base, message.data(), message.size(), &sent) ==
                        EBISU_SOCK_OK &&
                        tls.base.ops->recv(&tls.base, buffer, sizeof(buffer), &received) ==
                        EBISU_SOCK_OK &&
                        std::string(buffer, received) == message) {
                    ++echoed;
                }
                tls.base.ops->close(&tls.base);
            }
        });
    }
    for (std::thread& client : clients) {
        client.join();
    }
    CHECK( echoed == threads * each );
}

TEST_CASE( "A TLS configuration whose CA certificates can't be parsed fails" ) {
    const unsigned char garbage[] = "-----BEGIN CERTIFICATE-----\nnot one\n-----END CERTIFICATE-----\n";
    ebisu_polarssl_tls_t tls;
    CHECK( ebisu_polarssl_tls_init(&tls, garbage, sizeof(garbage)) != 0 );
}

namespace {

// Writes an HTTP response to what it reads: a server that isn't TLS.
void http(SSL*, int fd)
{
    char buffer[512];
    (void)recv(fd, buffer, sizeof(buffer), 0);
    const char response[] = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
    (void)send(fd, response, sizeof(response) - 1, 0);
}

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

// Answers a ClientHello with TLS10_SERVER_HELLO.
void tls10_hello(SSL*, int fd)
{
    char buffer[2048];
    (void)recv(fd, buffer, sizeof(buffer), 0);
    (void)send(fd, TLS10_SERVER_HELLO, sizeof(TLS10_SERVER_HELLO), 0);
    // For the client to close first.
    (void)recv(fd, buffer, sizeof(buffer), 0);
}

// Answers a ClientHello with a fatal alert of TLS 1.2's, of the description
// alert_description says.
unsigned char alert_description = 0;
void alert(SSL*, int fd)
{
    char buffer[2048];
    (void)recv(fd, buffer, sizeof(buffer), 0);
    const unsigned char record[] = { 0x15, 0x03, 0x03, 0x00, 0x02, 0x02, alert_description };
    (void)send(fd, record, sizeof(record), 0);
    (void)recv(fd, buffer, sizeof(buffer), 0);
}

const char MISMATCH_HINT[] =
    "localhost may take only TLS older than 1.2, or none of the cipher suites the "
    "configuration offers.";

}

// A server older than the configuration, or with none of its suites, answered
// with PolarSSL's fatal alert alone.
TEST_CASE( "The PolarSSL socket says when the server has no TLS version or suite in common with it" ) {
    Config config;
    SECTION("a server of TLS 1.0 or 1.1 alone") {
        for (int version : { TLS1_VERSION, TLS1_1_VERSION }) {
            CAPTURE( version );
            TlsServer server(pki().server(), nothing);
            REQUIRE( SSL_CTX_set_min_proto_version(server.ctx, version) == 1 );
            REQUIRE( SSL_CTX_set_max_proto_version(server.ctx, version) == 1 );
            // Security level 0, where TLS 1.0 and 1.1 still are.
            REQUIRE( SSL_CTX_set_cipher_list(server.ctx, "AES128-SHA:@SECLEVEL=0") == 1 );
            Client client(&config.tls);
            REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
            long detail = 0;
            CHECK( client.error(&detail) == EBISU_SOCK_ERROR_TLS_MISMATCH );
            CHECK( detail < 0 );
#ifdef EBISU_NO_LOG
            CHECK( client.logger.lines.empty() );
#else
            CHECK( client.logger.has("TLS handshake failed with localhost") );
            CHECK( client.logger.has(MISMATCH_HINT) );
#endif
        }
    }
    SECTION("each alert that says so, and no other") {
        // handshake_failure, protocol_version, insufficient_security; and
        // internal_error, which doesn't.
        for (unsigned char description : { 40, 70, 71, 80 }) {
            CAPTURE( (int)description );
            alert_description = description;
            TlsServer server(pki().server(), alert, 1, 0, true);
            Client client(&config.tls);
            REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
            long detail = 0;
            CHECK( client.error(&detail) ==
                    (description == 80 ? EBISU_SOCK_ERROR_TLS : EBISU_SOCK_ERROR_TLS_MISMATCH) );
            CHECK( detail == POLARSSL_ERR_SSL_FATAL_ALERT_MESSAGE );
        }
    }
    SECTION("a server that answers in TLS 1.0, which PolarSSL refuses itself") {
        TlsServer server(pki().server(), tls10_hello, 1, 0, true);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_TLS_MISMATCH );
        CHECK( detail == POLARSSL_ERR_SSL_BAD_HS_PROTOCOL_VERSION );
#ifndef EBISU_NO_LOG
        CHECK( client.logger.has(MISMATCH_HINT) );
#endif
    }
    SECTION("a server of TLS 1.2 without the configuration's suites") {
        static const int suites[] = { TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0 };
        config.suites = suites;
        TlsServer server(pki().server(), nothing, 1, TLS1_2_VERSION);
        REQUIRE( SSL_CTX_set_cipher_list(server.ctx, "ECDHE-RSA-AES256-GCM-SHA384") == 1 );
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        CHECK( client.error() == EBISU_SOCK_ERROR_TLS_MISMATCH );
#ifndef EBISU_NO_LOG
        CHECK( client.logger.has(MISMATCH_HINT) );
#endif
    }
    SECTION("but not a server that isn't TLS") {
        TlsServer server(pki().server(), http, 1, 0, true);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        CHECK( client.error() == EBISU_SOCK_ERROR_TLS );
        CHECK_FALSE( client.logger.has("may take only") );
    }
    SECTION("nor a certificate refused") {
        TlsServer server(pki().server("other.example", pki().now - DAY, pki().now + DAY), nothing);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        CHECK( client.error() == EBISU_SOCK_ERROR_VERIFY );
        CHECK_FALSE( client.logger.has("may take only") );
    }
}

// PolarSSL matches the host against a certificate's subjectAltNames, and its
// commonName when it has none, as OpenSSL did before 4.1.
TEST_CASE( "The PolarSSL socket takes a host's commonName for its name without a subjectAltName" ) {
    Config config;
    X509* cn_only = issue(pki().key, "localhost", false, pki().now - DAY, pki().now + DAY,
            pki().ca, pki().ca_key, false);
    TlsServer server(cn_only, echo);
    Client client(&config.tls);
    REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
    client.send("hello");
    CHECK( client.recv(64) == "hello" );

    // And still refuses another name.
    TlsServer other(issue(pki().key, "other.example", false, pki().now - DAY,
                pki().now + DAY, pki().ca, pki().ca_key, false), nothing);
    Client refused(&config.tls);
    CHECK( refused.connect(other.port) == EBISU_SOCK_FAIL );
    CHECK( refused.error() == EBISU_SOCK_ERROR_VERIFY );
}

TEST_CASE( "The PolarSSL socket gives the TLS version and cipher suite its connection negotiated" ) {
    Config config;
    {
        Client client(&config.tls);
        CHECK( ebisu_polarssl_socket_get_protocol(&client.tls) == nullptr );
        CHECK( ebisu_polarssl_socket_get_cipher(&client.tls) == nullptr );
    }
    SECTION("TLS 1.2") {
        TlsServer server(pki().server(), echo, 1, TLS1_2_VERSION);
        // The tests' keys are of P-256.
        REQUIRE( SSL_CTX_set_cipher_list(server.ctx, "ECDHE-ECDSA-AES128-GCM-SHA256") == 1 );
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        REQUIRE( ebisu_polarssl_socket_get_protocol(&client.tls) != nullptr );
        CHECK( std::string(ebisu_polarssl_socket_get_protocol(&client.tls)) == "TLSv1.2" );
        REQUIRE( ebisu_polarssl_socket_get_cipher(&client.tls) != nullptr );
        CHECK( std::string(ebisu_polarssl_socket_get_cipher(&client.tls)) ==
                "TLS-ECDHE-ECDSA-WITH-AES-128-GCM-SHA256" );
        client.send("hello");
        CHECK( client.recv(64) == "hello" );
        REQUIRE( client.sock->ops->close(client.sock) == EBISU_SOCK_OK );
        CHECK( ebisu_polarssl_socket_get_protocol(&client.tls) == nullptr );
        CHECK( ebisu_polarssl_socket_get_cipher(&client.tls) == nullptr );
    }
    SECTION("nothing after a connect that failed") {
        TlsServer server(pki().server("other.example", pki().now - DAY, pki().now + DAY), nothing);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        CHECK( ebisu_polarssl_socket_get_protocol(&client.tls) == nullptr );
        CHECK( ebisu_polarssl_socket_get_cipher(&client.tls) == nullptr );
    }
}
