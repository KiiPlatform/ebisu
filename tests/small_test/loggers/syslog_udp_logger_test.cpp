#include <catch.hpp>
#include <ebisu_loggers.h>
#include <ebisu_time.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <regex>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET sock_t;
static const sock_t no_socket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int sock_t;
static const sock_t no_socket = -1;
#endif

namespace {

// A syslog server on the loopback, on a port the system picks.
struct Receiver {
    sock_t sock;
    uint16_t port;
    bool listening;

    explicit Receiver(int family = AF_INET) : sock(no_socket), port(0), listening(false) {
#ifdef _WIN32
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
#endif
        sockaddr_storage addr;
        memset(&addr, 0, sizeof(addr));
        socklen_t length;
        if (family == AF_INET) {
            sockaddr_in* in = (sockaddr_in*)&addr;
            in->sin_family = AF_INET;
            in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            length = sizeof(*in);
        } else {
            sockaddr_in6* in6 = (sockaddr_in6*)&addr;
            in6->sin6_family = AF_INET6;
            in6->sin6_addr = in6addr_loopback;
            length = sizeof(*in6);
        }
        sock = socket(family, SOCK_DGRAM, 0);
        if (sock == no_socket || bind(sock, (sockaddr*)&addr, length) != 0 ||
                getsockname(sock, (sockaddr*)&addr, &length) != 0) {
            return;
        }
        port = ntohs(family == AF_INET ? ((sockaddr_in*)&addr)->sin_port
                                       : ((sockaddr_in6*)&addr)->sin6_port);
        listening = true;
    }

    ~Receiver() {
        if (sock != no_socket) {
#ifdef _WIN32
            closesocket(sock);
#else
            close(sock);
#endif
        }
#ifdef _WIN32
        WSACleanup();
#endif
    }

    // The next datagram, or "" if none comes in time.
    std::string next(int timeout_ms = 2000) {
#ifdef _WIN32
        DWORD timeout = (DWORD)timeout_ms;
#else
        timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
#endif
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        char datagram[4 * EBISU_LOGGER_LINE_MAX];
        int received = (int)recv(sock, datagram, sizeof(datagram), 0);
        return received > 0 ? std::string(datagram, (size_t)received) : std::string();
    }
};

// What the logger names the host, as RFC 5424 has it.
std::string hostname() {
    char name[256];
    if (gethostname(name, sizeof(name)) != 0) {
        return "-";
    }
    name[255] = '\0';
    std::string out(name);
    for (char c : out) {
        if ((unsigned char)c < 33 || (unsigned char)c > 126) {
            return "-";
        }
    }
    return out.empty() ? "-" : out;
}

unsigned long pid() {
#ifdef _WIN32
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

// A datagram's fields, as RFC 5424 has them.
struct Message {
    int priority = -1;
    std::string timestamp, hostname, app_name, procid, msg;
    int64_t seconds = 0;
};

Message parse(const std::string& datagram) {
    static const std::regex form(
            "<([0-9]{1,3})>1 "
            "(([0-9]{4})-([0-9]{2})-([0-9]{2})T([0-9]{2}):([0-9]{2}):([0-9]{2})\\.[0-9]{3}Z) "
            "([!-~]+) ([!-~]+) ([0-9]+) - - ([\\s\\S]*)");
    Message out;
    std::smatch m;
    if (!std::regex_match(datagram, m, form)) {
        return out;
    }
    out.priority = std::stoi(m[1]);
    out.timestamp = m[2];
    ebisu_time_from_civil(std::stoi(m[3]), std::stoi(m[4]), std::stoi(m[5]), std::stoi(m[6]),
            std::stoi(m[7]), std::stoi(m[8]), &out.seconds);
    out.hostname = m[9];
    out.app_name = m[10];
    out.procid = m[11];
    out.msg = m[12];
    return out;
}

} // namespace

TEST_CASE("The UDP syslog logger sends each message as important as its level as RFC 5424 has it") {
    Receiver server;
    REQUIRE(server.listening);
    ebisu_syslog_udp_logger_t logger;
    REQUIRE(ebisu_syslog_udp_logger_init(&logger, "127.0.0.1", server.port, "ebisu-test", 1,
            EBISU_LOG_INFO));
#ifndef _WIN32
    // Sending never waits, and a program the application executes doesn't
    // get the socket.
    CHECK((fcntl((int)logger._socket, F_GETFL) & O_NONBLOCK) != 0);
    CHECK((fcntl((int)logger._socket, F_GETFD) & FD_CLOEXEC) != 0);
#endif

    ebisu_log(&logger.base, EBISU_LOG_ERROR, "failed to connect to %s.", "example.com");
    const time_t now = time(nullptr);
    std::string datagram = server.next();
    INFO(datagram);
    Message message = parse(datagram);
    // User-level messages, 1, and an error, 3.
    CHECK(message.priority == 1 * 8 + 3);
    CHECK(message.seconds >= (int64_t)now - 5);
    CHECK(message.seconds <= (int64_t)now + 5);
    CHECK(message.hostname == hostname());
    CHECK(message.app_name == "ebisu-test");
    CHECK(message.procid == std::to_string(pid()));
    CHECK(message.msg == "failed to connect to example.com.");

    SECTION("at each level") {
        ebisu_log(&logger.base, EBISU_LOG_WARNING, "%d tries", 3);
        ebisu_log(&logger.base, EBISU_LOG_INFO, "connected");
        ebisu_log(&logger.base, EBISU_LOG_DEBUG, "dropped");
        ebisu_log(&logger.base, (ebisu_log_level_t)-1, "dropped");
        message = parse(server.next());
        CHECK(message.priority == 1 * 8 + 4);
        CHECK(message.msg == "3 tries");
        message = parse(server.next());
        CHECK(message.priority == 1 * 8 + 6);
        CHECK(message.msg == "connected");
        CHECK(server.next(300) == "");
    }

    SECTION("cutting a long one short") {
        const std::string long_message(2 * EBISU_LOGGER_LINE_MAX, 'x');
        ebisu_log(&logger.base, EBISU_LOG_ERROR, "%s", long_message.c_str());
        datagram = server.next();
        CHECK(datagram.size() == EBISU_LOGGER_LINE_MAX);
        message = parse(datagram);
        CHECK(message.msg.size() > 3);
        CHECK(message.msg == std::string(message.msg.size() - 3, 'x') + "...");
    }

    SECTION("until closed") {
        ebisu_syslog_udp_logger_close(&logger);
        // Not the socket, whose descriptor may be another's by then.
        CHECK_FALSE(logger._open);
        ebisu_log(&logger.base, EBISU_LOG_ERROR, "dropped");
        CHECK(server.next(300) == "");
        ebisu_syslog_udp_logger_close(&logger);
    }

    ebisu_syslog_udp_logger_close(&logger);
}

TEST_CASE("The UDP syslog logger takes any facility, and an application without a name") {
    Receiver server;
    REQUIRE(server.listening);
    ebisu_syslog_udp_logger_t logger;
    REQUIRE(ebisu_syslog_udp_logger_init(&logger, "127.0.0.1", server.port, nullptr, 23,
            EBISU_LOG_DEBUG));
    ebisu_log(&logger.base, EBISU_LOG_DEBUG, "resumed");
    Message message = parse(server.next());
    // Local use 7, 23, and debug, 7: the largest priority there is.
    CHECK(message.priority == 191);
    CHECK(message.app_name == "-");
    CHECK(message.msg == "resumed");
    ebisu_syslog_udp_logger_close(&logger);

    SECTION("or the longest name") {
        const std::string name(EBISU_SYSLOG_APP_NAME_MAX, 'n');
        REQUIRE(ebisu_syslog_udp_logger_init(&logger, "127.0.0.1", server.port, name.c_str(), 0,
                EBISU_LOG_DEBUG));
        ebisu_log(&logger.base, EBISU_LOG_ERROR, "kernel");
        message = parse(server.next());
        CHECK(message.priority == 3);
        CHECK(message.app_name == name);
        ebisu_syslog_udp_logger_close(&logger);
    }
}

TEST_CASE("The UDP syslog logger sends to an IPv6 server") {
    Receiver server(AF_INET6);
    if (!server.listening) {
        WARN("No IPv6 loopback here.");
        return;
    }
    ebisu_syslog_udp_logger_t logger;
    REQUIRE(ebisu_syslog_udp_logger_init(&logger, "::1", server.port, "ebisu-test", 1,
            EBISU_LOG_DEBUG));
    ebisu_log(&logger.base, EBISU_LOG_INFO, "over IPv6");
    CHECK(parse(server.next()).msg == "over IPv6");
    ebisu_syslog_udp_logger_close(&logger);
}

TEST_CASE("The UDP syslog logger refuses what RFC 5424 can't carry, and then drops everything") {
    Receiver server;
    REQUIRE(server.listening);
    ebisu_syslog_udp_logger_t logger;
    const std::string too_long(EBISU_SYSLOG_APP_NAME_MAX + 1, 'n');
    const char* host = "127.0.0.1";
    const char* app_name = "ebisu-test";
    int facility = 1;

    SECTION("no host") {
        host = nullptr;
    }
    SECTION("a facility below 0") {
        facility = -1;
    }
    SECTION("a facility above 23") {
        facility = 24;
    }
    SECTION("an empty name") {
        app_name = "";
    }
    SECTION("a name with a space") {
        app_name = "ebisu test";
    }
    SECTION("a name with a control character") {
        app_name = "ebisu\x7f";
    }
    SECTION("a name too long") {
        app_name = too_long.c_str();
    }

    CHECK_FALSE(ebisu_syslog_udp_logger_init(&logger, host, server.port, app_name, facility,
            EBISU_LOG_DEBUG));
    ebisu_log(&logger.base, EBISU_LOG_ERROR, "dropped");
    CHECK(server.next(300) == "");
    ebisu_syslog_udp_logger_close(&logger);
}
