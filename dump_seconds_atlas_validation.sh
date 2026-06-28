#!/bin/sh
set -e

# Setup a secure temporary working directory using mktemp to protect the disk
TMP_DIR=$(mktemp -d -t catclock_validate_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

# ==============================================================================
# BOILERPLATE INITIALIZATION (ALWAYS FIRST)
# ==============================================================================
# Create a 1x1 pixel high-contrast red color anchor file to break auto-palettization
magick -size 1x1 xc:red "$TMP_DIR/color_anchor.png"

# Convert any input PAM files to PNG with explicit boilerplate anchor protection
for f in *.pam; do
  magick "$f" "$TMP_DIR/color_anchor.png" +append -crop 100%x100%+0+0 +repage -gravity East -chop 1x0 "${f%.pam}.png"
done
rm -f *.pam

# ==============================================================================
# PIPELINE 1: CLOCK HAND VALIDATION LOOP (64x96 GRID)
# ==============================================================================

# Safely extract the clock face texture patch
magick dump_material_composition.png "$TMP_DIR/color_anchor.png" +append \
  -crop 59x83+20+100 +repage "$TMP_DIR/color_anchor.png" +append \
  -crop 59x83+0+0 +repage "$TMP_DIR/tmp_face.png"

for hand in hours minutes seconds; do
  # 1. Standard grid validation pass
  # Append the red pixel to expand the width from 640px to 641px
  magick dump_${hand}_atlas.png "$TMP_DIR/color_anchor.png" +append \
    \( -size 64x96 xc:none +antialias \
       -stroke blue -strokewidth 1 -fill none -draw "rectangle 0,0 63,95" \
       -stroke yellow -strokewidth 1 -draw "rectangle 2,6 60,88" \
       -stroke green -strokewidth 1 -draw "line 31,0 31,95" -draw "line 0,45 63,45" \
       -write mpr:mygrid +delete \) \
    \( +clone -tile mpr:mygrid -draw "color 0,0 reset" \) \
    -compose Dst_Over -composite \
    -crop 640x576+0+0 +repage dump_${hand}_atlas_validation.png

  # 2. Hard constraint composition pass
  # Append the red pixel to expand the width from 640px to 641px
  magick dump_${hand}_atlas.png "$TMP_DIR/color_anchor.png" +append \
    \( -size 64x96 xc:none +antialias \
       "$TMP_DIR/tmp_face.png" -gravity center -geometry +0-2 -compose Over -composite \
       -stroke blue -strokewidth 1 -fill none -draw "rectangle 0,0 63,95" \
       -stroke green -strokewidth 1 -draw "line 31,0 31,95" -draw "line 0,45 63,45" \
       -write mpr:compgrid +delete \) \
    \( +clone -tile mpr:compgrid -draw "color 0,0 reset" \) \
    -compose Dst_Over -composite \
    -crop 640x576+0+0 +repage dump_${hand}_composition_validation.png
done


# ==============================================================================
# PIPELINE 2: TAIL VALIDATION PASS (96x96 GRID, 960px CANVAS WIDTH)
# ==============================================================================
magick dump_tail_body_atlas.png "$TMP_DIR/color_anchor.png" +append \
  \( -size 96x96 xc:none +antialias \
     -stroke blue -strokewidth 1 -fill none -draw "rectangle 0,0 95,95" \
     -stroke green -strokewidth 1 -draw "line 47,0 47,95" -draw "line 0,12 95,12" \
     -write mpr:tailgrid +delete \) \
  \( +clone -tile mpr:tailgrid -draw "color 0,0 reset" \) \
  -compose Dst_Over -composite \
  -crop 960x576+0+0 +repage dump_tail_atlas_validation.png

# ==============================================================================
# PIPELINE 3: VIEWPORT COMPOSITION EMULATION (TUNING CONFIGURATIONS)
# ==============================================================================
# Tweak these parameters to discover the old viewport layout values
VIEW_W=150
VIEW_H=300

# Catback/Material bounds and placement location inside the viewport frame
BODY_CROP_W=101
BODY_CROP_H=201
BODY_X=24
BODY_Y=12

# Individual Tail Frame cell bounds and placement location inside the viewport frame
TAIL_CROP_W=96
TAIL_CROP_H=96
TAIL_X=27
TAIL_Y=203

echo "[+] Emulating transparent ${VIEW_W}x${VIEW_H} viewports with parameterized constraints..."

# Secure base images with boilerplate protection
magick dump_material_composition.png "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/anchored_body.png"
magick dump_tail_body_atlas.png "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/anchored_atlas.png"

for idx in $(seq 0 59); do
  X_OFF=$(( (idx % 10) * 96 ))
  Y_OFF=$(( (idx / 10) * 96 ))
  
  PAD_IDX=$(printf "%02d" "$idx")

  # Isolate the single tail frame cell cleanly with anchor protection
  magick "$TMP_DIR/anchored_atlas.png" -crop 96x96+${X_OFF}+${Y_OFF} +repage \
    "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/tail_${PAD_IDX}.png"

  # Layer composition into individual frame viewports
  # Stack order: 
  # 1. Base blank viewport canvas
  # 2. Outer blue border box around the full viewport 
  # 3. Inner blue border box outlining the specific 96x96 tail texture placement
  # 4. Tail asset overlay
  # 5. Master catback body asset overlay on top
  magick -size ${VIEW_W}x${VIEW_H} xc:none \
    -stroke blue -strokewidth 1 -fill none -draw "rectangle 0,0 $((VIEW_W-1)),$((VIEW_H-1))" \
    -stroke blue -strokewidth 1 -fill none -draw "rectangle ${TAIL_X},${TAIL_Y} $((TAIL_X + TAIL_CROP_W - 1)),$((TAIL_Y + TAIL_CROP_H - 1))" \
    \( "$TMP_DIR/tail_${PAD_IDX}.png" -crop ${TAIL_CROP_W}x${TAIL_CROP_H}+0+0 +repage \) -geometry +${TAIL_X}+${TAIL_Y} -compose Over -composite \
    \( "$TMP_DIR/anchored_body.png" -crop ${BODY_CROP_W}x${BODY_CROP_H}+0+0 +repage \) -geometry +${BODY_X}+${BODY_Y} -compose Over -composite \
    "$TMP_DIR/color_anchor.png" +append \
    "$TMP_DIR/view_${PAD_IDX}.png"
done

# Grid stitch the rows horizontally (+append)
for row in $(seq 0 5); do
  ROW_FILES=""
  for col in $(seq 0 9); do
    idx=$(( (row * 10) + col ))
    PAD_IDX=$(printf "%02d" "$idx")
    ROW_FILES="${ROW_FILES} $TMP_DIR/view_${PAD_IDX}.png"
  done
  magick $ROW_FILES -background none +append "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/row_${row}.png"
done

# Stack the completed rows vertically (-append) to build the grid matrix sheet
magick "$TMP_DIR/row_0.png" "$TMP_DIR/row_1.png" "$TMP_DIR/row_2.png" \
       "$TMP_DIR/row_3.png" "$TMP_DIR/row_4.png" "$TMP_DIR/row_5.png" \
       -background none -append "$TMP_DIR/color_anchor.png" +append "$TMP_DIR/tmp_composite.png"

# Final slice pass to strip the horizontal footprint while preserving forced alpha transparency
FINAL_TOTAL_W=$(( VIEW_W * 10 ))
FINAL_TOTAL_H=$(( VIEW_H * 6 ))

magick "$TMP_DIR/tmp_composite.png" -crop ${FINAL_TOTAL_W}x${FINAL_TOTAL_H}+0+0 +repage \
  "$TMP_DIR/color_anchor.png" +append -crop ${FINAL_TOTAL_W}x${FINAL_TOTAL_H}+0+0 +repage \
  dump_tail_composition_validation.png

echo "[+] Validation generation complete!"
