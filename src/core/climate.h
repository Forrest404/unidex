#pragma once

struct SensorReading {
    float temperatureC;
    float humidityRH;
};

// Reads the SHTC3.
// Returns false if communication or data validation fails.
bool sensorRead(SensorReading &out);
