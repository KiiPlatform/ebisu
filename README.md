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

### khc

`khc` is a simple HTTP client library.

The module is developed to implement `tio` and `kii`.

In most cases, you don't have to use this module directly in you IoT device application.

For more details, please refer to [./kii/README.md](./kii/README.md)

## jkii

`jkii` is a simple json parser library.

The module is developed to implement `tio` and `kii`.

You can use this module to parse json encoded string or you can choose other 3rd party libraries.

For more details, please refer to [./jkii/README.md](./jkii/README.md)

## API references

Details of API are available in [API references](https://kiiplatform.github.io/ebisu-doc).

## Building

The project is a single CMake project. Configure and build presets cover the
usual variants:

```
cmake --preset debug          # or release, or asan
cmake --build --preset debug
```

Tests run through CTest, labelled by cost:

```
ctest --preset small          # self-contained, parallel, no credentials
ctest --preset large          # against a live Kii application
ctest --preset all
```

The large tests need `APP_ID` and `DEFAULT_SITE` in the environment; without
them they are not configured at all and `ctest --preset small` still works. A
new test application is bootstrapped with `tests/large_test/initapp.sh`.

Other useful targets and options:

```
cmake --build --preset debug --target docs        # doxygen, into build/<preset>/ebisu-doc
ctest --preset small -T memcheck                  # under valgrind
ctest --preset asan-small                         # under ASan and UBSan
cmake --preset debug -DEBISU_BUILD_SAMPLES=ON     # also build tio/linux-sample
```

### macOS

Install OpenSSL:

```
$ brew install openssl
```

Setting `LDFLAGS`/`CPPFLAGS` by hand is no longer necessary: the build locates
OpenSSL with `find_package`, which works on both Apple Silicon and Intel
prefixes. If you have it somewhere unusual, point CMake at it with
`-DOPENSSL_ROOT_DIR=...`.
