/******************************************************************************
 * File Name:    catclock_hands.c
 * Project:      catclock-sdl3 (Modernized Kit-Cat Clock Desktop Widget)
 *
 * Authorship & Collaboration:
 *   - Developed in collaborative partnership between the User and Google Gemini AI.
 *   - Core engine optimization, refactoring architecture, and porting logic
 *     engineered jointly to achieve production-grade performance.
 *
 * Attribution & Legacy:
 *   - Inspired by the classic X11/Motif 'catclock' original program.
 *   - XBM Graphic Assets derived from the historical open-source X11 layout.
 *
 * License: Open Source / Educational - Preserve attribution upon redistribution.
 *****************************************************************************/

#include "catclock_shared.h"
#include <math.h>
#include <stdlib.h>

/**
 * @file catclock_hands.c
 * @brief Reference Integer Rasterization Pipeline for Dynamic Clock Hands.
 *
 * This module serves as the authoritative, pixel-accurate mathematical baseline
 * for the CatClock hand-rendering engine. It guarantees geometric symmetry,
 * structural proportion, and anamorphic stability across all 60 animation phases
 * without suffering from cardinal axis flattening, rounding drift, or sub-pixel
 * staircase clipping artifacts.
 *
 * DEVELOPER ROADMAP NOTE:
 * This layout is intentionally implemented as a software-backed CPU pass writing
 * to a 1D palette texture atlas array. Future optimization branches should preserve
 * this exact math model while porting into:
 *   1. Arbitrary Integer/Floating Scaling layers.
 *   2. Fixed 8-bit Anti-Aliasing (AA) sub-pixel weights.
 *   3. Modern GPU Vertex/Fragment Shader pipelines.
 */

/**
 * @brief Architectural trace helper preserved cleanly for mathematical reference.
 * @note Marked with standard unused attributes to avoid pipeline compilation alerts.
 */
__attribute__((unused)) static void DrawDebugReferenceLine(Uint8* buffer, int atlas_w, int atlas_h,
														   int x0, int y0, int x1, int y1,
														   Uint8 color_idx) {
	int dx = abs(x1 - x0);
	int dy = abs(y1 - y0);
	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;
	int err = dx - dy;

	while (1) {
		PlotSoftwarePixel(buffer, x0, y0, atlas_w, atlas_h, color_idx);
		if (x0 == x1 && y0 == y1) {
			break;
		}
		int e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x0 += sx;
		}
		if (e2 < dx) {
			err += dx;
			y0 += sy;
		}
	}
}

/**
 * @brief Authoritative Master Software Raster Pass for Clock Hand Geometry.
 *
 * MATHEMATICAL METHODOLOGY SUMMARY:
 *   1. TIMELINE & ELLIPTICAL BOUNDS: Evaluates cyclical frame indices to update
 *      a dynamic boundary extension factor. This compensates for the off-center face
 *      pivot and matches the non-uniform boundaries of the dial.
 *   2. DESIGN SEGREGATION MATRIX: Assigns hard parameter metrics (base widths,
 *      pivot extensions, shoulder midpoint anchors, color targets, and fractional multipliers)
 *      uniquely parsed per hand instance payload.
 *   3. CONTINUOUS ISOTROPIC FIELDS: Computes trajectory angles and orthogonal width
 *      directions in pure, un-skewed circular space using floating point values. This blocks
 *      algebraic symmetry breakage and right-angled flattening.
 *   4. CENTERLINE GRIDS ANCHORING: Rounded center nodes are calculated synchronously
 *      to ensure the reference axis matches integer coordinates before width extrusion occurs.
 *   5. ABSOLUTE MIRRORED EXTRUSION: Processes sub-pixel width steps using distance magnitudes
 *      to guarantee identical pixel weights on both sides of the hand. Sub-pixel protection filters
 *      force minimal 1-pixel heights to counter staircase clipping.
 *   6. MULTI-PASS COMPOSITION HULL: Passes three distinct software triangles down to the raster
 *      loop, seamlessly layering a reinforcing tapered quad torso over a sharp full-length tip.
 */
void CatClock_ShaderHands(void* renderer, int cell_x, int cell_y, int sheet_w, int sheet_h,
						  int frame_idx, void* userdata) {
	/* State variable blocks explicitly grouped at the top of the function module */
	Uint8* buffer = (Uint8*) renderer;
	int atlas_w = sheet_w;
	int phase = frame_idx % TOTAL_HAND_PHASES;
	int cell_w = ctx.hours_atlas.cell_w;
	int cell_h = ctx.hours_atlas.cell_h;

	/* Decode active configuration tracking container safely off incoming stack frames */
	struct {
		int type;
		SDL_Color color;
	}* hand_cfg = (typeof(hand_cfg)) userdata;
	int hand_type = hand_cfg ? hand_cfg->type : 0;

	/* Core structural layout parameter placeholders */
	float back_pivot_length = 7.0f;
	float base_width = 3.0f;
	float mid_factor = 0.45f;
	float shape_taper = 0.85f;
	float tip_length = 41.0f;
	float length_multiplier = 1.000f;
	Uint8 palette_hand_idx = PALETTE_HAND_SECOND;

	/* Sheet grid structure bounding alignments */
	int col_idx = frame_idx % 10;
	int row_idx = frame_idx / 10;
	int final_offset_x = col_idx * cell_w;
	int final_offset_y = row_idx * cell_h;

	/* Legacy face template configuration baselines */
	float local_pivot_x = 31.0f;
	float local_pivot_y = 45.0f;
	float aspect_x = 0.711f;
	float aspect_y = 0.97f;
	float scale = (float) ctx.current_half_steps / 2.0f;

	/* Dynamic timeline calculation tracking variables */
	float vertical_factor = 0.0f;
	float dynamic_seconds_base = 0.0f;

	/* Trigonometric circular vector elements */
	float angle_rad = 0.0f;
	float cos_a = 0.0f;
	float sin_a = 0.0f;

	/* Isotropic circular tracking profiles */
	float back_x = 0.0f, back_y = 0.0f;
	float forward_x = 0.0f, forward_y = 0.0f;

	/* Un-skewed continuous layout spine markers */
	float raw_back_x = 0.0f, raw_back_y = 0.0f;
	float raw_tip_x = 0.0f, raw_tip_y = 0.0f;
	float raw_mid_x = 0.0f, raw_mid_y = 0.0f;

	/* Continuous normal offset components */
	float perp_x = 0.0f, perp_y = 0.0f;
	float tri_half_w = 0.0f;

	/* Final integer pixel steps used for symmetrical wing extrusion */
	int step_l_x = 0, step_l_y = 0;
	int step_m_l_x = 0, step_m_l_y = 0;

	/* Final integer vertices indices mapped directly to the texture canvas grid */
	int x_tri_tip = 0, y_tri_tip = 0;
	int x_tri_l = 0, y_tri_l = 0;
	int x_tri_r = 0, y_tri_r = 0;
	int x_mid_l = 0, y_mid_l = 0;
	int x_mid_r = 0, y_mid_r = 0;

	/* Suppress mandatory callback signature parameters forced by the runtime framework */
	(void) cell_x;
	(void) cell_y;
	(void) sheet_h;

	/* =========================================================================
	 * STAGE 1: ANAMORPHIC ELLIPTICAL TIMELINE STATE EVALUATION
	 * =========================================================================
	 * Evaluates cyclical frame phases to interpolate non-uniform face heights.
	 * Compensates for the asymmetrical 31,45 face anchor, scaling the baseline
	 * seamlessly between 39px (North) and 43px (South) throughout the cycle.
	 * The output serves as the master length reference for the active frame pass.
	 */
	vertical_factor = -cosf(((float) phase / (float) TOTAL_HAND_PHASES) * 2.0f * (float) M_PI);
	dynamic_seconds_base = 41.0f + (vertical_factor * 2.0f);

	/* =========================================================================
	 * STAGE 2: CONFIGURATION MATRIX DESIGN SEGREGATION
	 * =========================================================================
	 * Maps specific structural configurations dynamically onto our state registers.
	 * Enforces the 7:5:3 structural width and length proportions relative to the
	 * dynamic baseline. This isolates each hand class without breaking shared formulas.
	 */
	if (hand_type == HAND_TYPE_HOUR) {
		back_pivot_length = 3.0f;
		length_multiplier = 28.0f / 41.0f; /* Fixed ratio multiplier relative to the second hand */
		base_width = 7.0f; /* Hard-coded structural thickness */
		mid_factor = 0.22f; /* Shoulder torso placement factor close to base */
		shape_taper = 0.90f; /* Slight width narrowing factor at torso */
		palette_hand_idx = PALETTE_HAND_HOUR;
	} else if (hand_type == HAND_TYPE_MINUTE) {
		back_pivot_length = 5.0f;
		length_multiplier = 36.0f / 41.0f; /* Fixed ratio multiplier relative to the second hand */
		base_width = 5.0f; /* Hard-coded structural thickness */
		mid_factor = 0.33f; /* Mid-range shoulder placement */
		shape_taper = 0.85f; /* Standard width tapering factor */
		palette_hand_idx = PALETTE_HAND_MINUTE;
	} else {
		back_pivot_length = 7.0f;
		length_multiplier = 1.000f; /* Second hand matches 100% of the reference scale */
		base_width = 3.0f; /* High-efficiency thin second needle */
		mid_factor = 0.45f; /* Long, slender torso reaching near center span */
		shape_taper = 0.85f;
		palette_hand_idx = PALETTE_HAND_SECOND;
	}

	/* Guard scale factor protections against degenerate tiny setups */
	if (scale < 1.0f) {
		scale = 1.0f;
	}

	/* =========================================================================
	 * STAGE 3: CONTINUOUS ISOTROPIC SPACE VECTOR COMPOSITION
	 * =========================================================================
	 * Computes directional trajectories and perpendicular cross-vectors in a
	 * uniform, isotropic circular frame before non-uniform aspect metrics are applied.
	 * This isolates our trigonometry from horizontal compression distorting our normals.
	 */
	angle_rad
		= ((float) phase / (float) TOTAL_HAND_PHASES) * 2.0f * (float) M_PI - ((float) M_PI / 2.0f);
	cos_a = cosf(angle_rad);
	sin_a = sinf(angle_rad);

	/* Apply the fractional length multiplier to isolate dynamic forward lengths */
	tip_length = dynamic_seconds_base * length_multiplier;

	/* Factor active global scaling parameters cleanly into un-rounded dimensions */
	back_pivot_length *= scale;
	tip_length *= scale;
	tri_half_w = (base_width / 2.0f) * scale;

	/* Extrude orientation directional poles symmetrically from the face origin */
	forward_x = cos_a * tip_length;
	forward_y = sin_a * tip_length;

	back_x = -cos_a * back_pivot_length;
	back_y = -sin_a * back_pivot_length;

	/* Calculate a mathematically precise normal vector in un-skewed isotropic space */
	perp_x = -sin_a;
	perp_y = cos_a;

	/* =========================================================================
	 * STAGE 4: CENTERLINE GRID POSITION LOCKING
	 * =========================================================================
	 * Core spine coordinates are locked to integer values first to guarantee a shared baseline */
	raw_back_x = local_pivot_x + (back_x * aspect_x);
	raw_back_y = local_pivot_y + (back_y * aspect_y);

	raw_tip_x = local_pivot_x + (forward_x * aspect_x);
	raw_tip_y = local_pivot_y + (forward_y * aspect_y);

	/* Linearly interpolate the torso shoulder node in continuous float space */
	raw_mid_x = raw_back_x + (raw_tip_x - raw_back_x) * mid_factor;
	raw_mid_y = raw_back_y + (raw_tip_y - raw_back_y) * mid_factor;

	/* Ground layout center anchors to the exact integer pixel tracks */
	int cx0 = (int) floorf(raw_back_x + 0.5f) + final_offset_x;
	int cy0 = (int) floorf(raw_back_y + 0.5f) + final_offset_y;

	int cx1 = (int) floorf(raw_tip_x + 0.5f) + final_offset_x;
	int cy1 = (int) floorf(raw_tip_y + 0.5f) + final_offset_y;

	int cx_mid = (int) floorf(raw_mid_x + 0.5f) + final_offset_x;
	int cy_mid = (int) floorf(raw_mid_y + 0.5f) + final_offset_y;

	/* =========================================================================
	 * STAGE 5: MAGNITUDE-BASED MIRRORED WIDTH EXTRUSION
	 * =========================================================================
	 * Resolves cardinal line collapsing and asymmetric pixel inflation.
	 * Width steps are rounded as absolute positive values before directional signs
	 * are applied, forcing left and right spans to match perfectly on the grid.
	 * Sub-pixel threshold filters force minimal 1-pixel heights to block clipping.
	 */
	float fx_l = perp_x * tri_half_w * aspect_x;
	float fy_l = perp_y * tri_half_w * aspect_y;

	int idx_w_x = (int) floorf(fabsf(fx_l) + 0.5f);
	int idx_w_y = (int) floorf(fabsf(fy_l) + 0.5f);

	/* Sub-pixel clipping prevention filter blocks zero-width flattening */
	if (idx_w_x == 0 && fabsf(fx_l) > 0.05f)
		idx_w_x = 1;
	if (idx_w_y == 0 && fabsf(fy_l) > 0.05f)
		idx_w_y = 1;

	/* Re-apply layout signs to establish the true directional integer offset steps */
	step_l_x = (fx_l >= 0.0f) ? idx_w_x : -idx_w_x;
	step_l_y = (fy_l >= 0.0f) ? idx_w_y : -idx_w_y;

	/* Apply the identical magnitude-mirroring pass to the torso shoulders */
	float fx_m = perp_x * tri_half_w * shape_taper * aspect_x;
	float fy_m = perp_y * tri_half_w * shape_taper * aspect_y;

	int idx_m_x = (int) floorf(fabsf(fx_m) + 0.5f);
	int idx_m_y = (int) floorf(fabsf(fy_m) + 0.5f);

	if (idx_m_x == 0 && fabsf(fx_m) > 0.05f)
		idx_m_x = 1;
	if (idx_m_y == 0 && fabsf(fy_m) > 0.05f)
		idx_m_y = 1;

	step_m_l_x = (fx_m >= 0.0f) ? idx_m_x : -idx_m_x;
	step_m_l_y = (fy_m >= 0.0f) ? idx_m_y : -idx_m_y;

	/* =========================================================================
	 * STAGE 6: GEOMETRY LAYER ASSEMBLING & SOFTWARE RASTERIZATION
	 * =========================================================================
	 * Extrudes vertices by combining our integer steps with the locked spine anchors.
	 * Deploys a multi-pass mesh hull that layers a reinforcing shoulder quad on top of
	 * a sharp, full-length diamond hull. This maintains a sleek taper while adding
	 * structural reinforcement near thin horizontal cardinal boundaries.
	 */
	x_tri_tip = cx1;
	y_tri_tip = cy1;

	/* Build the base wings symmetrically by adding and subtracting our mirrored steps */
	x_tri_l = cx0 + step_l_x;
	y_tri_l = cy0 + step_l_y;
	x_tri_r = cx0 - step_l_x;
	y_tri_r = cy0 - step_l_y;

	/* Build the shoulder wings symmetrically using identical structural spacing rules */
	x_mid_l = cx_mid + step_m_l_x;
	y_mid_l = cy_mid + step_m_l_y;
	x_mid_r = cx_mid - step_m_l_x;
	y_mid_r = cy_mid - step_m_l_y;

	/* Pass A: Rasterize the full-length baseline triangle shape */
	FillSoftwareTriangle(buffer, x_tri_l, y_tri_l, x_tri_r, y_tri_r, x_tri_tip, y_tri_tip, atlas_w,
						 ctx.hours_atlas.atlas_h, palette_hand_idx);

	/* Pass B: Layer the first half of the reinforcing shoulder torso quad on top */
	FillSoftwareTriangle(buffer, x_tri_l, y_tri_l, x_tri_r, y_tri_r, x_mid_l, y_mid_l, atlas_w,
						 ctx.hours_atlas.atlas_h, palette_hand_idx);

	/* Pass C: Layer the second half of the reinforcing shoulder torso quad on top */
	FillSoftwareTriangle(buffer, x_tri_r, y_tri_r, x_mid_l, y_mid_l, x_mid_r, y_mid_r, atlas_w,
						 ctx.hours_atlas.atlas_h, palette_hand_idx);
}
