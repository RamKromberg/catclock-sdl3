#!/bin/sh
make clean
make CFLAGS="-Wall -Wextra -O2 -DCATCLOCK_DEBUG -DCATCLOCK_CHROMA"
./catclock-sdl3 -scale 2.0
W=$(identify -format "%w" catclock_live_frame_60.png)
montage -geometry "${W}x+4+4" -tile 10x7 $(ls catclock_live_frame_*.png | sort -V) unified_live_grid.png
rm catclock_live_frame_*.png
echo 'magick compare ../catclock-sdl3-backup/unified_live_grid-reference.png unified_live_grid.png -metric AE -fuzz 5% diff_overlay.png'
