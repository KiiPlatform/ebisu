# Desktop sample application on MTK LinkIt-ONE platform

## How to build
1. Copy the following files to path libraries/kii:
   ../../../kii/* (except kii_push.c)
   ../../../kii-core/*
   ../../../lib/jsmn/*
2. Open file demo.ino with arduino and add library "kii"
3. If you want to DEBUG mode, you need to define "DEBUG" in file "kii.h"

## How to test
1. Modify WIFI_AP and WIFI_PWD in file "demo.ino" such as the follow:
   #define WIFI_AP "xxx" // replace with your setting
   #define WIFI_PWD "xxx" // replace with your setting
2. Build and upload firmware