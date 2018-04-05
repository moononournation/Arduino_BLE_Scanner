/*
   Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
   Ported to Arduino ESP32 by Evandro Copercini
*/

#define WIFI_SSID "YOURAPSSID"
#define WIFI_PASSWORD "YOURAPPASSWORD"
#define POST_URL "http://YOURSERVERNAMEORIP:3000/"
#define SCAN_TIME  30 // seconds
#define WAIT_WIFI_LOOP 5 // around 4 seconds for 1 loop
#define SLEEP_TIME  300 // seconds
// comment the follow line to disable serial message
#define SERIAL_PRINT

#include <Arduino.h>
#include <sstream>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <esp_wifi.h>

#include <HTTPClient.h>

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

WiFiMulti wifiMulti;
std::stringstream ss;
bool data_sent = false;
int wait_wifi_counter = 0;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
#ifdef SERIAL_PRINT
      Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
#endif
    }
};

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  esp_wifi_stop();

#ifdef SERIAL_PRINT
  Serial.begin(115200);
  Serial.println("ESP32 BLE Scanner");
#endif

  BLEDevice::init("");

  // put your main code here, to run repeatedly:
  BLEScan *pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(0x50);
  pBLEScan->setWindow(0x30);

#ifdef SERIAL_PRINT
  Serial.printf("Start BLE scan for %d seconds...\n", SCAN_TIME);
#endif

  BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME);
  int count = foundDevices.getCount();
  ss << "[";
  for (int i = 0; i < count; i++)
  {
    if (i > 0) {
      ss << ",";
    }
    BLEAdvertisedDevice d = foundDevices.getDevice(i);
    ss << "{\"Address\":\"" << d.getAddress().toString() << "\",\"Rssi\":" << d.getRSSI();

    if (d.haveName())
    {
      ss << ",\"Name\":\"" << d.getName() << "\"";
    }

    if (d.haveAppearance())
    {
      ss << ",\"Appearance\":" << d.getAppearance();
    }

    if (d.haveManufacturerData())
    {
      std::string md = d.getManufacturerData();
      uint8_t* mdp = (uint8_t*)d.getManufacturerData().data();
      char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
      ss << ",\"ManufacturerData\":\"" << pHex << "\"";
      free(pHex);
    }

    if (d.haveServiceUUID())
    {
      ss << ",\"ServiceUUID\":\"" << d.getServiceUUID().toString() << "\"" ;
    }

    if (d.haveTXPower())
    {
      ss << ",\"TxPower\":" << (int)d.getTXPower();
    }

    ss << "}";
  }
  ss << "]";

#ifdef SERIAL_PRINT
  Serial.println("Scan done!");
#endif

  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
}

void loop()
{
  // wait for WiFi connection
  if ((wifiMulti.run() == WL_CONNECTED))
  {
#ifdef SERIAL_PRINT
    Serial.println("WiFi Connected");
#endif
    // HTTP POST BLE list
    HTTPClient http;

#ifdef SERIAL_PRINT
    Serial.println("Payload:");
    Serial.println(ss.str().c_str());
    Serial.println("[HTTP] begin...");
#endif

    // configure traged server and url
    http.begin(POST_URL);

    // start connection and send HTTP header
    int httpCode = http.POST(ss.str().c_str());

    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
#ifdef SERIAL_PRINT
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
#endif

      // file found at server
      if (httpCode == HTTP_CODE_OK)
      {
#ifdef SERIAL_PRINT
        Serial.println(http.getString());
#endif
      }
    }
    else
    {
#ifdef SERIAL_PRINT
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
#endif
    }

    http.end();
    data_sent = true;
  }

  // wait WiFi connected
  if (data_sent || (wait_wifi_counter > WAIT_WIFI_LOOP)) {
    esp_sleep_enable_timer_wakeup(SLEEP_TIME * 1000000); // translate second to micro second

#ifdef SERIAL_PRINT
    Serial.printf("Enter deep sleep for %d seconds...\n", SLEEP_TIME);
#endif

    esp_wifi_stop();
    esp_deep_sleep_start();
  } else {
    wait_wifi_counter++;

#ifdef SERIAL_PRINT
    Serial.printf("Waiting count: %d\n", wait_wifi_counter);
#endif
  }
}
