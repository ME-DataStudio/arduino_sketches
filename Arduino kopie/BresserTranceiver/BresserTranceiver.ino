#include "WeatherSensorCfg.h"
#include "WeatherSensor.h"
#include "BresserTransmitter.h"
#include <SPI.h>

WeatherSensor ws;

// SPI pins for ESP32 S3 board
// in WeatherSensorCfg.h
//#define PIN_RECEIVER_CS   12 //CSN
//#define PIN_RECEIVER_IRQ  9 //GDO0
//#define PIN_RECEIVER_GPIO 10 //GDO2
//#define PIN_RECEIVER_RST  RADIOLIB_NC
//#define LORA_CS         12 //CSN
//#define LORA_SCK        14 //SCK
//#define LORA_MISO       11 //GOD1
//#define LORA_MOSI       13 //MOSI

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    Serial.printf("Starting execution...\n");
    
    SPI.begin(LORA_SCK,LORA_MISO,LORA_MOSI,LORA_CS);
    pinMode(PIN_RECEIVER_GPIO, INPUT);
    Serial.printf("WeatherSensor begin...\n");
    int rc = ws.begin();
    Serial.printf("begin rc=%d\n", rc);
    
    if (rc != RADIOLIB_ERR_NONE) {
        Serial.printf("Failed to initialize WeatherSensorReceiver\n");
        while (true)
            delay(10);
    }
}

void loop() 
{   
    uint8_t msg_buf[40];
    uint8_t msg_size;
    bool valid = true;
    //Serial.println("loop");
    // Tries to receive radio message (non-blocking) and to decode it.
    // Timeout occurs after a small multiple of expected time-on-air.
    int decode_status = ws.getMessage();
    //Serial.println(decode_status);
    if (decode_status == DECODE_OK) {
        char batt_ok[] = "OK ";
        char batt_low[] = "Low";
        char batt_inv[] = "---";
        char * batt;

        if ((ws.sensor.s_type == SENSOR_TYPE_WEATHER1) && !ws.sensor.w.temp_ok) {
            // Special handling for 6-in-1 decoder
            batt = batt_inv;
        } else if (ws.sensor.battery_ok) {
            batt = batt_ok;
        } else {
            batt = batt_low;
        }
        Serial.printf("Id: [%8X] Typ: [%X] Ch: [%d] St: [%d] Bat: [%-3s] RSSI: [%6.1fdBm] ",
            static_cast<int> (ws.sensor.sensor_id),
            ws.sensor.s_type,
            ws.sensor.chan,
            ws.sensor.startup,
            batt,
            ws.sensor.rssi);        
        if (ws.sensor.s_type == SENSOR_TYPE_AIR_PM) {
            // Air Quality (Particular Matter) Sensor
            if (ws.sensor.pm.pm_1_0_init) {
                Serial.printf("PM1.0: [init] ");
            } else {
                Serial.printf("PM1.0: [%uµg/m³] ", ws.sensor.pm.pm_1_0);
            }
            if (ws.sensor.pm.pm_2_5_init) {
                Serial.printf("PM2.5: [init] ");
            } else {
                Serial.printf("PM2.5: [%uµg/m³] ", ws.sensor.pm.pm_2_5);
            }
            if (ws.sensor.pm.pm_10_init) {
                Serial.printf("PM10: [init]\n");
            } else {
                Serial.printf("PM10: [%uµg/m³]\n", ws.sensor.pm.pm_10);
            }
        } else {
            // Any other (weather-like) sensor is very similar
            if (ws.sensor.w.temp_ok) {
                Serial.printf("Temp: [%5.1fC] ", ws.sensor.w.temp_c);
            } else {
                Serial.printf("Temp: [---.-C] ");
            }
            if (ws.sensor.w.humidity_ok) {
                Serial.printf("Hum: [%3d%%] ", ws.sensor.w.humidity);
            }
            else {
                Serial.printf("Hum: [---%%] ");
            }
            if (ws.sensor.w.wind_ok) {
                Serial.printf("Wmax: [%4.1fm/s] Wavg: [%4.1fm/s] Wdir: [%5.1fdeg] ",
                        ws.sensor.w.wind_gust_meter_sec,
                        ws.sensor.w.wind_avg_meter_sec,
                        ws.sensor.w.wind_direction_deg);
            } else {
                Serial.printf("Wmax: [--.-m/s] Wavg: [--.-m/s] Wdir: [---.-deg] ");
            }
            if (ws.sensor.w.rain_ok) {
                Serial.printf("Rain: [%7.1fmm] ",  
                    ws.sensor.w.rain_mm);
            } else {
                Serial.printf("Rain: [-----.-mm] "); 
            }
        
            #if defined BRESSER_6_IN_1 || defined BRESSER_7_IN_1
            if (ws.sensor.w.uv_ok) {
                Serial.printf("UVidx: [%2.1f] ",
                    ws.sensor.w.uv);
            }
            else {
                Serial.printf("UVidx: [--.-] ");
            }
            #endif
            #ifdef BRESSER_7_IN_1
            if (ws.sensor.w.light_ok) {
                Serial.printf("Light: [%2.1fklx] ",
                    ws.sensor.w.light_klx);
            }
            else {
                Serial.printf("Light: [--.-klx] ");
            }
            if (ws.sensor.s_type == SENSOR_TYPE_WEATHER8) {
                if (ws.sensor.w.tglobe_ok) {
                    Serial.printf("T_globe: [%3.1fC] ",
                    ws.sensor.w.tglobe_c);
                }
                else {
                    Serial.printf("T_globe: [--.-C] ");
                }
            }
            #endif
            Serial.printf("\n");
        }
        if (static_cast<int> (ws.sensor.sensor_id) == 44010) {       
            Serial.println("Correct Id");
            ws.sensor.sensor_id = -1053817806; //use sensor-id of base station
            msg_size = msgBegin(msg_buf);
            msg_size += encodeBresser6In1Payload(&msg_buf[msg_size],ws);
            ws.transmit(msg_size, msg_buf);
        }
        
    
    } // if (decode_status == DECODE_OK)
    delay(100);
    //Serial.println("Next receive");
} // loop()
