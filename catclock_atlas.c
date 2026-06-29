/******************************************************************************
 * File Name:    catclock_atlas.c
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
#include <string.h>

/* ==========================================================================
   1. CORE PRIMITIVE VECTOR RASTERIZATION ENGINE
   ========================================================================== */

/**
 * PlotSoftwarePixel
 * Direct un-interpolated index writer mapping 8-bit palette tokens to the canvas.
 * Isolated strictly to the texture atlas compilation pass.
 */
void PlotSoftwarePixel(uint8_t* buffer, int x, int y, int width, int height, uint8_t token) {
	if (x >= 0 && x < width && y >= 0 && y < height) {
		buffer[(y * width) + x] = token;
	}
}

/**
 * FillSoftwareTriangle
 * Standard top/bottom-split flat triangle rasterizer. Fixed to prevent
 * sub-pixel gap bleeding across complex multi-triangle hand layouts.
 */
void FillSoftwareTriangle(uint8_t* buffer, int x0, int y0, int x1, int y1, int x2, int y2,
						  int width, int height, uint8_t token) {
	/* Sort vertices vertically by Y coordinate: y0 <= y1 <= y2 */
	if (y0 > y1) {
		int tx = x0;
		x0 = x1;
		x1 = tx;
		int ty = y0;
		y0 = y1;
		y1 = ty;
	}
	if (y0 > y2) {
		int tx = x0;
		x0 = x2;
		x2 = tx;
		int ty = y0;
		y0 = y2;
		y2 = ty;
	}
	if (y1 > y2) {
		int tx = x1;
		x1 = x2;
		x2 = tx;
		int ty = y1;
		y1 = y2;
		y2 = ty;
	}

	if (y0 == y2)
		return;

	int total_height = y2 - y0;
	for (int i = 0; i < total_height; i++) {
		int current_y = y0 + i;
		if (current_y < 0 || current_y >= height)
			continue;

		bool second_half = i > (y1 - y0) || y1 == y0;
		int segment_height = second_half ? (y2 - y1) : (y1 - y0);
		if (segment_height == 0)
			continue;

		float alpha = (float) i / (float) total_height;
		float beta = (float) (i - (second_half ? (y1 - y0) : 0)) / (float) segment_height;

		int ax = x0 + (int) ((float) (x2 - x0) * alpha);
		int bx = second_half ? (x1 + (int) ((float) (x2 - x1) * beta))
							 : (x0 + (int) ((float) (x1 - x0) * beta));

		if (ax > bx) {
			int tx = ax;
			ax = bx;
			bx = tx;
		}

		/* Enforce tight bounding box clamping across the horizontal fill span */
		int start_x = ax < 0 ? 0 : ax;
		int end_x = bx >= width ? (width - 1) : bx;

		for (int cx = start_x; cx <= end_x; cx++) {
			buffer[(current_y * width) + cx] = token;
		}
	}
}

/* ==========================================================================
   2. PIPELINE ATLAS MATRIX AND COMPILATION LIFECYCLE MANAGEMENT
   ========================================================================== */

/**
 * CompareFloats
 * Standard qsort comparison helper for floating-point bounds validation.
 */
int CompareFloats(const void* a, const void* b) {
	float fa = *(const float*) a;
	float fb = *(const float*) b;
	return (fa > fb) - (fa < fb);
}

/**
 * CatClock_OnWindowResize
 * Responds to OS-level window resize signals. Marks the GPU texture caches
 * as stale to force a deterministic pipeline rebake on the next tick.
 */
void CatClock_OnWindowResize(SDL_WindowEvent* resize_event, CatClock_AppContext* context,
							 void* renderer) {
	(void) resize_event;
	(void) renderer;
	if (context) {
		context->texture_cache_stale = true;
	}
}

/**
 * CatClock_DestroyComputeAtlas
 * Safely releases internal multi-frame CPU configuration layers, pre-normalized
 * float bounding boxes, and texture sheets before reallocating.
 */
void CatClock_DestroyComputeAtlas(CatClock_ComputeAtlas* atlas) {
	if (!atlas)
		return;

	if (atlas->src_rects) {
		free(atlas->src_rects);
		atlas->src_rects = NULL;
	}
	if (atlas->index_buffer) {
		free(atlas->index_buffer);
		atlas->index_buffer = NULL;
	}

	atlas->total_frames = 0;
	atlas->cell_w = 0;
	atlas->cell_h = 0;
	atlas->atlas_w = 0;
	atlas->atlas_h = 0;
	atlas->last_scale = -1.0f;
}

/**
 * CatClock_RebakeComputeAtlas
 * Compiles mathematical layout paths onto structured VRAM-ready sheets.
 * Executes automated blueprint file dumps to disk for structural validation.
 */
void CatClock_RebakeComputeAtlas(void* renderer, CatClock_ComputeAtlas* atlas, int cell_base_w,
								 int cell_base_h, int total_frames, int cols,
								 CatClock_ShaderCallback shader, void* userdata) {
	(void) renderer;
	(void) cols;

	float scale = (float) ctx.current_half_steps / 2.0f;
	if (scale <= 0.001f) {
		scale = 1.0f;
	}

	int forced_cols = 10;

	/* Evaluate structure boundaries--reallocate sheet space if dimensions or scales shift */
	if (!atlas->index_buffer || scale != atlas->last_scale || total_frames != atlas->total_frames) {
		CatClock_DestroyComputeAtlas(atlas);

		atlas->total_frames = total_frames;
		atlas->last_scale = scale;
		atlas->cell_w = (int) ceilf((float) cell_base_w * scale);
		atlas->cell_h = (int) ceilf((float) cell_base_h * scale);

		/* Allocate explicit native coordinate data slots matching canonical definitions */
		atlas->src_rects = malloc(sizeof(SDL_FRect) * total_frames);
		if (!atlas->src_rects)
			return;

		int rows = (total_frames + forced_cols - 1) / forced_cols;
		atlas->atlas_w = forced_cols * atlas->cell_w;
		atlas->atlas_h = rows * atlas->cell_h;

		atlas->index_buffer = (uint8_t*) calloc(1, atlas->atlas_w * atlas->atlas_h);
		if (!atlas->index_buffer) {
			free(atlas->src_rects);
			atlas->src_rects = NULL;
			return;
		}

		/* Calculate lookups and pre-normalize them using row-subscript array indexing */
		for (int i = 0; i < total_frames; i++) {
			int cell_x = (i % forced_cols) * atlas->cell_w;
			int cell_y = (i / forced_cols) * atlas->cell_h;
			atlas->src_rects[i] = (SDL_FRect) { (float) cell_x, (float) cell_y,
												(float) atlas->cell_w, (float) atlas->cell_h };
		}
	}

	/* Compile individual frames using their respective procedural shader paths */
	for (int i = 0; i < total_frames; i++) {
		int cell_x = (i % forced_cols) * atlas->cell_w;
		int cell_y = (i / forced_cols) * atlas->cell_h;

		/* Erase background palette tokens within the target cell area to clean up texturing */
		for (int cy = 0; cy < atlas->cell_h; cy++) {
			for (int cx = 0; cx < atlas->cell_w; cx++) {
				int clear_idx = ((cell_y + cy) * atlas->atlas_w) + (cell_x + cx);
				atlas->index_buffer[clear_idx] = PALETTE_TRANSPARENT;
			}
		}

		/* Invoke procedural builder to draw the layered shapes into our palette sheet */
		shader((void*) atlas->index_buffer, cell_x, cell_y, atlas->atlas_w, atlas->atlas_h, i,
			   userdata);
	}

	/* ==========================================================================
	   SYSTEM ASSET AUTOMATED BLUEPRINT DUMPS (THE TRUTH BOUNDARY)
	   ========================================================================== */
	static bool diagnostic_atlases_dumped = false;
	if (!diagnostic_atlases_dumped) {

		/* Detect hands by matching native tracking dimensions and frame frequencies */
		if (cell_base_w == 64 && cell_base_h == 96 && total_frames == TOTAL_HAND_PHASES) {
			struct {
				int type;
				SDL_Color color;
			}* hand_ptr = userdata;
			if (hand_ptr && hand_ptr->type == HAND_TYPE_HOUR) {
				CatClock_DebugDumpGenericAtlasToPam("dump_hours_atlas.pam", atlas->index_buffer,
													atlas->atlas_w, atlas->atlas_h);
			} else if (hand_ptr && hand_ptr->type == HAND_TYPE_MINUTE) {
				CatClock_DebugDumpGenericAtlasToPam("dump_minutes_atlas.pam", atlas->index_buffer,
													atlas->atlas_w, atlas->atlas_h);
			} else if (hand_ptr && hand_ptr->type == HAND_TYPE_SECOND) {
				CatClock_DebugDumpGenericAtlasToPam("dump_seconds_atlas.pam", atlas->index_buffer,
													atlas->atlas_w, atlas->atlas_h);
			}
		}
		/* Detect moving eyes layer properties */
		else if (cell_base_w == 64 && cell_base_h == 32) {
			CatClock_DebugDumpGenericAtlasToPam("dump_eyes_atlas.pam", atlas->index_buffer,
												atlas->atlas_w, atlas->atlas_h);
		}
		/* Detect swinging tail system configurations */
		else if (cell_base_w == 96 && cell_base_h == 96) {
			/* Check if this is the standard tail blueprint pass or the dilation shadow mask */
			struct {
				void* app;
				float ox;
				float oy;
				bool is_halo;
			}* tail_ptr = userdata;
			if (tail_ptr && tail_ptr->is_halo) {
				CatClock_DebugDumpGenericAtlasToPam("dump_tail_halo_atlas.pam", atlas->index_buffer,
													atlas->atlas_w, atlas->atlas_h);
			} else {
				CatClock_DebugDumpGenericAtlasToPam("dump_tail_body_atlas.pam", atlas->index_buffer,
													atlas->atlas_w, atlas->atlas_h);

				/* Once the final system component completes compilation, latch our diagnostic flag
				 */
				diagnostic_atlases_dumped = true;
				printf("[Verification] All component asset sheets successfully frozen to "
					   "structural disk dumps.\n");
			}
		}
	}
}
