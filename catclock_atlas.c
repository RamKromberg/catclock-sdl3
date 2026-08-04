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

void get_optimal_sprite_sheet_dimensions(int fps, int cell_w, int cell_h, int* out_rows,
										 int* out_cols);

/**
 * CatClock_RebakeComputeAtlas
 * Compiles mathematical layout paths onto structured VRAM-ready sheets.
 * Executes automated blueprint file dumps to disk for structural validation.
 */
void CatClock_RebakeComputeAtlas(void* renderer, CatClock_ComputeAtlas* atlas, int cell_base_w,
								 int cell_base_h, int total_frames, int forced_cols,
								 CatClock_ShaderCallback shader, void* userdata) {
	(void) renderer;
	float scale = (float) ctx.current_half_steps / 2.0f;

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

		/* Utilize layout trackers computed early during parsing stages if valid */
		if (forced_cols == 0) {
			if (cell_base_w == EYES_CELL_W && cell_base_h == EYES_CELL_H) {
				atlas->atlas_cols = ctx.eyes_optimized_cols;
				atlas->atlas_rows = ctx.eyes_optimized_rows;
			} else if (cell_base_w == TAIL_CELL_W && cell_base_h == TAIL_CELL_H) {
				atlas->atlas_cols = ctx.tail_optimized_cols;
				atlas->atlas_rows = ctx.tail_optimized_rows;
			} else {
				get_optimal_sprite_sheet_dimensions(total_frames, atlas->cell_w, atlas->cell_h,
													&atlas->atlas_rows, &atlas->atlas_cols);
			}
		} else {
			atlas->atlas_cols = forced_cols;
			atlas->atlas_rows = (total_frames + forced_cols - 1) / forced_cols;
		}

		atlas->atlas_w = atlas->atlas_cols * atlas->cell_w;
		atlas->atlas_h = atlas->atlas_rows * atlas->cell_h;

		atlas->index_buffer = (uint8_t*) calloc(1, atlas->atlas_w * atlas->atlas_h);
		if (!atlas->index_buffer) {
			free(atlas->src_rects);
			atlas->src_rects = NULL;
			return;
		}

		/* Calculate lookups and pre-normalize them using row-subscript array indexing */
		for (int i = 0; i < total_frames; i++) {
			int cell_x = (i % atlas->atlas_cols) * atlas->cell_w;
			int cell_y = (i / atlas->atlas_cols) * atlas->cell_h;
			atlas->src_rects[i] = (SDL_FRect) { (float) cell_x, (float) cell_y,
												(float) atlas->cell_w, (float) atlas->cell_h };
		}
	}

	/* Compile individual frames using their respective procedural shader paths */
	for (int i = 0; i < total_frames; i++) {
		int cell_x = (i % atlas->atlas_cols) * atlas->cell_w;
		int cell_y = (i / atlas->atlas_cols) * atlas->cell_h;

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
#ifdef DEBUG_DUMP_ATLAS
		/* Track clock-hands packing limits safely */
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
		}
		/* Track ping-pong cells using explicit base identifiers */
		else if (cell_base_w == EYES_CELL_W && cell_base_h == EYES_CELL_H) {
			char filename_buf[128]; /* Fixed sizing buffer width added */
			snprintf(filename_buf, sizeof(filename_buf), "dump_eyes_atlas_fps%d_%dx%d.pam",
					 ctx.target_fps, atlas->atlas_rows, atlas->atlas_cols);
			CatClock_DebugDumpGenericAtlasToPam(filename_buf, atlas->index_buffer, atlas->atlas_w,
												atlas->atlas_h);
		} else if (cell_base_w == TAIL_CELL_W && cell_base_h == TAIL_CELL_H) {
			CatClock_TailShaderArgs* tail_ptr = userdata;
			if (tail_ptr && tail_ptr->force_halo_color) {
				CatClock_DebugDumpGenericAtlasToPam("dump_tail_halo_atlas.pam", atlas->index_buffer,
													atlas->atlas_w, atlas->atlas_h);
			} else {
				char filename_buf[128]; /* Fixed sizing buffer width added */
				snprintf(filename_buf, sizeof(filename_buf), "dump_tail_body_atlas_fps%d_%dx%d.pam",
						 ctx.target_fps, atlas->atlas_rows, atlas->atlas_cols);
				CatClock_DebugDumpGenericAtlasToPam(filename_buf, atlas->index_buffer,
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

/**
 * Calculates the optimal number of rows and columns to pack 'fps' cells
 * of size 'cell_w' x 'cell_h' such that the overall sheet is as square as possible.
 *
 * Caveat emptor: The 2D bin packing problem is NP-hard.
 *
 * @param fps      Total number of frames/cells to pack (1 to 1024)
 * @param cell_w   Width of an individual cell (e.g., 64 or 96)
 * @param cell_h   Height of an individual cell (e.g., 32 or 96)
 * @param out_rows Pointer to store the calculated row count
 * @param out_cols Pointer to store the calculated column count
 */
void get_optimal_sprite_sheet_dimensions(int fps, int cell_w, int cell_h, int* out_rows,
										 int* out_cols) {
	if (fps <= 0 || cell_w <= 0 || cell_h <= 0 || !out_rows || !out_cols) {
		if (out_rows)
			*out_rows = 0;
		if (out_cols)
			*out_cols = 0;
		return;
	}

	// Branching optimizations for common aspect ratios to narrow down search bounds
	int start_cols = 1;
	int end_cols = fps;

	if (cell_w == cell_h) {
		// 1:1 Aspect Ratio (e.g., 96x96) -> Optimal cols will be near sqrt(fps)
		int ideal = (int) sqrt(fps);
		start_cols = ideal - 5;
		end_cols = ideal + 5;
	} else if (cell_w == cell_h * 2) {
		// 2:1 Aspect Ratio (e.g., 64x32) -> Cols will be roughly sqrt(fps / 2)
		int ideal = (int) sqrt(fps / 2.0);
		start_cols = ideal - 5;
		end_cols = ideal + 5;
	} else if (cell_w * 3 == cell_h * 2) {
		// 2:3 Aspect Ratio (e.g., 64x96) -> Cols will be roughly sqrt(fps * 1.5)
		int ideal = (int) sqrt(fps * 1.5);
		start_cols = ideal - 5;
		end_cols = ideal + 5;
	}

	// Clamp bounds safely to valid dimensions
	if (start_cols < 1)
		start_cols = 1;
	if (end_cols > fps)
		end_cols = fps;

	// Fallback: If bounds are too restricted due to low FPS, search the full range
	if (end_cols - start_cols < 2) {
		start_cols = 1;
		end_cols = fps;
	}

	long long min_pixel_diff = -1;
	int best_cols = 1;
	int best_rows = fps;

	// Exhaustive search over the target column window
	for (int cols = start_cols; cols <= end_cols; cols++) {
		// Ceiling division: rows = ceil(fps / cols)
		int rows = (fps + cols - 1) / cols;

		long long sheet_w = (long long) cols * cell_w;
		long long sheet_h = (long long) rows * cell_h;
		long long diff = labs(sheet_w - sheet_h);

		// Secondary optimization: track empty wasted cell slots
		int wasted_cells = (rows * cols) - fps;

		if (min_pixel_diff == -1 || diff < min_pixel_diff) {
			min_pixel_diff = diff;
			best_cols = cols;
			best_rows = rows;
		}
		// If dimensions tie on squariness, select the one that wastes fewer cells
		else if (diff == min_pixel_diff) {
			int current_best_wasted = (best_rows * best_cols) - fps;
			if (wasted_cells < current_best_wasted) {
				best_cols = cols;
				best_rows = rows;
			}
		}
	}

	*out_rows = best_rows;
	*out_cols = best_cols;
}
