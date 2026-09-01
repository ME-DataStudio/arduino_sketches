

// Network and API Configuration
const char* WIFI_SSID = "my-ssid";
const char* WIFI_PASS = "my-password";
const char* HA_TOKEN = "hass-token";
const char* HA_URL = "http://192.168.1.242:8123";
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

const char* ENTITY_OUTSIDE_TEMP =
    "sensor.buiten_temperatuur";

const char* ENTITY_LIVING_TEMP =
    "sensor.woonkamer_temperatuur";

const char* ENTITY_WEATHER =
    "weather.home";

const char* ENTITY_POWER =
    "sensor.total_power";   

// Global Variables
RTC_DATA_ATTR bool rtcInvertDisplay = false;  // Persists across deep sleep
bool invertDisplay = false;  // Current display state
