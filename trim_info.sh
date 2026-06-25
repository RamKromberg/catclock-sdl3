#!/bin/sh
set -e

# Validate that an input file is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <input_image_path>" >&2
    exit 1
fi

INPUT_IMG="$1"

# Check if file exists
if [ ! -f "$INPUT_IMG" ]; then
    echo "Error: File '$INPUT_IMG' not found." >&2
    exit 1
fi

# 1. Query ImageMagick for original dimensions and the internal trim geometry
# Format output: W_ORIG H_ORIG TWxTH+TX+TY
INFO_STR=$(magick "$INPUT_IMG" -format "%w %h %@" info:)

# 2. Parse the space-separated output safely using standard POSIX read
IFS=" " read -r W_ORIG H_ORIG TRIM_GEOM <<EOF
$INFO_STR
EOF

# 3. Parse the trim geometry components (WxH+X+Y)
IFS="x+" read -r TW TH TX TY <<EOF
$TRIM_GEOM
EOF

# 4. Calculate directional chops using POSIX arithmetic
CHOP_NORTH="$TY"
CHOP_WEST="$TX"
CHOP_EAST=$((W_ORIG - TW - TX))
CHOP_SOUTH=$((H_ORIG - TH - TY))

# 5. Build up the precise chain of chop arguments dynamically based on calculated changes
CHOP_ARGS=""

if [ "$CHOP_EAST" -gt 0 ]; then
    CHOP_ARGS="$CHOP_ARGS -gravity East -chop ${CHOP_EAST}x0"
fi

if [ "$CHOP_WEST" -gt 0 ]; then
    CHOP_ARGS="$CHOP_ARGS -gravity West -chop ${CHOP_WEST}x0"
fi

if [ "$CHOP_NORTH" -gt 0 ]; then
    CHOP_ARGS="$CHOP_ARGS -gravity North -chop 0x${CHOP_NORTH}"
fi

if [ "$CHOP_SOUTH" -gt 0 ]; then
    CHOP_ARGS="$CHOP_ARGS -gravity South -chop 0x${CHOP_SOUTH}"
fi

# 6. Check if any trimming actually occurred
if [ -z "$CHOP_ARGS" ]; then
    echo "# Image does not require any trimming."
    exit 0
fi

# 7. Print out the exact reconstructed string command
echo "magick $INPUT_IMG$CHOP_ARGS $(basename "$INPUT_IMG")"
