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

#define STEM_VERTICES 4
#define RING_SEGMENTS 24
#define RING_VERTICES (RING_SEGMENTS * 2)
#define CAP_SEGMENTS 16
#define CAP_VERTICES (CAP_SEGMENTS + 1)

#define TOTAL_VERTICES (STEM_VERTICES + RING_VERTICES + CAP_VERTICES)
#define TOTAL_TRIANGLES (2 + (RING_SEGMENTS - 1) * 2 + (CAP_SEGMENTS - 1))

void CatClock_ShaderTail(SDL_Renderer* renderer, int cell_w, int cell_h, float scale, int frame_idx,
						 void* userdata) {
	(void) cell_h;
	(void) scale;

	SDL_Color* fg_color = (SDL_Color*) userdata;
	SDL_Color color = fg_color ? *fg_color : (SDL_Color) { 0, 0, 0, 255 };

	/* ========================================================================
	   TUNING BAY: MANUAL OSCILLATION GOVERNORS
	   ======================================================================== */
	/* 1. Total number of animation slices pre-baked into the target atlas texture. */
	float TOTAL_CYCLE_FRAMES = 60.0f;

	/* 2. The maximum negative deflection angle (reaching toward screen's left / cat's right). */
	float MAX_LEFT_ANGLE_DEG = -19.5f;

	/* 3. The maximum positive deflection angle (reaching toward screen's right / cat's left). */
	float MAX_RIGHT_ANGLE_DEG = 30.0f;

	/* ========================================================================
	   TRIGONOMETRIC TRANSLATION PHASE
	   ======================================================================== */
	/* Map frame index cleanly over a normalized 0.0 to 1.0 cycle ratio */
	float progress = (float) frame_idx / TOTAL_CYCLE_FRAMES;

	/* Convert phase ratio to standard sinusoidal oscillator [-1.0, 1.0] */
	float sin_wave = sinf(progress * 2.0f * (float) M_PI);

	/* Map the symmetric wave into your distinct manual angle limits */
	/* Shift wave from [-1, 1] to [0, 1] range */
	float normalized_wave = (sin_wave + 1.0f) / 2.0f;

	/* Linearly interpolate directly between your governed left and right angle targets */
	float final_angle_deg
		= MAX_LEFT_ANGLE_DEG + (normalized_wave * (MAX_RIGHT_ANGLE_DEG - MAX_LEFT_ANGLE_DEG));

	/* Convert the evaluated angle to radians for the 2D transformation matrix */
	float rad = (final_angle_deg * (float) M_PI) / 180.0f;
	float cos_a = cosf(rad);
	float sin_a = sinf(rad);

	/* ========================================================================
	   GEOMETRY RENDERING BLOCK (STABLE)
	   ======================================================================== */
	float internal_unit_ratio = (float) cell_w / 96.0f;

	/* Pivot point anchors perfectly on the true mathematical center of our verified 96x96 grid */
	/* ADJUSTMENT: Applied -1.0px leftward bias to center the origin and prevent right-swing
	 * scissoring! */
	float pivot_x = ((float) cell_w / 2.0f) - (1.0f * internal_unit_ratio);
	float pivot_y = 12.0f * internal_unit_ratio;

	/* Proportional thickness metrics matching the approved visual look */
	float half_width = 6.5f * internal_unit_ratio;
	float stem_start_y = -8.5f * internal_unit_ratio;
	float stem_end_y = 62.0f * internal_unit_ratio;

	float outer_radius = 17.0f * internal_unit_ratio;
	float inner_radius = 4.5f * internal_unit_ratio;
	float loop_center_x = 11.0f * internal_unit_ratio;
	float loop_center_y = stem_end_y;

	SDL_Vertex* vertices = (SDL_Vertex*) SDL_malloc(sizeof(SDL_Vertex) * TOTAL_VERTICES);
	if (!vertices)
		return;

	SDL_FColor fcol = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };
	for (int i = 0; i < TOTAL_VERTICES; i++) {
		vertices[i].color = fcol;
		vertices[i].tex_coord.x = 0.0f;
		vertices[i].tex_coord.y = 0.0f;
	}

	int v_ptr = 0;

	/* COMPONENT A: The Straight Vertical Stem */
	float s0_x = -half_width, s0_y = stem_start_y;
	float s1_x = half_width, s1_y = stem_start_y;
	float s2_x = loop_center_x - outer_radius, s2_y = stem_end_y;
	float s3_x = loop_center_x - inner_radius, s3_y = stem_end_y;

	vertices[v_ptr++].position = (SDL_FPoint) { pivot_x + (s0_x * cos_a - s0_y * sin_a),
												pivot_y + (s0_x * sin_a + s0_y * cos_a) };
	vertices[v_ptr++].position = (SDL_FPoint) { pivot_x + (s1_x * cos_a - s1_y * sin_a),
												pivot_y + (s1_x * sin_a + s1_y * cos_a) };
	vertices[v_ptr++].position = (SDL_FPoint) { pivot_x + (s2_x * cos_a - s2_y * sin_a),
												pivot_y + (s2_x * sin_a + s2_y * cos_a) };
	vertices[v_ptr++].position = (SDL_FPoint) { pivot_x + (s3_x * cos_a - s3_y * sin_a),
												pivot_y + (s3_x * sin_a + s3_y * cos_a) };

	/* COMPONENT B: 180-Degree Ring Sector Loop */
	int ring_start_v = v_ptr;
	for (int i = 0; i < RING_SEGMENTS; i++) {
		float t_seg = (float) i / (float) (RING_SEGMENTS - 1);
		float sweep_rad = (float) M_PI - (t_seg * (float) M_PI);
		float cos_s = cosf(sweep_rad);
		float sin_s = sinf(sweep_rad);

		float ox = loop_center_x + outer_radius * cos_s;
		float oy = loop_center_y + outer_radius * sin_s;
		float ix = loop_center_x + inner_radius * cos_s;
		float iy = loop_center_y + inner_radius * sin_s;

		vertices[v_ptr++].position = (SDL_FPoint) { pivot_x + (ox * cos_a - oy * sin_a),
													pivot_y + (ox * sin_a + oy * cos_a) };
		vertices[v_ptr++].position = (SDL_FPoint) { pivot_x + (ix * cos_a - iy * sin_a),
													pivot_y + (ix * sin_a + iy * cos_a) };
	}

	/* COMPONENT C: Symmetrical Semi-Circular End Cap Fan */
	int cap_start_v = v_ptr;
	float cap_center_x = loop_center_x + ((outer_radius + inner_radius) / 2.0f);
	float cap_center_y = loop_center_y;

	vertices[v_ptr++].position
		= (SDL_FPoint) { pivot_x + (cap_center_x * cos_a - cap_center_y * sin_a),
						 pivot_y + (cap_center_x * sin_a + cap_center_y * cos_a) };

	float cap_radius = (outer_radius - inner_radius) / 2.0f;
	for (int i = 0; i < CAP_SEGMENTS; i++) {
		float t_cap = (float) i / (float) (CAP_SEGMENTS - 1);
		float cap_rad = -(float) M_PI + (t_cap * ((float) M_PI + 0.05f));
		float cx = cap_center_x + cap_radius * cosf(cap_rad);
		float cy = cap_center_y + cap_radius * sinf(cap_rad);

		vertices[v_ptr++].position = (SDL_FPoint) { pivot_x + (cx * cos_a - cy * sin_a),
													pivot_y + (cx * sin_a + cy * cos_a) };
	}

	/* Index Stitching Array Compilation */
	int* indices = (int*) SDL_malloc(sizeof(int) * TOTAL_TRIANGLES * 3);
	if (indices) {
		int idx = 0;

		/* Triangle Strip A: Vertical Stem */
		indices[idx++] = 0;
		indices[idx++] = 1;
		indices[idx++] = 2;
		indices[idx++] = 1;
		indices[idx++] = 3;
		indices[idx++] = 2;

		/* Triangle Strip B: 180-Degree Ring Sector Loop */
		for (int i = 0; i < RING_SEGMENTS - 1; i++) {
			int curr_o = ring_start_v + i * 2;
			int curr_i = ring_start_v + i * 2 + 1;
			int next_o = ring_start_v + (i + 1) * 2;
			int next_i = ring_start_v + (i + 1) * 2 + 1;

			indices[idx++] = curr_o;
			indices[idx++] = curr_i;
			indices[idx++] = next_o;
			indices[idx++] = curr_i;
			indices[idx++] = next_i;
			indices[idx++] = next_o;
		}

		/* Triangle Fan C: Smooth Round Tip Cap */
		for (int i = 0; i < CAP_SEGMENTS - 1; i++) {
			indices[idx++] = cap_start_v;
			indices[idx++] = cap_start_v + 1 + i;
			indices[idx++] = cap_start_v + 2 + i;
		}

		SDL_RenderGeometry(renderer, NULL, vertices, TOTAL_VERTICES, indices, TOTAL_TRIANGLES * 3);
		SDL_free(indices);
	}

	SDL_free(vertices);
}
