/* Calls the operations of platform's Schannel port one at a time, as
 * tls_test.py tells it to, and prints what each returned.
 *
 *   schannel_socket_test trust <certificate.der>
 *       Adds the certificate to the local machine's root store, so that
 *       servers with certificates it issued are trusted. Only under wine,
 *       where tls_test.py runs it in a wine prefix of its own.
 *
 *   schannel_socket_test <host> <port> <receive timeout, s> <step>...
 *       Runs the steps in order on one ebisu_schannel_socket_t, printing a line
 *       starting with "STEP" for each, and what the socket logs on lines of
 *       their own, starting with "LOG" and the level:
 *       connect          connect, and how long it took
 *       send:<text>      send until all of text is sent; \n in
 *                        text stands for a newline
 *       sendbig:<n>      send until n bytes are sent
 *       recv:<n>         one recv into an n-byte buffer, and
 *                        recv_timed_out if it failed
 *       drain:<n>        recv into an n-byte buffer until it fails
 *                        or returns 0 bytes, and the FNV-1a hash of it all
 *       close            close
 *       host:<host>      connect to host from now on
 *       to_send:<s>      init the socket again, with s for its send timeout
 *       to_connect:<s>   set the connect timeout
 *       pause:<ms>       sleep for ms milliseconds
 *       clock:<kind>:<s> check certificates' dates at a clock's time: an
 *                        ebisu_server_clock_t given the time s seconds from
 *                        now (exact) or that for its floor (floor), or the
 *                        system's clock (system)
 *       clockstate       whether the clock has the time, or a lower bound
 *
 *       After a connect, send or recv that fails, last_error is printed too,
 *       as error=<name> detail=<hex>.
 */
#include "ebisu_schannel.h"
#include "ebisu_server_clock.h"
#include "ebisu_win32.h"

#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* The clock of the clock: steps. */
static ebisu_server_clock_t server_clock;
static ebisu_clock_t* clock_set = NULL;

static const char* code_name(ebisu_sock_code_t code)
{
    switch (code) {
        case EBISU_SOCK_OK:
            return "OK";
        case EBISU_SOCK_FAIL:
            return "FAIL";
        case EBISU_SOCK_AGAIN:
            return "AGAIN";
    }
    return "?";
}

static const char* error_name(ebisu_sock_error_t error)
{
    switch (error) {
        case EBISU_SOCK_ERROR_NONE: return "NONE";
        case EBISU_SOCK_ERROR_DNS: return "DNS";
        case EBISU_SOCK_ERROR_CONNECT: return "CONNECT";
        case EBISU_SOCK_ERROR_TLS: return "TLS";
        case EBISU_SOCK_ERROR_VERIFY: return "VERIFY";
        case EBISU_SOCK_ERROR_TIMEOUT: return "TIMEOUT";
        case EBISU_SOCK_ERROR_CLOSED: return "CLOSED";
        case EBISU_SOCK_ERROR_OTHER: return "OTHER";
        case EBISU_SOCK_ERROR_TLS_MISMATCH: return "TLS_MISMATCH";
    }
    return "?";
}

/* What last_error says, after a step that failed. */
static void print_last_error(ebisu_schannel_socket_t* ctx, ebisu_sock_code_t res)
{
    if (res != EBISU_SOCK_FAIL) {
        return;
    }
    long detail = 0;
    ebisu_sock_error_t error = ctx->base.ops->last_error(&ctx->base, &detail);
    printf(" error=%s detail=%lx", error_name(error), (unsigned long)detail);
}

static void to_stdout(ebisu_logger_t* self, ebisu_log_level_t level,
        const char* fmt, va_list args)
{
    static const char* const levels[] = { "error", "warning", "info", "debug" };
    (void)self;
    printf("LOG %s ", levels[level]);
    vprintf(fmt, args);
    printf("\n");
}

static const ebisu_logger_ops_t stdout_ops = { to_stdout };
static ebisu_logger_t stdout_logger = { &stdout_ops };

#define FNV_OFFSET_BASIS 0xcbf29ce484222325ULL

static uint64_t fnv1a(uint64_t hash, const char* data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        hash ^= (unsigned char)data[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

static int trust(const char* path)
{
    /* Never on a real Windows machine. */
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll == NULL || GetProcAddress(ntdll, "wine_get_version") == NULL) {
        printf("trust is only for a wine prefix of tls_test.py's own.\n");
        return 2;
    }

    static BYTE der[65536];
    FILE* fp = fopen(path, "rb");
    if (fp == NULL) {
        printf("cannot open %s\n", path);
        return 1;
    }
    size_t len = fread(der, 1, sizeof(der), fp);
    fclose(fp);

    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_A, 0, 0,
            CERT_SYSTEM_STORE_LOCAL_MACHINE, "ROOT");
    if (store == NULL) {
        printf("CertOpenStore: 0x%08lx\n", GetLastError());
        return 1;
    }
    BOOL added = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
            der, (DWORD)len, CERT_STORE_ADD_REPLACE_EXISTING, NULL);
    if (!added) {
        printf("CertAddEncodedCertificateToStore: 0x%08lx\n", GetLastError());
    }
    CertCloseStore(store, 0);
    if (!added) {
        return 1;
    }
    printf("trusted %s\n", path);
    return 0;
}

static ebisu_sock_code_t send_all(ebisu_schannel_socket_t* ctx, const char* data,
        size_t len, size_t* total, int* calls)
{
    ebisu_sock_code_t res = EBISU_SOCK_OK;
    *total = 0;
    *calls = 0;
    while (*total < len && res == EBISU_SOCK_OK) {
        size_t sent = 0;
        res = ctx->base.ops->send(&ctx->base, data + *total, len - *total, &sent);
        *total += sent;
        ++*calls;
    }
    return res;
}

static void print_escaped(const char* data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)data[i];
        if (c == '\n') {
            printf("\\n");
        } else if (c < 32 || c > 126) {
            printf("\\x%02x", c);
        } else {
            putchar(c);
        }
    }
}

/* An option of a socket, as getsockopt() reads it, or -1. */
static long option_of(SOCKET sock, int level, int name)
{
    DWORD value = 0;
    int len = sizeof(value);
    if (getsockopt(sock, level, name, (char*)&value, &len) != 0) {
        return -1;
    }
    return (long)value;
}

static int run_step(ebisu_schannel_socket_t* ctx, const char** host, uint16_t port,
        unsigned int to_recv, const char* step)
{
    if (strcmp(step, "connect") == 0) {
        DWORD start = GetTickCount();
        ebisu_sock_code_t res = ctx->base.ops->connect(&ctx->base, *host, port);
        DWORD elapsed = GetTickCount() - start;
        printf("STEP connect %s", code_name(res));
        print_last_error(ctx, res);
        printf(" ms=%lu\n", (unsigned long)elapsed);
    } else if (strncmp(step, "send:", 5) == 0) {
        size_t len = 0;
        char* text = (char*)malloc(strlen(step));
        for (const char* p = step + 5; *p != '\0'; ++p) {
            if (p[0] == '\\' && p[1] == 'n') {
                text[len++] = '\n';
                ++p;
            } else {
                text[len++] = *p;
            }
        }
        size_t total;
        int calls;
        ebisu_sock_code_t res = send_all(ctx, text, len, &total, &calls);
        printf("STEP send %s %lu", code_name(res), (unsigned long)total);
        print_last_error(ctx, res);
        printf("\n");
        free(text);
    } else if (strncmp(step, "sendbig:", 8) == 0) {
        size_t len = (size_t)strtoul(step + 8, NULL, 10);
        char* data = (char*)malloc(len);
        for (size_t i = 0; i < len; ++i) {
            data[i] = (char)('a' + i % 26);
        }
        size_t total;
        int calls;
        ebisu_sock_code_t res = send_all(ctx, data, len, &total, &calls);
        printf("STEP sendbig %s %lu calls=%d fnv=%016llx\n", code_name(res),
                (unsigned long)total, calls,
                (unsigned long long)fnv1a(FNV_OFFSET_BASIS, data, total));
        free(data);
    } else if (strncmp(step, "recv:", 5) == 0) {
        size_t size = (size_t)strtoul(step + 5, NULL, 10);
        char* buf = (char*)malloc(size);
        size_t got = 0;
        DWORD start = GetTickCount();
        ebisu_sock_code_t res = ctx->base.ops->recv(&ctx->base, buf, size, &got);
        DWORD elapsed = GetTickCount() - start;
        int timed_out = res == EBISU_SOCK_FAIL &&
            ctx->base.ops->recv_timed_out(&ctx->base);
        printf("STEP recv %s %lu timed_out=%d ms=%lu", code_name(res),
                (unsigned long)got, timed_out, (unsigned long)elapsed);
        print_last_error(ctx, res);
        printf(" data=");
        print_escaped(buf, got > 64 ? 64 : got);
        printf("\n");
        free(buf);
    } else if (strncmp(step, "drain:", 6) == 0) {
        size_t size = (size_t)strtoul(step + 6, NULL, 10);
        char* buf = (char*)malloc(size);
        uint64_t hash = FNV_OFFSET_BASIS;
        unsigned long long total = 0;
        unsigned long calls = 0;
        ebisu_sock_code_t res;
        for (;;) {
            size_t got = 0;
            res = ctx->base.ops->recv(&ctx->base, buf, size, &got);
            ++calls;
            if (got > size) {
                printf("STEP drain received %lu into %lu bytes\n",
                        (unsigned long)got, (unsigned long)size);
                return 1;
            }
            if (res != EBISU_SOCK_OK || got == 0) {
                break;
            }
            hash = fnv1a(hash, buf, got);
            total += got;
        }
        int timed_out = res == EBISU_SOCK_FAIL &&
            ctx->base.ops->recv_timed_out(&ctx->base);
        printf("STEP drain %s bytes=%llu calls=%lu fnv=%016llx timed_out=%d\n",
                code_name(res), total, calls, (unsigned long long)hash,
                timed_out);
        free(buf);
    } else if (strncmp(step, "wait:", 5) == 0) {
        uint32_t ms = (uint32_t)strtoul(step + 5, NULL, 10);
        DWORD start = GetTickCount();
        bool ready = ctx->base.ops->recv_wait(&ctx->base, ms);
        DWORD elapsed = GetTickCount() - start;
        printf("STEP wait %d ms=%lu\n", ready ? 1 : 0, (unsigned long)elapsed);
    } else if (strcmp(step, "close") == 0) {
        printf("STEP close %s\n", code_name(ctx->base.ops->close(&ctx->base)));
    } else if (strcmp(step, "nologger") == 0) {
        ebisu_schannel_socket_set_logger(ctx, NULL);
        printf("STEP nologger OK\n");
    } else if (strncmp(step, "host:", 5) == 0) {
        *host = step + 5;
        printf("STEP host OK\n");
    } else if (strncmp(step, "to_send:", 8) == 0) {
        ebisu_schannel_socket_init(ctx, to_recv, (unsigned int)strtoul(step + 8, NULL, 10));
        ebisu_schannel_socket_set_logger(ctx, &stdout_logger);
        printf("STEP to_send OK\n");
    } else if (strncmp(step, "to_connect:", 11) == 0) {
        ebisu_schannel_socket_set_connect_timeout(ctx, (unsigned int)strtoul(step + 11, NULL, 10));
        printf("STEP to_connect OK\n");
    } else if (strncmp(step, "verify:", 7) == 0) {
        /* verify:full, verify:chain, verify:none, verify:pin:<64 hex digits>,
           or verify:pin alone, for one without a pin. */
        const char* mode = step + 7;
        ebisu_tls_verify_t verify = EBISU_TLS_VERIFY_FULL;
        unsigned char pin[EBISU_TLS_PIN_SIZE];
        const unsigned char* given = NULL;
        if (strcmp(mode, "chain") == 0) {
            verify = EBISU_TLS_VERIFY_CHAIN;
        } else if (strcmp(mode, "none") == 0) {
            verify = EBISU_TLS_VERIFY_NONE;
        } else if (strncmp(mode, "pin", 3) == 0) {
            verify = EBISU_TLS_VERIFY_PIN;
            if (mode[3] == ':' && strlen(mode + 4) == 2 * EBISU_TLS_PIN_SIZE) {
                for (int i = 0; i < EBISU_TLS_PIN_SIZE; ++i) {
                    char byte[3] = { mode[4 + 2 * i], mode[5 + 2 * i], '\0' };
                    pin[i] = (unsigned char)strtoul(byte, NULL, 16);
                }
                given = pin;
            }
        }
        printf("STEP verify %s\n",
                ebisu_schannel_socket_set_verify(ctx, verify, given) ? "OK" : "FAIL");
    } else if (strncmp(step, "keepalive:", 10) == 0) {
        /* keepalive:<idle>,<interval>,<count> */
        unsigned int idle = 0, interval = 0, count = 0;
        sscanf(step + 10, "%u,%u,%u", &idle, &interval, &count);
        ebisu_schannel_socket_set_keepalive(ctx, idle, interval, count);
        printf("STEP keepalive OK\n");
    } else if (strncmp(step, "nodelay:", 8) == 0) {
        ebisu_schannel_socket_set_nodelay(ctx, step[8] == '1');
        printf("STEP nodelay OK\n");
    } else if (strcmp(step, "negotiated") == 0) {
        const char* protocol = ebisu_schannel_socket_get_protocol(ctx);
        const char* cipher = ebisu_schannel_socket_get_cipher(ctx);
        printf("STEP negotiated protocol=%s cipher=%s\n", protocol != NULL ? protocol : "(null)",
                cipher != NULL ? cipher : "(null)");
    } else if (strcmp(step, "options") == 0) {
        /* What getsockopt() reads of the connection's socket, -1 for an
           option it doesn't give. */
        printf("STEP options keepalive=%ld idle=%ld interval=%ld count=%ld nodelay=%ld\n",
                option_of(ctx->socket, SOL_SOCKET, SO_KEEPALIVE),
                option_of(ctx->socket, IPPROTO_TCP, TCP_KEEPIDLE),
                option_of(ctx->socket, IPPROTO_TCP, TCP_KEEPINTVL),
                option_of(ctx->socket, IPPROTO_TCP, TCP_KEEPCNT),
                option_of(ctx->socket, IPPROTO_TCP, TCP_NODELAY));
    } else if (strncmp(step, "clock:", 6) == 0) {
        /* clock:exact:<seconds from now>, clock:floor:<seconds from now>, or
           clock:system. */
        bool ok = true;
        if (strcmp(step + 6, "system") == 0) {
            clock_set = ebisu_system_clock(ebisu_win32_system());
        } else {
            ok = ebisu_server_clock_init(&server_clock, ebisu_win32_system());
            const char* at = strchr(step + 6, ':');
            int64_t seconds = (int64_t)time(NULL) + (at != NULL ? atol(at + 1) : 0);
            if (strncmp(step + 6, "exact:", 6) == 0) {
                ok = ok && ebisu_server_clock_set_time(&server_clock, seconds);
            } else {
                ebisu_server_clock_set_floor(&server_clock, seconds);
            }
            clock_set = &server_clock.base;
        }
        ebisu_schannel_socket_set_clock(ctx, clock_set);
        printf("STEP clock %s\n", ok ? "OK" : "FAIL");
    } else if (strcmp(step, "clockstate") == 0) {
        int64_t seconds = 0;
        printf("STEP clockstate %s\n", clock_set == NULL ? "none" :
                ebisu_clock_get_time(clock_set, &seconds) ? "exact" : "bound");
    } else if (strncmp(step, "pause:", 6) == 0) {
        Sleep((DWORD)strtoul(step + 6, NULL, 10));
        printf("STEP pause OK\n");
    } else {
        printf("unknown step %s\n", step);
        return 1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc == 3 && strcmp(argv[1], "trust") == 0) {
        return trust(argv[2]);
    }
    if (argc < 5) {
        printf("usage: schannel_socket_test trust <certificate.der>\n"
                "       schannel_socket_test <host> <port> <receive timeout> <step>...\n");
        return 2;
    }

    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    ebisu_schannel_socket_t ctx;
    unsigned int to_recv = (unsigned int)atoi(argv[3]);
    ebisu_schannel_socket_init(&ctx, to_recv, 10);
    ebisu_schannel_socket_set_logger(&ctx, &stdout_logger);
    const char* host = argv[1];
    int result = 0;
    for (int i = 4; i < argc && result == 0; ++i) {
        result = run_step(&ctx, &host, (uint16_t)atoi(argv[2]), to_recv, argv[i]);
    }
    WSACleanup();
    return result;
}
