#!/bin/sh
# ============================================================================
# Project: catclock-sdl3 (Hitbox Extraction Blueprint)
# Purpose: Compiles a pixel-perfect aggregate click-mask from animation frames
# Syntax Format: POSIX Compliant Shell
# ============================================================================

# Define file naming schemas
OUTPUT_XBM="hitbox.xbm"
TEMP_COMPOSITE="temp_flattened_hitbox.png"

# Verify that our sequence directory contains valid frame data fields
first_frame=$(ls frame_cycle_*.pam 2>/dev/null | head -n 1)
if [ -z "$first_frame" ]; then
    echo "[Hitbox Tool Error] No matching sequence file tracks found (frame_cycle_*.pam)." >&2
    exit 1
fi

echo "[Hitbox Tool] Parsing frame arrays for composite tracking analysis..."

# 1. FLATTEN VIA MAXIMUM EVALUATION CHANNEL:
# Compiles all sequential frames using peak channel values to lock down the 
# complete spatial footprint of the moving components.
magick frame_cycle_*.pam -evaluate-sequence Max "$TEMP_COMPOSITE"

if [ ! -f "$TEMP_COMPOSITE" ]; then
    echo "[Hitbox Tool Error] Failed to generate intermediate composite layer." >&2
    exit 1
fi

# 2. THRESHOLD, INVERT, AND FORMAT AS XBM:
# - Extract the alpha transparency mask shape.
# - Apply the '-negate' command to invert the bitonal value mappings.
# - Apply strict bitonal clipping via thresholding to create flat matrix data.
magick "$TEMP_COMPOSITE" -alpha extract -negate -colorspace Gray -threshold 1 "$OUTPUT_XBM"

# Clean up intermediate tracking data
rm -f "$TEMP_COMPOSITE"

if [ -f "$OUTPUT_XBM" ]; then
    echo "[Hitbox Tool Trace] Precision inverted click asset generated successfully: $OUTPUT_XBM"
    
    # Optional verification dump: Display generated canvas metrics
    width=$(grep -m 1 "_width" "$OUTPUT_XBM" | awk '{print $NF}')
    height=$(grep -m 1 "_height" "$OUTPUT_XBM" | awk '{print $NF}')
    echo " -> Native Array Strides: ${width}px Wide x ${height}px Tall."
else
    echo "[Hitbox Tool Error] Graphic format backend translation failed." >&2
    exit 1
fi
