#include <WeatherSensorCfg.h>
#include <WeatherSensor.h>
#include <SPI.h>
#include "BresserTransmitter.h"

// ============================================================
// CC1101
// ============================================================

byte LORA_CS   = 17; //PICO CSn
byte LORA_SCK  = 18; //PICO SCK
byte LORA_MISO = 16; //PICO RX
byte LORA_MOSI = 19; //PICO TX
int8_t wait_transmit;


// ============================================================
// Bresser receiver
// ============================================================

WeatherSensor ws;



// ============================================================
// Setup
// ============================================================

void setup()
{
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    delay(5000);

    Serial.println();
    Serial.println("Bresser Weather Station");
    Serial.println("========================");

    // --------------------------------------------------------
    // CC1101
    // --------------------------------------------------------
    
    
    SPI.setSCK(LORA_SCK);
    SPI.setRX(LORA_MISO);
    SPI.setTX(LORA_MOSI);
    SPI.setCS(LORA_CS);
    SPI.begin();

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
    uint8_t msg_buf[40];
    uint8_t msg_size;
    
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
                        ws.sensor[i].rssi
                        //ws.sensor[i].lqi
                    );
                }    
            }
        //}
    }
    for(int i=0;i < ws.sensor.size();i++)
    {
        if (wait_transmit > 29) {wait_transmit=0;}
        if (ws.sensor[i].sensor_id == 0xABEA && wait_transmit == 29)
            {
                Serial.println("Sending to basestation");
                ws.sensor[i].sensor_id = -1053817806; //use sensor-id of base station
                msg_size = msgBegin(msg_buf);
                msg_size += encodeBresser6In1Payload(&msg_buf[msg_size],ws,i);
                ws.transmit(msg_size, msg_buf);                    
                wait_transmit = 0;
            }
    }
    delay(1000);
    wait_transmit+=1;
    Serial.print(wait_transmit);

}
