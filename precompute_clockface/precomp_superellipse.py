#!/usr/bin/env python3
import math
import os
import re

def load_xbm_bytes(file_path):
    """
    Parses a local XBM file and returns a ready-to-use array of integers
    representing the raw byte stream, decoupling file loading from processing.
    """
    with open(file_path, "r") as f:
        content = f.read()
    
    hex_data = re.search(r'\{(.*?)\}', content, re.DOTALL)
    if not hex_data:
        raise ValueError("Invalid XBM data layout payload segment structure block.")
        
    return [int(x.strip(), 16) for x in hex_data.group(1).split(',') if x.strip()]

# 1. Load asset cleanly from local workspace file path
clockface_bits = load_xbm_bytes("clockface.xbm")

clockface_width = 64
clockface_height = 96
bytes_per_row = 8

pivot_x = 31
pivot_y = 45

# Bounding box container dimensions
max_dx, min_dx = 29, -29
max_dy, min_dy = 43, -39

def is_black_boundary_wall(lx, ly):
    if lx < 0 or lx >= clockface_width or ly < 0 or ly >= clockface_height:
        return False
    byte_idx = ly * bytes_per_row + (lx // 8)
    bit_offset = lx % 8
    # Search for non-zero bit activations to identify the baked-in black border
    return (clockface_bits[byte_idx] & (1 << bit_offset)) != 0

# 2. Process Lamé Curve & Asymmetrical 3D Projection Tracing Matrix
final_offsets = []

for phase in range(60):
    # Pure base 6-degree angular tracking step per phase
    target_angle = math.radians(phase * 6.0)
    
    # Extract sin/cos vectors to analyze quad distributions
    sin_a = math.sin(target_angle)
    cos_a = math.cos(target_angle)
    
    # 3D Projection Parameters: Establish independent baseline metrics for each quadrant.
    # This accounts for the 3D perspective distortion slant and unskewing requirements.
    if cos_a >= 0 and sin_a >= 0:    # Quadrant 1 (0 to 90 degrees)
        a, b = 35.0, 42.0            # Over-projected semi-axes (Width, Height)
        n = 2.4                      # Lamé curve shape exponent factor
    elif cos_a < 0 and sin_a >= 0:   # Quadrant 2 (90 to 180 degrees)
        a, b = 35.0, 46.0            # Lengthened bottom profile to catch the skew shortfall
        n = 2.2                      # Adjusted roundness profile for the lower tilt shift
    elif cos_a < 0 and sin_a < 0:    # Quadrant 3 (180 to 270 degrees)
        a, b = 35.0, 46.0            
        n = 2.2                      
    else:                            # Quadrant 4 (270 to 360 degrees)
        a, b = 35.0, 42.0            
        n = 2.4                      

    # Lamé Curve Equation transformation pass:
    # Calculates the parametric intersection point for the given angle along the curve contour.
    cos_factor = math.pow(abs(cos_a), 2.0 / n) if abs(cos_a) > 1e-7 else 0.0
    sin_factor = math.pow(abs(sin_a), 2.0 / n) if abs(sin_a) > 1e-7 else 0.0
    
    denom = math.pow((cos_factor / math.pow(b, 2.0 / n)) + (sin_factor / math.pow(a, 2.0 / n)), -n / 2.0)
    
    # Determine maximum vector scale coordinates for the unwarped superellipse target
    target_dx = denom * sin_a
    target_dy = -denom * cos_a
    
    # Resolve projection angle heading path
    unwarped_angle = math.atan2(target_dx, -target_dy)
    
    last_safe_dx, last_safe_dy = 0, 0
    
    # Trace ray outward using sub-pixel increments until hitting the black edge masks
    for step in range(1, 1500):
        dist = step * 0.1
        dx = int(round(dist * math.sin(unwarped_angle)))
        dy = int(round(-dist * math.cos(unwarped_angle)))
        
        cur_x = pivot_x + dx
        cur_y = pivot_y + dy
        
        # Clip ray execution if canvas box dimensions are exceeded
        if dx > max_dx or dx < min_dx or dy > max_dy or dy < min_dy:
            break
            
        if cur_x < 0 or cur_x >= clockface_width or cur_y < 0 or cur_y >= clockface_height:
            break
            
        # Hit target black mask boundary ink: Terminate pass immediately 
        if is_black_boundary_wall(cur_x, cur_y):
            break
            
        # Caches the preceding pixel state location to terminate cleanly on last white pixel
        last_safe_dx, last_safe_dy = dx, dy
        
    final_offsets.append((last_safe_dx, last_safe_dy))

# Output formatted array table initialization directly to console output lines
print("static const HandMasterOffset HAND_MASTER_OFFSETS[TOTAL_HAND_PHASES] = {")
for idx, (dx, dy) in enumerate(final_offsets):
    comma = "," if idx < 59 else ""
    print(f"    {{ {dx:3d}, {dy:3d} }}{comma} // Phase {idx}")
print("};")
