# FreeRTOS-Kernel for the tests' firmware, on the MPS2 AN385 board's
# Cortex-M3: the release a test names, checked against its hash, as the target
# freertos_kernel, which ebisu's CMake builds its FreeRTOS port for. Before
# ebisu_test_freertos_kernel(<version>), the caller makes freertos_config, the
# target with its FreeRTOSConfig.h, and sets FREERTOS_HEAP, the heap, if any.
#
# A macro, so that the caller has freertos_kernel_SOURCE_DIR, and the
# kernel's major version, in ebisu_test_kernel_major.

# The kernels' releases, checked against their hashes. Those before V11 came
# before the kernel had a repository and releases of its own: their tags'
# archives.
set(ebisu_kernel_sha256_V8.1.2 236e757f8c01c9fb178ccbcfe977db43dca62c8bb8771f486cf4e3cb36ca0efd)
set(ebisu_kernel_sha256_V9.0.0 0b29584de9dadb74bf294c5ded60f37ffcbc145c41135bc3e0d5302ea042bcb6)
set(ebisu_kernel_sha256_V10.0.1 2022cc66442ea1d4f0fd5037f3ccdaf03c12a64607dd317c207b6515bc2c461b)
set(ebisu_kernel_sha256_V11.1.0 eebd58aa71a623c9381f25f77b708c0ed14ef995a8913e2460fe9f286bb271eb)
set(ebisu_kernel_sha256_V11.3.3 dd728be9ecfdcea4121b89ed015dff77eb0d3fa8ce2bff4b660da748b298990f)

macro(ebisu_test_freertos_kernel version)
    if(NOT DEFINED ebisu_kernel_sha256_${version})
        message(FATAL_ERROR "No such kernel to test with: ${version}")
    endif()
    string(REGEX MATCH "^V([0-9]+)\\." ebisu_test_kernel_major ${version})
    set(ebisu_test_kernel_major ${CMAKE_MATCH_1})
    if(ebisu_test_kernel_major LESS 11)
        set(ebisu_kernel_url https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/refs/tags/${version}.tar.gz)
    else()
        set(ebisu_kernel_url https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/download/${version}/FreeRTOS-Kernel${version}.zip)
    endif()

    include(FetchContent)
    FetchContent_Declare(freertos_kernel
        URL ${ebisu_kernel_url}
        URL_HASH SHA256=${ebisu_kernel_sha256_${version}}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    # What FreeRTOS-Kernel's CMake asks of an application, besides
    # freertos_config and the heap: the port.
    set(FREERTOS_PORT GCC_ARM_CM3)
    FetchContent_MakeAvailable(freertos_kernel)

    # A kernel before V11 has no CMake of its own, its tag only the sources:
    # built here as an application's own build builds them, into a target of
    # the name ebisu's CMake looks for. Their vTaskStartScheduler() sets the
    # tick count to 0, not to configINITIAL_TICK_COUNT, as V10.1.0 does: their
    # tasks.c with that line as V10.1.0 has it, where the configuration sets
    # configINITIAL_TICK_COUNT, which they don't default. Stream buffers are
    # V10's.
    if(ebisu_test_kernel_major LESS 11)
        set(ebisu_kernel ${freertos_kernel_SOURCE_DIR}/FreeRTOS/Source)
        file(READ ${ebisu_kernel}/tasks.c ebisu_tasks_c)
        string(FIND "${ebisu_tasks_c}" "xTickCount = ( TickType_t ) 0U;" ebisu_at)
        if(ebisu_at EQUAL -1)
            message(FATAL_ERROR
                "${ebisu_kernel}/tasks.c doesn't set the tick count to 0 as V8.1.2's to V10.0.1's do")
        endif()
        string(REPLACE "xTickCount = ( TickType_t ) 0U;"
            "\n#ifdef configINITIAL_TICK_COUNT\nxTickCount = ( TickType_t ) configINITIAL_TICK_COUNT;\n#else\nxTickCount = ( TickType_t ) 0U;\n#endif\n"
            ebisu_tasks_c "${ebisu_tasks_c}")
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/kernel/tasks.c "${ebisu_tasks_c}")
        add_library(freertos_kernel STATIC
            ${CMAKE_CURRENT_BINARY_DIR}/kernel/tasks.c
            ${ebisu_kernel}/queue.c
            ${ebisu_kernel}/list.c
            ${ebisu_kernel}/timers.c
            ${ebisu_kernel}/event_groups.c
            ${ebisu_kernel}/portable/GCC/ARM_CM3/port.c)
        if(ebisu_test_kernel_major GREATER_EQUAL 10)
            target_sources(freertos_kernel PRIVATE ${ebisu_kernel}/stream_buffer.c)
        endif()
        if(FREERTOS_HEAP)
            target_sources(freertos_kernel PRIVATE
                ${ebisu_kernel}/portable/MemMang/heap_${FREERTOS_HEAP}.c)
        endif()
        target_include_directories(freertos_kernel SYSTEM PUBLIC
            ${ebisu_kernel}/include
            ${ebisu_kernel}/portable/GCC/ARM_CM3)
        target_link_libraries(freertos_kernel PUBLIC freertos_config)
    endif()
endmacro()
