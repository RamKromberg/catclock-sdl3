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

#define TOTAL_MESH_SEGMENTS 60

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

	float internal_unit_ratio = (float) ctx.tail_atlas.cell_w / 96.0f;
	float pivot_x
		= (float) cell_x + ((float) ctx.tail_atlas.cell_w / 2.0f) - (2.0f * internal_unit_ratio);
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

	// Scale pivot and shift metrics into 8-bit Fixed-Point (Scale = 256)
	int scale_pivot_x = (int) (pivot_x * 256.0f);
	int scale_pivot_y = (int) (pivot_y * 256.0f);
	int scale_shift_x = (int) (shift_x * 256.0f);
	int scale_shift_y = (int) (shift_y * 256.0f);
	int fx_cos = (int) (cos_a * 256.0f);
	int fx_sin = (int) (sin_a * 256.0f);

#define FX_TRANSFORM_X(lx, ly)                                                                     \
	(scale_pivot_x + scale_shift_x                                                                 \
	 + ((((int) ((lx) * 256.0f) * fx_cos - (int) ((ly) * 256.0f) * fx_sin) + 128) >> 8))           \
		>> 8

#define FX_TRANSFORM_Y(lx, ly)                                                                     \
	(scale_pivot_y + scale_shift_y                                                                 \
	 + ((((int) ((lx) * 256.0f) * fx_sin + (int) ((ly) * 256.0f) * fx_cos) + 128) >> 8))           \
		>> 8

	// =================================================================
	// PART 1: STEM DRAW ROUTINE
	// =================================================================
	int sx0 = FX_TRANSFORM_X(-half_width, stem_start_y);
	int sy0 = FX_TRANSFORM_Y(-half_width, stem_start_y);
	int sx1 = FX_TRANSFORM_X(half_width, stem_start_y);
	int sy1 = FX_TRANSFORM_Y(half_width, stem_start_y);
	int sx2 = FX_TRANSFORM_X(-half_width, stem_end_y);
	int sy2 = FX_TRANSFORM_Y(-half_width, stem_end_y);
	int sx3 = FX_TRANSFORM_X(half_width, stem_end_y);
	int sy3 = FX_TRANSFORM_Y(half_width, stem_end_y);

	FillSoftwareTriangle(buffer, sx0, sy0, sx1, sy1, sx2, sy2, sheet_w, sheet_h, palette_idx);
	FillSoftwareTriangle(buffer, sx1, sy1, sx3, sy3, sx2, sy2, sheet_w, sheet_h, palette_idx);

	// =================================================================
	// PART 2: THE UNIFIED MESH HOOK & HOOK CAP ASSEMBLY
	// =================================================================
	int last_ox = 0, last_oy = 0, last_ix = 0, last_iy = 0;

	for (int i = 0; i <= TOTAL_MESH_SEGMENTS; i++) {
		float local_ox_x, local_ox_y, local_ix_x, local_ix_y;

		// SEGMENT DIVISION 1: The Semicircular Bottom Hook Ring (Indices 0 to 30)
		if (i <= 30) {
			float t = (float) i / 30.0f;
			float sweep_rad = (float) M_PI - (t * (float) M_PI);

			local_ox_x = loop_center_x + outer_radius * cosf(sweep_rad);
			local_ox_y = loop_center_y + outer_radius * sinf(sweep_rad);
			local_ix_x = loop_center_x + inner_radius * cosf(sweep_rad);
			local_ix_y = loop_center_y + inner_radius * sinf(sweep_rad);

			if (i == 0) {
				local_ox_x = -half_width;
				local_ox_y = stem_end_y;
				local_ix_x = half_width;
				local_ix_y = stem_end_y;
			}
		}
		// SEGMENT DIVISION 2: The Vertical Straight Extension Wall (Indices 31 to 40)
		else if (i <= 40) {
			float t = (float) (i - 30) / 10.0f;
			float current_y = loop_center_y - (t * (loop_center_y - cap_center_y));

			local_ox_x = loop_center_x + outer_radius;
			local_ox_y = current_y;
			local_ix_x = loop_center_x + inner_radius;
			local_ix_y = current_y;
		}
		// SEGMENT DIVISION 3: The Terminal Rounded Closure Cap (Indices 41 to 60)
		else {
			float t = (float) (i - 40) / 20.0f;
			float sweep_rad = 0.0f - (t * (float) M_PI);

			local_ox_x = cap_center_x + cap_radius * cosf(sweep_rad);
			local_ox_y = cap_center_y + cap_radius * sinf(sweep_rad);

			// Inner edge meets perfectly at the cap circle's spatial anchor origin
			local_ix_x = cap_center_x;
			local_ix_y = cap_center_y;
		}

		int px_ox = FX_TRANSFORM_X(local_ox_x, local_ox_y);
		int py_ox = FX_TRANSFORM_Y(local_ox_x, local_ox_y);
		int px_ix = FX_TRANSFORM_X(local_ix_x, local_ix_y);
		int py_ix = FX_TRANSFORM_Y(local_ix_x, local_ix_y);

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

#undef FX_TRANSFORM_X
#undef FX_TRANSFORM_Y
}
