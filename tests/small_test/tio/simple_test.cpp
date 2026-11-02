#include "catch.hpp"
#include "task_needs.h"
#include "tio.h"

#include <functional>
#include <stdint.h>
#include <string>
#include <string.h>

TEST_CASE( "enable absolute target test" ) {
    tio_handler_t handler;
    tio_handler_init(&handler);
    REQUIRE( handler._kii._khc._absolute_target == false );
    tio_handler_enable_absolute_target(&handler, KII_TRUE);
    REQUIRE( handler._kii._khc._absolute_target == true );
    tio_handler_enable_absolute_target(&handler, KII_FALSE);
    REQUIRE( handler._kii._khc._absolute_target == false );

    tio_updater_t updater;
    tio_updater_init(&updater);
    REQUIRE( updater._kii._khc._absolute_target == false );
    tio_updater_enable_absolute_target(&updater, KII_TRUE);
    REQUIRE( updater._kii._khc._absolute_target == true );
    tio_updater_enable_absolute_target(&updater, KII_FALSE);
    REQUIRE( updater._kii._khc._absolute_target == false );
}

TEST_CASE( "enable insecure test" ) {
    tio_handler_t handler;

    tio_handler_init(&handler);
    REQUIRE( handler._kii._khc._enable_insecure == 0 );
    REQUIRE( handler._kii._insecure_mqtt == KII_FALSE );

    tio_handler_enable_insecure_http(&handler, KII_TRUE);
    REQUIRE( handler._kii._khc._enable_insecure == 1 );
    REQUIRE( handler._kii._insecure_mqtt == KII_FALSE );

    tio_handler_enable_insecure_mqtt(&handler, KII_TRUE);
    REQUIRE( handler._kii._khc._enable_insecure == 1 );
    REQUIRE( handler._kii._insecure_mqtt == KII_TRUE );

    tio_handler_enable_insecure_http(&handler, KII_FALSE);
    REQUIRE( handler._kii._khc._enable_insecure == 0 );
    REQUIRE( handler._kii._insecure_mqtt == KII_TRUE );

    tio_handler_enable_insecure_mqtt(&handler, KII_FALSE);
    REQUIRE( handler._kii._khc._enable_insecure == 0 );
    REQUIRE( handler._kii._insecure_mqtt == KII_FALSE );

    tio_updater_t updater;
    tio_updater_init(&updater);
    REQUIRE( updater._kii._khc._enable_insecure == 0 );
    REQUIRE( updater._kii._insecure_mqtt == KII_FALSE );

    tio_updater_enable_insecure_http(&updater, KII_TRUE);
    REQUIRE( updater._kii._khc._enable_insecure == 1 );
    REQUIRE( updater._kii._insecure_mqtt == KII_FALSE );

    tio_updater_enable_insecure_http(&updater, KII_FALSE);
    REQUIRE( updater._kii._khc._enable_insecure == 0 );
    REQUIRE( updater._kii._insecure_mqtt == KII_FALSE );
}

static kii_bool_t recv_timed_out(void* sock_ctx)
{
    return KII_TRUE;
}

TEST_CASE( "tio_handler_set_cb_sock_recv_timed_out_mqtt sets the MQTT callback" ) {
    tio_handler_t handler;
    tio_handler_init(&handler);
    REQUIRE( handler._kii._cb_mqtt_sock_recv_timed_out == nullptr );
    REQUIRE( handler._kii._mqtt_sock_recv_timed_out_ctx == nullptr );

    int sock_ctx = 0;
    tio_handler_set_cb_sock_recv_timed_out_mqtt(&handler, recv_timed_out, &sock_ctx);
    REQUIRE( handler._kii._cb_mqtt_sock_recv_timed_out == recv_timed_out );
    REQUIRE( handler._kii._mqtt_sock_recv_timed_out_ctx == &sock_ctx );

    tio_handler_set_cb_sock_recv_timed_out_mqtt(&handler, NULL, NULL);
    REQUIRE( handler._kii._cb_mqtt_sock_recv_timed_out == nullptr );
    REQUIRE( handler._kii._mqtt_sock_recv_timed_out_ctx == nullptr );
}

static ebisu_task_code_t fail_task_create(const char*, EBISU_TASK_ENTRY, void*, void*)
{
    return EBISU_TASKC_FAIL;
}

static tio_bool_t no_action(tio_action_t*, tio_action_err_t*, tio_action_result_data_t*, void*)
{
    return KII_TRUE;
}

static ebisu_task_code_t count_task_create(const char* name, EBISU_TASK_ENTRY entry, void* entry_param, void* userdata)
{
    ++*(int*)userdata;
    return EBISU_TASKC_OK;
}

// MQTT carries a keep-alive of at most 65535 seconds. tio takes it as a
// uint32_t rather than a uint16_t, so a larger one arrives whole at
// tio_handler_start() to be turned away, rather than cut down at the call.
TEST_CASE( "tio_handler_start keep-alive range" ) {
    char buff[64];
    tio_handler_t handler;
    tio_handler_init(&handler);
    khct::set_push_needs(&handler._kii, buff, sizeof(buff));
    int call_task_create = 0;
    tio_handler_set_cb_task_create(&handler, count_task_create, &call_task_create);
    tio_author_t author = {};

    for (uint32_t interval : { (uint32_t)0, (uint32_t)300, (uint32_t)65535 }) {
        INFO("keep-alive " << interval);
        call_task_create = 0;
        tio_handler_set_keep_alive_interval(&handler, interval);
        CHECK( tio_handler_start(&handler, &author, no_action, NULL) == TIO_ERR_OK );
        CHECK( call_task_create == 1 );
        CHECK( handler._kii._keep_alive_interval == interval );
        // The callback runs no task, which would have said so when it ended.
        handler._kii._push_task_running = KII_FALSE;
    }

    for (uint32_t interval : { (uint32_t)65536, (uint32_t)70000, UINT32_MAX }) {
        INFO("keep-alive " << interval);
        call_task_create = 0;
        tio_handler_set_keep_alive_interval(&handler, interval);
        CHECK( tio_handler_start(&handler, &author, no_action, NULL) == TIO_ERR_FAIL );
        CHECK( call_task_create == 0 );
    }
}

// Stands in for the platform's task creation: counts the calls and returns the
// result the test chose.
struct TaskCreateCtx {
    ebisu_task_code_t result;
    int calls;
};

static ebisu_task_code_t mock_task_create(const char* name, EBISU_TASK_ENTRY entry, void* entry_param, void* userdata)
{
    TaskCreateCtx* ctx = (TaskCreateCtx*)userdata;
    ++ctx->calls;
    return ctx->result;
}

// tio_handler_start() returns what kii_start_push_task() does, which was
// KII_ERR_OK whether or not the MQTT task had been created. A task that could
// not be created is TIO_ERR_CREATE_TASK, as it is for tio_updater_start().
TEST_CASE( "tio_handler_start reports a task it could not create" ) {
    char buff[64];
    tio_handler_t handler;
    tio_handler_init(&handler);
    khct::set_push_needs(&handler._kii, buff, sizeof(buff));
    tio_author_t author = {};

    SECTION("task created") {
        TaskCreateCtx ctx = { EBISU_TASKC_OK, 0 };
        tio_handler_set_cb_task_create(&handler, mock_task_create, &ctx);
        CHECK( tio_handler_start(&handler, &author, no_action, NULL) == TIO_ERR_OK );
        CHECK( ctx.calls == 1 );
    }

    SECTION("task not created") {
        TaskCreateCtx ctx = { EBISU_TASKC_FAIL, 0 };
        tio_handler_set_cb_task_create(&handler, mock_task_create, &ctx);
        CHECK( tio_handler_start(&handler, &author, no_action, NULL) == TIO_ERR_CREATE_TASK );
        CHECK( ctx.calls == 1 );
    }

    SECTION("no task-create callback") {
        CHECK( tio_handler_start(&handler, &author, no_action, NULL) == TIO_ERR_CREATE_TASK );
    }
}

// tio_updater_init() leaves the task-create callback unset, and starting the
// updater without one used to call through the null pointer.
TEST_CASE( "tio_updater_start without a task-create callback" ) {
    tio_updater_t updater;
    tio_updater_init(&updater);
    tio_updater_set_interval(&updater, 60);
    tio_author_t author = {};

    REQUIRE( tio_updater_start(&updater, &author, NULL, NULL, NULL, NULL) == TIO_ERR_CREATE_TASK );
}

TEST_CASE( "tio_updater_start without an interval" ) {
    tio_updater_t updater;
    tio_updater_init(&updater);
    TaskCreateCtx ctx = { EBISU_TASKC_OK, 0 };
    tio_updater_set_cb_task_create(&updater, mock_task_create, &ctx);
    tio_author_t author = {};

    CHECK( tio_updater_start(&updater, &author, NULL, NULL, NULL, NULL) == TIO_ERR_FAIL );
    CHECK( ctx.calls == 0 );
}

namespace {

// The updater's task, run to its end by the test.
struct UpdaterTask {
    int continues_left;
    int continue_called;
    int exit_called;
    int state_size_called;
    uint32_t delayed_ms;
};

ebisu_task_code_t run_task(const char* name, EBISU_TASK_ENTRY entry, void* entry_param, void* userdata)
{
    entry(entry_param);
    return EBISU_TASKC_OK;
}

kii_bool_t updater_continue(void* task_info, void* userdata)
{
    UpdaterTask* task = (UpdaterTask*)userdata;
    ++task->continue_called;
    return task->continues_left-- > 0 ? KII_TRUE : KII_FALSE;
}

void updater_exit(void* task_info, void* userdata)
{
    ++((UpdaterTask*)userdata)->exit_called;
}

void updater_delay(uint32_t ms, void* userdata)
{
    ((UpdaterTask*)userdata)->delayed_ms += ms;
}

// Nothing to upload, so the task makes no request.
size_t updater_state_size(void* userdata)
{
    ++((UpdaterTask*)userdata)->state_size_called;
    return 0;
}

size_t updater_read(char*, size_t, void*)
{
    return 0;
}

}

// The task-create callback leaves the task to the test, which ends it.
struct HeldTask {
    EBISU_TASK_ENTRY entry;
    void* entry_param;
    int calls;
};

static ebisu_task_code_t hold_task(const char*, EBISU_TASK_ENTRY entry, void* entry_param, void* userdata)
{
    HeldTask* held = (HeldTask*)userdata;
    held->entry = entry;
    held->entry_param = entry_param;
    ++held->calls;
    return EBISU_TASKC_OK;
}

static kii_bool_t stop_task(void*, void*)
{
    return KII_FALSE;
}

// The running task reads what these set, so being refused has to leave it all
// as it was.
TEST_CASE( "tio_handler_start refuses while its task is running" ) {
    char buff[64];
    tio_handler_t handler;
    tio_handler_init(&handler);
    khct::set_push_needs(&handler._kii, buff, sizeof(buff));
    tio_handler_set_cb_task_continue(&handler, stop_task, NULL);
    HeldTask held = {};
    tio_handler_set_cb_task_create(&handler, hold_task, &held);
    tio_author_t author = {};
    strcpy(author.author_id, "first");
    REQUIRE( tio_handler_start(&handler, &author, no_action, NULL) == TIO_ERR_OK );

    tio_author_t other = {};
    strcpy(other.author_id, "other");
    int userdata = 0;
    CHECK( tio_handler_start(&handler, &other, no_action, &userdata) == TIO_ERR_FAIL );
    CHECK( held.calls == 1 );
    CHECK( std::string(handler._kii._author.author_id) == "first" );
    CHECK( handler._cb_action_data == nullptr );

    held.entry(held.entry_param);
    CHECK( tio_handler_start(&handler, &other, no_action, &userdata) == TIO_ERR_OK );
    CHECK( held.calls == 2 );
    CHECK( std::string(handler._kii._author.author_id) == "other" );
}

TEST_CASE( "tio_handler_start needs an action callback" ) {
    char buff[64];
    tio_handler_t handler;
    tio_handler_init(&handler);
    khct::set_push_needs(&handler._kii, buff, sizeof(buff));
    HeldTask held = {};
    tio_handler_set_cb_task_create(&handler, hold_task, &held);
    tio_author_t author = {};

    CHECK( tio_handler_start(&handler, &author, NULL, NULL) == TIO_ERR_FAIL );
    CHECK( held.calls == 0 );
}

// The task uses all of this without checking, so one left out crashed it
// whenever it got there, after tio_updater_start() had returned TIO_ERR_OK.
TEST_CASE( "tio_updater_start needs what the task uses" ) {
    char buff[64];
    tio_updater_t updater;
    tio_updater_init(&updater);
    khct::set_updater_needs(&updater._kii, buff, sizeof(buff));
    tio_updater_set_interval(&updater, 60);
    HeldTask held = {};
    tio_updater_set_cb_task_create(&updater, hold_task, &held);
    tio_author_t author = {};
    UpdaterTask task = {};

    const struct {
        const char* left_out;
        std::function<void(kii_t*)> unset;
    } cases[] = {
        { "delay", [](kii_t* k) { kii_set_cb_delay_ms(k, NULL, NULL); } },
        { "connect", [](kii_t* k) { kii_set_cb_http_sock_connect(k, NULL, NULL); } },
        { "send", [](kii_t* k) { kii_set_cb_http_sock_send(k, NULL, NULL); } },
        { "recv", [](kii_t* k) { kii_set_cb_http_sock_recv(k, NULL, NULL); } },
        { "close", [](kii_t* k) { kii_set_cb_http_sock_close(k, NULL, NULL); } },
        { "buffer", [](kii_t* k) { kii_set_buff(k, NULL, 0); } },
    };
    const tio_updater_t complete = updater;
    for (const auto& c : cases) {
        INFO("left out " << c.left_out);
        c.unset(&updater._kii);
        CHECK( tio_updater_start(&updater, &author, updater_state_size, &task, updater_read, NULL) == TIO_ERR_FAIL );
        CHECK( held.calls == 0 );
        updater = complete;
    }

    CHECK( tio_updater_start(&updater, &author, NULL, NULL, updater_read, NULL) == TIO_ERR_FAIL );
    CHECK( tio_updater_start(&updater, &author, updater_state_size, &task, NULL, NULL) == TIO_ERR_FAIL );
    CHECK( held.calls == 0 );

    CHECK( tio_updater_start(&updater, &author, updater_state_size, &task, updater_read, NULL) == TIO_ERR_OK );
    CHECK( held.calls == 1 );
}

struct RestartUpdater {
    tio_updater_t* updater;
    tio_author_t* author;
    UpdaterTask* task;
    tio_code_t result;
};

static void restart_updater(void*, void* userdata)
{
    RestartUpdater* ctx = (RestartUpdater*)userdata;
    ctx->result = tio_updater_start(ctx->updater, ctx->author, updater_state_size, ctx->task, updater_read, NULL);
}

TEST_CASE( "tio_updater_start refuses while its task is running" ) {
    char buff[64];
    tio_updater_t updater;
    tio_updater_init(&updater);
    khct::set_updater_needs(&updater._kii, buff, sizeof(buff));
    tio_updater_set_interval(&updater, 60);
    tio_updater_set_cb_task_continue(&updater, stop_task, NULL);
    HeldTask held = {};
    tio_updater_set_cb_task_create(&updater, hold_task, &held);
    tio_author_t author = {};
    strcpy(author.author_id, "first");
    UpdaterTask task = {};

    SECTION("until it has ended") {
        REQUIRE( tio_updater_start(&updater, &author, updater_state_size, &task, updater_read, NULL) == TIO_ERR_OK );

        tio_author_t other = {};
        strcpy(other.author_id, "other");
        CHECK( tio_updater_start(&updater, &other, updater_state_size, &task, updater_read, NULL) == TIO_ERR_FAIL );
        CHECK( held.calls == 1 );
        CHECK( std::string(updater._kii._author.author_id) == "first" );

        held.entry(held.entry_param);
        CHECK( tio_updater_start(&updater, &other, updater_state_size, &task, updater_read, NULL) == TIO_ERR_OK );
        CHECK( held.calls == 2 );
    }

    SECTION("but not from its exit callback") {
        RestartUpdater restart = { &updater, &author, &task, TIO_ERR_FAIL };
        tio_updater_set_cb_task_exit(&updater, restart_updater, &restart);
        REQUIRE( tio_updater_start(&updater, &author, updater_state_size, &task, updater_read, NULL) == TIO_ERR_OK );

        held.entry(held.entry_param);
        CHECK( restart.result == TIO_ERR_OK );
        CHECK( held.calls == 2 );
    }

    SECTION("but not after a task that could not be created") {
        tio_updater_set_cb_task_create(&updater, fail_task_create, NULL);
        REQUIRE( tio_updater_start(&updater, &author, updater_state_size, &task, updater_read, NULL) == TIO_ERR_CREATE_TASK );

        tio_updater_set_cb_task_create(&updater, hold_task, &held);
        CHECK( tio_updater_start(&updater, &author, updater_state_size, &task, updater_read, NULL) == TIO_ERR_OK );
    }
}

// An interval of 1 second went straight to its wait and upload, without asking
// whether to continue.
TEST_CASE( "The updater asks to continue every second, at any interval" ) {
    for (uint32_t interval : { (uint32_t)1, (uint32_t)2, (uint32_t)3, (uint32_t)60 }) {
        INFO("interval " << interval);
        char buff[64];
        tio_updater_t updater;
        tio_updater_init(&updater);
        khct::set_updater_needs(&updater._kii, buff, sizeof(buff));
        UpdaterTask task = {};
        task.continues_left = 6;
        tio_updater_set_cb_task_create(&updater, run_task, NULL);
        tio_updater_set_cb_task_continue(&updater, updater_continue, &task);
        tio_updater_set_cb_task_exit(&updater, updater_exit, &task);
        tio_updater_set_cb_delay_ms(&updater, updater_delay, &task);
        tio_updater_set_interval(&updater, interval);
        tio_author_t author = {};

        CHECK( tio_updater_start(&updater, &author, updater_state_size, &task, updater_read, NULL) == TIO_ERR_OK );
        CHECK( task.continue_called == 7 );
        CHECK( task.exit_called == 1 );
        CHECK( task.delayed_ms == 6000 );
        CHECK( task.state_size_called == (int)(6 / interval) );
    }
}

namespace {
uint64_t clock_now_ms(void*) { return 0; }
}

TEST_CASE( "The handler and the updater give a clock the time, through their kii_t" ) {
    tio_handler_t handler;
    tio_updater_t updater;
    tio_handler_init(&handler);
    tio_updater_init(&updater);

    ebisu_clock_t clock;
    ebisu_clock_init(&clock, NULL, NULL);
    CHECK( tio_handler_set_clock(&handler, &clock) == TIO_ERR_FAIL );
    CHECK( tio_updater_set_clock(&updater, &clock) == TIO_ERR_FAIL );
    CHECK( handler._kii._clock == nullptr );
    CHECK( updater._kii._clock == nullptr );

    ebisu_clock_init(&clock, clock_now_ms, NULL);
    CHECK( tio_handler_set_clock(&handler, &clock) == TIO_ERR_OK );
    CHECK( tio_updater_set_clock(&updater, &clock) == TIO_ERR_OK );
    CHECK( handler._kii._clock == &clock );
    CHECK( updater._kii._clock == &clock );
}
