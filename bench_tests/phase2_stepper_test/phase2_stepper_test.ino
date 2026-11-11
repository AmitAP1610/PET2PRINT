/*
 * PET2PRINT - Commissioning Phase 2: TMC2209 & 27:1 Stepper Test
 * Target: Arduino UNO R4 WiFi
 * 
 * Purpose:
 * Tests the TMC2209 driver and geared NEMA17 motor without heating.
 * Verifies motor wiring coil pairs, driver enable pin, stepping pulses, and direction.
 * 
 * Safety:
 * - Ensure motor coil pairs (1A/1B and 2A/2B) were verified with a multimeter before plugging in.
 * - Set TMC2209 VREF potentiometer before running.
 */

#include <Arduino.h>

#define PIN_EN    5
#define PIN_DIR   6
#define PIN_STEP  7

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 1500);

    Serial.println(F("=== PET2PRINT COMMISSIONING: PHASE 2 STEPPER TEST ==="));

    pinMode(PIN_EN, OUTPUT);
    pinMode(PIN_DIR, OUTPUT);
    pinMode(PIN_STEP, OUTPUT);

    digitalWrite(PIN_DIR, HIGH); // Forward
    digitalWrite(PIN_STEP, LOW);

    // TMC2209 EN is active LOW
    Serial.println(F("[STEP 1] Driver disabled (EN = HIGH). Coils unpowered."));
    digitalWrite(PIN_EN, HIGH);
    delay(2000);

    Serial.println(F("[STEP 2] Driver enabled (EN = LOW). Coils holding."));
    digitalWrite(PIN_EN, LOW);
    delay(1000);
}

void loop() {
    Serial.println(F("[TEST] Stepping forward 2000 steps at 1000us interval..."));
    digitalWrite(PIN_DIR, HIGH);
    for (int i = 0; i < 2000; i++) {
        digitalWrite(PIN_STEP, HIGH);
        delayMicroseconds(4);
        digitalWrite(PIN_STEP, LOW);
        delayMicroseconds(1000);
    }
    delay(1000);

    Serial.println(F("[TEST] Stepping reverse 2000 steps at 1000us interval..."));
    digitalWrite(PIN_DIR, LOW);
    for (int i = 0; i < 2000; i++) {
        digitalWrite(PIN_STEP, HIGH);
        delayMicroseconds(4);
        digitalWrite(PIN_STEP, LOW);
        delayMicroseconds(1000);
    }
    delay(1000);
}
