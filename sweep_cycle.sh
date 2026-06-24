#!/bin/sh
# === FULL RESTORATION OF sweep_cycle.sh WITH EXPLICIT PROCESS TRACKING ===

echo "[Sweep] Generating Frame Matrix via Paced Continuous Sweeper..."
rm -f catclock_live_frame_*.ppm catclock_live_frame_*.png catclock_cycle_montage.png

make clean > /dev/null
make CFLAGS="-Wall -Wextra -O2 -DSOKOL_GLCORE33 -DCATCLOCK_DEBUG" > /dev/null 2>&1

if [ ! -f "./catclock-sdl3" ]; then
    echo "[Error] Diagnostic build execution pass failed."
    exit 1
fi

echo "[Sweep] Spawning ticker process..."
./catclock-sdl3 &
CLOCK_PID=$!

echo "[Sweep] Waiting for process ($CLOCK_PID) to finish dumping frames natively..."
# Wait for the background process to finish writing frames 60-120 and exit cleanly
wait $CLOCK_PID

echo "[Sweep] Processing and converting frame blocks..."
for ppm_file in catclock_live_frame_*.ppm; do
	if [ -f "$ppm_file" ]; then
		png_file="${ppm_file%.ppm}.png"
		magick "$ppm_file" "$png_file"
		rm -f "$ppm_file"
	fi
done

echo "[Sweep] Generating compiled master frame layout sheet..."
magick montage catclock_live_frame_*.png -geometry +1+1 -tile 10x catclock_cycle_montage.png
rm -f catclock_live_frame_*.png

echo "[Sweep] Complete! Combined sheet: ./catclock_cycle_montage.png"
