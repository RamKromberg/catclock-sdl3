#!/usr/bin/env bash
# High-Precision Cross-Axial Symmetry Diagnostic Sheet Sweeper

set -euo pipefail

echo "⚙️ Recompiling framework layout with presentation snapshot hooks..."
make clean
make CFLAGS="-Wall -Wextra -O2 $(pkg-config --cflags sdl3) -DCATCLOCK_SHOT -DCATCLOCK_CHROMA"

if [ ! -f ./catclock-sdl3 ]; then
    echo "❌ Compilation failed. Aborting target angle phase sweep."
    exit 1
fi

echo "扫 Clearing legacy phase intermediate snapshots..."
rm -f phase_step_sec_*_composite.png unified_subphase_grid.png

# 📊 24-FRAME CROSS-AXIAL SYMMETRY PAIRS GRID
# Pairs are explicitly sequenced to compare matching angles across all 4 quadrants:
# Rows 1-2:   Horizontal pairs near top (02 vs 58) and (03 vs 57)
# Rows 3-4:   Horizontal pairs near bottom (27 vs 33) and (28 vs 32)
# Rows 5-6:   Vertical pairs near upper-right (07 vs 23) and (08 vs 22)
# Rows 7-8:   Vertical pairs near upper-left (53 vs 37) and (52 vs 38)
# Rows 9-10:  Diagonal mirror test quadrant 1 vs 3 (12 vs 42) and (13 vs 43)
# Rows 11-12: Diagonal mirror test quadrant 2 vs 4 (17 vs 47) and (18 vs 48)
declare -a MIRROR_STEPS=(
    "02" "58" "03" "57" 
    "27" "33" "28" "32" 
    "07" "23" "08" "22" 
    "53" "37" "52" "38"
    "12" "42" "13" "43"
    "17" "47" "18" "48"
)

echo "🚀 Commencing unthrottled live cross-axial animation capture loop..."

# Keep track of file indexing prefix to enforce exact side-by-side order in montage output
COUNTER=100
for step in "${MIRROR_STEPS[@]}"; do
    MOCK_TIMESTAMP="2026-01-01 12:00:${step}"
    OUTPUT_LABEL="phase_step_sec_${COUNTER}_${step}_composite.png"
    
    FAKETIME="${MOCK_TIMESTAMP}" ./catclock-sdl3 -scale 3.0
    
    if [ -f catclock_shot_target_raw.png ]; then
        mv catclock_shot_target_raw.png "${OUTPUT_LABEL}"
        echo "💾 Captured tick [${step}] into segment location slot: ${COUNTER}"
    else
        echo "⚠️ Warning: Frame drop at timestamp: ${MOCK_TIMESTAMP}"
    fi
    COUNTER=$((COUNTER + 1))
done

echo "📊 Compiling paired cross-axial tiles at 1:1 raw integer pixel sizes..."
# CRITICAL FIX: Specifying '+0+0' with empty width preserves uncompressed, pixel-perfect scaling
montage -geometry +0+0 -tile 2x12 $(ls phase_step_sec_*_composite.png | sort -V) unified_subphase_grid.png

echo "🧼 Purging localized workspace temporary intermediate frame snapshots..."
rm -f phase_step_sec_*_composite.png

echo "✅ Success! Pixel-accurate 24-frame symmetry sheet generated at: unified_subphase_grid.png"
