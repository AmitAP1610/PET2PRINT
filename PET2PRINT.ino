/*
 * =============================================================================
 * PET2PRINT - PET Bottle Recycling & Filament Extrusion System
 * Firmware Version: 1.0.0
 * Target Controller: Arduino UNO R4 WiFi
 * 
 * Hardware Architecture:
 * - Heater Switching : TC4420 Gate Driver + IRFZ44N Low-Side MOSFET
 * - Extruder Drive    : TMC2209 Driver + NEMA17 Geared Stepper (27:1)
 * - Temperature Sensor: 100k NTC Thermistor (Beta 3950) + 4.7k Pullup Divider
 * - Operator Controls : B10K Potentiometer (A0) Setpoint Control
 * - Visual Display    : SSD1306 128x64 I2C OLED (0x3C)
 * =============================================================================
 */

#include <Arduino.h>
#include "config.h"
#include "thermistor.h"
#include "heater.h"
#include "extruder.h"
#include "display.h"

#define FIRMWARE_VERSION "1.0.0"

// Subsystem Instances
Thermistor thermistor(PIN_THERMISTOR);
Heater heater(PIN_HEATER_PWM, thermistor);
Extruder extruder(PIN_MOTOR_STEP, PIN_MOTOR_DIR, PIN_MOTOR_EN);
DisplayManager display;

// Runtime Global Variables
float currentSetpointC = 0.0f;
float lastFilteredPotAdc = 0.0f;
unsigned long lastTelemetryMs = 0;

// Function Prototypes
float readSetpointPotentiometer();
void handleSerialCommands();
void sendTelemetry();
void printHelp();
void printDiagnostics();

void setup() {
    // 1. Initialize Serial Communication
    Serial.begin(SERIAL_BAUD_RATE);
    // Allow brief time for USB CDC connection on UNO R4
    unsigned long startSerial = millis();
    while (!Serial && (millis() - startSerial < 1500));

    Serial.println();
    Serial.println(F("=================================================="));
    Serial.println(F("   PET2PRINT - FILAMENT EXTRUSION CONTROLLER      "));
    Serial.print(F("   Firmware v"));
    Serial.println(F(FIRMWARE_VERSION));
    Serial.println(F("   Target: Arduino UNO R4 WiFi                    "));
    Serial.println(F("=================================================="));

    // 2. Configure ADC Resolution for Renesas RA4M1 Core (UNO R4)
#if defined(ARDUINO_UNOR4_WIFI) || defined(ARDUINO_UNOR4_MINIMA)
    analogReadResolution(ADC_RESOLUTION_BITS);
    Serial.print(F("[INIT] ADC configured to "));
    Serial.print(ADC_RESOLUTION_BITS);
    Serial.println(F("-bit resolution (0-4095)."));
#else
    Serial.println(F("[INIT] Standard 10-bit ADC resolution active."));
#endif

    // 3. Initialize Operator Controls
    pinMode(PIN_POT_SETPOINT, INPUT);

    // 4. Initialize Subsystems
    Serial.println(F("[INIT] Initializing Display..."));
    if (display.begin()) {
        display.showBootScreen(FIRMWARE_VERSION);
    }

    Serial.println(F("[INIT] Initializing Thermistor subsystem..."));
    thermistor.begin();

    Serial.println(F("[INIT] Initializing Heater & PID regulation..."));
    heater.begin();

    Serial.println(F("[INIT] Initializing TMC2209 Stepper driver..."));
    extruder.begin();

    // 5. Initial Potentiometer Reading
    currentSetpointC = readSetpointPotentiometer();

    Serial.println(F("[INIT] All subsystems initialized. Starting main loop."));
    Serial.println(F("[INFO] Type 'HELP' or '?' in Serial Monitor for commands."));
    Serial.println(F("--------------------------------------------------"));
}

void loop() {
    // -------------------------------------------------------------------------
    // 1. Update Sensor Readings
    // -------------------------------------------------------------------------
    thermistor.update();

    // -------------------------------------------------------------------------
    // 2. Read Operator Temperature Setpoint
    // -------------------------------------------------------------------------
    currentSetpointC = readSetpointPotentiometer();

    // -------------------------------------------------------------------------
    // 3. Update Heater & Safety Logic
    // -------------------------------------------------------------------------
    heater.update(currentSetpointC);

    // -------------------------------------------------------------------------
    // 4. Update Extruder Feed (Cold Extrusion Prevention & Safety Lockout)
    // -------------------------------------------------------------------------
    bool heaterActive = (heater.getTargetTemp() > 0.0f);
    bool extrusionAllowed = heater.isExtrusionReady();
    bool isFaulted = (heater.getFault() != FAULT_NONE);
    extruder.update(heaterActive, extrusionAllowed, isFaulted);

    // -------------------------------------------------------------------------
    // 5. Update OLED User Interface
    // -------------------------------------------------------------------------
    display.update(heater, extruder, lastFilteredPotAdc);

    // -------------------------------------------------------------------------
    // 6. Serial Telemetry & Command Handling
    // -------------------------------------------------------------------------
    handleSerialCommands();

    unsigned long currentMs = millis();
    if (currentMs - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
        lastTelemetryMs = currentMs;
        sendTelemetry();
    }
}

// -----------------------------------------------------------------------------
// Read & Filter Setpoint Potentiometer (B10K on A0)
// -----------------------------------------------------------------------------
float readSetpointPotentiometer() {
    // Read raw ADC with smoothing
    uint32_t potSum = 0;
    const uint8_t POT_SAMPLES = 8;
    for (uint8_t i = 0; i < POT_SAMPLES; i++) {
        potSum += analogRead(PIN_POT_SETPOINT);
    }
    float rawAdc = (float)potSum / (float)POT_SAMPLES;

    // Exponential smoothing to prevent target jitter
    if (lastFilteredPotAdc == 0.0f) {
        lastFilteredPotAdc = rawAdc;
    } else {
        lastFilteredPotAdc = (0.25f * rawAdc) + (0.75f * lastFilteredPotAdc);
    }

    // Threshold check: turn off heater when pot is near minimum
    if (lastFilteredPotAdc < POT_OFF_ADC_THRESHOLD) {
        return 0.0f; // Standby / Heater OFF
    }

    // Map ADC range [POT_OFF_ADC_THRESHOLD .. ADC_MAX_VALUE] to [SETPOINT_MIN_C .. SETPOINT_MAX_C]
    float normalized = (lastFilteredPotAdc - POT_OFF_ADC_THRESHOLD) / (ADC_MAX_VALUE - POT_OFF_ADC_THRESHOLD);
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    float target = SETPOINT_MIN_C + (normalized * (SETPOINT_MAX_C - SETPOINT_MIN_C));
    return round(target); // Round to integer degrees for stable setpoint
}

// -----------------------------------------------------------------------------
// Serial Telemetry
// -----------------------------------------------------------------------------
void sendTelemetry() {
    Serial.print(F("[TEL] Target:"));
    Serial.print(heater.getTargetTemp(), 1);
    Serial.print(F(",Actual:"));
    Serial.print(heater.getCurrentTemp(), 1);
    Serial.print(F(",PWM:"));
    Serial.print((int)heater.getPwmOutput());
    Serial.print(F(",Duty:"));
    Serial.print(heater.getPwmDutyPercent());
    Serial.print(F("%,Heater:"));
    Serial.print(heater.getStateString());
    Serial.print(F(",Feed:"));
    Serial.print(extruder.getStateString());
    Serial.print(F(",Steps:"));
    Serial.print(extruder.getStepCount());

    if (heater.getFault() != FAULT_NONE) {
        Serial.print(F(",FAULT:"));
        Serial.print(heater.getFaultString());
    }
    Serial.println();
}

// -----------------------------------------------------------------------------
// Serial Command Processor
// -----------------------------------------------------------------------------
void handleSerialCommands() {
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd.length() == 0) return;

    if (cmd == "?" || cmd == "HELP") {
        printHelp();
    } else if (cmd == "STATUS" || cmd == "DIAG") {
        printDiagnostics();
    } else if (cmd == "RESET") {
        heater.resetFault();
    } else if (cmd.startsWith("SPEED ")) {
        long newInterval = cmd.substring(6).toInt();
        if (newInterval >= 150 && newInterval <= 20000) {
            extruder.setSpeedIntervalUs((uint32_t)newInterval);
            Serial.print(F("[OK] Step interval set to: "));
            Serial.print(newInterval);
            Serial.println(F(" us"));
        } else {
            Serial.println(F("[ERR] Speed out of range (150 - 20000 us)."));
        }
    } else if (cmd.startsWith("PID ")) {
        // Format: PID <Kp> <Ki> <Kd>
        int firstSpace = cmd.indexOf(' ', 4);
        int secondSpace = cmd.indexOf(' ', firstSpace + 1);
        if (firstSpace > 0 && secondSpace > 0) {
            double kp = cmd.substring(4, firstSpace).toDouble();
            double ki = cmd.substring(firstSpace + 1, secondSpace).toDouble();
            double kd = cmd.substring(secondSpace + 1).toDouble();
            heater.setPidGains(kp, ki, kd);
            Serial.print(F("[OK] PID gains updated to Kp="));
            Serial.print(kp);
            Serial.print(F(" Ki="));
            Serial.print(ki);
            Serial.print(F(" Kd="));
            Serial.println(kd);
        } else {
            Serial.println(F("[ERR] Usage: PID <Kp> <Ki> <Kd>"));
        }
    } else {
        Serial.print(F("[ERR] Unknown command: '"));
        Serial.print(cmd);
        Serial.println(F("'. Type 'HELP' for command list."));
    }
}

void printHelp() {
    Serial.println();
    Serial.println(F("============= PET2PRINT SERIAL COMMANDS ============="));
    Serial.println(F("HELP or ?           : Print this help menu"));
    Serial.println(F("STATUS or DIAG      : Print detailed hardware diagnostics"));
    Serial.println(F("RESET               : Clear latched safety fault (if condition resolved)"));
    Serial.println(F("SPEED <us>          : Set motor step interval in microseconds (150-20000)"));
    Serial.println(F("PID <Kp> <Ki> <Kd>  : Adjust PID tuning coefficients"));
    Serial.println(F("======================================================"));
    Serial.println();
}

void printDiagnostics() {
    Serial.println();
    Serial.println(F("---------------- SYSTEM DIAGNOSTICS ----------------"));
    Serial.print(F("Firmware Version   : ")); Serial.println(FIRMWARE_VERSION);
    Serial.print(F("Actual Temperature : ")); Serial.print(thermistor.getTemperatureC(), 2); Serial.println(F(" °C"));
    Serial.print(F("Target Setpoint    : ")); Serial.print(heater.getTargetTemp(), 1); Serial.println(F(" °C"));
    Serial.print(F("Thermistor ADC     : ")); Serial.println(thermistor.getRawAdc(), 1);
    Serial.print(F("Thermistor Ohms    : ")); Serial.print(thermistor.getRawResistance(), 1); Serial.println(F(" Ω"));
    Serial.print(F("Sensor Health      : ")); Serial.println(thermistor.getStatusString());
    Serial.print(F("Potentiometer ADC  : ")); Serial.println(lastFilteredPotAdc, 1);
    Serial.print(F("Heater State       : ")); Serial.println(heater.getStateString());
    Serial.print(F("Heater PWM Output  : ")); Serial.print(heater.getPwmOutput(), 1);
    Serial.print(F(" (")); Serial.print(heater.getPwmDutyPercent()); Serial.println(F("%)"));
    Serial.print(F("Extruder State     : ")); Serial.println(extruder.getStateString());
    Serial.print(F("Extrusion Ready    : ")); Serial.println(heater.isExtrusionReady() ? "YES" : "NO");
    Serial.print(F("Step Interval      : ")); Serial.print(extruder.getSpeedIntervalUs()); Serial.println(F(" us"));
    Serial.print(F("Total Steps Run    : ")); Serial.println(extruder.getStepCount());
    Serial.print(F("System Fault       : ")); Serial.println(heater.getFaultString());
    Serial.println(F("-----------------------------------------------------"));
    Serial.println();
}
