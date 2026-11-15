#include <catch.hpp>
#include <ebisu_log.h>
#include <ebisu_object.h>
#include <stdio.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
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

namespace {

// A lock of the test's own, as an application has one of its system's.
struct TestLock {
    ebisu_lock_t base;
    std::mutex mutex;
    TestLock();
};

void test_lock(ebisu_lock_t* self)
{
    EBISU_CONTAINER_OF(self, TestLock, base)->mutex.lock();
}

void test_unlock(ebisu_lock_t* self)
{
    EBISU_CONTAINER_OF(self, TestLock, base)->mutex.unlock();
}

const ebisu_lock_ops_t test_lock_ops = { test_lock, test_unlock };

TestLock::TestLock() : base{ &test_lock_ops } {}

// Counts what it is given, from any task, and takes its time over it if told
// to, saying when it has started.
struct SlowLogger {
    ebisu_logger_t base;
    std::atomic<int> count;
    std::atomic<bool> started;
    std::atomic<bool> done;
    int sleep_ms;
    explicit SlowLogger(int sleep_ms = 0);
};

void slow_log(ebisu_logger_t* self, ebisu_log_level_t, const char*, va_list)
{
    SlowLogger* logger = EBISU_CONTAINER_OF(self, SlowLogger, base);
    logger->started = true;
    if (logger->sleep_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(logger->sleep_ms));
    }
    ++logger->count;
    logger->done = true;
}

const ebisu_logger_ops_t slow_ops = { slow_log };

SlowLogger::SlowLogger(int sleep_ms)
    : base{ &slow_ops }, count(0), started(false), done(false), sleep_ms(sleep_ms) {}

} // namespace

TEST_CASE("Level names are read as a configuration has them") {
    const std::vector<std::pair<const char*, ebisu_log_level_t>> names = {
        { "error", EBISU_LOG_ERROR }, { "ERROR", EBISU_LOG_ERROR }, { "Error", EBISU_LOG_ERROR },
        { "warn", EBISU_LOG_WARNING }, { "WARN", EBISU_LOG_WARNING },
        { "warning", EBISU_LOG_WARNING }, { "Warning", EBISU_LOG_WARNING },
        { "info", EBISU_LOG_INFO }, { "INFO", EBISU_LOG_INFO },
        { "debug", EBISU_LOG_DEBUG }, { "DeBuG", EBISU_LOG_DEBUG },
        { "trace", EBISU_LOG_DEBUG }, { "TRACE", EBISU_LOG_DEBUG },
    };
    for (const auto& n : names) {
        INFO(n.first);
        // Another level than the one expected, as an enum can't hold anything
        // else.
        ebisu_log_level_t level = n.second == EBISU_LOG_ERROR ? EBISU_LOG_DEBUG : EBISU_LOG_ERROR;
        CHECK(ebisu_log_level_parse(n.first, &level));
        CHECK(level == n.second);
    }
    for (const char* name : { "", " error", "error ", "err", "errors", "fatal", "warnings",
            "inf", "debugging", "trac", "error\n", "WARN-", "1" }) {
        INFO("\"" << name << "\"");
        ebisu_log_level_t level = EBISU_LOG_INFO;
        CHECK_FALSE(ebisu_log_level_parse(name, &level));
        CHECK(level == EBISU_LOG_INFO);
    }
    ebisu_log_level_t level = EBISU_LOG_INFO;
    CHECK_FALSE(ebisu_log_level_parse(nullptr, &level));
    CHECK(level == EBISU_LOG_INFO);
}

TEST_CASE("Level names are written as they are read") {
    CHECK(std::string(ebisu_log_level_name(EBISU_LOG_ERROR)) == "error");
    CHECK(std::string(ebisu_log_level_name(EBISU_LOG_WARNING)) == "warning");
    CHECK(std::string(ebisu_log_level_name(EBISU_LOG_INFO)) == "info");
    CHECK(std::string(ebisu_log_level_name(EBISU_LOG_DEBUG)) == "debug");
    for (ebisu_log_level_t level : { EBISU_LOG_ERROR, EBISU_LOG_WARNING, EBISU_LOG_INFO,
            EBISU_LOG_DEBUG }) {
        ebisu_log_level_t back = level == EBISU_LOG_ERROR ? EBISU_LOG_DEBUG : EBISU_LOG_ERROR;
        CHECK(ebisu_log_level_parse(ebisu_log_level_name(level), &back));
        CHECK(back == level);
    }
}

TEST_CASE("A level filter gives its logger the messages of its level, which can change") {
    TestLock lock;
    RecordingLogger target;
    ebisu_log_level_filter_t filter;
    ebisu_lock_t* locks[] = { &lock.base, nullptr };
    for (ebisu_lock_t* with : locks) {
        INFO((with != nullptr ? "with" : "without") << " a lock");
        target.lines.clear();
        ebisu_log_level_filter_init(&filter, &target.base, EBISU_LOG_WARNING, with);
        CHECK(ebisu_log_level_filter_level(&filter) == EBISU_LOG_WARNING);
        ebisu_log(&filter.base, EBISU_LOG_ERROR, "%s %d", "error", 1);
        ebisu_log(&filter.base, EBISU_LOG_WARNING, "%s %d", "warning", 2);
        ebisu_log(&filter.base, EBISU_LOG_INFO, "%s %d", "info", 3);
        ebisu_log(&filter.base, EBISU_LOG_DEBUG, "%s %d", "debug", 4);

        ebisu_log_level_filter_set_level(&filter, EBISU_LOG_DEBUG);
        CHECK(ebisu_log_level_filter_level(&filter) == EBISU_LOG_DEBUG);
        ebisu_log(&filter.base, EBISU_LOG_DEBUG, "%s %d", "debug", 5);

        ebisu_log_level_filter_set_level(&filter, EBISU_LOG_ERROR);
        ebisu_log(&filter.base, EBISU_LOG_WARNING, "%s %d", "warning", 6);
        ebisu_log(&filter.base, EBISU_LOG_ERROR, "%s %d", "error", 7);
        CHECK(target.lines == Lines({
                { EBISU_LOG_ERROR, "error 1" }, { EBISU_LOG_WARNING, "warning 2" },
                { EBISU_LOG_DEBUG, "debug 5" }, { EBISU_LOG_ERROR, "error 7" } }));
    }
}

TEST_CASE("A level filter's level is set while other tasks log through it") {
    TestLock lock;
    SlowLogger target;
    ebisu_log_level_filter_t filter;
    ebisu_log_level_filter_init(&filter, &target.base, EBISU_LOG_INFO, &lock.base);
    std::atomic<bool> stop(false);
    std::vector<std::thread> tasks;
    for (int t = 0; t < 4; ++t) {
        tasks.emplace_back([&filter, &stop] {
            while (!stop) {
                ebisu_log(&filter.base, EBISU_LOG_INFO, "%s", "info");
            }
        });
    }
    for (int i = 0; i < 1000; ++i) {
        ebisu_log_level_filter_set_level(&filter, i % 2 == 0 ? EBISU_LOG_ERROR : EBISU_LOG_DEBUG);
    }
    ebisu_log_level_filter_set_level(&filter, EBISU_LOG_ERROR);
    // Once it is error, nothing at info goes on, but for messages that read the
    // level before, which may take their time, under valgrind above all.
    int before = target.count;
    for (int i = 0; i < 100; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int now = target.count;
        if (now == before) {
            break;
        }
        before = now;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;
    for (auto& task : tasks) {
        task.join();
    }
    CHECK(target.count == before);
}

TEST_CASE("A slot gives the logger it holds the messages, and none while empty") {
    TestLock lock;
    RecordingLogger first, second;
    ebisu_log_slot_t slot;
    ebisu_log_slot_init(&slot, &lock.base);
    ebisu_log(&slot.base, EBISU_LOG_ERROR, "%s", "nowhere");

    CHECK(ebisu_log_slot_set(&slot, &first.base) == nullptr);
    ebisu_log(&slot.base, EBISU_LOG_INFO, "%s %d", "to the first", 1);
    CHECK(ebisu_log_slot_set(&slot, &second.base) == &first.base);
    ebisu_log(&slot.base, EBISU_LOG_DEBUG, "%s %d", "to the second", 2);
    CHECK(ebisu_log_slot_set(&slot, nullptr) == &second.base);
    ebisu_log(&slot.base, EBISU_LOG_ERROR, "%s", "nowhere again");

    CHECK(first.lines == Lines({ { EBISU_LOG_INFO, "to the first 1" } }));
    CHECK(second.lines == Lines({ { EBISU_LOG_DEBUG, "to the second 2" } }));
}

// The old logger may be closed once the change returns: a message going to it
// is done, and no other starts.
TEST_CASE("A slot's change returns once no message goes to the logger it held") {
    TestLock lock;
    SlowLogger slow(200), next;
    ebisu_log_slot_t slot;
    ebisu_log_slot_init(&slot, &lock.base);
    ebisu_log_slot_set(&slot, &slow.base);

    std::thread task([&slot] { ebisu_log(&slot.base, EBISU_LOG_INFO, "%s", "slow"); });
    while (!slow.started) {
        std::this_thread::yield();
    }
    CHECK(ebisu_log_slot_set(&slot, &next.base) == &slow.base);
    CHECK(slow.done);
    task.join();

    std::atomic<bool> stop(false);
    std::vector<std::thread> tasks;
    for (int t = 0; t < 4; ++t) {
        tasks.emplace_back([&slot, &stop] {
            while (!stop) {
                ebisu_log(&slot.base, EBISU_LOG_INFO, "%s", "busy");
            }
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    SlowLogger last;
    CHECK(ebisu_log_slot_set(&slot, &last.base) == &next.base);
    int after = next.count;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    stop = true;
    for (auto& t : tasks) {
        t.join();
    }
    CHECK(next.count == after);
    CHECK(slow.count == 1);
    CHECK(last.count > 0);
}
