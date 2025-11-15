#include <Arduino.h>
#include "Ubidots.h"

// https://help.ubidots.com/en/articles/4855281-connect-your-esp32-to-ubidots-over-http-tcp-or-udp
// https://ubidots.com

const char *UBIDOTS_TOKEN = "...";
const char *WIFI_SSID = "...";
const char *WIFI_PASS = "...";
const char *DEVICE_LABEL_TO_RETRIEVE_VALUES_FROM = "weather-station"; // Replace with your device label
const char *VARIABLE_LABEL_TO_RETRIEVE_VALUES_FROM = "humidity";      // Replace with your variable label

// Create a pointer to a instance of the Ubidots class to use it globally
Ubidots *ubidots{nullptr};

void setup()
{
  Serial.begin(115200);
  Ubidots::wifiConnect(WIFI_SSID, WIFI_PASS);
  ubidots = new Ubidots(UBIDOTS_TOKEN, UBI_HTTP);
  // ubidots->setDebug(true); // Uncomment this line for printing debug messages
}

void loop()
{
  ///////////////SET
  float value1 = random(0, 9) * 10;
  float value2 = random(0, 9) * 100;
  float value3 = random(0, 9) * 1000;
  ubidots->add("Variable_Name_One", value1); // Change for your variable name
  ubidots->add("Variable_Name_Two", value2);
  ubidots->add("Variable_Name_Three", value3);

  bool bufferSent = false;
  bufferSent = ubidots->send(); // Will send data to a device label that matches the device Id

  if (bufferSent)
  {
    // Do something if values were sent properly
    Serial.println("Values sent by the device");
  }

  ///////////////GET
  /* Obtain last value from a variable as float using HTTP */
  float value = ubidots->get(DEVICE_LABEL_TO_RETRIEVE_VALUES_FROM, VARIABLE_LABEL_TO_RETRIEVE_VALUES_FROM);

  // Evaluates the results obtained
  if (value != ERROR_VALUE)
  {
    Serial.print("Value:");
    Serial.println(value);
  }

  delay(5000);
}