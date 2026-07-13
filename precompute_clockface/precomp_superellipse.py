#!/usr/bin/env python3
import math
import os
import re
import cv2
import numpy as np

def load_xbm_bytes(file_path):
    """
    Parses a local XBM file dynamically and returns a ready-to-use array of integers
    representing the raw byte stream, decoupling file loading from processing.
    """
    if not os.path.exists(file_path):
        raise FileNotFoundError(f"Required source asset '{file_path}' not found in local path.")
        
    with open(file_path, "r") as f:
        content = f.read()
    
    hex_data = re.search(r'\{(.*?)\}', content, re.DOTALL)
    if not hex_data:
        raise ValueError("Invalid XBM data layout payload segment structure block.")
        
    return [int(x.strip(), 16) for x in hex_data.group(1).split(',') if x.strip()]

# 1. Dynamically read the 64x96 computational asset mask structure
clockface_bits = load_xbm_bytes("clockface.xbm")

clockface_width = 64
clockface_height = 96
bytes_per_row = 8

pivot_x = 31
pivot_y = 45

max_dx, min_dx = 29, -29
max_dy, min_dy = 43, -39

# 2. Extract and unpack the live bit-stream into a computational matrix grid
grid = np.zeros((clockface_height, clockface_width), dtype=np.uint8)
for r in range(clockface_height):
    for c in range(clockface_width):
        byte_idx = r * bytes_per_row + (c // 8)
        bit_offset = c % 8
        if (clockface_bits[byte_idx] & (1 << bit_offset)) != 0:
            grid[r, c] = 1

final_offsets = []

# 3. Subpixel Ray Cast tracking loop using original directional trajectory vectors
for phase in range(60):
    target_angle = math.radians(phase * 6.0)
    sin_a = math.sin(target_angle)
    cos_a = math.cos(target_angle)
    
    # Structural 3D transformation profiles tracking the quadrant skew shifts
    if cos_a >= 0 and sin_a >= 0:
        a, b = 35.0, 42.0; n = 2.4
    elif cos_a < 0 and sin_a >= 0:
        a, b = 35.0, 46.0; n = 2.2
    elif cos_a < 0 and sin_a < 0:
        a, b = 35.0, 46.0; n = 2.2
    else:
        a, b = 35.0, 42.0; n = 2.4

    cos_factor = math.pow(abs(cos_a), 2.0 / n) if abs(cos_a) > 1e-7 else 0.0
    sin_factor = math.pow(abs(sin_a), 2.0 / n) if abs(sin_a) > 1e-7 else 0.0
    denom = math.pow((cos_factor / math.pow(b, 2.0 / n)) + (sin_factor / math.pow(a, 2.0 / n)), -n / 2.0)
    
    target_dx = denom * sin_a
    target_dy = -denom * cos_a
    unwarped_angle = math.atan2(target_dx, -target_dy)
    
    last_safe_dx, last_safe_dy = 0, 0
    p_sub_x = pivot_x + 0.5
    p_sub_y = pivot_y + 0.5
    
    for step in range(1, 1500):
        dist = step * 0.1
        f_dx = dist * math.sin(unwarped_angle)
        f_dy = -dist * math.cos(unwarped_angle)
        
        # Check coverage sampling boundaries using the half-pixel center
        cur_sub_x = p_sub_x + f_dx
        cur_sub_y = p_sub_y + f_dy
        
        cur_x = int(math.floor(cur_sub_x))
        cur_y = int(math.floor(cur_sub_y))
        
        dx = int(round(f_dx))
        dy = int(round(f_dy))
        
        if dx > max_dx or dx < min_dx or dy > max_dy or dy < min_dy:
            break
        if cur_x < 0 or cur_x >= clockface_width or cur_y < 0 or cur_y >= clockface_height:
            break
        if grid[cur_y, cur_x] != 0: # Hit bounding clockface wall mask
            break
            
        last_safe_dx, last_safe_dy = dx, dy
        
    final_offsets.append((last_safe_dx, last_safe_dy))

# 4. Composite 2x Bilinear Output verification asset frame
vis_img = np.zeros((clockface_height, clockface_width, 3), dtype=np.uint8)
for r in range(clockface_height):
    for c in range(clockface_width):
        # White background for empty canvas area, black boundary for filled pixels
        vis_img[r, c] = [255, 255, 255] if grid[r, c] != 0 else [0, 0, 0]

# Perform 2x bilinear interpolation upscaling
vis_img_2x = cv2.resize(vis_img, (clockface_width * 2, clockface_height * 2), interpolation=cv2.INTER_LINEAR)

# Project red tracking dots onto the upscaled subpixel sampling canvas centers
for dx, dy in final_offsets:
    v_x_2x = int(round((pivot_x + 0.5 + dx) * 2))
    v_y_2x = int(round((pivot_y + 0.5 + dy) * 2))
    cv2.circle(vis_img_2x, (v_x_2x, v_y_2x), 2, (0, 0, 255), -1)

# Highlight central structural rotation axis axle point as a distinct blue dot
cv2.circle(vis_img_2x, (int((pivot_x + 0.5) * 2), int((pivot_y + 0.5) * 2)), 3, (255, 0, 0), -1)
cv2.imwrite("clockface_verification_2x.png", vis_img_2x)

# 5. Output structural configuration data array to separate precomp table file
with open("precomp_superellipse.c", "w") as out:
    out.write("static const HandMasterOffset HAND_MASTER_OFFSETS[TOTAL_HAND_PHASES] = {\n")
    for idx, (dx, dy) in enumerate(final_offsets):
        comma = "," if idx < 59 else " "
        out.write(f"    {{ {dx:3d}, {dy:3d} }}{comma} // Phase {idx}\n")
    out.write("};\n")

print("Processing successful. Data written to 'precomp_superellipse.c'. Image saved to 'clockface_verification_2x.png'.")
