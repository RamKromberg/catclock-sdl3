#!/usr/bin/env python3
import sys

def get_next_pot(val):
    if val <= 1:
        return 1
    return 1 << (val - 1).bit_length()

def brute_force_atlas_packing(max_fps, cell_w, cell_h, max_aspect_ratio=4.0):
    optimal_table = {}
    
    for fps in range(1, max_fps + 1):
        total_frames = fps + 1
        best_config = None
        
        for cols in range(1, total_frames + 1):
            rows = (total_frames + cols - 1) // cols
            sheet_w = cols * cell_w
            sheet_h = rows * cell_h
            
            # Enforce max aspect ratio to avoid ultra-narrow driver line-strips
            if min_sheet := min(sheet_w, sheet_h):
                aspect_ratio = max(sheet_w, sheet_h) / min_sheet
            else:
                aspect_ratio = 0
                
            if aspect_ratio > max_aspect_ratio:
                continue
                
            pot_w = get_next_pot(sheet_w)
            pot_h = get_next_pot(sheet_h)
            hardware_area = pot_w * pot_h
            wasted_cells = (rows * cols) - total_frames
            
            config = {
                "cols": cols,
                "rows": rows,
                "canvas_w": sheet_w,
                "canvas_h": sheet_h,
                "pot_w": pot_w,
                "pot_h": pot_h,
                "hardware_vram_area": hardware_area,
                "aspect_ratio": round(aspect_ratio, 2),
                "wasted_cells": wasted_cells
            }
            
            if best_config is None:
                best_config = config
            else:
                # 1. Prioritize minimum allocated GPU VRAM bucket size
                if config["hardware_vram_area"] < best_config["hardware_vram_area"]:
                    best_config = config
                elif config["hardware_vram_area"] == best_config["hardware_vram_area"]:
                    # 2. Prioritize square-like texture shapes for texture driver caches
                    if config["aspect_ratio"] < best_config["aspect_ratio"]:
                        best_config = config
                    # 3. Minimize unmapped empty grid blocks
                    elif config["aspect_ratio"] == best_config["aspect_ratio"]:
                        if config["wasted_cells"] < best_config["wasted_cells"]:
                            best_config = config
                            
        # Absolute safety override loop if constraints are met with 0 results
        if best_config is None:
            for cols in range(1, total_frames + 1):
                rows = (total_frames + cols - 1) // cols
                sheet_w, sheet_h = cols * cell_w, rows * cell_h
                pot_w, pot_h = get_next_pot(sheet_w), get_next_pot(sheet_h)
                hardware_area = pot_w * pot_h
                config = {
                    "cols": cols, "rows": rows, "canvas_w": sheet_w, "canvas_h": sheet_h, 
                    "pot_w": pot_w, "pot_h": pot_h, "hardware_vram_area": hardware_area, 
                    "aspect_ratio": max(sheet_w, sheet_h) / min(sheet_w, sheet_h), "wasted_cells": (rows*cols)-total_frames
                }
                if best_config is None or config["hardware_vram_area"] < best_config["hardware_vram_area"] or (config["hardware_vram_area"] == best_config["hardware_vram_area"] and config["aspect_ratio"] < best_config["aspect_ratio"]):
                    best_config = config

        optimal_table[fps] = best_config
    return optimal_table

def get_ranges(optimal_table):
    ranges = []
    start_fps = 1
    current_cols = optimal_table[1]["cols"]
    
    for fps in range(2, len(optimal_table) + 1):
        if optimal_table[fps]["cols"] != current_cols:
            ranges.append((start_fps, fps - 1, current_cols))
            start_fps = fps
            current_cols = optimal_table[fps]["cols"]
    ranges.append((start_fps, len(optimal_table), current_cols))
    return ranges

def print_compressed_ranges(name, ranges):
    print(f"\n=== {name.upper()} ===")
    for s, e, c in ranges:
        print(f"FPS {s:4d} - {e:4d} -> cols = {c:2d}")

def emit_c_switch_block(var_name, ranges):
    print(f"\n// Generated C code selection for {var_name}")
    print(f"if ({var_name}_condition) {{")
    for s, e, c in ranges[:-1]:
        print(f"    if (fps <= {e}) cols = {c};")
    print(f"    else cols = {ranges[-1][2]};")
    print("}")

if __name__ == "__main__":
    max_fps_target = 1024
    GENERATE_C_CODE = False  # Toggle to True to print copy-pasteable C blocks
    
    # Process dynamic asset profiles
    eyes_raw  = brute_force_atlas_packing(max_fps_target, 64, 32)
    tail_raw  = brute_force_atlas_packing(max_fps_target, 96, 96)
    hands_raw = brute_force_atlas_packing(max_fps_target, 64, 96)
    
    # Compress into ranges
    eyes_ranges  = get_ranges(eyes_raw)
    tail_ranges  = get_ranges(tail_raw)
    hands_ranges = get_ranges(hands_raw)
    
    # Print human-readable diagnostics
    print_compressed_ranges("Eyes (Pupils) Layer (64x32)", eyes_ranges)
    print_compressed_ranges("Tail Body Layer (96x96)", tail_ranges)
    print_compressed_ranges("Hands Layer (64x96)", hands_ranges)
    
    if GENERATE_C_CODE:
        print("\n" + "="*40 + "\nC CODE GENERATOR OUTPUT\n" + "="*40)
        emit_c_switch_block("eyes", eyes_ranges)
        emit_c_switch_block("tail", tail_ranges)
        emit_c_switch_block("hands", hands_ranges)
