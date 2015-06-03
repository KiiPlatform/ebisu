# Desktop sample application on WinnerMicro HED10W07SN

## Set up options for target project
1. Copy ../../../../kii, ../../../../kii-core, ../../../../lib to current path
1. Add "DEBUG" defination in C/C++ tab  page if debug mode is needed
2. Add related "include paths" in C/C++ tab  page.
3. Add all kii related souce code to WinnerMicro project

## How to test
1. Use "t-connect("WIFIID","pwd")" to connect wifi network
2. Use "t-kii" to run example application

## Issues
1. Need to modify api "prv_kii_util_get_http_body"