# Sample application on CC3200

## Security

**Do not use this sample's socket callbacks as they are in a product.** It
connects to Kii Cloud and to the MQTT broker over plain HTTP and MQTT, without
TLS, so nothing it sends is encrypted, its tokens and credentials included,
and the server is not authenticated: see [SSL Connection](#ssl-connection). A
product has to use TLS and verify the server's certificate: its chain to a
trusted CA, the host name it is issued for, and its validity period.
The [kii README](../../kii/README.md#the-time) describes how to check the
certificate at the right time on a device without a real-time clock, and the
[Linux](../linux-sample/sock_cb_linux.c) and
[Win32](../win32-sample/sock_cb_win32.c) samples' socket callbacks do so.

## Required tools

We built and tested this sample appliation on Windows 10.
Requilred tools are followings:

  * CC3200 SDK(1.3 or more)
  * Code Composer Studio IDE (CCS)

User guides for CC3200 are exist in [this site](http://www.tij.co.jp/tool/jp/cc3200sdk).
How to install these two tools are described in the document.

## Build sample application

The build steps are followings:

  1. Run make command on this folder. freertos\_ebisu\_demo folder is created.
     It also writes ebisu\_version.h there, with the version `git describe` gives,
     or 0.0.0 outside a git clone: run it where `sh` and `git` are found.
  1. Import freertos\_ebisu\_demo folder as CCS Project from CCS.
  1. Replace KII\_APP\_ID, KII\_APP\_HOST, VENDOR\_THING\_ID and THING\_PASSWORD in main.c to yours.
  1. Build sample application on CCS.

### SSL Connection

* This demo application use HTTP. Current CC3200 can not use HTTPS for Kii Cloud server, because supported cipher is old.

