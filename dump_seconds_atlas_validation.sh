#!/bin/sh
for f in *.pam; do magick "$f" "${f%.pam}.png"; done; rm *.pam;

# Extract the clock face texture patch
magick dump_material_composition.png -crop 59x83+20+100 tmp_face.png

# Create a 1x1 pixel high-contrast red color anchor file to break grayscale auto-detection
magick -size 1x1 xc:red color_anchor.png

for hand in hours minutes seconds; do
  # 1. Standard grid validation pass
  # Append the red pixel to expand the width from 640px to 641px
  magick dump_${hand}_atlas.png color_anchor.png +append \
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
  magick dump_${hand}_atlas.png color_anchor.png +append \
    \( -size 64x96 xc:none +antialias \
       tmp_face.png -gravity center -geometry +0-2 -compose Over -composite \
       -stroke blue -strokewidth 1 -fill none -draw "rectangle 0,0 63,95" \
       -stroke green -strokewidth 1 -draw "line 31,0 31,95" -draw "line 0,45 63,45" \
       -write mpr:compgrid +delete \) \
    \( +clone -tile mpr:compgrid -draw "color 0,0 reset" \) \
    -compose Dst_Over -composite \
    -crop 640x576+0+0 +repage dump_${hand}_composition_validation.png
done

# Clean up temporary assets
rm tmp_face.png color_anchor.png
