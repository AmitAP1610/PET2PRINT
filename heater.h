#ifndef HEATER_H
#define HEATER_H

#include <Arduino.h>
#include "PID_v1.h"
#include "config.h"
#include "thermistor.h"

enum HeaterState {
    HEATER_STATE_OFF = 0,
    HEATER_STATE_HEATING,
    HEATER_STATE_STABILIZED,
    HEATER_STATE_FAULT
};

enum HeaterFault {
    FAULT_NONE = 0,
    FAULT_SENSOR_OPEN,
    FAULT_SENSOR_SHORT,
    FAULT_OVERTEMP,
    FAULT_THERMAL_RUNAWAY
};

class Heater {
public:
    Heater(uint8_t pwmPin, Thermistor& thermistor);
    void begin();
    void update(float targetTemp);

    void shutdownEmergency(HeaterFault reason);
    void resetFault();

    HeaterState getState() const;
    HeaterFault getFault() const;
    const char* getStateString() const;
    const char* getFaultString() const;

    float getTargetTemp() const;
    float getCurrentTemp() const;
    float getPwmOutput() const;
    uint8_t getPwmDutyPercent() const;
    bool isExtrusionReady() const;

    void setPidGains(double kp, double ki, double kd);

private:
    uint8_t _pwmPin;
    Thermistor& _thermistor;

    double _pidInput;
    double _pidOutput;
    double _pidSetpoint;

    double _kp;
    double _ki;
    double _kd;

    PID _pid;
    HeaterState _state;
    HeaterFault _fault;

    // Thermal Runaway Watchdog Variables
    unsigned long _runawayWatchdogStartMs;
    float _runawayWatchdogStartTemp;
    bool _runawayTimerActive;
    bool _hasReachedTargetOnce;

    void evaluateSafety();
    void manageThermalRunawayWatchdog();
    void applyPwm(uint8_t pwmVal);
};

#endif // HEATER_H
