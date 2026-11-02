# Cross-compiles for 32-bit ARM Linux with hardware floating point, armv7-a
# with VFPv3-D16, and runs what it builds under qemu-arm, so the tests can run
# on an x86-64 host.
#
# Needs arm-linux-gnueabihf-gcc and -g++ on the PATH (crossbuild-essential-armhf
# on Debian and Ubuntu) and qemu-arm (qemu-user) for the tests. For an older
# compiler, -DEBISU_WERROR=OFF keeps a warning it adds from failing the build;
# without an ARM C++ compiler, -DBUILD_TESTING=OFF leaves the tests out.
#
# A toolchain that doesn't know the target's root file system, such as one
# taken from a device's SDK, is given it with -DCMAKE_SYSROOT=<path>. Without
# it, the toolchain's own is used, as Debian's finds /usr/arm-linux-gnueabihf.
# OPENSSL_ROOT_DIR is where an OpenSSL built for the target is installed, for
# platform's OpenSSL port and the Linux sample.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard")
# -Wno-psabi: GCC 7.1 changed how some C++ types are passed on ARM, and GCC 6
# notes it wherever the tests pass one. Everything here is built by one
# compiler, so nothing is passed across the change.
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -Wno-psabi")

if(CMAKE_SYSROOT)
    set(target_root ${CMAKE_SYSROOT})
else()
    set(target_root /usr/arm-linux-gnueabihf)
endif()

# The target's headers and libraries only, never the host's; the host's
# programs.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
list(APPEND CMAKE_FIND_ROOT_PATH ${target_root})
if(OPENSSL_ROOT_DIR)
    list(APPEND CMAKE_FIND_ROOT_PATH ${OPENSSL_ROOT_DIR})
endif()

# CTest prefixes this to every test whose command is an executable target, so
# `ctest` runs the ARM binaries, finding their loader and libraries in the
# target's file system.
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-arm -L ${target_root})
