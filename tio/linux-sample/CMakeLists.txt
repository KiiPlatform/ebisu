# The linux sample, previously built by a hand-written Makefile that hardcoded
# -L/usr/local/opt/openssl/lib and -I/usr/local/opt/openssl/include. That is the
# Intel-homebrew prefix, so the sample could not link on an Apple Silicon mac,
# where it is /opt/homebrew. find_package finds it wherever it lives.
#
# The system and the sockets are platform's POSIX and OpenSSL ports, which build
# where Threads and OpenSSL are found.
if(NOT TARGET ebisu_posix OR NOT TARGET ebisu_openssl)
    message(FATAL_ERROR "The linux sample needs POSIX threads and OpenSSL")
endif()

add_executable(exampleapp example.c)

target_include_directories(exampleapp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(exampleapp PRIVATE tio ebisu_posix ebisu_openssl)
# gnu11 rather than the libraries' strict C99: the sample is an application
# and not part of them. It uses <stdatomic.h>, which is C11, and POSIX --
# sigaction, usleep -- which glibc declares only with GNU extensions on.
ebisu_configure_target(exampleapp C_STANDARD 11)
