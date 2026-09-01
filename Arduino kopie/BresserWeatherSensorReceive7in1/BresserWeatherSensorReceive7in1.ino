#include "WeatherSensorCfg.h"
#include "WeatherSensor.h"
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
    // This example uses only a single slot in the sensor data array
    int const i=0;

    // Clear all sensor data
    ws.clearSlots();

    // Tries to receive radio message (non-blocking) and to decode it.
    // Timeout occurs after a small multiple of expected time-on-air.
    int decode_status = ws.getMessage();
    //Serial.println(decode_status);
    if (decode_status == DECODE_OK) {
        char batt_ok[] = "OK ";
        char batt_low[] = "Low";
        char batt_inv[] = "---";
        char * batt;

        if ((ws.sensor[i].s_type == SENSOR_TYPE_WEATHER1) && !ws.sensor[i].w.temp_ok) {
            // Special handling for 6-in-1 decoder
            batt = batt_inv;
        } else if (ws.sensor[i].battery_ok) {
            batt = batt_ok;
        } else {
            batt = batt_low;
        }
        Serial.printf("Id: [%8X] Typ: [%X] Ch: [%d] St: [%d] Bat: [%-3s] RSSI: [%6.1fdBm] ",
            static_cast<int> (ws.sensor[i].sensor_id),
            ws.sensor[i].s_type,
            ws.sensor[i].chan,
            ws.sensor[i].startup,
            batt,
            ws.sensor[i].rssi);
           
        if (ws.sensor[i].s_type == SENSOR_TYPE_LIGHTNING) {
            // Lightning Sensor
            Serial.printf("Lightning Counter: [%4d] ", ws.sensor[i].lgt.strike_count);
            if (ws.sensor[i].lgt.distance_km != 0) {
                Serial.printf("Distance: [%2dkm] ", ws.sensor[i].lgt.distance_km);
            } else {
                Serial.printf("Distance: [----] ");
            }
            Serial.printf("unknown1: [0x%03X] ", ws.sensor[i].lgt.unknown1);
            Serial.printf("unknown2: [0x%04X]\n", ws.sensor[i].lgt.unknown2);

        }
        else if (ws.sensor[i].s_type == SENSOR_TYPE_LEAKAGE) {
            // Water Leakage Sensor
            Serial.printf("Leakage: [%-5s]\n", (ws.sensor[i].leak.alarm) ? "ALARM" : "OK");
      
        }
        else if (ws.sensor[i].s_type == SENSOR_TYPE_AIR_PM) {
            // Air Quality (Particular Matter) Sensor
            if (ws.sensor[i].pm.pm_1_0_init) {
                Serial.printf("PM1.0: [init] ");
            } else {
                Serial.printf("PM1.0: [%uµg/m³] ", ws.sensor[i].pm.pm_1_0);
            }
            if (ws.sensor[i].pm.pm_2_5_init) {
                Serial.printf("PM2.5: [init] ");
            } else {
                Serial.printf("PM2.5: [%uµg/m³] ", ws.sensor[i].pm.pm_2_5);
            }
            if (ws.sensor[i].pm.pm_10_init) {
                Serial.printf("PM10: [init]\n");
            } else {
                Serial.printf("PM10: [%uµg/m³]\n", ws.sensor[i].pm.pm_10);
            }
            
        }
        else if (ws.sensor[i].s_type == SENSOR_TYPE_CO2) {
            // CO2 Sensor
            if (ws.sensor[i].co2.co2_init) {
                Serial.printf("CO2: [init]\n");
            } else {
                Serial.printf("CO2: [%uppm]\n", ws.sensor[i].co2.co2_ppm);
            }

        }
        else if (ws.sensor[i].s_type == SENSOR_TYPE_HCHO_VOC) {
            // HCHO / VOC Sensor
            if (ws.sensor[i].voc.hcho_init) {
                Serial.printf("HCHO: [init] ");
            } else {
                Serial.printf("HCHO: [%uppb] ", ws.sensor[i].voc.hcho_ppb);
            }
            if (ws.sensor[i].voc.voc_init) {
                Serial.printf("VOC: [init]\n");
            } else {
                Serial.printf("VOC: [%u]\n", ws.sensor[i].voc.voc_level);
            }

        }
        else if (ws.sensor[i].s_type == SENSOR_TYPE_SOIL) {
            Serial.printf("Temp: [%5.1fC] ", ws.sensor[i].soil.temp_c);
            Serial.printf("Moisture: [%2d%%]\n", ws.sensor[i].soil.moisture);

        } else {
            // Any other (weather-like) sensor is very similar
            if (ws.sensor[i].w.temp_ok) {
                Serial.printf("Temp: [%5.1fC] ", ws.sensor[i].w.temp_c);
            } else {
                Serial.printf("Temp: [---.-C] ");
            }
            if (ws.sensor[i].w.humidity_ok) {
                Serial.printf("Hum: [%3d%%] ", ws.sensor[i].w.humidity);
            }
            else {
                Serial.printf("Hum: [---%%] ");
            }
            if (ws.sensor[i].w.wind_ok) {
                Serial.printf("Wmax: [%4.1fm/s] Wavg: [%4.1fm/s] Wdir: [%5.1fdeg] ",
                        ws.sensor[i].w.wind_gust_meter_sec,
                        ws.sensor[i].w.wind_avg_meter_sec,
                        ws.sensor[i].w.wind_direction_deg);
            } else {
                Serial.printf("Wmax: [--.-m/s] Wavg: [--.-m/s] Wdir: [---.-deg] ");
            }
            if (ws.sensor[i].w.rain_ok) {
                Serial.printf("Rain: [%7.1fmm] ",  
                    ws.sensor[i].w.rain_mm);
            } else {
                Serial.printf("Rain: [-----.-mm] "); 
            }
        
            #if defined BRESSER_6_IN_1 || defined BRESSER_7_IN_1
            if (ws.sensor[i].w.uv_ok) {
                Serial.printf("UVidx: [%2.1f] ",
                    ws.sensor[i].w.uv);
            }
            else {
                Serial.printf("UVidx: [--.-] ");
            }
            #endif
            #ifdef BRESSER_7_IN_1
            if (ws.sensor[i].w.light_ok) {
                Serial.printf("Light: [%2.1fklx] ",
                    ws.sensor[i].w.light_klx);
            }
            else {
                Serial.printf("Light: [--.-klx] ");
            }
            if (ws.sensor[i].s_type == SENSOR_TYPE_WEATHER8) {
                if (ws.sensor[i].w.tglobe_ok) {
                    Serial.printf("T_globe: [%3.1fC] ",
                    ws.sensor[i].w.tglobe_c);
                }
                else {
                    Serial.printf("T_globe: [--.-C] ");
                }
            }
            #endif
            Serial.printf("\n");

      }
    
    } // if (decode_status == DECODE_OK)
    delay(1000);
    //Serial.println("Next receive");
} // loop()
