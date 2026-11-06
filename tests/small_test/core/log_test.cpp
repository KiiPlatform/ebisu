#include <catch.hpp>
#include <ebisu_log.h>
#include <ebisu_object.h>
#include <stdio.h>
#include <string>
#include <vector>

namespace {

// Formats what it is given, so that a logger that read its arguments from
// what another had read would get something else.
struct RecordingLogger {
    ebisu_logger_t base;
    std::vector<std::pair<ebisu_log_level_t, std::string>> lines;
    RecordingLogger();
};

void record(ebisu_logger_t* self, ebisu_log_level_t level, const char* fmt, va_list args)
{
    RecordingLogger* logger = EBISU_CONTAINER_OF(self, RecordingLogger, base);
    char line[256];
    vsnprintf(line, sizeof(line), fmt, args);
    logger->lines.emplace_back(level, line);
}

const ebisu_logger_ops_t recording_ops = { record };

RecordingLogger::RecordingLogger() : base{ &recording_ops } {}

typedef std::vector<std::pair<ebisu_log_level_t, std::string>> Lines;

} // namespace

TEST_CASE("ebisu_log() logs with the logger's log") {
    RecordingLogger logger;
    ebisu_log(&logger.base, EBISU_LOG_WARNING, "%s took %d tries, %ld ms", "connecting", 3,
            1500L);
    CHECK(logger.lines == Lines{ { EBISU_LOG_WARNING, "connecting took 3 tries, 1500 ms" } });

    SECTION("or drops the message without one") {
        ebisu_log(nullptr, EBISU_LOG_ERROR, "%s", "nowhere");
    }
}

TEST_CASE("A fan-out logger gives each message to each of its loggers, in order") {
    RecordingLogger first, second, third;
    ebisu_logger_t* loggers[] = { &first.base, nullptr, &second.base, &third.base };
    ebisu_log_fanout_t fanout;
    ebisu_log_fanout_init(&fanout, loggers, 4);

    // More arguments than registers carry, so that the ones read last come
    // from the stack, which a logger reading another's va_list would pass.
    ebisu_log(&fanout.base, EBISU_LOG_ERROR, "%s %d %d %d %d %d %d %d %s", "a", 1, 2, 3, 4, 5,
            6, 7, "z");
    ebisu_log(&fanout.base, EBISU_LOG_DEBUG, "resumed the TLS session with %s.", "example.com");
    const Lines expected = {
        { EBISU_LOG_ERROR, "a 1 2 3 4 5 6 7 z" },
        { EBISU_LOG_DEBUG, "resumed the TLS session with example.com." },
    };
    CHECK(first.lines == expected);
    CHECK(second.lines == expected);
    CHECK(third.lines == expected);

    SECTION("through another fan-out logger") {
        RecordingLogger outer;
        ebisu_logger_t* outer_loggers[] = { &fanout.base, &outer.base };
        ebisu_log_fanout_t outer_fanout;
        ebisu_log_fanout_init(&outer_fanout, outer_loggers, 2);
        ebisu_log(&outer_fanout.base, EBISU_LOG_INFO, "%d %s", 42, "nested");
        CHECK(first.lines.back() == std::make_pair(EBISU_LOG_INFO, std::string("42 nested")));
        CHECK(third.lines.back() == first.lines.back());
        CHECK(outer.lines == Lines{ { EBISU_LOG_INFO, "42 nested" } });
    }

    SECTION("or to none") {
        ebisu_log_fanout_t empty;
        ebisu_log_fanout_init(&empty, loggers, 0);
        ebisu_log(&empty.base, EBISU_LOG_ERROR, "%s", "dropped");
        CHECK(first.lines.size() == 2);
    }
}
