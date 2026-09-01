///////////////////////////////////////////////////////////////////////////////////////////////////
// SensorTransmitter.h
//
// Bresser 5-in-1/6-in-1/7-in-1 868 MHz Sensor Radio Transmitter
// based on CC1101 or SX1276/RFM95W and ESP32/ESP8266
//
// https://github.com/matthias-bs/SensorTransmitter
//
// created: 11/2023
//
//
// MIT License
//
// Copyright (c) 2023 Matthias Prinke
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// History:
// 20231111 Created based on 
//          https://github.com/jgromes/RadioLib/blob/master/examples/SX127x/SX127x_Transmit_Blocking/SX127x_Transmit_Blocking.ino
// 20231112 Added utilization of class WeatherSensor
//          Added JSON string as payload source
// 20231113 Added JSON string input from serial console
//          Added TRANSCEIVER_CHIP
// 20231114 Added enum Encoders
// 20241227 Added LilyGo T3 S3 SX1262/SX1276/LR1121
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(SENSOR_TRANSMITTER_H)
#define SENSOR_TRANSMITTER_H

#include <Arduino.h>

#define MAX_SENSORS_DEFAULT 1       //!< WeatherSensor - no. of sensors
#define WIND_DATA_FLOATINGPOINT     //!< WeatherSensor - wind data type

#define TX_INTERVAL 30              //!< transmit interval in seconds
#define TX_POWER 10                 //!< transmit power in dBm (valid values depend on the radio module)

enum struct Encoders {
    ENC_BRESSER_5IN1,
    ENC_BRESSER_6IN1,
    ENC_BRESSER_7IN1,
    ENC_BRESSER_LEAKAGE,
    ENC_BRESSER_LIGHTNING
};

#if defined(ESP32)
    #pragma message("ESP32 defined; this is a generic (i.e. non-specific) target")
    #pragma message("Cross check if the selected GPIO pins are really available on your board.")
    #pragma message("Connect a radio module with a supported chip.")
    #pragma message("Select the chip by setting the appropriate define.")
    #define USE_CC1101
    // Custom pinning for ESP32 development boards
    //#define PIN_RECEIVER_CS   27
    //#define PIN_RECEIVER_IRQ  21
    //#define PIN_RECEIVER_GPIO 33
    //#define PIN_RECEIVER_RST  32
    //esp32-s3-box 
    #define gdo2Pin 10
    #define sckPin 14
    #define mosiPin 13
    #define god1Pin 11
    #define gdo0Pin 9
    #define csnPin 12
#endif

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    #pragma message("RASPBERRY_PI_PICO_W defined; assuming CC1101 will be used")
    #define USE_CC1101
    
    #define PIN_RECEIVER_CS   17
    #define PIN_RECEIVER_IRQ  20
    #define PIN_RECEIVER_GPIO 21
    #define PIN_RECEIVER_RST  RADIOLIB_NC
    #define LORA_CS   17 //PICO CSn
    #define LORA_SCK  18 //PICO SCK
    #define LORA_MISO 16 //PICO RX
    #define LORA_MOSI 19 //PICO TX
#endif

// ------------------------------------------------------------------------------------------------
// --- Radio Transceiver ---
// ------------------------------------------------------------------------------------------------
#if defined(USE_CC1101)
    #define TRANSCEIVER_CHIP "[CC1101]"
#else
    #error "Either USE_CC1101, USE_SX1276, USE_SX1262 or USE_LR1121 must be defined!"
#endif

#if defined(USE_CC1101)
    #define RADIO_CHIP CC1101
#endif

#if defined(ESP32)
    // Custom pinning for ESP32-S3-BOX3
    #define PIN_TRANSCEIVER_CS   12

    // CC1101: GDO0 / RFM95W/SX127x: G0
    #define PIN_TRANSCEIVER_IRQ  9

    // CC1101: GDO2 / RFM95W/SX127x: G1
    #define PIN_TRANSCEIVER_GPIO 10

    // RFM95W/SX127x - GPIOxx / CC1101 - RADIOLIB_NC
    #define PIN_TRANSCEIVER_RST  RADIOLIB_NC
    
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#pragma message("Transmitter chip: " TRANSCEIVER_CHIP)
#pragma message("Pin config: RST->" STR(PIN_TRANSCEIVER_RST) ", CS->" STR(PIN_TRANSCEIVER_CS) ", GD0/G0/IRQ->" STR(PIN_TRANSCEIVER_IRQ) ", GDO2/G1/GPIO->" STR(PIN_TRANSCEIVER_GPIO) )

#endif