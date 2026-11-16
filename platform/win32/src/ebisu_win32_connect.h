/* Looking a host up and connecting to it, over Winsock 2, for the sockets of
   platform's Win32 and Schannel ports: the TCP that comes before the TLS of
   one, and all there is of the other. Each library compiles it in, static, so
   that neither needs the other.

   Not a header of the API: it defines what it declares, and is included once,
   by a source file that has defined log_to(), as ebisu_log() or nothing. It
   defines log_code_to(), for what failed with a Windows error code. */
#ifndef _EBISU_WIN32_CONNECT
#define _EBISU_WIN32_CONNECT

#include <ebisu_log.h>
#include <ebisu_object.h>
#include <ebisu_socket.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The longest host name, with its NUL, the sockets look up. */
#define CONNECT_HOST_SIZE 256

#ifdef EBISU_NO_LOG
#define log_code_at(logger, level, code, ...) ((void)0)
#else
#if defined(__GNUC__) || defined(__clang__)
static void log_code_at(ebisu_logger_t* logger, ebisu_log_level_t level, unsigned long code,
        const char* fmt, ...) __attribute__((format(printf, 4, 5)));
#endif

/* Logs, at level, what failed with a Winsock, Win32, SSPI or certificate error
   code, and the system's text for it where there is one. */
static void log_code_at(ebisu_logger_t* logger, ebisu_log_level_t level, unsigned long code,
        const char* fmt, ...)
{
    char what[320];
    va_list args;
    if (logger == NULL) {
        return;
    }
    va_start(args, fmt);
    vsnprintf(what, sizeof(what), fmt, args);
    va_end(args);
    what[sizeof(what) - 1] = '\0';

    char text[256];
    DWORD len = FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, code, 0, text, sizeof(text), NULL);
    while (len > 0 &&
            (text[len - 1] == '\r' || text[len - 1] == '\n' ||
             text[len - 1] == ' ' || text[len - 1] == '.')) {
        --len;
    }
    text[len] = '\0';
    log_to(logger, level, "%s: 0x%08lx%s%s", what, code, len > 0 ? " " : "", text);
}
#endif

/* What failed, as log_code_at() logs it, at EBISU_LOG_ERROR. */
#define log_code_to(logger, code, ...) log_code_at((logger), EBISU_LOG_ERROR, (code), __VA_ARGS__)

/* mingw-w64's headers, 13 at least, leave it out, though its import library
   for ws2_32 has it. */
#ifdef __MINGW32__
WINSOCK_API_LINKAGE int WSAAPI GetAddrInfoExCancel(LPHANDLE lpHandle);
#endif

/* A lookup, which Winsock does while the connecting task waits for it, no
   longer than its timeout. Winsock calls lookup_done() once it is done, on a
   thread of its own. A task that gives up cancels it, and leaves it to be
   freed once done, as Winsock writes to it until then. */
typedef struct lookup_t {
    OVERLAPPED overlapped;
    /* Set once the lookup is done. */
    HANDLE done;
    /* For GetAddrInfoExCancel(). */
    HANDLE cancel;
    DWORD error;
    ADDRINFOEXW* result;
    /* The task, until it is done with the lookup, and Winsock, until the
       lookup is done. Freed at none. */
    LONG users;
} lookup_t;

/* Drops a user of lookup, and frees it after the last. */
static void drop_user(lookup_t* lookup)
{
    if (InterlockedDecrement(&lookup->users) > 0) {
        return;
    }
    if (lookup->result != NULL) {
        FreeAddrInfoExW(lookup->result);
    }
    CloseHandle(lookup->done);
    free(lookup);
}

static void CALLBACK lookup_done(DWORD error, DWORD bytes, LPWSAOVERLAPPED overlapped)
{
    (void)bytes;
    lookup_t* lookup = EBISU_CONTAINER_OF(overlapped, lookup_t, overlapped);
    lookup->error = error;
    SetEvent(lookup->done);
    drop_user(lookup);
}

/* The host's addresses, looked up within timeout_s, or for as long as the
   system's resolver takes if that is 0, to be freed with FreeAddrInfoExW().
   NULL if they weren't, with why in out_error and out_detail. */
static ADDRINFOEXW* look_up(const char* host, uint16_t port, unsigned int timeout_s,
        ebisu_logger_t* logger, ebisu_sock_error_t* out_error, long* out_detail)
{
    *out_error = EBISU_SOCK_ERROR_OTHER;
    *out_detail = 0;
    if (strlen(host) >= CONNECT_HOST_SIZE) {
        log_to(logger, EBISU_LOG_ERROR, "failed to look up a host name that long.");
        *out_error = EBISU_SOCK_ERROR_DNS;
        return NULL;
    }
    WCHAR name[CONNECT_HOST_SIZE];
    if (MultiByteToWideChar(CP_UTF8, 0, host, -1, name,
                sizeof(name) / sizeof(name[0])) == 0) {
        DWORD code = GetLastError();
        log_code_to(logger, code, "host name");
        *out_error = EBISU_SOCK_ERROR_DNS;
        *out_detail = (long)code;
        return NULL;
    }
    char digits[6];
    snprintf(digits, sizeof(digits), "%u", (unsigned int)port);
    WCHAR service[sizeof(digits)];
    size_t i = 0;
    for (; digits[i] != '\0'; ++i) {
        service[i] = (WCHAR)digits[i];
    }
    service[i] = L'\0';

    ADDRINFOEXW hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    lookup_t* lookup = (lookup_t*)calloc(1, sizeof(*lookup));
    if (lookup == NULL) {
        log_to(logger, EBISU_LOG_ERROR, "failed to allocate a lookup.");
        return NULL;
    }
    lookup->done = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (lookup->done == NULL) {
        DWORD code = GetLastError();
        log_code_to(logger, code, "CreateEvent");
        *out_detail = (long)code;
        free(lookup);
        return NULL;
    }
    lookup->users = 2;
    /* NS_ALL: every namespace provider's answers, not DNS's alone. */
    int err = GetAddrInfoExW(name, service, NS_ALL, NULL, &hints, &lookup->result,
            NULL, &lookup->overlapped, lookup_done, &lookup->cancel);
    if (err != WSA_IO_PENDING) {
        /* Done already, and lookup_done() won't be called: the task is the
           only user. */
        lookup->error = (DWORD)err;
        lookup->users = 1;
    } else {
        DWORD timeout_ms = timeout_s == 0 ? INFINITE :
            timeout_s >= (INFINITE - 1) / 1000 ? INFINITE - 1 : timeout_s * 1000;
        DWORD waited = WaitForSingleObject(lookup->done, timeout_ms);
        if (waited != WAIT_OBJECT_0) {
            DWORD code = waited == WAIT_FAILED ? GetLastError() : 0;
            /* Winsock ends the lookup, and calls lookup_done() with
               WSA_E_CANCELLED, which Windows Server 2025 does at once, on
               this thread, unless it has just ended by itself, which leaves
               the handle no longer valid and the call changing nothing.
               Either way, the last of the two frees the lookup. */
            GetAddrInfoExCancel(&lookup->cancel);
            drop_user(lookup);
            if (waited == WAIT_TIMEOUT) {
                log_to(logger, EBISU_LOG_ERROR, "failed to look up %s in time.", host);
                *out_error = EBISU_SOCK_ERROR_TIMEOUT;
            } else {
                log_code_to(logger, code, "failed to wait for the lookup of %s", host);
                *out_detail = (long)code;
            }
            return NULL;
        }
    }

    /* Done: lookup_done(), if it is called, writes to the lookup no more. */
    DWORD error = lookup->error;
    ADDRINFOEXW* result = lookup->result;
    lookup->result = NULL;
    drop_user(lookup);
    if (error != 0) {
        if (result != NULL) {
            FreeAddrInfoExW(result);
        }
        log_code_to(logger, error, "failed to look up %s", host);
        *out_error = EBISU_SOCK_ERROR_DNS;
        *out_detail = (long)error;
        return NULL;
    }
    return result;
}

/* Connects sock to addr within timeout_s, or whenever TCP gives up if that is
   0. 0 once connected, otherwise the Winsock error, which is WSAETIMEDOUT
   for the timeout too. */
static int connect_within(SOCKET sock, const struct sockaddr* addr, int addr_len,
        unsigned int timeout_s)
{
    if (timeout_s == 0) {
        return connect(sock, addr, addr_len) == 0 ? 0 : WSAGetLastError();
    }
    u_long nonblocking = 1;
    if (ioctlsocket(sock, FIONBIO, &nonblocking) != 0) {
        return WSAGetLastError();
    }
    int error = connect(sock, addr, addr_len) == 0 ? 0 : WSAGetLastError();
    if (error == WSAEWOULDBLOCK) {
        fd_set connected;
        FD_ZERO(&connected);
        FD_SET(sock, &connected);
        fd_set failed;
        FD_ZERO(&failed);
        FD_SET(sock, &failed);
        struct timeval timeout;
        timeout.tv_sec = timeout_s > LONG_MAX ? LONG_MAX : (long)timeout_s;
        timeout.tv_usec = 0;
        /* Winsock says a connect that succeeded in the write set, one that
           failed in the except set, and ignores the first argument. */
        int ready = select(0, NULL, &connected, &failed, &timeout);
        if (ready == SOCKET_ERROR) {
            error = WSAGetLastError();
        } else if (ready == 0) {
            error = WSAETIMEDOUT;
        } else if (FD_ISSET(sock, &failed)) {
            int so_error = 0;
            int len = (int)sizeof(so_error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len) != 0) {
                error = WSAGetLastError();
            } else {
                error = so_error != 0 ? so_error : WSAECONNREFUSED;
            }
        } else {
            error = 0;
        }
    }
    /* Blocking again, for the handshake and what follows, which the receive
       and send timeouts bound. */
    u_long blocking = 0;
    if (ioctlsocket(sock, FIONBIO, &blocking) != 0 && error == 0) {
        error = WSAGetLastError();
    }
    return error;
}

/* A socket connected to one of host's addresses, looked up and each tried
   within timeout_s, or INVALID_SOCKET, with why in out_error and
   out_detail. */
static SOCKET connect_to_host(const char* host, uint16_t port, unsigned int timeout_s,
        ebisu_logger_t* logger, ebisu_sock_error_t* out_error, long* out_detail)
{
    ADDRINFOEXW* addrs = look_up(host, port, timeout_s, logger, out_error, out_detail);
    if (addrs == NULL) {
        return INVALID_SOCKET;
    }

    /* The first address that accepts, IPv6 or IPv4. Each gets the whole
       timeout: one that doesn't answer leaves the rest as much time as the
       first had. */
    SOCKET sock = INVALID_SOCKET;
    int last_error = 0;
    for (ADDRINFOEXW* addr = addrs; addr != NULL; addr = addr->ai_next) {
        sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            last_error = WSAGetLastError();
            continue;
        }
        last_error = connect_within(sock, addr->ai_addr, (int)addr->ai_addrlen, timeout_s);
        if (last_error == 0) {
            break;
        }
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    FreeAddrInfoExW(addrs);
    if (sock == INVALID_SOCKET) {
        log_code_to(logger, (unsigned long)last_error, "failed to connect to %s", host);
        *out_error = last_error == WSAETIMEDOUT ?
            EBISU_SOCK_ERROR_TIMEOUT : EBISU_SOCK_ERROR_CONNECT;
        *out_detail = last_error;
    }
    return sock;
}

/* Keepalive and TCP_NODELAY, as a socket of these sets them for each
   connection: keepalive off with idle_s 0, as TCP starts, and otherwise on,
   with its interval and count where they aren't 0. */
typedef struct tcp_options_t {
    unsigned int idle_s;
    unsigned int interval_s;
    unsigned int count;
    bool nodelay;
} tcp_options_t;

/* Sets a DWORD option, or says why it couldn't be: the connection goes on
   without it. */
static void set_dword_option(SOCKET sock, int level, int name, const char* what,
        unsigned int value, ebisu_logger_t* logger)
{
    DWORD v = (DWORD)value;
    if (setsockopt(sock, level, name, (const char*)&v, sizeof(v)) != 0) {
        int code = WSAGetLastError();
        (void)code;
        log_code_at(logger, EBISU_LOG_WARNING, (unsigned long)code, "failed to set %s to %u",
                what, value);
    }
}

/* Says an option was asked for that the headers have no way to set. */
#define unsettable(logger, what) \
    log_to((logger), EBISU_LOG_WARNING, "%s can't be set on this system.", (what))

/* Sets options on a socket just connected. Windows takes TCP_KEEPIDLE and
   TCP_KEEPINTVL from Windows 10 1709, and TCP_KEEPCNT from 1703. */
static void set_tcp_options(SOCKET sock, const tcp_options_t* options, ebisu_logger_t* logger)
{
    if (options->idle_s > 0) {
        set_dword_option(sock, SOL_SOCKET, SO_KEEPALIVE, "SO_KEEPALIVE", 1, logger);
#ifdef TCP_KEEPIDLE
        set_dword_option(sock, IPPROTO_TCP, TCP_KEEPIDLE, "TCP_KEEPIDLE", options->idle_s,
                logger);
#else
        unsettable(logger, "TCP_KEEPIDLE");
#endif
        if (options->interval_s > 0) {
#ifdef TCP_KEEPINTVL
            set_dword_option(sock, IPPROTO_TCP, TCP_KEEPINTVL, "TCP_KEEPINTVL",
                    options->interval_s, logger);
#else
            unsettable(logger, "TCP_KEEPINTVL");
#endif
        }
        if (options->count > 0) {
#ifdef TCP_KEEPCNT
            set_dword_option(sock, IPPROTO_TCP, TCP_KEEPCNT, "TCP_KEEPCNT", options->count,
                    logger);
#else
            unsettable(logger, "TCP_KEEPCNT");
#endif
        }
    }
    if (options->nodelay) {
        set_dword_option(sock, IPPROTO_TCP, TCP_NODELAY, "TCP_NODELAY", 1, logger);
    }
}

#endif /* _EBISU_WIN32_CONNECT */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
