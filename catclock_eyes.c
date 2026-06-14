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
									   SDL_Color color) {
	float ox = look_x * max_offset_x;
	float oy = 0.0f;

	float compression = sqrtf(fmaxf(0.1f, 1.0f - (look_x * look_x * 0.45f)));
	float pup_w = r_base_w * compression;
	float pup_h = r_base_h;

	int segments = 24;
	int total_vertices = segments + 1;
	SDL_Vertex* vertices = (SDL_Vertex*) SDL_malloc(sizeof(SDL_Vertex) * total_vertices);
	int* indices = (int*) SDL_malloc(sizeof(int) * segments * 3);
	if (!vertices || !indices) {
		if (vertices)
			SDL_free(vertices);
		if (indices)
			SDL_free(indices);
		return;
	}

	SDL_FColor fcol = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };
	vertices[0] = (SDL_Vertex) { { cx + ox, cy + oy }, fcol, { 0.0f, 0.0f } };

	for (int i = 0; i < segments; i++) {
		float angle = ((float) i / (float) segments) * 2.0f * (float) M_PI;
		float vx = (cx + ox) + (pup_w * cosf(angle));
		float vy = (cy + oy) + (pup_h * sinf(angle));
		vertices[i + 1] = (SDL_Vertex) { { vx, vy }, fcol, { 0.0f, 0.0f } };

		indices[i * 3] = 0;
		indices[i * 3 + 1] = i + 1;
		indices[i * 3 + 2] = (i == segments - 1) ? 1 : i + 2;
	}

	SDL_RenderGeometry(renderer, NULL, vertices, total_vertices, indices, segments * 3);
	SDL_free(vertices);
	SDL_free(indices);
}

void CatClock_ShaderEyes(SDL_Renderer* renderer, int cell_w, int cell_h, float scale, int frame_idx,
						 void* userdata) {
	if (!renderer)
		return;
	(void) userdata;
	int req_frames = target_fps_limit <= 0 ? 60 : target_fps_limit;

	float phase_ratio = (float) frame_idx / (float) req_frames;
	float swing_angle = phase_ratio * 2.0f * (float) M_PI;

	float base_w = 2.5f * scale;
	float base_h = 10.5f * scale;
	float max_offset_x = 7.0f * scale;

	SDL_Color active_pupil_color = ctx.pupil_color;
	SDL_Color active_sclera_color = ctx.sclera_color;

	float true_center_x = (float) (cell_w / 2);
	float true_center_y = (float) (cell_h / 2);

	/*
	 * SHADER RECTANGLE BOUNDS ADJUSTMENT (23x23 ALIGNMENT):
	 * The original socket asset is 23 pixels wide, but the cell framework is 24x24.
	 * We subtract 1.0f * scale from the width to prevent the extra pixel from leaking color.
	 * Offsetting the X-coordinate rightward by 0.5f * scale ensures the background fill is
	 * perfectly centered over the 23-pixel socket shape with zero gaps on either side!
	 */
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
	SDL_SetRenderDrawColor(renderer, active_sclera_color.r, active_sclera_color.g,
						   active_sclera_color.b, 255);

	SDL_FRect local_cell_bounds
		= { 0.5f * scale, 0.0f, (float) cell_w - (2.0f * scale), (float) cell_h };
	SDL_RenderFillRect(renderer, &local_cell_bounds);

	/* Restore normal drawing blend mode to layer the moving pupil cleanly on top */
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	/* Layer the moving geometric pupil directly over our custom sclera background color */
	LocalDrawHardwarePupilOval(renderer, true_center_x, true_center_y, base_w, base_h,
							   sinf(swing_angle), max_offset_x, active_pupil_color);
}
