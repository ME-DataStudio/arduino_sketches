#include "secrets.h"
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
//#include "imagedata.h"

// Network and API Configuration
const int MAX_NETWORKS = 1;
const char* ssid[MAX_NETWORKS] = {ssid}; //secrets.h
const char* password[MAX_NETWORKS] = {password}; //secrets.h
const char* apiKey = "openweathermap_api_key"; //secrets.h
const float latitude = 53.174145180943;  // for Ohrid
const float longitude = 6.6305010088279; // for Ohrid

// Pin Definitions
//#define PWR 7
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

// Global Variables
RTC_DATA_ATTR bool rtcInvertDisplay = false;  // Persists across deep sleep
bool invertDisplay = false;  // Current display state


GxEPD2_4C < GxEPD2_437c, GxEPD2_437c::HEIGHT / 2 > epd(GxEPD2_437c(/*CS=D8*/ CS, /*DC=D3*/ DC, /*RST=D4*/ RST, /*BUSY=D2*/ BUSY)); // Waveshare 4.37" 4-color

const int screenW = 400, screenH = 300;
const int graphBottom = 278, graphTop = 160, graphHeight = graphBottom - graphTop;
const int todayTop = 20, todayBottom = 138, todayHeight = todayBottom - todayTop;
const int todayWidth = screenW / 2 - 40, todayX = screenW - todayWidth - 8;

const int currentWeatherLeft = 7;
const int currentWeatherRight = todayX - 19;
const int currentWeatherWidth = currentWeatherRight - currentWeatherLeft;
const int currentWeatherTop = todayTop;
const int currentWeatherHeight = todayHeight;

// Weather Data Storage
int lastUpdateHour = -1;
String currentWeatherDesc = "";
float currentTemp = 0.0;
float currentPressure = 0.0;
int currentHumidity = 0;
int currentDayIndex = 0;

String daysOfWeek[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
float hourlyRain[24] = {0};
float dailyRain[6][8] = {0};
int dailyPop[6] = {0};

bool connectToWiFi() {
  for (int i = 0; i < MAX_NETWORKS; i++) {
    Serial.printf("Trying WiFi %d/%d: %s\n", i+1, MAX_NETWORKS, ssid[i]);
    WiFi.begin(ssid[i], password[i]);
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(250);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\nConnected to %s\n", ssid[i]);
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      return true;
    }
    Serial.println("\nConnection failed");
    WiFi.disconnect();
    delay(1000);
  }
  return false;
}

//void epdPower(int state) {
//  pinMode(PWR, OUTPUT);
//  digitalWrite(PWR, state);
//}

void epdInit() {
  epd.init(115200, true, 50, false);
  epd.setRotation(0);
  epd.setTextColor(invertDisplay ? GxEPD_WHITE : GxEPD_BLACK);
  epd.setFont(&FreeMonoBold9pt7b);
  epd.setFullWindow();
  Serial.println("E-paper initialized");
}

void drawImagedata(const unsigned char *Image, word xstart, word ystart, word image_width, word image_height) {
  // specific for 4.37in 4c from waveshare. Sending byte to display means coloring 4 pixels as once
  
  {
    word Width, Height, i, j;
    Width = (GxEPD2_437c::WIDTH % 4 == 0)? (GxEPD2_437c::WIDTH / 4 ): (GxEPD2_437c::WIDTH / 4 + 1);
    Height = GxEPD2_437c::HEIGHT;
    for(i=0; i<Height; i++) {
      for(j=0; j< Width; j++) {
        if(i<image_height+ystart && i>=ystart && j<(image_width+xstart)/4 && j>=xstart/4) {
          epd.drawPixel(i,j,pgm_read_byte(&Image[(j-xstart/4) + (image_width/4*(i-ystart))]));
        }
			  else {
				  epd.drawPixel(i,j,0x55);
			  }
		  }
    }
  }
}

int currentHour() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) return timeinfo.tm_hour;
  return 0;
}

void drawCurrentWeatherBox(String weatherDesc, float temperature, float pressure, int humidity, int updateHour) {
  uint16_t fgColor = invertDisplay ? GxEPD_WHITE : GxEPD_BLACK;
  epd.setTextColor(fgColor);
  
  // Draw the box
  epd.drawRect(currentWeatherLeft, currentWeatherTop, currentWeatherWidth, currentWeatherHeight, fgColor);
  epd.drawRect(currentWeatherLeft+1, currentWeatherTop+1, currentWeatherWidth-2, currentWeatherHeight-2, fgColor);
  
  // Draw label
  epd.setCursor(currentWeatherLeft + 5, currentWeatherTop - 5);
  epd.print("Current Weather");

  // Draw divider lines (dashed)
  for (int i = 1; i <= 3; i++) {
    int y = currentWeatherTop + i * currentWeatherHeight / 4;
    for (int x = currentWeatherLeft + 1; x < currentWeatherRight - 1; x += 4) {
      epd.drawPixel(x, y, fgColor);
      epd.drawPixel(x+1, y, fgColor);
    }
  }

  // Calculate text positions
  int rowHeight = currentWeatherHeight / 4;
  int textYOffset = rowHeight / 2 + 5;

  // Row 1: Weather description
  epd.setCursor(currentWeatherLeft + 5, currentWeatherTop + rowHeight * 0 + textYOffset);
  epd.print(weatherDesc);

  // Row 2: Temperature
  epd.setCursor(currentWeatherLeft + 5, currentWeatherTop + rowHeight * 1 + textYOffset);
  epd.print("Temp: ");
  epd.print(temperature, 1);
  epd.print(" °C");

  // Row 3: Pressure
  epd.setCursor(currentWeatherLeft + 5, currentWeatherTop + rowHeight * 2 + textYOffset);
  epd.print("Press: ");
  epd.print(pressure, 1);
  epd.print(" hPa");

  // Row 4: Humidity and Update time
  epd.setCursor(currentWeatherLeft + 5, currentWeatherTop + rowHeight * 3 + textYOffset);
  epd.print("Humid: ");
  epd.print(humidity);
  epd.print(" %");
  
  char updateStr[10];
  sprintf(updateStr, "UT %d", updateHour);
  int textWidth = 6 * strlen(updateStr);
  epd.setCursor(currentWeatherRight - textWidth - 35, currentWeatherTop + rowHeight * 3 + textYOffset);
  epd.print(updateStr);
}

void drawTodayBox(int currentHour) {
  uint16_t fgColor = invertDisplay ? GxEPD_WHITE : GxEPD_BLACK;
  epd.setTextColor(fgColor);

  epd.setCursor(todayX + 5, todayTop - 5);
  epd.print(daysOfWeek[currentDayIndex]);

  // Show POP percentage
  epd.setCursor(todayX + todayWidth - 90, todayTop - 5);
  char popStr[10];
  sprintf(popStr, "POP %d%%", dailyPop[0]);
  epd.print(popStr);

  epd.drawRect(todayX, todayTop, todayWidth, todayHeight, fgColor);
  epd.drawRect(todayX + 1, todayTop + 1, todayWidth - 2, todayHeight - 2, fgColor);

  // Draw hour markers
  for (int h = 6; h <= 18; h += 6) {
    int x = todayX + map(h, 0, 24, 4, todayWidth - 4);
    for (int y = todayTop; y < todayBottom; y += 4)
      epd.drawPixel(x, y, fgColor);
  }

  // Draw horizontal grid lines
  for (int i = 1; i <= 3; i++) {
    int y = todayTop + i * todayHeight / 4;
    for (int x = todayX + 1; x < todayX + todayWidth - 1; x += 4)
      epd.drawPixel(x, y, fgColor);
  }

  float maxRain = 0.1;
  for (int i = currentHour; i < 24; i++)
    if (hourlyRain[i] > maxRain) maxRain = hourlyRain[i];
  float roundedMax = getRoundedMax(maxRain);

  epd.setCursor(todayX - 15, todayTop + 10);
  epd.print((int)(roundedMax));

  epd.setCursor(todayX - 15, todayBottom);
  epd.print("0");

  bool hasRain = false;
  for (int h = currentHour; h < 24; h++) {
    int barHeight = map(hourlyRain[h] * 10, 0, roundedMax * 10, 0, todayHeight - 5);
    if (barHeight > 0) {
      hasRain = true;
      int x = todayX + map(h, 0, 24, 4, todayWidth - 4);
      
   //   for (int w = 0; w < 10; w++) {
      for (int w = 0; w < 15; w++) {   // So Podebeli Barovi

        
        if (x + w < todayX + todayWidth - 1)
          epd.drawFastVLine(x + w, todayBottom - barHeight, barHeight, fgColor);
      }
    }
  }

  if (!hasRain) {
    int midX = todayX + todayWidth / 2 - 10;
    int midY = todayTop + todayHeight / 2;
    epd.setCursor(midX, midY - 5);
    epd.print("NO");
    epd.setCursor(midX-12, midY + 12);
    epd.print("RAIN");
  }
}

void drawWeekBoxes() {
  uint16_t fgColor = invertDisplay ? GxEPD_WHITE : GxEPD_BLACK;
  epd.setTextColor(fgColor);
  
  float maxRain = 0.1;
  for (int d = 1; d <= 5; d++) {
    for (int i = 0; i < 8; i++) {
      if (dailyRain[d][i] > maxRain) maxRain = dailyRain[d][i];
    }
  }
  
  float roundedMax = getRoundedMax(maxRain);
  Serial.print("Rounded max rain: "); Serial.println(roundedMax);

  int rectW = 70, gap = 6, startX = 19;
  const int topPadding = 5;
  const int usableGraphHeight = graphHeight - topPadding;

  // Only draw numerical labels without grid lines
  epd.setCursor(4, graphBottom);
  epd.print("0");
  epd.setCursor(4, graphTop + topPadding + 8);
  epd.print((int)roundedMax);

  // Draw boxes for next 5 days
  for (int d = 1; d <= 5; d++) {
    int boxIndex = d - 1;
    int x = startX + boxIndex * (rectW + gap);
    
    // Draw box outline
    epd.drawRect(x, graphTop, rectW, graphHeight, fgColor);
    epd.drawRect(x + 1, graphTop + 1, rectW - 2, graphHeight - 2, fgColor);

    // Day label
    epd.setCursor(x + 15, graphTop - 7);
    epd.print(daysOfWeek[(currentDayIndex + d) % 7]);

    // Vertical center line
    for (int y = graphTop; y < graphBottom; y += 4)
      epd.drawPixel(x + rectW / 2, y, fgColor);

    // Internal horizontal grid lines - only within each box
    for (int j = 1; j <= 3; j++) {
      int y = graphTop + j * graphHeight / 4;
      for (int i = x + 1; i < x + rectW - 1; i += 4)
        epd.drawPixel(i, y, fgColor);
    }

    // Draw rain bars
    bool hasRain = false;
    for (int i = 0; i < 8; i++) {
      float rainVal = dailyRain[d][i];
      int barHeight = map(rainVal * 10, 0, roundedMax * 10, 0, usableGraphHeight);
      if (barHeight > 0) {
        hasRain = true;
        int barX = x + 5 + i * 7;
        for (int w = 0; w < 5; w++)
          epd.drawFastVLine(barX + w, graphBottom - barHeight, barHeight, fgColor);
      }
    }

    // Show POP percentage
    char popStr[6];
    sprintf(popStr, "%d%%", dailyPop[d]);
    epd.setCursor(x + 20, graphBottom + 15);
    epd.print(popStr);

    // "NO RAIN" text if applicable
    if (!hasRain) {
      int midX = x + rectW / 2 - 10;
      int midY = graphTop + graphHeight / 2;
      epd.setCursor(midX, midY - 5);
      epd.print("NO");
      epd.setCursor(midX-12, midY + 12);
      epd.print("RAIN");
    }
  }
}

float getRoundedMax(float maxRain) {
  if (maxRain <= 0) return 1.0;
  if (maxRain <= 1.0) return 1.0;
  return ceil(maxRain);
}

bool fetchCurrentWeather(String &weatherDesc, float &temperature, float &pressure, int &humidity, int &updateHour) {
  WiFiClient client;
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(latitude, 6) +
               "&lon=" + String(longitude, 6) + "&units=metric&appid=" + apiKey;
  
  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      weatherDesc = doc["weather"][0]["description"].as<String>();
      weatherDesc.setCharAt(0, toupper(weatherDesc[0]));
      
      temperature = doc["main"]["temp"].as<float>();
      pressure = doc["main"]["pressure"].as<float>();
      humidity = doc["main"]["humidity"].as<int>();
      
      time_t updateTime = doc["dt"].as<time_t>();
      updateTime += 0;
      struct tm *timeinfo = localtime(&updateTime);
      updateHour = timeinfo->tm_hour;
      
      return true;
    }
  }
  http.end();
  return false;
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

void fetchHassData() {
  WiFiClient client;
  HTTPClient http;
  String url = "http://192.168.1.242:8123/api/states/sensor.stookwijzer_advies_code";
  http.begin(client, url);
  http.setAuthorizationType("Bearer");
  http.setAuthorization("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiI1MmViMzlmNmQzYTY0OGU0OTRmYjYxZDhmNmRiNzUyNCIsImlhdCI6MTc3MDc1MTQ0MiwiZXhwIjoyMDg2MTExNDQyfQ.Git5jYG2KHkToHtPFZB1T7Rh_AF8n6Eqg0guzXoIv6I");
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(500);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      String StookwijzerCode = doc["state"].as<String>();
    }
  }
}

static const uint16_t input_buffer_pixels = 800; // may affect performance
//static const uint16_t input_buffer_pixels = 960; // may affect performance

static const uint16_t max_row_width = 1872; // for up to 7.8" display 1872x1404
static const uint16_t max_palette_pixels = 256; // for depth <= 8

uint8_t input_buffer[3 * input_buffer_pixels]; // up to depth 24
uint8_t output_row_mono_buffer[max_row_width / 8]; // buffer for at least one row of b/w bits
uint8_t output_row_color_buffer[max_row_width / 8]; // buffer for at least one row of color bits
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w
uint16_t rgb_palette_buffer[max_palette_pixels]; // palette buffer for depth <= 8 for buffered graphics, needed for 7-color display

void fetchPicture() {
  bool with_color = true;
  WiFiClient client;
  HTTPClient http;
  String url = "http://192.168.1.242:1880/pic4Eink.bmp";
 
  bool connection_ok = false;
  bool valid = false; // valid format to be handled
  bool flip = true; // bitmap is stored bottom-to-top
  //if ((x >= display.epd2.WIDTH) || (y >= display.epd2.HEIGHT)) return;
  Serial.println(); Serial.print("downloading file \""); Serial.print(url);  Serial.println("\"");
  
  http.begin(client, url);
  http.setAuthorization("mark", "brug2Heaven!");
  int httpCode = http.GET();

  if (httpCode == 200) {
    Serial.println("request ok");    
  }

  // Parse BMP header
  if (read16(client) == 0x4D42) // BMP signature
  {
    uint32_t fileSize = read32(client);
    uint32_t creatorBytes = read32(client); (void)creatorBytes; //unused
    uint32_t imageOffset = read32(client); // Start of image data
    uint32_t headerSize = read32(client);
    uint32_t width  = read32(client);
    int32_t height = (int32_t) read32(client);
    uint16_t planes = read16(client);
    uint16_t depth = read16(client); // bits per pixel
    uint32_t format = read32(client);
    uint32_t bytes_read = 7 * 4 + 3 * 2; // read so far
    if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
    {
      Serial.print("File size: "); Serial.println(fileSize);
      Serial.print("Image Offset: "); Serial.println(imageOffset);
      Serial.print("Header size: "); Serial.println(headerSize);
      Serial.print("Bit Depth: "); Serial.println(depth);
      Serial.print("Image size: ");
      Serial.print(width);
      Serial.print('x');
      Serial.println(abs(height));
          // BMP rows are padded (if needed) to 4-byte boundary
      uint32_t rowSize = (width * depth / 8 + 3) & ~3;
      if (depth < 8) rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
      if (height < 0)
      {
        height = -height;
        flip = false;
      }
      uint16_t w = width;
      uint16_t h = height;
      //if ((x + w - 1) >= display.epd2.WIDTH)  w = display.epd2.WIDTH  - x;
      //if ((y + h - 1) >= display.epd2.HEIGHT) h = display.epd2.HEIGHT - y;
      //if (w <= max_row_width) // handle with direct drawing
      //{
        valid = true;
        uint8_t bitmask = 0xFF;
        uint8_t bitshift = 8 - depth;
        uint16_t red, green, blue;
        bool whitish = false;
        bool colored = false;
        if (depth == 1) with_color = false;
        if (depth <= 8)
        {
          if (depth < 8) bitmask >>= depth;
          bytes_read += skip(client, imageOffset - (4 << depth) - bytes_read); // 54 for regular, diff for colorsimportant
          for (uint16_t pn = 0; pn < (1 << depth); pn++)
          {
            blue  = client.read();
            green = client.read();
            red   = client.read();
            client.read();
            bytes_read += 4;
            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
            colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
            if (0 == pn % 8) mono_palette_buffer[pn / 8] = 0;
            mono_palette_buffer[pn / 8] |= whitish << pn % 8;
            if (0 == pn % 8) color_palette_buffer[pn / 8] = 0;
            color_palette_buffer[pn / 8] |= colored << pn % 8;
          }
        }
        epd.clearScreen();
        uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
        bytes_read += skip(client, rowPosition - bytes_read);
        for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
        {
          if (!connection_ok || !(client.connected() || client.available())) break;
          delay(1); // yield() to avoid WDT
          uint32_t in_remain = rowSize;
          uint32_t in_idx = 0;
          uint32_t in_bytes = 0;
          uint8_t in_byte = 0; // for depth <= 8
          uint8_t in_bits = 0; // for depth <= 8
          uint8_t out_byte = 0xFF; // white (for w%8!=0 border)
          uint8_t out_color_byte = 0xFF; // white (for w%8!=0 border)
          uint32_t out_idx = 0;
          for (uint16_t col = 0; col < w; col++) // for each pixel
          {
            yield();
            if (!connection_ok || !(client.connected() || client.available())) break;
            // Time to read more pixel data?
            if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
            {
              uint32_t get = in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain;
              uint32_t got = read8n(client, input_buffer, get);
              while ((got < get) && connection_ok)
              {
                //Serial.print("got "); Serial.print(got); Serial.print(" < "); Serial.print(get); Serial.print(" @ "); Serial.println(bytes_read);
                uint32_t gotmore = read8n(client, input_buffer + got, get - got);
                got += gotmore;
                connection_ok = gotmore > 0;
              }
              in_bytes = got;
              in_remain -= got;
              bytes_read += got;
              in_idx = 0;
            }
            if (!connection_ok)
            {
              Serial.print("Error: got no more after "); Serial.print(bytes_read); Serial.println(" bytes read!");
              break;
            }
            switch (depth)
            {
              case 32:
                blue = input_buffer[in_idx++];
                green = input_buffer[in_idx++];
                red = input_buffer[in_idx++];
                in_idx++; // skip alpha
                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                break;
              case 24:
                blue = input_buffer[in_idx++];
                green = input_buffer[in_idx++];
                red = input_buffer[in_idx++];
                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                break;
              case 16:
                {
                  uint8_t lsb = input_buffer[in_idx++];
                  uint8_t msb = input_buffer[in_idx++];
                  if (format == 0) // 555
                  {
                    blue  = (lsb & 0x1F) << 3;
                    green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                    red   = (msb & 0x7C) << 1;
                  }
                  else // 565
                  {
                    blue  = (lsb & 0x1F) << 3;
                    green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                    red   = (msb & 0xF8);
                  }
                  whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                  colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                }
                break;
              case 1:
              case 2:
              case 4:
              case 8:
                {
                  if (0 == in_bits)
                  {
                    in_byte = input_buffer[in_idx++];
                    in_bits = 8;
                  }
                  uint16_t pn = (in_byte >> bitshift) & bitmask;
                  whitish = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                  colored = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
                  in_byte <<= depth;
                  in_bits -= depth;
                }
                break;
            }
            if (whitish)
            {
              // keep white
            }
            else if (colored && with_color)
            {
              out_color_byte &= ~(0x80 >> col % 8); // colored
            }
            else
            {
              out_byte &= ~(0x80 >> col % 8); // black
            }
            if ((7 == col % 8) || (col == w - 1)) // write that last byte! (for w%8!=0 border)
            {
              output_row_color_buffer[out_idx] = out_color_byte;
              output_row_mono_buffer[out_idx++] = out_byte;
              out_byte = 0xFF; // white (for w%8!=0 border)
              out_color_byte = 0xFF; // white (for w%8!=0 border)
            }
          } // end pixel
          //int16_t yrow = y + (flip ? h - row - 1 : row);
          epd.writeImage(output_row_mono_buffer, output_row_color_buffer, 0, 0, w, 1);
        } // end line
        Serial.print("downloaded ");
        
        epd.refresh();
      }
      Serial.print("bytes read "); Serial.println(bytes_read);
    }
  
  client.stop();
  if (!valid)
  {
    Serial.println("bitmap format not handled.");
  }

  
}

void fetchForecastData() {
  Serial.println("Fetching forecast data...");
  WiFiClient client;
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/forecast?lat=" + String(latitude, 6) +
               "&lon=" + String(longitude, 6) + "&units=metric&appid=" + apiKey;
  
  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(50000);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      JsonArray list = doc["list"];
      
      for (int d = 0; d < 6; d++) {
        dailyPop[d] = 0;
      }
      
      for (int i = 0; i < list.size(); i++) {
        JsonObject entry = list[i];
        const char* dt_txt = entry["dt_txt"];
        struct tm tm;
        strptime(dt_txt, "%Y-%m-%d %H:%M:%S", &tm);
        int dayIndex = (tm.tm_wday == 0 ? 6 : tm.tm_wday - 1);
        int dayOffset = (dayIndex - currentDayIndex + 7) % 7;

        float rain = 0.0;
        if (entry.containsKey("rain") && entry["rain"].containsKey("3h")) {
          rain = entry["rain"]["3h"].as<float>();
        }

        int pop = int(entry["pop"].as<float>() * 100);

        if (dayOffset == 0) {
          if (tm.tm_hour < 24) {
            hourlyRain[tm.tm_hour] = rain;
          }
          if (pop > dailyPop[0]) {
            dailyPop[0] = pop;
          }
        }
        else if (dayOffset >= 1 && dayOffset <= 5) {
          int slot = tm.tm_hour / 3;
          if (slot < 8) {
            dailyRain[dayOffset][slot] = rain;
            if (pop > dailyPop[dayOffset]) {
              dailyPop[dayOffset] = pop;
            }
          }
        }
      }

    }

  }
  http.end();
}

void syncTime() {
  Serial.println("Syncing time...");
  configTime(7200, 0, "pool.ntp.org");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) delay(100);
  currentDayIndex = timeinfo.tm_wday == 0 ? 6 : timeinfo.tm_wday - 1;
  Serial.print("Current hour: "); Serial.println(timeinfo.tm_hour);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Weather Display Booting...");
  SPI.begin(CLK, MISO, MOSI, CS);

  epdInit();
  //epd.drawImagePart(WS_Bitmap4c168x168, 172, 100, 168, 168);
  //drawImagedata(Image4color, 172, 100, 168, 168);
  //epd.display();
  //epd.hibernate();
  delay(5000);
  epd.fillScreen(invertDisplay ? GxEPD_BLACK : GxEPD_WHITE);
  epd.display();
  // Attempt WiFi connection
  bool wifiConnected = connectToWiFi();

  if (!wifiConnected) {
    Serial.println("Failed to connect to any network");
    currentWeatherDesc = "Offline";
    currentTemp = 0.0;
    currentPressure = 0.0;
    currentHumidity = 0;
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      lastUpdateHour = timeinfo.tm_hour;
    } else {
      lastUpdateHour = 0;
    }
  } else {
    syncTime();
    //fetchForecastData();
    fetchPicture();
    String weatherDesc;
    float temperature, pressure;
    int humidity, updateHour;
    if (fetchCurrentWeather(weatherDesc, temperature, pressure, humidity, updateHour)) {
      currentWeatherDesc = weatherDesc;
      currentTemp = temperature;
      currentPressure = pressure;
      currentHumidity = humidity;
      lastUpdateHour = updateHour;
    }
  }

  // Draw display
  // Serial.println("Drawing to e-paper...");
  // epd.fillScreen(invertDisplay ? GxEPD_BLACK : GxEPD_WHITE);
  // epd.drawRect(0, 0, screenW, screenH, invertDisplay ? GxEPD_WHITE : GxEPD_BLACK);
  
  // drawCurrentWeatherBox(currentWeatherDesc, currentTemp, currentPressure, currentHumidity, lastUpdateHour);
  // drawTodayBox(currentHour());
  // drawWeekBoxes();
  
  // epd.display();
  // epd.hibernate();
  // //epdPower(LOW);
 
  // Serial.println("Entering deep sleep...");
  // esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 0); // Wake on button press
  // esp_sleep_enable_timer_wakeup(900LL * 1000000); // 15 min
  // esp_deep_sleep_start();
}

void loop() {
  // Empty - device will be in deep sleep
} 