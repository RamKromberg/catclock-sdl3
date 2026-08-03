#!/bin/sh

set -e

# Initialize output workspace
mkdir -p icons

# ==============================================================================
# PART 1: THE DISCOVERED BASELINE SETUP
# ==============================================================================

# 1. Flood-fill background transparent (still reading from source assets folder)
magick ../assets/cat.xbm -colorspace sRGB -alpha set \
  -bordercolor white -border 5 \
  -fuzz 1% -fill none -draw "color 0,0 floodfill" \
  -alpha set alpha_mask.png

# 2. Trim to get the 47x50 source
magick ./alpha_mask.png -trim +repage -colorspace Gray alpha_trimmed.png

# 3. Chop rows, draw white pixel, and initialize the sRGB channel isolation trick
magick ./alpha_trimmed.png -gravity north -chop 0x2+0+27 +repage -gravity north -chop 0x1+0+44 +repage \
  -fill white -draw "point 21,24" \
  -fill red -draw "point 0,0" \
  -fill blue -opaque black \
  -transparent red +repage alpha_47x47.png

rm ./alpha_mask.png ./alpha_trimmed.png


# ==============================================================================
# PART 2: THE REUSABLE POSIX ICON PIPELINE FUNCTION
# ==============================================================================

build_icon() {
  SCALE_SIZE="$1"
  FINAL_SIZE="$2"
  
  # A safe working canvas buffer padding
  BUFFER_SIZE=$((FINAL_SIZE + 40))

  # A. Sharp Nearest-Neighbor resize to target dimension (e.g., 14x14)
  magick alpha_47x47.png -filter point -resize "${SCALE_SIZE}x${SCALE_SIZE}" +repage tmp_scaled.png
  
  # B. Expand canvas outward widely to give morphology plenty of breathing room
  magick tmp_scaled.png -background none -gravity center -extent "${BUFFER_SIZE}x${BUFFER_SIZE}" +repage tmp_expanded.png
  
  # C. Extract raw alpha shape into a solid green multi-channel silhouette
  magick tmp_expanded.png -alpha extract -background green -alpha shape +repage tmp_silhouette.png
  
  # D. Run morphology directly on the color channels of the solid green silhouette
  magick tmp_silhouette.png -morphology EdgeOut Square:1 tmp_edge_raw.png
  
  # E. Swap green to white, use the temporary green helper at 0,0 to preserve channels,
  # and then IMMEDIATELY turn that green pixel transparent so it doesn't break the final -trim!
  magick tmp_edge_raw.png -fill white -opaque green -fill green -draw "point 0,0" \
    -transparent green +repage tmp_outline.png
  
  # F. Composite original art on outline, trim to final perfect frame, flatten to Gray
  OUTPUT_PATH="icons/catclock_${FINAL_SIZE}.png"
  magick tmp_expanded.png tmp_outline.png -compose DstOver -composite \
    -trim +repage -colorspace Gray "$OUTPUT_PATH"

  # G. Compress PNGs
  exiftool -png:all= "$OUTPUT_PATH"
	optipng -o7 "$OUTPUT_PATH"
  zopflipng -y -m --filters=p "$OUTPUT_PATH" "$OUTPUT_PATH"
  
  # H. Deploy to standard Freedesktop structure under icons/
  mkdir -p "icons/hicolor/${FINAL_SIZE}x${FINAL_SIZE}/apps"
  cp "$OUTPUT_PATH" "icons/hicolor/${FINAL_SIZE}x${FINAL_SIZE}/apps/catclock.png"
  
  # Flush working memory files before next function run
  rm tmp_scaled.png tmp_expanded.png tmp_silhouette.png tmp_edge_raw.png tmp_outline.png
}

# ==============================================================================
# PART 3: GENERATING ALL NATIVE SIZES
# ==============================================================================

build_icon 14 16
build_icon 30 32
build_icon 126 128
build_icon 254 256
build_icon 510 512

# ==============================================================================
# PART 4: PACKAGING NON-SCALABLE TARGET SYSTEMS
# ==============================================================================

# A. Bundle the native power-of-two PNG icons into a single multi-res Windows ICO container under icons/
magick icons/catclock_16.png icons/catclock_32.png icons/catclock_128.png icons/catclock_256.png icons/catclock.ico

# B. Autogenerate your production-ready Win32 Resource script descriptor file (.rc) under icons/
cat <<EOF > icons/resource.rc
// Win32 App Resource definition script
IDI_APPLICATION_ICON ICON "catclock.ico"
EOF

# ==============================================================================
# PART 5: PACKAGING SCALABLE TARGET SYSTEMS
# ==============================================================================

# Target paths under icons/
PYTHON_TRACER="cook_catclock_47_to_svg.py"
OUTPUT_SVG="icons/catclock.svg"

# 1. Strip the helper colors to produce a clean, un-outlined 47x47 grayscale image for the SVG
magick alpha_47x47.png -fill black -opaque blue -colorspace Gray icons/catclock_47_clean.png
echo " -> Clean un-outlined 47x47 asset generated."

if [ ! -f "$PYTHON_TRACER" ]; then
  echo "Error: Python tracer script missing at '$PYTHON_TRACER'." >&2
  exit 1
fi

# 2. Get the perfectly ordered crisp vector outline path from Python using the clean asset path
echo " -> Tracing ordered pixel-art contours via Python..."
VECTOR_PATH=$(python3 "$PYTHON_TRACER" "icons/catclock_47_clean.png")

if [ -z "$VECTOR_PATH" ]; then
  echo "Error: Python tracer returned an empty path string." >&2
  exit 1
fi

# 3. Compress icons/catclock_47_clean.png
echo " -> Compressing PNG for SVG..."
exiftool -png:all= "icons/catclock_47_clean.png"
optipng -o7 "icons/catclock_47_clean.png"
zopflipng -y -m --filters=p "icons/catclock_47_clean.png" "icons/catclock_47_clean.png"

# 4. Base64 encode the clean unscaled 47x47 PNG image
echo " -> Generating image Base64 data stream..."
BASE64_DATA=$(base64 -w 0 icons/catclock_47_clean.png)

# 5. Construct the clean SVG container with the embedded asset and its unscaling contour path
echo " -> Constructing final standalone SVG container layout..."
cat <<EOF > "$OUTPUT_SVG"
<svg xmlns="http://www.w3.org/2000/svg" viewBox="-0.5 -0.5 48 48" shape-rendering="crispEdges">
  <style>
    /* Force modern desktop environments to upscale the embedded pixel art cleanly */
    image {
      image-rendering: -moz-crisp-edges;
      image-rendering: -webkit-crisp-edges;
      image-rendering: pixelated;
      image-rendering: crisp-edges;
    }
    
    /* Lock the custom form-fitting path lines to exactly 1 hardware layout display pixel */
    .unscaled-contour-outline {
      fill: none;
      stroke: #ffffff;
      stroke-width: 1px;
      vector-effect: non-scaling-stroke;
      stroke-linejoin: miter;
    }
  </style>
  <g>
    <!-- 1. The clean unscaled 47x47 base graphic embedded natively -->
    <image width="47" height="47" href="data:image/png;base64,${BASE64_DATA}"/>
    
    <!-- 2. The exact form-fitting vector path tracing your cat contours perfectly -->
    <path class="unscaled-contour-outline" d="${VECTOR_PATH}"/>
  </g>
</svg>
EOF

# Clean up working directory temporary assets
rm alpha_47x47.png icons/catclock_47_clean.png

echo " -> Success! Standalone vector icon built at '$OUTPUT_SVG'."
echo " -> All done."
