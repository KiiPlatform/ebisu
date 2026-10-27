# ESP32 sample

tio on an ESP32, with [ESP-IDF](https://docs.espressif.com/projects/esp-idf/).
The thing onboards, uploads its state periodically, and logs the commands it
receives. It runs on a board over Wi-Fi, and in Espressif's QEMU over the
Ethernet QEMU emulates. CI runs it in QEMU against Kii Cloud.

- [main/app_main.c](main/app_main.c): sets up tio as the [Linux
  sample](../linux-sample/example.c) does, with its settings from `idf.py
  menuconfig`.
- [main/sock_cb_esp32.c](main/sock_cb_esp32.c): the socket callbacks, on
  ESP-IDF's esp-tls, which uses mbedTLS over lwIP.
- [main/sys_cb_esp32.c](main/sys_cb_esp32.c): tasks, delays and locks on
  FreeRTOS, and the monotonic clock on `esp_timer`.
- [components/ebisu](components/ebisu/CMakeLists.txt): ebisu's libraries, built
  from this repository's sources.

## The server's certificate

The server's certificate has to chain to one of the CA certificates ESP-IDF
bundles, and be issued for the host. ESP-IDF's mbedTLS does not check the
validity period itself: it would do so at the system's time, which is 1970 on
a device that has not set it. The socket callbacks check it after the
handshake instead, at the time a `kii_clock_t` keeps, which the device learns
from the server. Until the first response, the clock has only a floor, a time
it is known not to be earlier than, and only a certificate that expired before
it can be told. The [kii README](../../kii/README.md#the-time) describes the
clock.

## Building

With ESP-IDF v5.5 set up, from this directory:

```sh
idf.py set-target esp32
idf.py menuconfig    # "ebisu sample", and Wi-Fi under "Example Connection Configuration"
idf.py build flash monitor
```

Under "ebisu sample", set the application's ID and host, and the vendor thing
ID and password the thing onboards with. The state it uploads is JSON, its
traits' states by alias, and has to match how the application defines the
thing type.

ebisu's version header is generated from this repository's git history, as the
CMake build of the libraries does.

## Running in QEMU

QEMU has no Wi-Fi, but emulates an OpenCores Ethernet MAC, which
`sdkconfig.qemu` switches to. Its user-mode networking reaches the internet
without privileges.

```sh
idf.py -B build-qemu -D SDKCONFIG=build-qemu/sdkconfig \
    -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.qemu" build
cd build-qemu
python -m esptool --chip esp32 merge_bin --fill-flash-size 4MB -o flash.bin @flash_args
qemu-system-xtensa -nographic -machine esp32 -m 4M \
    -drive file=flash.bin,if=mtd,format=raw -nic user,model=open_eth
```

[tests/samples/esp32_qemu.py](../../tests/samples/esp32_qemu.py) is how CI runs
it:

- with the clock's floor after Kii Cloud's certificate expires, the sample has
  to refuse the certificate;
- against the application CI tests with, the sample onboards a thing of the
  run's own, its state reaches the thing's owner, and so does the result of a
  command the owner sends.
