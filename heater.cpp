#include "heater.h"
#include <math.h>

Heater::Heater(uint8_t pwmPin, Thermistor& thermistor)
    : _pwmPin(pwmPin),
      _thermistor(thermistor),
      _pidInput(25.0),
      _pidOutput(0.0),
      _pidSetpoint(0.0),
      _kp(PID_KP_DEFAULT),
      _ki(PID_KI_DEFAULT),
      _kd(PID_KD_DEFAULT),
      _pid(&_pidInput, &_pidOutput, &_pidSetpoint, _kp, _ki, _kd, DIRECT),
      _state(HEATER_STATE_OFF),
      _fault(FAULT_NONE),
      _runawayWatchdogStartMs(0),
      _runawayWatchdogStartTemp(0.0f),
      _runawayTimerActive(false),
      _hasReachedTargetOnce(false) {}

void Heater::begin() {
    pinMode(_pwmPin, OUTPUT);
    digitalWrite(_pwmPin, LOW); // Hold MOSFET OFF immediately at boot

    _pid.SetMode(MANUAL);
    _pid.SetOutputLimits(PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    _pid.SetSampleTime(PID_SAMPLE_TIME_MS);
    _pidOutput = 0.0;
}

void Heater::applyPwm(uint8_t pwmVal) {
    if (_state == HEATER_STATE_FAULT || _fault != FAULT_NONE) {
        analogWrite(_pwmPin, 0);
        digitalWrite(_pwmPin, LOW);
        return;
    }
    analogWrite(_pwmPin, pwmVal);
}

void Heater::shutdownEmergency(HeaterFault reason) {
    _state = HEATER_STATE_FAULT;
    _fault = reason;
    _pid.SetMode(MANUAL);
    _pidOutput = 0.0;
    applyPwm(0);

    Serial.print(F("[CRITICAL SAFETY FAULT] Heater emergency shutdown! Reason: "));
    Serial.println(getFaultString());
}

void Heater::resetFault() {
    if (_fault != FAULT_NONE && _thermistor.isHealthy() && _thermistor.getTemperatureC() < TEMP_ABSOLUTE_MAX_C) {
        _fault = FAULT_NONE;
        _state = HEATER_STATE_OFF;
        _runawayTimerActive = false;
        _hasReachedTargetOnce = false;
        _pidOutput = 0.0;
        applyPwm(0);
        Serial.println(F("[SAFETY] Fault state manually cleared. Heater in STANDBY."));
    }
}

void Heater::manageThermalRunawayWatchdog() {
    unsigned long currentMs = millis();
    float currentTemp = _thermistor.getTemperatureC();

    // Check if target has been reached at least once (within 3°C)
    if (!_hasReachedTargetOnce && (_pidSetpoint > 0.0) && (currentTemp >= (_pidSetpoint - 3.0f))) {
        _hasReachedTargetOnce = true;
    }

    // Condition 1: High power applied, but temp is below target
    if (_pidOutput >= RUNAWAY_TRIGGER_PWM && currentTemp < (_pidSetpoint - 5.0f)) {
        if (!_runawayTimerActive) {
            _runawayTimerActive = true;
            _runawayWatchdogStartMs = currentMs;
            _runawayWatchdogStartTemp = currentTemp;
        } else {
            if ((currentMs - _runawayWatchdogStartMs) >= RUNAWAY_TIMEOUT_MS) {
                float tempDelta = currentTemp - _runawayWatchdogStartTemp;
                if (tempDelta < RUNAWAY_MIN_RISE_C) {
                    shutdownEmergency(FAULT_THERMAL_RUNAWAY);
                    return;
                }
                // Reset rolling window for next evaluation period
                _runawayWatchdogStartMs = currentMs;
                _runawayWatchdogStartTemp = currentTemp;
            }
        }
    } else {
        // Power is moderate or target reached; reset runaway heating timer
        _runawayTimerActive = false;
    }

    // Condition 2: Sudden large temperature drop while previously stabilized
    if (_hasReachedTargetOnce && (_pidSetpoint > 0.0)) {
        if (currentTemp < (_pidSetpoint - RUNAWAY_MAX_DROP_C) && _pidOutput >= RUNAWAY_TRIGGER_PWM) {
            shutdownEmergency(FAULT_THERMAL_RUNAWAY);
            return;
        }
    }
}

void Heater::evaluateSafety() {
    // 1. Verify thermistor sensor health
    SensorStatus sStatus = _thermistor.getStatus();
    if (sStatus == SENSOR_ERR_OPEN) {
        shutdownEmergency(FAULT_SENSOR_OPEN);
        return;
    } else if (sStatus == SENSOR_ERR_SHORT || sStatus == SENSOR_ERR_OUT_OF_BOUNDS) {
        shutdownEmergency(FAULT_SENSOR_SHORT);
        return;
    }

    // 2. Absolute over-temperature software cutoff
    float currentTemp = _thermistor.getTemperatureC();
    if (currentTemp >= TEMP_ABSOLUTE_MAX_C) {
        shutdownEmergency(FAULT_OVERTEMP);
        return;
    }

    // 3. Thermal runaway watchdog
    if (_state != HEATER_STATE_OFF && _state != HEATER_STATE_FAULT) {
        manageThermalRunawayWatchdog();
    }
}

void Heater::update(float targetTemp) {
    _pidInput = _thermistor.getTemperatureC();

    // Check safety continuously
    evaluateSafety();

    if (_fault != FAULT_NONE || _state == HEATER_STATE_FAULT) {
        applyPwm(0);
        return;
    }

    // Handle Setpoint changes / Standby
    if (targetTemp <= 0.0f) {
        _state = HEATER_STATE_OFF;
        _pidSetpoint = 0.0;
        _pid.SetMode(MANUAL);
        _pidOutput = 0.0;
        _runawayTimerActive = false;
        _hasReachedTargetOnce = false;
        applyPwm(0);
        return;
    }

    // Active heating mode
    _pidSetpoint = targetTemp;
    _pid.SetMode(AUTOMATIC);
    _pid.Compute();

    // State classification
    float tempDiff = fabs((float)_pidInput - (float)_pidSetpoint);
    if (tempDiff <= 2.5f) {
        _state = HEATER_STATE_STABILIZED;
    } else {
        _state = HEATER_STATE_HEATING;
    }

    applyPwm((uint8_t)_pidOutput);
}

HeaterState Heater::getState() const {
    return _state;
}

HeaterFault Heater::getFault() const {
    return _fault;
}

const char* Heater::getStateString() const {
    switch (_state) {
        case HEATER_STATE_OFF:        return "OFF";
        case HEATER_STATE_HEATING:    return "HEATING";
        case HEATER_STATE_STABILIZED: return "READY";
        case HEATER_STATE_FAULT:      return "FAULT";
        default:                      return "UNKNOWN";
    }
}

const char* Heater::getFaultString() const {
    switch (_fault) {
        case FAULT_NONE:             return "NO FAULT";
        case FAULT_SENSOR_OPEN:      return "SENSOR DISCONNECTED (OPEN)";
        case FAULT_SENSOR_SHORT:     return "SENSOR SHORT CIRCUIT";
        case FAULT_OVERTEMP:         return "OVERTEMPERATURE CUTOFF";
        case FAULT_THERMAL_RUNAWAY:  return "THERMAL RUNAWAY DETECTED";
        default:                     return "UNKNOWN FAULT";
    }
}

float Heater::getTargetTemp() const {
    return (float)_pidSetpoint;
}

float Heater::getCurrentTemp() const {
    return (float)_pidInput;
}

float Heater::getPwmOutput() const {
    return (float)_pidOutput;
}

uint8_t Heater::getPwmDutyPercent() const {
    return (uint8_t)((_pidOutput / 255.0f) * 100.0f + 0.5f);
}

bool Heater::isExtrusionReady() const {
    if (_state == HEATER_STATE_FAULT || _fault != FAULT_NONE) return false;
    if (_pidSetpoint <= 0.0) return false;
    float currentT = _thermistor.getTemperatureC();
    return (currentT >= MIN_EXTRUSION_TEMP_C) && (currentT >= (_pidSetpoint - EXTRUSION_TEMP_WINDOW_C));
}

void Heater::setPidGains(double kp, double ki, double kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
    _pid.SetTunings(_kp, _ki, _kd);
}
