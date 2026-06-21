#!/bin/sh

echo "🔨 Recompiling framework layout with presentation snapshot hooks..."
make clean
make CFLAGS="-Wall -Wextra -O2 $(pkg-config --cflags sdl3) -DCATCLOCK_SHOT -DCATCLOCK_CHROMA"

if [ ! -f ./catclock-sdl3 ]; then
    echo "❌ Compilation failed. Aborting target angle phase sweep."
    exit 1
fi

echo "🚀 Commencing sequential 2x scaled blocking sweep sequence..."
echo "💡 Focus the window and press [Escape] to save and load the next angle step."

# FIXED: Sequences from 3 seconds to 58 seconds in explicit 5-second step increments
for second_step in $(seq 3 5 58); do
    MOCK_TIMESTAMP="2026-01-01 12:10:$(printf "%02d" $second_step)"
    OUTPUT_LABEL="phase_step_sec_$(printf "%02d" $second_step)_composite.png"
    
    echo "--------------------------------------------------------"
    echo "📸 Activating blocking 2x instance at: ${MOCK_TIMESTAMP} (-scale 3.0)"
    echo "--------------------------------------------------------"
    
    FAKETIME="${MOCK_TIMESTAMP}" ./catclock-sdl3 -scale 3.0
    
    if [ -f catclock_shot_target_raw.png ]; then
        mv catclock_shot_target_raw.png "${OUTPUT_LABEL}"
        echo "💾 3x Scale asset saved successfully to: ${OUTPUT_LABEL}"
    else
        echo "⚠️ Warning: No snapshot baseline asset captured for timestamp: ${MOCK_TIMESTAMP}"
    fi
done

montage -geometry 120x+4+4 -tile 6x2 phase_step_sec_*.png unified_subphase_grid.png

echo "✅ All chronological timeline transformation phases complete!"
