import pandas as pd
import numpy as np
from xgboost import XGBRegressor
import m2cgen as m2c

print("--- SMART-DRIP TINYML GENERATOR ---")
print("1. Generating Synthetic Hydrostatic Decay Data...")

np.random.seed(42)
num_samples = 2000

start_weight = 0.500 
current_weight = np.random.uniform(0.01, 0.500, num_samples)
elapsed_time = (0.500 - current_weight) * 120 

lambda_decay = 0.01 
base_rate = 60 
smoothed_drip_rate = base_rate * np.exp(-lambda_decay * elapsed_time) + np.random.normal(0, 0.02, num_samples)

total_weight_lost = start_weight - current_weight
weight_loss_rate = total_weight_lost / (elapsed_time + 0.1) 
linear_eta = current_weight / (weight_loss_rate + 0.001)

actual_time_remaining = (current_weight / 0.500) * 120 * (1 + (0.500 - current_weight)) 

df = pd.DataFrame({
    'smoothed_drip_rate': smoothed_drip_rate,
    'current_weight': current_weight,
    'total_weight_lost': total_weight_lost,
    'elapsed_time': elapsed_time,
    'weight_loss_rate': weight_loss_rate,
    'linear_eta': linear_eta,
    'actual_time_remaining': actual_time_remaining
})

X = df[['smoothed_drip_rate', 'current_weight', 'total_weight_lost', 'elapsed_time', 'weight_loss_rate', 'linear_eta']]
y = df['actual_time_remaining']

print("2. Data prepared! Training the XGBoost Regressor...")

# ---> THE FIX IS HERE (base_score=0.5) <---
model = XGBRegressor(
    n_estimators=200,      
    max_depth=4,           
    learning_rate=0.05,    
    objective='reg:squarederror',
    base_score=0.5 
)

model.fit(X, y)
print("3. Training complete! Transpiling Python AI to native C code...")

c_code = m2c.export_to_c(model)
c_code = c_code.replace(" nan ", " 0.5 ").replace("nan +", "0.5 +").replace("+ nan", "+ 0.5")
with open("model_code.h", "w") as f:
    f.write(c_code)

print("\nSUCCESS! A clean 'model_code.h' has been generated in your folder.")