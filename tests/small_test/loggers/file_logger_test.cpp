// platform's logger to a file it keeps under a size, which writes in the
// working directory, the build tree, and is read back through streams of the
// test's own.
#include <catch.hpp>
#include <ebisu_loggers.h>
#include <ebisu_object.h>

#ifdef _WIN32
#include <direct.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <time.h>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

const char* const PATH = "file_logger_test.log";
const char* const BACKUP = "file_logger_test.log.1";

// What a file has, or "none" without the file.
std::string contents(const char* path) {
    FILE* in = fopen(path, "rb");
    if (in == nullptr) {
        return "none";
    }
    std::string all;
    int c;
    while ((c = fgetc(in)) != EOF) {
        all += (char)c;
    }
    fclose(in);
    return all;
}

void write_file(const char* path, const std::string& text) {
    FILE* out = fopen(path, "wb");
    REQUIRE(out != nullptr);
    REQUIRE(fwrite(text.data(), 1, text.size(), out) == text.size());
    fclose(out);
}

// Neither file, before and after each test.
struct Files {
    Files() { clear(); }
    ~Files() { clear(); }
    static void clear() {
        remove(PATH);
        remove(BACKUP);
    }
};

// A lock of the test's own, as an application has one of its system's.
struct TestLock {
    ebisu_lock_t base;
    std::mutex mutex;
    TestLock();
};

void test_lock(ebisu_lock_t* self) {
    EBISU_CONTAINER_OF(self, TestLock, base)->mutex.lock();
}

void test_unlock(ebisu_lock_t* self) {
    EBISU_CONTAINER_OF(self, TestLock, base)->mutex.unlock();
}

const ebisu_lock_ops_t test_lock_ops = { test_lock, test_unlock };

TestLock::TestLock() : base{ &test_lock_ops } {}

std::string numbered(int i) {
    char line[32];
    snprintf(line, sizeof(line), "info: line %02d\n", i);
    return line;
}

} // namespace

TEST_CASE("The file logger writes a line for each message as important as its level") {
    Files files;
    ebisu_file_logger_t logger;
    ebisu_logger_init_error_t error = EBISU_LOGGER_INIT_ERROR_OPEN;
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 0, EBISU_LOG_WARNING, nullptr, &error));
    CHECK(error == EBISU_LOGGER_INIT_ERROR_NONE);
    ebisu_log(&logger.base, EBISU_LOG_ERROR, "failed to connect to %s.", "example.com");
    ebisu_log(&logger.base, EBISU_LOG_WARNING, "%d tries", 3);
    ebisu_log(&logger.base, EBISU_LOG_INFO, "dropped");
    // Flushed with each line: read while the logger has it open.
    CHECK(contents(PATH) == "error: failed to connect to example.com.\nwarning: 3 tries\n");
    ebisu_file_logger_close(&logger);
    ebisu_log(&logger.base, EBISU_LOG_ERROR, "after it closed");
    ebisu_file_logger_close(&logger);
    CHECK(contents(PATH) == "error: failed to connect to example.com.\nwarning: 3 tries\n");
    CHECK(contents(BACKUP) == "none");
}

// Each line is 14 bytes, and 7 of them fit in 100.
TEST_CASE("The file logger keeps its file under its size, and what came before in a backup") {
    Files files;
    ebisu_file_logger_t logger;
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 100, EBISU_LOG_DEBUG, nullptr, nullptr));
    std::string expected[3];
    for (int i = 1; i <= 20; ++i) {
        ebisu_log(&logger.base, EBISU_LOG_INFO, "line %02d", i);
        expected[(i - 1) / 7] += numbered(i);
    }
    ebisu_file_logger_close(&logger);
    CHECK(contents(BACKUP) == expected[1]);
    CHECK(contents(PATH) == expected[2]);
}

TEST_CASE("The file logger adds to a file there is, counting what it has") {
    Files files;
    const std::string before(90, 'x');
    write_file(PATH, before);
    ebisu_file_logger_t logger;
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 100, EBISU_LOG_DEBUG, nullptr, nullptr));
    ebisu_log(&logger.base, EBISU_LOG_INFO, "line %02d", 1);
    ebisu_file_logger_close(&logger);
    CHECK(contents(BACKUP) == before);
    CHECK(contents(PATH) == numbered(1));

    SECTION("and one that fits, to it") {
        write_file(PATH, std::string(10, 'y'));
        REQUIRE(ebisu_file_logger_init(&logger, PATH, 100, EBISU_LOG_DEBUG, nullptr, nullptr));
        ebisu_log(&logger.base, EBISU_LOG_INFO, "line %02d", 2);
        ebisu_file_logger_close(&logger);
        CHECK(contents(PATH) == std::string(10, 'y') + numbered(2));
        CHECK(contents(BACKUP) == before);
    }
}

TEST_CASE("The file logger writes a line longer than its size in a file of its own") {
    Files files;
    ebisu_file_logger_t logger;
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 10, EBISU_LOG_DEBUG, nullptr, nullptr));
    ebisu_log(&logger.base, EBISU_LOG_INFO, "line %02d", 1);
    CHECK(contents(PATH) == numbered(1));
    ebisu_log(&logger.base, EBISU_LOG_INFO, "line %02d", 2);
    ebisu_file_logger_close(&logger);
    CHECK(contents(BACKUP) == numbered(1));
    CHECK(contents(PATH) == numbered(2));
}

TEST_CASE("The file logger fails on a path too long, or a file it can't open") {
    Files files;
    ebisu_file_logger_t logger;
    const std::string too_long(EBISU_FILE_LOGGER_PATH_MAX - 2, 'a');
    // And says which.
    ebisu_logger_init_error_t error = EBISU_LOGGER_INIT_ERROR_NONE;
    CHECK_FALSE(ebisu_file_logger_init(&logger, too_long.c_str(), 0, EBISU_LOG_DEBUG, nullptr,
            &error));
    CHECK(error == EBISU_LOGGER_INIT_ERROR_ARGUMENT);
    ebisu_log(&logger.base, EBISU_LOG_ERROR, "dropped");
    ebisu_file_logger_close(&logger);
    error = EBISU_LOGGER_INIT_ERROR_NONE;
    CHECK_FALSE(ebisu_file_logger_init(&logger, nullptr, 0, EBISU_LOG_DEBUG, nullptr, &error));
    CHECK(error == EBISU_LOGGER_INIT_ERROR_ARGUMENT);
    error = EBISU_LOGGER_INIT_ERROR_NONE;
    CHECK_FALSE(ebisu_file_logger_init(&logger, "", 0, EBISU_LOG_DEBUG, nullptr, &error));
    CHECK(error == EBISU_LOGGER_INIT_ERROR_ARGUMENT);
    // To no one who doesn't ask.
    CHECK_FALSE(ebisu_file_logger_init(&logger, "", 0, EBISU_LOG_DEBUG, nullptr, nullptr));

    SECTION("and tries the file again with each message") {
        const char* dir = "file_logger_test_dir";
        const std::string path = std::string(dir) + "/test.log";
        remove(path.c_str());
#ifdef _WIN32
        _rmdir(dir);
#else
        rmdir(dir);
#endif
        REQUIRE_FALSE(ebisu_file_logger_init(&logger, path.c_str(), 0, EBISU_LOG_DEBUG,
                nullptr, &error));
        CHECK(error == EBISU_LOGGER_INIT_ERROR_OPEN);
        ebisu_log(&logger.base, EBISU_LOG_ERROR, "lost, with no directory");
#ifdef _WIN32
        REQUIRE(_mkdir(dir) == 0);
#else
        REQUIRE(mkdir(dir, 0700) == 0);
#endif
        ebisu_log(&logger.base, EBISU_LOG_ERROR, "written");
        ebisu_file_logger_close(&logger);
        CHECK(contents(path.c_str()) == "error: written\n");
        remove(path.c_str());
#ifdef _WIN32
        _rmdir(dir);
#else
        rmdir(dir);
#endif
    }
}

// What 8 tasks write, some 200 kB, takes two files of the size, so that the
// file is renamed once and nothing is dropped.
TEST_CASE("The file logger's lines from several tasks at once stay whole, and none is lost") {
    Files files;
    TestLock lock;
    ebisu_file_logger_t logger;
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 128 * 1024, EBISU_LOG_DEBUG, &lock.base, nullptr));
    const int threads = 8;
    static const int each = 200;
    const std::string padding(100, '.');
    std::vector<std::thread> running;
    for (int t = 0; t < threads; ++t) {
        running.emplace_back([&logger, &padding, t]() {
            for (int i = 0; i < each; ++i) {
                ebisu_log(&logger.base, EBISU_LOG_INFO, "thread %d line %d %s", t, i,
                        padding.c_str());
            }
        });
    }
    for (std::thread& thread : running) {
        thread.join();
    }
    ebisu_file_logger_close(&logger);
    const std::string backup = contents(BACKUP);
    const std::string current = contents(PATH);
    REQUIRE(backup != "none");
    CHECK(backup.size() <= 128 * 1024);
    CHECK(current.size() <= 128 * 1024);
    std::set<std::string> seen;
    const std::string all = backup + current;
    size_t start = 0;
    for (size_t end; (end = all.find('\n', start)) != std::string::npos; start = end + 1) {
        const std::string line = all.substr(start, end - start);
        int t = -1, i = -1;
        char rest[128];
        REQUIRE(sscanf(line.c_str(), "info: thread %d line %d %127s", &t, &i, rest) == 3);
        CHECK(std::string(rest) == padding);
        seen.insert(line);
    }
    CHECK(start == all.size());
    CHECK(seen.size() == (size_t)(threads * each));
}

namespace {

// Days from 1970-01-01 to a date, as Howard Hinnant's days_from_civil has it.
long long days_from_civil(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

// The lines of a file, without their newlines.
std::vector<std::string> lines_of(const std::string& all) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t end; (end = all.find('\n', start)) != std::string::npos; start = end + 1) {
        out.push_back(all.substr(start, end - start));
    }
    return out;
}

// A line's timestamp, in milliseconds since the epoch, and what is after it,
// or -1 if it doesn't start with one of RFC 3339's, in UTC to the millisecond.
long long timestamp_of(const std::string& line, std::string* rest) {
    int y, mo, d, h, mi, s, ms;
    char z;
    int consumed = 0;
    if (line.size() < 25 || line[24] != ' ' ||
            sscanf(line.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d.%3d%c%n", &y, &mo, &d, &h, &mi, &s,
                &ms, &z, &consumed) != 8 || z != 'Z' || consumed != 24) {
        return -1;
    }
    *rest = line.substr(25);
    return ((days_from_civil(y, (unsigned)mo, (unsigned)d) * 24 + h) * 60 + mi) * 60000LL +
        s * 1000LL + ms;
}

// The time of day in milliseconds since the epoch, from the clock the logger
// reads: time() may not be it, as glibc's reads a coarser clock, which can be
// behind it by a few milliseconds, and so a second behind just after one
// starts.
long long now_ms() {
#ifdef _WIN32
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER intervals;
    intervals.LowPart = now.dwLowDateTime;
    intervals.HighPart = now.dwHighDateTime;
    return (long long)(intervals.QuadPart / 10000u) - 11644473600000LL;
#else
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
#endif
}

} // namespace

TEST_CASE("The file logger starts each line with the time it is written, if asked") {
    Files files;
    ebisu_file_logger_t logger;
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 0, EBISU_LOG_DEBUG, nullptr, nullptr));
    ebisu_file_logger_set_timestamps(&logger, true);
    const long long before = now_ms();
    ebisu_log(&logger.base, EBISU_LOG_ERROR, "failed to connect to %s.", "example.com");
    ebisu_log(&logger.base, EBISU_LOG_INFO, "%d tries", 3);
    const long long after = now_ms();
    ebisu_file_logger_close(&logger);
    const std::vector<std::string> lines = lines_of(contents(PATH));
    REQUIRE(lines.size() == 2);
    std::string rest;
    long long first = timestamp_of(lines[0], &rest);
    CHECK(rest == "error: failed to connect to example.com.");
    long long second = timestamp_of(lines[1], &rest);
    CHECK(rest == "info: 3 tries");
    // Between the test's readings of the same clock.
    CHECK(first >= before);
    CHECK(second >= first);
    CHECK(second <= after);

    SECTION("and not unless asked") {
        Files::clear();
        REQUIRE(ebisu_file_logger_init(&logger, PATH, 0, EBISU_LOG_DEBUG, nullptr, nullptr));
        ebisu_log(&logger.base, EBISU_LOG_ERROR, "plain");
        ebisu_file_logger_close(&logger);
        CHECK(contents(PATH) == "error: plain\n");
    }
}

// Each line is 25 bytes more, 39, and 2 of them fit in 100.
TEST_CASE("The file logger counts a line's timestamp in the file's size") {
    Files files;
    ebisu_file_logger_t logger;
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 100, EBISU_LOG_DEBUG, nullptr, nullptr));
    ebisu_file_logger_set_timestamps(&logger, true);
    for (int i = 1; i <= 5; ++i) {
        ebisu_log(&logger.base, EBISU_LOG_INFO, "line %02d", i);
    }
    ebisu_file_logger_close(&logger);
    const std::string backup = contents(BACKUP);
    const std::string current = contents(PATH);
    CHECK(backup.size() == 2 * 39);
    CHECK(current.size() == 39);
    std::string rest;
    REQUIRE(timestamp_of(lines_of(current)[0], &rest) >= 0);
    CHECK(rest + "\n" == numbered(5));
}

TEST_CASE("The file logger's line with its timestamp is at most the longest a logger writes") {
    Files files;
    ebisu_file_logger_t logger;
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 0, EBISU_LOG_DEBUG, nullptr, nullptr));
    ebisu_file_logger_set_timestamps(&logger, true);
    const std::string message(2 * EBISU_LOGGER_LINE_MAX, 'x');
    ebisu_log(&logger.base, EBISU_LOG_ERROR, "%s", message.c_str());
    ebisu_file_logger_close(&logger);
    const std::string all = contents(PATH);
    CHECK(all.size() == EBISU_LOGGER_LINE_MAX);
    std::string rest;
    REQUIRE(timestamp_of(lines_of(all)[0], &rest) >= 0);
    CHECK(rest == "error: " + std::string(EBISU_LOGGER_LINE_MAX - 25 - 7 - 1 - 3, 'x') + "...");
}

// The time is taken under the lock, so the lines of several tasks have their
// times in the order the file has them.
TEST_CASE("The file logger's timestamps are in the order of its lines") {
    Files files;
    TestLock lock;
    ebisu_file_logger_t logger;
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 0, EBISU_LOG_DEBUG, &lock.base, nullptr));
    ebisu_file_logger_set_timestamps(&logger, true);
    std::vector<std::thread> running;
    for (int t = 0; t < 4; ++t) {
        running.emplace_back([&logger, t]() {
            for (int i = 0; i < 100; ++i) {
                ebisu_log(&logger.base, EBISU_LOG_INFO, "thread %d line %d", t, i);
            }
        });
    }
    for (std::thread& thread : running) {
        thread.join();
    }
    ebisu_file_logger_close(&logger);
    const std::vector<std::string> lines = lines_of(contents(PATH));
    REQUIRE(lines.size() == 400);
    long long last = 0;
    for (const std::string& line : lines) {
        std::string rest;
        long long at = timestamp_of(line, &rest);
        REQUIRE(at >= 0);
        CHECK(at >= last);
        last = at;
    }
}
