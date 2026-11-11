#include "thermistor.h"
#include <math.h>

Thermistor::Thermistor(uint8_t pin)
    : _pin(pin),
      _filteredTempC(25.0f),
      _lastRawTempC(25.0f),
      _lastResistance(100000.0f),
      _lastAdc(0.0f),
      _status(SENSOR_OK),
      _isInitialized(false) {}

void Thermistor::begin() {
    pinMode(_pin, INPUT);
    // Initial priming read
    update();
    if (_status == SENSOR_OK) {
        _filteredTempC = _lastRawTempC;
        _isInitialized = true;
    }
}

float Thermistor::readAdcAveraged() {
    uint32_t adcSum = 0;
    for (uint8_t i = 0; i < THERMISTOR_SAMPLES; i++) {
        adcSum += analogRead(_pin);
        delayMicroseconds(60); // Allow ADC capacitor settling between samples
    }
    return (float)adcSum / (float)THERMISTOR_SAMPLES;
}

float Thermistor::calculateTemperatureFromAdc(float adcVal) {
    // Guard against edge cases where ADC would cause division by zero or log of non-positive
    if (adcVal >= (ADC_MAX_VALUE - 8.0f)) {
        _status = SENSOR_ERR_OPEN;
        return -999.0f;
    }
    if (adcVal <= 8.0f) {
        _status = SENSOR_ERR_SHORT;
        return 999.0f;
    }

    // Circuit: 5V -> Pullup (4.7k) -> A2 (Thermistor) -> GND
    // V_A2 = Vcc * (R_ntc / (R_pullup + R_ntc))
    // R_ntc = R_pullup * (ADC / (ADC_MAX - ADC))
    _lastResistance = PULLUP_RESISTOR_OHMS * (adcVal / (ADC_MAX_VALUE - adcVal));

    if (_lastResistance <= 1.0f) {
        _status = SENSOR_ERR_SHORT;
        return 999.0f;
    }

    // Steinhart-Hart / Beta equation:
    // 1 / T = 1 / T0 + (1 / Beta) * ln(R / R0)
    const float t0Kelvin = TEMPERATURE_NOMINAL_C + 273.15f;
    float steinhart = (1.0f / t0Kelvin) + (1.0f / THERMISTOR_BETA) * log(_lastResistance / THERMISTOR_NOMINAL_R);
    float tempKelvin = 1.0f / steinhart;
    float tempCelsius = tempKelvin - 273.15f;

    // Plausibility boundary check
    if (tempCelsius < TEMP_MIN_PLAUSIBLE_C) {
        _status = SENSOR_ERR_OPEN;
    } else if (tempCelsius > TEMP_MAX_PLAUSIBLE_C) {
        _status = SENSOR_ERR_SHORT;
    } else {
        _status = SENSOR_OK;
    }

    return tempCelsius;
}

void Thermistor::update() {
    _lastAdc = readAdcAveraged();
    float rawTemp = calculateTemperatureFromAdc(_lastAdc);

    if (_status == SENSOR_OK) {
        _lastRawTempC = rawTemp;
        if (!_isInitialized) {
            _filteredTempC = rawTemp;
            _isInitialized = true;
        } else {
            // Exponential moving average filter (alpha = 0.20 for smooth yet responsive readings)
            const float alpha = 0.20f;
            _filteredTempC = (alpha * rawTemp) + ((1.0f - alpha) * _filteredTempC);
        }
    } else {
        // Retain unplausible raw reading for fault logging
        _lastRawTempC = rawTemp;
    }
}

float Thermistor::getTemperatureC() const {
    return _filteredTempC;
}

float Thermistor::getRawResistance() const {
    return _lastResistance;
}

float Thermistor::getRawAdc() const {
    return _lastAdc;
}

SensorStatus Thermistor::getStatus() const {
    return _status;
}

const char* Thermistor::getStatusString() const {
    switch (_status) {
        case SENSOR_OK:                 return "OK";
        case SENSOR_ERR_OPEN:           return "ERR: OPEN CIRCUIT";
        case SENSOR_ERR_SHORT:          return "ERR: SHORT CIRCUIT";
        case SENSOR_ERR_OUT_OF_BOUNDS:  return "ERR: OUT OF BOUNDS";
        default:                        return "ERR: UNKNOWN";
    }
}

bool Thermistor::isHealthy() const {
    return (_status == SENSOR_OK);
}

