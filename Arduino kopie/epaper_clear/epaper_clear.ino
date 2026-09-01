#include <GxEPD2.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_EPD.h>
#include <GxEPD2_GFX.h>

#include <time.h>

// Pin Definitions
//#define PWR 7
#define BUSY 17
#define RST 7
#define DC 6
#define CS 10
#define CLK 12
#define MOSI 11

bool invertDisplay = false;  // Current display state

GxEPD2_4C < GxEPD2_437c, GxEPD2_437c::HEIGHT / 2 > epd(GxEPD2_437c(/*CS=D8*/ CS, /*DC=D3*/ DC, /*RST=D4*/ RST, /*BUSY=D2*/ BUSY)); // Waveshare 4.37" 4-color

void epdInit() {
  epd.init(115200, true, 50, false);
  epd.setRotation(0);
  epd.setTextColor(invertDisplay ? GxEPD_WHITE : GxEPD_BLACK);
  epd.setFullWindow();
  Serial.println("E-paper initialized");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Weather Display Booting...");
  SPI.begin(CLK, MISO, MOSI, CS);
  epdInit();
  // Draw display
  Serial.println("Drawing to e-paper...");
  epd.fillScreen(GxEPD_WHITE);
  
  epd.display();
  epd.hibernate();
  //epdPower(LOW);
 
  Serial.println("Entering deep sleep...");
  //esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 0); // Wake on button press
  //esp_sleep_enable_timer_wakeup(900LL * 1000000); // 15 min
  //esp_deep_sleep_start();
}

void loop() {
  // Empty - device will be in deep sleep
} 