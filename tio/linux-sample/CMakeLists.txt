# The linux sample, previously built by a hand-written Makefile that hardcoded
# -L/usr/local/opt/openssl/lib and -I/usr/local/opt/openssl/include. That is the
# Intel-homebrew prefix, so the sample could not link on an Apple Silicon mac,
# where it is /opt/homebrew. find_package finds it wherever it lives.

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(exampleapp
    example.c
    sock_cb_linux.c
    sys_cb_linux.c
    linux-env/task_impl.c)

target_include_directories(exampleapp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(exampleapp
    PRIVATE tio OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
ebisu_configure_target(exampleapp)
