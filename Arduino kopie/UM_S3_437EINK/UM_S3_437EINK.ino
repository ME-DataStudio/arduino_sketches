#include "config.h"
#include <GxEPD2.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_EPD.h>
#include <GxEPD2_GFX.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <time.h>

GxEPD2_4C < GxEPD2_437c, GxEPD2_437c::HEIGHT / 2 > display(GxEPD2_437c(/*CS=D8*/ CS, /*DC=D3*/ DC, /*RST=D4*/ RST, /*BUSY=D2*/ BUSY)); // Waveshare 4.37" 4-color


///////////////////////////////////////
// Helper functions:
// - drawDashboard()
// - connectWifi()
// - getState(String entity) - get state of Homeassistent entity
// - setupTime()
// - goToSleep()
// - 
//
///////////////////////////////////////

void drawDashboard(
  String outsideTemp,
  String livingTemp,
  String weather,
  String power)
{
  display.setFullWindow();
  display.firstPage();

  do
  {
    display.fillScreen(GxEPD_WHITE);

    display.setTextColor(GxEPD_BLACK);

    display.setCursor(20,40);
    display.setTextSize(2);
    display.print("HOME DASHBOARD");

    display.drawLine(
      20,50,
      490,50,
      GxEPD_BLACK);

    display.setCursor(20,90);
    display.print("Buiten:");
    display.print(outsideTemp);
    display.print(" C");

    display.setCursor(20,130);
    display.print("Woonkamer:");
    display.print(livingTemp);
    display.print(" C");

    display.setCursor(20,170);
    display.print("Weer:");
    display.print(weather);

    display.setCursor(20,210);
    display.print("Vermogen:");
    display.print(power);
    display.print(" W");

    time_t now;
    time(&now);

    struct tm* tm_info =
        localtime(&now);

    char buf[32];

    strftime(
      buf,
      sizeof(buf),
      "%d-%m-%Y %H:%M",
      tm_info);

    display.setCursor(20,330);
    display.print(buf);

  }
  while(display.nextPage());

  display.hibernate();
}

void connectWifi()
{
  WiFi.mode(WIFI_STA);

  WiFi.begin(
      WIFI_SSID,
      WIFI_PASS);

  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
}

String getState(String entity)
{
  HTTPClient http;

  String url =
      String(HA_URL) +
      "/api/states/" +
      entity;

  http.begin(url);

  http.addHeader(
      "Authorization",
      String("Bearer ") + HA_TOKEN);

  int code = http.GET();

  if(code != 200)
  {
    http.end();
    return "ERR";
  }

  String payload = http.getString();

  DynamicJsonDocument doc(4096);
  deserializeJson(doc, payload);

  String state = doc["state"];

  http.end();

  return state;
}

void setupTime()
{
  configTime(
      3600,
      3600,
      "pool.ntp.org");
}

void goToSleep()
{
  esp_sleep_enable_timer_wakeup(
      15ULL *
      60ULL *
      1000000ULL);

  esp_deep_sleep_start();
}

////////////////////////
//// setup en loop
////////////////////////
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  connectWifi();

  setupTime();

  String outsideTemp = getState(ENTITY_OUTSIDE_TEMP);

  String livingTemp = getState(ENTITY_LIVING_TEMP);

  String weather = getState(ENTITY_WEATHER);

  String power = getState(ENTITY_POWER);

  display.init();

  drawDashboard(
      outsideTemp,
      livingTemp,
      weather,
      power);

  //goToSleep();
}

void loop() {
  // put your main code here, to run repeatedly:

}
