#include "catch.hpp"
#include "tio.h"

#include <stdint.h>

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

static kii_task_code_t count_task_create(const char* name, KII_TASK_ENTRY entry, void* entry_param, void* userdata)
{
    ++*(int*)userdata;
    return KII_TASKC_OK;
}

// MQTT carries a keep-alive of at most 65535 seconds. tio takes it as a
// uint32_t rather than a uint16_t, so a larger one arrives whole at
// tio_handler_start() to be turned away, rather than cut down at the call.
TEST_CASE( "tio_handler_start keep-alive range" ) {
    tio_handler_t handler;
    tio_handler_init(&handler);
    int call_task_create = 0;
    tio_handler_set_cb_task_create(&handler, count_task_create, &call_task_create);
    tio_author_t author = {};

    for (uint32_t interval : { (uint32_t)0, (uint32_t)300, (uint32_t)65535 }) {
        INFO("keep-alive " << interval);
        call_task_create = 0;
        tio_handler_set_keep_alive_interval(&handler, interval);
        CHECK( tio_handler_start(&handler, &author, NULL, NULL) == TIO_ERR_OK );
        CHECK( call_task_create == 1 );
        CHECK( handler._kii._keep_alive_interval == interval );
    }

    for (uint32_t interval : { (uint32_t)65536, (uint32_t)70000, UINT32_MAX }) {
        INFO("keep-alive " << interval);
        call_task_create = 0;
        tio_handler_set_keep_alive_interval(&handler, interval);
        CHECK( tio_handler_start(&handler, &author, NULL, NULL) == TIO_ERR_FAIL );
        CHECK( call_task_create == 0 );
    }
}

// Stands in for the platform's task creation: counts the calls and returns the
// result the test chose.
struct TaskCreateCtx {
    kii_task_code_t result;
    int calls;
};

static kii_task_code_t mock_task_create(const char* name, KII_TASK_ENTRY entry, void* entry_param, void* userdata)
{
    TaskCreateCtx* ctx = (TaskCreateCtx*)userdata;
    ++ctx->calls;
    return ctx->result;
}

// tio_handler_start() returns what kii_start_push_task() does, which was
// KII_ERR_OK whether or not the MQTT task had been created. A task that could
// not be created is TIO_ERR_CREATE_TASK, as it is for tio_updater_start().
TEST_CASE( "tio_handler_start reports a task it could not create" ) {
    tio_handler_t handler;
    tio_handler_init(&handler);
    tio_author_t author = {};

    SECTION("task created") {
        TaskCreateCtx ctx = { KII_TASKC_OK, 0 };
        tio_handler_set_cb_task_create(&handler, mock_task_create, &ctx);
        CHECK( tio_handler_start(&handler, &author, NULL, NULL) == TIO_ERR_OK );
        CHECK( ctx.calls == 1 );
    }

    SECTION("task not created") {
        TaskCreateCtx ctx = { KII_TASKC_FAIL, 0 };
        tio_handler_set_cb_task_create(&handler, mock_task_create, &ctx);
        CHECK( tio_handler_start(&handler, &author, NULL, NULL) == TIO_ERR_CREATE_TASK );
        CHECK( ctx.calls == 1 );
    }

    SECTION("no task-create callback") {
        CHECK( tio_handler_start(&handler, &author, NULL, NULL) == TIO_ERR_CREATE_TASK );
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
    TaskCreateCtx ctx = { KII_TASKC_OK, 0 };
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

kii_task_code_t run_task(const char* name, KII_TASK_ENTRY entry, void* entry_param, void* userdata)
{
    entry(entry_param);
    return KII_TASKC_OK;
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

}

// An interval of 1 second went straight to its wait and upload, without asking
// whether to continue.
TEST_CASE( "The updater asks to continue every second, at any interval" ) {
    for (uint32_t interval : { (uint32_t)1, (uint32_t)2, (uint32_t)3, (uint32_t)60 }) {
        INFO("interval " << interval);
        tio_updater_t updater;
        tio_updater_init(&updater);
        UpdaterTask task = {};
        task.continues_left = 6;
        tio_updater_set_cb_task_create(&updater, run_task, NULL);
        tio_updater_set_cb_task_continue(&updater, updater_continue, &task);
        tio_updater_set_cb_task_exit(&updater, updater_exit, &task);
        tio_updater_set_cb_delay_ms(&updater, updater_delay, &task);
        tio_updater_set_interval(&updater, interval);
        tio_author_t author = {};

        CHECK( tio_updater_start(&updater, &author, updater_state_size, &task, NULL, NULL) == TIO_ERR_OK );
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

    kii_clock_t clock;
    kii_clock_init(&clock, NULL, NULL);
    CHECK( tio_handler_set_clock(&handler, &clock) == TIO_ERR_FAIL );
    CHECK( tio_updater_set_clock(&updater, &clock) == TIO_ERR_FAIL );
    CHECK( handler._kii._clock == nullptr );
    CHECK( updater._kii._clock == nullptr );

    kii_clock_init(&clock, clock_now_ms, NULL);
    CHECK( tio_handler_set_clock(&handler, &clock) == TIO_ERR_OK );
    CHECK( tio_updater_set_clock(&updater, &clock) == TIO_ERR_OK );
    CHECK( handler._kii._clock == &clock );
    CHECK( updater._kii._clock == &clock );
}
