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

void CatClock_ShaderHands(SDL_Renderer* renderer, int cell_w, int cell_h, float scale,
						  int frame_idx, void* userdata) {
	(void) scale;

	int hand_type = 0;
	if (userdata) {
		hand_type = *(int*) userdata;
	}

	int phase = frame_idx % TOTAL_PHASES;
	float angle_rad
		= ((float) phase / (float) TOTAL_PHASES) * 2.0f * (float) M_PI - ((float) M_PI / 2.0f);

	// COMPENSATED POSITIONING RESOLUTION: Shifting the center left by 1 pixel
	// to cancel out the floorf() truncation bug in the blit destination pass
	float center_x = ((float) cell_w / 2.0f) - (1.0f * scale);
	float center_y = (float) cell_h / 2.0f;

	// True anamorphic aspect ratio matching the 2:3 oval dial geometry bounds
	float aspect_x = 1.0f;
	float aspect_y = 1.5f;

	int max_vertices = 16;
	int max_indices = 32;
	CatClock_GpuVertex* gpu_vertices
		= (CatClock_GpuVertex*) SDL_malloc(sizeof(CatClock_GpuVertex) * max_vertices);
	int* indices = (int*) SDL_malloc(sizeof(int) * max_indices);

	if (!gpu_vertices || !indices) {
		if (gpu_vertices)
			SDL_free(gpu_vertices);
		if (indices)
			SDL_free(indices);
		return;
	}

	int v_count = 0;
	int i_count = 0;

	// Establish dimensional properties per hand type
	float base_half_width = 2.5f * scale;
	float tip_length = ((float) cell_w < (float) cell_h ? (float) cell_w : (float) cell_h) * 0.38f;
	float back_pivot_extension = 4.0f * scale;
	SDL_Color current_color = ctx.hour_color;

	if (hand_type == HAND_TYPE_MINUTE) {
		current_color = ctx.minute_color;
		base_half_width = 2.0f * scale;
		tip_length = ((float) cell_w < (float) cell_h ? (float) cell_w : (float) cell_h) * 0.31f;
		back_pivot_extension = 5.0f * scale;
	} else if (hand_type == HAND_TYPE_SECOND) {
		current_color = ctx.second_color;
		base_half_width = 1.0f * scale;
		tip_length = ((float) cell_w < (float) cell_h ? (float) cell_w : (float) cell_h) * 0.43f;
		back_pivot_extension = 6.0f * scale;
	} else {
		current_color = ctx.hour_color;
		base_half_width = 3.0f * scale;
		tip_length = ((float) cell_w < (float) cell_h ? (float) cell_w : (float) cell_h) * 0.33f;
		back_pivot_extension = 4.0f * scale;
	}

	float r_f = current_color.r / 255.0f;
	float g_f = current_color.g / 255.0f;
	float b_f = current_color.b / 255.0f;
	float a_f = current_color.a / 255.0f;

	if (base_half_width < 0.5f)
		base_half_width = 0.5f;

	float cos_a = cosf(angle_rad);
	float sin_a = sinf(angle_rad);

	float perp_x = -sin_a;
	float perp_y = cos_a;

	float tip_x = center_x + (cos_a * tip_length * aspect_x);
	float tip_y = center_y + (sin_a * tip_length * aspect_y);

	float back_x = -cos_a * back_pivot_extension * aspect_x;
	float back_y = -sin_a * back_pivot_extension * aspect_y;

	float base_l_x = center_x + back_x + (perp_x * base_half_width);
	float base_l_y = center_y + back_y + (perp_y * base_half_width);

	float base_r_x = center_x + back_x - (perp_x * base_half_width);
	float base_r_y = center_y + back_y - (perp_y * base_half_width);

	float global_alignment_offset_y = 0.0f * scale;

	// Vertex 0: Tip (Fixed Array Dimensions)
	gpu_vertices[v_count].position[0] = tip_x;
	gpu_vertices[v_count].position[1] = tip_y + global_alignment_offset_y;
	gpu_vertices[v_count].color[0] = r_f;
	gpu_vertices[v_count].color[1] = g_f;
	gpu_vertices[v_count].color[2] = b_f;
	gpu_vertices[v_count].color[3] = a_f;
	v_count++;

	// Vertex 1: Left Base
	gpu_vertices[v_count].position[0] = base_l_x;
	gpu_vertices[v_count].position[1] = base_l_y + global_alignment_offset_y;
	gpu_vertices[v_count].color[0] = r_f;
	gpu_vertices[v_count].color[1] = g_f;
	gpu_vertices[v_count].color[2] = b_f;
	gpu_vertices[v_count].color[3] = a_f;
	v_count++;

	// Vertex 2: Right Base
	gpu_vertices[v_count].position[0] = base_r_x;
	gpu_vertices[v_count].position[1] = base_r_y + global_alignment_offset_y;
	gpu_vertices[v_count].color[0] = r_f;
	gpu_vertices[v_count].color[1] = g_f;
	gpu_vertices[v_count].color[2] = b_f;
	gpu_vertices[v_count].color[3] = a_f;
	v_count++;

	// Fixed Pointer Element Dereferencing for Triangle Assignment
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	i_count = 3;

	SDL_Vertex* sdl_vertices = (SDL_Vertex*) SDL_malloc(sizeof(SDL_Vertex) * v_count);
	if (sdl_vertices) {
		for (int v = 0; v < v_count; v++) {
			sdl_vertices[v].position.x = gpu_vertices[v].position[0];
			sdl_vertices[v].position.y = gpu_vertices[v].position[1];
			sdl_vertices[v].color.r = (Uint8) (gpu_vertices[v].color[0] * 255.0f);
			sdl_vertices[v].color.g = (Uint8) (gpu_vertices[v].color[1] * 255.0f);
			sdl_vertices[v].color.b = (Uint8) (gpu_vertices[v].color[2] * 255.0f);
			sdl_vertices[v].color.a = (Uint8) (gpu_vertices[v].color[3] * 255.0f);
			sdl_vertices[v].tex_coord = (SDL_FPoint) { 0.0f, 0.0f };
		}

		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_RenderGeometry(renderer, NULL, sdl_vertices, v_count, indices, i_count);
		SDL_free(sdl_vertices);
	}

	SDL_free(gpu_vertices);
	SDL_free(indices);
}
