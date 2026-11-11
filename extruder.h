#ifndef EXTRUDER_H
#define EXTRUDER_H

#include <Arduino.h>
#include "config.h"

enum MotorState {
    MOTOR_STATE_DISABLED = 0,   // Heater OFF / Standby, motor coils unpowered
    MOTOR_STATE_COLD_LOCK,      // Hotend below extrusion threshold, feed locked
    MOTOR_STATE_RUNNING,        // Safe temperature reached, active feeding
    MOTOR_STATE_FAULT           // System safety fault, motor disabled immediately
};

class Extruder {
public:
    Extruder(uint8_t stepPin, uint8_t dirPin, uint8_t enPin);
    void begin();
    void update(bool heaterActive, bool extrusionAllowed, bool isFaulted);

    void setSpeedIntervalUs(uint32_t intervalUs);
    void setDirection(uint8_t dir);
    void emergencyStop();

    MotorState getState() const;
    const char* getStateString() const;
    uint32_t getStepCount() const;
    uint32_t getSpeedIntervalUs() const;

private:
    uint8_t _stepPin;
    uint8_t _dirPin;
    uint8_t _enPin;

    MotorState _state;
    uint8_t _direction;
    uint32_t _stepIntervalUs;
    unsigned long _lastStepMicros;
    uint32_t _totalSteps;

    void enableDriver(bool enable);
};

#endif // EXTRUDER_H
