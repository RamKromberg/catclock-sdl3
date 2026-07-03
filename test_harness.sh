#!/bin/sh
./sanitize_full_width.sh
make format
rm *.pam
rm *.png
rm *.log
make clean
CMD2PDF="'date'"
#for TEST in 1 2 3 4 5 6 7 8 9; do
#for TEST in 1 2 3 4 6 7 8 9; do
for TEST in 1; do
	make CFLAGS="-Wall -Wextra -O2 -DDEBUG_TELEMETRY_LINE -DDEBUG_TELEMETRY_TRIANGLE -DTEST_MODE=$TEST"
	for SCALE in 1.0 2.0; do
		if [[ "$TEST" -eq 1 && "$SCALE" == "1.0" ]]; then
            continue
        fi
		if [[ "$TEST" -eq 3 && "$SCALE" == "1.0" ]]; then
            continue
        fi
		if [[ "$TEST" -eq 4 && "$SCALE" == "1.0" ]]; then
            continue
        fi
		echo "Test $TEST $SCALE"
		./catclock-sdl3 --scale "$SCALE" > test_"$TEST"_"$SCALE".log 2>&1
		./dump_validation.sh
		mv dump_seconds_atlas.png test_"$TEST"_"$SCALE".png
		mv dump_seconds_atlas_validation.png test_"$TEST"_"$SCALE"_validation.png
		CMD2PDF="$CMD2PDF 'cat ./test_""$TEST""_$SCALE.log'"
	done
	rm ./catclock_hands.o
	rm ./catclock-sdl3
done

eval "
./cmd2pdf.sh test_harness.pdf $CMD2PDF \
\"grep -n -h '^.*$' ./catclock_hands.c\" \
\"git grep --no-index -hn -A 500 -F '/* ---DrawTriangleLikeMesa--- */' catclock_atlas.c\" \
\"grep -n -h '^.*$' '$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/lp_setup_context.h'\" \
\"grep -n -h '^.*$' '$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/lp_rast.h'\" \
\"grep -n -h '^.*$' '$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/lp_setup_tri.c'\" \
\"grep -n -h '^.*$' '$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/lp_rast_linear_fallback.c'\" \
\"grep -n -h '^.*$' '$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/lp_setup_vbuf.c'\" \
\"'grep -n -H -C 5 'util_iround' '$DEVSHELL_MESA_SRC/src/util/u_math.h'\" \
\"find '$DEVSHELL_MESA_SRC/src/gallium/' -name 'p_state.h' -exec grep -n -H -A 60 'struct pipe_viewport_state' {} +\" \
\"find '$DEVSHELL_MESA_SRC/src/gallium/include/pipe/' -name 'p_state.h' -exec grep -n -H -A 25 'struct pipe_viewport_state' {} +\" \
\"find '$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/' -name 'lp_context.h' -exec grep -n -H -A 40 'struct pipe_viewport_state' {} +\" \
\"find '$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/' -name 'lp_state_setup.c' -exec grep -n -H -A 80 'set_viewport' {} +\" \
\"find '$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/' -name 'lp_setup.c' -exec grep -n -H -C 20 'pixel_offset' {} +\"
"

rm *.pam
rm *.log
exit
