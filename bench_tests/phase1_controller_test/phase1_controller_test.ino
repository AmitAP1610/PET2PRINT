/*
 * PET2PRINT - Commissioning Phase 1: Controller & Sensor Bench Test
 * Target: Arduino UNO R4 WiFi
 * 
 * Purpose:
 * Tests the SSD1306 OLED, B10K Potentiometer (A0), and 100k NTC Thermistor (A2)
 * powered ONLY by USB 5V (12V main power disconnected).
 * 
 * Pass Criteria:
 * - OLED displays cleanly without flickering.
 * - Potentiometer rotates smoothly from OFF to 160°C - 260°C.
 * - Thermistor reads reasonable ambient room temperature (~20°C - 27°C).
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define PIN_POT         A0
#define PIN_THERMISTOR  A2
#define OLED_ADDR       0x3C
#define SCREEN_W        128
#define SCREEN_H        64

Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 1500);

    Serial.println(F("=== PET2PRINT COMMISSIONING: PHASE 1 TEST ==="));

#if defined(ARDUINO_UNOR4_WIFI) || defined(ARDUINO_UNOR4_MINIMA)
    analogReadResolution(12);
#endif

    pinMode(PIN_POT, INPUT);
    pinMode(PIN_THERMISTOR, INPUT);

    Wire.begin();
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
            Serial.println(F("[FAIL] OLED display not found on I2C 0x3C or 0x3D!"));
            while (1);
        }
    }

    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(10, 20);
    oled.print(F("PHASE 1: BENCH TEST"));
    oled.setCursor(10, 36);
    oled.print(F("Sensors & Display OK"));
    oled.display();
    delay(1500);
}

float readThermistorC() {
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(PIN_THERMISTOR);
        delayMicroseconds(60);
    }
    float adc = (float)sum / 16.0f;
    float maxAdc = 4095.0f; // 12-bit
    if (adc >= maxAdc - 5.0f || adc <= 5.0f) return -999.0f;

    float rNtc = 4700.0f * (adc / (maxAdc - adc));
    float steinhart = (1.0f / 298.15f) + (1.0f / 3950.0f) * log(rNtc / 100000.0f);
    return (1.0f / steinhart) - 273.15f;
}

void loop() {
    int rawPot = analogRead(PIN_POT);
    float tempC = readThermistorC();

    // Map pot
    float setpoint = 0;
    if (rawPot >= 120) {
        setpoint = 160.0f + ((float)(rawPot - 120) / (4095.0f - 120.0f)) * 100.0f;
    }

    // Print to Serial
    Serial.print(F("Pot ADC: ")); Serial.print(rawPot);
    Serial.print(F(" | Setpoint: "));
    if (setpoint == 0) Serial.print(F("OFF"));
    else { Serial.print((int)setpoint); Serial.print(F(" C")); }
    Serial.print(F(" | Ambient Temp: "));
    if (tempC < -100) Serial.print(F("SENSOR ERROR"));
    else { Serial.print(tempC, 2); Serial.print(F(" C")); }
    Serial.println();

    // Render OLED
    oled.clearDisplay();
    oled.fillRect(0, 0, 128, 12, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    oled.setCursor(10, 2);
    oled.print(F("PHASE 1: BENCH TEST"));

    oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    oled.setCursor(0, 18);
    oled.print(F("Ambient T: "));
    if (tempC < -100) oled.print(F("ERR"));
    else { oled.print(tempC, 1); oled.print(F(" C")); }

    oled.setCursor(0, 32);
    oled.print(F("Pot Raw  : ")); oled.print(rawPot);

    oled.setCursor(0, 46);
    oled.print(F("Dial Set : "));
    if (setpoint == 0) oled.print(F("OFF"));
    else { oled.print((int)setpoint); oled.print(F(" C")); }

    oled.display();
    delay(200);
}
