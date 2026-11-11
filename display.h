#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "heater.h"
#include "extruder.h"

class DisplayManager {
public:
    DisplayManager();
    bool begin();
    void update(const Heater& heater, const Extruder& extruder, float potReading);
    void showBootScreen(const char* versionStr);
    void showFaultScreen(HeaterFault fault, float actTemp);

private:
    Adafruit_SSD1306 _oled;
    bool _isAvailable;
    unsigned long _lastUpdateMs;

    void drawHeader(const char* title);
    void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent);
};

#endif // DISPLAY_H

