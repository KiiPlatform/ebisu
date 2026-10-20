#include "catch.hpp"
#include "tio.h"

#include <limits.h>
#include <stdint.h>
#include <vector>

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

// MQTT carries a keep-alive of at most 65535 seconds, which
// kii_start_push_task() checks, but tio passes it the size_t it keeps as an
// unsigned int, so one past UINT_MAX would arrive there cut down.
TEST_CASE( "tio_handler_start keep-alive range" ) {
    tio_handler_t handler;
    tio_handler_init(&handler);
    int call_task_create = 0;
    tio_handler_set_cb_task_create(&handler, count_task_create, &call_task_create);
    tio_author_t author = {};

    for (size_t interval : { (size_t)0, (size_t)300, (size_t)65535 }) {
        INFO("keep-alive " << interval);
        call_task_create = 0;
        tio_handler_set_keep_alive_interval(&handler, interval);
        CHECK( tio_handler_start(&handler, &author, NULL, NULL) == TIO_ERR_OK );
        CHECK( call_task_create == 1 );
        CHECK( handler._kii._keep_alive_interval == interval );
    }

    std::vector<size_t> too_long = { 65536, 70000, UINT_MAX };
    if (SIZE_MAX > UINT_MAX) {
        // 300 once cut down to an unsigned int.
        too_long.push_back((size_t)UINT_MAX + 1 + 300);
    }
    for (size_t interval : too_long) {
        INFO("keep-alive " << interval);
        call_task_create = 0;
        tio_handler_set_keep_alive_interval(&handler, interval);
        CHECK( tio_handler_start(&handler, &author, NULL, NULL) == TIO_ERR_FAIL );
        CHECK( call_task_create == 0 );
    }
}
