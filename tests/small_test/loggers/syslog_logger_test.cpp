#include <catch.hpp>
#include <ebisu_loggers.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <utility>
#include <vector>

// syslog() is the test's: the program's own definition is the one the
// logger's call links to, before the C library's. Without syslog.h, whose
// fortified syslog() is an inline function of its own, which calls
// __syslog_chk().
namespace {

std::vector<std::pair<int, std::string>> sent;

void record(int priority, const char* format, va_list args)
{
    char message[4 * EBISU_LOGGER_LINE_MAX];
    vsnprintf(message, sizeof(message), format, args);
    sent.emplace_back(priority, message);
}

// syslog.h's values, which are RFC 5424's.
const int LOG_ERR_ = 3, LOG_WARNING_ = 4, LOG_INFO_ = 6, LOG_DEBUG_ = 7;
const int LOG_LOCAL3_ = 19 << 3;

typedef std::vector<std::pair<int, std::string>> Sent;

} // namespace

extern "C" void syslog(int priority, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    record(priority, format, args);
    va_end(args);
}

#ifdef __GLIBC__
extern "C" void __syslog_chk(int priority, int flag, const char* format, ...)
{
    (void)flag;
    va_list args;
    va_start(args, format);
    record(priority, format, args);
    va_end(args);
}
#endif

TEST_CASE("The syslog logger sends each message as important as its level to syslog()") {
    sent.clear();
    ebisu_syslog_logger_t logger;
    ebisu_syslog_logger_init(&logger, LOG_LOCAL3_, EBISU_LOG_INFO);

    ebisu_log(&logger.base, EBISU_LOG_ERROR, "failed to connect to %s.", "example.com");
    ebisu_log(&logger.base, EBISU_LOG_WARNING, "%d tries", 3);
    ebisu_log(&logger.base, EBISU_LOG_INFO, "connected");
    ebisu_log(&logger.base, EBISU_LOG_DEBUG, "dropped");
    ebisu_log(&logger.base, (ebisu_log_level_t)-1, "dropped");
    CHECK(sent == Sent{
        { LOG_LOCAL3_ | LOG_ERR_, "failed to connect to example.com." },
        { LOG_LOCAL3_ | LOG_WARNING_, "3 tries" },
        { LOG_LOCAL3_ | LOG_INFO_, "connected" },
    });

    SECTION("with openlog()'s facility") {
        sent.clear();
        ebisu_syslog_logger_init(&logger, 0, EBISU_LOG_DEBUG);
        ebisu_log(&logger.base, EBISU_LOG_DEBUG, "resumed");
        CHECK(sent == Sent{ { LOG_DEBUG_, "resumed" } });
    }

    // The message is syslog()'s argument, not its format, which a percent
    // sign in it would otherwise be taken for.
    SECTION("as it is") {
        sent.clear();
        ebisu_log(&logger.base, EBISU_LOG_ERROR, "%s", "100%s %n sure");
        CHECK(sent == Sent{ { LOG_LOCAL3_ | LOG_ERR_, "100%s %n sure" } });
    }

    SECTION("cutting a long one short") {
        sent.clear();
        const std::string message(2 * EBISU_LOGGER_LINE_MAX, 'x');
        ebisu_log(&logger.base, EBISU_LOG_ERROR, "%s", message.c_str());
        REQUIRE(sent.size() == 1);
        CHECK(sent[0].second == std::string(EBISU_LOGGER_LINE_MAX - 3, 'x') + "...");
    }
}
