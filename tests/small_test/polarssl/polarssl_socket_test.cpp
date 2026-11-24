// platform's PolarSSL port, over the POSIX port's socket of plain TCP,
// against TLS servers of the test's own on the loopback, made with OpenSSL,
// and certificates issued here, by a CA of the test's own: the mbedTLS port's
// tests, mbedtls/mbedtls_socket_test.cpp, but for what is TLS 1.3's, which
// PolarSSL hasn't, and with the application's settings in a setup of its own,
// as PolarSSL has no configuration to set them in.
#include "catch_amalgamated.hpp"
#include <polarssl/version.h>

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

// For issue()'s alt_names: a subjectAltName with one name, a dNSName, of
// the 4 bytes 127.0.0.1 is as an address.
const char LOOPBACK_AS_NAME[] = "a dNSName of 127.0.0.1's bytes";

// A certificate for name, valid from not_before to not_after, issued by the
// issuer with its key, or self-signed without one: a CA's, or a server's.
X509* issue(EVP_PKEY* key, const std::string& name, bool ca, time_t not_before,
        time_t not_after, X509* issuer = nullptr, EVP_PKEY* issuer_key = nullptr,
        bool alt_name = true, const std::string& alt_names = "")
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
        if (alt_name && alt_names == LOOPBACK_AS_NAME) {
            // What no configuration string says: a dNSName of an address's
            // bytes.
            static const unsigned char loopback[] = { 127, 0, 0, 1 };
            GENERAL_NAMES* names = sk_GENERAL_NAME_new_null();
            GENERAL_NAME* entry = GENERAL_NAME_new();
            ASN1_IA5STRING* value = ASN1_IA5STRING_new();
            REQUIRE( ASN1_STRING_set(value, loopback, sizeof(loopback)) == 1 );
            GENERAL_NAME_set0_value(entry, GEN_DNS, value);
            sk_GENERAL_NAME_push(names, entry);
            REQUIRE( X509_add1_ext_i2d(cert, NID_subject_alt_name, names, 0, 0) == 1 );
            sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);
        } else if (alt_name) {
            add_extension(cert, &ctx, NID_subject_alt_name,
                    alt_names.empty() ? "DNS:" + name : alt_names);
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
                    pem.size() + 1, nullptr) );
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
// Whether PolarSSL is built with a suite that has forward secrecy and
// authenticated encryption, by the suites' names.
bool has_strict_suite()
{
    for (const int* id = ssl_list_ciphersuites(); *id != 0; ++id) {
        std::string name(ssl_get_ciphersuite_name(*id));
        bool forward = name.find("TLS1-3-") == 0 || name.find("TLS-ECDHE-") == 0 ||
            name.find("TLS-DHE-") == 0;
        bool aead = name.find("-GCM-") != std::string::npos ||
            name.find("-CHACHA20-") != std::string::npos ||
            (name.find("-CCM") != std::string::npos && name.find("-CCM-8") == std::string::npos);
        if (forward && aead && name.find("PSK") == std::string::npos) {
            return true;
        }
    }
    return false;
}

const char CBC_SUITE[] = "TLS-ECDHE-ECDSA-WITH-AES-128-CBC-SHA256";
const char SUITE_WARNING[] = "a cipher suite without forward secrecy or without";
const char NO_STRICT_SUITE[] = "is built with none that has both";

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

    // Whether nothing was logged, but, by a library without a suite of
    // forward secrecy and authenticated encryption, the warning of the suite
    // a connection has.
    bool quiet() {
        std::lock_guard<std::mutex> guard(lock);
        if (has_strict_suite()) {
            return lines.empty();
        }
        for (const std::string& line : lines) {
            if (line.find(SUITE_WARNING) == std::string::npos) {
                return false;
            }
        }
        return true;
    }

    int count(const std::string& text) {
        std::lock_guard<std::mutex> guard(lock);
        int found = 0;
        for (const std::string& line : lines) {
            if (line.find(text) != std::string::npos) {
                ++found;
            }
        }
        return found;
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
#if defined(EBISU_TLS_RESUME)
        // What it kept, which closing keeps for the next connection.
        ebisu_polarssl_socket_set_session_resume(&tls, nullptr);
#endif
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
    CHECK( client.logger.quiet() );
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
        REQUIRE( ebisu_polarssl_tls_init(&tls, nullptr, 0, nullptr) );
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
    SECTION("one not valid yet that the CA doesn't lead to is refused, and the clock keeps its time") {
        REQUIRE( ebisu_server_clock_set_time(&clock, now) );
        TlsServer server(pki().server("localhost", now + 30 * DAY, now + 60 * DAY, true), nothing);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_clock(&client.tls, &clock.base);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( (detail & BADCERT_FUTURE) != 0 );
        CHECK( (detail & BADCERT_NOT_TRUSTED) != 0 );
        int64_t time_now = 0;
        CHECK( ebisu_clock_get_time(&clock.base, &time_now) );
    }
    SECTION("one not valid yet for another host is refused, and the clock keeps its time") {
        REQUIRE( ebisu_server_clock_set_time(&clock, now) );
        TlsServer server(pki().server("other.example", now + 30 * DAY, now + 60 * DAY), nothing);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_clock(&client.tls, &clock.base);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
        CHECK( (detail & BADCERT_FUTURE) != 0 );
        CHECK( (detail & BADCERT_CN_MISMATCH) != 0 );
        int64_t time_now = 0;
        CHECK( ebisu_clock_get_time(&clock.base, &time_now) );
    }
    SECTION("one not valid yet for another host, with the host not checked, is refused, and the time forgotten") {
        REQUIRE( ebisu_polarssl_tls_set_verify(&config.tls, EBISU_TLS_VERIFY_CHAIN, nullptr) );
        REQUIRE( ebisu_server_clock_set_time(&clock, now) );
        TlsServer server(pki().server("other.example", now + 30 * DAY, now + 60 * DAY), nothing);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_clock(&client.tls, &clock.base);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_VERIFY );
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
    CHECK( client.logger.quiet() );
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

// A record that carries nothing for recv to return, in TLS 1.2, which is all
// PolarSSL speaks: a server's HelloRequest, which the socket declines with a
// warning alert. recv_wait asked the socket under it alone, so it took the
// record for something to return, and recv then waited for its whole timeout.
// While a request's body is being sent, khc asks it whether the server has
// answered early. PolarSSL before 1.3.9 gave the HelloRequest it declined
// from ssl_read() as data, the message's four bytes, as its ChangeLog says:
// recv_wait took them for data too.
TEST_CASE( "The PolarSSL socket's recv_wait doesn't take a record without data for data" ) {
    Config config;
    std::atomic<bool> asked(false);
    std::atomic<bool> done(false);
    TlsServer server(pki().server(), [&asked, &done](SSL* ssl, int) {
        // The HelloRequest, which SSL_do_handshake() sends and returns.
        asked = SSL_renegotiate(ssl) == 1 && SSL_do_handshake(ssl) == 1;
        wait_for(done);
        SSL_write(ssl, "ok", 2);
        char ignored[16];
        SSL_read(ssl, ignored, sizeof(ignored));
    }, 1, TLS1_2_VERSION);
    Client client(&config.tls, 1);
    REQUIRE( client.sock->ops->recv_wait != nullptr );
    REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
    REQUIRE( wait_for(asked) );
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto start = std::chrono::steady_clock::now();
    CHECK_FALSE( client.sock->ops->recv_wait(client.sock, 300) );
    const auto waited = std::chrono::steady_clock::now() - start;
    CHECK( waited >= std::chrono::milliseconds(250) );
    CHECK( waited < std::chrono::milliseconds(2000) );
    CHECK_FALSE( client.sock->ops->recv_wait(client.sock, 0) );
    done = true;
    // The data after it.
    CHECK( client.sock->ops->recv_wait(client.sock, 5000) );
    CHECK( client.recv(64) == "ok" );
}

// PolarSSL before 1.3.9 gave the HelloRequest it declined from ssl_read() as
// data, and recv returned its four bytes, all zeros, for the server's.
TEST_CASE( "The PolarSSL socket's recv doesn't return a HelloRequest as data" ) {
    Config config;
    std::atomic<bool> asked(false);
    TlsServer server(pki().server(), [&asked](SSL* ssl, int) {
        asked = SSL_renegotiate(ssl) == 1 && SSL_do_handshake(ssl) == 1;
        SSL_write(ssl, "ok", 2);
        char ignored[16];
        SSL_read(ssl, ignored, sizeof(ignored));
    }, 1, TLS1_2_VERSION);
    Client client(&config.tls);
    REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
    REQUIRE( wait_for(asked) );
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SECTION("read whole") {
        CHECK( client.recv(64) == "ok" );
    }
    SECTION("read a byte at a time") {
        CHECK( client.recv(1) == "o" );
        CHECK( client.recv(1) == "k" );
    }
}

// recv_wait reads ahead of recv: the first byte of data, or the end of the
// stream or a failure first. recv returns what reading it would have.
TEST_CASE( "The PolarSSL socket's recv returns what its recv_wait came to first" ) {
    Config config;
    std::atomic<bool> go(false);
    char buffer[64];

    SECTION("data, whole, though recv_wait read its first byte") {
        TlsServer server(pki().server(), [&go](SSL* ssl, int) {
            wait_for(go);
            SSL_write(ssl, "hello", 5);
            char ignored[16];
            SSL_read(ssl, ignored, sizeof(ignored));
        });
        Client client(&config.tls, 1);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        go = true;
        CHECK( client.sock->ops->recv_wait(client.sock, 5000) );
        CHECK( client.sock->ops->recv_wait(client.sock, 0) );
        CHECK( client.recv(1) == "h" );
        CHECK( client.recv(64) == "ello" );
    }

    SECTION("a record that doesn't decrypt") {
        TlsServer server(pki().server(), [&go](SSL* ssl, int fd) {
            wait_for(go);
            const unsigned char record[] = { 0x17, 0x03, 0x03, 0x00, 0x20,
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
            (void)!write(fd, record, sizeof(record));
            char ignored[16];
            SSL_read(ssl, ignored, sizeof(ignored));
        });
        Client client(&config.tls, 1);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        go = true;
        CHECK( client.sock->ops->recv_wait(client.sock, 5000) );
        size_t received = 1;
        CHECK( client.sock->ops->recv(client.sock, buffer, sizeof(buffer), &received) == EBISU_SOCK_FAIL );
        CHECK( received == 0 );
        CHECK( client.error() == EBISU_SOCK_ERROR_TLS );
        CHECK_FALSE( client.sock->ops->recv_timed_out(client.sock) );
    }

    SECTION("a reset") {
        // False where SO_LINGER can't be set, as under qemu-arm's emulation
        // of Linux, which doesn't have it.
        std::atomic<bool> reset(true);
        std::atomic<bool> tried(false);
        TlsServer server(pki().server(), [&go, &reset, &tried](SSL*, int fd) {
            wait_for(go);
            struct linger l = { 1, 0 };
            reset = setsockopt(fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) == 0;
            tried = true;
        });
        Client client(&config.tls, 1);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        go = true;
        wait_for(tried);
        if (!reset) {
            SKIP( "SO_LINGER can't be set here: no reset to check" );
        }
        CHECK( client.sock->ops->recv_wait(client.sock, 5000) );
        size_t received = 1;
        CHECK( client.sock->ops->recv(client.sock, buffer, sizeof(buffer), &received) == EBISU_SOCK_FAIL );
        CHECK( received == 0 );
        CHECK( client.error() != EBISU_SOCK_ERROR_NONE );
        CHECK_FALSE( client.sock->ops->recv_timed_out(client.sock) );
    }
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
    int error = 0;
    CHECK_FALSE( ebisu_polarssl_tls_init(&tls, garbage, sizeof(garbage), &error) );
    CHECK( error < 0 );
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
            CHECK( client.logger.quiet() );
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

TEST_CASE( "The PolarSSL socket offers the suites with forward secrecy and authenticated encryption, if PolarSSL has any" ) {
    Config config;
    bool strict = has_strict_suite();
    CAPTURE( strict );
    SECTION("a server of AES in CBC mode alone is refused, or, by a PolarSSL without the others, taken with a warning, once") {
        TlsServer server(pki().server(), echo, 2, TLS1_2_VERSION);
        REQUIRE( SSL_CTX_set_cipher_list(server.ctx, "ECDHE-ECDSA-AES128-SHA256:@SECLEVEL=0") == 1 );
        Client client(&config.tls);
        if (strict) {
            REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
            CHECK( client.error() == EBISU_SOCK_ERROR_TLS_MISMATCH );
            CHECK( client.logger.count(SUITE_WARNING) == 0 );
        } else {
            REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
            CHECK( std::string(ebisu_polarssl_socket_get_cipher(&client.tls)) == CBC_SUITE );
            REQUIRE( client.sock->ops->close(client.sock) == EBISU_SOCK_OK );
            REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
#ifndef EBISU_NO_LOG
            CHECK( client.logger.count(SUITE_WARNING) == 1 );
            CHECK( client.logger.count(std::string(CBC_SUITE) + ", " + SUITE_WARNING) == 1 );
            CHECK( client.logger.count(NO_STRICT_SUITE) == 1 );
#endif
        }
    }
    SECTION("a server of a cipher other than AES alone is refused, whichever PolarSSL it is") {
        // Camellia in CBC mode, which PolarSSL has by default.
        TlsServer server(pki().server(), nothing, 1, TLS1_2_VERSION);
        REQUIRE( SSL_CTX_set_cipher_list(server.ctx,
                    "ECDHE-ECDSA-CAMELLIA128-SHA256:@SECLEVEL=0") == 1 );
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        CHECK( client.error() == EBISU_SOCK_ERROR_TLS_MISMATCH );
    }
    // Only where PolarSSL has one of them.
    if (strict) SECTION("a server of both kinds gives one of them, without a warning") {
        TlsServer server(pki().server(), echo, 1, TLS1_2_VERSION);
        // The server's order, the one without first.
        REQUIRE( SSL_CTX_set_cipher_list(server.ctx,
                    "ECDHE-ECDSA-AES128-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:@SECLEVEL=0") == 1 );
        SSL_CTX_set_options(server.ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        CHECK( std::string(ebisu_polarssl_socket_get_cipher(&client.tls)) ==
                "TLS-ECDHE-ECDSA-WITH-AES-128-GCM-SHA256" );
        CHECK( client.logger.count(SUITE_WARNING) == 0 );
    }
    SECTION("the application's suites are taken, with the warning, once a socket, for one without") {
        // One every build the tests run with has.
        static const int suites[] = { TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256, 0 };
        const int* has = ssl_list_ciphersuites();
        while (*has != 0 && *has != suites[0]) {
            ++has;
        }
        REQUIRE( *has == suites[0] );
        config.suites = suites;
        TlsServer server(pki().server(), echo, 2, TLS1_2_VERSION);
        REQUIRE( SSL_CTX_set_cipher_list(server.ctx,
                    "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-SHA256:@SECLEVEL=0") == 1 );
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        CHECK( std::string(ebisu_polarssl_socket_get_cipher(&client.tls)) == CBC_SUITE );
        REQUIRE( client.sock->ops->close(client.sock) == EBISU_SOCK_OK );
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
#ifndef EBISU_NO_LOG
        CHECK( client.logger.count(SUITE_WARNING) == 1 );
        CHECK( client.logger.count(NO_STRICT_SUITE) == (strict ? 0 : 1) );
#endif
    }
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
        bool strict = has_strict_suite();
        REQUIRE( SSL_CTX_set_cipher_list(server.ctx, strict ? "ECDHE-ECDSA-AES128-GCM-SHA256" :
                    "ECDHE-ECDSA-AES128-SHA256:@SECLEVEL=0") == 1 );
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        REQUIRE( ebisu_polarssl_socket_get_protocol(&client.tls) != nullptr );
        CHECK( std::string(ebisu_polarssl_socket_get_protocol(&client.tls)) == "TLSv1.2" );
        REQUIRE( ebisu_polarssl_socket_get_cipher(&client.tls) != nullptr );
        CHECK( std::string(ebisu_polarssl_socket_get_cipher(&client.tls)) ==
                (strict ? "TLS-ECDHE-ECDSA-WITH-AES-128-GCM-SHA256" : CBC_SUITE) );
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

namespace {

// The header of a record of this type and length, of TLS 1.2, without the
// record.
std::string record_header(unsigned char type, unsigned int length)
{
    const char header[] = { (char)type, 0x03, 0x03, (char)(length >> 8), (char)(length & 0xff) };
    return std::string(header, sizeof(header));
}

// Sends what is given, at once if plain, after reading the ClientHello, and
// then waits for the client to close.
TlsServer::serve_t send_then_wait(const std::string& data, bool plain)
{
    return [data, plain](SSL*, int fd) {
        char buffer[2048];
        if (plain) {
            (void)!read(fd, buffer, sizeof(buffer));
        }
        (void)!write(fd, data.data(), data.size());
        while (read(fd, buffer, sizeof(buffer)) > 0) {
        }
    };
}

} // namespace

// A server may send up to 16384 bytes of data in a record, which a build that
// takes fewer, as embedded ones are, refuses. 18432 bytes, the most a TLS 1.2
// record may hold, are more than any build takes.
TEST_CASE( "The PolarSSL socket says when the server sends a record longer than it takes" ) {
    Config config;
    const std::string takes = " this PolarSSL takes, as SSL_MAX_CONTENT_LEN sets it.";
    const std::string more_than = "more than the " + std::to_string(SSL_MAX_CONTENT_LEN) + takes;

    SECTION("in the handshake") {
        TlsServer server(pki().server(), send_then_wait(record_header(22, 18432), true), 1, 0, true);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_TLS );
        CHECK( detail == POLARSSL_ERR_SSL_INVALID_RECORD );
#ifdef EBISU_NO_LOG
        CHECK( client.logger.quiet() );
#else
        CHECK( client.logger.has("localhost sent a record of 18432 bytes, " + more_than) );
#endif
    }

    SECTION("once connected") {
        TlsServer server(pki().server(), send_then_wait(record_header(23, 18432), false));
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        ebisu_sock_code_t code = EBISU_SOCK_OK;
        client.recv(16, &code);
        CHECK( code == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_TLS );
        CHECK( detail == POLARSSL_ERR_SSL_INVALID_RECORD );
#ifndef EBISU_NO_LOG
        CHECK( client.logger.has("the server sent a record of 18432 bytes, " + more_than) );
#endif
    }

    SECTION("but not of a server that isn't TLS") {
        // Whose "P/" would be a length of 20527.
        TlsServer server(pki().server(),
                send_then_wait("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n", true), 1, 0, true);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_TLS );
        CHECK( detail == POLARSSL_ERR_SSL_INVALID_RECORD );
        CHECK_FALSE( client.logger.has("sent a record of") );
    }

    SECTION("nor of a record of a type TLS hasn't") {
        TlsServer server(pki().server(), send_then_wait(record_header(99, 18432), true), 1, 0, true);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        CHECK_FALSE( client.logger.has("sent a record of") );
    }

    SECTION("nor of a record of a version TLS hasn't") {
        std::string header = record_header(22, 18432);
        header[1] = 9;
        TlsServer server(pki().server(), send_then_wait(header, true), 1, 0, true);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        CHECK_FALSE( client.logger.has("sent a record of") );
    }

    SECTION("just over, which fails as for a MAC that doesn't match") {
        // As a record whose data is just over what the build takes does with
        // AES-CBC and no encrypt-then-MAC, in mbedTLS 2.4 and PolarSSL: here
        // a record of what it takes and 32 bytes, its data not what it says.
        const unsigned int length = SSL_MAX_CONTENT_LEN + 32;
        TlsServer server(pki().server(),
                send_then_wait(record_header(23, length) + std::string(length, 'x'), false));
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        ebisu_sock_code_t code = EBISU_SOCK_OK;
        client.recv(16, &code);
        CHECK( code == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_TLS );
        CHECK( detail == POLARSSL_ERR_SSL_INVALID_MAC );
#ifndef EBISU_NO_LOG
        CHECK( client.logger.has("the server sent a record of " + std::to_string(length) +
                " bytes, " + more_than) );
#endif
    }

    SECTION("but not of one no longer than it takes whose MAC doesn't match") {
        const unsigned int length = SSL_MAX_CONTENT_LEN;
        TlsServer server(pki().server(),
                send_then_wait(record_header(23, length) + std::string(length, 'x'), false));
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        ebisu_sock_code_t code = EBISU_SOCK_OK;
        client.recv(16, &code);
        CHECK( code == EBISU_SOCK_FAIL );
        long detail = 0;
        CHECK( client.error(&detail) == EBISU_SOCK_ERROR_TLS );
        CHECK( detail == POLARSSL_ERR_SSL_INVALID_MAC );
        CHECK_FALSE( client.logger.has("sent a record of") );
    }

    SECTION("nor of a record no longer than it takes") {
        // Of a TLS version that isn't, which is refused whatever its length.
        std::string header = record_header(22, SSL_MAX_CONTENT_LEN);
        header[2] = 9;
        TlsServer server(pki().server(), send_then_wait(header + std::string(SSL_MAX_CONTENT_LEN, 'x'), true), 1, 0, true);
        Client client(&config.tls);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_FAIL );
        CHECK_FALSE( client.logger.has("sent a record of") );
    }
}

namespace {

// Says nothing, for as long as the client stays.
void say_nothing(SSL*, int fd)
{
    char buffer[2048];
    while (read(fd, buffer, sizeof(buffer)) > 0) {
    }
}

// The start of a handshake record of 16384 bytes, then a byte of it every
// 100 ms: each wait for the server is short, and there is no end to them.
void drip(SSL*, int fd)
{
    const char header[] = { 0x16, 0x03, 0x03, 0x40, 0x00 };
    (void)!write(fd, header, sizeof(header));
    for (int i = 0; i < 100; ++i) {
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

} // namespace

// The socket under it bounds each wait for the server, and not how many of
// them a handshake takes.
TEST_CASE( "The PolarSSL socket gives up a handshake at the time set for it" ) {
    Config config;
    const char ran_out[] = "TLS handshake with localhost not done in the 1000 ms it has.";

    SECTION("with a server that says nothing, and no receive timeout") {
        TlsServer server(pki().server(), say_nothing, 1, 0, true);
        Client client(&config.tls, 0, 0);
        ebisu_polarssl_socket_set_handshake_timeout(&client.tls, ebisu_posix_system(), 1);
        auto start = std::chrono::steady_clock::now();
        CHECK( client.connect(server.port) == EBISU_SOCK_FAIL );
        CHECK( seconds_since(start) >= 0.9 );
        CHECK( seconds_since(start) < 4 );
        CHECK( client.error() == EBISU_SOCK_ERROR_TIMEOUT );
#ifndef EBISU_NO_LOG
        CHECK( client.logger.has(ran_out) );
#endif
    }

    SECTION("with a server that sends a byte at a time") {
        TlsServer server(pki().server(), drip, 1, 0, true);
        Client client(&config.tls, 5, 5);
        ebisu_polarssl_socket_set_handshake_timeout(&client.tls, ebisu_posix_system(), 1);
        auto start = std::chrono::steady_clock::now();
        CHECK( client.connect(server.port) == EBISU_SOCK_FAIL );
        CHECK( seconds_since(start) >= 0.9 );
        CHECK( seconds_since(start) < 4 );
        CHECK( client.error() == EBISU_SOCK_ERROR_TIMEOUT );
    }

    SECTION("over a socket without recv_wait, between its receives") {
        TlsServer server(pki().server(), drip, 1, 0, true);
        ebisu_posix_tcp_socket_t tcp;
        ebisu_posix_tcp_socket_init(&tcp, 5, 5);
        ebisu_socket_ops_t ops = *tcp.base.ops;
        ops.recv_wait = nullptr;
        tcp.base.ops = &ops;
        ebisu_polarssl_socket_t tls;
        ebisu_polarssl_socket_init(&tls, &tcp.base, &config.tls);
        ebisu_polarssl_socket_set_handshake_timeout(&tls, ebisu_posix_system(), 1);
        auto start = std::chrono::steady_clock::now();
        CHECK( tls.base.ops->connect(&tls.base, "localhost", server.port) == EBISU_SOCK_FAIL );
        CHECK( seconds_since(start) >= 0.9 );
        CHECK( seconds_since(start) < 4 );
        long detail = 0;
        CHECK( tls.base.ops->last_error(&tls.base, &detail) == EBISU_SOCK_ERROR_TIMEOUT );
        tls.base.ops->close(&tls.base);
    }

    SECTION("and a handshake within it leaves the receive timeout as it was") {
        std::atomic<bool> timed_out(false);
        TlsServer server(pki().server(), [&timed_out](SSL* ssl, int) {
            wait_for(timed_out);
            SSL_write(ssl, "late", 4);
            char buffer[16];
            SSL_read(ssl, buffer, sizeof(buffer));
        });
        Client client(&config.tls, 1);
        ebisu_polarssl_socket_set_handshake_timeout(&client.tls, ebisu_posix_system(), 5);
        REQUIRE( client.connect(server.port) == EBISU_SOCK_OK );
        auto start = std::chrono::steady_clock::now();
        ebisu_sock_code_t code;
        client.recv(64, &code);
        CHECK( code == EBISU_SOCK_FAIL );
        CHECK( client.error() == EBISU_SOCK_ERROR_TIMEOUT );
        CHECK( seconds_since(start) >= 0.9 );
        CHECK( seconds_since(start) < 4 );
        CHECK_FALSE( client.logger.has("not done in") );
        timed_out = true;
        CHECK( client.recv(64) == "late" );
    }

    SECTION("unset, by the socket under it alone, as before") {
        TlsServer server(pki().server(), say_nothing, 1, 0, true);
        Client client(&config.tls, 1, 1);
        auto start = std::chrono::steady_clock::now();
        CHECK( client.connect(server.port) == EBISU_SOCK_FAIL );
        CHECK( seconds_since(start) >= 0.9 );
        CHECK( seconds_since(start) < 4 );
        CHECK( client.error() == EBISU_SOCK_ERROR_TIMEOUT );
        CHECK_FALSE( client.logger.has("not done in") );
    }

    SECTION("with no system, or one without a monotonic clock, or 0 seconds, there is none") {
        Client client(&config.tls);
        ebisu_polarssl_socket_set_handshake_timeout(&client.tls, nullptr, 3);
        CHECK( client.tls._handshake_ms == 0 );
        ebisu_system_ops_t ops = *ebisu_posix_system()->ops;
        ops.monotonic_ms = nullptr;
        ebisu_system_t system = { &ops };
        ebisu_polarssl_socket_set_handshake_timeout(&client.tls, &system, 3);
        CHECK( client.tls._handshake_ms == 0 );
        ebisu_polarssl_socket_set_handshake_timeout(&client.tls, ebisu_posix_system(), 0);
        CHECK( client.tls._handshake_ms == 0 );
        ebisu_polarssl_socket_set_handshake_timeout(&client.tls, ebisu_posix_system(), 3);
        CHECK( client.tls._handshake_ms == 3000 );
    }
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

// What connecting to host gives, at a server whose certificate has alt_names
// for its subjectAltName, or none without any, and name for its commonName.
ebisu_sock_error_t connect_to_host(const char* host, const std::string& alt_names,
        ebisu_tls_verify_t verify = EBISU_TLS_VERIFY_FULL, const char* name = "other.invalid")
{
    Config config;
    REQUIRE( ebisu_polarssl_tls_set_verify(&config.tls, verify, nullptr) );
    ebisu_sock_error_t error = EBISU_SOCK_ERROR_NONE;
    long detail = 0;
    sni_seen = "no handshake";
    {
        TlsServer server(issue(pki().key, name, false, pki().now - DAY, pki().now + DAY,
                    pki().ca, pki().ca_key, !alt_names.empty(), alt_names), nothing);
        REQUIRE( SSL_CTX_set_tlsext_servername_callback(server.ctx, note_sni) == 1 );
        Client client(&config.tls);
        if (client.connect(server.port, host) != EBISU_SOCK_OK) {
            error = client.error(&detail);
        }
    }
    if (error == EBISU_SOCK_ERROR_VERIFY) {
        // Its name, and nothing else of it.
        CHECK( detail == BADCERT_CN_MISMATCH );
    }
    return error;
}

}

// A certificate names the addresses it is issued for apart from the names:
// in subjectAltNames of a kind of their own, which PolarSSL doesn't match a
// host against. And a ClientHello's server name is a name, never an address
// (RFC 6066, section 3).
TEST_CASE( "The PolarSSL socket checks a host given as an address against the certificate's addresses" ) {
    SECTION("an IPv4 address the certificate is issued for, in any form a socket takes") {
        CHECK( connect_to_host("127.0.0.1", "IP:127.0.0.1") == EBISU_SOCK_ERROR_NONE );
        CHECK( sni_seen == "none" );
        CHECK( connect_to_host("127.1", "IP:127.0.0.1") == EBISU_SOCK_ERROR_NONE );
        CHECK( sni_seen == "none" );
        CHECK( connect_to_host("0x7f000001", "IP:127.0.0.1") == EBISU_SOCK_ERROR_NONE );
        CHECK( sni_seen == "none" );
    }
    SECTION("an IPv6 address the certificate is issued for") {
        // The server's, as IPv6 has an IPv4 address.
        CHECK( connect_to_host("::ffff:127.0.0.1", "IP:::ffff:127.0.0.1") == EBISU_SOCK_ERROR_NONE );
        CHECK( sni_seen == "none" );
        CHECK( connect_to_host("0:0:0:0:0:ffff:7f00:1", "IP:::ffff:127.0.0.1") == EBISU_SOCK_ERROR_NONE );
        CHECK( sni_seen == "none" );
    }
    SECTION("among other names and addresses, in a subjectAltName that is critical or not") {
        CHECK( connect_to_host("127.0.0.1", "DNS:other.invalid,IP:10.0.0.1,IP:::1,IP:127.0.0.1") ==
                EBISU_SOCK_ERROR_NONE );
        CHECK( connect_to_host("127.0.0.1", "critical,IP:10.0.0.1,IP:127.0.0.1") ==
                EBISU_SOCK_ERROR_NONE );
    }
    SECTION("an address the certificate isn't issued for") {
        CHECK( connect_to_host("127.0.0.1", "IP:127.0.0.2") == EBISU_SOCK_ERROR_VERIFY );
        CHECK( sni_seen == "none" );
        CHECK( connect_to_host("127.0.0.1", "IP:10.0.0.1,IP:127.0.0.2,DNS:localhost") ==
                EBISU_SOCK_ERROR_VERIFY );
        // IPv4's is none of IPv6's that has it, nor the other way round.
        CHECK( connect_to_host("127.0.0.1", "IP:::ffff:127.0.0.1") == EBISU_SOCK_ERROR_VERIFY );
        CHECK( connect_to_host("::ffff:127.0.0.1", "IP:127.0.0.1") == EBISU_SOCK_ERROR_VERIFY );
        // Nor of one that begins with its bytes.
        CHECK( connect_to_host("127.0.0.1", "IP:7f00:1::") == EBISU_SOCK_ERROR_VERIFY );
    }
    SECTION("an address the certificate has as a name, which is no address") {
        CHECK( connect_to_host("127.0.0.1", "DNS:127.0.0.1") == EBISU_SOCK_ERROR_VERIFY );
        CHECK( connect_to_host("127.0.0.1", "DNS:other.invalid", EBISU_TLS_VERIFY_FULL,
                    "127.0.0.1") == EBISU_SOCK_ERROR_VERIFY );
        // Nor is a name of the address's bytes.
        CHECK( connect_to_host("127.0.0.1", LOOPBACK_AS_NAME) == EBISU_SOCK_ERROR_VERIFY );
        // Without a subjectAltName at all.
        CHECK( connect_to_host("127.0.0.1", "", EBISU_TLS_VERIFY_FULL, "127.0.0.1") ==
                EBISU_SOCK_ERROR_VERIFY );
    }
    SECTION("no address, when the configuration checks the chain without the name") {
        CHECK( connect_to_host("127.0.0.1", "IP:127.0.0.2", EBISU_TLS_VERIFY_CHAIN) ==
                EBISU_SOCK_ERROR_NONE );
        CHECK( sni_seen == "none" );
    }
    SECTION("a name, with a socket that connected to an address before") {
        Config config;
        Client client(&config.tls);
        {
            TlsServer server(issue(pki().key, "other.invalid", false, pki().now - DAY,
                        pki().now + DAY, pki().ca, pki().ca_key, true, "IP:127.0.0.1"), nothing);
            REQUIRE( client.connect(server.port, "127.0.0.1") == EBISU_SOCK_OK );
            CHECK( client.sock->ops->close(client.sock) == EBISU_SOCK_OK );
        }
        TlsServer server(pki().server(), nothing);
        CHECK( client.connect(server.port) == EBISU_SOCK_OK );
    }
    SECTION("a name, as before: sent to the server, and checked against the names") {
        CHECK( connect_to_host("localhost", "DNS:localhost,IP:127.0.0.1") == EBISU_SOCK_ERROR_NONE );
        CHECK( sni_seen == "localhost" );
        CHECK( connect_to_host("localhost", "DNS:other.invalid,IP:127.0.0.1") ==
                EBISU_SOCK_ERROR_VERIFY );
        CHECK( sni_seen == "localhost" );
    }
}

#if defined(EBISU_TLS_RESUME)
namespace {

// A system whose monotonic_ms is the test's to move on.
struct Clocked {
    ebisu_system_t system;
    ebisu_system_ops_t ops;
    uint64_t now_ms;

    static uint64_t monotonic(ebisu_system_t* self) {
        return ((Clocked*)self)->now_ms;
    }

    Clocked() : now_ms(1000) {
        memset(&ops, 0, sizeof(ops));
        ops.monotonic_ms = monotonic;
        system.ops = &ops;
    }
};

// Whether each connection's handshake resumed a session, as the server saw
// it, for servers that echo once.
struct Resumptions {
    std::mutex lock;
    std::vector<bool> reused;

    TlsServer::serve_t serve() {
        return [this](SSL* ssl, int fd) {
            {
                std::lock_guard<std::mutex> guard(lock);
                reused.push_back(SSL_session_reused(ssl) == 1);
            }
            echo(ssl, fd);
        };
    }

    std::vector<bool> seen() {
        std::lock_guard<std::mutex> guard(lock);
        return reused;
    }
};

// A connection, a round trip on it, and its close.
void round_trip(Client& client, uint16_t port)
{
    REQUIRE( client.connect(port) == EBISU_SOCK_OK );
    client.send("again");
    CHECK( client.recv(64) == "again" );
    CHECK( client.sock->ops->close(client.sock) == EBISU_SOCK_OK );
}

const int64_t MINUTE_MS = 60 * 1000;

}

// Every connection made a full handshake: the server's certificate chain and
// a key exchange, though khc makes a connection for each request. Built with
// EBISU_TLS_RESUME, a socket asked to keeps its last connection's session,
// for the next to the same host and port to resume.
TEST_CASE( "The PolarSSL socket resumes the session of its last connection, when asked to" ) {
    Config config;
    Resumptions seen;
    Clocked clock;
    typedef std::vector<bool> resumed;

    SECTION("not unless asked") {
        TlsServer server(pki().server(), seen.serve(), 2, TLS1_2_VERSION);
        Client client(&config.tls);
        round_trip(client, server.port);
        round_trip(client, server.port);
        CHECK( seen.seen() == resumed({ false, false }) );
    }

    SECTION("the next connections, once asked") {
        TlsServer server(pki().server(), seen.serve(), 3, TLS1_2_VERSION);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_session_resume(&client.tls, &clock.system);
        round_trip(client, server.port);
        round_trip(client, server.port);
        round_trip(client, server.port);
        CHECK( seen.seen() == resumed({ false, true, true }) );
        CHECK( client.logger.quiet() );
    }

    SECTION("for an hour after the full handshake, however often resumed meanwhile") {
        TlsServer server(pki().server(), seen.serve(), 4, TLS1_2_VERSION);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_session_resume(&client.tls, &clock.system);
        round_trip(client, server.port);
        clock.now_ms += 59 * MINUTE_MS;
        round_trip(client, server.port);
        clock.now_ms += 2 * MINUTE_MS;
        round_trip(client, server.port);
        // The session of that full handshake.
        round_trip(client, server.port);
        CHECK( seen.seen() == resumed({ false, true, false, true }) );
    }

    SECTION("only to the same host and port") {
        Resumptions other_seen;
        TlsServer server(pki().server(), seen.serve(), 2, TLS1_2_VERSION);
        TlsServer other(pki().server(), other_seen.serve(), 1, TLS1_2_VERSION);
        // The same ticket keys, so that the other would resume a session of
        // the first's if it were offered one.
        unsigned char keys[80];
        REQUIRE( SSL_CTX_get_tlsext_ticket_keys(server.ctx, keys, sizeof(keys)) == 1 );
        REQUIRE( SSL_CTX_set_tlsext_ticket_keys(other.ctx, keys, sizeof(keys)) == 1 );
        Client client(&config.tls);
        ebisu_polarssl_socket_set_session_resume(&client.tls, &clock.system);
        round_trip(client, server.port);
        round_trip(client, other.port);
        round_trip(client, server.port);
        CHECK( seen.seen() == resumed({ false, false }) );
        CHECK( other_seen.seen() == resumed({ false }) );
    }

    SECTION("only while the configuration checks certificates as it did") {
        TlsServer server(pki().server(), seen.serve(), 3, TLS1_2_VERSION);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_session_resume(&client.tls, &clock.system);
        REQUIRE( ebisu_polarssl_tls_set_verify(&config.tls, EBISU_TLS_VERIFY_NONE, nullptr) );
        round_trip(client, server.port);
        // A session of a connection that checked nothing, resumed, would
        // check nothing either.
        REQUIRE( ebisu_polarssl_tls_set_verify(&config.tls, EBISU_TLS_VERIFY_FULL, nullptr) );
        round_trip(client, server.port);
        round_trip(client, server.port);
        CHECK( seen.seen() == resumed({ false, false, true }) );
    }

    SECTION("none once told so, freeing the one kept") {
        TlsServer server(pki().server(), seen.serve(), 2, TLS1_2_VERSION);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_session_resume(&client.tls, &clock.system);
        round_trip(client, server.port);
        ebisu_polarssl_socket_set_session_resume(&client.tls, nullptr);
        round_trip(client, server.port);
        CHECK( seen.seen() == resumed({ false, false }) );
    }

    SECTION("with a server whose highest is TLS 1.3, over TLS 1.2, PolarSSL's highest") {
        TlsServer server(pki().server(), seen.serve(), 2);
        Client client(&config.tls);
        ebisu_polarssl_socket_set_session_resume(&client.tls, &clock.system);
        round_trip(client, server.port);
        round_trip(client, server.port);
        CHECK( seen.seen() == resumed({ false, true }) );
    }
}
#endif
