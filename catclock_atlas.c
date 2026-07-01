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

// GPU Center-Tester Rasterizer
void FillSoftwareTriangle(uint8_t* buffer, int x0, int y0, int x1, int y1, int x2, int y2,
						  int width, int height, uint8_t token) {
	// 1. Compute Bounding Box (Clamped strictly to canvas limits)
	int min_x = (x0 < x1) ? ((x0 < x2) ? x0 : x2) : ((x1 < x2) ? x1 : x2);
	int max_x = (x0 > x1) ? ((x0 > x2) ? x0 : x2) : ((x1 > x2) ? x1 : x2);
	int min_y = (y0 < y1) ? ((y0 < y2) ? y0 : y2) : ((y1 < y2) ? y1 : y2);
	int max_y = (y0 > y1) ? ((y0 > y2) ? y0 : y2) : ((y1 > y2) ? y1 : y2);

#ifdef DEBUG_TELEMETRY_TRIANGLE
	int raw_area = (max_x - min_x + 1) * (max_y - min_y + 1);
#endif

	if (min_x < 0) {
		min_x = 0;
	}
	if (max_x >= width) {
		max_x = width - 1;
	}
	if (min_y < 0) {
		min_y = 0;
	}
	if (max_y >= height) {
		max_y = height - 1;
	}

#ifdef DEBUG_TELEMETRY_TRIANGLE
	int clamped_area = (max_x - min_x + 1) * (max_y - min_y + 1);
#endif

	// 2. Precompute Edge Setup Delta Vectors
	int dx01 = x1 - x0, dy01 = y1 - y0;
	int dx12 = x2 - x1, dy12 = y2 - y1;
	int dx20 = x0 - x2, dy20 = y0 - y2;

	// 3. Determine Top/Left Flags for Tie-Breaking (Enforces hardware edge exclusion)
	bool tl0 = (dy01 < 0) || (dy01 == 0 && dx01 > 0);
	bool tl1 = (dy12 < 0) || (dy12 == 0 && dx12 > 0);
	bool tl2 = (dy20 < 0) || (dy20 == 0 && dx20 > 0);

#ifdef DEBUG_TELEMETRY_TRIANGLE
	int tri_double_area = dx01 * dy12 - dy01 * dx12;
	bool is_ccw = (tri_double_area > 0);

	// Filter logging footprint down strictly to high-value clock needle entities
	if (token == 2 || token == 3 || token == 4) {
		printf("[GPU RASTER AUDIT] UNIT_START | Token: %u | Winding: %s | Analytical Double Area: "
			   "%d\n",
			   token, is_ccw ? "CCW" : "CW", tri_double_area);
		printf("  -> Boundary footprint: Raw Area: %d px | Clamped Screen Area: %d px\n", raw_area,
			   clamped_area);
		printf("  -> Geometry Vertices:  V0(%d,%d) -> V1(%d,%d) -> V2(%d,%d)\n", x0, y0, x1, y1, x2,
			   y2);
	}
#endif

	// 4. Processing Loop matching GLSL Fragment Generation Behavior
	for (int y = min_y; y <= max_y; y++) {
		float px_y = (float) y + 0.5f; // Sample at half-pixel center vertically
		int pixels_generated_on_row = 0;

		for (int x = min_x; x <= max_x; x++) {
			float px_x = (float) x + 0.5f; // Sample at half-pixel center horizontally

			// Cross product vector magnitude checks (Edge Functional Distance)
			float w0 = (px_x - x0) * dy01 - (px_y - y0) * dx01;
			float w1 = (px_x - x1) * dy12 - (px_y - y1) * dx12;
			float w2 = (px_x - x2) * dy20 - (px_y - y2) * dx20;

			// Handle Winding Order Agnosticism (Evaluates both CW and CCW layouts smoothly)
			bool inside_ccw = (w0 > 0 || (w0 == 0 && tl0)) && (w1 > 0 || (w1 == 0 && tl1))
				&& (w2 > 0 || (w2 == 0 && tl2));

			bool inside_cw = (w0 < 0 || (w0 == 0 && !tl0)) && (w1 < 0 || (w1 == 0 && !tl1))
				&& (w2 < 0 || (w2 == 0 && !tl2));

			if (inside_ccw || inside_cw) {
				buffer[(y * width) + x] = token;
				pixels_generated_on_row++;
			}
		}

#ifdef DEBUG_TELEMETRY_TRIANGLE
		if ((token == 2 || token == 3 || token == 4) && pixels_generated_on_row > 0) {
			printf("  [Row Allocation Execution] Row Y: %d | Generated Span: %d px wide\n", y,
				   pixels_generated_on_row);
		}
#endif
	}

#ifdef DEBUG_TELEMETRY_TRIANGLE
	if (token == 2 || token == 3 || token == 4) {
		printf("[GPU RASTER AUDIT] UNIT_COMPLETE\n\n");
	}
#endif
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
	atlas->scale_half_steps = 0;
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

	/* System Asset Automated Blueprint Dumps System */
	/* STAGE 2: Force disk dumps to overwrite whenever texture cache state transitions */
	if (ctx.texture_cache_stale) {
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
		} else if (cell_base_w == 64 && cell_base_h == 32) {
			CatClock_DebugDumpGenericAtlasToPam("dump_eyes_atlas.pam", atlas->index_buffer,
												atlas->atlas_w, atlas->atlas_h);
		} else if (cell_base_w == 96 && cell_base_h == 96) {
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
	}
}
