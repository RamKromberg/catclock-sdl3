#!/bin/sh
# ============================================================================
# MESA 3D COMPREHENSIVE PRIMITIVE SETUP DIAGNOSTIC EXTRACTOR
# Extracts core, fallback, and structural definitions into a single file.
# ============================================================================

OUTPUT_FILE="./dump.txt"
rm -f "$OUTPUT_FILE"

{
  echo "=============================================================================="
  echo " COMPREHENSIVE MESA RASTERIZATION PIPELINE ROADMAP"
  echo "=============================================================================="
  echo "Target Defect: 1px top-left clipping under 2.00x scale metrics."
  echo "Context Scope: Full non-SIMD scalar geometry infrastructure."
  echo ""
  
  echo "=============================================================================="
  echo " CORE INFRASTRUCTURE DEFINITIONS & FIELD ALIGNMENTS"
  echo "=============================================================================="
} >> "$OUTPUT_FILE"

# 1. Structural Canvas Property Schematics
echo "--- src/gallium/drivers/llvmpipe/lp_setup_context.h: Structure Specifications ---" >> "$OUTPUT_FILE"
grep -n -h -A 120 "struct lp_setup_context" "$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/lp_setup_context.h" >> "$OUTPUT_FILE"

echo "--- src/gallium/drivers/llvmpipe/lp_rast.h: Structure Specifications ---" >> "$OUTPUT_FILE"
grep -n -h -A 100 "struct lp_rasterizer" "$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/lp_rast.h" >> "$OUTPUT_FILE"

# 2. Math & Precision Macros
{
  echo ""
  echo "=============================================================================="
  echo " PRECISION INVARIANTS & INTEGRATION MACROS"
  echo "=============================================================================="
} >> "$OUTPUT_FILE"
echo "--- src/util/u_math.h: util_iround ---" >> "$OUTPUT_FILE"
grep -n -h -C 5 "util_iround" "$DEVSHELL_MESA_SRC/src/util/u_math.h" >> "$OUTPUT_FILE"
echo "--- src/gallium/drivers/llvmpipe/lp_rast.h: #define FIXED_ ---" >> "$OUTPUT_FILE"
grep -n -h "#define FIXED_" "$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/lp_rast.h" >> "$OUTPUT_FILE"

# 3. Comprehensive Setup Function Blocks
{
  echo ""
  echo "========================================================================================"
  echo " SCALAR PRIMITIVE SETUP PIPELINE ROUTINES (src/gallium/drivers/llvmpipe/lp_setup_tri.c) "
  echo "========================================================================================"
} >> "$OUTPUT_FILE"

# Extract full triangle setup entry path (bounding box calculations, orientation checks)
echo "--- lp_setup_triangle Entry & Validation Logic ---" >> "$OUTPUT_FILE"
grep -n -h -A 250 "void
lp_setup_triangle" "$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/lp_setup_tri.c" >> "$OUTPUT_FILE"

# Extract plane coefficient calculation, tie-breaking, and trivial reject offsets
echo "--- do_triangle Winding, Coefficient Base, & Tie-Breaker Injection ---" >> "$OUTPUT_FILE"
grep -n -h -A 300 "static void
do_triangle" "$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/lp_setup_tri.c" >> "$OUTPUT_FILE"

# 4. Fallback Block Processing Paths
{
  echo ""
  echo "=============================================================================================="
  echo " INCREMENTAL LINEAR BLOCK SWEEP LOOP (src/gallium/drivers/llvmpipe/lp_rast_linear_fallback.c) "
  echo "=============================================================================================="
} >> "$OUTPUT_FILE"
grep -n -h -A 200 "void
lp_rast_linear_triangle" "$DEVSHELL_MESA_SRC/src/gallium/drivers/llvmpipe/lp_rast_linear_fallback.c" >> "$OUTPUT_FILE"

echo "Diagnostic payload generated successfully."
