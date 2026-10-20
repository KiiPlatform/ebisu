# Cross-compiles for 64-bit Windows with mingw-w64, and runs what it builds
# under wine, so the tests can run on a Linux host.
#
# 64-bit Windows is LLP64: long is 32 bits while pointers and size_t are 64.
# Neither of the other builds has that combination -- 64-bit Linux has a 64-bit
# long, and on 32-bit x86 everything is 32 bits -- so code that holds a size_t
# or a pointer in a long, or prints one as a long, only shows up here.
#
# Used by the mingw configure preset. Needs x86_64-w64-mingw32-gcc and -g++ on
# the PATH (g++-mingw-w64-x86-64-posix on Debian and Ubuntu, mingw64-gcc-c++ on
# Fedora) and wine to run the tests.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)

# CTest prefixes this to every test whose command is an executable target,
# which all of ebisu_add_test_suite's are, so `ctest` runs the .exe files under
# wine. Wine maps the working directory through, so the khc suite still finds
# its fixtures under ./data.
set(CMAKE_CROSSCOMPILING_EMULATOR wine)

# Otherwise the test executables need libstdc++-6.dll, libgcc_s_seh-1.dll and,
# where the toolchain uses POSIX threads, libwinpthread-1.dll at run time. Wine
# looks for those beside the .exe and on its own PATH, where they are not, and
# where they live on the host differs between distributions. A test that cannot
# find them exits with status 53, and under WINEDEBUG=-all says nothing about
# why. Linked statically there is nothing to find.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
