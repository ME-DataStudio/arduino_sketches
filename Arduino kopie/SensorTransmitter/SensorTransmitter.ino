///////////////////////////////////////////////////////////////////////////////////////////////////
// SensorTransmitter.ino
//
// Bresser 6-in-1 868 MHz Sensor Radio Transmitter
// based on CC1101 and ESP32, reduced version from:
//
// This can be used to emulate sensors for testing purposes or to implement sensors currently not
// available. In the the latter, emulate a sensor supported by the base station, but send
// measurement values. E.g. emulating a temperature sensor, the snow depth could be displayed by
// the base station.
//
// https://github.com/matthias-bs/SensorTransmitter
//
// created: 11/2026
//
//
// MIT License
//
// Copyright (c) 2026 Matthias Prinke
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
//          Added checksum calculation
// 20231112 Added utilization of class WeatherSensor
//          Added JSON string as payload source
// 20231113 Added JSON string input from serial console
//          Added encodeBresserLightningPayload (DATA_RAW, DATA_GEN)
// 20231114 Added setting of encoder and tx_interval
// 20231115 Added support of CC1101 transceiver
//          Added encodeBresser<6In1|7In1|Leakage>Payload() - only raw data input!
// 20231117 Implemented encodeBresser6In1Payload() (basic functionality)
// 20231118 encodeBresser6In1Payload(): Added UV index and remaining (known) sensors
// 20231119 Restructured data generation and encoding
// 20231120 Implemented encodeBresser7In1Payload()
// 20231121 Implemented encodeBresserLeakage() - CRC errors at receiver
// 20240129 Fixed lightning counter encoding
// 20240209 Added CO2 and HCHO/VOC sensors
// 20240210 Added missing CO2 and HCHO/VOC sensor encoding
// 20241227 Added LilyGo T3 S3 SX1262/SX1276/LR1121
// 20260130 Fixed radio module initialization for LilyGo T3S3 boards using RadioLib 7.5.0
// 20260203 Fixed exception in JSON deserialization for Bresser 7in1 sensor with wrong s_type
//          Fixed Water Leakage Sensor encoder
//          Fixed HCHO encoding
// 20260204 Fixed exception in deSerialize()
// 20260222 Removed getDataRate() call, because this method has been removed in RadioLib 7.6.0
// 20260620 Changed radio initialization to new ConfigFSK_t structure in RadioLib 7.7.x
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "secrets.h"
#include <WiFi.h>  
#include <HTTPClient.h>  
#include "SensorTransmitter.h"
#include <RadioLib.h>
#include "logging.h"
#include "WeatherSensor.h"
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <SPI.h>

static RADIO_CHIP radio = new Module(PIN_RECEIVER_CS, PIN_RECEIVER_IRQ, RADIOLIB_NC, PIN_RECEIVER_GPIO);

// Weather sensor object; used for message generation
WeatherSensor ws;

typedef struct
{
  float temperature_inside;
  float humidity_inside;
  float temperature_outside;
  float humidity_outside;
  float wind_speed;
  String wind_direction;
  String weather_forecast_now;
  String time;
  String timestamp;
} HAData;

HAData haData;
const char *http_endpoint = "http://homeassistant.local:8123/api/states/sensor.epaper_esp32s3_data";

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password); // Connect to the network
  //Serial.println("\nConnecting to WiFi Network ..");

  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(100);
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
  #if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    SPI.setSCK(LORA_SCK);
    SPI.setRX(LORA_MISO);
    SPI.setTX(LORA_MOSI);
    SPI.setCS(LORA_CS);
    SPI.begin();
      Serial.println("SPI initialized");
  #elif defined(ESP32)
    SPI.begin(sckPin, god1Pin, mosiPin, csnPin);
    pinMode(gdo2Pin,INPUT);
      Serial.println("SPI initialized");
  #endif

  // initialize radio
  log_i("%s Initializing ... ", TRANSCEIVER_CHIP);

  ConfigFSK_t config;
  config.frequency = 868.3;              // MHz
  config.bitRate = 8.21;                 // kBaud
  config.frequencyDeviation = 57.136417; // kHz
  config.power = TX_POWER;               // dBm
  config.preambleLength = 32;            // bits
  // RX bandwidth in kHz, radio chip specific (see RadioLib documentation for details)
  config.receiverBandwidth = 270;
  int state = radio.begin(config);

  if (state == RADIOLIB_ERR_NONE)
  {
    log_i("CC1101 begin success!");
  }
  else
  {
    log_e("CC1101 begin failed, code %d", state);
    while (true)
      ;
  }

  // Set weather sensor array size to 1
  ws.sensor.resize(1);

} //end setup

String httpGETRequest(const char *serverName)
{
  WiFiClient client;
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(client, serverName);
  #if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    http.addHeader("Authorization", strcat("Bearer ",token));
  #endif
  #if defined(ESP32)
    http.setAuthorizationType("Bearer");
    http.setAuthorization(token);  
  #endif
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
} //end httpGETRequest

void getData()
{
  StaticJsonDocument<1024> doc; // change size if needed
  deserializeJson(doc, httpGETRequest(http_endpoint));

  ws.sensor[0].sensor_id = -1053817806;
  ws.sensor[0].s_type = 1;
  ws.sensor[0].chan = 0;
  ws.sensor[0].startup = 0;
  ws.sensor[0].battery_ok = 1;
  ws.sensor[0].w.temp_c = doc["attributes"]["temperature_outside"].as<float>();
  ws.sensor[0].w.humidity = doc["attributes"]["humidity_outside"].as<float>();
  ws.sensor[0].w.wind_gust_meter_sec = doc["attributes"]["wind_speed"].as<float>();
  ws.sensor[0].w.wind_avg_meter_sec = doc["attributes"]["wind_speed"].as<float>();
  ws.sensor[0].w.wind_direction_deg = doc["attributes"]["wind_direction"].as<int>();
  ws.sensor[0].w.rain_mm = 0;
  ws.sensor[0].w.uv = 0;
  Serial.println(ws.sensor[0].w.temp_c);
  Serial.println(ws.sensor[0].w.humidity);
  Serial.println(ws.sensor[0].w.wind_avg_meter_sec);
}

int msgBegin(uint8_t *msg)
{
  uint8_t preamble[] = {0xAA, 0xAA, 0xAA, 0xAA};
  uint8_t syncword[] = {0x2D, 0xD4};

  memcpy(msg, preamble, sizeof(preamble));
  memcpy(&msg[sizeof(preamble)], syncword, sizeof(syncword));

  return sizeof(preamble) + sizeof(syncword);
}

//
// From from rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_6in1.c (20220608)
//
// - also Bresser Weather Center 7-in-1 indoor sensor.
// - also Bresser new 5-in-1 sensors.
// - also Froggit WH6000 sensors.
// - also rebranded as Ventus C8488A (W835)
// - also Bresser 3-in-1 Professional Wind Gauge / Anemometer PN 7002531
// - also Bresser Pool / Spa Thermometer PN 7009973 (s_type = 3)
//
// There are at least two different message types:
// - 24 seconds interval for temperature, hum, uv and rain (alternating messages)
// - 12 seconds interval for wind data (every message)
//
// Also Bresser Explore Scientific SM60020 Soil moisture Sensor.
// https://www.bresser.de/en/Weather-Time/Accessories/EXPLORE-SCIENTIFIC-Soil-Moisture-and-Soil-Temperature-Sensor.html
//
// Moisture:
//
//     f16e 187000e34 7 ffffff0000 252 2 16 fff 004 000 [25,2, 99%, CH 7]
//     DIGEST:8h8h ID?8h8h8h8h TYPE:4h STARTUP:1b CH:3d 8h 8h8h 8h8h TEMP:12h ?2b BATT:1b ?1b MOIST:8h UV?~12h ?4h CHKSUM:8h
//
// Moisture is transmitted in the humidity field as index 1-16: 0, 7, 13, 20, 27, 33, 40, 47, 53, 60, 67, 73, 80, 87, 93, 99.
// The Wind speed and direction fields decode to valid zero but we exclude them from the output.
//
//     aaaa2dd4e3ae1870079341ffffff0000221201fff279 [Batt ok]
//     aaaa2dd43d2c1870079341ffffff0000219001fff2fc [Batt low]
//
//     {206}55555555545ba83e803100058631ff11fe6611ffffffff01cc00 [Hum 96% Temp 3.8 C Wind 0.7 m/s]
//     {205}55555555545ba999263100058631fffffe66d006092bffe0cff8 [Hum 95% Temp 3.0 C Wind 0.0 m/s]
//     {199}55555555545ba840523100058631ff77fe668000495fff0bbe [Hum 95% Temp 3.0 C Wind 0.4 m/s]
//     {205}55555555545ba94d063100058631fffffe665006092bffe14ff8
//     {206}55555555545ba860703100058631fffffe6651ffffffff0135fc [Hum 95% Temp 3.0 C Wind 0.0 m/s]
//     {205}55555555545ba924d23100058631ff99fe68b004e92dffe073f8 [Hum 96% Temp 2.7 C Wind 0.4 m/s]
//     {202}55555555545ba813403100058631ff77fe6810050929ffe1180 [Hum 94% Temp 2.8 C Wind 0.4 m/s]
//     {205}55555555545ba98be83100058631fffffe6130050929ffe17800 [Hum 95% Temp 2.8 C Wind 0.8 m/s]
//
//     2dd4  1f 40 18 80 02 c3 18 ff 88 ff 33 08 ff ff ff ff 80 e6 00 [Hum 96% Temp 3.8 C Wind 0.7 m/s]
//     2dd4  cc 93 18 80 02 c3 18 ff ff ff 33 68 03 04 95 ff f0 67 3f [Hum 95% Temp 3.0 C Wind 0.0 m/s]
//     2dd4  20 29 18 80 02 c3 18 ff bb ff 33 40 00 24 af ff 85 df    [Hum 95% Temp 3.0 C Wind 0.4 m/s]
//     2dd4  a6 83 18 80 02 c3 18 ff ff ff 33 28 03 04 95 ff f0 a7 3f
//     2dd4  30 38 18 80 02 c3 18 ff ff ff 33 28 ff ff ff ff 80 9a 7f [Hum 95% Temp 3.0 C Wind 0.0 m/s]
//     2dd4  92 69 18 80 02 c3 18 ff cc ff 34 58 02 74 96 ff f0 39 3f [Hum 96% Temp 2.7 C Wind 0.4 m/s]
//     2dd4  09 a0 18 80 02 c3 18 ff bb ff 34 08 02 84 94 ff f0 8c 0  [Hum 94% Temp 2.8 C Wind 0.4 m/s]
//     2dd4  c5 f4 18 80 02 c3 18 ff ff ff 30 98 02 84 94 ff f0 bc 00 [Hum 95% Temp 2.8 C Wind 0.8 m/s]
//
//     {147} 5e aa 18 80 02 c3 18 fa 8f fb 27 68 11 84 81 ff f0 72 00 [Temp 11.8 C  Hum 81%]
//     {149} ae d1 18 80 02 c3 18 fa 8d fb 26 78 ff ff ff fe 02 db f0
//     {150} f8 2e 18 80 02 c3 18 fc c6 fd 26 38 11 84 81 ff f0 68 00 [Temp 11.8 C  Hum 81%]
//     {149} c4 7d 18 80 02 c3 18 fc 78 fd 29 28 ff ff ff fe 03 97 f0
//     {149} 28 1e 18 80 02 c3 18 fb b7 fc 26 58 ff ff ff fe 02 c3 f0
//     {150} 21 e8 18 80 02 c3 18 fb 9c fc 33 08 11 84 81 ff f0 b7 f8 [Temp 11.8 C  Hum 81%]
//     {149} 83 ae 18 80 02 c3 18 fc 78 fc 29 28 ff ff ff fe 03 98 00
//     {150} 5c e4 18 80 02 c3 18 fb ba fc 26 98 11 84 81 ff f0 16 00 [Temp 11.8 C  Hum 81%]
//     {148} d0 bd 18 80 02 c3 18 f9 ad fa 26 48 ff ff ff fe 02 ff f0
//
// Wind and Temperature/Humidity or Rain:
//
//     DIGEST:8h8h ID:8h8h8h8h TYPE:4h STARTUP:1b CH:3d WSPEED:~8h~4h ~4h~8h WDIR:12h ?4h TEMP:8h.4h ?2b BATT:1b ?1b HUM:8h UV?~12h ?4h CHKSUM:8h
//     DIGEST:8h8h ID:8h8h8h8h TYPE:4h STARTUP:1b CH:3d WSPEED:~8h~4h ~4h~8h WDIR:12h ?4h RAINFLAG:8h RAIN:8h8h UV:8h8h CHKSUM:8h
//
// Digest is LFSR-16 gen 0x8810 key 0x5412, excluding the add-checksum and trailer.
// Checksum is 8-bit add (with carry) to 0xff.
//
// Notes on different sensors:
//
// - 1910 084d 18 : RebeckaJohansson, VENTUS W835
// - 2030 088d 10 : mvdgrift, Wi-Fi Colour Weather Station with 5in1 Sensor, Art.No.: 7002580, ff 01 in the UV field is (obviously) invalid.
// - 1970 0d57 18 : danrhjones, bresser 5-in-1 model 7002580, no UV
// - 18b0 0301 18 : konserninjohtaja 6-in-1 outdoor sensor
// - 18c0 0f10 18 : rege245 BRESSER-PC-Weather-station-with-6-in-1-outdoor-sensor
// - 1880 02c3 18 : f4gqk 6-in-1
// - 18b0 0887 18 : npkap
uint8_t encodeBresser6In1Payload(uint8_t *msg)
{
  static int msg_type = 0;
  char buf[10];  // Buffer for snprintf formats: %07.1f (10 bytes), %04.1f (6 bytes), %02d (3 bytes)
  uint8_t payload[18] = {0};

  payload[2] = ws.sensor[0].sensor_id >> 24;
  payload[3] = (ws.sensor[0].sensor_id >> 16) & 0xFF;
  payload[4] = (ws.sensor[0].sensor_id >> 8) & 0xFF;
  payload[5] = (ws.sensor[0].sensor_id) & 0xFF;
  payload[6] = ws.sensor[0].s_type << 4;
  payload[6] |= (ws.sensor[0].startup ? 0 : 8) | ws.sensor[0].chan;

  snprintf(buf, 10, "%04.1f", ws.sensor[0].w.wind_gust_meter_sec);
  log_d("Wind gust: %04.1f", ws.sensor[0].w.wind_gust_meter_sec);
  payload[7] = ((buf[0] - '0') << 4) | (buf[1] - '0');
  payload[8] = (buf[3] - '0') << 4;

  snprintf(buf, 10, "%04.1f", ws.sensor[0].w.wind_avg_meter_sec);
  log_d("Wind avg: %04.1f", ws.sensor[0].w.wind_avg_meter_sec);
  payload[9] = ((buf[0] - '0') << 4) | (buf[1] - '0');
  payload[8] |= buf[3] - '0';

  // Invert bytes
  payload[7] ^= 0xFF;
  payload[8] ^= 0xFF;
  payload[9] ^= 0xFF;

  snprintf(buf, 10, "%03d", (int)ws.sensor[0].w.wind_direction_deg);
  log_d("Wind dir: %03d", (int)ws.sensor[0].w.wind_direction_deg);
  payload[10] = ((buf[0] - '0') << 4) | (buf[1] - '0');
  payload[11] = (buf[2] - '0') << 4;

  if ((ws.sensor[0].s_type == SENSOR_TYPE_WEATHER1) ||
      (ws.sensor[0].s_type == SENSOR_TYPE_POOL_THERMO) ||
      (ws.sensor[0].s_type == SENSOR_TYPE_THERMO_HYGRO) ||
      (ws.sensor[0].s_type == SENSOR_TYPE_SOIL))
  {
    if (msg_type == 0)
    {
      float temp_c;
      if (ws.sensor[0].s_type == SENSOR_TYPE_SOIL)
      {
        temp_c = ws.sensor[0].soil.temp_c;
      }
      else
      {
        temp_c = ws.sensor[0].w.temp_c;
      }
      log_d("Temp: %04.1f", temp_c);
      if (temp_c < 0)
      {
        temp_c += 100;
        payload[13] = 8;
      }
      else
      {
        payload[13] = 0;
      }

      snprintf(buf, 10, "%04.1f", temp_c);
      payload[12] = ((buf[0] - '0') << 4) | (buf[1] - '0');
      payload[13] |= ((buf[3] - '0') << 4) | (ws.sensor[0].battery_ok ? 2 : 0);
      payload[16] = 0; // Flags: temp_ok

      if ((ws.sensor[0].s_type == SENSOR_TYPE_WEATHER1) ||
          (ws.sensor[0].s_type == SENSOR_TYPE_THERMO_HYGRO))
      {
        snprintf(buf, 10, "%02d", ws.sensor[0].w.humidity);
        payload[14] = ((buf[0] - '0') << 4) | (buf[1] - '0');
      }

      if (ws.sensor[0].s_type == SENSOR_TYPE_SOIL)
      {
        int const moisture_map[] = {0, 7, 13, 20, 27, 33, 40, 47, 53, 60, 67, 73, 80, 87, 93, 99}; // scale is 20/3
        const int MOISTURE_MAP_MAX_INDEX = 15;
        payload[14] = MOISTURE_MAP_MAX_INDEX; // Default to maximum if moisture >= 99
        for (int i = 0; i < 16; i++)
        {
          if (moisture_map[i] > ws.sensor[0].soil.moisture)
          {
            log_d("Moisture: %d Index: %d", ws.sensor[0].soil.moisture, i);
            payload[14] = i;
            break;
          }
        }
      }

      if (ws.sensor[0].s_type == SENSOR_TYPE_WEATHER1)
      {
        msg_type = 1;
      }
    } // msg_type == 0
    else
    {
      snprintf(buf, 10, "%07.1f", ws.sensor[0].w.rain_mm);
      log_d("Rain: %07.1f", ws.sensor[0].w.rain_mm);
      payload[12] = ((buf[0] - '0') << 4) | (buf[1] - '0');
      payload[13] = ((buf[2] - '0') << 4) | (buf[3] - '0');
      payload[14] = ((buf[4] - '0') << 4) | (buf[6] - '0');
      payload[12] ^= 0xFF;
      payload[13] ^= 0xFF;
      payload[14] ^= 0xFF;
      payload[16] = 1; // Flags: !temp_ok
      msg_type = 0;
    }
  }

  snprintf(buf, 10, "%04.1f", ws.sensor[0].w.uv);
  log_d("UV: %04.1f", ws.sensor[0].w.uv);
  payload[15] = ((buf[0] - '0') << 4) | (buf[1] - '0');
  payload[16] |= ((buf[3] - '0') << 4);
  payload[15] ^= 0xFF;
  payload[16] ^= 0xF0;
  
  int sum = add_bytes(&payload[2], 15);
  int chk = 0xFF - (sum & 0xFF);
  log_d("Checksum: 0x%02X vs 0x%02X", chk, payload[17]);
  payload[17] = chk;

  // int crc = crc16(&payload[2], 16, 0x1021 /* polynomial */, 0 /* init */);
  // int digest = crc ^ 0xE359;
  //  log_d("CRC: 0x%04X", crc ^ 0xE359);
  int digest = lfsr_digest16(&payload[2], 15, 0x8810, 0x5412);
  payload[0] = digest >> 8;
  payload[1] = digest & 0xFF;

  memcpy(msg, payload, 18);

  // Return message size
  return 18;
}

void loop()
{
  static unsigned tx_interval = TX_INTERVAL;
  uint8_t msg_buf[40];
  uint8_t msg_size;
  bool valid = true;

  //get weather data from HomaAssistant
  //ws.sensor[0].w.humidity = 0.0;
  //while (ws.sensor[0].w.humidity < 10.0) {
  getData();
  //}
  
  msg_size = msgBegin(msg_buf);
  msg_size += encodeBresser6In1Payload(&msg_buf[msg_size]);

  // Transmitting
  log_i("%s Transmitting packet (%d bytes)... ", TRANSCEIVER_CHIP, msg_size);
  int state = radio.transmit(msg_buf, msg_size);

  // wait for TX_INTERVAL seconds before transmitting again
  delay(tx_interval * 1000);

} // end loop

//
// From from rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/util.c
//
int add_bytes(uint8_t const message[], unsigned num_bytes)
{
  int result = 0;
  for (unsigned i = 0; i < num_bytes; ++i)
  {
    result += message[i];
  }
  return result;
}

//
// From from rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/util.c
//
uint16_t lfsr_digest16(uint8_t const message[], unsigned bytes, uint16_t gen, uint16_t key)
{
  uint16_t sum = 0;
  for (unsigned k = 0; k < bytes; ++k)
  {
    uint8_t data = message[k];
    for (int i = 7; i >= 0; --i)
    {
      // fprintf(stderr, "key at bit %d : %04x\n", i, key);
      // if data bit is set then xor with key
      if ((data >> i) & 1)
        sum ^= key;

      // roll the key right (actually the lsb is dropped here)
      // and apply the gen (needs to include the dropped lsb as msb)
      if (key & 1)
        key = (key >> 1) ^ gen;
      else
        key = (key >> 1);
    }
  }
  return sum;
}

//
// From from rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/util.c
//
uint16_t crc16(uint8_t const message[], unsigned nBytes, uint16_t polynomial, uint16_t init)
{
  uint16_t remainder = init;
  unsigned byte, bit;

  for (byte = 0; byte < nBytes; ++byte)
  {
    remainder ^= message[byte] << 8;
    for (bit = 0; bit < 8; ++bit)
    {
      if (remainder & 0x8000)
      {
        remainder = (remainder << 1) ^ polynomial;
      }
      else
      {
        remainder = (remainder << 1);
      }
    }
  }
  return remainder;
}
