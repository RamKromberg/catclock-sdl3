/******************************************************************************
 * File Name:    catclock_tail.c
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
#include <stdio.h>
#include <stdlib.h>

#define RING_SEGMENTS 36
#define CAP_SEGMENTS 17

void CatClock_ShaderTail(void* target_buffer, int cell_x, int cell_y, int sheet_w, int sheet_h,
						 int frame_idx, void* userdata) {
	Uint8* buffer = (Uint8*) target_buffer;
	if (!buffer || sheet_w <= 0 || sheet_h <= 0)
		return;

	CatClock_TailShaderArgs* args = (CatClock_TailShaderArgs*) userdata;
	float shift_x = args ? args->offset_x : 0.0f;
	float shift_y = args ? args->offset_y : 0.0f;

	bool is_halo = args && args->force_halo_color;
	Uint8 palette_idx = is_halo ? PALETTE_HALO : PALETTE_CAT_BODY;

	int total_tail_frames = ctx.tail_atlas.total_frames > 0 ? ctx.tail_atlas.total_frames : 1;
	float progress = ((float) frame_idx + 0.5f) / (float) total_tail_frames;
	float sin_wave = sinf(progress * 2.0f * (float) M_PI);
	float normalized_wave = (sin_wave + 1.0f) / 2.0f;

	float MAX_LEFT_ANGLE_DEG = -18.0f;
	float MAX_RIGHT_ANGLE_DEG = 28.5f;
	float final_angle_deg
		= MAX_LEFT_ANGLE_DEG + (normalized_wave * (MAX_RIGHT_ANGLE_DEG - MAX_LEFT_ANGLE_DEG));

	float rad = (final_angle_deg * (float) M_PI) / 180.0f;
	float cos_a = cosf(rad);
	float sin_a = sinf(rad);

	/* Read scale multipliers directly from the active atlas structure properties */
	float internal_unit_ratio = (float) ctx.tail_atlas.cell_w / 96.0f;
	float pivot_x
		= (float) cell_x + ((float) ctx.tail_atlas.cell_w / 2.0f) - (1.0f * internal_unit_ratio);
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

	FillSoftwareTriangle(buffer, sx0, sy0, sx1, sy1, sx2, sy2, sheet_w, sheet_h, palette_idx);
	FillSoftwareTriangle(buffer, sx1, sy1, sx3, sy3, sx2, sy2, sheet_w, sheet_h, palette_idx);

	// PART 2: CURVED RING HOOK
	int last_ox = 0, last_oy = 0, last_ix = 0, last_iy = 0;
	for (int i = 0; i < 36; i++) {
		float t = (float) i / 35.0f;
		float sweep_rad = (float) M_PI - (t * (float) M_PI);
		float cos_s = cosf(sweep_rad);
		float sin_s = sinf(sweep_rad);

		float ox = loop_center_x + outer_radius * cos_s;
		float ox_y = loop_center_y + outer_radius * sin_s;
		float ix = loop_center_x + inner_radius * cos_s;
		float ix_y = loop_center_y + inner_radius * sin_s;

		if (i == 0 || i == 35) {
			ox_y = stem_end_y;
			ix_y = stem_end_y;
		}

		int px_ox, py_ox, px_ix, py_ix;
		TRANSFORM_AND_PLOT(ox, ox_y, px_ox, py_ox);
		TRANSFORM_AND_PLOT(ix, ix_y, px_ix, py_ix);

		if (i > 0) {
			FillSoftwareTriangle(buffer, last_ox, last_oy, px_ox, py_ox, last_ix, last_iy, sheet_w,
								 sheet_h, palette_idx);
			FillSoftwareTriangle(buffer, px_ox, py_ox, px_ix, py_ix, last_ix, last_iy, sheet_w,
								 sheet_h, palette_idx);
		}
		last_ox = px_ox;
		last_oy = py_ox;
		last_ix = px_ix;
		last_iy = py_ix;
	}

	// PART 3: TOP CAPSULE CLOSURE
	int last_cx = 0, last_cy = 0;
	for (int i = 0; i < 17; i++) {
		float t = (float) i / 16.0f;
		float sweep_rad = (float) M_PI + (t * (float) M_PI);
		int px_cx, py_cy;

		TRANSFORM_AND_PLOT(cap_center_x + cap_radius * cosf(sweep_rad),
						   cap_center_y + cap_radius * sinf(sweep_rad), px_cx, py_cy);

		if (i > 0) {
			int px_c0, py_c0;
			TRANSFORM_AND_PLOT(cap_center_x, cap_center_y, px_c0, py_c0);
			FillSoftwareTriangle(buffer, px_c0, py_c0, last_cx, last_cy, px_cx, py_cy, sheet_w,
								 sheet_h, palette_idx);
		}
		last_cx = px_cx;
		last_cy = py_cy;
	}
#undef TRANSFORM_AND_PLOT
}
