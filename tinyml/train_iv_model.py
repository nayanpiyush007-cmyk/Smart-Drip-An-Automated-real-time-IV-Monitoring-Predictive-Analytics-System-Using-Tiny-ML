#!/usr/bin/env python3
"""
Smart-Drip TinyML Model Generator
---------------------------------
Trains an XGBoost Regressor on synthetic hydrostatic decay fluid dynamics data,
then transpiles the trained model into zero-dependency native C code (m2cgen)
for execution on the ESP32-S3 microcontroller.
"""

import os
from pathlib import Path
import numpy as np
import pandas as pd
from xgboost import XGBRegressor
import m2cgen as m2c


def generate_synthetic_data(num_samples: int = 2000, random_seed: int = 42) -> pd.DataFrame:
    """
    Generates synthetic infusion dataset modeling hydrostatic decay.
    As the IV bag empties, the liquid column height drops, reducing
    gravitational head pressure and causing non-linear flow rate decay.
    """
    np.random.seed(random_seed)

    start_weight = 0.500  # 500 mL / 0.500 kg standard IV infusion bag
    current_weight = np.random.uniform(0.01, start_weight, num_samples)
    elapsed_time = (start_weight - current_weight) * 120  # minutes

    lambda_decay = 0.01
    base_rate = 60.0  # nominal drip rate (drops/min)
    smoothed_drip_rate = base_rate * np.exp(-lambda_decay * elapsed_time) + np.random.normal(0, 0.02, num_samples)

    total_weight_lost = start_weight - current_weight
    weight_loss_rate = total_weight_lost / (elapsed_time + 0.1)
    linear_eta = current_weight / (weight_loss_rate + 0.001)

    # Ground truth non-linear remaining infusion time considering pressure drop
    actual_time_remaining = (current_weight / start_weight) * 120.0 * (1.0 + (start_weight - current_weight))

    return pd.DataFrame({
        'smoothed_drip_rate': smoothed_drip_rate,
        'current_weight': current_weight,
        'total_weight_lost': total_weight_lost,
        'elapsed_time': elapsed_time,
        'weight_loss_rate': weight_loss_rate,
        'linear_eta': linear_eta,
        'actual_time_remaining': actual_time_remaining
    })


def train_and_export():
    print("==================================================")
    print("      SMART-DRIP TINYML GENERATOR & TRANSPILER    ")
    print("==================================================")

    print("\n1. Generating Synthetic Hydrostatic Decay Data...")
    df = generate_synthetic_data(num_samples=2000, random_seed=42)

    features = [
        'smoothed_drip_rate',
        'current_weight',
        'total_weight_lost',
        'elapsed_time',
        'weight_loss_rate',
        'linear_eta'
    ]
    X = df[features]
    y = df['actual_time_remaining']

    print(f"   Features ({len(features)}): {', '.join(features)}")
    print(f"   Total training samples: {len(df)}")

    print("\n2. Training XGBoost Regressor...")
    model = XGBRegressor(
        n_estimators=200,
        max_depth=4,
        learning_rate=0.05,
        objective='reg:squarederror',
        base_score=0.5,
        random_state=42
    )
    model.fit(X, y)
    print("   Model trained successfully.")

    print("\n3. Transpiling Python model to zero-dependency C code via m2cgen...")
    c_code = m2c.export_to_c(model)
    # Sanitize any potential NaN artifacts from tree leaves
    c_code = c_code.replace(" nan ", " 0.5 ").replace("nan +", "0.5 +").replace("+ nan", "+ 0.5")

    # Output paths: current tinyml folder and firmware include/ directory
    base_dir = Path(__file__).resolve().parent
    project_root = base_dir.parent
    firmware_include_dir = project_root / "include"

    destinations = [base_dir / "model_code.h"]
    if firmware_include_dir.exists():
        destinations.append(firmware_include_dir / "model_code.h")

    for dest in destinations:
        dest.parent.mkdir(parents=True, exist_ok=True)
        with open(dest, "w") as f:
            f.write(c_code)
        print(f"   Exported header -> {dest}")

    print("\nSUCCESS: TinyML model generated and exported successfully!\n")


if __name__ == "__main__":
    train_and_export()