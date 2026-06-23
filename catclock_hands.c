/******
 * File Name:    catclock_hands.c
 * Project:     catclock-sdl3 (Modernized Kit-Cat Klock Desktop Widget)
 *
 * Authorship & Collaboration:
 *   - Refactored in collaborative partnership between the User and AI.
 *
 * Description:
 *   - Pure software-rasterized clock hand geometry generating 8-bit index data.
 *   - Bridges vector geometry smoothly into a headless 8-bit palette array sheet.
 *
 * Architectural & Alignment Specification (Crucial for Downstream Shaders):
 *   1. THE ASSET BIAS DEEP GROUND TRUTH:
 *      The unscaled native 1-bit background asset ('catback.xbm') spans 100x240 pixels.
 *      The white oval clock face dial cuts in horizontally from column index X=20 to X=78,
 *      implying a raw design diameter bounds width of exactly 58 pixels.
 *      Measuring via GIMP reveals the physical center-pin artwork (a 3x3 black square dot)
 *      has its exact coordinate pivot at global coordinate positions (X=49.0, Y=139.0).
 *
 *   2. LOCAL ATLAS SPACE CONSTRAINTS:
 *      The structural compositor inside 'catclock_atlas.c' extracts the hands canvas
 *      sub-region starting at an unscaled global offset of (X=18.0f, Y=92.0f).
 *      Subtracting this clipping origin maps the artwork pivot point to local cell space:
 *        local_center_x = 49.0f - 18.0f = 31.0f
 *        local_center_y = 139.0f - 92.0f = 47.0f
 *
 *   3. EXTRA INJECTED BUFFER PADDING COMPENSATIONS:
 *      During 'CatClock_RebakeComputeAtlas', cell sizes add an explicit 2-pixel margin
 *      to buffer against hardware filter-bleeding artifacts:
 *        atlas->cell_w = (int)ceilf(cell_base_w * scale) + 2;
 *      This shifts the inner drawing canvas index plane by exactly +1 scaled integer
 *      pixel on both axes. Hence, the perfect visual horizontal anchor formula evaluates
 *      strictly to: center_x = (31.0f * scale) + 1.0f.
 *      Conversely, 'cell_h' expands with uneven padding bounds at the bottom container floor.
 *      To avoid floating point truncation drift, center_y balances symmetrically from
 *      the live container bounding midpoint with a clean linear shift:
 *        center_y = (cell_h / 2.0f) + (1.0f * scale).
 *
 *   4. OVAL ANAMORPHIC MATHEMATICS:
 *      The dial is an ellipse with a 2:3 anamorphic aspect ratio. To prevent the hands from
 *      jittering or distorting their width during rotation, the perpendicular base lines
 *      (perp_x, perp_y) are computed inside circular space before applying the 1.5x vertical
 *      aspect scale factor to final positions.
 *
 * License: Open Source / Educational - Preserve attribution upon redistribution.
 ******/

#include "catclock_shared.h"
#include <math.h>

/* Linkage against the core 8-bit array triangle filler defined inside catclock_shared.h */
extern void FillSoftwareTriangle(uint8_t* buffer, int x0, int y0, int x1, int y1, int x2, int y2,
								 int width, int height, uint8_t color_idx);

void CatClock_ShaderHands(void* renderer, int cell_x, int cell_y, float atlas_w_f, int frame_idx,
						  void* userdata) {
	/* Cast the renderer context address straight to our headless 8-bit tracking plane */
	uint8_t* buffer = (uint8_t*) renderer;
	int atlas_w = (int) atlas_w_f;

	/* Extract dynamic scale-aware sheet layout metrics from active runtime contexts */
	int cell_w = ctx.hands_atlas.cell_w;
	int cell_h = ctx.hands_atlas.cell_h;

	int hand_type = 0;
	if (userdata) {
		hand_type = *(int*) userdata;
	}

	/* Track timeline step loops; TOTAL_PHASES is locked to standard 60-tick timeline frames */
	int phase = frame_idx % TOTAL_PHASES;

	/* Rotate counterclockwise by PI/2 so that phase 0 points perfectly up at 12 o'clock */
	float angle_rad
		= ((float) phase / (float) TOTAL_PHASES) * 2.0f * (float) M_PI - ((float) M_PI / 2.0f);
	float scale = ctx.current_scale;

	/* 🎯 PIXEL-PERFECT VISUAL ARTWORK PIN ANCHORING
	 * center_x: 31.0f base offset from dial edge, scaled linearly, plus 1px texture border padding.
	 * center_y: Live cell midpoint container line plus 1px per scale level vertical drift
	 * correction.
	 */
	float center_x = (31.0f * scale) + 1.0f;
	float center_y = ((float) cell_h / 2.0f) + (1.0f * scale);

	/* Dimensional layout parameters matching original X11 structural hand proportions */
	float base_half_width = 2.5f * scale;
	float tip_length = ((float) cell_w < (float) cell_h ? (float) cell_w : (float) cell_h) * 0.38f;
	float back_pivot_extension = 4.0f * scale;
	uint8_t palette_hand_idx = PALETTE_HAND_HOUR;

	/* Isolate individual hand configuration types and map them to their strict 8-bit semantic
	 * tokens */
	if (hand_type == HAND_TYPE_MINUTE) {
		palette_hand_idx = PALETTE_HAND_MINUTE;
		base_half_width = 2.0f * scale;
		tip_length = ((float) cell_w < (float) cell_h ? (float) cell_w : (float) cell_h) * 0.31f;
		back_pivot_extension = 5.0f * scale;
	} else if (hand_type == HAND_TYPE_SECOND) {
		palette_hand_idx = PALETTE_HAND_SECOND;
		base_half_width = 1.0f * scale;
		tip_length = ((float) cell_w < (float) cell_h ? (float) cell_w : (float) cell_h) * 0.43f;
		back_pivot_extension = 6.0f * scale;
	}

	/* Enforce a minimum safety width layout bound of a half-pixel to prevent triangle collapsing */
	if (base_half_width < 0.5f) {
		base_half_width = 0.5f;
	}

	/* 2:3 Anamorphic oval conversion factors applied exclusively to final spatial vectors */
	float aspect_scale_x = 1.0f;
	float aspect_scale_y = 1.5f;

	/* High-accuracy trigonometric direction components */
	float cos_a = cosf(angle_rad);
	float sin_a = sinf(angle_rad);

	/* Compute pointer tip coordinates using direction angles and vertical oval stretch multipliers
	 */
	float tip_x = center_x + (cos_a * tip_length * aspect_scale_x);
	float tip_y = center_y + (sin_a * tip_length * aspect_scale_y);

	/* Compute back pivot root extension point along the inverse direction vector path */
	float root_x = center_x - (cos_a * back_pivot_extension * aspect_scale_x);
	float root_y = center_y - (sin_a * back_pivot_extension * aspect_scale_y);

	/* CRITICAL OVAL PRESERVATION: Calculate thickness vectors perpendicular to unscaled space
	 * to keep thickness completely uniform during the full 360-degree rotation loop.
	 */
	float perp_x = -sin_a;
	float perp_y = cos_a;

	/* Project outer coordinates from the root midpoint to form the base pointer structure */
	float base_l_x = root_x + (perp_x * base_half_width * aspect_scale_x);
	float base_l_y = root_y + (perp_y * base_half_width * aspect_scale_y);

	float base_r_x = root_x - (perp_x * base_half_width * aspect_scale_x);
	float base_r_y = root_y - (perp_y * base_half_width * aspect_scale_y);

	/* 📐 SYMMETRIC ROUNDING IMPLEMENTATION
	 * Shifting values by a ±0.5 fraction before casting balances the float-to-integer truncation
	 * steps. This eliminates directional horizontal lean, ensuring the triangles rasterize
	 * symmetrically.
	 */
	int x0 = (int) (tip_x + (cos_a >= 0.0f ? 0.5f : -0.5f)) + cell_x;
	int y0 = (int) (tip_y + (sin_a >= 0.0f ? 0.5f : -0.5f)) + cell_y;

	int x1 = (int) (base_l_x + (perp_x >= 0.0f ? 0.5f : -0.5f)) + cell_x;
	int y1 = (int) (base_l_y + (perp_y >= 0.0f ? 0.5f : -0.5f)) + cell_y;

	int x2 = (int) (base_r_x + (-perp_x >= 0.0f ? 0.5f : -0.5f)) + cell_x;
	int y2 = (int) (base_r_y + (-perp_y >= 0.0f ? 0.5f : -0.5f)) + cell_y;

	/* Pack the finalized pixel tokens safely into our headless 8-bit palette index buffer sheet */
	FillSoftwareTriangle(buffer, x0, y0, x1, y1, x2, y2, atlas_w, ctx.hands_atlas.atlas_h,
						 palette_hand_idx);
}
