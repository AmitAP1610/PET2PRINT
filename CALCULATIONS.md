# PET2PRINT: Electrical, Thermal & Mechanical Engineering Calculations

This document details the engineering verifications, thermal dissipation analyses, power supply ratings, fuse sizing, and kinematic calculations for the PET2PRINT system based on the project documentation.

---

## 1. DC Power Supply & Load Budget

The system is powered by a **12V 10A (120W)** Switched-Mode Power Supply (SMPS).

### 1.1 Individual Subsystem Loads

| Subsystem | Operating Voltage | Peak Power | Operating Current | Duty Cycle |
| :--- | :--- | :--- | :--- | :--- |
| **Hotend Heater Cartridge** | 12V DC | 40W | $I = \frac{40\text{W}}{12\text{V}} = 3.33\text{A}$ | 100% warmup, ~35% steady |
| **NEMA17 Stepper via TMC2209** | 12V DC (VM) | ~10W | $I_{in} \approx 0.8\text{A}$ | Continuous during feed |
| **TC4420 Gate Driver** | 12V DC (VDD) | < 0.5W | $I_{avg} < 0.05\text{A}$ | Continuous |
| **Arduino UNO R4 & OLED** | 5V DC (USB/Reg) | ~1.5W | Powered from USB / 5V | Continuous |
| **Cooling Fan (Future)** | 12V DC | 2.4W | 0.20A | Optional |

### 1.2 Combined Power Consumption
- **Maximum Continuous Power:**
  $$P_{total} = 40\text{W (heater)} + 10\text{W (stepper)} + 2.4\text{W (fan)} + 1\text{W (driver)} \approx 53.4\text{ W}$$
- **Maximum Continuous Current:**
  $$I_{total} = 3.33\text{A} + 0.80\text{A} + 0.20\text{A} + 0.05\text{A} \approx 4.38\text{ A}$$
- **Power Supply Utilization:**
  $$\text{Utilization} = \frac{4.38\text{A}}{10.0\text{A}} \times 100\% = 43.8\%$$

> [!NOTE]
> The 12V 10A SMPS operates at **under 50% capacity**, ensuring long lifespan, low ripple voltage, and minimal thermal stress on the power supply.

---

## 2. IRFZ44N Power MOSFET Thermal & Switching Analysis

The IRFZ44N is used as a low-side power switch driven by the **TC4420** high-speed non-inverting driver.

### 2.1 Gate Drive Operating Point
- **Gate Voltage ($V_{GS}$):** Driven to **12V** by the TC4420.
- **On-State Resistance ($R_{DS(on)}$):** At $V_{GS} = 10\text{V} - 12\text{V}$, $R_{DS(on)} \le 0.0175\ \Omega$ ($17.5\text{ m}\Omega$).

### 2.2 Conduction Losses ($P_{cond}$)
At maximum heater load ($I_{heater} = 3.33\text{A}$):
$$P_{cond} = I^2 \times R_{DS(on)} = (3.33\text{A})^2 \times 0.0175\ \Omega = 0.194\text{ W}$$

### 2.3 Switching Losses ($P_{sw}$)
- TC4420 peak gate drive current: **6.0 A**.
- Total Gate Charge of IRFZ44N ($Q_g$): $\approx 63\text{ nC}$.
- With a $10\ \Omega$ gate series resistor, gate rise time $t_r$ and fall time $t_f$ are both $< 50\text{ ns}$.
- Arduino UNO R4 hardware PWM frequency on pin D9: $f_{PWM} \approx 490\text{ Hz}$.
- Switching loss per cycle:
  $$E_{sw} = \frac{1}{2} \times V_{DS} \times I_D \times (t_r + t_f) = 0.5 \times 12\text{V} \times 3.33\text{A} \times (100 \times 10^{-9}\text{s}) \approx 2.0\ \mu\text{J}$$
  $$P_{sw} = E_{sw} \times f_{PWM} = 2.0\ \mu\text{J} \times 490\text{ Hz} \approx 0.001\text{ W}$$

### 2.4 Total Dissipation & Junction Temperature Rise
$$P_{total} = P_{cond} + P_{sw} \approx 0.195\text{ W}$$
- Junction-to-ambient thermal resistance without heatsink: $R_{\theta JA} = 62^\circ\text{C/W}$.
- Expected temperature rise without heatsink:
  $$\Delta T = P_{total} \times R_{\theta JA} = 0.195\text{W} \times 62^\circ\text{C/W} \approx 12.1^\circ\text{C}$$
- At an ambient temperature of $25^\circ\text{C}$, the MOSFET junction operates at $\approx 37.1^\circ\text{C}$ (barely warm to the touch).
- **Recommendation:** A small TO-220 clip-on heatsink provides mechanical protection and ensures stability inside an enclosed chassis.

---

## 3. Circuit Protection & Conductor Sizing

### 3.1 Fuse Ratings
- **Main 12V Supply Fuse:** **10A** fast-acting automotive blade fuse (protects main SMPS wiring).
- **Heater Branch Fuse:** **5A** fast-acting fuse (protects heater cartridge, TC4420 output, and IRFZ44N).
- **Independent Hardware Thermal Cutoff:** **260°C or 280°C** thermal cutoff fuse clamped directly against the aluminum heater block in series with the heater cartridge +12V positive supply line.

### 3.2 Wire Gauge (AWG) Requirements
- **12V High-Current Heater Wiring:**
  - Continuous current: $3.33\text{A}$.
  - Minimum requirement: **18 AWG (0.75 mm²)** silicone high-temperature wire.
  - Solderless breadboards **must never** be used for any heater power conductor.
- **Motor Coil Wiring:**
  - Phase current: $0.8\text{A} - 1.2\text{A}$.
  - Wire gauge: **22 AWG or 24 AWG**.
- **Sensor & I2C Logic Wiring:**
  - Current: $< 10\text{mA}$.
  - Wire gauge: **26 AWG or 28 AWG** shielded or twisted pair.

---

## 4. Extruder Stepper Kinematics (27:1 Planetary Gearbox)

### 4.1 Step Resolution
- NEMA17 basic step angle: $1.8^\circ \implies 200\text{ steps/revolution}$.
- TMC2209 microstepping setting: **1/16 microstepping** ($3200\text{ microsteps/motor rev}$).
- Planetary Gearbox reduction: **27:1**.
- Microsteps per output shaft revolution:
  $$\text{Steps per revolution} = 3200 \times 27 = 86,400\text{ steps/rev}$$

### 4.2 Linear Strip Feed Velocity
Assuming a knurled feed roller with effective pitch diameter $D = 12.0\text{ mm}$:
$$\text{Circumference} = \pi \times D = 3.1416 \times 12.0\text{ mm} \approx 37.70\text{ mm/rev}$$
$$\text{Linear step resolution} = \frac{37.70\text{ mm}}{86,400\text{ steps}} \approx 0.000436\text{ mm/step } (2,292\text{ steps/mm})$$

At default firmware setting (`STEP_INTERVAL_US = 1200` $\mu\text{s}$):
$$\text{Step frequency } f = \frac{1,000,000}{1200} \approx 833.3\text{ steps/second}$$
$$\text{Output shaft speed} = \frac{833.3\text{ steps/s}}{86,400\text{ steps/rev}} \times 60\text{ s/min} \approx 0.579\text{ RPM}$$
$$\text{Linear feed speed} = 0.579\text{ RPM} \times 37.70\text{ mm/rev} \approx 21.8\text{ mm/minute } (\approx 0.36\text{ mm/s})$$

> [!TIP]
> A feed velocity of **~20 to 25 mm/min** provides sufficient residence time in a standard 20mm heater melt zone for PET strip material (typically 0.25mm to 0.4mm wall thickness) to reach uniform melting temperature ($215^\circ\text{C}-235^\circ\text{C}$) without buckling or jamming. Feed rate can be adjusted via the `SPEED <us>` serial command.
