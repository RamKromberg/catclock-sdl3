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
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FLOAT_TO_FIXED24_8(f) ((int32_t) ((f) * 256.0f))
#define INT_TO_FIXED24_8(i) ((int32_t) ((i) << 8))

/* =========================================================================
   STRUCTURAL TYPE DEFINITIONS
   ========================================================================= */
typedef struct {
	int dx; /* Exact 1x horizontal pixel offset from focal center */
	int dy; /* Exact 1x vertical pixel offset from focal center */
} HandMasterOffset;

typedef struct {
	float x;
	float y;
} gl_Vec2;

typedef struct {
	int32_t x;
	int32_t y;
} gl_Vertex;

const int TEXTURE_CELL_W = 64;
const int TEXTURE_CELL_H = 96;
const int RELATIVE_FOCAL_X = 29;
const int RELATIVE_FOCAL_Y = 39;
const int ENVELOPE_PAD_X = 2;
const int ENVELOPE_PAD_Y = 6;
const int PIVOT_AXIS_X = ENVELOPE_PAD_X + RELATIVE_FOCAL_X;
const int PIVOT_AXIS_Y = ENVELOPE_PAD_Y + RELATIVE_FOCAL_Y;

/* Master Phase Coordinate Target Array (Strictly Preserved Superellipse Mapping) */
static const HandMasterOffset HAND_MASTER_OFFSETS[TOTAL_HAND_PHASES]
	= { { 0, -39 },	  { 4, -39 },	{ 8, -38 },	  { 12, -37 },	{ 15, -35 },  { 19, -32 },
		{ 22, -30 },  { 24, -27 },	{ 26, -24 },  { 27, -20 },	{ 28, -16 },  { 29, -13 },
		{ 29, -10 },  { 29, -6 },	{ 29, -3 },	  { 29, 0 },	{ 29, 3 },	  { 29, 6 },
		{ 29, 10 },	  { 29, 13 },	{ 29, 17 },	  { 29, 21 },	{ 28, 25 },	  { 26, 29 },
		{ 23, 32 },	  { 20, 35 },	{ 17, 38 },	  { 13, 40 },	{ 9, 42 },	  { 5, 43 },
		{ 0, 43 },	  { -5, 43 },	{ -9, 42 },	  { -13, 40 },	{ -17, 38 },  { -20, 35 },
		{ -23, 32 },  { -26, 29 },	{ -28, 25 },  { -29, 21 },	{ -29, 17 },  { -29, 13 },
		{ -29, 10 },  { -29, 6 },	{ -29, 3 },	  { -29, 0 },	{ -29, -3 },  { -29, -6 },
		{ -29, -10 }, { -29, -13 }, { -28, -16 }, { -27, -20 }, { -26, -24 }, { -24, -26 },
		{ -22, -30 }, { -19, -33 }, { -15, -35 }, { -12, -37 }, { -8, -38 },  { -4, -39 } };

/* =========================================================================
   CLEAN PIXEL-GRID GEOMETRY UTILITIES
   ========================================================================= */

/**
 * Calculates the baseline unscaled pivot origin absolute coordinate inside the target frame cell
 * space.
 */
static gl_Vec2 CalculateBaselinePivot(int cell_x, int cell_y) {
	gl_Vec2 pivot;
	pivot.x = (float) cell_x + (float) PIVOT_AXIS_X;
	pivot.y = (float) cell_y + (float) PIVOT_AXIS_Y;
	return pivot;
}

/**
 * Clean baseline float rounding helper.
 */
static inline int32_t CleanPixelRound(float val) { return (int32_t) floorf(val + 0.5f); }

/**
 * Symmetric tie breaker pushing floating point offsets outwards from vector alignments cleanly.
 */
static inline int32_t SymmetricCrossRound(float base_coord, float displacement, float sign_dir) {
	float target = base_coord + displacement;
	if (sign_dir > 0.0f) {
		return (int32_t) floorf(target + 0.5001f);
	} else if (sign_dir < 0.0f) {
		return (int32_t) floorf(target + 0.4999f);
	}
	return (int32_t) floorf(target + 0.5f);
}

/* =========================================================================
   EXTRACTED REFERENCE GEOMETRY PIPELINE
   ========================================================================= */

// ============================================================================
// DUAL-PHASE SYMMETRIC HAND GEOMETRY COMPOTATION ENGINE
// ============================================================================
/**
 * Computes pixel-perfect reference vertices for a specified hand type and phase at 1x resolution.
 * This completely isolates structural lookup coordinates from scaling artifacts or context
 * transformations.
 */
void CatClock_ComputeReferenceHandVertices(int cell_x, int cell_y, int hand_type, int phase_idx,
										   gl_Vertex out_vertices[3]) {
	int phase = phase_idx % TOTAL_HAND_PHASES;

	// Structural Constants matching unscaled 1x reference dimensions
	float back_pivot_length = 7.0f;
	float length_multiplier = 1.0f;
	float base_width = 3.0f;

	if (hand_type == HAND_TYPE_HOUR) {
		back_pivot_length = 3.0f;
		length_multiplier = 3.0f / 7.0f;
		base_width = 7.0f;
	} else if (hand_type == HAND_TYPE_MINUTE) {
		back_pivot_length = 5.0f;
		length_multiplier = 5.0f / 7.0f;
		base_width = 5.0f;
	}

	gl_Vec2 pivot = CalculateBaselinePivot(cell_x, cell_y);

	// 1. Forward tip target lookup translation (Vertex 0 stays anchored as tip)
	float target_dx = (float) HAND_MASTER_OFFSETS[phase].dx * length_multiplier;
	float target_dy = (float) HAND_MASTER_OFFSETS[phase].dy * length_multiplier;

	out_vertices[0].x = (int32_t) pivot.x + CleanPixelRound(target_dx);
	out_vertices[0].y = (int32_t) pivot.y + CleanPixelRound(target_dy);

	// 2. Compute the exact rear center coordinate profile
	int tail_phase = (phase + 30) % TOTAL_HAND_PHASES;
	float tail_dx = (float) HAND_MASTER_OFFSETS[tail_phase].dx * length_multiplier;
	float tail_dy = (float) HAND_MASTER_OFFSETS[tail_phase].dy * length_multiplier;

	float base_center_xf = pivot.x + (tail_dx * (back_pivot_length / 39.0f));
	float base_center_yf = pivot.y + (tail_dy * (back_pivot_length / 39.0f));

	// 3. ISOLATE DUAL OPPOSING PERPENDICULAR PHASES (+15 AND -15)
	int left_perp_phase = (phase + TOTAL_HAND_PHASES - 15) % TOTAL_HAND_PHASES;
	int right_perp_phase = (phase + 15) % TOTAL_HAND_PHASES;

	// Fetch separate directional trajectories straight from the grid matrix
	float left_perp_dx = (float) HAND_MASTER_OFFSETS[left_perp_phase].dx;
	float left_perp_dy = (float) HAND_MASTER_OFFSETS[left_perp_phase].dy;

	float right_perp_dx = (float) HAND_MASTER_OFFSETS[right_perp_phase].dx;
	float right_perp_dy = (float) HAND_MASTER_OFFSETS[right_perp_phase].dy;

	// Scale outward symmetrically relative to the base center node
	float half_base_width = base_width * 0.5f;

	float rel_left_x = left_perp_dx * (half_base_width / 39.0f);
	float rel_left_y = left_perp_dy * (half_base_width / 39.0f);

	float rel_right_x = right_perp_dx * (half_base_width / 39.0f);
	float rel_right_y = right_perp_dy * (half_base_width / 39.0f);

	// 4. Map final vertex bounds ensuring opposing forces balance layout distortion
	out_vertices[1].x = SymmetricCrossRound(base_center_xf, rel_left_x, left_perp_dx);
	out_vertices[1].y = SymmetricCrossRound(base_center_yf, rel_left_y, left_perp_dy);

	out_vertices[2].x = SymmetricCrossRound(base_center_xf, rel_right_x, right_perp_dx);
	out_vertices[2].y = SymmetricCrossRound(base_center_yf, rel_right_y, right_perp_dy);
}

/**
 * Step 1: Topology Analysis Pass.
 * Computes exact edge-aligned miter vectors directly from the discrete 1x vertices.
 * Extrudes corners symmetrically along vertex normals without relying on continuous lookups.
 */
void Triangle_GetMinkowskiBiases(const gl_Vertex v1x[3], float scale, gl_Vec2 out_biases[3]) {
	float push_amt = (scale - 1.0f) * 0.5f;

	// Iterate through all 3 corners to find edge-aligned displacements
	for (int i = 0; i < 3; i++) {
		int prev = (i + 2) % 3;
		int next = (i + 1) % 3;

		// Vector 1: Current point to previous point
		float e1_x = (float) v1x[prev].x - (float) v1x[i].x;
		float e1_y = (float) v1x[prev].y - (float) v1x[i].y;
		float len1 = sqrtf(e1_x * e1_x + e1_y * e1_y);
		if (len1 > 0.001f) {
			e1_x /= len1;
			e1_y /= len1;
		}

		// Vector 2: Current point to next point
		float e2_x = (float) v1x[next].x - (float) v1x[i].x;
		float e2_y = (float) v1x[next].y - (float) v1x[i].y;
		float len2 = sqrtf(e2_x * e2_x + e2_y * e2_y);
		if (len2 > 0.001f) {
			e2_x /= len2;
			e2_y /= len2;
		}

		// The vertex miter normal is the normalized inversion of the edge angle bisector
		float miter_x = -(e1_x + e2_x);
		float miter_y = -(e1_y + e2_y);
		float len_miter = sqrtf(miter_x * miter_x + miter_y * miter_y);
		if (len_miter > 0.001f) {
			miter_x /= len_miter;
			miter_y /= len_miter;
		}

		// Compute the scaling component matching the inner angle expansion rules
		float inner_dot = (e1_x * e2_x) + (e1_y * e2_y);
		float sin_half_angle = sqrtf((1.0f - inner_dot) * 0.5f);
		float miter_scale = (sin_half_angle > 0.1f) ? (1.0f / sin_half_angle) : 1.0f;

		// Map the expanded biases directly to the output structures
		out_biases[i].x = miter_x * miter_scale * push_amt;
		out_biases[i].y = miter_y * miter_scale * push_amt;
	}
}

void Triangle_ScaleToFixedPoint(const gl_Vertex v1x[3], const gl_Vec2 biases[3], int pivot_x,
								int pivot_y, float scale, gl_Vertex out_v24_8[3]) {
	// Convert absolute integer pivot coordinates to 24.8 fixed point base anchors
	int32_t f_pivot_x = INT_TO_FIXED24_8(pivot_x);
	int32_t f_pivot_y = INT_TO_FIXED24_8(pivot_y);

	// Calculate the subpixel center-offset of the scaled block canvas space.
	// For 1.0x scale, this is 0.0f. For 3.0x scale, this is exactly 1.0f pixel unit.
	float cell_block_offset = (scale - 1.0f) * 0.5f;

	for (int i = 0; i < 3; i++) {
		// Compute discrete pixel steps relative to the unscaled pivot boundaries
		int32_t dx_1x = v1x[i].x - PIVOT_AXIS_X;
		int32_t dy_1x = v1x[i].y - PIVOT_AXIS_Y;

		// Scale the discrete deltas uniformly out into target canvas space
		float raw_scaled_dx = (float) dx_1x * scale;
		float raw_scaled_dy = (float) dy_1x * scale;

		// Shift baseline to the true cell block center, then inject Minkowski bias offsets
		float final_dx = raw_scaled_dx + cell_block_offset + biases[i].x;
		float final_dy = raw_scaled_dy + cell_block_offset + biases[i].y;

		// Pack final localized offsets directly into fixed-point vertex primitive matrices
		out_v24_8[i].x = f_pivot_x + FLOAT_TO_FIXED24_8(final_dx);
		out_v24_8[i].y = f_pivot_y + FLOAT_TO_FIXED24_8(final_dy);
	}
}

void CatClock_ShaderHands(void* renderer, int cell_x, int cell_y, int sheet_w, int sheet_h,
						  int frame_idx, void* userdata) {
	uint8_t* buffer = (uint8_t*) renderer;

	struct {
		int type;
		SDL_Color color;
	}* hand_cfg = (typeof(hand_cfg)) userdata;

	float scale = (float) ctx.current_half_steps / 2.0f;

	uint8_t palette_hand_idx = PALETTE_HAND_SECOND;
	int hand_type = hand_cfg ? hand_cfg->type : 0;
	// float mid_factor = 0.45f; // TBD hack for shoulders in 1x triangle if it collapses when
	// rendering
	if (hand_type == HAND_TYPE_HOUR) {
		palette_hand_idx = PALETTE_HAND_HOUR;
		// mid_factor = 0.22f;
	} else if (hand_type == HAND_TYPE_MINUTE) {
		palette_hand_idx = PALETTE_HAND_MINUTE;
		// mid_factor = 0.33f;
	}

	/* ------------------------------------------------------------------------
	   ATLAS RE-CENTERING STAGE
	   ------------------------------------------------------------------------ */
	// KEEP THE MASTER PIVOT AT THE TOP-LEFT CORNER GRID BOUNDARY
	// Do not inject the cell_block_offset here, as it will double-drift the matrix downstream
	float px_f = (float) cell_x + ((float) PIVOT_AXIS_X * scale);
	float py_f = (float) cell_y + ((float) PIVOT_AXIS_Y * scale);

	int px = (int) roundf(px_f);
	int py = (int) roundf(py_f);

	/* ------------------------------------------------------------------------
	   GEOMETRY MAPPING
	   ------------------------------------------------------------------------ */
	gl_Vertex vertices1x[3];
	CatClock_ComputeReferenceHandVertices(0, 0, hand_type, frame_idx, vertices1x);

	gl_Vec2 minkowski_biases[3];
	Triangle_GetMinkowskiBiases(vertices1x, scale, minkowski_biases);

	gl_Vertex vertices24_8[3];

	// Apply the single cell_block_offset to the relative deltas
	Triangle_ScaleToFixedPoint(vertices1x, minkowski_biases, px, py, scale, vertices24_8);

	/* ------------------------------------------------------------------------
	   RENDERING
	   ------------------------------------------------------------------------ */
#if (1)
	DrawTriangleFixedSDF(buffer, vertices24_8[0].x, vertices24_8[0].y, vertices24_8[1].x,
						 vertices24_8[1].y, vertices24_8[2].x, vertices24_8[2].y, sheet_w, sheet_h,
						 palette_hand_idx);
	/*
	DrawTriangleFixed(buffer, vertices24_8[0].x, vertices24_8[0].y, vertices24_8[1].x,
					  vertices24_8[1].y, vertices24_8[2].x, vertices24_8[2].y, sheet_w, sheet_h,
					  palette_hand_idx);
	*/
	/*
	DrawTriangleFloat(buffer, (float) vertices24_8[0].x / 256.0f,
					  (float) vertices24_8[0].y / 256.0f, (float) vertices24_8[1].x / 256.0f,
					  (float) vertices24_8[1].y / 256.0f, (float) vertices24_8[2].x / 256.0f,
					  (float) vertices24_8[2].y / 256.0f, sheet_w, sheet_h, palette_hand_idx);
	*/
#else
	/* -------------------------------------------------------------------------
	   DIAGNOSTIC PRINT TELEMETRY
	   ------------------------------------------------------------------------- */
	int phase = frame_idx % TOTAL_HAND_PHASES;
	gl_Vertex plotted_vertices[3];
	plotted_vertices[0].x = (vertices24_8[0].x + 128) >> 8;
	plotted_vertices[0].y = (vertices24_8[0].y + 128) >> 8;
	plotted_vertices[1].x = (vertices24_8[1].x + 128) >> 8;
	plotted_vertices[1].y = (vertices24_8[1].y + 128) >> 8;
	plotted_vertices[2].x = (vertices24_8[2].x + 128) >> 8;
	plotted_vertices[2].y = (vertices24_8[2].y + 128) >> 8;

	if (phase == 0 || phase == 1 || phase == 15 || phase == 30) {
		printf("[Output-State][Phase:%2d] Scale:%0.2f | Core Metric Pivot:(%3d,%3d)\n", phase,
			   scale, px, py);
		printf("  -> Final 24.8 Absolute V0(Tip) : (%5.2f, %5.2f) -> Plotted Pixel: (%3d, %3d)\n",
			   (float) vertices24_8[0].x / 256.0f, (float) vertices24_8[0].y / 256.0f,
			   plotted_vertices[0].x, plotted_vertices[0].y);
		printf("  -> Final 24.8 Absolute V1(Left) : (%5.2f, %5.2f) -> Plotted Pixel: (%3d, %3d)\n",
			   (float) vertices24_8[1].x / 256.0f, (float) vertices24_8[1].y / 256.0f,
			   plotted_vertices[1].x, plotted_vertices[1].y);
		printf("  -> Final 24.8 Absolute V2(Right): (%5.2f, %5.2f) -> Plotted Pixel: (%3d, %3d)\n",
			   (float) vertices24_8[2].x / 256.0f, (float) vertices24_8[2].y / 256.0f,
			   plotted_vertices[2].x, plotted_vertices[2].y);
		printf("\n");
	}

	/* -------------------------------------------------------------------------
	   DIAGNOSTIC DRAWING TELEMETRY
	   ------------------------------------------------------------------------- */
	// Calculate the continuous coordinates of the tip vertex (V0) directly from fixed-point
	float tip_xf = (float) vertices24_8[0].x / 256.0f;
	float tip_yf = (float) vertices24_8[0].y / 256.0f;

	// Isolate the fractional component of the continuous coordinates
	float tip_frac_x = tip_xf - floorf(tip_xf);
	float tip_frac_y = tip_yf - floorf(tip_yf);

	// Plot the standard rounded primary tip pixel
	PlotSoftwarePixel(buffer, plotted_vertices[0].x, plotted_vertices[0].y, sheet_w, sheet_h,
					  palette_hand_idx);

	// 2x Validation Splitting Pass: Detect half-pixel alignment boundaries
	if (scale == 2.0f || (fabsf(scale - floorf(scale)) > 0.01f)) {
		// If the X coordinate sits perfectly on a half-pixel seam (e.g., Phase 0 pointing up)
		if (fabsf(tip_frac_x - 0.5f) < 0.1f) {
			// Extrude an adjacent horizontal pixel to form a balanced 2-pixel wide tip
			int extra_tip_x = (tip_xf > (float) plotted_vertices[0].x) ? plotted_vertices[0].x + 1
																	   : plotted_vertices[0].x - 1;
			PlotSoftwarePixel(buffer, extra_tip_x, plotted_vertices[0].y, sheet_w, sheet_h,
							  palette_hand_idx);

			if (phase == 0) {
				printf("[Validation][Tip:2x] Horizontal split detected. Plotted 2px wide tip at X: "
					   "%d and %d\n",
					   plotted_vertices[0].x, extra_tip_x);
			}
		}
		// If the Y coordinate sits perfectly on a half-pixel seam (e.g., Phase 15 pointing
		// horizontally)
		if (fabsf(tip_frac_y - 0.5f) < 0.1f) {
			// Extrude an adjacent vertical pixel to maintain thickness consistency
			int extra_tip_y = (tip_yf > (float) plotted_vertices[0].y) ? plotted_vertices[0].y + 1
																	   : plotted_vertices[0].y - 1;
			PlotSoftwarePixel(buffer, plotted_vertices[0].x, extra_tip_y, sheet_w, sheet_h,
							  palette_hand_idx);
		}
	}
	PlotSoftwarePixel(buffer, plotted_vertices[1].x, plotted_vertices[1].y, sheet_w, sheet_h,
					  palette_hand_idx);
	PlotSoftwarePixel(buffer, plotted_vertices[2].x, plotted_vertices[2].y, sheet_w, sheet_h,
					  palette_hand_idx);

	/* -------------------------------------------------------------------------
		SUBPIXEL CENTER-PIN CLUSTER RASTERIZATION PASS
	   ------------------------------------------------------------------------- */
	// Plot the standard rounded primary apex pixel
	float cell_block_offset = (scale - 1.0f) * 0.5f;
	float final_pin_xf = (float) px + cell_block_offset;
	float final_pin_yf = (float) py + cell_block_offset;

	// Check if the current scale produces a perfect fractional 0.5 pixel split
	float internal_part_x = final_pin_xf - floorf(final_pin_xf);
	float internal_part_y = final_pin_yf - floorf(final_pin_yf);

	if (scale == 2.0f
		|| (fabsf(internal_part_x - 0.5f) < 0.01f && fabsf(internal_part_y - 0.5f) < 0.01f)) {
		// Even-scale workaround: Extrude a 4-pixel sub-grid around the center boundary
		int base_x = (int) floorf(final_pin_xf);
		int base_y = (int) floorf(final_pin_yf);

		PlotSoftwarePixel(buffer, base_x, base_y, sheet_w, sheet_h, palette_hand_idx);
		PlotSoftwarePixel(buffer, base_x + 1, base_y, sheet_w, sheet_h, palette_hand_idx);
		PlotSoftwarePixel(buffer, base_x, base_y + 1, sheet_w, sheet_h, palette_hand_idx);
		PlotSoftwarePixel(buffer, base_x + 1, base_y + 1, sheet_w, sheet_h, palette_hand_idx);

		if (phase == 0) {
			printf("[Validation][Scale:2x] Split-pixel center cluster plotted at base index (%d, "
				   "%d)\n",
				   base_x, base_y);
		}
	} else {
		// Odd scale or fractional fallback: standard clean midpoint rounding
		int pin_offset = (int) cell_block_offset;
		PlotSoftwarePixel(buffer, px + pin_offset, py + pin_offset, sheet_w, sheet_h,
						  palette_hand_idx);
	}
#endif
}
