#include <Wire.h>

#define HAS_VBUS_SENSE 1

enum class MAX17048_REG
{
    VCELL = 0x02,
    SOC = 0x04,
    MODE = 0x06,
    VERSION = 0x08,
    HIBRT = 0x0A,
    CONFIG = 0x0C,
    VALRT = 0x14,
    CRATE = 0x16,
    VRESET_ID = 0x18,
    STATUS = 0x1A,
    TABLE = 0x40,
    CMD = 0xFE
};

const uint8_t I2C_ADDR = 0x36;

float getBatteryVoltage()
{
    return ((float)i2c_read(MAX17048_REG::VCELL) * 78.125f / 1000000.f);
}

uint8_t FG_version()
{
    return (uint8_t)i2c_read(MAX17048_REG::VERSION);
}

bool getVbusPresent()
{
    return digitalRead(VBUS_SENSE);
}

/* I2C communication for MAX17048 FG*/
void i2c_write(const MAX17048_REG reg)
{
    Wire.beginTransmission(I2C_ADDR);
    Wire.write((uint8_t)reg);
    Wire.endTransmission();
}

void i2c_write(const MAX17048_REG reg, const uint16_t data)
{
    Wire.beginTransmission(I2C_ADDR);
    Wire.write((uint8_t)reg);
    Wire.write((data & 0xFF00) >> 8);
    Wire.write((data & 0x00FF) >> 0);
    Wire.endTransmission();
}

uint16_t i2c_read(const MAX17048_REG reg)
{
    i2c_write(reg);
    Wire.requestFrom((uint8_t)I2C_ADDR, (uint8_t)2); // 2byte R/W only
    uint16_t data = (uint16_t)((Wire.read() << 8) & 0xFF00);
    data |= (uint16_t)(Wire.read() & 0x00FF);
    return data;
}

void setup()
{
    Serial.begin(115200);

    // Delay to allow native USB to kick in to get serial output
    delay(2000);

    // Initialize all board peripherals, call this first
    Serial.println("begin");
 
    // We initialise the I2C peripheral outside of the helper library and pass the reference in
    // In case you want to use the BUS for other I2C peripherals as well.
    Wire.begin();

    // Print the MAX17048 FG version
    Serial.printf("MAX17048 version: %d\n", FG_version());
}

// Gets the battery voltage and shows it using the neopixel LED.
// These values are all approximate, you should do your own testing and
// find values that work for you.
void checkBattery()
{
    // Get the battery voltage, corrected for the on-board voltage divider
    // Full should be around 4.2v and empty should be around 3v
    float battery = getBatteryVoltage();

    if (getVbusPresent())
    {
        
        Serial.printf("Running from 5V - Battery: %fV\n", battery);
    }
    else
    {
        
        Serial.printf("Running from Battery: %fV\n", battery);
    }
}

// Store the millis of the last battery check
unsigned long lastBatteryCheck = 0;
// Define the battery check interval as five seconds
#define BATTERY_CHECK_INTERVAL 5000

void loop()
{
    if (lastBatteryCheck == 0 || millis() - lastBatteryCheck > BATTERY_CHECK_INTERVAL)
    {
        checkBattery();
        lastBatteryCheck = millis();
    }
}