#include "catch.hpp"
#include "tio.h"

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
