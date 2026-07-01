#!/bin/sh
# === FILE: dump_validation.sh ===
set -e
TMP_DIR=$(mktemp -d -t catclock_validate_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

# ==============================================================================
# BOILERPLATE INITIALIZATION (ALWAYS FIRST)
# ==============================================================================
magick -size 1x1 xc:red "$TMP_DIR/color_anchor.png"

for f in *.pam; do
magick "$f" "$TMP_DIR/color_anchor.png" +append -crop 100%x100%+0+0 +repage -gravity East -chop 1x0 "${f%.pam}.png"
done
rm -f *.pam

# ==============================================================================
# EVALUATE GEOMETRY COEFFICIENTS DYNAMICALLY (STAGE 2)
# ==============================================================================
LIVE_W=$(identify -format "%w" dump_hours_atlas.png)
SCALE_FACTOR=$(echo "scale=4; $LIVE_W / 640" | bc)

CELL_W=$(echo "64 * $SCALE_FACTOR" | bc | cut -d. -f1)
CELL_H=$(echo "96 * $SCALE_FACTOR" | bc | cut -d. -f1)
C_W_MINUS_1=$(echo "$CELL_W - 1" | bc)
C_H_MINUS_1=$(echo "$CELL_H - 1" | bc)

STROKE_W=$(echo "$SCALE_FACTOR / 1" | bc | cut -d. -f1)
if [ "$STROKE_W" -lt 1 ]; then
    STROKE_W=1
fi

# Dynamically scale original clockface vertical adjustment properties
GEOM_Y=$(echo "-2 * $SCALE_FACTOR" | bc | cut -d. -f1)

# ==============================================================================
# PIPELINE BRANCHING: SEPARATE WHOLE NUMBERS (1,2,3) FROM HALVES (0.5,1.5,2.5)
# ==============================================================================
IS_FRACTIONAL=$(echo "$SCALE_FACTOR" | grep "\.[1-9]" | grep -v "\.0" || true)

if [ -n "$IS_FRACTIONAL" ]; then
    # PATH A: Fractional Half-Steps Math (0.5, 1.5, 2.5...)
    X_START=$(echo "31 * $SCALE_FACTOR" | bc | cut -d. -f1)
    Y_START=$(echo "(45 * $SCALE_FACTOR) + ($GEOM_Y)" | bc | cut -d. -f1)
else
    # PATH B: Whole Integer Scale Steps Math (1, 2, 3...)
    # Shifting coordinates right/down by 1px to resolve the integer off-by-one
    X_START=$(echo "(31 * $SCALE_FACTOR) + 1" | bc | cut -d. -f1)
    Y_START=$(echo "(45 * $SCALE_FACTOR) + ($GEOM_Y) + 1" | bc | cut -d. -f1)
fi

X_END=$(echo "$X_START + $STROKE_W - 1" | bc)
Y_END=$(echo "$Y_START + $STROKE_W - 1" | bc)

RECT_X1=$(echo "2 * $SCALE_FACTOR" | bc | cut -d. -f1)
RECT_Y1=$(echo "6 * $SCALE_FACTOR" | bc | cut -d. -f1)
RECT_X2=$(echo "60 * $SCALE_FACTOR" | bc | cut -d. -f1)
RECT_Y2=$(echo "88 * $SCALE_FACTOR" | bc | cut -d. -f1)

FACE_CROP_W=$(echo "59 * $SCALE_FACTOR" | bc | cut -d. -f1)
FACE_CROP_H=$(echo "83 * $SCALE_FACTOR" | bc | cut -d. -f1)
FACE_X=$(echo "20 * $SCALE_FACTOR" | bc | cut -d. -f1)
FACE_Y=$(echo "100 * $SCALE_FACTOR" | bc | cut -d. -f1)

P1_CROP_W=$(echo "640 * $SCALE_FACTOR" | bc | cut -d. -f1)
P1_CROP_H=$(echo "576 * $SCALE_FACTOR" | bc | cut -d. -f1)

P2_CELL_W=$(echo "96 * $SCALE_FACTOR" | bc | cut -d. -f1)
P2_CELL_H=$(echo "96 * $SCALE_FACTOR" | bc | cut -d. -f1)
P2_W_MINUS_1=$(echo "$P2_CELL_W - 1" | bc)
P2_H_MINUS_1=$(echo "$P2_CELL_H - 1" | bc)

P2_LINE1_START=$(echo "39 * $SCALE_FACTOR" | bc | cut -d. -f1)
P2_LINE1_END=$(echo "$P2_LINE1_START + $STROKE_W - 1" | bc)

P2_LINE2_START=$(echo "52 * $SCALE_FACTOR" | bc | cut -d. -f1)
P2_LINE2_END=$(echo "$P2_LINE2_START + $STROKE_W - 1" | bc)

VIEW_W=$(echo "150 * $SCALE_FACTOR" | bc | cut -d. -f1)
VIEW_H=$(echo "300 * $SCALE_FACTOR" | bc | cut -d. -f1)

BODY_CROP_W=$(echo "101 * $SCALE_FACTOR" | bc | cut -d. -f1)
BODY_CROP_H=$(echo "201 * $SCALE_FACTOR" | bc | cut -d. -f1)
BODY_X=$(echo "24 * $SCALE_FACTOR" | bc | cut -d. -f1)
BODY_Y=$(echo "12 * $SCALE_FACTOR" | bc | cut -d. -f1)

TAIL_CROP_W=$(echo "96 * $SCALE_FACTOR" | bc | cut -d. -f1)
TAIL_CROP_H=$(echo "96 * $SCALE_FACTOR" | bc | cut -d. -f1)
TAIL_X=$(echo "27 * $SCALE_FACTOR" | bc | cut -d. -f1)
TAIL_Y=$(echo "203 * $SCALE_FACTOR" | bc | cut -d. -f1)

# ==============================================================================
# PIPELINE 1: CLOCK FACE EXTRACTION PASS
# ==============================================================================
magick dump_material_composition.png "$TMP_DIR/color_anchor.png" +append \
-crop ${FACE_CROP_W}x${FACE_CROP_H}+${FACE_X}+${FACE_Y} +repage "$TMP_DIR/color_anchor.png" +append \
-crop ${FACE_CROP_W}x${FACE_CROP_H}+0+0 +repage "$TMP_DIR/tmp_face.png"

# EXCLUSIVE ISOLATED DIAGNOSTIC SHEET: Hands-free view using partitioned scale math
magick -size ${CELL_W}x${CELL_H} xc:none \
"$TMP_DIR/tmp_face.png" -gravity center -geometry +0${GEOM_Y} -compose Over -composite \
-stroke blue -strokewidth 1 -fill none -draw "rectangle 0,0 ${C_W_MINUS_1},${C_H_MINUS_1}" \
-stroke none -fill green -draw "rectangle ${X_START},0 ${X_END},${C_H_MINUS_1}" -draw "rectangle 0,${Y_START} ${C_W_MINUS_1},${Y_END}" \
dump_face_only_validation.png

# ==============================================================================
# PIPELINE 1 CONTINUED: CLOCK HAND VALIDATION LOOP
# ==============================================================================
for hand in hours minutes seconds; do
magick dump_${hand}_atlas.png "$TMP_DIR/color_anchor.png" +append \
\( -size ${CELL_W}x${CELL_H} xc:none \
-stroke blue -strokewidth 1 -fill none -draw "rectangle 0,0 ${C_W_MINUS_1},${C_H_MINUS_1}" \
-stroke yellow -strokewidth 1 -draw "rectangle ${RECT_X1},${RECT_Y1} ${RECT_X2},${RECT_Y2}" \
-stroke none -fill green -draw "rectangle ${X_START},0 ${X_END},${C_H_MINUS_1}" -draw "rectangle 0,${Y_START} ${C_W_MINUS_1},${Y_END}" \
-write mpr:mygrid +delete \) \
\( +clone -tile mpr:mygrid -draw "color 0,0 reset" \) \
-compose Dst_Over -composite \
-crop ${P1_CROP_W}x${P1_CROP_H}+0+0 +repage dump_${hand}_atlas_validation.png

magick dump_${hand}_atlas.png "$TMP_DIR/color_anchor.png" +append \
\( -size ${CELL_W}x${CELL_H} xc:none \
"$TMP_DIR/tmp_face.png" -gravity center -geometry +0${GEOM_Y} -compose Over -composite \
-stroke blue -strokewidth 1 -fill none -draw "rectangle 0,0 ${C_W_MINUS_1},${C_H_MINUS_1}" \
-stroke none -fill green -draw "rectangle ${X_START},0 ${X_END},${C_H_MINUS_1}" -draw "rectangle 0,${Y_START} ${C_W_MINUS_1},${Y_END}" \
-write mpr:compgrid +delete \) \
\( +clone -tile mpr:compgrid -draw "color 0,0 reset" \) \
-compose Dst_Over -composite \
-crop ${P1_CROP_W}x${P1_CROP_H}+0+0 +repage dump_${hand}_composition_validation.png
done

# ==============================================================================
# PIPELINE 2: TAIL VALIDATION PASS
# ==============================================================================
magick dump_tail_body_atlas.png "$TMP_DIR/color_anchor.png" +append \
\( -size ${P2_CELL_W}x${P2_CELL_H} xc:none \
-stroke blue -strokewidth 1 -fill none -draw "rectangle 0,0 ${P2_W_MINUS_1},${P2_H_MINUS_1}" \
-stroke none -fill green -draw "rectangle ${P2_LINE1_START},0 ${P2_LINE1_END},${P2_H_MINUS_1}" -draw "rectangle ${P2_LINE2_START},0 ${P2_LINE2_END},${P2_H_MINUS_1}" \
-write mpr:tailgrid +delete \) \
\( +clone -tile mpr:tailgrid -draw "color 0,0 reset" \) \
-compose Dst_Over -composite \
-crop ${P2_CROP_W}x${P2_CROP_H}+0+0 +repage dump_tail_atlas_validation.png

# ==============================================================================
# PIPELINE 3: VIEWPORT COMPOSITION EMULATION
# ==============================================================================
echo "[+] Emulating transparent ${VIEW_W}x${VIEW_H} viewports with parameterized constraints..."

magick dump_material_composition.png "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/anchored_body.png"
magick dump_tail_body_atlas.png "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/anchored_atlas.png"

for idx in $(seq 0 59); do
X_OFF=$(( (idx % 10) * P2_CELL_W ))
Y_OFF=$(( (idx / 10) * P2_CELL_H ))
PAD_IDX=$(printf "%02d" "$idx")

magick "$TMP_DIR/anchored_atlas.png" -crop ${P2_CELL_W}x${P2_CELL_H}+${X_OFF}+${Y_OFF} +repage \
"${TMP_DIR}/color_anchor.png" +append "${TMP_DIR}/tail_${PAD_IDX}.png"

magick -size ${VIEW_W}x${VIEW_H} xc:none \
-stroke blue -strokewidth 1 -fill none -draw "rectangle 0,0 $((VIEW_W-1)),$((VIEW_H-1))" \
-stroke blue -strokewidth 1 -fill none -draw "rectangle ${TAIL_X},${TAIL_Y} $((TAIL_X + TAIL_CROP_W - 1)),$((TAIL_Y + TAIL_CROP_H - 1))" \
\( "$TMP_DIR/tail_${PAD_IDX}.png" -crop ${TAIL_CROP_W}x${TAIL_CROP_H}+0+0 +repage \) -geometry +${TAIL_X}+${TAIL_Y} -compose Over -composite \
\( "$TMP_DIR/anchored_body.png" -crop ${BODY_CROP_W}x${BODY_CROP_H}+0+0 +repage \) -geometry +${BODY_X}+${BODY_Y} -compose Over -composite \
"$TMP_DIR/color_anchor.png" +append \
"$TMP_DIR/view_${PAD_IDX}.png"
done

for row in $(seq 0 5); do
ROW_FILES=""
for col in $(seq 0 9); do
idx=$(( (row * 10) + col ))
PAD_IDX=$(printf "%02d" "$idx")
ROW_FILES="${ROW_FILES} $TMP_DIR/view_${PAD_IDX}.png"
done
magick $ROW_FILES -background none +append "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/row_${row}.png"
done

magick "$TMP_DIR/row_0.png" "$TMP_DIR/row_1.png" "$TMP_DIR/row_2.png" \
"$TMP_DIR/row_3.png" "$TMP_DIR/row_4.png" "$TMP_DIR/row_5.png" \
-background none -append "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/tmp_composite.png"

FINAL_TOTAL_W=$(( VIEW_W * 10 ))
FINAL_TOTAL_H=$(( VIEW_H * 6 ))
magick "$TMP_DIR/tmp_composite.png" -crop ${FINAL_TOTAL_W}x${FINAL_TOTAL_H}+0+0 +repage \
"$TMP_DIR/color_anchor.png" +append -crop ${FINAL_TOTAL_W}x${FINAL_TOTAL_H}+0+0 +repage \
dump_tail_composition_validation.png
echo "[+] Validation generation complete!"
