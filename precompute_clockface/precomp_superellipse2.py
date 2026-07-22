#!/usr/bin/env python3
import math
import os
import re
import numpy as np

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

max_dx, min_dx = 29, -29
max_dy, min_dy = 43, -39

grid = np.zeros((clockface_height, clockface_width), dtype=np.uint8)
for r in range(clockface_height):
    for c in range(clockface_width):
        byte_idx = r * bytes_per_row + (c // 8)
        bit_offset = c % 8
        if (clockface_bits[byte_idx] & (1 << bit_offset)) != 0:
            grid[r, c] = 1

final_offsets = []
p_sub_x = pivot_x + 0.5
p_sub_y = pivot_y + 0.5

# 2. Re-evaluate subpixel ray casting using optimized parameters per quadrant
for phase in range(60):
    target_angle = math.radians(phase * 6.0)
    sin_a = math.sin(target_angle)
    cos_a = math.cos(target_angle)
    
    # Inject optimized metrics
    if cos_a >= 0 and sin_a >= 0:      # Q1: Top-Right
        a, b, n = 30.4978, 39.6495, 2.4166
    elif cos_a < 0 and sin_a >= 0:    # Q2: Bottom-Right
        a, b, n = 30.7448, 43.3683, 2.6605
    elif cos_a < 0 and sin_a < 0:     # Q3: Bottom-Left
        a, b, n = 30.7717, 43.3313, 2.6324
    else:                             # Q4: Top-Left
        a, b, n = 30.4939, 39.7917, 2.4017

    cos_factor = math.pow(abs(cos_a), n) if abs(cos_a) > 1e-7 else 0.0
    sin_factor = math.pow(abs(sin_a), n) if abs(sin_a) > 1e-7 else 0.0
    
    # Corrected denominator notation
    denom = math.pow((cos_factor / math.pow(b, n)) + (sin_factor / math.pow(a, n)), -1.0 / n)
    
    target_dx = denom * sin_a
    target_dy = -denom * cos_a
    unwarped_angle = math.atan2(target_dx, -target_dy)
    
    last_safe_dx, last_safe_dy = 0, 0
    
    # High-density incremental stepping outward
    for step in range(1, 1500):
        dist = step * 0.1
        f_dx = dist * math.sin(unwarped_angle)
        f_dy = -dist * math.cos(unwarped_angle)
        
        cur_sub_x = p_sub_x + f_dx
        cur_sub_y = p_sub_y + f_dy
        cur_x = int(math.floor(cur_sub_x))
        cur_y = int(math.floor(cur_sub_y))
        
        dx = int(round(f_dx))
        dy = int(round(f_dy))
        
        # Hard limits check
        if dx > max_dx or dx < min_dx or dy > max_dy or dy < min_dy:
            break
        if cur_x < 0 or cur_x >= clockface_width or cur_y < 0 or cur_y >= clockface_height:
            break
        if grid[cur_y, cur_x] != 0: # Pierced the outer clock face wall margin
            break
            
        last_safe_dx, last_safe_dy = dx, dy
        
    final_offsets.append((last_safe_dx, last_safe_dy))

# 3. Output structural C table source file
with open("precomp_superellipse.c", "w") as out:
    out.write("static const HandMasterOffset HAND_MASTER_OFFSETS[TOTAL_HAND_PHASES] = {\n")
    for idx, (dx, dy) in enumerate(final_offsets):
        comma = "," if idx < 59 else " "
        out.write(f"    {{ {dx:3d}, {dy:3d} }}{comma} // Phase {idx}\n")
    out.write("};\n")

print("Processing complete. Clean table written to 'precomp_superellipse.c'.")
