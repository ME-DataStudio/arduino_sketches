///////////////////////////////////////////////////////////////////////////////////////////////////
// WeatherSensorDecoders.cpp
//
// Sensor data decoding functions
//
// Bresser 5-in-1/6-in-1/7-in1 868 MHz Weather Sensor Radio Receiver
// based on CC1101 or SX1276/RFM95W and ESP32/ESP8266
//
// https://github.com/matthias-bs/BresserWeatherSensorReceiver
//
// Based on:
// ---------
// Bresser5in1-CC1101 by Sean Siford (https://github.com/seaniefs/Bresser5in1-CC1101)
// RadioLib by Jan Gromeš (https://github.com/jgromes/RadioLib)
// rtl433 by Benjamin Larsson (https://github.com/merbanan/rtl_433)
//     - https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_5in1.c
//     - https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_6in1.c
//     - https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_7in1.c
//
// created: 05/2024
//
//
// MIT License
//
// Copyright (c) 2024 Matthias Prinke
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
//
// 20240513 Created from WeatherSensor.cpp
// 20240603 Updated decodeBresser5In1Payload() according to
//          https://github.com/merbanan/rtl_433/commit/271bed886c5b1ff7c1a47e6cf1366e397aeb8364
//          and
//          https://github.com/merbanan/rtl_433/commit/9928efe5c8d55e9ca01f1ebab9e8b20b0e7ba01e
// 20240716 Added assignment of sensor.decoder
// 20250127 Added SENSOR_TYPE_WEATHER8 (8-in-1 Weather Sensor)
// 20250129 Minor change in SENSOR_TYPE_WEATHER8 handling
// 20260224 Removed obsolete variable f_3in1 and related code in decodeBresser6In1Payload()
//          Fixed High Precision Thermo Hygro Sensor (P/N 7009971) in decodeBresser6In1Payload()
// 20260306 Added missing 0x prefix for ID in verbose log message
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "WeatherSensorCfg.h"
#include "WeatherSensor.h"


DecodeStatus WeatherSensor::decodeMessage(const uint8_t *msg, uint8_t msgSize)
{
    DecodeStatus decode_res = DECODE_INVALID;

#ifdef BRESSER_7_IN_1
    if (enDecoders & DECODER_7IN1) {
        decode_res = decodeBresser7In1Payload(msg, msgSize);
        if (decode_res == DECODE_OK ||
            decode_res == DECODE_FULL ||
            decode_res == DECODE_SKIP)
        {
            return decode_res;
        }
    }
#endif
#ifdef BRESSER_6_IN_1
    if (enDecoders & DECODER_6IN1) {
        decode_res = decodeBresser6In1Payload(msg, msgSize);
        if (decode_res == DECODE_OK ||
            decode_res == DECODE_FULL ||
            decode_res == DECODE_SKIP)
        {
            return decode_res;
        }
    }
#endif
#ifdef BRESSER_5_IN_1
    if (enDecoders & DECODER_5IN1) {
        decode_res = decodeBresser5In1Payload(msg, msgSize);
        if (decode_res == DECODE_OK ||
            decode_res == DECODE_FULL ||
            decode_res == DECODE_SKIP)
        {
            return decode_res;
        }
    }
#endif
    return decode_res;
}

//
// From rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_5in1.c (20220212)
//
// Example input data:
//   EA EC 7F EB 5F EE EF FA FE 76 BB FA FF 15 13 80 14 A0 11 10 05 01 89 44 05 00
//   CC CC CC CC CC CC CC CC CC CC CC CC CC uu II sS GG DG WW  W TT  T HH RR RR Bt
// - C = check, inverted data of 13 byte further
// - uu = checksum (number/count of set bits within bytes 14-25)
// - I = station ID (maybe)
// - s = startup, MSb is 0b0 after power-on/reset and 0b1 after 1 hour
// - S = sensor type, 0x9/0xA/0xB for Bresser Professional Rain Gauge
// - G = wind gust in 1/10 m/s, normal binary coded, GGxG = 0x76D1 => 0x0176 = 256 + 118 = 374 => 37.4 m/s.  MSB is out of sequence.
// - D = wind direction 0..F = N..NNE..E..S..W..NNW
// - W = wind speed in 1/10 m/s, BCD coded, WWxW = 0x7512 => 0x0275 = 275 => 27.5 m/s. MSB is out of sequence.
// - T = temperature in 1/10 °C, BCD coded, TTxT = 1203 => 31.2 °C, 0xf on error
// - t = temperature sign, minus if unequal 0
// - H = humidity in percent, BCD coded, HH = 23 => 23 %, 0xf on error
// - R = rain in mm, BCD coded, RRRR = 1203 => 031.2 mm
// - B = battery. 0=Ok, 8=Low
// - s = startup, 0 after power-on/reset / 8 after 1 hour
// - S = sensor type, only low nibble used, 0x9 for Bresser Professional Rain Gauge
//
// Parameters:
//
// msg     - Pointer to message
// msgSize - Size of message
// pOut    - Pointer to WeatherData
//
// Returns:
//
// DECODE_OK      - OK - WeatherData will contain the updated information
// DECODE_PAR_ERR - Parity Error
// DECODE_CHK_ERR - Checksum Error
//
#ifdef BRESSER_5_IN_1
DecodeStatus WeatherSensor::decodeBresser5In1Payload(const uint8_t *msg, uint8_t msgSize)
{
    // First 13 bytes need to match inverse of last 13 bytes
    for (unsigned col = 0; col < msgSize / 2; ++col)
    {
        if ((msg[col] ^ msg[col + 13]) != 0xff)
        {
            log_d("Parity wrong at column %d", col);
            return DECODE_PAR_ERR;
        }
    }

    // Verify checksum (number bits set in bytes 14-25)
    uint8_t bitsSet = 0;
    uint8_t expectedBitsSet = msg[13];

    for (uint8_t p = 14; p < msgSize; p++)
    {
        uint8_t currentByte = msg[p];
        while (currentByte)
        {
            bitsSet += (currentByte & 1);
            currentByte >>= 1;
        }
    }

    if (bitsSet != expectedBitsSet)
    {
        log_d("Checksum wrong - actual [%02X] != [%02X]", bitsSet, expectedBitsSet);
        return DECODE_CHK_ERR;
    }

    uint8_t id_tmp = msg[14];
    uint8_t type_tmp = msg[15] & 0x7F;
    DecodeStatus status;

    sensor.sensor_id = id_tmp;
    sensor.chan = 0; // for compatibility with other decoders
    sensor.startup = ((msg[15] & 0x80) == 0) ? true : false;
    sensor.battery_ok = (msg[25] & 0x80) ? false : true;
    sensor.valid = true;
    sensor.rssi = rssi;
    sensor.complete = true;

    int temp_raw = (msg[20] & 0x0f) + ((msg[20] & 0xf0) >> 4) * 10 + (msg[21] & 0x0f) * 100;
    if (msg[25] & 0x0f)
    {
        temp_raw = -temp_raw;
    }
    sensor.w.temp_c = temp_raw * 0.1f;

    sensor.w.humidity = (msg[22] & 0x0f) + ((msg[22] & 0xf0) >> 4) * 10;

    int wind_direction_raw = ((msg[17] & 0xf0) >> 4) * 225;
    int gust_raw = ((msg[17] & 0x0f) << 8) + msg[16];
    int wind_raw = (msg[18] & 0x0f) + ((msg[18] & 0xf0) >> 4) * 10 + (msg[19] & 0x0f) * 100;

#ifdef WIND_DATA_FLOATINGPOINT
    sensor.w.wind_direction_deg = wind_direction_raw * 0.1f;
    sensor.w.wind_gust_meter_sec = gust_raw * 0.1f;
    sensor.w.wind_avg_meter_sec = wind_raw * 0.1f;
#endif
#ifdef WIND_DATA_FIXEDPOINT
    sensor.w.wind_direction_deg_fp1 = wind_direction_raw;
    sensor.w.wind_gust_meter_sec_fp1 = gust_raw;
    sensor.w.wind_avg_meter_sec_fp1 = wind_raw;
#endif

    int rain_raw = (msg[23] & 0x0f) + ((msg[23] & 0xf0) >> 4) * 10 + (msg[24] & 0x0f) * 100 + ((msg[24] & 0xf0) >> 4) * 1000;
    sensor.w.rain_mm = rain_raw * 0.1f;

    // Check if the message is from a Bresser Professional Rain Gauge
    // The sensor type for the Rain Gauge can be either 0x9, 0xA, or 0xB. The
    // value changes between resets, and the meaning of the two least
    // significant bits is unknown.
    // The Bresser Lightning Sensor has type 0x9, too -
    // we change the type to SENSOR_TYPE_WEATHER0 here to simplify processing by the application.
    if ((type_tmp >= 0x39) && (type_tmp <= 0x3b))
    {
        // rescale the rain sensor readings
        sensor.w.rain_mm *= 2.5;
        type_tmp = SENSOR_TYPE_WEATHER0;

        // Rain Gauge has no humidity (according to description) and no wind sensor (obviously)
        sensor.w.humidity_ok = false;
        sensor.w.wind_ok = false;
    }
    else
    {
        sensor.w.wind_ok = true;
        sensor.w.humidity_ok = (msg[22] & 0x0f) <= 9; // BCD, 0x0f on error
    }

    sensor.s_type = type_tmp;
    sensor.decoder = DECODER_5IN1;
    sensor.w.temp_ok = (msg[20] & 0x0f) <= 9; // BCD, 0x0f on error
    sensor.w.light_ok = false;
    sensor.w.uv_ok = false;
    sensor.w.rain_ok = true;

    log_d("sensor: v=%d id=0x%08X t=%d c=%d", sensor.valid, (unsigned int)sensor.sensor_id, sensor.s_type, sensor.complete);

    return DECODE_OK;
}
#endif

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
//
// Parameters:
//
//  msg     - Pointer to message
//  msgSize - Size of message
//  pOut    - Pointer to WeatherData
//
//  Returns:
//
//  DECODE_OK      - OK - WeatherData will contain the updated information
//  DECODE_DIG_ERR - Digest Check Error
//  DECODE_CHK_ERR - Checksum Error
#ifdef BRESSER_6_IN_1
DecodeStatus WeatherSensor::decodeBresser6In1Payload(const uint8_t *msg, uint8_t msgSize)
{
    (void)msgSize;                                                                             // unused parameter - kept for consistency with other decoders; avoid warning
    int const moisture_map[] = {0, 7, 13, 20, 27, 33, 40, 47, 53, 60, 67, 73, 80, 87, 93, 99}; // scale is 20/3

    // Per-message status flags
    bool temp_ok = false;
    bool humidity_ok = false;
    bool uv_ok = false;
    bool wind_ok = false;
    bool rain_ok = false;

    // LFSR-16 digest, generator 0x8810 init 0x5412
    int chkdgst = (msg[0] << 8) | msg[1];
    int digest = lfsr_digest16(&msg[2], 15, 0x8810, 0x5412);
    if (chkdgst != digest)
    {
        log_d("Digest check failed - [%02X] != [%02X]", chkdgst, digest);
        return DECODE_DIG_ERR;
    }
    // Checksum, add with carry
    int sum = add_bytes(&msg[2], 16); // msg[2] to msg[17]
    if ((sum & 0xff) != 0xff)
    {
        log_d("Checksum failed");
        return DECODE_CHK_ERR;
    }

    uint32_t id_tmp = ((uint32_t)msg[2] << 24) | (msg[3] << 16) | (msg[4] << 8) | (msg[5]);
    uint8_t type_tmp = (msg[6] >> 4); // 1: weather station, 2: indoor?, 4: soil probe
    uint8_t chan_tmp = (msg[6] & 0x7);
    uint8_t flags = (msg[16] & 0x0f);
    DecodeStatus status;

    sensor.sensor_id = id_tmp;
    sensor.s_type = type_tmp;
    sensor.chan = chan_tmp;
    sensor.decoder = DECODER_6IN1;
    sensor.startup = ((msg[6] & 0x8) == 0) ? true : false; // s.a. #1214
    sensor.battery_ok = (msg[13] >> 1) & 1;                // b[13] & 0x02 is battery_good, s.a. #1993

    // temperature, humidity(, uv) - shared with rain counter
    temp_ok = humidity_ok = (flags == 0);
    float temp = 0;
    if (temp_ok)
    {
        bool sign = (msg[13] >> 3) & 1;
        int temp_raw = (msg[12] >> 4) * 100 + (msg[12] & 0x0f) * 10 + (msg[13] >> 4);

        temp = ((sign) ? (temp_raw - 1000) : temp_raw) * 0.1f;

        // Correction for Bresser 3-in-1 Professional Wind Gauge / Anemometer, PN 7002531
        // The temperature range (as far as provided in other Bresser manuals) is -40...+60°C
        if (temp < -50.0)
        {
            temp = -temp_raw * 0.1f;
        }

        sensor.w.temp_c = temp;
        sensor.w.humidity = (msg[14] >> 4) * 10 + (msg[14] & 0x0f);

        // apparently ff01 or 0000 if not available, ???0 if valid, inverted BCD
        uv_ok = ((~msg[15] & 0xff) <= 0x99) && ((~msg[16] & 0xf0) <= 0x90);
        if (uv_ok)
        {
            int uv_raw = ((~msg[15] & 0xf0) >> 4) * 100 + (~msg[15] & 0x0f) * 10 + ((~msg[16] & 0xf0) >> 4);
            sensor.w.uv = uv_raw * 0.1f;
        }
    }

    // int unk_ok  = (msg[16] & 0xf0) == 0xf0;
    // int unk_raw = ((msg[15] & 0xf0) >> 4) * 10 + (msg[15] & 0x0f);

    // invert 3 bytes wind speeds
    uint8_t _imsg7 = msg[7] ^ 0xff;
    uint8_t _imsg8 = msg[8] ^ 0xff;
    uint8_t _imsg9 = msg[9] ^ 0xff;

    wind_ok = (_imsg7 <= 0x99) && (_imsg8 <= 0x99) && (_imsg9 <= 0x99);
    if (wind_ok)
    {
        int gust_raw = (_imsg7 >> 4) * 100 + (_imsg7 & 0x0f) * 10 + (_imsg8 >> 4);
        int wavg_raw = (_imsg9 >> 4) * 100 + (_imsg9 & 0x0f) * 10 + (_imsg8 & 0x0f);
        int wind_dir_raw = ((msg[10] & 0xf0) >> 4) * 100 + (msg[10] & 0x0f) * 10 + ((msg[11] & 0xf0) >> 4);

#ifdef WIND_DATA_FLOATINGPOINT
        sensor.w.wind_gust_meter_sec = gust_raw * 0.1f;
        sensor.w.wind_avg_meter_sec = wavg_raw * 0.1f;
        sensor.w.wind_direction_deg = wind_dir_raw * 1.0f;
#endif
#ifdef WIND_DATA_FIXEDPOINT
        sensor.w.wind_gust_meter_sec_fp1 = gust_raw;
        sensor.w.wind_avg_meter_sec_fp1 = wavg_raw;
        sensor.w.wind_direction_deg_fp1 = wind_dir_raw * 10;
#endif
    }

    // rain counter, inverted 3 bytes BCD - shared with temp/hum
    uint8_t _imsg12 = msg[12] ^ 0xff;
    uint8_t _imsg13 = msg[13] ^ 0xff;
    uint8_t _imsg14 = msg[14] ^ 0xff;

    rain_ok = (flags == 1) && (type_tmp == 1);
    if (rain_ok)
    {
        int rain_raw = (_imsg12 >> 4) * 100000 + (_imsg12 & 0x0f) * 10000 + (_imsg13 >> 4) * 1000 + (_imsg13 & 0x0f) * 100 + (_imsg14 >> 4) * 10 + (_imsg14 & 0x0f);
        sensor.w.rain_mm = rain_raw * 0.1f;
    }

    // Pool / Spa thermometer
    if (sensor.s_type == SENSOR_TYPE_POOL_THERMO)
    {
        humidity_ok = false;
    }

    // The thermo hygro sensor and the soil moisture sensor might present valid readings but do not have the hardware
    if ((sensor.s_type == SENSOR_TYPE_SOIL) || (sensor.s_type == SENSOR_TYPE_THERMO_HYGRO))
    {
        wind_ok = 0;
        uv_ok = 0;
    }

    if (sensor.s_type == SENSOR_TYPE_SOIL && temp_ok && sensor.w.humidity >= 1 && sensor.w.humidity <= 16)
    {
        humidity_ok = false;
        sensor.soil.moisture = moisture_map[sensor.w.humidity - 1];
        sensor.soil.temp_c = temp;
    }

    // Update per-slot status flags
    sensor.w.temp_ok |= temp_ok;
    sensor.w.humidity_ok |= humidity_ok;
    sensor.w.uv_ok |= uv_ok;
    sensor.w.wind_ok |= wind_ok;
    sensor.w.rain_ok |= rain_ok;
    log_d("Flags: Temp=%d  Hum=%d  Wind=%d  Rain=%d  UV=%d", temp_ok, humidity_ok, wind_ok, rain_ok, uv_ok);

    sensor.valid = true;

    // Weather station data is split into two separate messages (except for Professional Wind Gauge)
    if (sensor.s_type == SENSOR_TYPE_WEATHER1)
    {
        if (sensor.w.temp_ok && sensor.w.rain_ok)
        {
            sensor.complete = true;
        }
    }
    else
    {
        sensor.complete = true;
    }

    // Save rssi to sensor specific data set
    sensor.rssi = rssi;

    log_d("sensor: v=%d id=0x%08X t=%d c=%d", sensor.valid, (unsigned int)sensor.sensor_id, sensor.s_type, sensor.complete);
    return DECODE_OK;
}
#endif

//
// From rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_7in1.c (20230215)
//
/**
Decoder for Bresser Weather Center 7-in-1 and Air quality sensors.
- Air Quality PM2.5/PM10 PN 7009970
- CO2 sensor             PN 7009977
- HCHO/VOC sensor        PN 7009978
- 3-in-1 Weather Station PN 7002530
- 8-in-1 Weather Station PN 7003150

See https://github.com/merbanan/rtl_433/issues/1492
Preamble:
    aa aa aa aa aa 2d d4
Observed length depends on reset_limit.
The data (not including STYPE, STARTUP, CH and maybe ID) has a whitening of 0xaa.

Weather Center
Data layout:
    {271}631d05c09e9a18abaabaaaaaaaaa8adacbacff9cafcaaaaaaa000000000000000000
    {262}10b8b4a5a3ca10aaaaaaaaaaaaaa8bcacbaaaa2aaaaaaaaaaa0000000000000000 [0.08 klx]
    {220}543bb4a5a3ca10aaaaaaaaaaaaaa8bcacbaaaa28aaaaaaaaaa00000 [0.08 klx]
    {273}2492b4a5a3ca10aaaaaaaaaaaaaa8bdacbaaaa2daaaaaaaaaa0000000000000000000 [0.08klx]
    {269}9a59b4a5a3da10aaaaaaaaaaaaaa8bdac8afea28a8caaaaaaa000000000000000000 [54.0 klx UV=2.6]
    {230}fe15b4a5a3da10aaaaaaaaaaaaaa8bdacbba382aacdaaaaaaa00000000 [109.2klx   UV=6.7]
    {254}2544b4a5a32a10aaaaaaaaaaaaaa8bdac88aaaaabeaaaaaaaa00000000000000 [200.000 klx UV=14
    DIGEST:8h8h ID?8h8h WDIR:8h4h 4h STYPE:4h STARTUP:1b CH:3d WGUST:8h.4h WAVG:8h.4h RAIN:8h8h4h.4h RAIN?:8h TEMP:8h.4hC FLAGS?:4h HUM:8h% LIGHT:8h4h,8h4hKL UV:8h.4h TRAILER:8h8h8h4h
Unit of light is kLux (not W/m²).

Air Quality Sensor PM2.5 / PM10 Sensor (PN 7009970)
Data layout:
DIGEST:8h8h ID?8h8h ?8h8h STYPE:4h STARTUP:1b CH:3b ?8h 4h ?4h8h4h PM_2_5:4h8h4h PM10:4h8h4h ?4h ?8h4h BATT:1b ?3b ?8h8h8h8h8h8h TRAILER:8h8h8h

Air Quality Sensor CO2 (PN 7009977) : issue #2813

From user manual , co2 ppm is from 400 to 5000 ppm, so it's 16 bits coded.

Samples :
Raw :
                  SType Startup & Channel

                      | |
    {207}dab6d782acd9 a 1 ad9aad9aad9aaaaaaaaaaaaaaaaae99aaaaa00 Type = 0xa = 10, Startup = 0, ch = 1
    {207}04a9d782acd8 a 1 ad9aad9aad9aaaaaaaaaaaaaaaaae99aaaaa00 Type = 0xa = 10, Startup = 0, ch = 1
    {207}04a9d782acd8 a 1 ad9aad9aad9aaaaaaaaaaaaaaaaae99aaaaa00 Type = 0xa = 10, Startup = 0, ch = 1
    {207}0dd1d782b8ee a 1 ad9aad9aad9aaaaaaaaaaaaaaaaae99aaaaa00 Type = 0xa = 10, Startup = 0, ch = 1

Data layout raw :
    DIGEST:16h ID:16h 8x8x STYPE:4h STARTUP:1b CH:3d 8x8x8x8x8x8x8x8x8x8x8x8x8x8x8x8x8x8x TRAILER:8x

XOR / de-whitened :

          0 1  2 3  4 5  6 7 8 9101112131415161718192021222324
       DIGEST   ID  ppm                  bat
            |    |    |                    |
    {200}701c 7d28 0673 0b073007300730000000000000000043300000 [ XOR from g001_868.34M_1000k.cu8 co2 ppm  673]
    {200}ae03 7d28 0672 0b073007300730000000000000000043300000 [ XOR from g001_868.34M_250k.cu8  co2 ppm  672]
    {200}ae03 7d28 0672 0b073007300730000000000000000043300000 [ XOR from g002_868.34M_1000k.cu8 co2 ppm  672]
    {200}a77b 7d28 1244 0b073007300730000000000000000043300000 [ XOR from g002_868.34M_250k.cu8  co2 ppm 1244]

Data layout de-whitened :
    DIGEST:16h ID:16h PPM:16h 8x8x8x8x8x8x8x8x8x8x4x BATT:1b 3x8x8x8x8x8x8x TRAILER:16x

Air Quality Sensor HCHO/VOC (PN 7009978) : issue #2814

From user manual , hcho ppb is from 0 to 1000 ppm, so it's 16 bits coded.
              and voc level is from 1 (bad air quality) to 5 (good air quality), so it's 4 bits coded.

Samples:
Raw :
                  SType Startup & Channel
                      | |
    {207}3f2dc4a5aaaf b 1 aaa8aaa8aaa8aaaaaaaaaaaaaaaae9feaaaa00 Type = 0xb = 11, Startup = 0, ch = 1
    {207}0c1cc4a5aaaf b 1 aaa8aaa8aaa8aaaaaaaaaaaaaaaae9ffaaaa00 Type = 0xb = 11, Startup = 0, ch = 1
    {207}3f2dc4a5aaaf b 1 aaa8aaa8aaa8aaaaaaaaaaaaaaaae9feaaaa00 Type = 0xb = 11, Startup = 0, ch = 1
    {207}0c1cc4a5aaaf b 1 aaa8aaa8aaa8aaaaaaaaaaaaaaaae9ffaaaa00 Type = 0xb = 11, Startup = 0, ch = 1
    {207}61afc4a5aaa2 b 9 aaa8aaa8aaa9aaaaaaaaaaaaaaaae9f8aaaa00 Type = 0xb = 11, Startup = 1, ch = 1
    {207}ecddc4a5aaae b 9 aaa8aaa8aaa9aaaaaaaaaaaaaaaae9fbaaaa00 Type = 0xb = 11, Startup = 1, ch = 1

Data layout raw :
    DIGEST:16h ID:16h 8x8x STYPE:4h STARTUP:1b CH:3d 8x8x8x8x8x8x8x8x8x8x8x8x8x8x8x8x8x8x TRAILER:8x

XOR / de-whitened :

          0 1  2 3  4 5  6 7 8 9101112131415161718192021 22 2324
       DIGEST   ID  ppb                  bat            voc
            |    |    |                    |              |
    {200}9587 6e0f 0005 1b0002000200020000000000000000435 4 0000 [XOR from g001_868.34M_1000k.cu8 hcho_ppb 5 voc_level 4]
    {200}a6b6 6e0f 0005 1b0002000200020000000000000000435 5 0000 [XOR from g001_868.34M_250k.cu8  hcho_ppb 5 voc_level 5]
    {200}9587 6e0f 0005 1b0002000200020000000000000000435 4 0000 [XOR from g002_868.34M_1000k.cu8 hcho_ppb 5 voc_level 4]
    {200}a6b6 6e0f 0005 1b0002000200020000000000000000435 5 0000 [XOR from g001_868.34M_250k.cu8  hcho_ppb 5 voc_level 5]
    {200}cb05 6e0f 0008 130002000200030000000000000000435 2 0000 [XOR from g003_868.34M_1000k.cu8 hcho_ppb 8 voc_level 2]
    {200}4677 6e0f 0004 130002000200030000000000000000435 1 0000 [XOR from g004_868.34M_1000k.cu8 hcho_ppb 4 voc_level 1]

Data layout de-whitened :
    DIGEST:16h ID:16h PPB:16h 8x8x8x8x8x8x8x8x8x8x4x BATT:1b 3x8x8x8x8x8x4x VOC:4h TRAILER:16x

#2816 Bresser Air Quality sensors, ignore first packet:
    The first signal is not sending the good BCD values , all at 0xF and need to be excluded from result (BCD value can't be > 9) .

First two bytes are an LFSR-16 digest, generator 0x8810 key 0xba95 with a final xor 0x6df1, which likely means we got that wrong.
*/
#ifdef BRESSER_7_IN_1
DecodeStatus WeatherSensor::decodeBresser7In1Payload(const uint8_t *msg, uint8_t msgSize)
{

    if (msg[21] == 0x00)
    {
        log_d("Data sanity check failed");
    }

    // data de-whitening
    uint8_t msgw[MSG_BUF_SIZE];
    for (unsigned i = 0; i < msgSize; ++i)
    {
        msgw[i] = msg[i] ^ 0xaa;
    }

    // LFSR-16 digest, generator 0x8810 key 0xba95 final xor 0x6df1
    int chkdgst = (msgw[0] << 8) | msgw[1];
    int digest = lfsr_digest16(&msgw[2], 23, 0x8810, 0xba95); // bresser_7in1
    if ((chkdgst ^ digest) != 0x6df1)
    { // bresser_7in1
        log_d("Digest check failed - [%04X] vs [%04X] (%04X)", chkdgst, digest, chkdgst ^ digest);
        return DECODE_DIG_ERR;
    }

#if CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    log_message("De-whitened Data", msgw, msgSize);
#endif

    int id_tmp = (msgw[2] << 8) | (msgw[3]);
    int s_type = msg[6] >> 4; // raw data, no de-whitening

    DecodeStatus status;

    int flags = (msgw[15] & 0x0f);
    int battery_low = (flags & 0x06) == 0x06;

    sensor.sensor_id = id_tmp;
    sensor.s_type = s_type;
    sensor.startup = (msg[6] & 0x08) == 0x00; // raw data, no de-whitening
    sensor.chan = msg[6] & 0x07;              // raw data, no de-whitening
    sensor.decoder = DECODER_7IN1;
    sensor.battery_ok = !battery_low;
    sensor.valid = true;
    sensor.complete = true;
    sensor.rssi = rssi;

    if ((s_type == SENSOR_TYPE_WEATHER1) || (s_type == SENSOR_TYPE_WEATHER3) || (s_type == SENSOR_TYPE_WEATHER8))
    {
        // 3-in-1 (type 12) features Temp/Hum/Rain only
        bool wind_light_ok = (s_type != SENSOR_TYPE_WEATHER3);

        sensor.w.tglobe_ok = false;
        int wdir = (msgw[4] >> 4) * 100 + (msgw[4] & 0x0f) * 10 + (msgw[5] >> 4);
        int wgst_raw = (msgw[7] >> 4) * 100 + (msgw[7] & 0x0f) * 10 + (msgw[8] >> 4);
        int wavg_raw = (msgw[8] & 0x0f) * 100 + (msgw[9] >> 4) * 10 + (msgw[9] & 0x0f);
        int rain_raw = (msgw[10] >> 4) * 100000 + (msgw[10] & 0x0f) * 10000 + (msgw[11] >> 4) * 1000 + (msgw[11] & 0x0f) * 100 + (msgw[12] >> 4) * 10 + (msgw[12] & 0x0f) * 1; // 6 digits
        float rain_mm = rain_raw * 0.1f;
        int temp_raw = (msgw[14] >> 4) * 100 + (msgw[14] & 0x0f) * 10 + (msgw[15] >> 4);
        float temp_c = temp_raw * 0.1f;
        if (temp_raw > 600)
            temp_c = (temp_raw - 1000) * 0.1f;
        int humidity = (msgw[16] >> 4) * 10 + (msgw[16] & 0x0f);
        int lght_raw = (msgw[17] >> 4) * 100000 + (msgw[17] & 0x0f) * 10000 + (msgw[18] >> 4) * 1000 + (msgw[18] & 0x0f) * 100 + (msgw[19] >> 4) * 10 + (msgw[19] & 0x0f);
        int uv_raw = (msgw[20] >> 4) * 100 + (msgw[20] & 0x0f) * 10 + (msgw[21] >> 4);

        float light_klx = lght_raw * 0.001f; // TODO: remove this
        float light_lux = lght_raw;
        float uv_index = uv_raw * 0.1f;

        // The RTL_433 decoder does not include any field to verify that these data
        // are ok, so we are assuming that they are ok if the decode status is ok.
        sensor.w.temp_ok = true;
        sensor.w.humidity_ok = true;
        sensor.w.wind_ok = wind_light_ok;
        sensor.w.rain_ok = true;
        sensor.w.light_ok = wind_light_ok;
        sensor.w.uv_ok = wind_light_ok;
        sensor.w.temp_c = temp_c;
        sensor.w.humidity = humidity;
#ifdef WIND_DATA_FLOATINGPOINT
        sensor.w.wind_gust_meter_sec = wgst_raw * 0.1f;
        sensor.w.wind_avg_meter_sec = wavg_raw * 0.1f;
        sensor.w.wind_direction_deg = wdir * 1.0f;
#endif
#ifdef WIND_DATA_FIXEDPOINT
        sensor.w.wind_gust_meter_sec_fp1 = wgst_raw;
        sensor.w.wind_avg_meter_sec_fp1 = wavg_raw;
        sensor.w.wind_direction_deg_fp1 = wdir * 10;
#endif
        sensor.w.rain_mm = rain_mm;
        sensor.w.light_klx = light_klx;
        sensor.w.light_lux = light_lux;
        sensor.w.uv = uv_index;

        if (s_type == SENSOR_TYPE_WEATHER8)
        {
            // 8-in-1 sensor
            if ((msgw[23] >> 4) < 10) {
                sensor.w.tglobe_ok = true;
            }
            sensor.w.tglobe_c = (msgw[22] >> 4) * 10 + (msgw[22] & 0x0f) + (msgw[23] >> 4) * 0.1f;
        }
    }
    else if (s_type == SENSOR_TYPE_AIR_PM)
    {
#if CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
        uint16_t pn1 = (msgw[14] & 0x0f) * 1000 + (msgw[15] >> 4) * 100 + (msgw[15] & 0x0f) * 10 + (msgw[16] >> 4);
        uint16_t pn2 = (msgw[17] >> 4) * 100 + (msgw[17] & 0x0f) * 10 + (msgw[18] >> 4);
        uint16_t pn3 = (msgw[19] >> 4) * 100 + (msgw[19] & 0x0f) * 10 + (msgw[20] >> 4);
#endif
        log_d("PN1: %04d PN2: %04d PN3: %04d", pn1, pn2, pn3);
        sensor.pm.pm_1_0 = (msgw[8] & 0x0f) * 1000 + (msgw[9] >> 4) * 100 + (msgw[9] & 0x0f) * 10 + (msgw[10] >> 4);
        sensor.pm.pm_2_5 = (msgw[10] & 0x0f) * 1000 + (msgw[11] >> 4) * 100 + (msgw[11] & 0x0f) * 10 + (msgw[12] >> 4);
        sensor.pm.pm_10 = (msgw[12] & 0x0f) * 1000 + (msgw[13] >> 4) * 100 + (msgw[13] & 0x0f) * 10 + (msgw[14] >> 4);
        sensor.pm.pm_1_0_init = ((msgw[10] >> 4) & 0x0f) == 0x0f;
        sensor.pm.pm_2_5_init = ((msgw[12] >> 4) & 0x0f) == 0x0f;
        sensor.pm.pm_10_init = ((msgw[14] >> 4) & 0x0f) == 0x0f;
    }
    

    //const int i = slot;
    log_d("sensor: v=%d id=0x%08X t=%d c=%d", sensor.valid, (unsigned int)sensor.sensor_id, sensor.s_type, sensor.complete);

    return DECODE_OK;
}
#endif