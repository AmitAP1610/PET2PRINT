#include "display.h"

DisplayManager::DisplayManager()
    : _oled(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET_PIN),
      _isAvailable(false),
      _lastUpdateMs(0) {}

bool DisplayManager::begin() {
    Wire.begin();
    // Try primary I2C address
    if (_oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        _isAvailable = true;
    } else {
        // Fallback to alternative I2C address
        if (_oled.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
            _isAvailable = true;
        } else {
            Serial.println(F("[WARN] SSD1306 OLED display not detected on I2C (0x3C or 0x3D)."));
            _isAvailable = false;
            return false;
        }
    }

    _oled.clearDisplay();
    _oled.setTextColor(SSD1306_WHITE);
    _oled.display();
    return true;
}

void DisplayManager::drawHeader(const char* title) {
    _oled.fillRect(0, 0, OLED_SCREEN_WIDTH, 11, SSD1306_WHITE);
    _oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    _oled.setTextSize(1);
    _oled.setCursor(8, 2);
    _oled.print(title);
    _oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
}

void DisplayManager::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent) {
    if (percent > 100) percent = 100;
    _oled.drawRect(x, y, w, h, SSD1306_WHITE);
    int16_t fillW = (int16_t)(((float)(w - 4) * percent) / 100.0f);
    if (fillW > 0) {
        _oled.fillRect(x + 2, y + 2, fillW, h - 4, SSD1306_WHITE);
    }
}

void DisplayManager::showBootScreen(const char* versionStr) {
    if (!_isAvailable) return;
    _oled.clearDisplay();
    drawHeader("PET2PRINT INITIALIZING");

    _oled.setTextSize(1);
    _oled.setCursor(4, 18);
    _oled.print(F("Controller: UNO R4"));
    _oled.setCursor(4, 30);
    _oled.print(F("Driver: TMC2209"));
    _oled.setCursor(4, 42);
    _oled.print(F("Switch: TC4420+MOSFET"));
    _oled.setCursor(4, 54);
    _oled.print(F("Ver: "));
    _oled.print(versionStr);

    _oled.display();
    delay(1200); // Brief splash
}

void DisplayManager::showFaultScreen(HeaterFault fault, float actTemp) {
    if (!_isAvailable) return;

    _oled.clearDisplay();
    _oled.fillRect(0, 0, OLED_SCREEN_WIDTH, 13, SSD1306_WHITE);
    _oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    _oled.setTextSize(1);
    _oled.setCursor(4, 3);
    _oled.print(F("! EMERGENCY SHUTDOWN !"));

    _oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    _oled.setCursor(0, 18);
    _oled.print(F("FAULT:"));

    _oled.setCursor(0, 28);
    switch (fault) {
        case FAULT_SENSOR_OPEN:
            _oled.print(F("THERMISTOR OPEN/CUT"));
            break;
        case FAULT_SENSOR_SHORT:
            _oled.print(F("THERMISTOR SHORTED"));
            break;
        case FAULT_OVERTEMP:
            _oled.print(F("OVERTEMPERATURE CUTOFF"));
            break;
        case FAULT_THERMAL_RUNAWAY:
            _oled.print(F("THERMAL RUNAWAY!"));
            break;
        default:
            _oled.print(F("SYSTEM HARDWARE FAULT"));
            break;
    }

    _oled.setCursor(0, 41);
    _oled.print(F("Temp: "));
    _oled.print(actTemp, 1);
    _oled.print(F(" C"));

    _oled.setCursor(0, 53);
    _oled.print(F("ALL POWER TERMINATED"));

    _oled.display();
}

void DisplayManager::update(const Heater& heater, const Extruder& extruder, float potReading) {
    if (!_isAvailable) return;

    unsigned long currentMs = millis();
    if ((currentMs - _lastUpdateMs) < DISPLAY_REFRESH_MS) {
        return; // Non-blocking throttle
    }
    _lastUpdateMs = currentMs;

    // Check if system is in fault state
    if (heater.getFault() != FAULT_NONE) {
        showFaultScreen(heater.getFault(), heater.getCurrentTemp());
        return;
    }

    _oled.clearDisplay();
    drawHeader("PET2PRINT EXTRUDER");

    // Line 1: Actual Temperature (Prominent text size 2)
    _oled.setTextSize(2);
    _oled.setCursor(0, 15);
    float actTemp = heater.getCurrentTemp();
    if (actTemp < 0.0f) actTemp = 0.0f;
    _oled.print(actTemp, 1);
    _oled.setTextSize(1);
    _oled.print((char)247); // Degree symbol
    _oled.print(F("C"));

    // Right-aligned status badge
    _oled.setCursor(76, 18);
    _oled.print(F("["));
    _oled.print(heater.getStateString());
    _oled.print(F("]"));

    // Line 2: Target Temperature & Heater Power
    _oled.setCursor(0, 33);
    _oled.print(F("Set:"));
    if (heater.getTargetTemp() <= 0.0f) {
        _oled.print(F("OFF"));
    } else {
        _oled.print((int)heater.getTargetTemp());
        _oled.print((char)247);
        _oled.print(F("C"));
    }

    _oled.setCursor(68, 33);
    _oled.print(F("Pwr:"));
    _oled.print(heater.getPwmDutyPercent());
    _oled.print(F("%"));

    // Line 3: Heater Power Progress Bar
    drawProgressBar(0, 44, 128, 6, heater.getPwmDutyPercent());

    // Line 4: Motor Status
    _oled.setCursor(0, 54);
    _oled.print(F("Feed:"));
    switch (extruder.getState()) {
        case MOTOR_STATE_RUNNING:
            _oled.print(F("RUNNING (ON)"));
            break;
        case MOTOR_STATE_COLD_LOCK:
            _oled.print(F("LOCK (COLD)"));
            break;
        case MOTOR_STATE_DISABLED:
            _oled.print(F("STANDBY (OFF)"));
            break;
        case MOTOR_STATE_FAULT:
            _oled.print(F("FAULT (HALT)"));
            break;
    }

    _oled.display();
}

