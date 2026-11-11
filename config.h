#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// PET2PRINT - System Configuration & Pin Mapping
// Controller: Arduino UNO R4 WiFi
// Target: PET Bottle Recycling & Filament Extrusion System
// =============================================================================

// -----------------------------------------------------------------------------
// 1. PIN ALLOCATION (as per Document Section 5.1)
// -----------------------------------------------------------------------------
#define PIN_POT_SETPOINT    A0    // B10K Potentiometer wiper (Temperature target)
#define PIN_THERMISTOR      A2    // 4.7k Pull-up + 100k NTC divider node
#define PIN_MOTOR_EN        5     // TMC2209 EN (Active LOW: LOW=Enabled, HIGH=Disabled)
#define PIN_MOTOR_DIR       6     // TMC2209 DIR (Motor direction)
#define PIN_MOTOR_STEP      7     // TMC2209 STEP (Step pulse output)
#define PIN_HEATER_PWM      9     // TC4420 INPUT (PWM driving IRFZ44N MOSFET gate)

// I2C Pins for SSD1306 OLED (Hardware I2C on UNO R4 WiFi)
// SDA -> UNO R4 SDA pin
// SCL -> UNO R4 SCL pin
#define OLED_I2C_ADDRESS    0x3C  // Default SSD1306 I2C address (alt: 0x3D)
#define OLED_SCREEN_WIDTH   128   // OLED display width in pixels
#define OLED_SCREEN_HEIGHT  64    // OLED display height in pixels
#define OLED_RESET_PIN      -1    // Shared reset pin (-1 if not used / tied to RST)

// -----------------------------------------------------------------------------
// 2. THERMISTOR & ADC SPECIFICATIONS (Section 5.3 & 6.1)
// -----------------------------------------------------------------------------
// Circuit: 5V -> 4.7k Resistor -> A2 node -> NTC Thermistor -> GND
#define THERMISTOR_NOMINAL_R     100000.0f  // 100k Ohms at 25°C
#define TEMPERATURE_NOMINAL_C    25.0f      // Reference temperature in °C
#define THERMISTOR_BETA          3950.0f    // Beta value (typically 3950 for 100k NTC)
#define PULLUP_RESISTOR_OHMS     4700.0f    // Exact pull-up resistance (4.7kΩ)
#define THERMISTOR_SAMPLES       16         // Oversampling count for analog reading

// ADC Configuration (UNO R4 supports up to 14-bit, 12-bit gives 4096 counts for high precision)
#if defined(ARDUINO_UNOR4_WIFI) || defined(ARDUINO_UNOR4_MINIMA)
  #define ADC_RESOLUTION_BITS      12         // 12-bit on Arduino UNO R4
  #define ADC_MAX_VALUE            4095.0f    // (1 << 12) - 1
  #define POT_OFF_ADC_THRESHOLD    120        // Below this ADC reading (~3%), heater is commanded OFF
#else
  #define ADC_RESOLUTION_BITS      10         // Default 10-bit fallback for other Arduino boards
  #define ADC_MAX_VALUE            1023.0f    // (1 << 10) - 1
  #define POT_OFF_ADC_THRESHOLD    30         // Below this ADC reading (~3%), heater is commanded OFF
#endif
#define SETPOINT_HYSTERESIS_C    0.8f       // Filter threshold to prevent display jitter

// -----------------------------------------------------------------------------
// 4. PID HEATER REGULATION (Section 6.1 & 6.2)
// -----------------------------------------------------------------------------
// Default starting PID gains for standard 12V 40W hotend cartridge
// Tune using M303 / step response if required for your specific heat block
#define PID_KP_DEFAULT           22.0f
#define PID_KI_DEFAULT           1.4f
#define PID_KD_DEFAULT           65.0f
#define PID_SAMPLE_TIME_MS       100        // PID update interval (100ms = 10Hz)
#define PID_OUTPUT_MIN           0.0f       // 0% PWM duty cycle
#define PID_OUTPUT_MAX           255.0f     // 100% PWM duty cycle (8-bit)

// -----------------------------------------------------------------------------
// 5. TMC2209 & 27:1 GEARED STEPPER MOTOR (Section 4.1, 5.5, 6.2)
// -----------------------------------------------------------------------------
// NEMA17 with 27:1 planetary gearbox for controlled strip feed
// Stepper step interval in microseconds (non-blocking step generation)
// Adjust according to desired output shaft RPM and strip pull speed
#define STEP_INTERVAL_US         1200       // Default step period in microseconds
#define STEP_PULSE_WIDTH_US      4          // TMC2209 step pulse duration (>100ns)
#define MOTOR_DIR_FORWARD        HIGH       // Forward strip feed direction
#define MOTOR_DIR_REVERSE        LOW        // Reverse strip retract direction

// Extrusion Temperature Interlock (Cold Extrusion Prevention)
#define MIN_EXTRUSION_TEMP_C     190.0f     // Absolute minimum temp allowed to run feed
#define EXTRUSION_TEMP_WINDOW_C  5.0f       // Allowed temp drop below target before lockout

// -----------------------------------------------------------------------------
// 6. SAFETY SYSTEM & THERMAL RUNAWAY PROTECTION (Section 6.2 & Section 9)
// -----------------------------------------------------------------------------
#define TEMP_MIN_PLAUSIBLE_C     5.0f       // Lower limit; below this = SENSOR_OPEN / DISCONNECTED
#define TEMP_MAX_PLAUSIBLE_C     285.0f     // Upper limit; above this = SENSOR_SHORT / SHORTED
#define TEMP_ABSOLUTE_MAX_C      268.0f     // Software over-temperature cutoff threshold

// Thermal Runaway Watchdog Parameters
#define RUNAWAY_TRIGGER_PWM      180.0f     // Output PWM (>70%) where runaway timer starts
#define RUNAWAY_TIMEOUT_MS       25000UL    // 25 seconds allowed to see temperature rise
#define RUNAWAY_MIN_RISE_C       2.0f       // Must rise at least 2.0°C within timeout window
#define RUNAWAY_MAX_DROP_C       12.0f      // Uncommanded sudden drop when at target trips alarm

// -----------------------------------------------------------------------------
// 7. TELEMETRY & DISPLAY REFRESH RATES
// -----------------------------------------------------------------------------
#define SERIAL_BAUD_RATE         115200
#define DISPLAY_REFRESH_MS       250        // 4 Hz non-blocking OLED update
#define TELEMETRY_INTERVAL_MS    500        // 2 Hz serial telemetry output

#endif // CONFIG_H
