# The MQTT task against a Mosquitto broker of the run's own. They need the
# broker, which tests/broker_test/with_broker.py starts in a container, so they
# have a label of their own, and run with `ctest --preset broker` under it.
if(NOT TARGET ebisu_openssl)
    message(FATAL_ERROR "The broker tests need platform's OpenSSL port")
endif()

ebisu_add_test_suite(broker_test_kii
    LABEL broker
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCES broker_test.cpp
    LINK ${EBISU_KII_OBJS} ebisu_openssl Threads::Threads
    INCLUDES ${PROJECT_SOURCE_DIR}/kii/include)
