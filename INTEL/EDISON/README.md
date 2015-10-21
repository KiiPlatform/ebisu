#Intel Edison Example

####Prerequests :
1. Intel Edison Board that is connected to your local network. Please refer to [Official Intel getting started tutorial] (https://software.intel.com/en-us/iot/library/edison-getting-started) with eclipse as IDE.
2. This example optionally requires additional hardware which is a push button that is attached on the gpio. If you don't have, the sample will still run but won't have hardware triggered demonstration. 
3. Kii Developer account and a Kii apps created. You can create a new one or just reuse your existing Kii apps.
4. Create a Thing object on Kii developer portal, don't forget to remember the 'thing' password since we will use it on the code.


####Installation Steps.
1. Update git sub modules and run install script

 ```bash
 cd ../../
 git submodule init
 git submodule update
 cd INTEL/EDISON
 sh install_dep.sh
 ```
2. Run eclipse Intel IOT Developer Kit
3. Switch workspace by clinking `file`->`switch workspace`->`other..` and point to this repository `eclipse_workspace` folder.
4. Import `example` project by clicking `file`->`import`->`existing Projects Into Workspace`, browse to your current workspace and select only `example`
5. Edit `example.c` const bellow with your Kii Apps information (see above pre requests) 
```c
// Kii Apps information, please get the information on Developer Portal
const char EX_APP_SITE[] = "JP";//Your app kii site (JP/US/SG/CN/CN3)
const char EX_APP_ID[] = "your_app_id";
const char EX_APP_KEY[] = "your_app_key";

// Example's Thing informations, please register it on Developer Portal
const char EX_AUTH_VENDOR_ID[] = "sweet_wks_2"; // fill with value you have created on pre request step 4
const char EX_AUTH_VENDOR_PASS[] = "password"; // fill with value you have created on pre request step 4
const char EX_AUTH_VENDOR_TYPE[] = "my_type"; // optional, you can fill any string
```
6. Build and run into your connected edison target.
7. Confirm your successful runtime by opening Developer Portal Object console. Your data should be saved on the cloud once you hit refresh.


