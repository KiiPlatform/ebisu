# ebisu - Embedded IoT Suite

`ebisu` provides access to `Kii IoT cloud` to the limited resource devices.

`ebisu` consists of following components.

## tio

You may mainly use this module when you implement your IoT devices connected to Kii.

The module provides following functionalities

- Upload state data read from sensors.
- Receive remote control command via MQTT and propagate it to the application on the devices.

For more details, please refer to [./tio/README.md](./tio/README.md)

## kii

While `tio` covers basic functionality required by IoT devices, `kii` provides

generic cloud data storage functionalities.

### Key-Value storage

Write/ read arbitrary key-value structured data to/ from Kii Cloud.

### File storage

Write/ read arbitrary binary data to/ from Kii Cloud.

For example, you can use these storage to store configuration data.

For more details, please refer to [./kii/README.md](./kii/README.md)

## khc

`khc` is a simple HTTP client library.

The module is developed to implement `tio` and `kii`.

In most cases, you don't have to use this module directly in you IoT device application.

For more details, please refer to [./khc/README.md](./khc/README.md)

## jkii

`jkii` is a simple json parser library.

The module is developed to implement `tio` and `kii`.

You can use this module to parse json encoded string or you can choose other 3rd party libraries.

For more details, please refer to [./jkii/README.md](./jkii/README.md)

## core

`core` is what the other modules are written against: socket, system and lock
interfaces, which the device implements, a clock that keeps the time of day on
a device without a real-time one, a logger interface, and the bounded string,
number and memory utilities they all use. It is a library of its own,
`libebisu_core`, which they link.

For more details, please refer to [./core/README.md](./core/README.md)

## platform

`platform` holds the ports, which implement `core`'s interfaces for a kind of
system each: POSIX threads and OpenSSL on Linux and macOS, and the Win32 API
and Schannel on Windows, as static libraries to link.

For more details, please refer to [./platform/README.md](./platform/README.md)

## API references

Details of API are available in [API references](https://kiiplatform.github.io/ebisu/),
for each major version.

## Breaking changes

[UPGRADING.md](./UPGRADING.md) lists what an application has to change or check
for each major version. For 3.0, every application changes how it gives ebisu
its sockets and its system, which are objects set with one call each, and most
then need to delete a setter that did nothing, and a rebuild.

## Building

The project is a single CMake project, which needs CMake 3.15. Configure and
build presets, which need 3.21, cover the usual variants:

```
cmake --preset debug          # or release, or asan
cmake --build --preset debug
```

Tests run through CTest, labelled by cost:

```
ctest --preset small          # self-contained, parallel, no credentials
ctest --preset large          # against a live Kii application
ctest --preset broker         # against a Mosquitto broker, see below
ctest --preset all
```

The large tests need `APP_ID` and `DEFAULT_SITE` in the environment; without
them they are not configured at all and `ctest --preset small` still works. A
new test application is bootstrapped with `tests/large_test/initapp.sh`.
Cross-compiled for 64-bit Windows with `cmake --preset mingw`, they run under
wine with `ctest --preset large-mingw`, over the win32 sample's Winsock and
Schannel callbacks in place of OpenSSL.

The broker tests run the MQTT task, with an endpoint of their own, against a
Mosquitto broker that `tests/broker_test/with_broker.py` starts in a container
for the run, with docker, or with podman given `DOCKER=podman`. They need no
credentials, and are configured with `-DEBISU_TEST_BROKER=ON`:

```
cmake --preset debug -DEBISU_TEST_BROKER=ON
cmake --build --preset debug --target broker_test_kii
tests/broker_test/with_broker.py ctest --preset broker
```

They connect over the platform's socket, as the large tests do: platform's
OpenSSL port, or its Schannel port on Windows, where they run under wine with
`ctest --preset broker-mingw`. Schannel trusts the Windows root store, so the
tests add the run's CA to the root store of the wine prefix they run in, and
remove it when they end. They refuse to run on a real Windows machine.

For 32-bit ARM Linux, armv7-a with hardware floating point, the toolchain file
is `cmake/arm-linux-gnueabihf.cmake`, and the small tests run under qemu-arm.
The libraries need only the C library. platform's OpenSSL port and the Linux
sample also need an OpenSSL built for the target, named with
`-DOPENSSL_ROOT_DIR`:

```
cmake -B build/armhf -DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabihf.cmake \
    -DOPENSSL_ROOT_DIR=/path/to/arm/openssl -DEBISU_BUILD_SAMPLES=ON
cmake --build build/armhf
ctest --test-dir build/armhf -L small
```

For another ARM target, copy the toolchain file and change its compiler and
flags. With an older compiler, `-DEBISU_WERROR=OFF` keeps a warning it adds from
failing the build, and without a C++ compiler for the target,
`-DBUILD_TESTING=OFF` leaves the tests out. `tests/samples/armhf.sh` builds it
all in Debian stretch, glibc 2.24, against OpenSSL 1.0.2 or 1.1.1, as CI does.

Other useful targets and options:

```
cmake --build --preset debug --target docs        # doxygen, into build/<preset>/ebisu-doc
ctest --preset small --test-dir build/debug -T memcheck  # under valgrind
ctest --preset asan-small                         # under ASan and UBSan
cmake --preset fuzz && cmake --build --preset fuzz && tests/fuzz/run.sh 60  # fuzz each target for 60 seconds, with clang
cmake --preset debug -DEBISU_BUILD_SAMPLES=ON     # also build tio/linux-sample
```

### Packaging

The libraries are built as strict ISO C99, `-std=c99 -pedantic-errors`, and
warnings are errors by default, so a warning cannot quietly reappear. A compiler
newer than the one this was last checked against may report something we have
never seen and stop your build. Turn the option off rather than patching the
source; it turns `-pedantic-errors` into `-pedantic` as well:

```
cmake -S . -B build -DEBISU_WERROR=OFF
```

`cmake --install` puts the libraries, their headers, a CMake package and a
pkg-config file for each library under the prefix. The libraries are
`libebisu_core`, `libebisu_jkii`, `libebisu_khc`, `libebisu_kii` and
`libebisu_tio`, each with its own code alone, linking the ones below it:

```
cmake --install build --prefix /opt/ebisu
```

A CMake project then finds the package, whose targets are `ebisu::core`,
`ebisu::khc`, `ebisu::jkii`, `ebisu::kii` and `ebisu::tio`, and, where they were
built, `ebisu::posix` and `ebisu::openssl`, the ports. Each brings in the
libraries it links:

```cmake
find_package(ebisu 3 CONFIG REQUIRED)
target_link_libraries(app PRIVATE ebisu::openssl ebisu::posix ebisu::tio)
```

Any other build asks pkg-config, for `ebisu-core`, `ebisu-khc`, `ebisu-jkii`,
`ebisu-kii`, `ebisu-tio`, `ebisu-posix` and `ebisu-openssl`, each of which
requires what its library links. The ports are static, so they come before the
library:

```
cc app.c $(pkg-config --cflags --libs ebisu-openssl ebisu-posix ebisu-tio)
```

which, for the libraries, gives `-lebisu_tio -lebisu_kii -lebisu_khc
-lebisu_jkii -lebisu_core`, the order a link by hand against the static
libraries needs too.

`tests/samples/package_consumer.sh` builds a program both ways against an
installed copy, as CI does.

### Code size

For a device short of flash, `-DEBISU_MIN_SIZE=ON` compiles the libraries with
`-Os`, and each function and object in a section of its own, so that a link with
`--gc-sections` leaves out what nothing uses. The shared libraries are linked so.
A program linking the static libraries has to pass it to its own link, as most
embedded builds already do. The static libraries are also built without
position-independent code, unless `-DCMAKE_POSITION_INDEPENDENT_CODE=ON` asks
for it, say to link them into a shared library of the application's.
CMake's `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` adds link-time optimization,
and a program linking the static libraries then links with `-flto` too:

```
cmake -B build/small -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SHARED_LIBS=OFF \
    -DEBISU_MIN_SIZE=ON -DBUILD_TESTING=OFF
```

`-DEBISU_LOG=OFF` leaves out what ebisu logs itself, messages and all: the
ports' diagnostics, the requests khc makes and what it gets back, kii's MQTT
events and what tio fails at. With GCC at `-Os` on x86-64, the OpenSSL port's
code goes from 6.5 KB to 5.2 KB, the Schannel port's from 7.1 KB to 5.3 KB,
khc's from 15.7 KB to 14.5 KB, kii's from 39.2 KB to 38.7 KB and tio's from
8.4 KB to 7.3 KB. A build without CMake defines `EBISU_NO_LOG` for the same. A
socket's `last_error` still says why an operation failed, and an application's
loggers still take its own messages.

`tests/samples/size_report.sh` builds the libraries each way and reports their
size, and that of a thing for a Cortex-M3 linked against them, as CI does. For
that thing, with GCC 13, `EBISU_MIN_SIZE` takes the text, code and constant
data, from about 61 KB to 54 KB, newlib-nano included, and link-time
optimization to 52 KB.

### macOS

Install OpenSSL:

```
$ brew install openssl
```

Setting `LDFLAGS`/`CPPFLAGS` by hand is no longer necessary: the build locates
OpenSSL with `find_package`, which works on both Apple Silicon and Intel
prefixes. If you have it somewhere unusual, point CMake at it with
`-DOPENSSL_ROOT_DIR=...`.

## License

ebisu is under the [Apache License 2.0](./LICENSE). [NOTICE](./NOTICE) lists the
third-party code it includes, under those projects' own licenses.
