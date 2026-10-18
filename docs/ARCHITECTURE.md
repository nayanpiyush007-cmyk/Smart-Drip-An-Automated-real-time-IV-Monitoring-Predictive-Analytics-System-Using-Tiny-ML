# System Architecture: Smart-Drip

This document outlines the software execution model, state machine, interrupt pipeline, and edge machine learning inference lifecycle for the **Smart-Drip System**.

---

## 1. High-Level Architecture

The system operates across three tightly integrated tiers:

```mermaid
flowchart TD
    subgraph Sensing["1. Physical & Sensing Layer"]
        A[IV Fluid Column] -->|Strain Force| B[HX711 24-bit ADC]
        A -->|Optical Droplet Interruption| C[IR Phototransistor]
    end

    subgraph Edge["2. Edge Compute & TinyML (ESP32-S3)"]
        B -->|Digital Raw Bits (GPIO 4, 5)| D[Mass & Volume Estimator]
        C -->|Hardware ISR (GPIO 2)| E[Droplet Frequency & EMA Smoother]
        D --> F[6-Feature Vector Builder]
        E --> F
        F --> G[XGBoost TinyML Inference Engine]
        G --> H[Infusion Depletion ETA]
    end

    subgraph Actuation["3. Alerting & Telemetry"]
        H --> I[I2C SSD1306 OLED Display]
        D --> I
        E --> I
        H --> J[Anomaly Rule Engine]
        J -->|Occlusion / Runaway / Low Level| K[Piezo Buzzer]
        J -->|Event Notification| L[Blynk IoT Cloud]
        H -->|Virtual Pins V0-V3| L
    end
```

---

## 2. Firmware Pipeline & Execution Cycles

1. **Bare-Metal Asynchronous Droplet ISR (`countDrop()`):**
   - Attached to `GPIO 2` on the `FALLING` edge.
   - Executes inside `IRAM_ATTR` to bypass flash cache latency.
   - Employs software debounce (`timeDiff > 50ms`) to eliminate fluid bounce artifacts.
   - Calculates dynamic inter-drop interval ($\Delta t = t_n - t_{n-1}$) with microsecond resolution.

2. **Periodic Sampling Loop (`measureAndPredict()` every 1000 ms):**
   - Computes smoothed drip rate using an Exponential Moving Average (EMA):
     $$\text{DPM}_{\text{smoothed}} = 0.8 \cdot \text{DPM}_{t-1} + 0.2 \cdot \text{DPM}_{\text{new}}$$
   - Reads 5-sample averaged units from the HX711 load cell.
   - Derives instantaneous volume (mL) and remaining percentage (%).

3. **TinyML Feature Vector Assembly & Inference:**
   - Evaluates warm-up guards: requires at least 30 seconds of infusion activity and > 20 grams of fluid loss before executing predictions (`minsLeft = 999` during warmup).
   - Passes array `double model_features[6]` into transpiled function `score(model_features)` from `model_code.h`.
   - Returns real-time time-to-depletion in minutes without dynamic memory overhead.

4. **Multi-Condition Alert Engine:**
   - **Occlusion Detection:** DPM < 5 while bag percentage > 10%.
   - **Tachy-Infusion Warning:** DPM > 80.
   - **Brady-Infusion Warning:** DPM < 10 and percentage > 20%.
   - **Half-Capacity Milestone:** 40% ≤ percentage ≤ 50%.
   - **Critical Emptiness Alarm:** percentage ≤ 20% triggers 3-second continuous acoustic buzzer alarm to prevent catheter reflux and air embolism.

5. **Cloud Telemetry via Blynk:**
   - Virtual Pin mapping:
     - `V0`: Volume Remaining (mL)
     - `V1`: Percentage Remaining (%)
     - `V2`: Current Drip Rate (dpm)
     - `V3`: TinyML Depletion ETA (min)
