///////////////////////////////////////////////////////////////////////////////////////////////////
// BresserTransmitter.h - 02-08-2026
//
// Bresser 6-in-1 868 MHz Sensor Radio Transmitter
// based on CC1101 and ESP32, reduced version from:
// https://github.com/matthias-bs/SensorTransmitter created: 11/2026
//
// This is used to transmit data to a Bresser weather base station. 
// At the moment only 3in1 7902531 base station belonging to Productnummer 7002531 is supported.
// One needs the original ID which is hardcoded in the header-file.
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
#ifndef BresserTransmitter_h
#define BresserTransmitter_h
#include "Arduino.h"
#include "WeatherSensor.h"

//class BresserTransmitter
//{
//  public:
    //BresserTransmitter();
    int msgBegin(uint8_t *msg);
    uint8_t encodeBresser6In1Payload(uint8_t *msg, WeatherSensor ws);

//  private:
    int add_bytes(uint8_t const message[], unsigned num_bytes);
    uint16_t lfsr_digest16(uint8_t const message[], unsigned bytes, uint16_t gen, uint16_t key);
    uint16_t crc16(uint8_t const message[], unsigned nBytes, uint16_t polynomial, uint16_t init);
    static unsigned _tx_interval;
    //bool _valid = true;
//};

#endif