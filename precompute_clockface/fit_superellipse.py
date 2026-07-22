#!/usr/bin/env python3
import math
import os
import re
import numpy as np
import cv2
from scipy.optimize import minimize

def load_xbm_bytes(file_path):
    if not os.path.exists(file_path):
        raise FileNotFoundError(f"Required source asset '{file_path}' not found in local path.")
    with open(file_path, "r") as f:
        content = f.read()
    hex_data = re.search(r'\{(.*?)\}', content, re.DOTALL)
    if not hex_data:
        raise ValueError("Invalid XBM data layout payload segment structure block.")
    return [int(x.strip(), 16) for x in hex_data.group(1).split(',') if x.strip()]

# 1. Load the XBM asset mask grid configuration
clockface_bits = load_xbm_bytes("clockface.xbm")
clockface_width = 64
clockface_height = 96
bytes_per_row = 8
pivot_x = 31
pivot_y = 45

grid = np.zeros((clockface_height, clockface_width), dtype=np.uint8)
for r in range(clockface_height):
    for c in range(clockface_width):
        byte_idx = r * bytes_per_row + (c // 8)
        bit_offset = c % 8
        if (clockface_bits[byte_idx] & (1 << bit_offset)) != 0:
            grid[r, c] = 1

# 2. Extract boundary points where empty cells (0) meet the clock face wall (1)
boundary_points = []
p_sub_x = pivot_x + 0.5
p_sub_y = pivot_y + 0.5

for r in range(1, clockface_height - 1):
    for c in range(1, clockface_width - 1):
        if grid[r, c] == 1:
            # Check 4-connectivity neighbors to find edge boundaries
            if grid[r+1, c]==0 or grid[r-1, c]==0 or grid[r, c+1]==0 or grid[r, c-1]==0:
                dx = (c + 0.5) - p_sub_x
                dy = (r + 0.5) - p_sub_y
                angle = math.atan2(dx, -dy) # Aligned with original orientation layout
                radius = math.sqrt(dx**2 + dy**2)
                boundary_points.append((dx, dy, angle, radius))

# 3. Separate boundary data sets into their respective directional quadrants
quadrants = {
    "Q1 (Top-Right)":    [],
    "Q2 (Bottom-Right)": [],
    "Q3 (Bottom-Left)":  [],
    "Q4 (Top-Left)":     []
}

for pt in boundary_points:
    dx, dy, angle, radius = pt
    # Determine quadrant matching the precomp system checks
    if dx >= 0 and dy <= 0:
        quadrants["Q1 (Top-Right)"].append(pt)
    elif dx >= 0 and dy > 0:
        quadrants["Q2 (Bottom-Right)"].append(pt)
    elif dx < 0 and dy > 0:
        quadrants["Q3 (Bottom-Left)"].append(pt)
    else:
        quadrants["Q4 (Top-Left)"].append(pt)

# 4. Define the Lamé curve target objective loss function
def superellipse_loss(params, points):
    a, b, n = params
    if a <= 0 or b <= 0 or n <= 0:
        return 1e9
    
    total_error = 0.0
    for dx, dy, angle, actual_r in points:
        sin_a = math.sin(angle)
        cos_a = math.cos(angle)
        
        # The mathematically correct polar radius function for a superellipse
        cos_factor = math.pow(abs(cos_a), n) if abs(cos_a) > 1e-7 else 0.0
        sin_factor = math.pow(abs(sin_a), n) if abs(sin_a) > 1e-7 else 0.0

        denom = math.pow((cos_factor / math.pow(b, n)) + (sin_factor / math.pow(a, n)), -1.0 / n)
        
        total_error += (denom - actual_r) ** 2
        
    return total_error / len(points)

# 5. Run numerical minimization loop over each independent quadrant profile
print("=== OPTIMIZING LAMÉ CURVE PARAMETERS ===")
for name, pts in quadrants.items():
    if len(pts) == 0:
        print(f"{name}: No boundary coordinates captured.")
        continue
        
    # Initial guessing weights: [a, b, n] centered on current script geometry estimates
    initial_guess = [35.0, 42.0, 2.4]
    
    # Boundary guard limits to keep optimization stable
    bounds = ((10.0, 100.0), (10.0, 100.0), (1.0, 5.0))
    
    res = minimize(superellipse_loss, initial_guess, args=(pts,), method='Nelder-Mead', bounds=bounds)
    opt_a, opt_b, opt_n = res.x
    
    print(f"\n{name}:")
    print(f"  -> Optimized Parameters: a = {opt_a:.4f}, b = {opt_b:.4f}, n = {opt_n:.4f}")
    print(f"  -> Mean Square Distance Error: {res.fun:.6f} px")
# 6. Verification Pass: Render the optimized piecewise curves over the mask
# Create an upscaled 4x visualization grid for clean subpixel rendering bounds
vis_scale = 4
vis_h, vis_w = clockface_height * vis_scale, clockface_width * vis_scale
vis_img = np.zeros((vis_h, vis_w, 3), dtype=np.uint8)

# Draw the background mask (white for clock face interior, black for empty space)
for r in range(clockface_height):
    for c in range(clockface_width):
        color = [255, 255, 255] if grid[r, c] == 1 else [20, 20, 20]
        cv2.rectangle(vis_img, 
                      (c * vis_scale, r * vis_scale), 
                      ((c + 1) * vis_scale - 1, (r + 1) * vis_scale - 1), 
                      color, -1)

# Center pivot point in the upscaled visualization space
v_pivot_x = p_sub_x * vis_scale
v_pivot_y = p_sub_y * vis_scale

# Plot 360 analytical points (1 point per degree) using the optimized quadrant parameters
for degree in range(360):
    angle = math.radians(degree)
    sin_a = math.sin(angle)
    cos_a = math.cos(angle)
    
    # Select the precise optimized parameter triplet matching the target quadrant
    if cos_a >= 0 and sin_a >= 0:    # Q1: Top-Right
        a, b, n = 30.4978, 39.6495, 2.4166
    elif cos_a < 0 and sin_a >= 0:  # Q2: Bottom-Right
        a, b, n = 30.7448, 43.3683, 2.6605
    elif cos_a < 0 and sin_a < 0:   # Q3: Bottom-Left
        a, b, n = 30.7717, 43.3313, 2.6324
    else:                           # Q4: Top-Left
        a, b, n = 30.4939, 39.7917, 2.4017

    cos_factor = math.pow(abs(cos_a), n) if abs(cos_a) > 1e-7 else 0.0
    sin_factor = math.pow(abs(sin_a), n) if abs(sin_a) > 1e-7 else 0.0
    
    # Calculate radius and project the corresponding coordinate vector
    denom = math.pow((cos_factor / math.pow(b, n)) + (sin_factor / math.pow(a, n)), -1.0 / n)
    
    target_dx = denom * sin_a * vis_scale
    target_dy = -denom * cos_a * vis_scale
    
    # Render the continuous mathematical curve as a bright green pixel trail
    px = int(round(v_pivot_x + target_dx))
    py = int(round(v_pivot_y + target_dy))
    
    if 0 <= px < vis_w and 0 <= py < vis_h:
        cv2.circle(vis_img, (px, py), 1, (0, 255, 0), -1)

# Highlight center hub axle with a distinct blue marker dot
cv2.circle(vis_img, (int(round(v_pivot_x)), int(round(v_pivot_y))), 4, (255, 0, 0), -1)
cv2.imwrite("superellipse_fit_verification.png", vis_img)
print("\nSanity check rendering committed to 'superellipse_fit_verification.png'.")
