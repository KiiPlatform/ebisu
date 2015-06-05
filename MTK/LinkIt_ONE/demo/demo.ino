#include <LTask.h>
#include <LWiFi.h>
#include <LWiFiClient.h>

#include <example.h>
#include <jsmn.h>
#include <kii.h>
#include <kii_core.h>
#include <kii_core_impl.h>
#include <kii_json.h>
#include <kii_libc_wrapper.h>
#include <kii_mqtt.h>
#include <kii_socket_callback.h>
#include <kii_socket_impl.h>
#include <kii_task_callback.h>
#include <kii_task_impl.h>
#include <kii_util.h>

//#define WIFI_AP "KiiChina2" // replace with your setting
//#define WIFI_PWD "9999@Kii@9999" // replace with your setting
#define WIFI_AP "TP-LINK_3dworld" // replace with your setting
#define WIFI_PWD "3dw3dw3dw" // replace with your setting



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
  kiiDemo_test(NULL);
}

void loop() {
}
