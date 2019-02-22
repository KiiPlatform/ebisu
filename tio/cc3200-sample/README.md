# Sample application on CC3200

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
  1. Import freertos\_ebisu\_demo folder as CCS Project from CCS.
  1. Replace KII\_APP\_ID, KII\_APP\_HOST, VENDOR\_THING\_ID and THING\_PASSWORD in main.c to yours.
  1. Build sample application on CCS.

### SSL Connection

* This demo application use HTTP. Current CC3200 can not use HTTPS for Kii Cloud server, because supported cipher is old.

