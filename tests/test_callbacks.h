#include <ebisu_log.h>
#include <ebisu_socket.h>
#include <ebisu_system.h>
#include <functional>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <utility>
#include <vector>

// kii.h's, declared here for the khc tests, which don't have kii.h.
struct kii_mqtt_message_t;

namespace khct {
namespace cb {

struct SockCtx;

// What ebisu is given: standard layout, so that self leads back to the SockCtx.
struct MockSocket {
    ebisu_socket_t base;
    SockCtx* owner;
};

ebisu_sock_code_t mock_connect(ebisu_socket_t* self, const char* host, uint16_t port);
ebisu_sock_code_t mock_send(ebisu_socket_t* self, const char* buffer, size_t length, size_t* out_sent_length);
ebisu_sock_code_t mock_recv(ebisu_socket_t* self, char* buffer, size_t length_to_read, size_t* out_actual_length);
ebisu_sock_code_t mock_close(ebisu_socket_t* self);
bool mock_recv_timed_out(ebisu_socket_t* self);
ebisu_sock_error_t mock_last_error(ebisu_socket_t* self, long* out_detail);
bool mock_recv_wait(ebisu_socket_t* self, uint32_t timeout_ms);

// A socket whose operations call these. The first argument each gets is the
// SockCtx. ops is the SockCtx's own, so a test may leave an operation out.
struct SockCtx {
    std::function<ebisu_sock_code_t(void* socket_context, const char* host, uint16_t port)> on_connect;
    std::function<ebisu_sock_code_t(void* socket_context, const char* buffer, size_t length, size_t* out_sent_length)> on_send;
    std::function<ebisu_sock_code_t(void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length)> on_recv;
    std::function<ebisu_sock_code_t(void* socket_context)> on_close;
    // Unset, a failed receive is taken for a timeout, as with no recv_timed_out.
    std::function<bool(void* socket_context)> on_recv_timed_out;
    // Unset, the socket can't tell why it failed.
    std::function<ebisu_sock_error_t(long* out_detail)> on_last_error;
    // Called if ops.recv_wait is set to mock_recv_wait, which it isn't unless
    // a test sets it.
    std::function<bool(uint32_t timeout_ms)> on_recv_wait;
    ebisu_socket_ops_t ops;
    MockSocket sock;

    SockCtx() {
        ops.connect = mock_connect;
        ops.send = mock_send;
        ops.recv = mock_recv;
        ops.close = mock_close;
        ops.recv_timed_out = mock_recv_timed_out;
        ops.last_error = mock_last_error;
        ops.recv_wait = NULL;
        sock.base.ops = &ops;
        sock.owner = this;
    }
    SockCtx(const SockCtx&) = delete;
    SockCtx& operator=(const SockCtx&) = delete;

    ebisu_socket_t* socket() {
        return &sock.base;
    }
};

inline SockCtx* sock_ctx_of(ebisu_socket_t* self) {
    return reinterpret_cast<MockSocket*>(self)->owner;
}

inline ebisu_sock_code_t mock_connect(ebisu_socket_t* self, const char* host, uint16_t port) {
    SockCtx* ctx = sock_ctx_of(self);
    return ctx->on_connect(ctx, host, port);
}

inline ebisu_sock_code_t mock_send(ebisu_socket_t* self, const char* buffer, size_t length, size_t* out_sent_length) {
    SockCtx* ctx = sock_ctx_of(self);
    return ctx->on_send(ctx, buffer, length, out_sent_length);
}

inline ebisu_sock_code_t mock_recv(ebisu_socket_t* self, char* buffer, size_t length_to_read, size_t* out_actual_length) {
    SockCtx* ctx = sock_ctx_of(self);
    return ctx->on_recv(ctx, buffer, length_to_read, out_actual_length);
}

inline ebisu_sock_code_t mock_close(ebisu_socket_t* self) {
    SockCtx* ctx = sock_ctx_of(self);
    return ctx->on_close(ctx);
}

inline bool mock_recv_timed_out(ebisu_socket_t* self) {
    SockCtx* ctx = sock_ctx_of(self);
    return ctx->on_recv_timed_out ? ctx->on_recv_timed_out(ctx) : true;
}

inline ebisu_sock_error_t mock_last_error(ebisu_socket_t* self, long* out_detail) {
    SockCtx* ctx = sock_ctx_of(self);
    return ctx->on_last_error ? ctx->on_last_error(out_detail) : EBISU_SOCK_ERROR_NONE;
}

inline bool mock_recv_wait(ebisu_socket_t* self, uint32_t timeout_ms) {
    SockCtx* ctx = sock_ctx_of(self);
    return ctx->on_recv_wait(timeout_ms);
}

// A socket for what only has to be set, never used: calling it throws.
inline ebisu_socket_t* unused_socket() {
    static SockCtx ctx;
    return ctx.socket();
}

struct SystemCtx;

struct MockSystem {
    ebisu_system_t base;
    SystemCtx* owner;
};

ebisu_task_code_t mock_task_create(ebisu_system_t* self, const char* name, EBISU_TASK_ENTRY entry, void* entry_param);
void mock_delay_ms(ebisu_system_t* self, uint32_t msec);
uint64_t mock_monotonic_ms(ebisu_system_t* self);

// A system whose operations call these. Unset, delay_ms returns at once and
// monotonic_ms gives 0; task_create throws. ops is the SystemCtx's own, so a
// test may leave an operation out.
struct SystemCtx {
    std::function<ebisu_task_code_t(const char* name, EBISU_TASK_ENTRY entry, void* entry_param)> on_task_create;
    std::function<void(uint32_t msec)> on_delay_ms;
    std::function<uint64_t()> on_monotonic_ms;
    ebisu_system_ops_t ops;
    MockSystem sys;

    SystemCtx() {
        ops.task_create = mock_task_create;
        ops.delay_ms = mock_delay_ms;
        ops.monotonic_ms = mock_monotonic_ms;
        sys.base.ops = &ops;
        sys.owner = this;
    }
    SystemCtx(const SystemCtx&) = delete;
    SystemCtx& operator=(const SystemCtx&) = delete;

    ebisu_system_t* system() {
        return &sys.base;
    }
};

inline SystemCtx* system_ctx_of(ebisu_system_t* self) {
    return reinterpret_cast<MockSystem*>(self)->owner;
}

inline ebisu_task_code_t mock_task_create(ebisu_system_t* self, const char* name, EBISU_TASK_ENTRY entry, void* entry_param) {
    return system_ctx_of(self)->on_task_create(name, entry, entry_param);
}

inline void mock_delay_ms(ebisu_system_t* self, uint32_t msec) {
    SystemCtx* ctx = system_ctx_of(self);
    if (ctx->on_delay_ms) {
        ctx->on_delay_ms(msec);
    }
}

inline uint64_t mock_monotonic_ms(ebisu_system_t* self) {
    SystemCtx* ctx = system_ctx_of(self);
    return ctx->on_monotonic_ms ? ctx->on_monotonic_ms() : 0;
}

// A system that only delays, at once, as the tests' delay callback used to.
// Without a monotonic clock, the MQTT task estimates its keep-alive.
inline ebisu_system_t* delay_only_system() {
    static SystemCtx ctx;
    ctx.ops.task_create = NULL;
    ctx.ops.monotonic_ms = NULL;
    return ctx.system();
}

struct LockCtx;

struct MockLock {
    ebisu_lock_t base;
    LockCtx* owner;
};

void mock_lock(ebisu_lock_t* self);
void mock_unlock(ebisu_lock_t* self);

// A lock whose operations call these; unset, they do nothing.
struct LockCtx {
    std::function<void()> on_lock;
    std::function<void()> on_unlock;
    ebisu_lock_ops_t ops;
    MockLock lock;

    LockCtx() {
        ops.lock = mock_lock;
        ops.unlock = mock_unlock;
        lock.base.ops = &ops;
        lock.owner = this;
    }
    LockCtx(const LockCtx&) = delete;
    LockCtx& operator=(const LockCtx&) = delete;

    ebisu_lock_t* get() {
        return &lock.base;
    }
};

inline void mock_lock(ebisu_lock_t* self) {
    LockCtx* ctx = reinterpret_cast<MockLock*>(self)->owner;
    if (ctx->on_lock) {
        ctx->on_lock();
    }
}

inline void mock_unlock(ebisu_lock_t* self) {
    LockCtx* ctx = reinterpret_cast<MockLock*>(self)->owner;
    if (ctx->on_unlock) {
        ctx->on_unlock();
    }
}

struct IOCtx {
    std::function<size_t(char *buffer, size_t size, void *userdata)> on_read;
    std::function<size_t(char *buffer, size_t size, void *userdata)> on_header;
    std::function<size_t(char *buffer, size_t size, void *userdata)> on_write;
};

inline size_t cb_write(char *buffer, size_t size, void *userdata) {
    IOCtx* ctx = (IOCtx*)(userdata);
    return ctx->on_write(buffer, size, userdata);
}

inline size_t cb_read(char *buffer, size_t size, void *userdata) {
    IOCtx* ctx = (IOCtx*)(userdata);
    return ctx->on_read(buffer, size, userdata);
}

inline size_t cb_header(char *buffer, size_t size, void *userdata) {
    IOCtx* ctx = (IOCtx*)(userdata);
    return ctx->on_header(buffer, size, userdata);
}

struct PushCtx {
    std::function<void(const kii_mqtt_message_t* message)> on_push;
};

struct LoggerCtx;

struct MockLogger {
    ebisu_logger_t base;
    LoggerCtx* owner;
};

void mock_log(ebisu_logger_t* self, ebisu_log_level_t level, const char* fmt, va_list args);

// A logger that keeps each line it is given, with its level.
struct LoggerCtx {
    typedef std::vector<std::pair<ebisu_log_level_t, std::string>> Lines;
    Lines lines;
    ebisu_logger_ops_t ops;
    MockLogger logger;

    LoggerCtx() {
        ops.log = mock_log;
        logger.base.ops = &ops;
        logger.owner = this;
    }
    LoggerCtx(const LoggerCtx&) = delete;
    LoggerCtx& operator=(const LoggerCtx&) = delete;

    ebisu_logger_t* get() {
        return &logger.base;
    }

    bool has(ebisu_log_level_t level, const std::string& line) const {
        for (const auto& l : lines) {
            if (l.first == level && l.second == line) {
                return true;
            }
        }
        return false;
    }

    // Whether any line has part, at any level.
    bool mentions(const std::string& part) const {
        for (const auto& l : lines) {
            if (l.second.find(part) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

inline void mock_log(ebisu_logger_t* self, ebisu_log_level_t level, const char* fmt, va_list args) {
    LoggerCtx* ctx = reinterpret_cast<MockLogger*>(self)->owner;
    char line[1024];
    vsnprintf(line, sizeof(line), fmt, args);
    ctx->lines.emplace_back(level, line);
}

inline void cb_push(const kii_mqtt_message_t* message, void *userdata) {
    PushCtx* ctx = (PushCtx*)(userdata);
    ctx->on_push(message);
}
}
}
