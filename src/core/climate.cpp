#include "climate.h"

#include <Arduino.h>
#include <Wire.h>

namespace {

constexpr uint8_t SHTC3_ADDR = 0x70;

// SHTC3 commands.
constexpr uint16_t CMD_WAKEUP = 0x3517;
constexpr uint16_t CMD_MEASURE = 0x7866;
constexpr uint16_t CMD_SLEEP = 0xB098;

bool sendCommand(uint16_t command) {
    Wire.beginTransmission(SHTC3_ADDR);
    Wire.write(static_cast<uint8_t>(command >> 8));
    Wire.write(static_cast<uint8_t>(command & 0xFF));
    return Wire.endTransmission() == 0;
}

// SHTC3 uses CRC-8, polynomial 0x31, initial value 0xFF.
uint8_t crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0xFF;

    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];

        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80)
                ? static_cast<uint8_t>((crc << 1) ^ 0x31)
                : static_cast<uint8_t>(crc << 1);
        }
    }

    return crc;
}

}  // namespace

bool sensorRead(SensorReading &out) {
    // The I2C bus is initialised by clockBegin().
    if (!sendCommand(CMD_WAKEUP)) {
        return false;
    }

    delay(1);

    if (!sendCommand(CMD_MEASURE)) {
        sendCommand(CMD_SLEEP);
        return false;
    }

    // Allow the measurement to complete.
    delay(20);

    uint8_t data[6];

    if (Wire.requestFrom(
            static_cast<uint16_t>(SHTC3_ADDR),
            static_cast<size_t>(6)) != 6) {
        sendCommand(CMD_SLEEP);
        return false;
    }

    for (uint8_t &byte : data) {
        byte = static_cast<uint8_t>(Wire.read());
    }

    // Validate the temperature and humidity independently.
    if (crc8(data, 2) != data[2] ||
        crc8(data + 3, 2) != data[5]) {
        sendCommand(CMD_SLEEP);
        return false;
    }

    const uint16_t rawTemperature =
        (static_cast<uint16_t>(data[0]) << 8) | data[1];

    const uint16_t rawHumidity =
        (static_cast<uint16_t>(data[3]) << 8) | data[4];

    // Convert the raw measurements using the SHTC3 formulas.
    out.temperatureC =
        -45.0f + 175.0f * rawTemperature / 65535.0f;

    out.humidityRH =
        100.0f * rawHumidity / 65535.0f;

    sendCommand(CMD_SLEEP);

    return true;
}
