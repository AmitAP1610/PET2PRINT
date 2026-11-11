/*
 * PET2PRINT - Commissioning Phase 3: TC4420 + IRFZ44N Switching Test
 * Target: Arduino UNO R4 WiFi
 * 
 * Purpose:
 * Safely verifies PWM switching of Arduino pin D9 -> TC4420 -> IRFZ44N Gate
 * BEFORE connecting the actual 40W heater cartridge.
 * 
 * Safe Setup:
 * Instead of connecting the heater cartridge, connect a 12V test automotive bulb
 * or an LED with a 1kΩ resistor between +12V and the IRFZ44N Drain.
 * 
 * Pass Criteria:
 * - Test lamp/LED ramps brightness smoothly with PWM duty cycle.
 * - IRFZ44N remains completely cool to the touch.
 * - MOSFET Gate voltage measures 0V at 0% PWM and ~12V at 100% PWM.
 */

#include <Arduino.h>

#define PIN_PWM 9

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 1500);

    Serial.println(F("=== PET2PRINT COMMISSIONING: PHASE 3 GATE DRIVER & MOSFET ==="));
    Serial.println(F("[SAFETY] Connect a low-power 12V test bulb or LED between +12V and MOSFET Drain."));

    pinMode(PIN_PWM, OUTPUT);
    digitalWrite(PIN_PWM, LOW); // Start OFF
}

void loop() {
    Serial.println(F("[TEST] Ramping PWM from 0% to 100%..."));
    for (int pwm = 0; pwm <= 255; pwm += 15) {
        analogWrite(PIN_PWM, pwm);
        Serial.print(F("PWM: ")); Serial.print(pwm);
        Serial.print(F(" / 255 (")); Serial.print((int)((pwm / 255.0) * 100)); Serial.println(F("%)"));
        delay(300);
    }

    Serial.println(F("[TEST] Holding 100% duty cycle for 2 seconds..."));
    delay(2000);

    Serial.println(F("[TEST] Ramping PWM down to 0%..."));
    for (int pwm = 255; pwm >= 0; pwm -= 15) {
        analogWrite(PIN_PWM, pwm);
        delay(200);
    }

    analogWrite(PIN_PWM, 0);
    digitalWrite(PIN_PWM, LOW);
    Serial.println(F("[TEST] OFF state. Holding for 3 seconds. Check gate voltage is 0V."));
    delay(3000);
}
