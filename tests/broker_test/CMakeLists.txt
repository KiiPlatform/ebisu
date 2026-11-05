# The MQTT task against a Mosquitto broker of the run's own. They need the
# broker, which tests/broker_test/with_broker.py starts in a container, so they
# have a label of their own, and run with `ctest --preset broker` under it.
#
# Over the platform's socket, as the large tests: platform's Schannel port on
# Windows, with crypt32 for the root store the run's CA is added to, and its
# OpenSSL port elsewhere.
if(WIN32)
    set(broker_test_socket_libs ebisu_schannel crypt32)
else()
    if(NOT TARGET ebisu_openssl)
        message(FATAL_ERROR "The broker tests need platform's OpenSSL port")
    endif()
    set(broker_test_socket_libs ebisu_openssl)
endif()

ebisu_add_test_suite(broker_test_kii
    LABEL broker
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCES broker_test.cpp
    LINK ${EBISU_KII_OBJS} ${broker_test_socket_libs} Threads::Threads
    INCLUDES ${PROJECT_SOURCE_DIR}/kii/include)
