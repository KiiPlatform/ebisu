/* Calls the callbacks in sock_cb_win32.c one at a time, as tls_test.py tells
 * it to, and prints what each returned.
 *
 *   sock_cb_test trust <certificate.der>
 *       Adds the certificate to the local machine's root store, so that
 *       servers with certificates it issued are trusted. Only under wine,
 *       where tls_test.py runs it in a wine prefix of its own.
 *
 *   sock_cb_test <host> <port> <receive timeout, s> <step>...
 *       Runs the steps in order on one socket_context_t, printing a line
 *       starting with "STEP" for each:
 *       connect          sock_cb_connect()
 *       send:<text>      sock_cb_send() until all of text is sent; \n in
 *                        text stands for a newline
 *       sendbig:<n>      sock_cb_send() until n bytes are sent
 *       recv:<n>         one sock_cb_recv() into an n-byte buffer, and
 *                        sock_cb_recv_timed_out() if it failed
 *       drain:<n>        sock_cb_recv() into an n-byte buffer until it fails
 *                        or returns 0 bytes, and the FNV-1a hash of it all
 *       close            sock_cb_close()
 */
#include "sock_cb_win32.h"

#include <windows.h>
#include <wincrypt.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* code_name(khc_sock_code_t code)
{
    switch (code) {
        case KHC_SOCK_OK:
            return "OK";
        case KHC_SOCK_FAIL:
            return "FAIL";
        case KHC_SOCK_AGAIN:
            return "AGAIN";
    }
    return "?";
}

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

static khc_sock_code_t send_all(socket_context_t* ctx, const char* data,
        size_t len, size_t* total, int* calls)
{
    khc_sock_code_t res = KHC_SOCK_OK;
    *total = 0;
    *calls = 0;
    while (*total < len && res == KHC_SOCK_OK) {
        size_t sent = 0;
        res = sock_cb_send(ctx, data + *total, len - *total, &sent);
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

static int run_step(socket_context_t* ctx, const char* host, uint16_t port,
        const char* step)
{
    if (strcmp(step, "connect") == 0) {
        printf("STEP connect %s\n", code_name(sock_cb_connect(ctx, host, port)));
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
        khc_sock_code_t res = send_all(ctx, text, len, &total, &calls);
        printf("STEP send %s %lu\n", code_name(res), (unsigned long)total);
        free(text);
    } else if (strncmp(step, "sendbig:", 8) == 0) {
        size_t len = (size_t)strtoul(step + 8, NULL, 10);
        char* data = (char*)malloc(len);
        for (size_t i = 0; i < len; ++i) {
            data[i] = (char)('a' + i % 26);
        }
        size_t total;
        int calls;
        khc_sock_code_t res = send_all(ctx, data, len, &total, &calls);
        printf("STEP sendbig %s %lu calls=%d fnv=%016llx\n", code_name(res),
                (unsigned long)total, calls,
                (unsigned long long)fnv1a(FNV_OFFSET_BASIS, data, total));
        free(data);
    } else if (strncmp(step, "recv:", 5) == 0) {
        size_t size = (size_t)strtoul(step + 5, NULL, 10);
        char* buf = (char*)malloc(size);
        size_t got = 0;
        DWORD start = GetTickCount();
        khc_sock_code_t res = sock_cb_recv(ctx, buf, size, &got);
        DWORD elapsed = GetTickCount() - start;
        int timed_out = res == KHC_SOCK_FAIL &&
            sock_cb_recv_timed_out(ctx) == KII_TRUE;
        printf("STEP recv %s %lu timed_out=%d ms=%lu data=", code_name(res),
                (unsigned long)got, timed_out, (unsigned long)elapsed);
        print_escaped(buf, got > 64 ? 64 : got);
        printf("\n");
        free(buf);
    } else if (strncmp(step, "drain:", 6) == 0) {
        size_t size = (size_t)strtoul(step + 6, NULL, 10);
        char* buf = (char*)malloc(size);
        uint64_t hash = FNV_OFFSET_BASIS;
        unsigned long long total = 0;
        unsigned long calls = 0;
        khc_sock_code_t res;
        for (;;) {
            size_t got = 0;
            res = sock_cb_recv(ctx, buf, size, &got);
            ++calls;
            if (got > size) {
                printf("STEP drain received %lu into %lu bytes\n",
                        (unsigned long)got, (unsigned long)size);
                return 1;
            }
            if (res != KHC_SOCK_OK || got == 0) {
                break;
            }
            hash = fnv1a(hash, buf, got);
            total += got;
        }
        int timed_out = res == KHC_SOCK_FAIL &&
            sock_cb_recv_timed_out(ctx) == KII_TRUE;
        printf("STEP drain %s bytes=%llu calls=%lu fnv=%016llx timed_out=%d\n",
                code_name(res), total, calls, (unsigned long long)hash,
                timed_out);
        free(buf);
    } else if (strcmp(step, "close") == 0) {
        printf("STEP close %s\n", code_name(sock_cb_close(ctx)));
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
        printf("usage: sock_cb_test trust <certificate.der>\n"
                "       sock_cb_test <host> <port> <receive timeout> <step>...\n");
        return 2;
    }

    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    socket_context_t ctx;
    sock_cb_init_context(&ctx, (unsigned int)atoi(argv[3]), 10);
    int result = 0;
    for (int i = 4; i < argc && result == 0; ++i) {
        result = run_step(&ctx, argv[1], (uint16_t)atoi(argv[2]), argv[i]);
    }
    WSACleanup();
    return result;
}
