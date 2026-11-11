#ifndef THERMISTOR_H
#define THERMISTOR_H

#include <Arduino.h>
#include "config.h"

enum SensorStatus {
    SENSOR_OK = 0,
    SENSOR_ERR_OPEN,        // Thermistor disconnected or open circuit (ADC high)
    SENSOR_ERR_SHORT,       // Thermistor leads shorted together (ADC low)
    SENSOR_ERR_OUT_OF_BOUNDS// Reading outside physical bounds (5°C to 285°C)
};

class Thermistor {
public:
    Thermistor(uint8_t pin);
    void begin();
    void update();

    float getTemperatureC() const;
    float getRawResistance() const;
    float getRawAdc() const;
    SensorStatus getStatus() const;
    const char* getStatusString() const;
    bool isHealthy() const;

private:
    uint8_t _pin;
    float _filteredTempC;
    float _lastRawTempC;
    float _lastResistance;
    float _lastAdc;
    SensorStatus _status;
    bool _isInitialized;

    float readAdcAveraged();
    float calculateTemperatureFromAdc(float adcVal);
};

#endif // THERMISTOR_H

