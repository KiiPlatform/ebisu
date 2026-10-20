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
    tio_author_t author = {};

    REQUIRE( tio_updater_start(&updater, &author, NULL, NULL, NULL, NULL) == TIO_ERR_CREATE_TASK );
}
