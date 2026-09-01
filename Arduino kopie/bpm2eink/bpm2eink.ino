#include "secrets.h"
#include <GxEPD2.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_EPD.h>
#include <GxEPD2_GFX.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
//#include <ArduinoJson.h>
//#include <Fonts/FreeMonoBold9pt7b.h>
//#include <time.h>
//#include "imagedata.h"

// epaper SPI unexpected maker esp32s3D 
#define BUSY 12
#define RST 14
#define DC 6
#define CS 5
#define CLK 11
#define MOSI 10
#define MISO 7

// Global Variables
RTC_DATA_ATTR bool rtcInvertDisplay = false;  // Persists across deep sleep
bool invertDisplay = false;  // Current display state

GxEPD2_4C < GxEPD2_437c, GxEPD2_437c::HEIGHT / 2 > epd(GxEPD2_437c(/*CS=D8*/ CS, /*DC=D3*/ DC, /*RST=D4*/ RST, /*BUSY=D2*/ BUSY)); // Waveshare 4.37" 4-color

// Network and API Configuration

// display
const int screenW = 512, screenH = 368;

bool connectToWiFi() {
  Serial.printf("Trying WiFi: %s\n", ssid);
  WiFi.begin(ssid, password); //from secrets.h
    
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
    Serial.print(".");
  }
    
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected to %s\n", ssid);
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\nConnection failed");
    WiFi.disconnect();
    delay(1000);
  }
  return false;
}

/****************************
*
* Display routines and variables
*
*****************************/
static const uint16_t input_buffer_pixels = 800; // may affect performance

static const uint16_t max_row_width = 1872; // for up to 7.8" display 1872x1404
static const uint16_t max_palette_pixels = 256; // for depth <= 8

uint8_t input_buffer[3 * input_buffer_pixels]; // up to depth 24
uint8_t output_row_mono_buffer[max_row_width / 8]; // buffer for at least one row of b/w bits
uint8_t output_row_color_buffer[max_row_width / 8]; // buffer for at least one row of color bits
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w
uint16_t rgb_palette_buffer[max_palette_pixels]; // palette buffer for depth <= 8 for buffered graphics, needed for 7-color display

void epdInit() {
  epd.init(115200, true, 50, false);
  epd.setRotation(0);
  epd.setFullWindow();
  Serial.println("E-paper initialized");
}

uint32_t skip(WiFiClient& client, int32_t bytes)
{
  int32_t remain = bytes;
  uint32_t start = millis();
  while ((client.connected() || client.available()) && (remain > 0))
  {
    if (client.available())
    {
      client.read();
      remain--;
    }
    else delay(1);
    if (millis() - start > 2000) break; // don't hang forever
  }
  return bytes - remain;
}

uint32_t read8n(WiFiClient& client, uint8_t* buffer, int32_t bytes)
{
  int32_t remain = bytes;
  uint32_t start = millis();
  while ((client.connected() || client.available()) && (remain > 0))
  {
    if (client.available())
    {
      int16_t v = client.read();
      *buffer++ = uint8_t(v);
      remain--;
    }
    else delay(1);
    if (millis() - start > 2000) break; // don't hang forever
  }
  return bytes - remain;
}

uint16_t read16(WiFiClient& client)
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read(); // MSB
  return result;
}

uint32_t read32(WiFiClient& client)
{
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read();
  ((uint8_t *)&result)[2] = client.read();
  ((uint8_t *)&result)[3] = client.read(); // MSB
  return result;
}

void fetchPicture() {
  bool with_color = true;
  WiFiClient client;
  HTTPClient http;
  String url = "http://192.168.1.242:1880/pic4Eink.bmp";
 
  bool connection_ok = false;
  bool valid = false; // valid format to be handled
  Serial.println(); Serial.print("downloading file \""); Serial.print(url);  Serial.println("\"");
  
  // start http over wifi-client
  http.begin(client, url);
  http.setAuthorization("mark", "brug2Heaven!");
  int httpCode = http.GET();
  if (httpCode == 200) {
    Serial.println("request ok");    
    Serial.println(read16(client));
    //BPM signature = 0x4D42 = 19778 (dec)
  
    uint32_t fileSize = read32(client);
    uint32_t creatorBytes = read32(client);
    uint32_t imageOffset = read32(client); // Start of image data
    uint32_t headerSize = read32(client);
    uint32_t width  = read32(client);
    int32_t height = (int32_t) read32(client);
    uint16_t planes = read16(client);
    uint16_t depth = read16(client); // bits per pixel
    uint32_t format = read32(client);
    uint32_t bytes_read = 7 * 4 + 3 * 2; // read so far
    Serial.print("File size: "); Serial.println(fileSize);
    Serial.print("Image Offset: "); Serial.println(imageOffset);
    Serial.print("Header size: "); Serial.println(headerSize);
    Serial.print("Bit Depth: "); Serial.println(depth);
    Serial.print("Image size: ");
    Serial.print(width);
    Serial.print('x');
    Serial.println(abs(height));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  SPI.begin(CLK, MISO, MOSI, CS);
  epdInit();
  delay(5000);
  epd.fillScreen(invertDisplay ? GxEPD_BLACK : GxEPD_WHITE);
  epd.display();

  // Attempt WiFi connection
  bool wifiConnected = connectToWiFi();

  if (!wifiConnected) {
    Serial.println("Failed to connect to any network");
  } else {
    fetchPicture();
  }
  
  // epd.display();
  // epd.hibernate();
  
  // Serial.println("Entering deep sleep...");
  // esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 0); // Wake on button press
  // esp_sleep_enable_timer_wakeup(900LL * 1000000); // 15 min
  // esp_deep_sleep_start();
}

void loop() {
  // Empty - device will be in deep sleep
} 