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

#define STANDALONE_
#ifndef FALL_THROUGH
#if defined(__GNUC__) && __GNUC__ >= 7
#define FALL_THROUGH __attribute__((fallthrough))
#else
#define FALL_THROUGH ((void) 0)
#endif
#endif
#include "ftgrays.c"
#define STANDALONE_RASTER_POOL_SIZE (1024 * 128)

typedef struct {
	uint8_t* buffer;
	int width;
	int height;
	uint8_t token;
} FreetypeRenderTarget;

void DrawTriangleFreetype(uint8_t* buffer, int32_t fx_x0, int32_t fx_y0, int32_t fx_x1,
						  int32_t fx_y1, int32_t fx_x2, int32_t fx_y2, int width, int height,
						  uint8_t token);
void DrawCapsuleFreetype(uint8_t* buffer, int32_t fx_x0, int32_t fx_y0, int32_t fx_x1,
						 int32_t fx_y1, int32_t fx_x2, int32_t fx_y2, int width, int height,
						 uint8_t token);

#define FLOAT_TO_FIXED24_8(f) ((int32_t) ((f) * 256.0f))
#define INT_TO_FIXED24_8(i) ((int32_t) ((i) << 8))

/* Multiplies two 24.8 fixed-point numbers and maintains 24.8 scaling */
static inline int32_t FixedMul24_8(int32_t a, int32_t b) {
	return (int32_t) (((int64_t) a * b) >> 8);
}

/* Divides two 24.8 fixed-point numbers and maintains 24.8 scaling */
static inline int32_t FixedDiv24_8(int32_t num, int32_t den) {
	if (den == 0)
		return 0;
	return (int32_t) (((int64_t) num << 8) / den);
}

/* Precise integer square root for 24.8 fixed-point space */
static int32_t FixedSqrt24_8(int32_t val) {
	if (val <= 0)
		return 0;
	int64_t temp = (int64_t) val << 8;
	int64_t res = 0;
	int64_t bit = (int64_t) 1 << 30; // Stabilized mask initial allocation
	int security_counter = 0;
	while (bit > temp) {
		bit >>= 2;
		if (++security_counter > 64) {
			printf("[FATAL Sqrt] First while-loop mask allocation spiraled out.\n");
			break;
		}
	}
	security_counter = 0;
	while (bit != 0) {
		if (temp >= res + bit) {
			temp -= res + bit;
			res = (res >> 1) + bit;
		} else {
			res >>= 1;
		}
		bit >>= 2;
		if (++security_counter > 1000) {
			printf("[FATAL Sqrt] Second while-loop extraction trapped in an infinite sequence.\n");
			return -1; // Force escape
		}
	}
	return (int32_t) res;
}

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
Geometry Validation
========================================================================= */
#ifdef DEBUG_DUMP_HAND_VERTICIES
static void ValidateGeometryDebug(uint8_t* buffer, int sheet_w, int sheet_h, int cell_x, int cell_y,
								  int phase, int hand_type, float scale, int back_pivot_length,
								  gl_Vertex uncomp_vertices, gl_Vertex vertices24_8,
								  gl_Vertex back_center_fixed,
								  const gl_Vertex* biased_pipeline_pack,
								  uint8_t palette_hand_idx_org);
#endif

/* =========================================================================
GEOMETRY UTILITIES
========================================================================= */
static gl_Vertex CalculateBaselinePivot24_8(int cell_x, int cell_y, float scale) {
	gl_Vertex pivot;
	int32_t scale_24_8 = FLOAT_TO_FIXED24_8(scale);
	int32_t local_axis_x = INT_TO_FIXED24_8(PIVOT_AXIS_X) + 0x0080;
	int32_t local_axis_y = INT_TO_FIXED24_8(PIVOT_AXIS_Y) + 0x0080;
	pivot.x = INT_TO_FIXED24_8(cell_x) + FixedMul24_8(local_axis_x, scale_24_8);
	pivot.y = INT_TO_FIXED24_8(cell_y) + FixedMul24_8(local_axis_y, scale_24_8);
	return pivot;
}

void Triangle_ComputeTip(int cell_x, int cell_y, int length_multiplier, int phase_idx, float scale,
						 gl_Vertex* out_v24_8) {
	int phase = phase_idx % TOTAL_HAND_PHASES;
	int32_t scale_24_8 = FLOAT_TO_FIXED24_8(scale);
	int32_t length_multiplier_24_8
		= FixedDiv24_8(INT_TO_FIXED24_8(length_multiplier), INT_TO_FIXED24_8(7));
	int32_t master_dx = INT_TO_FIXED24_8(HAND_MASTER_OFFSETS[phase].dx);
	int32_t master_dy = INT_TO_FIXED24_8(HAND_MASTER_OFFSETS[phase].dy);
	int32_t dx_1x = FixedMul24_8(master_dx, length_multiplier_24_8);
	int32_t dy_1x = FixedMul24_8(master_dy, length_multiplier_24_8);
	gl_Vertex true_pivot = CalculateBaselinePivot24_8(cell_x, cell_y, scale);
	out_v24_8[0].x = true_pivot.x + FixedMul24_8(dx_1x, scale_24_8);
	out_v24_8[0].y = true_pivot.y + FixedMul24_8(dy_1x, scale_24_8);
}

void Triangle_ApplyMiterSharpenerTip(int phase_idx, float scale, const gl_Vertex* uncomp_vertices,
									 gl_Vertex* out_vertices24_8) {
	int phase = phase_idx % TOTAL_HAND_PHASES;
	int current_scale_int = (int) scale;
	int half_span = current_scale_int >> 1;
	int is_even_scale = (current_scale_int % 2 == 0);

	int32_t uncomp_base_x = uncomp_vertices->x >> 8;
	int32_t uncomp_base_y = uncomp_vertices->y >> 8;
	int master_dx = HAND_MASTER_OFFSETS[phase].dx;
	int master_dy = HAND_MASTER_OFFSETS[phase].dy;
	int final_pixel_x = uncomp_base_x;
	int final_pixel_y = uncomp_base_y;

	/* -------------------------------------------------------------------------
	BRANCH A: EVEN LAYERS CONSTRICTOR MATRIX (2x, 4x, 6x...)
	------------------------------------------------------------------------- */
	if (is_even_scale) {
		int32_t mx = INT_TO_FIXED24_8(master_dx);
		int32_t my = INT_TO_FIXED24_8(master_dy);
		int32_t sq_x = FixedMul24_8(mx, mx);
		int32_t sq_y = FixedMul24_8(my, my);
		int32_t len = FixedSqrt24_8(sq_x + sq_y);
		int32_t dir_x = 0, dir_y = -INT_TO_FIXED24_8(1);

		int32_t scale_minus_one;
		int32_t push_x;
		int32_t push_y;
		int32_t projected_fixed_x;
		int32_t projected_fixed_y;
		int target_x;
		int target_y;
		int hand_sgn_x;
		int hand_sgn_y;

		int min_allowed_dx;
		int max_allowed_dx;
		int min_allowed_dy;
		int max_allowed_dy;

		if (len > 0) {
			dir_x = FixedDiv24_8(mx, len);
			dir_y = FixedDiv24_8(my, len);
		}
		scale_minus_one = FLOAT_TO_FIXED24_8(scale) - INT_TO_FIXED24_8(1);
		push_x = FixedMul24_8(dir_x, scale_minus_one >> 1);
		push_y = FixedMul24_8(dir_y, scale_minus_one >> 1);

		projected_fixed_x = uncomp_vertices->x + push_x;
		projected_fixed_y = uncomp_vertices->y + push_y;
		target_x = projected_fixed_x >> 8;
		target_y = projected_fixed_y >> 8;
		// Ensure neutral axes stay zeroed out to prevent off-by-one directional drift
		hand_sgn_x = (master_dx > 0) ? 1 : ((master_dx < 0) ? -1 : 0);
		hand_sgn_y = (master_dy > 0) ? 1 : ((master_dy < 0) ? -1 : 0);

		if (abs(master_dx) >= abs(master_dy)) {
			if (target_y == uncomp_base_y && half_span > 1) {
				target_y += hand_sgn_y;
			}
		} else {
			if (target_x == uncomp_base_x && half_span > 1) {
				target_x += hand_sgn_x;
			}
		}

		min_allowed_dx = -half_span;
		max_allowed_dx = half_span - 1;
		min_allowed_dy = -half_span;
		max_allowed_dy = half_span - 1;

		if (target_x < uncomp_base_x + min_allowed_dx)
			target_x = uncomp_base_x + min_allowed_dx;
		if (target_x > uncomp_base_x + max_allowed_dx)
			target_x = uncomp_base_x + max_allowed_dx;
		if (target_y < uncomp_base_y + min_allowed_dy)
			target_y = uncomp_base_y + min_allowed_dy;
		if (target_y > uncomp_base_y + max_allowed_dy)
			target_y = uncomp_base_y + max_allowed_dy;

		final_pixel_x = target_x;
		final_pixel_y = target_y;

		/* -------------------------------------------------------------------------
		BRANCH B: ODD LAYERS PIECEWISE LAMÉ PROJECTION (1x, 3x, 5x...)
		------------------------------------------------------------------------- */
	} else {
		float angle = atan2f((float) master_dx, -(float) master_dy);
		float sin_a = sinf(angle);
		float cos_a = cosf(angle);

		float la = 30.4978f, lb = 39.6495f, ln = 2.4166f;
		float abs_cos;
		float abs_sin;
		float cos_factor;
		float sin_factor;
		float denom;

		int target_x;
		int target_y;

		int min_allowed_dx;
		int max_allowed_dx;
		int min_allowed_dy;
		int max_allowed_dy;

		if (cos_a >= 0.0f && sin_a >= 0.0f) {
			la = 30.4978f;
			lb = 39.6495f;
			ln = 2.4166f; // Q1 (Top-Right)
		} else if (cos_a < 0.0f && sin_a >= 0.0f) {
			la = 30.7448f;
			lb = 43.3683f;
			ln = 2.6605f; // Q2 (Bottom-Right)
		} else if (cos_a < 0.0f && sin_a < 0.0f) {
			la = 30.7717f;
			lb = 43.3313f;
			ln = 2.6324f; // Q3 (Bottom-Left)
		} else {
			la = 30.4939f;
			lb = 39.7917f;
			ln = 2.4017f; // Q4 (Top-Left)
		}
		abs_cos = fabsf(cos_a);
		abs_sin = fabsf(sin_a);
		cos_factor = (abs_cos > 1e-6f) ? powf(abs_cos, ln) : 0.0f;
		sin_factor = (abs_sin > 1e-6f) ? powf(abs_sin, ln) : 0.0f;

		denom = powf((cos_factor / powf(lb, ln)) + (sin_factor / powf(la, ln)), -1.0f / ln);
		(void) denom;

		target_x = uncomp_base_x;
		target_y = uncomp_base_y;

		if (abs(master_dx) >= abs(master_dy)) {
			int step_x = (master_dx >= 0) ? half_span : -half_span;
			target_x = uncomp_base_x + step_x;
			if (master_dx != 0) {
				float scale_ratio = (float) master_dy / (float) master_dx;
				target_y = uncomp_base_y + (int) roundf((float) step_x * scale_ratio);
			}
		} else {
			int step_y = (master_dy >= 0) ? half_span : -half_span;
			target_y = uncomp_base_y + step_y;
			if (master_dy != 0) {
				float scale_ratio = (float) master_dx / (float) master_dy;
				target_x = uncomp_base_x + (int) roundf((float) step_y * scale_ratio);
			}
		}

		min_allowed_dx = -half_span;
		max_allowed_dx = half_span;
		min_allowed_dy = -half_span;
		max_allowed_dy = half_span;

		if (target_x < uncomp_base_x + min_allowed_dx)
			target_x = uncomp_base_x + min_allowed_dx;
		if (target_x > uncomp_base_x + max_allowed_dx)
			target_x = uncomp_base_x + max_allowed_dx;
		if (target_y < uncomp_base_y + min_allowed_dy)
			target_y = uncomp_base_y + min_allowed_dy;
		if (target_y > uncomp_base_y + max_allowed_dy)
			target_y = uncomp_base_y + max_allowed_dy;

		final_pixel_x = target_x;
		final_pixel_y = target_y;
	}

	out_vertices24_8->x = (final_pixel_x << 8) | (uncomp_vertices->x & 0xFF);
	out_vertices24_8->y = (final_pixel_y << 8) | (uncomp_vertices->y & 0xFF);
}

/**
 * Computes the absolute 24.8 fixed-point coordinate of the rear base center point,
 * projected directly opposite to the hand's active directional rotation trajectory.
 */
gl_Vertex Triangle_ComputeBaseCenter24_8(int cell_x, int cell_y, int phase_idx, float scale,
										 int back_pivot_length) {
	int phase = phase_idx % TOTAL_HAND_PHASES;
	int32_t scale_24_8 = FLOAT_TO_FIXED24_8(scale);
	int32_t back_pivot_length_24_8 = INT_TO_FIXED24_8(back_pivot_length);

	int32_t hand_dx = INT_TO_FIXED24_8(HAND_MASTER_OFFSETS[phase].dx);
	int32_t hand_dy = INT_TO_FIXED24_8(HAND_MASTER_OFFSETS[phase].dy);
	int32_t hand_len
		= FixedSqrt24_8(FixedMul24_8(hand_dx, hand_dx) + FixedMul24_8(hand_dy, hand_dy));

	int32_t dir_x = 0;
	int32_t dir_y = -INT_TO_FIXED24_8(1);
	if (hand_len > 0) {
		dir_x = FixedDiv24_8(hand_dx, hand_len);
		dir_y = FixedDiv24_8(hand_dy, hand_len);
	}

	// Fetch absolute center axle pivot point
	gl_Vertex true_pivot = CalculateBaselinePivot24_8(cell_x, cell_y, scale);
	int32_t scaled_back_len = FixedMul24_8(back_pivot_length_24_8, scale_24_8);

	// Project base center vector
	gl_Vertex back_center;
	back_center.x = true_pivot.x - FixedMul24_8(dir_x, scaled_back_len);
	back_center.y = true_pivot.y - FixedMul24_8(dir_y, scaled_back_len);

	return back_center;
}

/**
 * Computes the two baseline corners of the clock hand triangle.
 * Centered on the back pivot point and extended perpendicular to the spine vector.
 * Employs scale-invariant miter bias tracking to preserve strict center-pixel precision.
 * Instrumented with complete micro-step telemetry tracking.
 */
void CatClock_ComputeTriangleBaseCorners24_8(gl_Vertex back_center, gl_Vertex tip_vertex,
											 float base_width, float scale, gl_Vertex* out_v1,
											 gl_Vertex* out_v2) {
	// 1. Calculate the directional vector from back pivot toward the tip
	int32_t spine_dx = tip_vertex.x - back_center.x;
	int32_t spine_dy = tip_vertex.y - back_center.y;

	// 2. Find the length of the spine vector using the fixed-point square root
	int32_t spine_len
		= FixedSqrt24_8(FixedMul24_8(spine_dx, spine_dx) + FixedMul24_8(spine_dy, spine_dy));

	int32_t dir_x = 0;
	int32_t dir_y = 0;

	if (spine_len > 0) {
		dir_x = FixedDiv24_8(spine_dx, spine_len);
		dir_y = FixedDiv24_8(spine_dy, spine_len);
	}

	// 3. Obtain the true perpendicular unit direction vector: (-dir_y, dir_x)
	int32_t perp_dx = -dir_y;
	int32_t perp_dy = dir_x;

	// 4. APPLY MULTI-SCALE MITER WIDTH BIAS
	float biased_width = base_width - 0.01f;
	int32_t total_width_fixed = FLOAT_TO_FIXED24_8(biased_width * scale);
	int32_t half_width_fixed = total_width_fixed >> 1;

	// 5. Compute the raw outward geometric projections
	int32_t proj_v1_dx = FixedMul24_8(perp_dx, half_width_fixed);
	int32_t proj_v1_dy = FixedMul24_8(perp_dy, half_width_fixed);
	int32_t proj_v2_dx = FixedMul24_8(perp_dx, half_width_fixed);
	int32_t proj_v2_dy = FixedMul24_8(perp_dy, half_width_fixed);

	// 6. Project outward cleanly from the back center pixel
	out_v1->x = back_center.x + proj_v1_dx;
	out_v1->y = back_center.y + proj_v1_dy;
	out_v2->x = back_center.x - proj_v2_dx;
	out_v2->y = back_center.y - proj_v2_dy;
}

void Triangle_ApplyPhaseFreetypeBias(int phase_idx, float scale, const gl_Vertex* base_center,
									 const gl_Vertex* input_vertices, gl_Vertex* out_vertices24_8) {
	int phase = phase_idx % TOTAL_HAND_PHASES;
	int current_scale_int = (int) scale;
	int is_even_scale = (current_scale_int % 2 == 0);
	(void) base_center;

	// Hard Pass-Through Baseline: Seed out array structure indices natively
	out_vertices24_8[0].x = input_vertices[0].x;
	out_vertices24_8[0].y = input_vertices[0].y; // Tip V0
	out_vertices24_8[1].x = input_vertices[1].x;
	out_vertices24_8[1].y = input_vertices[1].y; // Left V1
	out_vertices24_8[2].x = input_vertices[2].x;
	out_vertices24_8[2].y = input_vertices[2].y; // Right V2

	int master_dx = HAND_MASTER_OFFSETS[phase].dx;
	int master_dy = HAND_MASTER_OFFSETS[phase].dy;

	int32_t bias_dx = 0;
	int32_t bias_dy = 0;

	/* -------------------------------------------------------------------------
	   ODD SCALES ARCHITECTURE BRANCH (1x, 3x, 5x...)
	   ------------------------------------------------------------------------- */
	if (!is_even_scale) {
		if (master_dx == 0 && master_dy < 0) {
			// Target the 1st Cardinal (12 O'Clock): Widens base corners horizontally
			out_vertices24_8[1].x -= 16;
			out_vertices24_8[2].x += 16;
		}
	}
	/* -------------------------------------------------------------------------
	   EVEN SCALES ARCHITECTURE BRANCH (2x, 4x, 6x...)
	   ------------------------------------------------------------------------- */
	else {
		if (phase == 30) {
			bias_dx = 0;
			bias_dy = 256;
		} else if (master_dy == 0) {
			// Hard zero collapse for horizontal major axis (3 and 9 O'Clock)
			bias_dx = 32;
			bias_dy = 0;
		} else {
			// THE VALIDATED EVEN SLANT SLOPING ENGINE (clamped_mag = 32)
			int32_t mx = INT_TO_FIXED24_8(master_dx);
			int32_t my = INT_TO_FIXED24_8(master_dy);
			int32_t len = FixedSqrt24_8(FixedMul24_8(mx, mx) + FixedMul24_8(my, my));
			int32_t dir_x = 0;
			int32_t dir_y = -INT_TO_FIXED24_8(1);

			if (len > 0) {
				dir_x = FixedDiv24_8(mx, len);
				dir_y = FixedDiv24_8(my, len);
			}

			int32_t clamped_mag = 32;

			if (abs(master_dx) >= abs(master_dy)) {
				if (dir_x < 0)
					clamped_mag = -clamped_mag;
			} else {
				if (dir_y < 0)
					clamped_mag = -clamped_mag;
			}

			bias_dx = FixedMul24_8(dir_x, INT_TO_FIXED24_8(clamped_mag)) >> 8;
			bias_dy = FixedMul24_8(dir_y, INT_TO_FIXED24_8(clamped_mag)) >> 8;
		}

		// Apply our scale-invariant bias coordinates onto the sharp apex
		out_vertices24_8[0].x += bias_dx;
		out_vertices24_8[0].y += bias_dy;
	}
}

void CatClock_ShaderHands(void* renderer, int cell_x, int cell_y, int sheet_w, int sheet_h,
						  int frame_idx, void* userdata) {
	uint8_t* buffer = (uint8_t*) renderer;
	struct {
		int type;
		SDL_Color color;
	}* hand_cfg = (typeof(hand_cfg)) userdata;
	int hand_type = hand_cfg ? hand_cfg->type : 0;

	float scale = (float) ctx.current_half_steps / 2.0f;
// only render the seconds while validating geometry
#ifdef DEBUG_DUMP_HAND_VERTICIES
	if (hand_type == HAND_TYPE_HOUR) {
		return;
	} else if (hand_type == HAND_TYPE_MINUTE) {
		return;
	}
#endif

	// per-hand sizes
	uint8_t palette_hand_idx = PALETTE_HAND_SECOND;
	int back_pivot_length = 7;
	int length_multiplier_nominator = 7; // denominator = 7
	float base_width = 3.0f;
	if (hand_type == HAND_TYPE_HOUR) {
		palette_hand_idx = PALETTE_HAND_HOUR;
		back_pivot_length = 3;
		length_multiplier_nominator = 3; // denominator = 7
		base_width = 7.0f;
	} else if (hand_type == HAND_TYPE_MINUTE) {
		palette_hand_idx = PALETTE_HAND_MINUTE;
		back_pivot_length = 5;
		length_multiplier_nominator = 5; // denominator = 7
		base_width = 5.0f;
	}

	/* ========================================================================
	   GEOMETRY
	   ======================================================================== */
	// tip uncompensated
	gl_Vertex uncomp_vertices;
	memset(&uncomp_vertices, 0, sizeof(uncomp_vertices));
	Triangle_ComputeTip(cell_x, cell_y, length_multiplier_nominator, frame_idx, scale,
						&uncomp_vertices);

	// tip miter compensation
	gl_Vertex vertices24_8;
	Triangle_ApplyMiterSharpenerTip(frame_idx, scale, &uncomp_vertices, &vertices24_8);

	// base center
	gl_Vertex back_center_fixed
		= Triangle_ComputeBaseCenter24_8(cell_x, cell_y, frame_idx, scale, back_pivot_length);

	// base corners miter
	gl_Vertex base_corner_v1;
	gl_Vertex base_corner_v2;
	memset(&base_corner_v1, 0, sizeof(base_corner_v1));
	memset(&base_corner_v2, 0, sizeof(base_corner_v2));
	CatClock_ComputeTriangleBaseCorners24_8(back_center_fixed, vertices24_8, base_width, scale,
											&base_corner_v1, &base_corner_v2);

	// tip freetype bias
	gl_Vertex raw_pipeline_pack[3] = { vertices24_8, base_corner_v1, base_corner_v2 };
	gl_Vertex biased_pipeline_pack[3];
	memset(biased_pipeline_pack, 0, sizeof(biased_pipeline_pack));

	Triangle_ApplyPhaseFreetypeBias(frame_idx, scale, &back_center_fixed, raw_pipeline_pack,
									biased_pipeline_pack);

	/* ========================================================================
	   RASTERIZATION
	   ======================================================================== */
#ifndef DEBUG_DUMP_HAND_VERTICIES
	if (true) {
		DrawCapsuleFreetype(buffer, biased_pipeline_pack[0].x, biased_pipeline_pack[0].y,
							biased_pipeline_pack[1].x, biased_pipeline_pack[1].y,
							biased_pipeline_pack[2].x, biased_pipeline_pack[2].y, sheet_w, sheet_h,
							palette_hand_idx);
	} else {
		DrawTriangleFreetype(buffer, biased_pipeline_pack[0].x, biased_pipeline_pack[0].y,
							 biased_pipeline_pack[1].x, biased_pipeline_pack[1].y,
							 biased_pipeline_pack[2].x, biased_pipeline_pack[2].y, sheet_w, sheet_h,
							 palette_hand_idx);
	}
#else
	int phase = frame_idx % TOTAL_HAND_PHASES;
	ValidateGeometryDebug(buffer, sheet_w, sheet_h, cell_x, cell_y, phase, hand_type, scale,
						  back_pivot_length, uncomp_vertices, vertices24_8, back_center_fixed,
						  biased_pipeline_pack, palette_hand_idx);
#endif
}

static void render_freetype_span_callback(int y, int count, const FT_Span* spans, void* user) {
	FreetypeRenderTarget* target = (FreetypeRenderTarget*) user;
	if (y < 0 || y >= target->height)
		return;

	int row_offset = y * target->width;
	for (int i = 0; i < count; i++) {
		int x1 = spans[i].x;
		int len = spans[i].len;
		int x2 = x1 + len;

		for (int x = x1; x < x2; x++) {
			if (x >= 0 && x < target->width) {
				target->buffer[row_offset + x] = target->token;
			}
		}
	}
}

void DrawTriangleFreetype(uint8_t* buffer, int32_t fx_x0, int32_t fx_y0, int32_t fx_x1,
						  int32_t fx_y1, int32_t fx_x2, int32_t fx_y2, int width, int height,
						  uint8_t token) {
	if (width <= 0 || height <= 0 || buffer == NULL)
		return;

	// Direct, un-drifted translation down to FreeType 26.6 fixed-point space
	FT_Vector points[3];
	points[0].x = (FT_Pos) (fx_x0 >> 2);
	points[0].y = (FT_Pos) (fx_y0 >> 2);
	points[1].x = (FT_Pos) (fx_x1 >> 2);
	points[1].y = (FT_Pos) (fx_y1 >> 2);
	points[2].x = (FT_Pos) (fx_x2 >> 2);
	points[2].y = (FT_Pos) (fx_y2 >> 2);

	char tags[] = { FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON };
	unsigned short contours[] = { 2 };

	FT_Outline outline = { .n_points = 3,
						   .n_contours = 1,
						   .points = points,
						   .tags = (unsigned char*) tags,
						   .contours = contours,
						   .flags = FT_OUTLINE_SMART_DROPOUTS | FT_OUTLINE_INCLUDE_STUBS };

	FT_Raster raster;
	if (ft_grays_raster.raster_new(NULL, &raster) != 0)
		return;

	unsigned char raster_pool[STANDALONE_RASTER_POOL_SIZE];
	ft_grays_raster.raster_reset(raster, raster_pool, STANDALONE_RASTER_POOL_SIZE);

	FreetypeRenderTarget target
		= { .buffer = buffer, .width = width, .height = height, .token = token };
	FT_Raster_Params params
		= { .flags = FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_AA,
			.gray_spans = render_freetype_span_callback,
			.user = &target,
			.source = &outline,
			.clip_box = { .xMin = 0, .yMin = 0, .xMax = width, .yMax = height } };

	ft_grays_raster.raster_render(raster, &params);
	ft_grays_raster.raster_done(raster);
}

void DrawCapsuleFreetype(uint8_t* buffer, int32_t fx_x0, int32_t fx_y0, int32_t fx_x1,
						 int32_t fx_y1, int32_t fx_x2, int32_t fx_y2, int width, int height,
						 uint8_t token) {
	if (width <= 0 || height <= 0 || buffer == NULL)
		return;

	DrawTriangleFreetype(buffer, fx_x0, fx_y0, fx_x1, fx_y1, fx_x2, fx_y2, width, height, token);

	/* =========================================================================
	   BÉZIER ROUNDED REAR CAP PASS
	   Calculate a trailing control anchor behind the rear pivot baseline center.
	   Invoking FreeType directly allows us to render a hardware-accelerated
	   quadratic arc cap over the axle center pin.
	   ========================================================================= */

	// Locate the structural center point of your mitered rear baseline edge
	int32_t mid_base_x = (fx_x1 + fx_x2) >> 1;
	int32_t mid_base_y = (fx_y1 + fx_y2) >> 1;

	// Direction vector tracking the spine projection from base to tip
	int32_t spine_dx = fx_x0 - mid_base_x;
	int32_t spine_dy = fx_y0 - mid_base_y;

	// Project the rear base control anchor outward opposite to the hand trajectory
	// Shifting right by 3 bitwise positions scales the rounding radius proportionally with length
	int32_t back_anchor_x = mid_base_x - (spine_dx >> 5);
	int32_t back_anchor_y = mid_base_y - (spine_dy >> 5);

	// Translate coordinates down to FreeType 26.6 fixed-point space
	FT_Vector cap_points[3];
	cap_points[0].x = (FT_Pos) (fx_x1 >> 2);
	cap_points[0].y = (FT_Pos) (fx_y1 >> 2); // Left Baseline Corner
	cap_points[1].x = (FT_Pos) (back_anchor_x >> 2);
	cap_points[1].y = (FT_Pos) (back_anchor_y >> 2); // Conic Control Anchor
	cap_points[2].x = (FT_Pos) (fx_x2 >> 2);
	cap_points[2].y = (FT_Pos) (fx_y2 >> 2); // Right Baseline Corner

	// Construct a quadratic Bézier curve path using standard FreeType tags
	char tags[] = { FT_CURVE_TAG_ON, FT_CURVE_TAG_CONIC, FT_CURVE_TAG_ON };
	unsigned short contours[] = { 2 };

	FT_Outline outline = { .n_points = 3,
						   .n_contours = 1,
						   .points = cap_points,
						   .tags = (unsigned char*) tags,
						   .contours = contours,
						   .flags = FT_OUTLINE_SMART_DROPOUTS | FT_OUTLINE_INCLUDE_STUBS };

	FT_Raster raster;
	if (ft_grays_raster.raster_new(NULL, &raster) != 0)
		return;

	unsigned char raster_pool[STANDALONE_RASTER_POOL_SIZE];
	ft_grays_raster.raster_reset(raster, raster_pool, STANDALONE_RASTER_POOL_SIZE);

	FreetypeRenderTarget target
		= { .buffer = buffer, .width = width, .height = height, .token = token };

	FT_Raster_Params params
		= { .flags = FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_AA,
			.gray_spans = render_freetype_span_callback,
			.user = &target,
			.source = &outline,
			.clip_box = { .xMin = 0, .yMin = 0, .xMax = width, .yMax = height } };

	// Render the smooth trailing base arc directly onto your frame palette buffer
	ft_grays_raster.raster_render(raster, &params);
	ft_grays_raster.raster_done(raster);
}

#ifdef DEBUG_DUMP_HAND_VERTICIES
/**
 * Extracted Geometry Validation Pass
 * Renders the underlying footprint, mitered centers, crosshairs, and metrics cleanly on top.
 */
static void ValidateGeometryDebug(uint8_t* buffer, int sheet_w, int sheet_h, int cell_x, int cell_y,
								  int phase, int hand_type, float scale, int back_pivot_length,
								  gl_Vertex uncomp_vertices, gl_Vertex vertices24_8,
								  gl_Vertex back_center_fixed,
								  const gl_Vertex* biased_pipeline_pack,
								  uint8_t palette_hand_idx_org) {
	(void) palette_hand_idx_org;
	uint8_t palette_hand_idx = PALETTE_HAND_SECOND;

	int current_scale_int = (int) scale;
	int half_span = current_scale_int >> 1;
	int is_even_scale = (current_scale_int % 2 == 0);

	// Clockface focal pivot down conversion
	int32_t pivot_base_x, pivot_base_y;
	gl_Vertex system_true_pivot = CalculateBaselinePivot24_8(cell_x, cell_y, scale);
	pivot_base_x = system_true_pivot.x >> 8;
	pivot_base_y = system_true_pivot.y >> 8;

	// Anchor points down conversion
	int32_t back_base_x = back_center_fixed.x >> 8;
	int32_t back_base_y = back_center_fixed.y >> 8;
	int32_t uncomp_base_x = uncomp_vertices.x >> 8;
	int32_t uncomp_base_y = uncomp_vertices.y >> 8;
	int32_t comp_base_x = vertices24_8.x >> 8;
	int32_t comp_base_y = vertices24_8.y >> 8;

	int32_t corner1_pixel_x = biased_pipeline_pack[1].x >> 8;
	int32_t corner1_pixel_y = biased_pipeline_pack[1].y >> 8;
	int32_t corner2_pixel_x = biased_pipeline_pack[2].x >> 8;
	int32_t corner2_pixel_y = biased_pipeline_pack[2].y >> 8;

	printf("\n=== START PIPELINE EVALUATION [Phase %d, Hand Type %d, Scale %.2f] ===\n", phase,
		   hand_type, scale);
	printf("[Trace Centerline Spacing] Pivot: (%d, %d) | BackPivot: (%d, %d) | Expected Delta: %d "
		   "px\n",
		   pivot_base_x, pivot_base_y, back_base_x, back_base_y,
		   back_pivot_length * current_scale_int);

	// 1. VISUALIZE BACKGROUND LAYER: Raw Uncompensated Geometry Matrix Footprint
	int span_min = -half_span;
	int span_max = is_even_scale ? half_span : half_span + 1;
	for (int dy = span_min; dy < span_max; dy++) {
		for (int dx = span_min; dx < span_max; dx++) {
			PlotSoftwarePixel(buffer, uncomp_base_x + dx, uncomp_base_y + dy, sheet_w, sheet_h,
							  palette_hand_idx);
		}
	}

	// 2. VISUALIZE FOREGROUND NODE: Compensated Miter Sharpener Node
	PlotSoftwarePixel(buffer, comp_base_x, comp_base_y, sheet_w, sheet_h, PALETTE_PUPIL);

	// 3. VISUALIZE BACK PIVOT FOOTPRINT
	if (is_even_scale) {
		PlotSoftwarePixel(buffer, back_base_x - 1, back_base_y - 1, sheet_w, sheet_h,
						  PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, back_base_x, back_base_y - 1, sheet_w, sheet_h, PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, back_base_x - 1, back_base_y, sheet_w, sheet_h, PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, back_base_x, back_base_y, sheet_w, sheet_h, PALETTE_PUPIL);

		// 4. VISUALIZE MAIN AXLE PIVOT FOOTPRINT
		PlotSoftwarePixel(buffer, pivot_base_x - 1, pivot_base_y - 1, sheet_w, sheet_h,
						  palette_hand_idx);
		PlotSoftwarePixel(buffer, pivot_base_x, pivot_base_y - 1, sheet_w, sheet_h,
						  palette_hand_idx);
		PlotSoftwarePixel(buffer, pivot_base_x - 1, pivot_base_y, sheet_w, sheet_h,
						  palette_hand_idx);
		PlotSoftwarePixel(buffer, pivot_base_x, pivot_base_y, sheet_w, sheet_h, palette_hand_idx);

		// 5. VISUALIZE COMPUTED TRIANGLE BASELINE CORNERS (V1 & V2)
		PlotSoftwarePixel(buffer, corner1_pixel_x, corner1_pixel_y, sheet_w, sheet_h,
						  PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, corner2_pixel_x, corner2_pixel_y, sheet_w, sheet_h,
						  PALETTE_PUPIL);
	} else {
		int p_min = -half_span;
		int p_max = half_span;
		PlotSoftwarePixel(buffer, back_base_x + p_min, back_base_y + p_min, sheet_w, sheet_h,
						  PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, back_base_x + p_max, back_base_y + p_min, sheet_w, sheet_h,
						  PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, back_base_x + p_min, back_base_y + p_max, sheet_w, sheet_h,
						  PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, back_base_x + p_max, back_base_y + p_max, sheet_w, sheet_h,
						  PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, back_base_x, back_base_y, sheet_w, sheet_h, PALETTE_PUPIL);

		// 4. VISUALIZE MAIN AXLE PIVOT FOOTPRINT
		PlotSoftwarePixel(buffer, pivot_base_x + p_min, pivot_base_y + p_min, sheet_w, sheet_h,
						  palette_hand_idx);
		PlotSoftwarePixel(buffer, pivot_base_x + p_max, pivot_base_y + p_min, sheet_w, sheet_h,
						  palette_hand_idx);
		PlotSoftwarePixel(buffer, pivot_base_x + p_min, pivot_base_y + p_max, sheet_w, sheet_h,
						  palette_hand_idx);
		PlotSoftwarePixel(buffer, pivot_base_x + p_max, pivot_base_y + p_max, sheet_w, sheet_h,
						  palette_hand_idx);
		PlotSoftwarePixel(buffer, pivot_base_x, pivot_base_y, sheet_w, sheet_h, palette_hand_idx);

		// 5. VISUALIZE COMPUTED TRIANGLE BASELINE CORNERS (V1 & V2 CROSSHAIRS)
		PlotSoftwarePixel(buffer, corner1_pixel_x + p_min, corner1_pixel_y + p_min, sheet_w,
						  sheet_h, PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, corner1_pixel_x + p_max, corner1_pixel_y + p_min, sheet_w,
						  sheet_h, PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, corner1_pixel_x + p_min, corner1_pixel_y + p_max, sheet_w,
						  sheet_h, PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, corner1_pixel_x + p_max, corner1_pixel_y + p_max, sheet_w,
						  sheet_h, PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, corner1_pixel_x, corner1_pixel_y, sheet_w, sheet_h,
						  PALETTE_PUPIL);

		PlotSoftwarePixel(buffer, corner2_pixel_x + p_min, corner2_pixel_y + p_min, sheet_w,
						  sheet_h, PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, corner2_pixel_x + p_max, corner2_pixel_y + p_min, sheet_w,
						  sheet_h, PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, corner2_pixel_x + p_min, corner2_pixel_y + p_max, sheet_w,
						  sheet_h, PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, corner2_pixel_x + p_max, corner2_pixel_y + p_max, sheet_w,
						  sheet_h, PALETTE_PUPIL);
		PlotSoftwarePixel(buffer, corner2_pixel_x, corner2_pixel_y, sheet_w, sheet_h,
						  PALETTE_PUPIL);
	}
	printf("[Trace Baseline Alignment] Corner 1: (%d, %d) | Corner 2: (%d, %d)\n", corner1_pixel_x,
		   corner1_pixel_y, corner2_pixel_x, corner2_pixel_y);
	printf("=== END PIPELINE EVALUATION [Phase %d] ===\n", phase);
}
#endif
