#!/bin/sh
# === FULL RESTORATION OF sweep_phases.sh USING FAKETIME ===

rm -rf phases_snapshot_out
mkdir -p phases_snapshot_out

echo "[Sweep] Recompiling runtime binary once..."
make clean > /dev/null
make CFLAGS="-Wall -Wextra -O2 -DSOKOL_GLCORE33 -DCATCLOCK_SHOT" > /dev/null 2>&1

if [ ! -f "./catclock-sdl3" ]; then
    echo "[Error] Build failed"
    exit 1
fi

echo "[Sweep] Executing side-by-side symmetry validation loops..."
for i in $(seq 1 29); do
	left_phase=$i
	right_phase=$(( (60 - i) % 60 ))
	
	echo "[Sweep] Sampling phase $left_phase vs $right_phase via faketime..."
	
	# Execute Left Phase via faketime injection (e.g. 12:00:15)
	faketime -f "2026-01-01 12:00:$(printf "%02d" $left_phase)" ./catclock-sdl3 > /dev/null 2>&1
	mv catclock_shot_target_raw.ppm left.ppm
	
	# Execute Right Phase via faketime injection (e.g. 12:00:45)
	faketime -f "2026-01-01 12:00:$(printf "%02d" $right_phase)" ./catclock-sdl3 > /dev/null 2>&1
	mv catclock_shot_target_raw.ppm right.ppm
	
	out_png="phases_snapshot_out/pair_$(printf "%02d" $left_phase)_vs_$(printf "%02d" $right_phase).png"
	magick left.ppm right.ppm +append "$out_png"
	rm -f left.ppm right.ppm
done

echo "[Sweep] Packaging master layout tiles..."
magick montage phases_snapshot_out/pair_*.png -geometry +2+2 -tile 5x6 phases_master_montage.png

echo "[Sweep] Complete! Review sheet: ./phases_master_montage.png"
