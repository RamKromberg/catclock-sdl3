#!/bin/sh
set -e
TMP_DIR=$(mktemp -d -t catclock_validate_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

# ================================================================================
# BOILERPLATE INITIALIZATION (ALWAYS FIRST)
# ================================================================================
magick -size 1x1 xc:red "$TMP_DIR/color_anchor.png"

# Check if there are any .pam files to process to prevent open errors
if ls *.pam >/dev/null 2>&1; then
    for f in *.pam; do
        # Safely check if the image has any visible pixels without triggering scientific notation
        is_empty=$(magick "$f" -alpha extract -threshold 0 -format "%[fx:mean==0?1:0]" info:)

        if [ "$is_empty" -eq 1 ]; then
            # Delete the empty file immediately
            echo "$f" is empty. Deleting...
            rm "$f"
        else
            # Process the file normally if it contains visible pixels
            magick "$f" "$TMP_DIR/color_anchor.png" +append -crop 100%x100%+0+0 +repage -gravity East -chop 1x0 "${f%.pam}.png"
        fi
    done
    rm -f *.pam
else
    echo "No .pam files found. Skipping initialization loop."
fi

# ================================================================================
# EVALUATE GEOMETRY COEFFICIENTS DYNAMICALLY (STAGE 2)
# ================================================================================
# Dynamically locate any available clock hand atlas to determine layout and scale
HANDS_SAMPLE_FILE=$(ls dump_seconds_atlas_*.png dump_minutes_atlas_*.png dump_hours_atlas_*.png 2>/dev/null | head -n 1)

if [[ -n "$HANDS_SAMPLE_FILE" ]]; then
    # Grab dimension raw
    RAW_W=$(identify -format "%w" "$HANDS_SAMPLE_FILE")
    # Convert immediately to integer to strip any scientific notation or decimals
    LIVE_W=$(printf "%.0f" "$RAW_W" 2>/dev/null || echo "$RAW_W")

    HANDS_SAMPLE_GRID=$(echo "$HANDS_SAMPLE_FILE" | sed -E 's/.*_([0-9]+x[0-9]+)\.png/\1/')
    HANDS_SAMPLE_COLS=$(echo "$HANDS_SAMPLE_GRID" | cut -dx -f2)

    UNSCALED_W=$(( HANDS_SAMPLE_COLS * 64 ))

    # Calculate scale factor
    RAW_SCALE=$(echo "scale=4; $LIVE_W / $UNSCALED_W" | bc)
    # Ensure scale factor doesn't print as scientific notation (keep it standard decimal)
    SCALE_FACTOR=$(printf "%.4f" "$RAW_SCALE" 2>/dev/null || echo "$RAW_SCALE")
else
    # Fallback to check if legacy un-suffixed files exist
    if [ -f dump_seconds_atlas.png ]; then
        LIVE_W=$(identify -format "%w" dump_seconds_atlas.png)
    elif [ -f dump_minutes_atlas.png ]; then
        LIVE_W=$(identify -format "%w" dump_minutes_atlas.png)
    elif [ -f dump_hours_atlas.png ]; then
        LIVE_W=$(identify -format "%w" dump_hours_atlas.png)
    fi
    if [[ -n "$LIVE_W" ]]; then
        SCALE_FACTOR=$(echo "scale=4; $LIVE_W / 640" | bc)
    fi
fi

if [[ -n "$SCALE_FACTOR" ]]; then
    CELL_W_RAW=$(echo "64 * $SCALE_FACTOR" | bc)
    CELL_W=$(printf "%.0f" "$CELL_W_RAW" 2>/dev/null || echo "$CELL_W_RAW" | cut -d. -f1)

    CELL_H_RAW=$(echo "96 * $SCALE_FACTOR" | bc)
    CELL_H=$(printf "%.0f" "$CELL_H_RAW" 2>/dev/null || echo "$CELL_H_RAW" | cut -d. -f1)

    C_W_MINUS_1=$((CELL_W - 1))
    C_H_MINUS_1=$((CELL_H - 1))

    STROKE_W=$(echo "$SCALE_FACTOR / 1" | bc | cut -d. -f1)
    if [ "$STROKE_W" -lt 1 ]; then
        STROKE_W=1
    fi

    # Detect filename format: dump_tail_body_atlas_fps30_6x6.png
    TAIL_FILE=$(ls dump_tail_body_atlas_fps*.png 2>/dev/null | head -n 1)
    if [[ -n "$TAIL_FILE" ]]; then
        TAIL_GRID_STR=$(echo "$TAIL_FILE" | sed -E 's/.*fps[0-9]+_([0-9]+x[0-9]+)\.png/\1/')
        TAIL_DUMP_ROWS=$(echo "$TAIL_GRID_STR" | cut -dx -f1)
        TAIL_DUMP_COLS=$(echo "$TAIL_GRID_STR" | cut -dx -f2)
    else
        TAIL_DUMP_ROWS=6
        TAIL_DUMP_COLS=6
    fi

    EYES_FILE=$(ls dump_eyes_atlas_fps*.png 2>/dev/null | head -n 1)
    if [[ -n "$EYES_FILE" ]]; then
        EYES_GRID_STR=$(echo "$EYES_FILE" | sed -E 's/.*fps[0-9]+_([0-9]+x[0-9]+)\.png/\1/')
        EYES_DUMP_ROWS=$(echo "$EYES_GRID_STR" | cut -dx -f1)
        EYES_DUMP_COLS=$(echo "$EYES_GRID_STR" | cut -dx -f2)
    else
        EYES_DUMP_ROWS=4
        EYES_DUMP_COLS=10
    fi
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
REAL_CELL_W=$(echo "64 * $SCALE_FACTOR" | bc | cut -d. -f1)
REAL_CELL_H=$(echo "96 * $SCALE_FACTOR" | bc | cut -d. -f1)

# DYNAMIC OVERRIDES LINKED TO DETECTED ATLASED COLS
P2_CROP_W=$(( P2_CELL_W * TAIL_DUMP_COLS ))
P2_CROP_H=$(( P3_CELL_H * TAIL_DUMP_ROWS ))

# OUTER YELLOW FRAME BOUNDARIES (Snaps exactly to the asset frame edges)
RECT_X1=$(echo "2 * $SCALE_FACTOR" | bc | cut -d. -f1)
RECT_Y1=$(echo "6 * $SCALE_FACTOR" | bc | cut -d. -f1)
SCALED_SPAN_W=$(echo "59 * $SCALE_FACTOR" | bc | cut -d. -f1)
SCALED_SPAN_H=$(echo "83 * $SCALE_FACTOR" | bc | cut -d. -f1)
TARGET_X=$(echo "31 * $SCALE_FACTOR" | bc | cut -d. -f1)
TARGET_Y=$(echo "45 * $SCALE_FACTOR" | bc | cut -d. -f1)

IM_LEFT=$(( (REAL_CELL_W - SCALED_SPAN_W) / 2 ))
IM_TOP=$(( (REAL_CELL_H - SCALED_SPAN_H) / 2 ))
LOCAL_FOCAL_X=$(( (SCALED_SPAN_W * 29) / 59 ))
LOCAL_FOCAL_Y=$(( (SCALED_SPAN_H * 39) / 83 ))
OFF_X=$(( TARGET_X - IM_LEFT - LOCAL_FOCAL_X ))
OFF_Y=$(( TARGET_Y - IM_TOP - LOCAL_FOCAL_Y ))

if [ "$OFF_X" -ge 0 ]; then GEOM_X="+$OFF_X"; else GEOM_X="$OFF_X"; fi
if [ "$OFF_Y" -ge 0 ]; then GEOM_Y="+$OFF_Y"; else GEOM_Y="$OFF_Y"; fi

RECT_X2=$(( RECT_X1 + SCALED_SPAN_W - 1 ))
RECT_Y2=$(( RECT_Y1 + SCALED_SPAN_H - 1 ))

# INNER CUTOUT BOUNDARIES (Pushed inward by exactly STROKE_W pixels)
INNER_X1=$(( RECT_X1 + STROKE_W ))
INNER_Y1=$(( RECT_Y1 + STROKE_W ))
INNER_X2=$(( RECT_X2 - STROKE_W ))
INNER_Y2=$(( RECT_Y2 - STROKE_W ))

# ================================================================================
# PIPELINE 1: CLOCK HAND VALIDATION
# ================================================================================
for hand in seconds minutes hours; do
    HAND_FILE=$(ls dump_${hand}_atlas_*.png 2>/dev/null | head -n 1)
    if [ -z "$HAND_FILE" ]; then
        continue
    fi

    # Parse rows and columns dynamically from filename
    HAND_GRID_STR=$(echo "$HAND_FILE" | sed -E 's/.*_([0-9]+x[0-9]+)\.png/\1/')
    HAND_ROWS=$(echo "$HAND_GRID_STR" | cut -dx -f1)
    HAND_COLS=$(echo "$HAND_GRID_STR" | cut -dx -f2)
    HAND_CROP_W=$(( CELL_W * HAND_COLS ))
    HAND_CROP_H=$(( CELL_H * HAND_ROWS ))

    magick "$HAND_FILE" "$TMP_DIR/color_anchor.png" +append \
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
    -crop ${HAND_CROP_W}x${HAND_CROP_H}+0+0 +repage dump_${hand}_atlas_validation.png
done

# ================================================================================
# PIPELINE 2: TAIL VALIDATION
# ================================================================================
if [[ -n "$TAIL_FILE" ]]; then
    P2_W_MINUS_1=$((P2_CELL_W - 1))
    P2_H_MINUS_1=$((P3_CELL_H - 1))
    P2_LINE1_START=$(echo "40 * $SCALE_FACTOR" | bc | cut -d. -f1)
    P2_LINE1_END=$((P2_LINE1_START + STROKE_W - 1))
    P2_LINE2_START=$(echo "52 * $SCALE_FACTOR" | bc | cut -d. -f1)
    P2_LINE2_END=$((P2_LINE2_START + STROKE_W - 1))

    magick "$TAIL_FILE" "$TMP_DIR/color_anchor.png" +append \
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
fi

# ================================================================================
# PIPELINE 2.5: EYES VALIDATION
# ================================================================================
if [[ -n "$EYES_FILE" ]]; then
    E_CELL_W=$(echo "64 * $SCALE_FACTOR" | bc | cut -d. -f1)
    E_CELL_H=$(echo "32 * $SCALE_FACTOR" | bc | cut -d. -f1)
    E_W_MINUS_1=$((E_CELL_W - 1))
    E_H_MINUS_1=$((E_CELL_H - 1))
    E_CROP_W=$(( E_CELL_W * EYES_DUMP_COLS ))
    E_CROP_H=$(( E_CELL_H * EYES_DUMP_ROWS ))

    magick "$EYES_FILE" "$TMP_DIR/color_anchor.png" +append \
    \( -size ${E_CELL_W}x${E_CELL_H} xc:none \
    +antialias -stroke blue -strokewidth ${STROKE_W} -fill none \
    -draw "rectangle 0,0 ${E_W_MINUS_1},${E_H_MINUS_1}" \
    -write mpr:eyesgrid +delete \) \
    \( +clone -tile mpr:eyesgrid -draw "color 0,0 reset" \) \
    -compose Over -composite \
    -crop ${E_CROP_W}x${E_CROP_H}+0+0 +repage dump_eyes_atlas_validation.png
fi

# ================================================================================
# PIPELINE 3: COMPOSITION VALIDATION
# ================================================================================
if [ -f dump_material_composition.png ]; then
    magick dump_material_composition.png "$TMP_DIR/color_anchor.png" +append \
    -crop ${FACE_CROP_W}x${FACE_CROP_H}+${FACE_X}+${FACE_Y} +repage \
    "$TMP_DIR/color_anchor.png" +append \
    -crop ${FACE_CROP_W}x${FACE_CROP_H}+0+0 +repage "$TMP_DIR/tmp_face.png"

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
# PIPELINE 4: CLOCK HAND COMPOSITION VALIDATION
# ================================================================================
    for hand in seconds minutes hours; do
        HAND_FILE=$(ls dump_${hand}_atlas_*.png 2>/dev/null | head -n 1)
        if [ -z "$HAND_FILE" ]; then
            continue
        fi

        HAND_GRID_STR=$(echo "$HAND_FILE" | sed -E 's/.*_([0-9]+x[0-9]+)\.png/\1/')
        HAND_ROWS=$(echo "$HAND_GRID_STR" | cut -dx -f1)
        HAND_COLS=$(echo "$HAND_GRID_STR" | cut -dx -f2)
        HAND_CROP_W=$(( CELL_W * HAND_COLS ))
        HAND_CROP_H=$(( CELL_H * HAND_ROWS ))

        magick "$HAND_FILE" "$TMP_DIR/color_anchor.png" +append \
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
        -crop ${HAND_CROP_W}x${HAND_CROP_H}+0+0 +repage dump_${hand}_composition_validation.png
    done

# ================================================================================
# PIPELINE 5: VIEWPORT COMPOSITION VALIDATION
# ================================================================================
    if [ -f dump_tail_atlas_validation.png ]; then
        echo "[+] Emulating transparent ${VIEW_W}x${VIEW_H} viewports with parameterized constraints..."
        magick dump_material_composition.png "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/anchored_body.png"
        magick dump_tail_atlas_validation.png "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/anchored_atlas.png"

        MAX_GRID_IDX=$(( (TAIL_DUMP_ROWS * TAIL_DUMP_COLS) - 1 ))
        for idx in $(seq 0 $MAX_GRID_IDX); do
            X_OFF=$(( (idx % TAIL_DUMP_COLS) * P2_CELL_W ))
            Y_OFF=$(( (idx / TAIL_DUMP_COLS) * P3_CELL_H ))
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

        MAX_ROW_IDX=$(( TAIL_DUMP_ROWS - 1 ))
        MAX_COL_IDX=$(( TAIL_DUMP_COLS - 1 ))
        for row in $(seq 0 $MAX_ROW_IDX); do
            ROW_FILES=""
            for col in $(seq 0 $MAX_COL_IDX); do
                idx=$(( (row * TAIL_DUMP_COLS) + col ))
                PAD_IDX=$(printf "%02d" "$idx")
                ROW_FILES="${ROW_FILES} $TMP_DIR/view_${PAD_IDX}.png"
            done
            magick $ROW_FILES -background none +append "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/row_${row}.png"
        done

        ROW_COMPOSITE_FILES=""
        for row in $(seq 0 $MAX_ROW_IDX); do
            ROW_COMPOSITE_FILES="${ROW_COMPOSITE_FILES} $TMP_DIR/row_${row}.png"
        done
        magick $ROW_COMPOSITE_FILES -background none -append "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/tmp_composite.png"

        FINAL_TOTAL_W=$(( VIEW_W * TAIL_DUMP_COLS ))
        FINAL_TOTAL_H=$(( VIEW_H * TAIL_DUMP_ROWS ))
        magick "$TMP_DIR/tmp_composite.png" -crop ${FINAL_TOTAL_W}x${FINAL_TOTAL_H}+0+0 +repage \
        "dump_material_composition.png" +append -crop ${FINAL_TOTAL_W}x${FINAL_TOTAL_H}+0+0 +repage \
        dump_tail_composition_validation.png
    fi
else
    echo dump_material_composition.png not found.
fi

echo "[+] Validation generation complete!"
