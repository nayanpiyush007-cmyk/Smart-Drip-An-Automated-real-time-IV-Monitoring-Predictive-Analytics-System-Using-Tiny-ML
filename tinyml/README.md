# Smart-Drip TinyML Pipeline: Hydrostatic Decay Regression

This directory contains the machine learning training pipeline that predicts remaining intravenous (IV) infusion time on-device (TinyML) running on the ESP32-S3 microcontroller.

---

## 1. Physical Motivation: Hydrostatic Decay

Standard clinical IV infusion calculations assume a strictly constant linear flow rate:
$$\text{ETA}_{\text{linear}} = \frac{\text{Volume Remaining}}{\text{Flow Rate}}$$

However, in gravity-fed intravenous infusion sets without motorized peristaltic pumps, the flow rate is governed by Torricelli's Law and the hydrostatic fluid column height:
$$P_h = \rho \cdot g \cdot h(t)$$
Where:
- $\rho$ is the fluid density
- $g$ is gravitational acceleration
- $h(t)$ is the dynamic height of the fluid level in the IV bottle/bag

As the fluid is dispensed, the height $h(t)$ decreases, leading to a non-linear drop in hydrostatic driving pressure and an exponential decay in the instantaneous drip rate. Estimating time-to-empty using simple linear extrapolation introduces significant clinical error (up to 20–35% divergence near the final third of the infusion).

---

## 2. Feature Engineering

The regression model takes **6 engineered features** that capture both the static state and the temporal rate of change:

| Index | Feature Name | Unit | Clinical Significance |
| :---: | :--- | :---: | :--- |
| `input[0]` | `smoothed_drip_rate` | drops/min | Exponential moving average (EMA) of droplet frequency |
| `input[1]` | `current_weight` | kg | Direct gross mass measured by the 24-bit HX711 load cell |
| `input[2]` | `total_weight_lost` | kg | Cumulative mass delivered since infusion onset ($W_0 - W_t$) |
| `input[3]` | `elapsed_time` | minutes | Infusion operational duration |
| `input[4]` | `weight_loss_rate` | kg/min | Differential depletion rate ($\Delta W / \Delta t$) |
| `input[5]` | `linear_eta` | minutes | Baseline naive linear expectation ($W_t / (\Delta W / \Delta t)$) |

**Target Variable:** `actual_time_remaining` (minutes until IV bag is completely depleted).

---

## 3. Model Architecture & Transpilation

- **Algorithm:** XGBoost Regressor (`n_estimators=200`, `max_depth=4`, `learning_rate=0.05`, `objective='reg:squarederror'`)
- **Transpilation Framework:** `m2cgen` (Model to Code Generator)
- **Embedded Target:** ESP32-S3 (Xtensa dual-core 32-bit LX7)
- **Deployment Format:** Pure C function `double score(double *input)` in [model_code.h](file:///d:/Coding/esp32_s3_project/include/model_code.h)
- **Zero-Dependency Architecture:**
  - No TensorFlow Lite for Microcontrollers (TFLM) runtime overhead
  - No dynamic heap allocation (`malloc`/`free`)
  - Execution time: **< 120 microseconds** per inference pass on ESP32-S3 at 240 MHz

---

## 4. Retraining & Generating the Header

To retrain the model or update hyperparameters:

```bash
# 1. Install dependencies
pip install -r requirements.txt

# 2. Run training and C code transpilation
python train_iv_model.py
```

The script will automatically compile the XGBoost decision trees into pure C `if-else` branches and update:
- `tinyml/model_code.h`
- `include/model_code.h` (for PlatformIO compilation)
