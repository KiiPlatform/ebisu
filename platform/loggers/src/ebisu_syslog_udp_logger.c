#include "ebisu_loggers.h"
#include "ebisu_loggers_format.h"

#include <ebisu_object.h>
#include <ebisu_time.h>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET socket_t;
#define BAD_SOCKET INVALID_SOCKET
#else
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
typedef int socket_t;
#define BAD_SOCKET (-1)
#endif

/* The longest host name RFC 5424 has a message carry. */
#define HOSTNAME_MAX 255

/* Whether s is 1 to max characters of printable US-ASCII without spaces, as
   the fields of RFC 5424's header are. */
static bool is_header_field(const char* s, size_t max)
{
    size_t i;
    for (i = 0; s[i] != '\0'; ++i) {
        if (i == max || (unsigned char)s[i] < 33 || (unsigned char)s[i] > 126) {
            return false;
        }
    }
    return i > 0;
}

/* The system's time of day, as RFC 5424 has a timestamp: UTC, to the
   millisecond. "-", RFC 5424's for a value unknown, if it has none. */
static void get_timestamp(char* out, size_t size)
{
    int64_t seconds;
    long milliseconds;
    ebisu_civil_time_t civil;
#ifdef _WIN32
    FILETIME now;
    ULARGE_INTEGER intervals;
    GetSystemTimeAsFileTime(&now);
    /* 100-nanosecond intervals since 1601-01-01T00:00:00Z. */
    intervals.LowPart = now.dwLowDateTime;
    intervals.HighPart = now.dwHighDateTime;
    seconds = (int64_t)(intervals.QuadPart / 10000000u) - 11644473600;
    milliseconds = (long)(intervals.QuadPart / 10000u % 1000u);
#else
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        snprintf(out, size, "-");
        return;
    }
    seconds = (int64_t)now.tv_sec;
    milliseconds = now.tv_nsec / 1000000;
#endif
    if (!ebisu_time_to_civil(seconds, &civil)) {
        snprintf(out, size, "-");
        return;
    }
    snprintf(out, size, "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ", civil.year, civil.month,
            civil.day, civil.hour, civil.minute, civil.second, milliseconds);
}

static void close_socket(socket_t sock)
{
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

/* Non-blocking, so that a send never waits, and on POSIX not inherited by a
   program the application executes. */
static bool configure_socket(socket_t sock)
{
#ifdef _WIN32
    u_long non_blocking = 1;
    return ioctlsocket(sock, FIONBIO, &non_blocking) == 0;
#else
    int flags = fcntl(sock, F_GETFL);
    if (flags == -1 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        return false;
    }
    return fcntl(sock, F_SETFD, FD_CLOEXEC) != -1;
#endif
}

static void syslog_udp_log(
        ebisu_logger_t* self,
        ebisu_log_level_t level,
        const char* fmt,
        va_list args)
{
    ebisu_syslog_udp_logger_t* logger =
        EBISU_CONTAINER_OF(self, ebisu_syslog_udp_logger_t, base);
    char datagram[EBISU_LOGGER_LINE_MAX + 1];
    /* Room for any int and long the format could be given, as GCC checks for,
       though a timestamp takes 24 bytes. */
    char timestamp[96];
    int header;
    size_t length;
    if (!logger->_open || _EBISU_LOGGERS_DROPS(logger->_level, level)) {
        return;
    }
    get_timestamp(timestamp, sizeof(timestamp));
    header = snprintf(datagram, sizeof(datagram), "<%d>1 %s%s",
            logger->_facility * 8 + _ebisu_loggers_severities[level], timestamp,
            logger->_fields);
    if (header < 0) {
        return;
    }
    length = _ebisu_loggers_format(datagram, sizeof(datagram), (size_t)header, fmt, args);
    /* A datagram the socket can't take, or the server refused, is dropped. */
#ifdef _WIN32
    (void)send((socket_t)logger->_socket, datagram, (int)length, 0);
#else
    (void)send((socket_t)logger->_socket, datagram, length, 0);
#endif
}

static const ebisu_logger_ops_t syslog_udp_logger_ops = {
    syslog_udp_log
};

bool ebisu_syslog_udp_logger_init(
        ebisu_syslog_udp_logger_t* logger,
        const char* host,
        uint16_t port,
        const char* app_name,
        int facility,
        ebisu_log_level_t level)
{
    char hostname[HOSTNAME_MAX + 1];
    char service[6];
    unsigned long pid;
    struct addrinfo hints;
    struct addrinfo* addrs;
    struct addrinfo* addr;
#ifdef _WIN32
    WSADATA wsa;
#endif

    logger->base.ops = &syslog_udp_logger_ops;
    logger->_socket = 0;
    logger->_open = false;
    logger->_facility = facility;
    logger->_level = level;
    logger->_fields[0] = '\0';
    if (host == NULL || facility < 0 || facility > 23) {
        return false;
    }
    if (app_name == NULL) {
        app_name = "-";
    } else if (!is_header_field(app_name, EBISU_SYSLOG_APP_NAME_MAX)) {
        return false;
    }

#ifdef _WIN32
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
    pid = (unsigned long)GetCurrentProcessId();
    if (gethostname(hostname, (int)sizeof(hostname)) != 0) {
        hostname[0] = '\0';
    }
#else
    pid = (unsigned long)getpid();
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        hostname[0] = '\0';
    }
#endif
    /* gethostname() needn't terminate a name it cut short. */
    hostname[HOSTNAME_MAX] = '\0';
    if (!is_header_field(hostname, HOSTNAME_MAX)) {
        snprintf(hostname, sizeof(hostname), "-");
    }
    snprintf(logger->_fields, sizeof(logger->_fields), " %s %s %lu - - ", hostname,
            app_name, pid);

    snprintf(service, sizeof(service), "%u", (unsigned)port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, service, &hints, &addrs) == 0) {
        /* Connected, so that a send needs no address, and the system picks
           the address to send from. */
        for (addr = addrs; addr != NULL && !logger->_open; addr = addr->ai_next) {
            socket_t sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
            if (sock == BAD_SOCKET) {
                continue;
            }
            if (connect(sock, addr->ai_addr, (socklen_t)addr->ai_addrlen) == 0 &&
                    configure_socket(sock)) {
                logger->_socket = (uintptr_t)sock;
                logger->_open = true;
            } else {
                close_socket(sock);
            }
        }
        freeaddrinfo(addrs);
    }
#ifdef _WIN32
    if (!logger->_open) {
        WSACleanup();
    }
#endif
    return logger->_open;
}

void ebisu_syslog_udp_logger_close(ebisu_syslog_udp_logger_t* logger)
{
    if (!logger->_open) {
        return;
    }
    close_socket((socket_t)logger->_socket);
    logger->_open = false;
#ifdef _WIN32
    WSACleanup();
#endif
}
