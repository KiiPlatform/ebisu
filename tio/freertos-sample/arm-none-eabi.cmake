# arm-none-eabi-gcc, for a Cortex-M3 without an operating system.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m3)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m3 -mthumb")

# A program cannot link without the board's start-up code and linker script, so
# CMake checks the compiler with a library.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
