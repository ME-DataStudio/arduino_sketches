#include <secrets.h>
#include <WiFi.h>  
#include <HTTPClient.h>  
#include <ArduinoJson.h>  
#include <map>
#include <esp_sleep.h>
#include <RadioLib.h>
#include <SPI.h>

const char *http_endpoint = "http://homeassistant.local:8123/api/states/sensor.epaper_esp32s3_data";

// RTC memory
RTC_DATA_ATTR int rtcCounter = 0; // This will persist across deep sleep

typedef struct
{
  float temperature_inside;
  float humidity_inside;
  float temperature_outside;
  float humidity_outside;
  float wind_speed;
  String wind_direction;
  String weather_forecast_now;
  String time;
  String timestamp;
} HAData;

HAData haData;
//esp32-s3-box 
int gdo2Pin=10;
int sckPin=14;
int mosiPin=13;
int god1Pin=11;
int gdo0Pin=9;
int csnPin=12;

CC1101 radio = new Module(csnPin, gdo0Pin, RADIOLIB_NC, gdo2Pin);

String httpGETRequest(const char *serverName)
{
  WiFiClient client;
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(client, serverName);
  http.setAuthorizationType("Bearer");
  http.setAuthorization(token);  //secrets.h
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void getData()
{
  StaticJsonDocument<1024> doc; // change size if needed
  deserializeJson(doc, httpGETRequest(http_endpoint));
  haData.temperature_inside = doc["attributes"]["temperature_inside"].as<float>();
  haData.humidity_inside = doc["attributes"]["humidity_inside"].as<float>();
  haData.temperature_outside = doc["attributes"]["temperature_outside"].as<float>();
  haData.humidity_outside = doc["attributes"]["humidity_outside"].as<float>();
  haData.wind_speed = doc["attributes"]["wind_speed"].as<float>();
  haData.wind_direction = doc["attributes"]["wind_direction"].as<String>();
  haData.weather_forecast_now = doc["attributes"]["weather_forecast_now"].as<String>();
  haData.time = doc["attributes"]["time"].as<String>();
}

void sendBresser() {
  Serial.print(F("[CC1101] Transmitting packet ... "));

  // you can transmit C-string or Arduino string up to 255 characters long
  //int state = radio.transmit("AAAAAAAAAA2DD4AA5B2C10051218FFFFFF0008040616FFF0800000000000000000");
  int state = radio.transmit("AAAAAAAA2DD4AA5B2C10051218FFFFFF0008040616FFF0800000000000000000");
  

  // you can also transmit byte array up to 255 bytes long
  // With some limitations see here: https://github.com/jgromes/RadioLib/discussions/1138
  /*
    byte byteArr[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    int state = radio.transmit(byteArr, 8);
  */

  if (state == RADIOLIB_ERR_NONE) {
    // the packet was successfully transmitted
    Serial.println(F("success!"));

  } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
    // the supplied packet was longer than 255 bytes
    Serial.println(F("too long!"));

  } else {
    // some other error occurred
    Serial.print(F("failed, code "));
    Serial.println(state);

  }

  // wait for a second before transmitting again
  delay(1000);
}

/////////////////
/// setup & loop
/////////////////
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Sending to Bresser");
  
  ConfigFSK_t config;
  config.frequency = 868.3;              // MHz
  config.bitRate = 8.21;                 // kBaud
  config.frequencyDeviation = 57.136417; // kHz
  config.power = 10;               // dBm
  config.preambleLength = 32;            // bits
  config.receiverBandwidth = 270; 

  SPI.begin(sckPin, god1Pin, mosiPin, csnPin);
  pinMode(gdo2Pin, INPUT);
  Serial.printf("SPI.begin(SCK=%d, MISO=%d, MOSI=%d, CS=%d)\n", sckPin, god1Pin, mosiPin, csnPin);
  int state = radio.begin(config);
  WiFi.begin(ssid, password); // secrets.h - Connect to the network
  //Serial.println("\nConnecting to WiFi Network ..");

  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(100);
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
  
    // initialize CC1101 with default settings
  Serial.print(F("[CC1101] Initializing ... "));
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }
}

void loop()
{
  haData.humidity_outside = 0.0;
  while (haData.humidity_outside < 10.0) {
      getData();
  }
  //sendBresser();
  
  //ESP.deepSleep(10 * 60 * 1000000); // sleep 10 minutes will enter setup() again
  for (int x = 1; x < 5; x = x + 1) {
    Serial.print(F("[CC1101] Transmitting packet ... "));

  // you can transmit C-string or Arduino string up to 255 characters long
  //String str = "Hello World! #" + String(count++);
  //int state = radio.transmit(str);

  // you can also transmit byte array up to 255 bytes long with some limitations; https://github.com/jgromes/RadioLib/discussions/1138
  //0xaa,0xaa,0xaa,0xaa,0xaa,0x2d,
      byte byteArr[] = {0xaa,0xaa,0xaa,0xaa,0x2d,0xd4,0xC4,0x24,0xC1,0x30,0x04,0x32,0x18,0xff,0xff,0xff,0x11,0x28,0xff,0xfd,0xff,0xff,0x01,0x8F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
      int state = radio.transmit(byteArr, 8);
  

    if (state == RADIOLIB_ERR_NONE) {
    // the packet was successfully transmitted
      Serial.println(F("success!"));

    } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
    // the supplied packet was longer than 255 bytes
      Serial.println(F("too long!"));

    } else {
    // some other error occurred
      Serial.print(F("failed, code "));
      Serial.println(state);
    }

  // wait for a second before transmitting again
  delay(30000);
  } //end for loop  
}