

#if !defined(WEATHER_SENSOR_CFG_H)
#define WEATHER_SENSOR_CFG_H

#include <Arduino.h>

// ------------------------------------------------------------------------------------------------
// --- Weather Sensors ---
// ------------------------------------------------------------------------------------------------

// Disable data type which will not be used to save RAM
#define WIND_DATA_FLOATINGPOINT
#define WIND_DATA_FIXEDPOINT

// Select appropriate sensor message format(s)
// Comment out unused decoders to save operation time/power
//#define BRESSER_5_IN_1
//#define BRESSER_6_IN_1
#define BRESSER_7_IN_1

#if defined(ARDUINO_XIAO_ESP32S3)
    #pragma message("ARDUINO_XIAO_ESP32S3 defined; assuming Wio-SX1262 will be used")
   #define USE_CC1101
    // Use pinning for Seeed XIAO ESP32S3 with Wio-SX1262
    #define PIN_RECEIVER_CS   41
    #define PIN_RECEIVER_IRQ  39
    #define PIN_RECEIVER_GPIO 40
    #define PIN_RECEIVER_RST  42

#elif defined(ARDUINO_ESP32S3_DEV)
    #pragma message("ARDUINO_ESP32S3_DEV defined; this is a generic (i.e. non-specific) target")
    #define USE_CC1101
    #pragma message("Cross check if the selected GPIO pins are really available on your board.")
    #pragma message("Connect a radio module with a supported chip.")
    #pragma message("Select the chip by setting the appropriate define.")
    // Use pinning for generic ESP32 S3 board with unspecified radio module
    // For esp32-s3-box-3 with dock
    // gdo2Pin = 10;
    // sckPin = 14;
    // mosiPin = 13;
    // god1Pin = 11;
    // gdo0Pin = 9;
    // csnPin = 12;

    #define PIN_RECEIVER_CS   12 //CSN
    #define PIN_RECEIVER_IRQ  9 //GDO0
    #define PIN_RECEIVER_GPIO 10 //GDO2
    #define PIN_RECEIVER_RST  RADIOLIB_NC
    #define LORA_CS         12 //CSN
    #define LORA_SCK        14 //SCK
    #define LORA_MISO       11 //GOD1
    #define LORA_MOSI       13 //MOSI

#elif defined(ESP32)
    #pragma message("ESP32 defined; this is a generic (i.e. non-specific) target")
    #pragma message("Cross check if the selected GPIO pins are really available on your board.")
    #pragma message("Connect a radio module with a supported chip.")
    #pragma message("Select the chip by setting the appropriate define.")
    #define USE_CC1101
    // Generic pinning for ESP32 development boards
    #define PIN_RECEIVER_CS   27
    #define PIN_RECEIVER_IRQ  21
    #define PIN_RECEIVER_GPIO 33
    #define PIN_RECEIVER_RST  32
    
    // When using SPI bus other than FSPI, e.g. HSPI, define the following
    //#define LORA_SPI_BUS    HSPI
    //#define LORA_CS         48
    //#define LORA_SCK        45
    //#define LORA_MISO       46
    //#define LORA_MOSI       47
#elif defined(ESP8266)
    #pragma message("ESP8266 defined; this is a generic (i.e. non-specific) target")
    #pragma message("Cross check if the selected GPIO pins are really available on your board.")
    #pragma message("Connect a radio module with a supported chip.")
    #pragma message("Select the chip by setting the appropriate define.")
    //#define USE_SX1276
    //#define USE_SX1262
    #define USE_CC1101
    //#define USE_LR1121

    // Generic pinning for ESP8266 development boards (e.g. LOLIN/WEMOS D1 mini)
    #define PIN_RECEIVER_CS   15
    #define PIN_RECEIVER_IRQ  4
    #define PIN_RECEIVER_GPIO 5
    #define PIN_RECEIVER_RST  2
#endif

#if defined(USE_CC1101)
#define RADIO_CHIP CC1101
#pragma message("No radio chip selected!")
#endif

#if defined(ESP8266) || defined(ARDUINO_ARCH_RP2040)
    #define ARDUHAL_LOG_LEVEL_NONE      0
    #define ARDUHAL_LOG_LEVEL_ERROR     1
    #define ARDUHAL_LOG_LEVEL_WARN      2
    #define ARDUHAL_LOG_LEVEL_INFO      3
    #define ARDUHAL_LOG_LEVEL_DEBUG     4
    #define ARDUHAL_LOG_LEVEL_VERBOSE   5

    #if defined(ARDUINO_ARCH_RP2040) && defined(DEBUG_RP2040_PORT)
        #define DEBUG_PORT DEBUG_RP2040_PORT
    #elif defined(DEBUG_ESP_PORT)
        #define DEBUG_PORT DEBUG_ESP_PORT
    #endif
    
    // Set desired level here if not defined elsewhere!
    #if !defined(CORE_DEBUG_LEVEL)
        #define CORE_DEBUG_LEVEL ARDUHAL_LOG_LEVEL_INFO
    #endif

    #if defined(DEBUG_PORT) && CORE_DEBUG_LEVEL > ARDUHAL_LOG_LEVEL_NONE
        #define log_e(...) { DEBUG_PORT.printf("%s(), l.%d: ",__func__, __LINE__); DEBUG_PORT.printf(__VA_ARGS__); DEBUG_PORT.println(); }
     #else
        #define log_e(...) {}
     #endif
    #if defined(DEBUG_PORT) && CORE_DEBUG_LEVEL > ARDUHAL_LOG_LEVEL_ERROR
        #define log_w(...) { DEBUG_PORT.printf("%s(), l.%d: ", __func__, __LINE__); DEBUG_PORT.printf(__VA_ARGS__); DEBUG_PORT.println(); }
     #else
        #define log_w(...) {}
     #endif
    #if defined(DEBUG_PORT) && CORE_DEBUG_LEVEL > ARDUHAL_LOG_LEVEL_WARN
        #define log_i(...) { DEBUG_PORT.printf("%s(), l.%d: ", __func__, __LINE__); DEBUG_PORT.printf(__VA_ARGS__); DEBUG_PORT.println(); }
     #else
        #define log_i(...) {}
     #endif
    #if defined(DEBUG_PORT) && CORE_DEBUG_LEVEL > ARDUHAL_LOG_LEVEL_INFO
        #define log_d(...) { DEBUG_PORT.printf("%s(), l.%d: ", __func__, __LINE__); DEBUG_PORT.printf(__VA_ARGS__); DEBUG_PORT.println(); }
     #else
        #define log_d(...) {}
     #endif
    #if defined(DEBUG_PORT) && CORE_DEBUG_LEVEL > ARDUHAL_LOG_LEVEL_DEBUG
        #define log_v(...) { DEBUG_PORT.printf("%s(), l.%d: ", __func__, __LINE__); DEBUG_PORT.printf(__VA_ARGS__); DEBUG_PORT.println(); }
     #else
        #define log_v(...) {}
     #endif

#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define RECEIVER_CHIP "[" STR(RADIO_CHIP) "]"
#pragma message("Receiver chip: " RECEIVER_CHIP)
#pragma message("Pin config: RST->" STR(PIN_RECEIVER_RST) ", CS->" STR(PIN_RECEIVER_CS) ", GD0/G0/IRQ->" STR(PIN_RECEIVER_IRQ) ", GDO2/G1/GPIO->" STR(PIN_RECEIVER_GPIO) )

#endif
