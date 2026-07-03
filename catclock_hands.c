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
#include <stdio.h>

const int TEXTURE_CELL_W = 64;
const int TEXTURE_CELL_H = 96;
const int RELATIVE_FOCAL_X = 29;
const int RELATIVE_FOCAL_Y = 39;
const int ENVELOPE_PAD_X = 2;
const int ENVELOPE_PAD_Y = 6;
const int PIVOT_AXIS_X = ENVELOPE_PAD_X + RELATIVE_FOCAL_X;
const int PIVOT_AXIS_Y = ENVELOPE_PAD_Y + RELATIVE_FOCAL_Y;

// ========================================================================
// CONFIGURATION ROUTER
// 0 = WIP PRODUCTION WORK SHADER (ACTUAL RASTER PIPELINE)
// 1 = CENTERED TRIANGLE TEST
// 2 = PIVOT-ANCHORED TRIANGLE TEST
// 3 = CENTERED DIAMOND RASTER TEST
// 4 = EXPLICIT CCW DIAMOND RASTER TEST
// 5 = CROSS TEST (LINE TEST)
// 6 = ISOLATION TEST A: PURE COUNTER-CLOCKWISE (CCW) VERTEX ORDER
// 7 = ISOLATION TEST B: PURE CLOCKWISE (CW) VERTEX ORDER (FORCES ROTATION)
// 8 = ISOLATION TEST C: SUBPIXEL SNAP AT EXACT EXPLICIT INTEGER BOUNDS
// 9 = ISOLATION TEST D: FRACTIONAL BIAS OFFSET TEST (+0.5 PIXEL SHIFT)
// ========================================================================
#ifndef TEST_MODE
#define TEST_MODE 0
#endif

typedef struct {
	int dx; // Exact 1x horizontal pixel offset from focal center
	int dy; // Exact 1x vertical pixel offset from focal center
} HandMasterOffset;

static const HandMasterOffset HAND_MASTER_OFFSETS[TOTAL_HAND_PHASES]
	= { { 0, -39 },	  { 4, -39 },	{ 8, -38 },	  { 12, -37 },	{ 15, -35 },  { 19, -32 },
		{ 22, -30 },  { 24, -27 },	{ 26, -24 },  { 27, -20 },	{ 28, -16 },  { 29, -13 },
		{ 29, -10 },  { 29, -6 },	{ 29, -3 },	  { 29, 0 },	{ 29, 3 },	  { 29, 6 },
		{ 29, 10 },	  { 29, 13 },	{ 29, 17 },	  { 29, 21 },	{ 28, 25 },	  { 26, 29 },
		{ 23, 32 },	  { 20, 36 },	{ 17, 38 },	  { 13, 40 },	{ 9, 42 },	  { 5, 43 },
		{ 0, 43 },	  { -5, 43 },	{ -9, 42 },	  { -13, 40 },	{ -17, 38 },  { -20, 35 },
		{ -23, 32 },  { -26, 29 },	{ -28, 25 },  { -29, 21 },	{ -29, 17 },  { -29, 13 },
		{ -29, 10 },  { -29, 6 },	{ -29, 3 },	  { -29, 0 },	{ -29, -3 },  { -29, -6 },
		{ -29, -10 }, { -29, -13 }, { -28, -16 }, { -27, -20 }, { -26, -24 }, { -24, -26 },
		{ -22, -30 }, { -19, -33 }, { -15, -35 }, { -12, -37 }, { -8, -38 },  { -4, -39 } };

// Forward declarations sharing an identical layout payload profile
static void DrawProductionFrame(uint8_t* buffer, int sheet_w, int sheet_h, int frame_idx,
								float scale, float px_f, float py_f, int px, int py,
								uint8_t hand_color, uint8_t hand_halo);

static void DrawTestFrame(uint8_t* buffer, int sheet_w, int sheet_h, int frame_idx, float scale,
						  float px_f, float py_f, int px, int py, uint8_t hand_color,
						  uint8_t hand_halo);

// ============================================================================
// DRIFT-FREE GEOMETRIC GENERATION WRAPPERS
// ============================================================================

/**
 * TIER 1: CORE GEOMETRIC FUNCTION (Floating-Point)
 * Computes pure continuous geometric endpoints.
 */
static void CatClock_CalculateHandEndpointsFloat(int phase, float pivot_x, float pivot_y,
												 float scale, float* out_end_x, float* out_end_y) {
	if (phase < 0 || phase >= TOTAL_HAND_PHASES) {
		if (out_end_x)
			*out_end_x = pivot_x;
		if (out_end_y)
			*out_end_y = pivot_y;
		return;
	}

	HandMasterOffset offset = HAND_MASTER_OFFSETS[phase];

	if (out_end_x) {
		*out_end_x = pivot_x + ((float) offset.dx * scale);
	}
	if (out_end_y) {
		*out_end_y = pivot_y + ((float) offset.dy * scale);
	}
}

/**
 * TIER 2: GEOMETRIC WRAPPER (Integer Pass-through)
 * Exposes raw un-biased scale metrics into integer coordinate properties.
 */
__attribute__((unused)) static void
CatClock_CalculateHandEndpoints(int phase, int pivot_x, int pivot_y, float scale, int cell_x,
								int cell_y, int* out_end_x, int* out_end_y) {
	float float_out_x = 0.0f;
	float float_out_y = 0.0f;

	CatClock_CalculateHandEndpointsFloat(phase, (float) pivot_x, (float) pivot_y, scale,
										 &float_out_x, &float_out_y);

	if (out_end_x)
		*out_end_x = (int) roundf(float_out_x);
	if (out_end_y)
		*out_end_y = (int) roundf(float_out_y);

	(void) cell_x;
	(void) cell_y;
}

/**
 * TIER 3: RASTERIZER COMPENSATION WRAPPER
 * Direct master asset extraction layout loop. Performs scale mapping first, then
 * rounds to the nearest grid step using roundf() to preserve geometric symmetry.
 */
static void CatClock_CalculateHandEndpointsRaster(int phase, float pivot_xf, float pivot_yf,
												  float scale, int* out_piv_x, int* out_piv_y,
												  int* out_end_x, int* out_end_y) {
	int final_piv_x = (int) floorf(pivot_xf + 0.5f);
	int final_piv_y = (int) floorf(pivot_yf + 0.5f);

	int safe_phase = phase % TOTAL_HAND_PHASES;
	if (safe_phase < 0)
		safe_phase += TOTAL_HAND_PHASES;
	HandMasterOffset offset = HAND_MASTER_OFFSETS[safe_phase];

	int scaled_dx = (int) roundf((float) offset.dx * scale);
	int scaled_dy = (int) roundf((float) offset.dy * scale);

	if (out_piv_x)
		*out_piv_x = final_piv_x;
	if (out_piv_y)
		*out_piv_y = final_piv_y;
	if (out_end_x)
		*out_end_x = final_piv_x + scaled_dx;
	if (out_end_y)
		*out_end_y = final_piv_y + scaled_dy;
}

// ============================================================================
// MAIN PIPELINE ENTRY INTERFACE
// ============================================================================
void CatClock_ShaderHands(void* renderer, int cell_x, int cell_y, int sheet_w, int sheet_h,
						  int frame_idx, void* userdata) {
	uint8_t* buffer = (uint8_t*) renderer;

	// Unified Core Metric Pipeline
	float scale = (float) ctx.current_half_steps / 2.0f;
	float px_f = (float) cell_x + ((float) PIVOT_AXIS_X * scale);
	float py_f = (float) cell_y + ((float) PIVOT_AXIS_Y * scale);
	int px = (int) roundf(px_f);
	int py = (int) roundf(py_f);

	// Unified Configuration & Palette Block
	struct {
		int type;
		SDL_Color color;
	}* hand_cfg = (typeof(hand_cfg)) userdata;

	int hand_type = hand_cfg ? hand_cfg->type : 0;
	uint8_t hand_color = PALETTE_HAND_SECOND;
	uint8_t hand_halo = PALETTE_HALO;

	if (hand_type == HAND_TYPE_HOUR) {
		hand_color = PALETTE_HAND_HOUR;
	} else if (hand_type == HAND_TYPE_MINUTE) {
		hand_color = PALETTE_HAND_MINUTE;
	}

	// Interchangeable calling interface pattern
	if (TEST_MODE == 0) {
		DrawProductionFrame(buffer, sheet_w, sheet_h, frame_idx, scale, px_f, py_f, px, py,
							hand_color, hand_halo);
	} else if (frame_idx == 0 && hand_type == HAND_TYPE_SECOND) {
		DrawTestFrame(buffer, sheet_w, sheet_h, frame_idx, scale, px_f, py_f, px, py, hand_color,
					  hand_halo);
	}
}

// ============================================================================
// PRODUCTION SHADER PIPELINE EXECUTION
// ============================================================================
static void DrawProductionFrame(uint8_t* buffer, int sheet_w, int sheet_h, int frame_idx,
								float scale, float px_f, float py_f, int px, int py,
								uint8_t hand_color, uint8_t hand_halo) {
	(void) px;
	(void) py;
	(void) hand_halo; // Voiding metrics not utilized by the raster line compiler path

	int target_piv_x, target_piv_y, target_end_x, target_end_y;
	int phase = frame_idx % TOTAL_HAND_PHASES;

	CatClock_CalculateHandEndpointsRaster(phase, px_f, py_f, scale, &target_piv_x, &target_piv_y,
										  &target_end_x, &target_end_y);

	int int_scale_factor = (int) floorf(scale);
	if (int_scale_factor < 1) {
		int_scale_factor = 1;
	}

	DrawLineLikeMesa(buffer, sheet_w, sheet_h, (float) target_piv_x, (float) target_piv_y,
					 (float) target_end_x, (float) target_end_y, (float) int_scale_factor,
					 hand_color);
}

// ============================================================================
// MODULAR REFACTOR TEST BENCH FRAMEWORK
// ============================================================================
static void DrawTestFrame(uint8_t* buffer, int sheet_w, int sheet_h, int frame_idx, float scale,
						  float px_f, float py_f, int px, int py, uint8_t hand_color,
						  uint8_t hand_halo) {
	// Structural logging headers for cross-thread parsers
	printf("[DIAG-FRAME-START] FrameIdx: %d | Scale: %.4f\n", frame_idx, scale);
	printf("[DIAG-FRAME-PIVOT] Float: (%.4f, %.4f) | Rounded: (%d, %d)\n", px_f, py_f, px, py);
	printf("[DIAG-FRAME-CONFIG] Target Colors - Color: %u, Halo: %u\n", hand_color, hand_halo);

#if (TEST_MODE == 1)
	// ====================================================================
	// CENTERED TRIANGLE TEST
	// ====================================================================
	/*
	 * Continuous bounds under 2x layout context:
	 * v0: (42.0, 70.0) | v1: (82.0, 70.0) | v2: (42.0, 110.0)
	 *
	 * Continuous lengths of the orthogonal components:
	 * \Delta X = 82.0 - 42.0 = 40.0 continuous pixels
	 * \Delta Y = 110.0 - 70.0 = 40.0 continuous pixels
	 *
	 * Mesa/OpenGL Rule Mapping Constraints:
	 * 1. The Shared Edge Orientation: The diagonal hypotenuse steps from
	 *    top-right (82.0, 70.0) down to bottom-left (42.0, 110.0).
	 * 2. Left-Edge Inclusions: Shared boundary segments classified as
	 *    left-edges or horizontal top-edges belong inclusively to that primitive.
	 * 3. The Ownership Mapping: For Primitive 1 (upper-left), this diagonal
	 *    forms its bounding right/bottom-right edge constraint. For Primitive 2
	 *    (lower-right), it forms its left/bottom-left constraint boundary.
	 *
	 * Because left-edges are inclusive under Mesa rules, Primitive 2 (lower-right)
	 * captures the shared diagonal boundary pixels entirely. This forces the
	 * lower primitive to fill completely to a full 40 pixels wide along the Y=109 row,
	 * while Primitive 1 steps back exclusively, finishing at exactly 39 pixels wide.
	 */
	float half_size_f = 10.0f * scale;
	// Shift the baseline pivot to a pixel center space
	float center_x = floorf(px_f) + 0.5f;
	float center_y = floorf(py_f) + 0.5f;

	float x_min_f = center_x - half_size_f;
	float x_max_f = center_x + half_size_f;
	float y_min_f = center_y - half_size_f;
	float y_max_f = center_y + half_size_f;

	printf("[TEST-1-INPUT] Centered Triangle Primitive Configuration:\n");
	printf("  Tri1 vertices -> v0:(%.4f, %.4f) v1:(%.4f, %.4f) v2:(%.4f, %.4f)\n", x_min_f, y_min_f,
		   x_max_f, y_min_f, x_min_f, y_max_f);
	printf("  Tri2 vertices -> v0:(%.4f, %.4f) v1:(%.4f, %.4f) v2:(%.4f, %.4f)\n", x_max_f, y_min_f,
		   x_max_f, y_max_f, x_min_f, y_max_f);

	// Convert old continuous bounds to pure fixed-viewport NDC coordinate values:
	float v0x_ndc = (x_min_f / ((float) sheet_w / 2.0f)) - 1.0f;
	float v0y_ndc = 1.0f - (y_min_f / ((float) sheet_h / 2.0f));

	float v1x_ndc = (x_max_f / ((float) sheet_w / 2.0f)) - 1.0f;
	float v1y_ndc = 1.0f - (y_min_f / ((float) sheet_h / 2.0f));

	float v2x_ndc = (x_min_f / ((float) sheet_w / 2.0f)) - 1.0f;
	float v2y_ndc = 1.0f - (y_max_f / ((float) sheet_h / 2.0f));

	float v3x_ndc = (x_max_f / ((float) sheet_w / 2.0f)) - 1.0f;
	float v3y_ndc = 1.0f - (y_max_f / ((float) sheet_h / 2.0f));

	DrawTriangleLikeMesa(buffer, sheet_w, sheet_h, v0x_ndc, v0y_ndc, v1x_ndc, v1y_ndc, v2x_ndc,
						 v2y_ndc, hand_color);
	DrawTriangleLikeMesa(buffer, sheet_w, sheet_h, v1x_ndc, v1y_ndc, v3x_ndc, v3y_ndc, v2x_ndc,
						 v2y_ndc, hand_halo);

#elif (TEST_MODE == 2)
	float size = 20.0f * scale;
	float x1 = px_f + size;
	float x2 = px_f;
	float x3 = px_f + size;
	float y3 = py_f + size;

	printf("[TEST-2-INPUT] Pivot-Anchored Triangle Configuration:\n");
	printf("  Tri1 vertices -> v0:(%.4f, %.4f) v1:(%.4f, %.4f) v2:(%.4f, %.4f)\n", px_f, py_f, x1,
		   py_f, x2, py_f + size);
	printf("  Tri2 vertices -> v0:(%.4f, %.4f) v1:(%.4f, %.4f) v2:(%.4f, %.4f)\n", x1, py_f, x3, y3,
		   x2, py_f + size);

	// Convert positions to Normalized Device Coordinates (NDC) bounds
	float v0x_ndc = (px_f / ((float) sheet_w / 2.0f)) - 1.0f;
	float v0y_ndc = 1.0f - (py_f / ((float) sheet_h / 2.0f));

	float v1x_ndc = (x1 / ((float) sheet_w / 2.0f)) - 1.0f;
	float v1y_ndc = 1.0f - (py_f / ((float) sheet_h / 2.0f));

	float v2x_ndc = (x2 / ((float) sheet_w / 2.0f)) - 1.0f;
	float v2y_ndc = 1.0f - ((py_f + size) / ((float) sheet_h / 2.0f));

	float v3x_ndc = (x3 / ((float) sheet_w / 2.0f)) - 1.0f;
	float v3y_ndc = 1.0f - (y3 / ((float) sheet_h / 2.0f));

	DrawTriangleLikeMesa(buffer, sheet_w, sheet_h, v0x_ndc, v0y_ndc, v1x_ndc, v1y_ndc, v2x_ndc,
						 v2y_ndc, hand_color);
	DrawTriangleLikeMesa(buffer, sheet_w, sheet_h, v1x_ndc, v1y_ndc, v3x_ndc, v3y_ndc, v2x_ndc,
						 v2y_ndc, hand_halo);

#elif (TEST_MODE == 3)
	// ====================================================================
	// CENTERED DIAMOND RASTER TEST
	// ====================================================================
	int radius = 15 * (int) roundf(scale);
	float px_float = (float) px;
	float py_float = (float) py;
	float r_float = (float) radius;

	printf("[TEST-3-INPUT] Centered Diamond Configuration (Integer Anchored):\n");
	printf("  Anchor Pixel PX: %d, PY: %d | Computed Radius: %d\n", px, py, radius);

	// Map vertex positions into standard NDC target coordinates
	float top_x_ndc = (px_float / ((float) sheet_w / 2.0f)) - 1.0f;
	float top_y_ndc = 1.0f - ((py_float - r_float) / ((float) sheet_h / 2.0f));

	float right_x_ndc = ((px_float + r_float) / ((float) sheet_w / 2.0f)) - 1.0f;
	float right_y_ndc = 1.0f - (py_float / ((float) sheet_h / 2.0f));

	float bottom_x_ndc = (px_float / ((float) sheet_w / 2.0f)) - 1.0f;
	float bottom_y_ndc = 1.0f - ((py_float + r_float) / ((float) sheet_h / 2.0f));

	float left_x_ndc = ((px_float - r_float) / ((float) sheet_w / 2.0f)) - 1.0f;
	float left_y_ndc = 1.0f - (py_float / ((float) sheet_h / 2.0f));

	DrawTriangleLikeMesa(buffer, sheet_w, sheet_h, top_x_ndc, top_y_ndc, right_x_ndc, right_y_ndc,
						 bottom_x_ndc, bottom_y_ndc, hand_color);
	DrawTriangleLikeMesa(buffer, sheet_w, sheet_h, top_x_ndc, top_y_ndc, bottom_x_ndc, bottom_y_ndc,
						 left_x_ndc, left_y_ndc, hand_halo);

#elif (TEST_MODE == 4)
	// ====================================================================
	// EXPLICIT CCW DIAMOND RASTER TEST
	// ====================================================================
	int radius = 15 * (int) roundf(scale);
	float px_float = (float) px;
	float py_float = (float) py;
	float r_float = (float) radius;

	printf("[TEST-4-INPUT] Explicit CCW Diamond Configuration:\n");

	float top_x_ndc = (px_float / ((float) sheet_w / 2.0f)) - 1.0f;
	float top_y_ndc = 1.0f - ((py_float - r_float) / ((float) sheet_h / 2.0f));

	float right_x_ndc = ((px_float + r_float) / ((float) sheet_w / 2.0f)) - 1.0f;
	float right_y_ndc = 1.0f - (py_float / ((float) sheet_h / 2.0f));

	float bottom_x_ndc = (px_float / ((float) sheet_w / 2.0f)) - 1.0f;
	float bottom_y_ndc = 1.0f - ((py_float + r_float) / ((float) sheet_h / 2.0f));

	float left_x_ndc = ((px_float - r_float) / ((float) sheet_w / 2.0f)) - 1.0f;
	float left_y_ndc = 1.0f - (py_float / ((float) sheet_h / 2.0f));

	DrawTriangleLikeMesa(buffer, sheet_w, sheet_h, top_x_ndc, top_y_ndc, left_x_ndc, left_y_ndc,
						 bottom_x_ndc, bottom_y_ndc, hand_color);
	DrawTriangleLikeMesa(buffer, sheet_w, sheet_h, top_x_ndc, top_y_ndc, bottom_x_ndc, bottom_y_ndc,
						 right_x_ndc, right_y_ndc, hand_halo);

#elif (TEST_MODE == 5)
	// ====================================================================
	// CROSS TEST (LINE TEST)
	// ====================================================================
	int size = 20 * (int) roundf(scale);
	int int_scale_factor = (int) floorf(scale);
	if (int_scale_factor < 1) {
		int_scale_factor = 1;
	}

	printf("[TEST-5-INPUT] Crosshair Line Vector Configuration:\n");
	printf("  Horiz Line -> x0:%.4f y0:%.4f | x1:%.4f y1:%.4f | Width: %d\n", (float) (px - size),
		   (float) py, (float) (px + size), (float) py, int_scale_factor);
	printf("  Vert Line  -> x0:%.4f y0:%.4f | x1:%.4f y1:%.4f | Width: %d\n", (float) px,
		   (float) (py - size), (float) px, (float) (py + size), int_scale_factor);

	DrawLineLikeMesa(buffer, sheet_w, sheet_h, (float) (px - size), (float) py, (float) (px + size),
					 (float) py, (float) int_scale_factor, hand_color);
	DrawLineLikeMesa(buffer, sheet_w, sheet_h, (float) px, (float) (py - size), (float) px,
					 (float) (py + size), (float) int_scale_factor, hand_color);

#elif (TEST_MODE == 6)
	// ====================================================================
	// ISOLATION TEST A: PURE COUNTER-CLOCKWISE (CCW) VERTEX ORDER
	// ====================================================================
	float offset = 15.0f * scale;
	float v0_x = (float) px, v0_y = (float) py;
	float v1_x = (float) px, v1_y = (float) py + offset;
	float v2_x = (float) px + offset, v2_y = (float) py + offset;

	printf("[TEST-6-INPUT] CCW Verification Triangle:\n");

	float v0x_ndc = (v0_x / ((float) sheet_w / 2.0f)) - 1.0f;
	float v0y_ndc = 1.0f - (v0_y / ((float) sheet_h / 2.0f));

	float v1x_ndc = (v1_x / ((float) sheet_w / 2.0f)) - 1.0f;
	float v1y_ndc = 1.0f - (v1_y / ((float) sheet_h / 2.0f));

	float v2x_ndc = (v2_x / ((float) sheet_w / 2.0f)) - 1.0f;
	float v2y_ndc = 1.0f - (v2_y / ((float) sheet_h / 2.0f));

	DrawTriangleLikeMesa(buffer, sheet_w, sheet_h, v0x_ndc, v0y_ndc, v1x_ndc, v1y_ndc, v2x_ndc,
						 v2y_ndc, hand_color);

#elif (TEST_MODE == 7)
	// ====================================================================
	// ISOLATION TEST B: PURE CLOCKWISE (CW) VERTEX ORDER
	// ====================================================================
	float offset = 15.0f * scale;
	float v0_x = (float) px, v0_y = (float) py;
	float v1_x = (float) px + offset, v1_y = (float) py + offset;
	float v2_x = (float) px, v2_y = (float) py + offset;

	printf("[TEST-7-INPUT] CW (Sorting Pipeline Target) Triangle:\n");

	float v0x_ndc = (v0_x / ((float) sheet_w / 2.0f)) - 1.0f;
	float v0y_ndc = 1.0f - (v0_y / ((float) sheet_h / 2.0f));

	float v1x_ndc = (v1_x / ((float) sheet_w / 2.0f)) - 1.0f;
	float v1y_ndc = 1.0f - (v1_y / ((float) sheet_h / 2.0f));

	float v2x_ndc = (v2_x / ((float) sheet_w / 2.0f)) - 1.0f;
	float v2y_ndc = 1.0f - (v2_y / ((float) sheet_h / 2.0f));

	DrawTriangleLikeMesa(buffer, sheet_w, sheet_h, v0x_ndc, v0y_ndc, v1x_ndc, v1y_ndc, v2x_ndc,
						 v2y_ndc, hand_color);

#elif (TEST_MODE == 8)
	// ====================================================================
	// ISOLATION TEST C: SUBPIXEL SNAP AT EXACT EXPLICIT INTEGER BOUNDS
	// ====================================================================
	float offset = 10.0f * scale;
	float v0_x = (float) px, v0_y = (float) py;
	float v1_x = (float) px + offset, v1_y = (float) py;
	float v2_x = (float) px, v2_y = (float) py + offset;

	printf("[TEST-8-INPUT] Integer Boundaries Grid Snap Alignment:\n");

	float v0x_ndc = (v0_x / ((float) sheet_w / 2.0f)) - 1.0f;
	float v0y_ndc = 1.0f - (v0_y / ((float) sheet_h / 2.0f));

	float v1x_ndc = (v1_x / ((float) sheet_w / 2.0f)) - 1.0f;
	float v1y_ndc = 1.0f - (v0_y / ((float) sheet_h / 2.0f));

	float v2x_ndc = (v0_x / ((float) sheet_w / 2.0f)) - 1.0f;
	float v2y_ndc = 1.0f - (v2_y / ((float) sheet_h / 2.0f));

	DrawTriangleLikeMesa(buffer, sheet_w, sheet_h, v0x_ndc, v0y_ndc, v1x_ndc, v1y_ndc, v2x_ndc,
						 v2y_ndc, hand_color);

#elif (TEST_MODE == 9)
	// ====================================================================
	// ISOLATION TEST D: FRACTIONAL BIAS OFFSET TEST (+0.5 PIXEL SHIFT)
	// ====================================================================
	float offset = 10.0f * scale;
	float v0_x = (float) px + 0.5f, v0_y = (float) py + 0.5f;
	float v1_x = (float) px + offset + 0.5f, v1_y = (float) py + 0.5f;
	float v2_x = (float) px + 0.5f, v2_y = (float) py + offset + 0.5f;

	printf("[TEST-9-INPUT] Fractional +0.5 Subpixel Shift Bias Alignment:\n");

	float v0x_ndc = (v0_x / ((float) sheet_w / 2.0f)) - 1.0f;
	float v0y_ndc = 1.0f - (v0_y / ((float) sheet_h / 2.0f));

	float v1x_ndc = (v1_x / ((float) sheet_w / 2.0f)) - 1.0f;
	float v1y_ndc = 1.0f - (v1_y / ((float) sheet_h / 2.0f));

	float v2x_ndc = (v2_x / ((float) sheet_w / 2.0f)) - 1.0f;
	float v2y_ndc = 1.0f - (v2_y / ((float) sheet_h / 2.0f));

	DrawTriangleLikeMesa(buffer, sheet_w, sheet_h, v0x_ndc, v0y_ndc, v1x_ndc, v1y_ndc, v2x_ndc,
						 v2y_ndc, hand_color);

#endif
// Place this in catclock_hands.c inside the (scale == 2.0000f) block
#if (TEST_MODE != 0)
	if (scale == 2.0000f) {
		int atlas_w = sheet_w;
		printf("\n[BUFFER-AUDIT] FINAL COMBINED FRAMEBUFFER (X[30..105], Y[55..130]):\n");

		// --- 1. HUNDREDS DIGIT LINE ---
		printf("         ");
		for (int test_x = 30; test_x <= 105; test_x++) {
			printf("%d", test_x / 100);
		}
		printf("\n");

		// --- 2. TENS DIGIT LINE ---
		printf("         ");
		for (int test_x = 30; test_x <= 105; test_x++) {
			printf("%d", (test_x / 10) % 10);
		}
		printf("\n");

		// --- 3. ONES DIGIT LINE ---
		printf("         ");
		for (int test_x = 30; test_x <= 105; test_x++) {
			printf("%d", test_x % 10);
		}
		printf("\n");

		// --- 4. PRINT BODY ROWS ---
		for (int test_y = 55; test_y <= 130; test_y++) {
			printf("Row %03d: ", test_y);
			for (int test_x = 30; test_x <= 105; test_x++) {
				uint32_t target_offset = test_y * atlas_w + test_x;
				uint8_t pixel_val = buffer[target_offset];

				if (pixel_val == hand_color) {
					printf("1");
				} else if (pixel_val == hand_halo) {
					printf("H");
				} else if (pixel_val != 0) {
					printf("X");
				} else {
					printf(".");
				}
			}
			printf("\n");
		}
		printf("[BUFFER-AUDIT] Final Frame Pass Complete\n\n");
	}
	printf("[DIAG-FRAME-STOP] FrameIdx: %d Execution Matrix End\n\n", frame_idx);
#endif
}
