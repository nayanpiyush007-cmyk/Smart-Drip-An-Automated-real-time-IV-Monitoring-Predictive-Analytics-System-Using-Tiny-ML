# Hardware Architecture & Wiring Guide

This document describes the circuit schematics, hardware components, and pin connections for the **Smart-Drip IV Monitoring System** based on the **ESP32-S3 DevKitC-1**.

---

## 1. System Pinout Mapping Table

| Component | Component Pin | ESP32-S3 Pin | Function / Protocol | Power Supply |
| :--- | :--- | :--- | :--- | :--- |
| **HX711 (Load Cell ADC)** | `DOUT` (Data Out) | `GPIO 4` | Digital Serial Data | 3.3V / 5V |
| | `SCK` / `CLK` | `GPIO 5` | Clock Input | GND |
| **0.96" OLED Display (SSD1306)** | `SDA` | `GPIO 8` | I2C Data Line | 3.3V |
| | `SCL` | `GPIO 9` | I2C Clock Line | GND |
| **IR Drop Sensor (LM393/FC-33)**| `OUT` (Digital) | `GPIO 2` | Hardware Interrupt (`FALLING`) | 3.3V |
| | `VCC` | `3V3` | Power | GND |
| **Piezo Buzzer** | Positive (`+`) | `GPIO 15` | Digital Output Alarm | 3.3V / 5V |
| | Negative (`-`) | `GND` | Ground | GND |

---

## 2. Subsystem Details

### 2.1 Weight Measurement Subsystem (HX711 + Strain Gauge Load Cell)
- **Sensor:** 1 kg / 5 kg micro-load cell mounted to an IV bag bracket.
- **ADC:** HX711 24-bit high-precision analog-to-digital converter.
- **Sampling Rate:** 10 Hz / 80 Hz selectable.
- **Software Calibration Factor:** Defined as `calibration = 442084` in [main.cpp](file:///d:/Coding/esp32_s3_project/src/main.cpp).

### 2.2 Optical Droplet Counter Subsystem (IR Break-Beam Sensor)
- **Sensor Module:** Infrared optical optocoupler / break-beam phototransistor positioned across the IV drip chamber.
- **Microcontroller Interface:** Configured with `INPUT_PULLUP` on `GPIO 2`.
- **Interrupt Handler:** Configured with `IRAM_ATTR countDrop()` on `FALLING` edge with a 50 ms software debounce window to prevent multi-triggering from fluid meniscus splash.

### 2.3 Visual & Audio Notification Subsystem
- **OLED Display:** 128×64 pixels monochrome SSD1306 connected over software/hardware I2C at address `0x3C`.
- **Auditory Alerts:**
  - 1 Short Beep: 50% milestone alert
  - 2 Beeps: Low flow warning (< 10 DPM)
  - 3 Beeps: Occlusion/blockage alert (< 5 DPM with remaining fluid) or runaway flow alert (> 80 DPM)
  - Continuous 3-second Alarm: Critical depletion warning (≤ 20% bag volume)

---

## 3. Circuit Block Diagram

```
                 +-------------------------------------------+
                 |          ESP32-S3 DevKitC-1               |
                 |                                           |
[HX711 ADC] ---->| GPIO 4 (DOUT)                             |
[Strain Gauge] ->| GPIO 5 (CLK)                              |
                 |                                           |
[IR Drop Sensor]-> GPIO 2 (Interrupt, FALLING)               |
                 |                                           |
[I2C SSD1306] <--| GPIO 8 (SDA)                              |
[128x64 OLED] <--| GPIO 9 (SCL)                              |
                 |                                           |
[Piezo Buzzer] <-| GPIO 15 (Active Alarm Output)             |
                 |                                           |
                 | [Wi-Fi 2.4 GHz 802.11 b/g/n]              |
                 +---------------------+---------------------+
                                       |
                                       v
                          [Blynk IoT Cloud Dashboard]
                          - V0: Infusion Volume (mL)
                          - V1: Volume Percentage (%)
                          - V2: Instantaneous Drip Rate (dpm)
                          - V3: TinyML Predicted ETA (min)
```

---

## 4. Hardware Reference Documents

Complete academic review, circuit schematics, bare-metal ISR architecture, and NIR vein detection research papers are preserved in:
- [review1_main.pdf](file:///d:/Coding/esp32_s3_project/hardware/schematics/review1_main.pdf)
