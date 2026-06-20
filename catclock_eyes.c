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

// clang-format off
#include "catclock_shared.h"
#include "catclock_atlas.h"
// clang-format on
#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>

static void LocalDrawHardwarePupilOval(SDL_Renderer* renderer, float cx, float cy, float r_base_w,
									   float r_base_h, float look_x, float max_offset_x,
									   float scale, SDL_Color color) {
	float true_center_x = cx + (look_x * max_offset_x);
	float true_center_y = cy;

	float compression_w = sqrtf(fmaxf(0.1f, 1.0f - (look_x * look_x * 0.45f)));
	float compression_h = sqrtf(fmaxf(0.1f, 1.0f - (look_x * look_x * 0.225f)));
	float pup_w = r_base_w * compression_w;
	float pup_h = r_base_h * compression_h;

	int segments = 32;
	int total_vertices = (segments * 2) + 1;
	int total_indices = segments * 9;

	SDL_Vertex* vertices = (SDL_Vertex*) SDL_malloc(sizeof(SDL_Vertex) * total_vertices);
	int* indices = (int*) SDL_malloc(sizeof(int) * total_indices);
	if (!vertices || !indices) {
		if (vertices)
			SDL_free(vertices);
		if (indices)
			SDL_free(indices);
		return;
	}

	float r_f = color.r / 255.0f;
	float g_f = color.g / 255.0f;
	float b_f = color.b / 255.0f;
	float a_f = color.a / 255.0f;

	SDL_FColor opaque_col = { r_f, g_f, b_f, a_f };
	SDL_FColor trans_col = { r_f, g_f, b_f, 0.0f };

	/* Target the zero slot index element explicitly */
	vertices[0] = (SDL_Vertex) { { true_center_x, true_center_y }, opaque_col, { 0.0f, 0.0f } };

	/* Constant-width multi-ring edge loop offset by half a physical pixel */
	float half_px = 0.5f / (scale <= 0.01f ? 1.0f : scale);

	for (int i = 0; i < segments; i++) {
		float angle = ((float) i / (float) segments) * 2.0f * (float) M_PI;
		float c = cosf(angle);
		float s = sinf(angle);

		/* Inner Solid Rim Boundary (-0.5 physical pixels) */
		float inner_w = fmaxf(0.1f, pup_w - half_px);
		float inner_h = fmaxf(0.1f, pup_h - half_px);
		float v1x = true_center_x + (inner_w * c);
		float v1y = true_center_y + (inner_h * s);
		vertices[1 + i] = (SDL_Vertex) { { v1x, v1y }, opaque_col, { 0.0f, 0.0f } };

		/* Outer Transparent Rim Boundary (+0.5 physical pixels) */
		float outer_w = pup_w + half_px;
		float outer_h = pup_h + half_px;
		float v2x = true_center_x + (outer_w * c);
		float v2y = true_center_y + (outer_h * s);
		vertices[1 + segments + i] = (SDL_Vertex) { { v2x, v2y }, trans_col, { 0.0f, 0.0f } };

		int next = (i == segments - 1) ? 0 : i + 1;

		indices[i * 9 + 0] = 0;
		indices[i * 9 + 1] = 1 + i;
		indices[i * 9 + 2] = 1 + next;

		indices[i * 9 + 3] = 1 + i;
		indices[i * 9 + 4] = 1 + segments + i;
		indices[i * 9 + 5] = 1 + segments + next;

		indices[i * 9 + 6] = 1 + i;
		indices[i * 9 + 7] = 1 + segments + next;
		indices[i * 9 + 8] = 1 + next;
	}

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_RenderGeometry(renderer, NULL, vertices, total_vertices, indices, total_indices);

	SDL_free(vertices);
	SDL_free(indices);
}

void CatClock_ShaderEyes(SDL_Renderer* renderer, int cell_w, int cell_h, float scale, int frame_idx,
						 void* userdata) {
	if (!renderer)
		return;
	(void) userdata;
	(void) cell_w;
	(void) cell_h;

	int total_fps_frames = target_fps_limit <= 0 ? 30 : target_fps_limit;
	int total_cycle_frames = total_fps_frames * 2;

	/* --- ENHANCED PUPIL TIMELINE CORRECTION --- */
	// Offset the discrete integer index to the temporal center (+0.5f) of the slice.
	// This distributes the horizontal sine increments symmetrically across the pre-baked atlas
	// cells.
	float phase_ratio = ((float) frame_idx + 0.5f) / (float) total_cycle_frames;
	float swing_angle = phase_ratio * 2.0f * (float) M_PI;
	float horizontal_look = sinf(swing_angle);

	float base_w = 2.5f * scale;
	float base_h = 10.5f * scale;
	float max_offset_x = 5.0f * scale;

	SDL_Color active_pupil_color = ctx.pupil_color;
	SDL_Color active_sclera_color = ctx.sclera_color;
	SDL_Color active_cat_color = ctx.cat_color;

	float base_pad_x = 5.0f;
	float base_pad_y = 4.0f;
	float base_eye_w = 23.0f;
	float base_gap_x = 36.0f;

	/* Symmetrical centers tracking from scaled metrics */
	float left_eye_cx = (base_pad_x + (base_eye_w / 2.0f)) * scale;
	float right_eye_cx = (base_gap_x + (base_eye_w / 2.0f)) * scale;
	float true_center_y = (base_pad_y + (base_eye_w / 2.0f)) * scale;

	float mask_dst_x = base_pad_x * scale;
	float mask_dst_y = base_pad_y * scale;

	// STEP 1: INITIALIZE CANVAS WITH TRANSPARENT BACKDROPS
	SDL_SetTextureBlendMode(CatClock_GetXbmTextureLayer(ctx.xbm_lib, "eyes"), SDL_BLENDMODE_NONE);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_FRect transparent_bounds = { 0.0f, 0.0f, roundf(64.0f * scale), roundf(32.0f * scale) };
	SDL_RenderFillRect(renderer, &transparent_bounds);

	// STEP 2: DRAW OUTWARD-ONLY INDEPENDENT GUARD RECTANGLES
	SDL_SetTextureBlendMode(CatClock_GetXbmTextureLayer(ctx.xbm_lib, "eyes"), SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, active_cat_color.r, active_cat_color.g, active_cat_color.b,
						   255);

	SDL_FRect left_guard_rect = { roundf((base_pad_x - 1.0f) * scale), roundf(base_pad_y * scale),
								  roundf((base_eye_w + 1.0f) * scale), roundf(base_eye_w * scale) };
	SDL_FRect right_guard_rect
		= { roundf(base_gap_x * scale), roundf(base_pad_y * scale),
			roundf((base_eye_w + 1.0f) * scale), roundf(base_eye_w * scale) };
	SDL_RenderFillRect(renderer, &left_guard_rect);
	SDL_RenderFillRect(renderer, &right_guard_rect);

	// STEP 3: DRAW SOLID SCLERA PRIMITIVE RECTANGLES
	SDL_SetRenderDrawColor(renderer, active_sclera_color.r, active_sclera_color.g,
						   active_sclera_color.b, 255);

	SDL_FRect left_sclera_rect = { roundf(base_pad_x * scale), roundf(base_pad_y * scale),
								   roundf(base_eye_w * scale), roundf(base_eye_w * scale) };
	SDL_FRect right_sclera_rect = { roundf(base_gap_x * scale), roundf(base_pad_y * scale),
									roundf(base_eye_w * scale), roundf(base_eye_w * scale) };
	SDL_RenderFillRect(renderer, &left_sclera_rect);
	SDL_RenderFillRect(renderer, &right_sclera_rect);

	// STEP 4: DRAW DUAL MOVING PUPILS
	LocalDrawHardwarePupilOval(renderer, left_eye_cx, true_center_y, base_w, base_h,
							   horizontal_look, max_offset_x, scale, active_pupil_color);
	LocalDrawHardwarePupilOval(renderer, right_eye_cx, true_center_y, base_w, base_h,
							   horizontal_look, max_offset_x, scale, active_pupil_color);

	// STEP 5: STAMP ALPHA FACEPLATE OVERLAY LAST
	if (ctx.xbm_lib) {
		SDL_Texture* mask_tex = CatClock_GetXbmTextureLayer(ctx.xbm_lib, "eyes");
		if (mask_tex) {
			SDL_SetTextureBlendMode(mask_tex, SDL_BLENDMODE_BLEND);
			SDL_SetTextureColorMod(mask_tex, active_cat_color.r, active_cat_color.g,
								   active_cat_color.b);
			SDL_SetTextureAlphaMod(mask_tex, 255);

			SDL_FRect mask_dst = { roundf(mask_dst_x), roundf(mask_dst_y), roundf(54.0f * scale),
								   roundf(base_eye_w * scale) };
			SDL_RenderTexture(renderer, mask_tex, NULL, &mask_dst);
		}
	}
}
