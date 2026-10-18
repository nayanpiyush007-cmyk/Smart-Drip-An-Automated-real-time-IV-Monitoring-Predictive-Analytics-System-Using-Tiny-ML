# Smart-Drip: Automated Real-Time IV Monitoring & Predictive Analytics System Using TinyML

[![ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-red.svg?logo=espressif)](https://www.espressif.com/en/products/socs/esp32-s3)
[![PlatformIO](https://img.shields.io/badge/Framework-PlatformIO%20%7C%20Arduino-orange.svg?logo=platformio)](https://platformio.org/)
[![TinyML](https://img.shields.io/badge/Machine%20Learning-TinyML%20%7C%20XGBoost-blue.svg?logo=python)](https://xgboost.readthedocs.io/)
[![IoT Cloud](https://img.shields.io/badge/Cloud-Blynk%20IoT-green.svg?logo=blynk)](https://blynk.io/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg?logo=c%2B%2B)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

An intelligent, IoT-enabled intravenous (IV) infusion monitoring and predictive safety system powered by an **ESP32-S3 microcontroller**. By fusing high-precision strain-gauge mass sensing (HX711) with bare-metal optical droplet counting (IR interrupt) and an embedded **TinyML XGBoost regression model**, Smart-Drip predicts non-linear infusion completion times (ETA) influenced by hydrostatic head pressure decay, prevents fatal air embolisms, and transmits real-time telemetry to hospital ward dashboards.

---

## Table of Contents

- [1. Clinical Problem & Motivation](#1-clinical-problem--motivation)
- [2. System Architecture](#2-system-architecture)
- [3. Key Features](#3-key-features)
- [4. Hardware Components & Pinout](#4-hardware-components--pinout)
- [5. TinyML Predictive Analytics Pipeline](#5-tinyml-predictive-analytics-pipeline)
- [6. Anomaly Detection & Alert Engine](#6-anomaly-detection--alert-engine)
- [7. Blynk IoT Cloud Integration](#7-blynk-iot-cloud-integration)
- [8. Repository Structure](#8-repository-structure)
- [9. Getting Started & Flashing](#9-getting-started--flashing)
- [10. Hardware Schematics & Research](#10-hardware-schematics--research)

---

## 1. Clinical Problem & Motivation

In conventional clinical inpatient wards, intravenous (IV) therapy relies heavily on periodic manual inspection by nursing staff. This approach presents critical failure modes:
1. **Air Embolism & Catheter Backflow:** When an IV bag empties unnoticed, venous back-pressure causes blood to flow back into the line, clotting the cannula, or introduces air into the circulatory system.
2. **Non-Linear Hydrostatic Decay:** As fluid leaves the IV container, the fluid column height ($h$) decreases, reducing hydrostatic pressure ($P = \rho g h$) and decelerating the drip rate. Linear estimations ($\text{Time} = \text{Volume} / \text{Rate}$) fail with up to 35% error near bag completion.
3. **Occlusions & Phlebitis:** Kinked tubing or vein collapse stops medication delivery without immediate auditory warnings.
4. **Runaway Infusion:** Accidental clamp slippage results in fluid overload.

**Smart-Drip resolves these challenges** with continuous dual-sensor fusion, edge AI inference, and autonomous multi-tiered local and cloud alarms.

---

## 2. System Architecture

```mermaid
flowchart TD
    subgraph Hardware["1. Sensing Layer"]
        A[IV Fluid Container] -->|Strain Force| B[HX711 24-bit ADC + Load Cell]
        A -->|Optical Droplet Passage| C[IR Phototransistor LM393]
    end

    subgraph Firmware["2. Edge Intelligence (ESP32-S3)"]
        B -->|GPIO 4, 5| D[Mass & Volume Estimator]
        C -->|GPIO 2 (Hardware ISR)| E[Drip Frequency & EMA Smoother]
        D --> F[6-Dimensional Feature Vector]
        E --> F
        F --> G[TinyML XGBoost Regressor (Zero-Heap C)]
        G --> H[Accurate Depletion ETA]
    end

    subgraph Interface["3. User Interface & IoT Cloud"]
        H --> I[SSD1306 128x64 OLED Display]
        D --> I
        E --> I
        H --> J[Acoustic Alarm Engine (Buzzer GPIO 15)]
        H --> K[Blynk IoT Cloud / Mobile App]
        D --> K
        E --> K
    end
```

---

## 3. Key Features

- **Dual-Sensor Fusion:** Combines continuous gravimetric mass telemetry (HX711 24-bit ADC) with high-speed optical droplet counting.
- **Bare-Metal Interrupt Handling:** Drip counter operates in an `IRAM_ATTR` ISR with hardware-level debouncing for sub-millisecond droplet capture without missing counts during Wi-Fi transmissions.
- **On-Device TinyML Inference:** Computes remaining infusion time using a transpiled C model (`model_code.h`) with **< 120 µs execution time** and **zero dynamic memory allocation**.
- **Quad-Telemetry OLED UI:** Local 128×64 graphical display showing Volume Remaining (mL), Percentage (%), Drip Rate (dpm), and AI ETA (min).
- **Multi-Stage Anomaly Detection:**
  - Occlusion / Line Blockage Alert (< 5 DPM with fluid remaining)
  - Runaway Flow Alert (> 80 DPM)
  - Sluggish Infusion Warning (< 10 DPM)
  - 50% Infusion Milestone Alert
  - Critical Empty Warning (≤ 20% with 3-second continuous acoustic buzzer alarm)
- **IoT Cloud Connectivity:** Wireless synchronization with the Blynk IoT mobile/web dashboard over 2.4 GHz Wi-Fi.

---

## 4. Hardware Components & Pinout

### Component Bill of Materials
- **Microcontroller:** ESP32-S3 DevKitC-1 (Xtensa 32-bit LX7 dual-core, 240 MHz, Wi-Fi + BLE 5.0)
- **Weight Sensor:** Strain-gauge load cell (1 kg / 5 kg) coupled to an **HX711 24-bit ADC**
- **Drip Counter:** Infrared optical break-beam sensor module (LM393 comparator / FC-33)
- **Display:** 0.96-inch monochrome I2C OLED (SSD1306, 128×64 pixels)
- **Acoustic Alarm:** 5V active/passive piezo buzzer
- **Power:** Regulated 5V / 3.3V power supply

### Pin Mapping Table

| Component | Pin Label | ESP32-S3 GPIO | Description / Mode | Voltage |
| :--- | :--- | :--- | :--- | :--- |
| **HX711 ADC** | `DOUT` | `GPIO 4` | Serial Data Output | 3.3V |
| | `SCK` | `GPIO 5` | Serial Clock Input | 3.3V |
| **OLED (SSD1306)** | `SDA` | `GPIO 8` | I2C Data | 3.3V |
| | `SCL` | `GPIO 9` | I2C Clock | 3.3V |
| **IR Drip Sensor**| `OUT` | `GPIO 2` | Hardware Interrupt (`FALLING`, `INPUT_PULLUP`) | 3.3V |
| **Piezo Buzzer** | `POS (+)` | `GPIO 15` | Digital Output Alarm | 3.3V / 5V |

For wiring diagrams and detailed schematics, see [hardware/PINOUT_AND_WIRING.md](file:///d:/Coding/esp32_s3_project/hardware/PINOUT_AND_WIRING.md).

---

## 5. TinyML Predictive Analytics Pipeline

The TinyML pipeline models the physical hydrostatic head decay of IV containers:

$$P_{\text{hydrostatic}}(t) = \rho \cdot g \cdot h(t)$$

```
                                      6 Input Features
                               ┌─────────────────────────────┐
                               │ 0: smoothed_drip_rate (dpm) │
                               │ 1: current_weight (kg)      │
Raw Sensor Telemetry           │ 2: total_weight_lost (kg)   │      m2cgen C Code
[HX711 + IR Interrupt] ───────>│ 3: elapsed_time (min)       │───> [model_code.h] ───> Predicted ETA (min)
                               │ 4: weight_loss_rate (kg/min)│     score(double*)
                               │ 5: linear_eta (min)         │
                               └─────────────────────────────┘
```

### Feature Breakdown
1. `smoothed_drip_rate`: EMA-filtered droplet frequency.
2. `current_weight`: Gross remaining mass in kg from load cell.
3. `total_weight_lost`: Cumulative mass dispensed ($W_0 - W_t$).
4. `elapsed_time`: Minutes elapsed since start of infusion.
5. `weight_loss_rate`: Differential depletion speed ($\Delta W / \Delta t$).
6. `linear_eta`: Naive linear estimate ($W_t / (\Delta W / \Delta t)$).

Details on model training, hyperparameter selection, and transpilation can be found in [tinyml/README.md](file:///d:/Coding/esp32_s3_project/tinyml/README.md).

---

## 6. Anomaly Detection & Alert Engine

The firmware contains an autonomous safety engine evaluated during every 1-second sampling window:

| Condition | Rule Threshold | Audio Alarm Pattern | Cloud Event |
| :--- | :--- | :--- | :--- |
| **Occlusion / Blockage** | DPM < 5 and Level > 10% | 3 Short Beeps | `iv_alert`: "Possible blockage detected!" |
| **Tachy-Infusion (Fast)** | DPM > 80 | 3 Short Beeps | `iv_alert`: "Drip rate too fast!" |
| **Brady-Infusion (Slow)** | 5 ≤ DPM < 10 and Level > 20% | 2 Short Beeps | `iv_alert`: "Drip rate too slow!" |
| **50% Milestone** | 40% ≤ Level ≤ 50% | 1 Beep | `iv_alert`: "IV Bag is at 50%" |
| **Critical Empty** | Level ≤ 20% | Continuous 3-second Alarm | `iv_alert`: "IV Bag critically LOW" |

---

## 7. Blynk IoT Cloud Integration

Smart-Drip streams telemetry over Wi-Fi directly to the Blynk IoT Cloud platform:

### Virtual Pin Mappings
- **`V0` (Integer):** Fluid Volume Remaining (`mL`)
- **`V1` (Integer):** Bag Level (`%`)
- **`V2` (Float):** Real-time Drip Rate (`dpm`)
- **`V3` (Integer):** TinyML Predicted Depletion ETA (`min`)

### Event Logging
- Event Code: `iv_alert` with push notification support to on-duty nurse smartphone terminals.

---

## 8. Repository Structure

```
Smart-Drip/
├── .gitignore                      # Tailored ignore rules for ESP32, PlatformIO & C++
├── platformio.ini                  # PlatformIO target, dependencies & build flags
├── README.md                       # Master project documentation
│
├── src/                            # Embedded C++ firmware
│   └── main.cpp                    # Application entry point, ISR, loop & Blynk tasks
│
├── include/                        # C/C++ Header Files
│   ├── model_code.h                # Transpiled TinyML XGBoost inference model
│   ├── config.h.example            # Template for Wi-Fi and Blynk credentials
│   └── README                      # PlatformIO include directory guidelines
│
├── tinyml/                         # Machine learning training & export pipeline
│   ├── train_iv_model.py           # Synthetic dataset generator, trainer & C exporter
│   ├── model_code.h                # Standalone C model artifact
│   ├── requirements.txt            # Python dependencies (XGBoost, m2cgen, Pandas)
│   └── README.md                   # Machine learning methodology & physics docs
│
├── hardware/                       # Hardware schematics & wiring specifications
│   ├── PINOUT_AND_WIRING.md        # Comprehensive pin connections & circuit diagram
│   └── schematics/
│       └── review1_main.pdf        # Academic review, schematics & research report
│
├── docs/                           # System architecture & technical documents
│   └── ARCHITECTURE.md             # Firmware state machine & Mermaid diagrams
│
├── lib/                            # Private project libraries (PlatformIO)
└── test/                           # Embedded unit tests (Unity framework)
```

---

## 9. Getting Started & Flashing

### Prerequisites
- [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE Extension](https://platformio.org/platformio-ide) installed.
- Python 3.10+ (optional, only required if retraining the TinyML model).

### Firmware Build & Flash
1. **Clone the repository:**
   ```bash
   git clone https://github.com/nayanpiyush007-cmyk/Smart-Drip-An-Automated-real-time-IV-Monitoring-Predictive-Analytics-System-Using-Tiny-ML.git
   cd Smart-Drip-An-Automated-real-time-IV-Monitoring-Predictive-Analytics-System-Using-Tiny-ML
   ```

2. **Configure Credentials:**
   Copy `include/config.h.example` to `include/config.h`:
   ```bash
   cp include/config.h.example include/config.h
   ```
   Edit `include/config.h` with your Wi-Fi SSID, password, and Blynk Auth Token:
   ```cpp
   #define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
   #define BLYNK_TEMPLATE_NAME "SmartDripVein"
   #define BLYNK_AUTH_TOKEN    "YOUR_AUTH_TOKEN"
   #define WIFI_SSID           "YOUR_WIFI_SSID"
   #define WIFI_PASS           "YOUR_WIFI_PASSWORD"
   ```

3. **Build & Upload via PlatformIO:**
   Connect the ESP32-S3 via USB and run:
   ```bash
   pio run --target upload
   pio device monitor
   ```

### (Optional) Retraining the TinyML Model
```bash
cd tinyml
pip install -r requirements.txt
python train_iv_model.py
```
This automatically updates `include/model_code.h` and `tinyml/model_code.h`.

---

## 10. Hardware Schematics & Research

For in-depth analysis of the bare-metal assembly ISR, component selection rationale, and near-infrared (NIR) vein detection pipelines, consult the complete project research report:
- [hardware/schematics/review1_main.pdf](file:///d:/Coding/esp32_s3_project/hardware/schematics/review1_main.pdf)

---

## License

This project is open-source and released under the [MIT License](LICENSE).
