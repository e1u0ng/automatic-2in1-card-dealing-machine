# 🃏 Automated ESP32 Card Shuffler & Launcher

An ESP32-powered machine that shuffles and passes cards to players with an interactive menu screen for customized game settings. 

## 🔄 V1 Assembly Changes (9/11/26)
* **Improved Shuffler Performance:** 
  * Increased deck space to prevent overflowing cards going near spinning wheels
  * Covered more gap in the inner walls to prevent lodged cards
* **Redesigned Dispenser:** 
  * Added an angled ramp in the deck compartment to guarantee contact with the wheels (aimed to combat inconsistency with bent cards)
  * Switched to compound gear train for compactness and its gear ratio from 1:6.5 to 1:10 to make up for smaller wheel diameter
  * Switched to 32 mm diameter gecko wheels for compactness
* **Code Layout Redesign:** 
  * Implemented header files and source files for each subsystem (dispenser, shuffler, turret, menu) containing initialization and action functions
  * Used a separate command system file to translate inputs (button encoders) into outputs from the shuffler, dispenser, turret, and menu screen 
  * Utilized the FreeRTOS task system to run multiple tasks at the same time

## 🚀 Next Steps
* **Github Files:**
  * Upload current bill of materials for V1 (BOM)
  * Update specifications checklist for V1
* **Power Management:**
  * Assemble the 12.6V 3S Li-ion battery pack with integrated BMS protection.
  * Wire an SPDT charging/main power selector switch to safely isolate the load during recharges.
  * Implement an ESP32 ADC resistor divider circuit and software-based low-voltage shutdown routine before BMS cutoff.
* **Turret Redesign:**
  * Switch to a smaller brushed DC motor and separate absolute magnetic encoder
  * Refine ESP-IDF PID loop control for new turret setup and implement exact player-angle targeting using an absolute encoder
* **PCB Design:**
  * Design PCB to hold the ESP32 microcontroller, shutdown circuit, and power lines for compactness
* **Sensors Implementations:**
  * Integrate IR sensors for directional card delivery detection and optional distance sensors for adaptive shuffle timing.
* **Enclosure Design:**
  * Use Fusion360 to design a container to hold the subsystems together for a finished look

## 📂 Repository Structure
* `docs/` — Design sketches, calculations, and proof-of-concept testing logs.
* `hardware/` — Circuit diagrams, power schematics, and 3D STL/STEP models.
* `main/` — ESP32 C++ source code and all test codes
