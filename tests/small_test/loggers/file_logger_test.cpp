// platform's logger to a file it keeps under a size, which writes in the
// working directory, the build tree, and is read back through streams of the
// test's own.
#include <catch.hpp>
#include <ebisu_loggers.h>
#include <ebisu_object.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <stdio.h>
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
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 0, EBISU_LOG_WARNING, nullptr));
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
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 100, EBISU_LOG_DEBUG, nullptr));
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
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 100, EBISU_LOG_DEBUG, nullptr));
    ebisu_log(&logger.base, EBISU_LOG_INFO, "line %02d", 1);
    ebisu_file_logger_close(&logger);
    CHECK(contents(BACKUP) == before);
    CHECK(contents(PATH) == numbered(1));

    SECTION("and one that fits, to it") {
        write_file(PATH, std::string(10, 'y'));
        REQUIRE(ebisu_file_logger_init(&logger, PATH, 100, EBISU_LOG_DEBUG, nullptr));
        ebisu_log(&logger.base, EBISU_LOG_INFO, "line %02d", 2);
        ebisu_file_logger_close(&logger);
        CHECK(contents(PATH) == std::string(10, 'y') + numbered(2));
        CHECK(contents(BACKUP) == before);
    }
}

TEST_CASE("The file logger writes a line longer than its size in a file of its own") {
    Files files;
    ebisu_file_logger_t logger;
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 10, EBISU_LOG_DEBUG, nullptr));
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
    CHECK_FALSE(ebisu_file_logger_init(&logger, too_long.c_str(), 0, EBISU_LOG_DEBUG, nullptr));
    ebisu_log(&logger.base, EBISU_LOG_ERROR, "dropped");
    ebisu_file_logger_close(&logger);
    CHECK_FALSE(ebisu_file_logger_init(&logger, nullptr, 0, EBISU_LOG_DEBUG, nullptr));
    CHECK_FALSE(ebisu_file_logger_init(&logger, "", 0, EBISU_LOG_DEBUG, nullptr));

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
                nullptr));
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
    REQUIRE(ebisu_file_logger_init(&logger, PATH, 128 * 1024, EBISU_LOG_DEBUG, &lock.base));
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
