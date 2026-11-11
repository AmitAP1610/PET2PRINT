#include "extruder.h"

Extruder::Extruder(uint8_t stepPin, uint8_t dirPin, uint8_t enPin)
    : _stepPin(stepPin),
      _dirPin(dirPin),
      _enPin(enPin),
      _state(MOTOR_STATE_DISABLED),
      _direction(MOTOR_DIR_FORWARD),
      _stepIntervalUs(STEP_INTERVAL_US),
      _lastStepMicros(0),
      _totalSteps(0) {}

void Extruder::begin() {
    pinMode(_stepPin, OUTPUT);
    digitalWrite(_stepPin, LOW);

    pinMode(_dirPin, OUTPUT);
    digitalWrite(_dirPin, _direction);

    pinMode(_enPin, OUTPUT);
    // TMC2209 EN is active LOW: set HIGH at boot to keep motor coils de-energized
    enableDriver(false);

    _state = MOTOR_STATE_DISABLED;
    _lastStepMicros = micros();
}

void Extruder::enableDriver(bool enable) {
    // TMC2209: LOW = Output stages enabled, HIGH = Disabled (high-impedance)
    digitalWrite(_enPin, enable ? LOW : HIGH);
}

void Extruder::emergencyStop() {
    _state = MOTOR_STATE_FAULT;
    enableDriver(false);
    digitalWrite(_stepPin, LOW);
}

void Extruder::setDirection(uint8_t dir) {
    _direction = dir;
    digitalWrite(_dirPin, _direction);
}

void Extruder::setSpeedIntervalUs(uint32_t intervalUs) {
    if (intervalUs < 100) intervalUs = 100; // Floor safeguard
    _stepIntervalUs = intervalUs;
}

void Extruder::update(bool heaterActive, bool extrusionAllowed, bool isFaulted) {
    if (isFaulted) {
        emergencyStop();
        return;
    }

    if (!heaterActive) {
        // Heater in standby / off: disable stepper motor
        enableDriver(false);
        _state = MOTOR_STATE_DISABLED;
        return;
    }

    if (!extrusionAllowed) {
        // Heater warming up or outside allowable extrusion temperature window
        enableDriver(false);
        _state = MOTOR_STATE_COLD_LOCK;
        return;
    }

    // Extrusion condition satisfied: enable driver and generate stepping pulses
    _state = MOTOR_STATE_RUNNING;
    enableDriver(true);

    unsigned long currentMicros = micros();
    if ((currentMicros - _lastStepMicros) >= _stepIntervalUs) {
        _lastStepMicros = currentMicros;

        // Produce clean step pulse for TMC2209
        digitalWrite(_stepPin, HIGH);
        delayMicroseconds(STEP_PULSE_WIDTH_US);
        digitalWrite(_stepPin, LOW);

        _totalSteps++;
    }
}

MotorState Extruder::getState() const {
    return _state;
}

const char* Extruder::getStateString() const {
    switch (_state) {
        case MOTOR_STATE_DISABLED:  return "OFF";
        case MOTOR_STATE_COLD_LOCK: return "COLD LOCK";
        case MOTOR_STATE_RUNNING:   return "RUNNING";
        case MOTOR_STATE_FAULT:     return "FAULT";
        default:                    return "UNKNOWN";
    }
}

uint32_t Extruder::getStepCount() const {
    return _totalSteps;
}

uint32_t Extruder::getSpeedIntervalUs() const {
    return _stepIntervalUs;
}
