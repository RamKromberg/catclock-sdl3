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
#include <stdlib.h>

#define RING_SEGMENTS 36
#define CAP_SEGMENTS 17

void CatClock_ShaderTail(SDL_Renderer* renderer, int cell_w, int cell_h, float scale, int frame_idx,
						 void* userdata) {
	(void) cell_h;
	(void) scale;

	CatClock_TailShaderArgs* args = (CatClock_TailShaderArgs*) userdata;

	float shift_x = args ? args->offset_x : 0.0f;
	float shift_y = args ? args->offset_y : 0.0f;

	bool is_halo = args && args->force_halo_color;
	SDL_Color color_bits;

	if (is_halo) {
		color_bits = (SDL_Color) { 255, 255, 255, 255 };
	} else if (args && args->app_ctx) {
		color_bits = args->app_ctx->cat_color;
	} else {
		color_bits = (SDL_Color) { 0, 0, 0, 255 };
	}

	float r_f = color_bits.r / 255.0f;
	float g_f = color_bits.g / 255.0f;
	float b_f = color_bits.b / 255.0f;
	float a_f = color_bits.a / 255.0f;

	int total_fps_frames = target_fps_limit <= 0 ? 30 : target_fps_limit;
	float total_cycle_frames = (float) (total_fps_frames * 2);

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

	float internal_unit_ratio = (float) cell_w / 96.0f;
	float pivot_x = ((float) cell_w / 2.0f) - (1.0f * internal_unit_ratio) + shift_x;
	float pivot_y = (12.0f * internal_unit_ratio) + shift_y;

	float horizontal_cushion = is_halo ? (0.35f * internal_unit_ratio) : 0.0f;
	float half_width = (6.5f * internal_unit_ratio) + horizontal_cushion;
	float outer_radius = 16.5f * internal_unit_ratio;
	float inner_radius = 3.5f * internal_unit_ratio;

	float stem_start_y = -8.5f * internal_unit_ratio;
	float stem_end_y = 62.0f * internal_unit_ratio;
	float capsule_length_stretch = 6.5f * internal_unit_ratio;

	float loop_center_x = (outer_radius + horizontal_cushion) - half_width;
	float loop_center_y = stem_end_y;

	float cap_center_x = loop_center_x + ((outer_radius + inner_radius) / 2.0f);
	float cap_center_y = loop_center_y - capsule_length_stretch;
	float cap_radius = (outer_radius - inner_radius) / 2.0f;

	int max_v = 150;
	CatClock_GpuVertex* gpu_vertices
		= (CatClock_GpuVertex*) SDL_malloc(sizeof(CatClock_GpuVertex) * max_v);
	int* indices = (int*) SDL_malloc(sizeof(int) * max_v * 6);

	if (!gpu_vertices || !indices) {
		if (gpu_vertices)
			SDL_free(gpu_vertices);
		if (indices)
			SDL_free(indices);
		return;
	}

	int v_count = 0;
	int i_count = 0;

// Software pipeline lambda routine to populate clean sequential GPU vertex layouts
#define ADD_GPU_VTX(lx, ly)                                                                        \
	do {                                                                                           \
		if (v_count < max_v) {                                                                     \
			gpu_vertices[v_count].position[0] = pivot_x + ((lx) * cos_a - (ly) * sin_a);           \
			gpu_vertices[v_count].position[1] = pivot_y + ((lx) * sin_a + (ly) * cos_a);           \
			gpu_vertices[v_count].color[0] = r_f;                                                  \
			gpu_vertices[v_count].color[1] = g_f;                                                  \
			gpu_vertices[v_count].color[2] = b_f;                                                  \
			gpu_vertices[v_count].color[3] = a_f;                                                  \
			v_count++;                                                                             \
		}                                                                                          \
	} while (0)

	// PART 1: STEM BODY
	int s0 = v_count;
	ADD_GPU_VTX(-half_width, stem_start_y);
	int s1 = v_count;
	ADD_GPU_VTX(half_width, stem_start_y);
	int s2 = v_count;
	ADD_GPU_VTX(-half_width, stem_end_y);
	int s3 = v_count;
	ADD_GPU_VTX(half_width, stem_end_y);

	indices[i_count++] = s0;
	indices[i_count++] = s1;
	indices[i_count++] = s2;
	indices[i_count++] = s1;
	indices[i_count++] = s3;
	indices[i_count++] = s2;

	// PART 2: THE CURVED BOTTOM RING HOOK
	int ring_v_start = v_count;
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

		ADD_GPU_VTX(ox, oy);
		ADD_GPU_VTX(ix, iy);
	}

	for (int i = 0; i < RING_SEGMENTS - 1; i++) {
		int r_o0 = ring_v_start + (i * 2);
		int r_i0 = ring_v_start + (i * 2) + 1;
		int r_o1 = ring_v_start + ((i + 1) * 2);
		int r_i1 = ring_v_start + ((i + 1) * 2) + 1;

		indices[i_count++] = r_o0;
		indices[i_count++] = r_i0;
		indices[i_count++] = r_o1;
		indices[i_count++] = r_i0;
		indices[i_count++] = r_i1;
		indices[i_count++] = r_o1;
	}

	// PART 3: THE ELONGATED STRAIGHT EXTENSION
	int eo0 = ring_v_start + (RING_SEGMENTS - 1) * 2;
	int ei0 = ring_v_start + (RING_SEGMENTS - 1) * 2 + 1;

	float ex1 = loop_center_x + outer_radius;
	float ey1 = loop_center_y - capsule_length_stretch;
	float ex0 = loop_center_x + inner_radius;
	float ey0 = loop_center_y - capsule_length_stretch;

	int eo1 = v_count;
	ADD_GPU_VTX(ex1, ey1);
	int ei1 = v_count;
	ADD_GPU_VTX(ex0, ey0);

	indices[i_count++] = eo0;
	indices[i_count++] = ei0;
	indices[i_count++] = eo1;
	indices[i_count++] = ei0;
	indices[i_count++] = ei1;
	indices[i_count++] = eo1;

	// PART 4: THE ROUNDED CAPSULE END-CAP TIP
	int cap_center = v_count;
	ADD_GPU_VTX(cap_center_x, cap_center_y);
	int cap_arc_start = v_count;

	for (int i = 0; i < CAP_SEGMENTS; i++) {
		float t = (float) i / (float) (CAP_SEGMENTS - 1);
		float cap_arc_rad = t * (float) M_PI;
		float cx = cap_center_x + cap_radius * cosf(cap_arc_rad);
		float cy = cap_center_y - cap_radius * sinf(cap_arc_rad);
		ADD_GPU_VTX(cx, cy);
	}

	for (int i = 0; i < CAP_SEGMENTS - 1; i++) {
		indices[i_count++] = cap_center;
		indices[i_count++] = cap_arc_start + i;
		indices[i_count++] = cap_arc_start + i + 1;
	}

	// Terminal Rasterization Translation Pass mapping GpuVertex blocks into native SDL // Terminal
	// Rasterization Translation Pass mapping GpuVertex blocks into native SDL components
	SDL_Vertex* sdl_vertices = (SDL_Vertex*) SDL_malloc(sizeof(SDL_Vertex) * v_count);
	if (sdl_vertices) {
		for (int v = 0; v < v_count; v++) {
			sdl_vertices[v].position.x = gpu_vertices[v].position[0];
			sdl_vertices[v].position.y = gpu_vertices[v].position[1];
			sdl_vertices[v].color.r = gpu_vertices[v].color[0];
			sdl_vertices[v].color.g = gpu_vertices[v].color[1];
			sdl_vertices[v].color.b = gpu_vertices[v].color[2];
			sdl_vertices[v].color.a = gpu_vertices[v].color[3];
			sdl_vertices[v].tex_coord = (SDL_FPoint) { 0.0f, 0.0f };
		}

		if (is_halo) {
			SDL_SetTextureBlendMode(CatClock_GetXbmTextureLayer(ctx.xbm_lib, "catback"),
									SDL_BLENDMODE_BLEND);
		} else {
			SDL_SetTextureBlendMode(CatClock_GetXbmTextureLayer(ctx.xbm_lib, "catback"),
									SDL_BLENDMODE_NONE);
		}

		SDL_RenderGeometry(renderer, NULL, sdl_vertices, v_count, indices, i_count);
		SDL_free(sdl_vertices);
	}

#undef ADD_GPU_VTX
	SDL_free(indices);
	SDL_free(gpu_vertices);
}
