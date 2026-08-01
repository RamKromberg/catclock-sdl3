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
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void CatClock_OnWindowResize(SDL_WindowEvent* resize_event, CatClock_AppContext* context,
							 void* renderer);

void CatClock_DestroyComputeAtlas(CatClock_ComputeAtlas* atlas);

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
	int forced_cols = 10;

	/* Restrict dynamic regeneration to run only when state updates or frame limits shift */
	if (!atlas->index_buffer || ctx.current_half_steps != atlas->scale_half_steps
		|| total_frames != atlas->total_frames) {
		CatClock_DestroyComputeAtlas(atlas);

		atlas->total_frames = total_frames;
		atlas->scale_half_steps = ctx.current_half_steps;
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

	if (ctx.texture_cache_stale) {
/* System Asset Automated Blueprint Dumps System */
/* STAGE 2: Force disk dumps to overwrite whenever texture cache state transitions */
#ifdef DEBUG_DUMP_ATLAS
		if (cell_base_w == HAND_CELL_W && cell_base_h == HAND_CELL_H
			&& total_frames == TOTAL_HAND_PHASES) {
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
		} else if (cell_base_w == EYES_CELL_W && cell_base_h == EYES_CELL_H) {
			CatClock_DebugDumpGenericAtlasToPam("dump_eyes_atlas.pam", atlas->index_buffer,
												atlas->atlas_w, atlas->atlas_h);
		} else if (cell_base_w == TAIL_CELL_W && cell_base_h == TAIL_CELL_H) {
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
				printf("[Verification] Dynamic component asset sheets successfully updated on "
					   "disk.\n");
			}
		}
#endif
	}
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
	atlas->scale_half_steps = 0;
}
