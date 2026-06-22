/******************************************************************************
 * File Name:    catclock_tail.h
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

#define RING_SEGMENTS 36
#define CAP_SEGMENTS 17

void CatClock_ShaderTail(SDL_Renderer* renderer, int cell_x, int cell_y, float atlas_w_f,
						 int frame_idx, void* userdata) {
	uint8_t* buffer = (uint8_t*) renderer;
	int atlas_w = (int) atlas_w_f;

	// Dynamically resolve total vertical dimensions to protect clipping passes
	int cols = 8;
	int total_fps_frames = target_fps_limit <= 0 ? 30 : target_fps_limit;
	int total_frames = total_fps_frames * 2;
	int rows = (total_frames + cols - 1) / cols;

	// FIX THE SCALE BUG: Derive actual scaled cell height from the horizontal sheet stride
	int scaled_cell_h = atlas_w / cols;
	int atlas_h = rows * scaled_cell_h;

	CatClock_TailShaderArgs* args = (CatClock_TailShaderArgs*) userdata;

	float shift_x = args ? args->offset_x : 0.0f;
	float shift_y = args ? args->offset_y : 0.0f;

	bool is_halo = args && args->force_halo_color;
	uint8_t palette_idx = is_halo ? PALETTE_HALO : PALETTE_CAT_BODY;

	float total_cycle_frames = (float) total_frames;
	float progress = ((float) frame_idx + 0.5f) / total_cycle_frames;
	float sin_wave = sinf(progress * 2.0f * (float) M_PI);
	float normalized_wave = (sin_wave + 1.0f) / 2.0f;

	float MAX_LEFT_ANGLE_DEG = -18.0f;
	float MAX_RIGHT_ANGLE_DEG = 28.5f;
	float final_angle_deg
		= MAX_LEFT_ANGLE_DEG + (normalized_wave * (MAX_RIGHT_ANGLE_DEG - MAX_LEFT_ANGLE_DEG));

	float rad = (final_angle_deg * (float) M_PI) / 180.0f;
	float cos_a = cosf(rad);
	float sin_a = sinf(rad);

	float internal_unit_ratio = (float) (atlas_w / 8) / 96.0f;
	float pivot_x
		= (float) cell_x + (((float) atlas_w / 8.0f) / 2.0f) - (1.0f * internal_unit_ratio);
	float pivot_y = (float) cell_y + (12.0f * internal_unit_ratio);

	float horizontal_cushion = is_halo ? (0.35f * internal_unit_ratio) : 0.0f;
	float half_width = (6.5f * internal_unit_ratio) + horizontal_cushion;
	float outer_radius = 16.5f * internal_unit_ratio;
	float inner_radius = 3.5f * internal_unit_ratio;

	float stem_start_y = -8.5f * internal_unit_ratio;
	float stem_end_y = 62.0f * internal_unit_ratio;
	float capsule_length_stretch = 6.5f * internal_unit_ratio;

	float loop_center_x = (outer_radius + horizontal_cushion) - half_width;
	float seam_pad = 0.5f * internal_unit_ratio;
	float loop_center_y = stem_end_y;

	float cap_center_x = loop_center_x + ((outer_radius + inner_radius) / 2.0f);
	// Shift only the vertical position downward to overlap the straight rod cleanly
	float cap_center_y = loop_center_y - capsule_length_stretch + seam_pad;
	float cap_radius = (outer_radius - inner_radius) / 2.0f;

#define TRANSFORM_AND_PLOT(lx, ly, out_x, out_y)                                                   \
	do {                                                                                           \
		out_x = (int) (pivot_x + shift_x + ((lx) * cos_a - (ly) * sin_a));                         \
		out_y = (int) (pivot_y + shift_y + ((lx) * sin_a + (ly) * cos_a));                         \
	} while (0)

	// PART 1: STEM BODY
	int sx0, sy0, sx1, sy1, sx2, sy2, sx3, sy3;
	TRANSFORM_AND_PLOT(-half_width, stem_start_y, sx0, sy0);
	TRANSFORM_AND_PLOT(half_width, stem_start_y, sx1, sy1);
	TRANSFORM_AND_PLOT(-half_width, stem_end_y, sx2, sy2);
	TRANSFORM_AND_PLOT(half_width, stem_end_y, sx3, sy3);

	FillSoftwareTriangle(buffer, sx0, sy0, sx1, sy1, sx2, sy2, atlas_w, atlas_h, palette_idx);
	FillSoftwareTriangle(buffer, sx1, sy1, sx3, sy3, sx2, sy2, atlas_w, atlas_h, palette_idx);

	// PART 2: THE CURVED BOTTOM RING HOOK
	int last_ox = 0, last_oy = 0, last_ix = 0, last_iy = 0;
	for (int i = 0; i < RING_SEGMENTS; i++) {
		float t = (float) i / (float) (RING_SEGMENTS - 1);
		float sweep_rad = (float) M_PI - (t * (float) M_PI);
		float cos_s = cosf(sweep_rad);
		float sin_s = sinf(sweep_rad);

		float ox = loop_center_x + outer_radius * cos_s;
		float oy = loop_center_y + outer_radius * sin_s;
		float ix = loop_center_x + inner_radius * cos_s;
		float iy = loop_center_y + inner_radius * sin_s;

		if (i == 0 || i == RING_SEGMENTS - 1) {
			oy = stem_end_y;
			iy = stem_end_y;
		}

		int p_ox, p_oy, p_ix, p_iy;
		TRANSFORM_AND_PLOT(ox, oy, p_ox, p_oy);
		TRANSFORM_AND_PLOT(ix, iy, p_ix, p_iy);

		if (i > 0) {
			FillSoftwareTriangle(buffer, last_ox, last_oy, last_ix, last_iy, p_ox, p_oy, atlas_w,
								 atlas_h, palette_idx);
			FillSoftwareTriangle(buffer, last_ix, last_iy, p_ix, p_iy, p_ox, p_oy, atlas_w, atlas_h,
								 palette_idx);
		}
		last_ox = p_ox;
		last_oy = p_oy;
		last_ix = p_ix;
		last_iy = p_iy;
	}

	// PART 3: THE ELONGATED STRAIGHT EXTENSION
	int ex0, ey0, ex1, ey1;
	TRANSFORM_AND_PLOT(loop_center_x + outer_radius, loop_center_y - capsule_length_stretch, ex0,
					   ey0);
	TRANSFORM_AND_PLOT(loop_center_x + inner_radius, loop_center_y - capsule_length_stretch, ex1,
					   ey1);

	FillSoftwareTriangle(buffer, last_ox, last_oy, last_ix, last_iy, ex0, ey0, atlas_w, atlas_h,
						 palette_idx);
	FillSoftwareTriangle(buffer, last_ix, last_iy, ex1, ey1, ex0, ey0, atlas_w, atlas_h,
						 palette_idx);

	// PART 4: THE ROUNDED CAPSULE END-CAP TIP
	int p_cap_cx, p_cap_cy;
	TRANSFORM_AND_PLOT(cap_center_x, cap_center_y, p_cap_cx, p_cap_cy);

	int last_cx = ex0, last_cy = ey0;
	for (int i = 0; i < CAP_SEGMENTS; i++) {
		float t = (float) i / (float) (CAP_SEGMENTS - 1);
		float cap_arc_rad = t * (float) M_PI;
		float cx = cap_center_x + cap_radius * cosf(cap_arc_rad);
		float cy = cap_center_y - cap_radius * sinf(cap_arc_rad);

		int p_cx, p_cy;
		TRANSFORM_AND_PLOT(cx, cy, p_cx, p_cy);

		if (i > 0) {
			FillSoftwareTriangle(buffer, p_cap_cx, p_cap_cy, last_cx, last_cy, p_cx, p_cy, atlas_w,
								 atlas_h, palette_idx);
		}
		last_cx = p_cx;
		last_cy = p_cy;
	}

#undef TRANSFORM_AND_PLOT
}
