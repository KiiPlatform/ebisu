#include <kii.h>
#include <kii_def.h>
#include <kii_device.h>
#include <kii_extension.h>
#include <kii_hal.h>
#include <kii_mqtt.h>
#include <kii_object.h>
#include <kii_push.h>
#include <light.h>
#include <light_if.h>

#include <LTask.h>
#include <LWiFi.h>
#include <LWiFiClient.h>

//#define WIFI_AP "KiiChina2" // replace with your setting
//#define WIFI_PWD "9999@Kii@9999" // replace with your setting
#define WIFI_AP "TP-LINK_3dworld" // replace with your setting
#define WIFI_PWD "3dw3dw3dw" // replace with your setting

extern kii_push_struct g_kii_push;


void setup() {
  LTask.begin();
  LWiFi.begin();
  Serial.begin(115200);
  
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);              // wait for a second
  digitalWrite(13, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);
  digitalWrite(13, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);              // wait for a second
  digitalWrite(13, LOW);    // turn the LED off by making the voltage LOW

  Serial.println("Connecting to AP...");
  if (LWiFi.connectWPA(WIFI_AP, WIFI_PWD) < 0)
  {
    Serial.println("Connect to AP failed!");
    return;
  }
  Serial.println("Connected to AP");
  light_init();
}

void loop() {
    kiiPush_pingReq();
    kiiPush_recvMsg();
}
