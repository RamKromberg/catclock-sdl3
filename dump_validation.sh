#!/bin/sh
# === FILE: dump_validation.sh ===
set -e
TMP_DIR=$(mktemp -d -t catclock_validate_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

# ================================================================================
# BOILERPLATE INITIALIZATION (ALWAYS FIRST)
# ================================================================================
magick -size 1x1 xc:red "$TMP_DIR/color_anchor.png"

for f in *.pam; do
    magick "$f" "$TMP_DIR/color_anchor.png" +append -crop 100%x100%+0+0 +repage -gravity East -chop 1x0 "${f%.pam}.png"
done
rm -f *.pam

# ================================================================================
# EVALUATE GEOMETRY COEFFICIENTS DYNAMICALLY (STAGE 2)
# ================================================================================
LIVE_W=$(identify -format "%w" dump_hours_atlas.png)
SCALE_FACTOR=$(echo "scale=4; $LIVE_W / 640" | bc)

CELL_W=$(echo "64 * $SCALE_FACTOR" | bc | cut -d. -f1)
CELL_H=$(echo "96 * $SCALE_FACTOR" | bc | cut -d. -f1)
C_W_MINUS_1=$((CELL_W - 1))
C_H_MINUS_1=$((CELL_H - 1))

STROKE_W=$(echo "$SCALE_FACTOR / 1" | bc | cut -d. -f1)
if [ "$STROKE_W" -lt 1 ]; then
    STROKE_W=1
fi

IS_FRACTIONAL=$(echo "$SCALE_FACTOR" | grep "\." || true)

if [ -n "$IS_FRACTIONAL" ]
then
    # PATH A: Fractional Half-Steps Math
    X_START=$(echo "31 * $SCALE_FACTOR" | bc | cut -d. -f1)
    Y_START=$(echo "45 * $SCALE_FACTOR" | bc | cut -d. -f1)
else
    # PATH B: Whole Integer Scale Steps
    X_START=$(echo "31 * $SCALE_FACTOR" | bc | cut -d. -f1)
    Y_START=$(echo "45 * $SCALE_FACTOR" | bc | cut -d. -f1)
fi

X_END=$((X_START + STROKE_W - 1))
Y_END=$((Y_START + STROKE_W - 1))

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
P3_CELL_H=$(echo "96 * $SCALE_FACTOR" | bc | cut -d. -f1)
P2_W_MINUS_1=$((P2_CELL_W - 1))
P2_H_MINUS_1=$((P3_CELL_H - 1))

P2_LINE1_START=$(echo "40 * $SCALE_FACTOR" | bc | cut -d. -f1)
P2_LINE1_END=$((P2_LINE1_START + STROKE_W - 1))
P2_LINE2_START=$(echo "52 * $SCALE_FACTOR" | bc | cut -d. -f1)
P2_LINE2_END=$((P2_LINE2_START + STROKE_W - 1))

VIEW_W=$(echo "150 * $SCALE_FACTOR" | bc | cut -d. -f1)
VIEW_H=$(echo "300 * $SCALE_FACTOR" | bc | cut -d. -f1)

BODY_CROP_W=$(echo "101 * $SCALE_FACTOR" | bc | cut -d. -f1)
BODY_CROP_H=$(echo "201 * $SCALE_FACTOR" | bc | cut -d. -f1)
BODY_X=$(echo "24 * $SCALE_FACTOR" | bc | cut -d. -f1)
BODY_Y=$(echo "12 * $SCALE_FACTOR" | bc | cut -d. -f1)

TAIL_CROP_W=$(echo "96 * $SCALE_FACTOR" | bc | cut -d. -f1)
TAIL_CROP_H=$(echo "96 * $SCALE_FACTOR" | bc | cut -d. -f1)
TAIL_X=$(echo "27 * $SCALE_FACTOR" | bc | cut -d. -f1)
TAIL_Y=$(echo "204 * $SCALE_FACTOR" | bc | cut -d. -f1)

# --------------------------------------------------------------------------------
# UNIVERSAL STEP-PATTERN RASTER CORRECTION LAYER (1x TO 10x)
# --------------------------------------------------------------------------------
calculate_geometry_offsets() {
    REAL_CELL_W=$(echo "64 * $SCALE_FACTOR" | bc | cut -d. -f1)
    REAL_CELL_H=$(echo "96 * $SCALE_FACTOR" | bc | cut -d. -f1)

    REAL_FACE_W=$(identify -format "%w" "$TMP_DIR/tmp_face.png")
    REAL_FACE_H=$(identify -format "%h" "$TMP_DIR/tmp_face.png")

    IM_LEFT=$(( (REAL_CELL_W - REAL_FACE_W) / 2 ))
    IM_TOP=$(( (REAL_CELL_H - REAL_FACE_H) / 2 ))

    TARGET_X=$(echo "31 * $SCALE_FACTOR" | bc | cut -d. -f1)
    TARGET_Y=$(echo "45 * $SCALE_FACTOR" | bc | cut -d. -f1)

    LOCAL_FOCAL_X=$(( (REAL_FACE_W * 29) / 59 ))
    LOCAL_FOCAL_Y=$(( (REAL_FACE_H * 39) / 83 ))

    OFF_X=$(( TARGET_X - IM_LEFT - LOCAL_FOCAL_X ))
    OFF_Y=$(( TARGET_Y - IM_TOP - LOCAL_FOCAL_Y ))

    if [ "$OFF_X" -ge 0 ]; then GEOM_X="+$OFF_X"; else GEOM_X="$OFF_X"; fi
    if [ "$OFF_Y" -ge 0 ]; then GEOM_Y="+$OFF_Y"; else GEOM_Y="$OFF_Y"; fi

    # 🎯 OUTER YELLOW FRAME BOUNDARIES (Snaps exactly to the asset frame edges)
    RECT_X1=$(echo "2 * $SCALE_FACTOR" | bc | cut -d. -f1)
    RECT_Y1=$(echo "6 * $SCALE_FACTOR" | bc | cut -d. -f1)

    SCALED_SPAN_W=$(echo "59 * $SCALE_FACTOR" | bc | cut -d. -f1)
    SCALED_SPAN_H=$(echo "83 * $SCALE_FACTOR" | bc | cut -d. -f1)

    RECT_X2=$(( RECT_X1 + SCALED_SPAN_W - 1 ))
    RECT_Y2=$(( RECT_Y1 + SCALED_SPAN_H - 1 ))

    # ✂️ INNER CUTOUT BOUNDARIES (Pushed inward by exactly STROKE_W pixels)
    INNER_X1=$(( RECT_X1 + STROKE_W ))
    INNER_Y1=$(( RECT_Y1 + STROKE_W ))
    INNER_X2=$(( RECT_X2 - STROKE_W ))
    INNER_Y2=$(( RECT_Y2 - STROKE_W ))
}

# ================================================================================
# PIPELINE 1: CLOCK FACE EXTRACTION PASS
# ================================================================================
magick dump_material_composition.png "$TMP_DIR/color_anchor.png" +append \
    -crop ${FACE_CROP_W}x${FACE_CROP_H}+${FACE_X}+${FACE_Y} +repage \
    "$TMP_DIR/color_anchor.png" +append \
    -crop ${FACE_CROP_W}x${FACE_CROP_H}+0+0 +repage "$TMP_DIR/tmp_face.png"

calculate_geometry_offsets

# EXCLUSIVE BLITTED DIAGNOSTIC SHEET: Hands-free using partitioned scale math
magick -size ${CELL_W}x${CELL_H} xc:none "$TMP_DIR/tmp_face.png" \
    -gravity center -geometry ${GEOM_X}${GEOM_Y} -compose Over -composite \
    +antialias -stroke blue -strokewidth ${STROKE_W} -fill none -draw "rectangle 0,0 ${C_W_MINUS_1},${C_H_MINUS_1}" \
    -gravity Forget -geometry +0+0 -compose Over \
    -stroke none -fill "#FFFF00BF" \
    -draw "rectangle ${RECT_X1},${RECT_Y1} ${RECT_X2},${INNER_Y1}" \
    -draw "rectangle ${RECT_X1},${INNER_Y2} ${RECT_X2},${RECT_Y2}" \
    -draw "rectangle ${RECT_X1},${INNER_Y1} ${INNER_X1},${INNER_Y2}" \
    -draw "rectangle ${INNER_X2},${INNER_Y1} ${RECT_X2},${INNER_Y2}" \
    -stroke none -fill "#008000BF" -draw "rectangle ${X_START},0 ${X_END},${C_H_MINUS_1}" -draw "rectangle 0,${Y_START} ${C_W_MINUS_1},${Y_END}" \
    dump_face_only_validation.png

# ================================================================================
# PIPELINE 1 CONTINUED: CLOCK HAND VALIDATION LOOP
# ================================================================================
for hand in hours minutes seconds; do
    magick dump_${hand}_atlas.png "$TMP_DIR/color_anchor.png" +append \
        \( -size ${CELL_W}x${CELL_H} xc:none \
        +antialias -stroke blue -strokewidth ${STROKE_W} -fill none -draw "rectangle 0,0 ${C_W_MINUS_1},${C_H_MINUS_1}" \
        -gravity Forget -geometry +0+0 -compose Over \
        -stroke none -fill "#FFFF00BF" \
        -draw "rectangle ${RECT_X1},${RECT_Y1} ${RECT_X2},${INNER_Y1}" \
        -draw "rectangle ${RECT_X1},${INNER_Y2} ${RECT_X2},${RECT_Y2}" \
        -draw "rectangle ${RECT_X1},${INNER_Y1} ${INNER_X1},${INNER_Y2}" \
        -draw "rectangle ${INNER_X2},${INNER_Y1} ${RECT_X2},${INNER_Y2}" \
        -stroke none -fill "#008000BF" -draw "rectangle ${X_START},0 ${X_END},${C_H_MINUS_1}" -draw "rectangle 0,${Y_START} ${C_W_MINUS_1},${Y_END}" \
        -write mpr:mygrid +delete \) \
        \( +clone -tile mpr:mygrid -draw "color 0,0 reset" \) \
        -compose Dst_Over -composite \
        -crop ${P1_CROP_W}x${P1_CROP_H}+0+0 +repage dump_${hand}_atlas_validation.png

    magick dump_${hand}_atlas.png "$TMP_DIR/color_anchor.png" +append \
        \( -size ${CELL_W}x${CELL_H} xc:none \
        "$TMP_DIR/tmp_face.png" -gravity center -geometry ${GEOM_X}${GEOM_Y} -compose Over -composite \
        +antialias -stroke blue -strokewidth ${STROKE_W} -fill none -draw "rectangle 0,0 ${C_W_MINUS_1},${C_H_MINUS_1}" \
        -gravity Forget -geometry +0+0 -compose Over \
        -stroke none -fill "#FFFF00BF" \
        -draw "rectangle ${RECT_X1},${RECT_Y1} ${RECT_X2},${INNER_Y1}" \
        -draw "rectangle ${RECT_X1},${INNER_Y2} ${RECT_X2},${RECT_Y2}" \
        -draw "rectangle ${RECT_X1},${INNER_Y1} ${INNER_X1},${INNER_Y2}" \
        -draw "rectangle ${INNER_X2},${INNER_Y1} ${RECT_X2},${INNER_Y2}" \
        -stroke none -fill "#008000BF" -draw "rectangle ${X_START},0 ${X_END},${C_H_MINUS_1}" -draw "rectangle 0,${Y_START} ${C_W_MINUS_1},${Y_END}" \
        -write mpr:compgrid +delete \) \
        \( +clone -tile mpr:compgrid -draw "color 0,0 reset" \) \
        -compose Dst_Over -composite \
        -crop ${P1_CROP_W}x${P1_CROP_H}+0+0 +repage dump_${hand}_composition_validation.png
done

# ================================================================================
# PIPELINE 2: TAIL VALIDATION PASS
# ================================================================================
magick dump_tail_body_atlas.png "$TMP_DIR/color_anchor.png" +append \
    \( -size ${P2_CELL_W}x${CELL_H} xc:none \
    +antialias -stroke blue -strokewidth ${STROKE_W} -fill none -draw "rectangle 0,0 ${P2_W_MINUS_1},${C_H_MINUS_1}" \
    -gravity Forget -geometry +0+0 -compose Over \
    -stroke none -fill "#00800080" \
    -draw "rectangle ${P2_LINE1_START},0 ${P2_LINE1_END},${CELL_H}" \
    -draw "rectangle ${P2_LINE2_START},0 ${P2_LINE2_END},${CELL_H}" \
    -write mpr:tailgrid +delete \) \
    \( +clone -tile mpr:tailgrid -draw "color 0,0 reset" \) \
    -compose Over -composite \
    -crop ${P2_CROP_W}x${P2_CROP_H}+0+0 +repage dump_tail_atlas_validation.png

# ================================================================================
# PIPELINE 3: VIEWPORT COMPOSITION EMULATION
# ================================================================================
echo "[+] Emulating transparent ${VIEW_W}x${VIEW_H} viewports with parameterized constraints..."

magick dump_material_composition.png "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/anchored_body.png"
magick dump_tail_atlas_validation.png "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/anchored_atlas.png"

for idx in $(seq 0 59); do
    X_OFF=$(( (idx % 10) * P2_CELL_W ))
    Y_OFF=$(( (idx / 10) * P3_CELL_H ))
    PAD_IDX=$(printf "%02d" "$idx")

    magick "$TMP_DIR/anchored_atlas.png" -crop ${P2_CELL_W}x${P3_CELL_H}+${X_OFF}+${Y_OFF} +repage \
        "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/tail_${PAD_IDX}.png"

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
