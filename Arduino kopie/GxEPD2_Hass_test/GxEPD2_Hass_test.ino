#include "secrets.h"
#include <GxEPD2_4C.h>  
#include <WiFi.h>  
#include <HTTPClient.h>  
#include <ArduinoJson.h>  
#include <Fonts/FreeMonoBold9pt7b.h>
// epaper Definitions
#define BUSY 12
#define RST 14
#define DC 6
#define CS 5
#define CLK 11
#define MOSI 10
#define MISO 7
int updateteller;
bool einde;
  
// For Waveshare 4.37" 4-color display (512x368)
// Set correct PINS and use SPI to set MOSI and CLK
GxEPD2_437c display(/*CS=*/CS, /*DC=*/DC, /*RST=*/RST, /*BUSY=*/BUSY);  
GxEPD2_4C<GxEPD2_437c, GxEPD2_437c::HEIGHT> gfx(display);

void setup() {
  Serial.begin(115200);
  delay(2000);
 
  Serial.println("437in4C Display Booting...");
  SPI.begin(CLK, MISO, MOSI, CS); // remap hspi for EPD (swap pins)  
  // Initialize display (use 2ms reset for Waveshare boards with "clever" reset)  
  gfx.init(115200, true, 2, false);  
    
  // Connect to WiFi  
  WiFi.begin(ssid,password);  
  while (WiFi.status() != WL_CONNECTED) {  
    delay(1000);  
  }  
  updateteller = 0;
  einde = false;
}

String getHAData(String entity_id) {  
  HTTPClient http;  
  String url = "http://homeassistant.local:8123/api/states/" + entity_id;  
    
  http.begin(url);  
  //http.addHeader("Authorization", "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiI1MmViMzlmNmQzYTY0OGU0OTRmYjYxZDhmNmRiNzUyNCIsImlhdCI6MTc3MDc1MTQ0MiwiZXhwIjoyMDg2MTExNDQyfQ.Git5jYG2KHkToHtPFZB1T7Rh_AF8n6Eqg0guzXoIv6I");  
  http.setAuthorizationType("Bearer");
  http.setAuthorization("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiI1MmViMzlmNmQzYTY0OGU0OTRmYjYxZDhmNmRiNzUyNCIsImlhdCI6MTc3MDc1MTQ0MiwiZXhwIjoyMDg2MTExNDQyfQ.Git5jYG2KHkToHtPFZB1T7Rh_AF8n6Eqg0guzXoIv6I");
  http.addHeader("Content-Type", "application/json");  
    
  int httpCode = http.GET();  
  String payload = "";  
    
  if (httpCode == HTTP_CODE_OK) {  
    payload = http.getString();  
  }  
  http.end();  
  return payload;  
}

void displayHomeAssistantData() {  
  gfx.firstPage();  
  do {  
    gfx.fillScreen(GxEPD_WHITE);  
      
    // Title in black  
    gfx.setTextColor(GxEPD_BLACK);  
    gfx.setFont(&FreeMonoBold9pt7b);  
    gfx.setCursor(10, 30);  
    gfx.println("Home Assistant");  
      
    // Temperature in red if high, yellow if normal  
    String gasData = getHAData("sensor.gas_meter_gasverbruik");  
    DynamicJsonDocument doc(1024);  
    deserializeJson(doc, gasData);  
    float gas = doc["state"];  
      
    
    gfx.setTextColor(GxEPD_BLACK);   
    gfx.setCursor(10, 80);  
    gfx.println("Gasverbruik: " + String(gas) + "m3");  
      
    // Humidity in blue (maps to red on 4-color)  
    // Temperature in red if high, yellow if normal  
    String swData = getHAData("sensor.stookwijzer_advies_code");  
    //DynamicJsonDocument doc(1024);  
    deserializeJson(doc, swData);  
    String adviescode = doc["state"];  
  
    if (adviescode="code_red") {
      gfx.setTextColor(GxEPD_RED);
      gfx.setCursor(10, 120);  
      gfx.println("Stookwijzer Adviescode: Rood");    
    } else if (adviescode="code_orange") {
      gfx.setTextColor(GxEPD_YELLOW);  
      gfx.setCursor(10, 120);  
      gfx.println("Stookwijzer Adviescode: Oranje");    
    } else {
      gfx.setTextColor(GxEPD_BLACK);
      gfx.setCursor(10, 120);  
      gfx.println("Stookwijzer Adviescode: Geel");    
    }
      
  } while (gfx.nextPage());  
}

void loop() {  
  if (!einde) {
    displayHomeAssistantData();  
    delay(300000); // Update every 5 minutes  
    updateteller++;
  } else {
    gfx.clearScreen();
    gfx.
  }
  if (updateteller > 4){
    einde = true;
  }
}