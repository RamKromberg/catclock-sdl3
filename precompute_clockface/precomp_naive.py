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

# 1. Modular decoupled file loading stage
clockface_bits = load_xbm_bytes("clockface.xbm")

# 2. Immutable canvas parameters matching your updated GIMP layout properties
clockface_width = 64
clockface_height = 96
bytes_per_row = 8

# Asymmetrical anchors calculated directly with the new padding offsets
pivot_x = 31
pivot_y = 45

def is_black_boundary_wall(lx, ly):
    if lx < 0 or lx >= clockface_width or ly < 0 or ly >= clockface_height:
        return False
    byte_idx = ly * bytes_per_row + (lx // 8)
    bit_offset = lx % 8
    
    # FIXED BIT POLARITY: Your GIMP asset uses 1/0xff to declare boundary lines.
    # We must scan for non-zero bit activations to identify the outer shell.
    return (clockface_bits[byte_idx] & (1 << bit_offset)) != 0

# 3. Clean vector processing pass with 3D perspective skew & tilt adjustments
final_offsets = []

for phase in range(60):
    target_angle = math.radians(phase * 6.0)
    
    # 3D Perspective Tilt and Skew Unwarping
    unwarped_angle = math.atan2(math.sin(target_angle) * 59.0, math.cos(target_angle) * 83.0)
    
    # Track the last known safe (white) pixel offset location
    last_safe_dx, last_safe_dy = 0, 0
    
    # Trace cleanly outward using sub-pixel increments
    for step in range(1, 1200):
        dist = step * 0.1
        dx = int(round(dist * math.sin(unwarped_angle)))
        dy = int(round(-dist * math.cos(unwarped_angle)))
        
        cur_x = pivot_x + dx
        cur_y = pivot_y + dy
        
        # Physical sheet limit boundary clipping
        if cur_x < 0 or cur_x >= clockface_width or cur_y < 0 or cur_y >= clockface_height:
            break
            
        # Hit the border: Break out immediately WITHOUT saving this coordinate
        if is_black_boundary_wall(cur_x, cur_y):
            break
            
        # If the pixel is white/clear, cache it as our newest safe endpoint
        last_safe_dx, last_safe_dy = dx, dy
        
    final_offsets.append((last_safe_dx, last_safe_dy))

# Output formatted structure matrix directly to stdout
print("static const HandMasterOffset HAND_MASTER_OFFSETS[TOTAL_HAND_PHASES] = {")
for idx, (dx, dy) in enumerate(final_offsets):
    comma = "," if idx < 59 else ""
    print(f"    {{ {dx:3d}, {dy:3d} }}{comma} // Phase {idx}")
print("};")
