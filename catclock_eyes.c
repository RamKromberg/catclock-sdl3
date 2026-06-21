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
#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>

static void LookUpDrawHardwarePupilOval(SDL_Renderer* renderer, float cx, float cy, float r_base_w,
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

	vertices[0] = (SDL_Vertex) { { true_center_x, true_center_y }, opaque_col, { 0.0f, 0.0f } };

	float half_px = 0.5f / (scale <= 0.01f ? 1.0f : scale);

	for (int i = 0; i < segments; i++) {
		float angle = ((float) i / (float) segments) * 2.0f * (float) M_PI;
		float c = cosf(angle);
		float s = sinf(angle);

		float inner_w = fmaxf(0.1f, pup_w - half_px);
		float inner_h = fmaxf(0.1f, pup_h - half_px);
		float v1x = true_center_x + (inner_w * c);
		float v1y = true_center_y + (inner_h * s);
		vertices[1 + i] = (SDL_Vertex) { { v1x, v1y }, opaque_col, { 0.0f, 0.0f } };

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

	(void) cell_w;
	(void) cell_h;

	CatClock_RenderUniforms* uniforms = (CatClock_RenderUniforms*) userdata;
	float current_scale_factor
		= (uniforms && uniforms->current_scale > 0.01f) ? uniforms->current_scale : scale;

	SDL_Color sclera_rgba = { 255, 255, 255, 255 };
	SDL_Color pupil_rgba = { 0, 0, 0, 255 };
	SDL_Color body_rgba = { 0, 0, 0, 255 };

	if (uniforms) {
		sclera_rgba.r = (Uint8) (uniforms->sclera_color[0] * 255.0f);
		sclera_rgba.g = (Uint8) (uniforms->sclera_color[1] * 255.0f);
		sclera_rgba.b = (Uint8) (uniforms->sclera_color[2] * 255.0f);
		sclera_rgba.a = (Uint8) (uniforms->sclera_color[3] * 255.0f);

		pupil_rgba.r = (Uint8) (uniforms->pupil_color[0] * 255.0f);
		pupil_rgba.g = (Uint8) (uniforms->pupil_color[1] * 255.0f);
		pupil_rgba.b = (Uint8) (uniforms->pupil_color[2] * 255.0f);
		pupil_rgba.a = (Uint8) (uniforms->pupil_color[3] * 255.0f);

		body_rgba.r = (Uint8) (uniforms->cat_color[0] * 255.0f);
		body_rgba.g = (Uint8) (uniforms->cat_color[1] * 255.0f);
		body_rgba.b = (Uint8) (uniforms->cat_color[2] * 255.0f);
		body_rgba.a = (Uint8) (uniforms->cat_color[3] * 255.0f);
	} else {
		sclera_rgba = ctx.sclera_color;
		pupil_rgba = ctx.pupil_color;
		body_rgba = ctx.cat_color;
	}

	int total_fps_frames = target_fps_limit <= 0 ? 30 : target_fps_limit;
	int total_cycle_frames = total_fps_frames * 2;

	float phase_ratio = ((float) frame_idx + 0.5f) / (float) total_cycle_frames;
	float swing_angle = phase_ratio * 2.0f * (float) M_PI;
	float horizontal_look = sinf(swing_angle);

	float pup_base_w = 2.5f * current_scale_factor;
	float pup_base_h = 10.5f * current_scale_factor;
	float max_offset_x = 5.0f * current_scale_factor;

	float base_pad_x = 5.0f;
	float base_pad_y = 4.0f;
	float base_eye_w = 23.0f;
	float base_gap_x = 36.0f;

	float left_eye_cx = (base_pad_x + (base_eye_w / 2.0f)) * current_scale_factor;
	float right_eye_cx = (base_gap_x + (base_eye_w / 2.0f)) * current_scale_factor;
	float true_center_y = (base_pad_y + (base_eye_w / 2.0f)) * current_scale_factor;

	// STEP 1: INITIALIZE CANVAS WITH TRANSPARENT BACKDROP CLEAR
	SDL_SetTextureBlendMode(CatClock_GetXbmTextureLayer(ctx.xbm_lib, "eyes"), SDL_BLENDMODE_NONE);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_FRect transparent_bounds = { 0.0f, 0.0f, (float) cell_w, (float) cell_h };
	SDL_RenderFillRect(renderer, &transparent_bounds);

	// STEP 2: DRAW SOLID SCLERA PRIMITIVES
	SDL_SetRenderDrawColor(renderer, sclera_rgba.r, sclera_rgba.g, sclera_rgba.b, 255);

	SDL_FRect left_sclera_rect
		= { base_pad_x * current_scale_factor, base_pad_y * current_scale_factor,
			base_eye_w * current_scale_factor, base_eye_w * current_scale_factor };
	SDL_FRect right_sclera_rect
		= { base_gap_x * current_scale_factor, base_pad_y * current_scale_factor,
			base_eye_w * current_scale_factor, base_eye_w * current_scale_factor };
	SDL_RenderFillRect(renderer, &left_sclera_rect);
	SDL_RenderFillRect(renderer, &right_sclera_rect);

	// STEP 3: DRAW DUAL MOVING PUPILS
	LookUpDrawHardwarePupilOval(renderer, left_eye_cx, true_center_y, pup_base_w, pup_base_h,
								horizontal_look, max_offset_x, current_scale_factor, pupil_rgba);
	LookUpDrawHardwarePupilOval(renderer, right_eye_cx, true_center_y, pup_base_w, pup_base_h,
								horizontal_look, max_offset_x, current_scale_factor, pupil_rgba);

	// STEP 4: STAMP MASK VIA SELECTIVE UV CLIPPING TO ISOLATE THE ACTIVE 54x23 REGION
	if (ctx.xbm_lib) {
		SDL_Texture* mask_tex = CatClock_GetXbmTextureLayer(ctx.xbm_lib, "eyes");
		if (mask_tex) {
			SDL_SetTextureBlendMode(mask_tex, SDL_BLENDMODE_BLEND);
			SDL_SetTextureColorMod(mask_tex, body_rgba.r, body_rgba.g, body_rgba.b);
			SDL_SetTextureAlphaMod(mask_tex, 255);

			// Define the source coordinates rectangle to crop the un-padded 54x23 section out of
			// the 64-pixel atlas source
			SDL_FRect mask_src = { 0.0f, 0.0f, 54.0f, 23.0f };

			// Project the 54x23 sub-region onto its intended position inside the cell, matching the
			// sclera padding offsets
			SDL_FRect mask_dst
				= { base_pad_x * current_scale_factor, base_pad_y * current_scale_factor,
					54.0f * current_scale_factor, 23.0f * current_scale_factor };

			SDL_RenderTexture(renderer, mask_tex, &mask_src, &mask_dst);
		}
	}
}
