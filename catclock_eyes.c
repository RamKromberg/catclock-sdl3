/******************************************************************************
 * File Name:    catclock_eyes.c
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

#include "catclock_atlas.h"
#include "catclock_shared.h"
#include <math.h>
#include <stdlib.h>

static void SoftwareDrawPupilOval(uint8_t* buffer, float cx, float cy, float r_base_w,
								  float r_base_h, float look_x, float max_offset_x, int atlas_w,
								  int atlas_h, int clip_x0, int clip_y0, int clip_x1, int clip_y1) {
	float true_center_x = cx + (look_x * max_offset_x);
	float true_center_y = cy;

	float compression_w = sqrtf(fmaxf(0.1f, 1.0f - (look_x * look_x * 0.45f)));
	float compression_h = sqrtf(fmaxf(0.1f, 1.0f - (look_x * look_x * 0.225f)));

	float pup_w = r_base_w * compression_w;
	float pup_h = r_base_h * compression_h;

	int x0 = (int) floorf(true_center_x - pup_w);
	int x1 = (int) ceilf(true_center_x + pup_w);
	int y0 = (int) floorf(true_center_y - pup_h);
	int y1 = (int) ceilf(true_center_y + pup_h);

	for (int y = y0; y <= y1; y++) {
		if (y < clip_y0 || y >= clip_y1 || y >= atlas_h)
			continue;
		for (int x = x0; x <= x1; x++) {
			if (x < clip_x0 || x >= clip_x1 || x >= atlas_w)
				continue;

			float dx = ((float) x + 0.5f - true_center_x) / pup_w;
			float dy = ((float) y + 0.5f - true_center_y) / pup_h;

			if ((dx * dx) + (dy * dy) <= 1.0f) {
				// Assign Stateless Pupil Token ID 4 (0xCC)
				PlotSoftwarePixel(buffer, x, y, atlas_w, atlas_h, 0xCC);
			}
		}
	}
}

void CatClock_ShaderEyes(void* renderer, int cell_x, int cell_y, int sheet_w, int sheet_h,
						 int frame_idx, void* userdata) {
	Uint8* buffer = (Uint8*) renderer;
	int atlas_w = sheet_w;
	int atlas_h = sheet_h;
	(void) userdata;

	int cols = 10;
	int total_fps_frames = target_fps_limit <= 0 ? 30 : target_fps_limit;
	int total_frames = total_fps_frames * 2;

	int cell_w = atlas_w / cols;
	int scaled_cell_h = (int) ceilf(32.0f * ctx.current_scale) + 2;

	float current_scale_factor = ctx.current_scale;

	float pup_base_w = 2.5f * current_scale_factor;
	float pup_base_h = 10.5f * current_scale_factor;
	float max_offset_x = 5.0f * current_scale_factor;

	int unscaled_mask_w = 54;
	int unscaled_mask_h = 23;

	float base_pad_x = 5.0f;
	float base_pad_y = 4.0f;
	float base_eye_w = 23.0f;
	float base_gap_x = 36.0f;

	float visual_pad = ctx.use_decorations ? 0.0f : 1.0f;
	float global_origin_x = (20.0f + visual_pad);
	float global_origin_y = (16.0f + visual_pad);

	int clip_x0 = cell_x
		+ ((int) floorf((global_origin_x + base_pad_x) * current_scale_factor)
		   - (int) floorf(global_origin_x * current_scale_factor));
	int clip_y0 = cell_y
		+ ((int) floorf((global_origin_y + base_pad_y) * current_scale_factor)
		   - (int) floorf(global_origin_y * current_scale_factor));
	int clip_x1 = clip_x0 + (int) ceilf((float) unscaled_mask_w * current_scale_factor);
	int clip_y1 = clip_y0 + (int) ceilf((float) unscaled_mask_h * current_scale_factor);

	float left_eye_cx = (float) cell_x + (base_pad_x + (base_eye_w / 2.0f)) * current_scale_factor;
	float right_eye_cx = (float) cell_x + (base_gap_x + (base_eye_w / 2.0f)) * current_scale_factor;
	float true_center_y
		= (float) cell_y + (base_pad_y + (base_eye_w / 2.0f)) * current_scale_factor;

	float phase_ratio = ((float) frame_idx + 0.5f) / (float) total_frames;
	float swing_angle = phase_ratio * 2.0f * (float) M_PI;
	float horizontal_look = sinf(swing_angle);

	// STEP 1: CLEAR TARGET ATLAS CELL CANVAS (TRANSPARENT BOUNDS INDEX)
	for (int y = cell_y; y < cell_y + scaled_cell_h; y++) {
		if (y >= atlas_h)
			continue;
		for (int x = cell_x; x < cell_x + cell_w; x++) {
			if (x >= atlas_w)
				continue;
			PlotSoftwarePixel(buffer, x, y, atlas_w, atlas_h, PALETTE_TRANSPARENT);
		}
	}

	// STEP 2: RASTERIZE SCLERAS MATCHING PRE-COMPUTED SYNCHRONIZED COMPOSITOR STRIDES
	if (ctx.clean_eye_mask) {
		int mask_bytes_per_row = (unscaled_mask_w + 7) / 8;
		int* x_starts = (int*) malloc(sizeof(int) * (unscaled_mask_w + 1));
		int* y_starts = (int*) malloc(sizeof(int) * (unscaled_mask_h + 1));
		int origin_px_x = (int) floorf(global_origin_x * current_scale_factor);
		int origin_px_y = (int) floorf(global_origin_y * current_scale_factor);

		for (int x = 0; x <= unscaled_mask_w; x++) {
			float absolute_head_x = global_origin_x + base_pad_x + (float) x;
			x_starts[x]
				= (int) floorf(absolute_head_x * current_scale_factor) - origin_px_x + cell_x;
		}
		for (int y = 0; y <= unscaled_mask_h; y++) {
			float absolute_head_y = global_origin_y + base_pad_y + (float) y;
			y_starts[y]
				= (int) floorf(absolute_head_y * current_scale_factor) - origin_px_y + cell_y;
		}

		int left_x0 = 0;
		int left_x1 = (int) base_eye_w;
		int right_x0 = (int) (base_gap_x - base_pad_x);
		int right_x1 = right_x0 + (int) base_eye_w;

		for (int unscaled_y = 0; unscaled_y < unscaled_mask_h; unscaled_y++) {
			int start_y = y_starts[unscaled_y];
			int end_y = y_starts[unscaled_y + 1];

			for (int unscaled_x = 0; unscaled_x < unscaled_mask_w; unscaled_x++) {
				bool in_left_rect = (unscaled_x >= left_x0 && unscaled_x < left_x1);
				bool in_right_rect = (unscaled_x >= right_x0 && unscaled_x < right_x1);

				if (in_left_rect || in_right_rect) {
					int byte_idx = (unscaled_y * mask_bytes_per_row) + (unscaled_x / 8);
					int bit_pos = unscaled_x % 8;

					// Directive 2: Target calculations directly against pristine eye mask
					bool is_sclera_pixel = (ctx.clean_eye_mask[byte_idx] & (1 << bit_pos)) != 0;

					// XBM format maps 0 as active ink; evaluate background state cleanly
					if (!is_sclera_pixel) {
						int start_x = x_starts[unscaled_x];
						int end_x = x_starts[unscaled_x + 1];

						for (int out_y = start_y; out_y < end_y; out_y++) {
							if (out_y >= atlas_h)
								continue;
							for (int out_x = start_x; out_x < end_x; out_x++) {
								if (out_x >= atlas_w)
									continue;
								// Assign Stateless Sclera Token ID 5 (0xFF)
								PlotSoftwarePixel(buffer, out_x, out_y, atlas_w, atlas_h, 0xFF);
							}
						}
					}
				}
			}
		}
		free(x_starts);
		free(y_starts);
	}

	// STEP 3: RASTERIZE ANIMATING PUPILS OVER THE PIXELATED CONTOUR BACKDROP
	SoftwareDrawPupilOval(buffer, left_eye_cx, true_center_y, pup_base_w, pup_base_h,
						  horizontal_look, max_offset_x, atlas_w, atlas_h, clip_x0, clip_y0,
						  clip_x1, clip_y1);
	SoftwareDrawPupilOval(buffer, right_eye_cx, true_center_y, pup_base_w, pup_base_h,
						  horizontal_look, max_offset_x, atlas_w, atlas_h, clip_x0, clip_y0,
						  clip_x1, clip_y1);
}
