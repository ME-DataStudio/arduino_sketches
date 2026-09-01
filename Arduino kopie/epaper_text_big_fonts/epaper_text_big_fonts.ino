#include <secrets.h>
#include <Adafruit_GFX.h> 
#include <fonts/GothamRoundedBook.h>
#include <fonts/GothamRoundedBold.h>
#include <fonts/GothamRoundedBoldBig.h>
#include <glyphs/weather.h>
#include <glyphs/icons.h>
#include <glyphs/weather_small.h>
#include <GxEPD2_4C.h>  
#include <WiFi.h>  
#include <HTTPClient.h>  
#include <ArduinoJson.h>  
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <ArduinoJson.h>
#include <map>
#include <esp_sleep.h>
#include <Wire.h>

#define HAS_VBUS_SENSE 1

// epaper Definitions
#define BUSY 17
#define RST 7
#define DC 6
#define CS 10
#define CLK 12
#define MOSI 11
//#define BUTTON_PIN 2  // New: Button for display inversion
//#define EPD_WIDTH 512
//#define EPD_HEIGHT 368
// set borders of display inside frame
#define OFFSET_LEFT 10
#define OFFSET_TOP 10
#define OFFSET_RIGHT 10
#define OFFSET_BOTTOM 20

GxEPD2_4C < GxEPD2_437c, GxEPD2_437c::HEIGHT> epd(GxEPD2_437c(/*CS=D8*/ CS, /*DC=D3*/ DC, /*RST=D4*/ RST, /*BUSY=D2*/ BUSY)); // Waveshare 4.37" 4-color

const char *http_endpoint = "http://homeassistant.local:8123/api/states/sensor.epaper_esp32s3_data";

const char *title = "WEER";
const size_t GLYPH_SIZE_WEATHER = 100;
const size_t GLYPH_SIZE_WEATHER_SMALL = 45;

enum class MAX17048_REG
{
    VCELL = 0x02,
    SOC = 0x04,
    MODE = 0x06,
    VERSION = 0x08,
    HIBRT = 0x0A,
    CONFIG = 0x0C,
    VALRT = 0x14,
    CRATE = 0x16,
    VRESET_ID = 0x18,
    STATUS = 0x1A,
    TABLE = 0x40,
    CMD = 0xFE
};
const uint8_t I2C_ADDR = 0x36;
float charge;

typedef const unsigned char *weather_icon;
std::map<std::string, weather_icon> weather_icon_map{
    {"clear-night", weather_clear_night},
    {"cloudy", weather_cloudy},
    {"fog", weather_foggy},
    {"hail", weather_thunderstorm},
    {"lightning", weather_thunderstorm},
    {"lightning-rainy", weather_thunderstorm},
    {"partlycloudy", weather_partly_cloudy},
    {"night-partly-cloudy", weather_partly_cloudy_night},
    {"pouring", weather_rainy},
    {"rainy", weather_rainy},
    {"snowy", weather_snowing},
    {"snowy-rainy", weather_snowing},
    {"sunny", weather_sunny},
    {"windy", weather_wind},
    {"windy-variant", weather_wind}};

typedef const unsigned char *weather_icon;
std::map<std::string, weather_icon> weather_icon_map_small{
    {"clear-night", weather_small_clear_night},
    {"cloudy", weather_small_cloudy},
    {"fog", weather_small_foggy},
    {"hail", weather_small_thunderstorm},
    {"lightning", weather_small_thunderstorm},
    {"lightning-rainy", weather_small_thunderstorm},
    {"partlycloudy", weather_small_partly_cloudy},
    {"night-partly-cloudy", weather_small_partly_cloudy_night},
    {"pouring", weather_small_rainy},
    {"rainy", weather_small_rainy},
    {"snowy", weather_small_snowing},
    {"snowy-rainy", weather_small_snowing},
    {"sunny", weather_small_sunny},
    {"windy", weather_small_wind},
    {"windy-variant", weather_small_wind}};

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
  String weather_forecast_2h;
  float weather_forecast_2h_temp;
  String weather_forecast_2h_time;
  String weather_forecast_4h;
  float weather_forecast_4h_temp;
  String weather_forecast_4h_time;
  String weather_forecast_6h;
  float weather_forecast_6h_temp;
  String weather_forecast_6h_time;
  String weather_forecast_8h;
  float weather_forecast_8h_temp;
  String weather_forecast_8h_time;
  String time;
  String timestamp;
} HAData;

HAData haData;

void epdInit() {
  epd.init(115200, true, 50, false);
  epd.setRotation(0);
  epd.setFullWindow();
  Serial.println("E-paper initialized");
}

const unsigned char *get_weather_icon(String forecast, bool small = false)
{
  if (small)
  {
    if (weather_icon_map_small.find(forecast.c_str()) != weather_icon_map_small.end())
    {
      return weather_icon_map_small[forecast.c_str()];
    }
    else
    {
      return weather_small_sunny;
    }
  }
  else
  {
    if (weather_icon_map.find(forecast.c_str()) != weather_icon_map.end())
    {
      return weather_icon_map[forecast.c_str()];
    }
    else
    {
      return weather_sunny;
    }
  }
}

void printForecast(int offset_x, int offset_y, weather_icon icon, float temperature, String time)
{
  int kleur;
  epd.setFont(&GothamRounded_Book14pt8b);
  epd.setCursor(OFFSET_LEFT + offset_x, OFFSET_TOP + offset_y);
  epd.print(time);

  kleur = GxEPD_BLACK;
  if (temperature > 30) {
     kleur = GxEPD_RED;
  }
  if (temperature > 25 && temperature < 31) {
     kleur = GxEPD_YELLOW;
  }
  epd.drawBitmap(OFFSET_LEFT + offset_x - 13, OFFSET_TOP + offset_y + 8, icon, GLYPH_SIZE_WEATHER_SMALL, GLYPH_SIZE_WEATHER_SMALL, kleur);

  if (temperature > 9.99)
  {
    //epd.setCursor(OFFSET_LEFT + offset_x + 10, OFFSET_TOP + offset_y + 10 + GLYPH_SIZE_WEATHER_SMALL + 28);
    epd.setCursor(OFFSET_LEFT + offset_x + 30, OFFSET_TOP + offset_y + 40);
  }
  else
  {
    epd.setCursor(OFFSET_LEFT + offset_x + 27, OFFSET_TOP + offset_y + 40);
  }
  epd.setFont(&GothamRounded_Book14pt8b);
  epd.printf("%.0f°C", temperature);
}

void writeDisplay()
{
  epd.setRotation(0);

  epd.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  epd.setFont(&GothamRounded_Bold32pt7b); // title
  epd.getTextBounds(title, 0, 0, &tbx, &tby, &tbw, &tbh);

  epd.firstPage();
  int kleur;
  do
  {
    // print Title
    //epd.setCursor(((epd.width() - tbw) / 2) - tbx - 100 , OFFSET_TOP + 60);
    //epd.print(title);
    // time
    

    // print temperature and humidity ///////////////////
    epd.setFont(&GothamRounded_Bold32pt7b);    
    size_t weatherdetails_offset = 40;
    weather_icon weather_icon = get_weather_icon(haData.weather_forecast_now,false);
    kleur = GxEPD_BLACK;
    if (haData.temperature_outside > 30) {
      kleur = GxEPD_RED;
    }
    if (haData.temperature_outside > 25 && haData.temperature_outside < 31) {
      kleur = GxEPD_YELLOW;
    }
    epd.drawBitmap(OFFSET_LEFT + 100, OFFSET_TOP + weatherdetails_offset - GLYPH_SIZE_WEATHER / 2, weather_icon, GLYPH_SIZE_WEATHER, GLYPH_SIZE_WEATHER, kleur);
    epd.setCursor(OFFSET_LEFT + 200, OFFSET_TOP + weatherdetails_offset + GLYPH_SIZE_WEATHER / 4);
    epd.printf("%.1f°C", haData.temperature_outside);

    epd.setFont(&FreeMonoBold24pt7b);
    epd.drawBitmap(OFFSET_LEFT + 150, OFFSET_TOP + 100 + weatherdetails_offset - GLYPH_SIZE_WEATHER / 2, icon_humidity, GLYPH_SIZE_WEATHER_SMALL, GLYPH_SIZE_WEATHER_SMALL, GxEPD_BLACK);
    epd.setCursor(OFFSET_LEFT + 200, OFFSET_TOP + 60 + weatherdetails_offset + GLYPH_SIZE_WEATHER / 4);
    epd.printf("%.1f%%", haData.humidity_outside);

    // print wind ////////////////////
    epd.setFont(&FreeMonoBold24pt7b);
    weatherdetails_offset = 150;
    epd.drawBitmap(OFFSET_LEFT + 60, OFFSET_TOP + weatherdetails_offset - 10 , weather_small_wind, GLYPH_SIZE_WEATHER_SMALL, GLYPH_SIZE_WEATHER_SMALL, GxEPD_BLACK);
    epd.setCursor(OFFSET_LEFT + 260, OFFSET_TOP + weatherdetails_offset + GLYPH_SIZE_WEATHER / 4);
    epd.printf("%.1fkm/h", haData.wind_speed);

    epd.setCursor(OFFSET_LEFT + 130, OFFSET_TOP + weatherdetails_offset + GLYPH_SIZE_WEATHER / 4);
    epd.printf("%s", haData.wind_direction);
        
    // living room temperature
    weatherdetails_offset = 200;
    epd.drawBitmap(OFFSET_LEFT , OFFSET_TOP + weatherdetails_offset, icon_living_room, 80, 80, GxEPD_BLACK);
    epd.drawBitmap(OFFSET_LEFT + 90, OFFSET_TOP + weatherdetails_offset + 30, icon40_thermometer, 40, 40, GxEPD_BLACK);
    epd.drawBitmap(OFFSET_LEFT + 280, OFFSET_TOP + weatherdetails_offset+ 30, icon40_humidity, 40, 40, GxEPD_BLACK);

    epd.setFont(&FreeMonoBold24pt7b);
    epd.setCursor(OFFSET_LEFT + 125, OFFSET_TOP + weatherdetails_offset + 60);
    epd.printf("%.1f°C", haData.temperature_inside);

    epd.setCursor(OFFSET_LEFT + 310, OFFSET_TOP + weatherdetails_offset + 60);
    epd.printf("%.1f%%", haData.humidity_inside);

    // timestamp and battery charge
    epd.setFont(&FreeMonoBold12pt7b);
    epd.setCursor(10, OFFSET_TOP + 330);
    epd.printf("Stand op %s", haData.time.c_str());
    epd.setCursor( 300, OFFSET_TOP + 330);
    epd.printf("Charge %i%%", int((charge/3.7 * 100)));

    epd.drawFastHLine(0, 200, 512, GxEPD_BLACK);
    epd.drawFastHLine(0, 310, 512, GxEPD_BLACK);
  } while (epd.nextPage());
}

String httpGETRequest(const char *serverName)
{
  WiFiClient client;
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(client, serverName);
  //http.begin(serverName);
  // Send HTTP POST request
  http.setAuthorizationType("Bearer");
  http.setAuthorization(token);  
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
  //haData.weather_forecast_2h = doc["attributes"]["weather_forecast_2h"].as<String>();
  //haData.weather_forecast_2h_temp = doc["attributes"]["weather_forecast_2h_temp"].as<float>();
  //haData.weather_forecast_2h_time = doc["attributes"]["weather_forecast_2h_time"].as<String>();
  //haData.weather_forecast_4h = doc["attributes"]["weather_forecast_4h"].as<String>();
  //haData.weather_forecast_4h_temp = doc["attributes"]["weather_forecast_4h_temp"].as<float>();
  //haData.weather_forecast_4h_time = doc["attributes"]["weather_forecast_4h_time"].as<String>();
  //haData.weather_forecast_6h = doc["attributes"]["weather_forecast_6h"].as<String>();
  //haData.weather_forecast_6h_temp = doc["attributes"]["weather_forecast_6h_temp"].as<float>();
  //haData.weather_forecast_6h_time = doc["attributes"]["weather_forecast_6h_time"].as<String>();
  //haData.weather_forecast_8h = doc["attributes"]["weather_forecast_8h"].as<String>();
  //haData.weather_forecast_8h_temp = doc["attributes"]["weather_forecast_8h_temp"].as<float>();
  //haData.weather_forecast_8h_time = doc["attributes"]["weather_forecast_8h_time"].as<String>();
  haData.time = doc["attributes"]["time"].as<String>();
  //haData.timestamp = doc["attributes"]["timestamp"].as<String>();
  charge = getBatteryVoltage();
  charge = ( charge > 3.7 ) ? 3.7 : charge;
}

///////////////////
/// Battery functions
///////////////////
float getBatteryVoltage()
{
    return ((float)i2c_read(MAX17048_REG::VCELL) * 78.125f / 1000000.f);
}

bool getVbusPresent()
{
    return digitalRead(VBUS_SENSE);
}

/* I2C communication for MAX17048 FG*/
void i2c_write(const MAX17048_REG reg)
{
    Wire.beginTransmission(I2C_ADDR);
    Wire.write((uint8_t)reg);
    Wire.endTransmission();
}

void i2c_write(const MAX17048_REG reg, const uint16_t data)
{
    Wire.beginTransmission(I2C_ADDR);
    Wire.write((uint8_t)reg);
    Wire.write((data & 0xFF00) >> 8);
    Wire.write((data & 0x00FF) >> 0);
    Wire.endTransmission();
}

uint16_t i2c_read(const MAX17048_REG reg)
{
    i2c_write(reg);
    Wire.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2); // 2byte R/W only
    uint16_t data = (uint16_t)((Wire.read() << 8) & 0xFF00);
    data |= (uint16_t)(Wire.read() & 0x00FF);
    return data;
}

// Gets the battery voltage and shows it using the neopixel LED.
// These values are all approximate, you should do your own testing and
// find values that work for you.
void checkBattery()
{
    // Get the battery voltage, corrected for the on-board voltage divider
    // Full should be around 4.2v and empty should be around 3v
    float battery = getBatteryVoltage();

    if (getVbusPresent())
    {
        
        Serial.printf("Running from 5V - Battery: %fV\n", battery);
    }
    else
    {
        
        Serial.printf("Running from Battery: %fV\n", battery);
    }
}

/////////////////
/// setup & loop
/////////////////
void setup() {
  Serial.begin(115200);
  delay(1000);
  //Serial.println("Weather Display Booting...");
  
  SPI.begin(CLK, MISO, MOSI, CS);
  Wire.begin();
  WiFi.begin(ssid, password); // Connect to the network
  //Serial.println("\nConnecting to WiFi Network ..");

  while(WiFi.status() != WL_CONNECTED){
    //Serial.print(".");
    delay(100);
  }

  //Serial.println("\nConnected to the WiFi network");
  //Serial.print("Local ESP32 IP: ");
  //Serial.println(WiFi.localIP());

  rtcCounter++;
  //Serial.println(rtcCounter);
  if (rtcCounter % 12 == 0) // first load or every 20th time
  {
    // init if first start
    epdInit();
  }
  else
  {
    // initial false for re-init after processor deep sleep wake up, if display power supply was kept
    // this can be used to avoid the repeated initial full refresh on displays with fast partial update
    epd.init(115200, false, 50, false); // initial = false
    epd.setPartialWindow(0, 0, epd.width(), epd.height());
  }
}

void loop()
{
  haData.humidity_outside = 0.0;
  while (haData.humidity_outside < 10.0) {
      getData();
  }
  writeDisplay();
  epd.powerOff();
  ESP.deepSleep(10 * 60 * 1000000); // sleep 10 minutes will enter setup() again
   
  delay(200);
}