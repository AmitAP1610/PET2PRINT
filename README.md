# PET2PRINT: PET Bottle Recycling & Filament Extrusion Firmware

Firmware for the **PET2PRINT** automated recycling system, converting post-consumer PET bottle strips into continuous 3D printer filament.

- **Main Controller:** Arduino UNO R4 WiFi (Renesas RA4M1 Arm Cortex-M4 @ 48 MHz)
- **Heater Switching:** High-Speed Gate Driver (TC4420) + N-Channel Power MOSFET (IRFZ44N)
- **Extruder Feed:** TMC2209 Stepper Driver + NEMA17 Geared Stepper (27:1 Gearbox)
- **Temperature Sensing:** 100kΩ NTC Thermistor (Beta 3950) with 4.7kΩ Precision Divider
- **Operator Interface:** B10K Linear Potentiometer (Setpoint) & SSD1306 128x64 I2C OLED (0x3C)

---

## 1. Pin Map & Hardware Connections

### 1.1 Arduino UNO R4 WiFi Pinout

| Pin | Connected Device | Function / Signal | Notes |
| :--- | :--- | :--- | :--- |
| **A0** | B10K Potentiometer | Setpoint Target Input | Center wiper. Outer pins to 5V and GND. |
| **A2** | Thermistor Divider | Hotend Temperature Sense | Junction of 4.7kΩ pull-up to 5V and NTC to GND. |
| **D5** | TMC2209 `EN` | Stepper Driver Enable | **Active LOW** (LOW = Enabled, HIGH = Disabled). |
| **D6** | TMC2209 `DIR` | Stepper Direction | Sets forward strip feeding direction. |
| **D7** | TMC2209 `STEP` | Step Pulse Output | Non-blocking microstep pulse generator. |
| **D9** | TC4420 `INPUT` (Pin 2) | Heater PWM Command | Hardware PWM driving TC4420 gate driver. |
| **SDA**| SSD1306 `SDA` | I2C Serial Data | Hardware I2C (Address: `0x3C` / fallback `0x3D`). |
| **SCL**| SSD1306 `SCL` | I2C Serial Clock | Hardware I2C. |
| **5V** | Pot, OLED, TMC2209 VIO | 5V Logic Supply | Verify module 5V compatibility. |
| **GND**| Power Supply & Modules | Common Logic Ground | Must be tied to 12V SMPS Negative GND. |

---

### 1.2 TC4420 & IRFZ44N Heater Driver Circuit

The heater uses a low-side N-channel MOSFET switch driven by a dedicated high-current non-inverting MOSFET driver:

```
                          +12V Supply (Fused)
                                 |
                          [Hotend Heater Cartridge]
                                 |
                                 +-----> IRFZ44N Drain (Pin 2 / Tab)
                                         IRFZ44N Source (Pin 3) ----> Supply GND
                                         IRFZ44N Gate (Pin 1)
                                                 ^
                                                 |  10Ω Gate Resistor
                 +12V SMPS ----+                 |
                               |          +------+------+
                              [Pin 1,8]   [Pin 6,7]     |
                            +-----------------------+   |
                            |       TC4420          |   |
   Arduino D9 (PWM) ------->| Pin 2 (INPUT)         |   |
   Arduino GND ------------>| Pin 4,5 (GND)         |   |
                            +-----------------------+   |
                               |                        |
                              1µF Bypass Cap            [10kΩ Pull-down to GND]
                               |
                              GND
```

#### TC4420 DIP-8 Pin Configuration:
- **Pin 1 & 8 (`VDD`):** +12V DC Supply (decouple with 1µF low-ESR ceramic capacitor to GND).
- **Pin 2 (`INPUT`):** Connects to Arduino **Pin D9**.
- **Pin 3 (`NC`):** No connection.
- **Pin 4 & 5 (`GND`):** System Common Ground.
- **Pin 6 & 7 (`OUTPUT`):** Joined together, connected through **10Ω series resistor** to IRFZ44N Gate.
- **IRFZ44N Gate:** Add **10kΩ pull-down resistor** from Gate to Source (GND) to ensure the MOSFET stays fully OFF during Arduino reset/boot.

---

### 1.3 TMC2209 Stepper Driver & 27:1 Geared NEMA17

| TMC2209 Carrier Pin | Connection | Specification / Notes |
| :--- | :--- | :--- |
| **VM** | +12V DC Supply | Add 100µF electrolytic capacitor across VM and GND. |
| **GND** (Power) | 12V Supply Negative | High-current ground return. |
| **VIO** | Arduino 5V | Logic interface voltage. |
| **GND** (Logic) | Arduino GND | Logic ground reference. |
| **EN** | Arduino **D5** | Active LOW enable. |
| **DIR** | Arduino **D6** | Direction signal. |
| **STEP**| Arduino **D7** | Microstep pulses. |
| **1A / 1B** | Motor Phase A | Identify coil pair using multimeter resistance test. |
| **2A / 2B** | Motor Phase B | Identify coil pair using multimeter resistance test. |

> [!WARNING]
> Never connect or disconnect the stepper motor while the TMC2209 is powered. Doing so can permanently destroy the driver output transistors. Set the driver VREF / current limit according to the motor's rated phase current before loading feedstock.

---

### 1.4 Thermistor Voltage Divider Circuit

```
   Arduino 5V
       |
     [ 4.7kΩ Resistor (1% tolerance) ]
       |
       +----------> Arduino Pin A2
       |
     [ 100kΩ NTC Thermistor (Beta 3950) ]
       |
   Arduino GND
```

Temperature calculation is implemented directly via the Beta equation:
$$\frac{1}{T} = \frac{1}{T_0} + \frac{1}{\beta} \ln\left(\frac{R_{ntc}}{R_0}\right)$$
- $T_0 = 298.15\text{ K } (25^\circ\text{C})$
- $R_0 = 100,000\ \Omega$
- $\beta = 3950\text{ K}$

---

## 2. Firmware Architecture

```
PET2PRINT/
├── PET2PRINT.ino    # Main application setup, loop & state orchestration
├── config.h         # Centralized hardware configuration, pinout & safety thresholds
├── thermistor.h     # Thermistor driver interface & status definitions
├── thermistor.cpp   # Multi-sample averaging, Beta equation & open/short checks
├── heater.h         # PID regulation wrapper & thermal safety monitor
├── heater.cpp       # PWM control, safety state latch & runaway watchdog
├── extruder.h       # TMC2209 controller & cold-extrusion prevention logic
├── extruder.cpp     # Non-blocking microstep pulse generation
├── display.h        # SSD1306 OLED interface & screen definitions
├── display.cpp      # Live UI rendering, power bar & emergency fault screens
├── PID_v1.h         # Standard PID library header
└── PID_v1.cpp       # Self-contained PID algorithm implementation
```

---

## 3. Safety & Thermal Protection System

The firmware incorporates a **multi-tier safety supervisor** that operates continuously in the background:

1. **Sensor Disconnection Detection (`FAULT_SENSOR_OPEN`):**
   If the thermistor is severed or disconnected, the 4.7kΩ pull-up pulls A2 to 5V. If ADC reaches full-scale or temperature drops below `5.0°C`, the heater shuts down immediately.
2. **Sensor Short-Circuit Detection (`FAULT_SENSOR_SHORT`):**
   If the thermistor wires short together, A2 is pulled to GND. If ADC drops near 0 or temperature reads above `285.0°C`, power is immediately cut.
3. **Software Over-temperature Cutoff (`FAULT_OVERTEMP`):**
   If the hotend temperature exceeds `268.0°C` at any time, a critical fault is latched.
4. **Thermal Runaway Watchdog (`FAULT_THERMAL_RUNAWAY`):**
   If the PID controller outputs high power ($>70\%$ PWM) for more than 25 seconds without the temperature rising by at least $2.0^\circ\text{C}$, or if temperature drops by more than $12.0^\circ\text{C}$ after reaching the target, the firmware concludes the thermistor has dislodged or the heater cartridge is decoupled, immediately locking out power.
5. **Cold Extrusion Prevention:**
   The geared stepper motor is electronically interlocked. It **will not step** unless:
   - Target temperature is greater than 0°C.
   - Hotend temperature is $\ge 190.0^\circ\text{C}$ (`MIN_EXTRUSION_TEMP_C`).
   - Hotend temperature is within $5.0^\circ\text{C}$ of the dialed target setpoint.

> [!CRITICAL]
> **Hardware Protection:** Software safeguards do not replace a physical thermal fuse. Install an independent thermal cutoff (e.g. 260°C or 280°C thermal fuse) in series with the heater cartridge supply line, mounted securely against the heat block.

---

## 4. Compilation & Upload Guide

### Prerequisites
1. **Arduino IDE 2.x** installed.
2. Under **Boards Manager**, install:
   - `Arduino UNO R4 Boards` (by Arduino)
3. Under **Library Manager**, install:
   - `Adafruit SSD1306`
   - `Adafruit GFX Library`

### Steps
1. Open `PET2PRINT.ino` in Arduino IDE 2.x.
2. Select Board: **Tools > Board > Arduino UNO R4 Boards > Arduino UNO R4 WiFi**.
3. Select Port: **Tools > Port > COMx (Arduino UNO R4 WiFi)**.
4. Click **Verify** (Checkmark) to confirm compilation.
5. Click **Upload** (Arrow) to program the board.

---

## 5. Serial Command Interface

Open the Arduino Serial Monitor at **115200 baud** (newline line ending).

| Command | Arguments | Description |
| :--- | :--- | :--- |
| `HELP` or `?` | None | Lists available commands. |
| `STATUS` | None | Prints detailed real-time diagnostic readings (ADC, resistance, temps, duty cycle, step count). |
| `RESET` | None | Clears a latched safety fault (only if physical temperature is within safe limits). |
| `SPEED <us>`| `<150 - 20000>` | Updates the step pulse interval in microseconds on-the-fly. |
| `PID <Kp> <Ki> <Kd>` | `<float> <float> <float>` | Updates PID gains in RAM (e.g., `PID 22.0 1.4 65.0`). |

---

## 6. Commissioning & Testing Procedure

Follow the phased commissioning procedure outlined in Section 8 of the project documentation:

1. **Phase 1: Controller-Only Test (No 12V Power)**
   - Connect only USB power to the Arduino.
   - Verify OLED lights up and shows `PET2PRINT INITIALIZING` followed by the main screen.
   - Verify room temperature displays accurately ($\sim 20^\circ\text{C} - 27^\circ\text{C}$).
   - Turn B10K potentiometer: verify setpoint smoothly adjusts between `OFF` and `160°C - 260°C`.
2. **Phase 2: Stepper Drive Test**
   - Connect 12V power to the TMC2209 `VM` line.
   - Set potentiometer to `210°C`.
   - Once hotend reaches temperature, verify motor starts turning smoothly in the forward direction.
   - Test step speed changes using the `SPEED` serial command.
3. **Phase 3: Heater Regulation Test**
   - Monitor temperature curve using the Arduino Serial Plotter.
   - Verify temperature approaches setpoint with minimal overshoot ($\le 3^\circ\text{C}$).
   - Verify power indicator bar settles to steady-state holding duty cycle ($\sim 25\% - 45\%$).
4. **Phase 4: Integrated Extrusion**
   - Feed clean, dry PET strip into the guide and hotend.
   - Check strip traction, melt flow, and filament output diameter with a digital micrometer.

#   P E T 2 P R I N T  
 