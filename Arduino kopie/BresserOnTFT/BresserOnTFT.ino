#include "WeatherSensorCfg.h"
#include "WeatherSensor.h"

#include <SPI.h>
#include <TFT_eSPI.h>

// ============================================================
// CC1101
// ============================================================

#define LORA_CS    12
#define LORA_SCK   14
#define LORA_MISO  11
#define LORA_MOSI  13


// ============================================================
// Display
//
// TFT_eSPI configuration is in User_Setup.h:
//   USE_HSPI_PORT
//
// BOX-3 LCD:
//   MOSI = GPIO6
//   SCLK = GPIO7
//   CS   = GPIO5
//   DC   = GPIO4
//   RST  = GPIO48
// ============================================================

TFT_eSPI tft = TFT_eSPI();


// ============================================================
// Bresser receiver
// ============================================================

WeatherSensor ws;


// ============================================================
// Display
// ============================================================

void initDisplay()
{
    // BOX-3 LCD reset
    pinMode(48, OUTPUT);

    digitalWrite(48, HIGH);
    delay(100);

    digitalWrite(48, LOW);
    delay(100);

    tft.init();

    // This is the correct orientation for your BOX-3
    tft.setRotation(3);

    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);

    tft.setCursor(10, 10);
    tft.println("BRESSER WEATHER");

    tft.drawLine(10, 35, 310, 35, TFT_DARKGREY);
}


// ============================================================
// Display weather data
// ============================================================

void displayWeather(WeatherSensor::sensor_t &sensor)
{
    // Temperature area
    tft.fillRect(10, 50, 300, 60, TFT_BLACK);

    tft.setTextSize(4);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);

    tft.setCursor(15, 55);
    tft.print(sensor.w.temp_c, 1);
    tft.print(" C");


    // Humidity area
    tft.fillRect(10, 120, 300, 60, TFT_BLACK);

    tft.setTextColor(TFT_CYAN, TFT_BLACK);

    tft.setCursor(15, 125);
    tft.print((int)sensor.w.humidity, 0);
    tft.print("%");


    // RSSI
    tft.fillRect(10, 195, 300, 30, TFT_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);

    tft.setCursor(15, 200);
    tft.print("RSSI: ");
    tft.print(sensor.rssi, 1);
    tft.print(" dBm");
}


// ============================================================
// Setup
// ============================================================

void setup()
{
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    delay(500);

    Serial.println();
    Serial.println("Bresser Weather Station");
    Serial.println("========================");


    // --------------------------------------------------------
    // Display
    // --------------------------------------------------------

    initDisplay();

    Serial.println("Display initialized");


    // --------------------------------------------------------
    // CC1101
    // --------------------------------------------------------

    SPI.begin(
        LORA_SCK,
        LORA_MISO,
        LORA_MOSI,
        LORA_CS
    );

    Serial.println("SPI initialized");


    // --------------------------------------------------------
    // Bresser receiver
    // --------------------------------------------------------

    Serial.println("Initializing Bresser receiver...");

    ws.begin();

    Serial.println("Bresser receiver initialized");
}


// ============================================================
// Main loop
// ============================================================

void loop()
{
    // Try to receive and decode one message.
    DecodeStatus status = ws.getMessage();

    if (status == DECODE_OK)
    {
        Serial.println("Valid Bresser packet received");
        for(int i=0;i < ws.sensor.size();i++)
        {
            Serial.printf("ID: %08X in slot %u\n",
                      ws.sensor[i].sensor_id, i);
        }
        // The library normally stores the decoded sensor
        // in ws.sensor[].

        //if (ws.sensor.size() > 0)
        //{
            for(int i=0;i < ws.sensor.size();i++)
            {
                if (ws.sensor[i].sensor_id == 0xABEA) {

                    Serial.printf(
                        "ID: %08X  Temp: %.1f C  Humidity: %u %%  RSSI: %.1f dBm, LQI: %u\n",
                        ws.sensor[i].sensor_id,
                        ws.sensor[i].w.temp_c,
                        ws.sensor[i].w.humidity,
                        ws.sensor[i].rssi,
                        ws.lqi
                    );

                    if (ws.sensor[i].w.temp_ok && ws.sensor[i].w.humidity_ok)
                    {
                        displayWeather(ws.sensor[i]);
                    }
                }    
            }
        //}
    }

    delay(10);
}
