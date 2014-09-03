#!/bin/sh

rm -rf ../../WM_SDK
cd ../../
mkdir WM_SDK
cd -

cd ../../WM_SDK
mkdir Bin
cd -

cp -rf ../../App ../../WM_SDK/App
cp -rf ../../Doc ../../WM_SDK/Doc
cp -rf ../../Include ../../WM_SDK/Include
cp -rf ../../Lib ../../WM_SDK/Lib
cp -rf ../../Src ../../WM_SDK/Src
cp -rf ../../Tools ../../WM_SDK/Tools
cp -rf ../../Bin/*.exe ../../WM_SDK/Bin

#delete some source code for lib
cd ../../WM_SDK/App
rm -rf wm_main.c
rm -rf oneshotconfig
rm -rf wm_atcmd
rm -rf .svn
cd -

cd ../../WM_SDK/App/demo
rm -rf .svn
cd -

cd ../../WM_SDK/App/dhcpserver
rm -rf .svn
cd -

cd ../../WM_SDK/App/dnsserver
rm -rf .svn
cd -

cd ../../WM_SDK/App/httpclient
rm -rf .svn
cd -

cd ../../WM_SDK/App/iperf
rm -rf .svn
cd -

cd ../../WM_SDK/App/iperf
rm -rf .svn
cd -

cd ../../WM_SDK/Doc
rm -rf .svn
cd -

cd ../../WM_SDK/Include
rm -rf .svn
cd -

cd ../../WM_SDK/Lib
rm -rf .svn
cd -

cd ../../WM_SDK/Src
rm -rf Sys
rm -rf Wlan
rm -rf .svn
cd -

cd ../../WM_SDK/Src/Boot
rm -rf .svn
cd -

cd ../../WM_SDK/Src/Common
rm -rf .svn
cd -

cd ../../WM_SDK/Src/Drivers
rm -rf .svn
cd -

cd ../../WM_SDK/Src
rm -rf Sys
rm -rf Wlan
rm -rf .svn
cd -


cd ../../WM_SDK/Bin
rm UA03_CreateImg.exe
rm -rf .svn
cd -

cd ../../WM_SDK/Src/Drivers
rm -rf litepoint
rm -rf .svn
cd -

cd ../../WM_SDK/Tools/Keil/SDKProject
rm -rf objs
rm -rf WM_SDK.map
rm -rf WM_SDK_WM_SDK.dep
rm -rf WM_SDK_uvopt.bak
rm -rf WM_SDK.bak
rm -rf WM_SDK_uvproj.bak
rm -rf mpu.lst
rm -rf os_cpu_a.lst
rm -rf start.lst
rm -rf .svn
cd -

rm -rf ../../WM_SDK/Tools/Keil/Project

cd ../../WM_SDK/Tools/Keil
mv SDKProject Project
cd -

cd ../../WM_SDK/Tools/Keil
rm -rf makelib
rm -rf packlib
rm -rf sdk.sh
rm -rf catlib
rm -rf goall
rm -rf makefile
rm -rf package.sh
cd -

cd ../../
DATE=$(date +%Y%m%d)
tar -cf WM_SDK_$DATE.tar WM_SDK
rm -rf WM_SDK