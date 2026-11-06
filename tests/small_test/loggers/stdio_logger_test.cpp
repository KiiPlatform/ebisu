#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <ebisu_loggers.h>
#include <stdio.h>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

// A file of its own in the working directory, which is the build tree, read
// back whole through a stream of its own, which sees only what the logger's
// flushed.
struct LogFile {
    FILE* file;
    LogFile() : file(fopen("stdio_logger_test.log", "w")) {}
    ~LogFile() {
        if (file != nullptr) {
            fclose(file);
        }
        remove("stdio_logger_test.log");
    }
    std::vector<std::string> lines() {
        std::string all;
        FILE* in = fopen("stdio_logger_test.log", "r");
        if (in == nullptr) {
            return { "not readable" };
        }
        int c;
        while ((c = fgetc(in)) != EOF) {
            all += (char)c;
        }
        fclose(in);
        std::vector<std::string> out;
        size_t start = 0;
        for (size_t end; (end = all.find('\n', start)) != std::string::npos; start = end + 1) {
            out.push_back(all.substr(start, end - start));
        }
        // What came after the last newline, which nothing should have.
        if (start < all.size()) {
            out.push_back("unterminated: " + all.substr(start));
        }
        return out;
    }
};

typedef std::vector<std::string> Lines;

} // namespace

TEST_CASE("The stdio logger writes a line for each message as important as its level") {
    LogFile log;
    REQUIRE(log.file != nullptr);
    ebisu_stdio_logger_t logger;
    ebisu_stdio_logger_init(&logger, log.file, EBISU_LOG_WARNING);

    ebisu_log(&logger.base, EBISU_LOG_ERROR, "failed to connect to %s.", "example.com");
    ebisu_log(&logger.base, EBISU_LOG_WARNING, "%d tries", 3);
    ebisu_log(&logger.base, EBISU_LOG_INFO, "dropped");
    ebisu_log(&logger.base, EBISU_LOG_DEBUG, "dropped");
    // Not a level at all.
    ebisu_log(&logger.base, (ebisu_log_level_t)-1, "dropped");
    CHECK(log.lines() == Lines{ "error: failed to connect to example.com.", "warning: 3 tries" });

    SECTION("at each level") {
        ebisu_stdio_logger_init(&logger, log.file, EBISU_LOG_DEBUG);
        ebisu_log(&logger.base, EBISU_LOG_INFO, "connected");
        ebisu_log(&logger.base, EBISU_LOG_DEBUG, "resumed");
        ebisu_log(&logger.base, (ebisu_log_level_t)(EBISU_LOG_DEBUG + 1), "dropped");
        CHECK(log.lines() == Lines{ "error: failed to connect to example.com.", "warning: 3 tries",
                "info: connected", "debug: resumed" });
    }

    // Even with a level beyond the least important, which would let it through.
    SECTION("not one that isn't a level") {
        ebisu_stdio_logger_init(&logger, log.file, (ebisu_log_level_t)100);
        ebisu_log(&logger.base, (ebisu_log_level_t)(EBISU_LOG_DEBUG + 1), "dropped");
        ebisu_log(&logger.base, (ebisu_log_level_t)99, "dropped");
        ebisu_log(&logger.base, EBISU_LOG_DEBUG, "kept");
        CHECK(log.lines() == Lines{ "error: failed to connect to example.com.", "warning: 3 tries",
                "debug: kept" });
    }

    SECTION("cutting a long one short") {
        const std::string message(2 * EBISU_LOGGER_LINE_MAX, 'x');
        ebisu_log(&logger.base, EBISU_LOG_ERROR, "%s", message.c_str());
        Lines lines = log.lines();
        REQUIRE(lines.size() == 3);
        // The line's newline is the last of the longest.
        CHECK(lines[2].size() == EBISU_LOGGER_LINE_MAX - 1);
        CHECK(lines[2] == "error: " + std::string(EBISU_LOGGER_LINE_MAX - 1 - 7 - 3, 'x') + "...");
    }

    SECTION("whose length is the longest") {
        const std::string message(EBISU_LOGGER_LINE_MAX - 1 - 7, 'y');
        ebisu_log(&logger.base, EBISU_LOG_ERROR, "%s", message.c_str());
        CHECK(log.lines().back() == "error: " + message);
    }
}

TEST_CASE("The stdio logger's lines from several threads at once stay whole") {
    LogFile log;
    REQUIRE(log.file != nullptr);
    ebisu_stdio_logger_t logger;
    ebisu_stdio_logger_init(&logger, log.file, EBISU_LOG_DEBUG);

    const int threads = 8, each = 200;
    // Long enough that the C library would write one in several pieces.
    const std::string padding(300, '.');
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

    std::set<std::string> expected;
    for (int t = 0; t < threads; ++t) {
        for (int i = 0; i < each; ++i) {
            expected.insert("info: thread " + std::to_string(t) + " line " + std::to_string(i) +
                    " " + padding);
        }
    }
    Lines lines = log.lines();
    CHECK(lines.size() == (size_t)(threads * each));
    CHECK(std::set<std::string>(lines.begin(), lines.end()) == expected);
}
