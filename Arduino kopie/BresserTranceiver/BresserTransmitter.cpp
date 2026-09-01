///////////////////////////////////////////////////////////////////////////////////////////////////
// BresserTransmitter.cpp - 02-08-2026
//
// Bresser 6-in-1 868 MHz Sensor Radio Transmitter
// based on CC1101 and ESP32, reduced version from:
// https://github.com/matthias-bs/SensorTransmitter created: 11/2026
//
// This is used to transmit data to a Bresser weather base station. 
// At the moment only 3in1 7902531 base station belonging to Productnummer 7002531 is supported.
// One needs the original ID which is hardcoded in the header-file.
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
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <Arduino.h>
#include <BresserTransmitter.h>
#include "WeatherSensor.h"

// this config has worked. Only to keep it it is still in this file.
//   ConfigFSK_t config;
//   config.frequency = 868.3;              // MHz
//   config.bitRate = 8.21;                 // kBaud
//   config.frequencyDeviation = 57.136417; // kHz
//   config.power = TX_POWER;               // dBm
//   config.preambleLength = 32;            // bits
//   // RX bandwidth in kHz, radio chip specific (see RadioLib documentation for details)
//   config.receiverBandwidth = 270;

int msgBegin(uint8_t *msg)
{
  uint8_t preamble[] = {0xAA, 0xAA, 0xAA, 0xAA};
  uint8_t syncword[] = {0x2D, 0xD4};

  memcpy(msg, preamble, sizeof(preamble));
  memcpy(&msg[sizeof(preamble)], syncword, sizeof(syncword));

  return sizeof(preamble) + sizeof(syncword);
}

uint8_t encodeBresser6In1Payload(uint8_t *msg, WeatherSensor ws)
{
  static int msg_type = 0;
  char buf[10];  // Buffer for snprintf formats: %07.1f (10 bytes), %04.1f (6 bytes), %02d (3 bytes)
  uint8_t payload[18] = {0};

  payload[2] = ws.sensor.sensor_id >> 24;
  payload[3] = (ws.sensor.sensor_id >> 16) & 0xFF;
  payload[4] = (ws.sensor.sensor_id >> 8) & 0xFF;
  payload[5] = (ws.sensor.sensor_id) & 0xFF;
  payload[6] = ws.sensor.s_type << 4;
  payload[6] |= (ws.sensor.startup ? 0 : 8) | ws.sensor.chan;
  Serial.print(payload[2],HEX);
  Serial.print(payload[3],HEX);
  Serial.print(payload[4],HEX);
  Serial.println(payload[5],HEX);




  snprintf(buf, 10, "%04.1f", ws.sensor.w.wind_gust_meter_sec);
  log_d("Wind gust: %04.1f", ws.sensor.w.wind_gust_meter_sec);
  payload[7] = ((buf[0] - '0') << 4) | (buf[1] - '0');
  payload[8] = (buf[3] - '0') << 4;

  snprintf(buf, 10, "%04.1f", ws.sensor.w.wind_avg_meter_sec);
  log_d("Wind avg: %04.1f", ws.sensor.w.wind_avg_meter_sec);
  payload[9] = ((buf[0] - '0') << 4) | (buf[1] - '0');
  payload[8] |= buf[3] - '0';

  // Invert bytes
  payload[7] ^= 0xFF;
  payload[8] ^= 0xFF;
  payload[9] ^= 0xFF;

  snprintf(buf, 10, "%03d", (int)ws.sensor.w.wind_direction_deg);
  log_d("Wind dir: %03d", (int)ws.sensor.w.wind_direction_deg);
  payload[10] = ((buf[0] - '0') << 4) | (buf[1] - '0');
  payload[11] = (buf[2] - '0') << 4;

  if (ws.sensor.s_type == SENSOR_TYPE_WEATHER1)
  {
    if (msg_type == 0)
    {
      float temp_c;
      temp_c = ws.sensor.w.temp_c;
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
      payload[13] |= ((buf[3] - '0') << 4) | (ws.sensor.battery_ok ? 2 : 0);
      payload[16] = 0; // Flags: temp_ok

      if (ws.sensor.s_type == SENSOR_TYPE_WEATHER1)
      {
        snprintf(buf, 10, "%02d", ws.sensor.w.humidity);
        payload[14] = ((buf[0] - '0') << 4) | (buf[1] - '0');
      }

      if (ws.sensor.s_type == SENSOR_TYPE_WEATHER1)
      {
        msg_type = 1;
      }
    } // msg_type == 0
    else
    {
      snprintf(buf, 10, "%07.1f", ws.sensor.w.rain_mm);
      log_d("Rain: %07.1f", ws.sensor.w.rain_mm);
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

  snprintf(buf, 10, "%04.1f", ws.sensor.w.uv);
  log_d("UV: %04.1f", ws.sensor.w.uv);
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

void setMsgSize(uint8_t)
{

}

// void loop()
// {
//   static unsigned tx_interval = TX_INTERVAL;
//   uint8_t msg_buf[40];
//   uint8_t msg_size;
//   bool valid = true;

//   //get weather data from HomaAssistant
//   //ws.sensor.w.humidity = 0.0;
//   //while (ws.sensor.w.humidity < 10.0) {
//   //getData();
//   //}
  
//   msg_size = msgBegin(msg_buf);
//   msg_size += encodeBresser6In1Payload(&msg_buf[msg_size]);

//   // Transmitting
//   log_i("%s Transmitting packet (%d bytes)... ", TRANSCEIVER_CHIP, msg_size);
//   int state = radio.transmit(msg_buf, msg_size);

//   // wait for TX_INTERVAL seconds before transmitting again
//   delay(tx_interval * 1000);

// } // end loop