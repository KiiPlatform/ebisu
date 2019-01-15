Requilred tools are followings:

  * CC3200 SDK(1.3 or more)
  * Code Composer Studio IDE (CCS)

User guides for CC3200 are exist in [this site](http://www.tij.co.jp/tool/jp/cc3200sdk).
How to install these two tools are described in the document.

## Build sample application

```sh
make
```

After make, freertos_ebisu_demo folder is created.
Please import freertos_ebisu_demo folder as CCS Project from CCS.
Please replace KII_APP_ID, KII_APP_HOST, VENDOR_THING_ID and THING_PASSWORD in main.c to yours.
After these steps, you can build sample application on CCS.

