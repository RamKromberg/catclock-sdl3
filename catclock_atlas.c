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

int CompareFloats(const void* a, const void* b);

void PlotSoftwarePixel(uint8_t* buffer, int x, int y, int width, int height, uint8_t token);

void DrawTriangleLikeMesa(uint8_t* buffer, int atlas_w, int atlas_h, float x0, float y0, float x1,
						  float y1, float x2, float y2, uint8_t color);
void DrawLineLikeMesa(uint8_t* buffer, int atlas_w, int atlas_h, float x0, float y0, float x1,
					  float y1, float thickness, uint8_t color);
void DrawTriangleLegacy(uint8_t* buffer, int x0, int y0, int x1, int y1, int x2, int y2, int width,
						int height, uint8_t token);

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
 * CompareFloats
 * Standard qsort comparison helper for floating-point bounds validation.
 */
int CompareFloats(const void* a, const void* b) {
	float fa = *(const float*) a;
	float fb = *(const float*) b;
	return (fa > fb) - (fa < fb);
}

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
void DrawTriangleLegacy(uint8_t* buffer, int x0, int y0, int x1, int y1, int x2, int y2, int width,
						int height, uint8_t token) {
	// 1. Compute Bounding Box (Clamped strictly to canvas limits)
	int min_x = (x0 < x1) ? ((x0 < x2) ? x0 : x2) : ((x1 < x2) ? x1 : x2);
	int max_x = (x0 > x1) ? ((x0 > x2) ? x0 : x2) : ((x1 > x2) ? x1 : x2);
	int min_y = (y0 < y1) ? ((y0 < y2) ? y0 : y2) : ((y1 < y2) ? y1 : y2);
	int max_y = (y0 > y1) ? ((y0 > y2) ? y0 : y2) : ((y1 > y2) ? y1 : y2);

#ifdef DEBUG_TELEMETRY_TRIANGLE_LEGACY
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

#ifdef DEBUG_TELEMETRY_TRIANGLE_LEGACY
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

#ifdef DEBUG_TELEMETRY_TRIANGLE_LEGACY
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

#ifdef DEBUG_TELEMETRY_TRIANGLE_LEGACY
		if ((token == 2 || token == 3 || token == 4) && pixels_generated_on_row > 0) {
			printf("  [Row Allocation Execution] Row Y: %d | Generated Span: %d px wide\n", y,
				   pixels_generated_on_row);
		}
#endif
	}

#ifdef DEBUG_TELEMETRY_TRIANGLE_LEGACY
	if (token == 2 || token == 3 || token == 4) {
		printf("[GPU RASTER AUDIT] UNIT_COMPLETE\n\n");
	}
#endif
}

/*












*/

// ====================================================================
// LINE ENGINE (NATIVE SUB-PIXEL FLOAT TRACKING)
// Extrudes wide-line ribbon quads using high-precision math.
// ====================================================================
void DrawLineLikeMesa(uint8_t* buffer, int atlas_w, int atlas_h, float x0, float y0, float x1,
					  float y1, float dynamic_hand_thickness, uint8_t color) {
	float dx = x1 - x0;
	float dy = y1 - y0;
	float len = sqrtf(dx * dx + dy * dy);

	if (len < 0.00001f) {
		// Fallback for single-point lines using the sub-pixel float engine
		DrawTriangleLikeMesa(buffer, atlas_w, atlas_h, x0, y0, x0, y0, x0, y0, color);
		return;
	}

	// High-precision perpendicular unit vector rules
	float nx = -dy / len;
	float ny = dx / len;

	// Shift sub-pixel coordinates slightly to align cleanly with hardware grids
	float half_w = dynamic_hand_thickness * 0.5f;
	float alignment_shift = (dynamic_hand_thickness <= 1.05f) ? 0.0f : 0.5f;

	// Extrude the boundary fence edges cleanly as continuous high-precision floats
	float fx0 = x0 - nx * half_w + alignment_shift;
	float fy0 = y0 - ny * half_w + alignment_shift;
	float fx1 = x0 + nx * half_w + alignment_shift;
	float fy1 = y0 + ny * half_w + alignment_shift;
	float fx2 = x1 - nx * half_w + alignment_shift;
	float fy2 = y1 - ny * half_w + alignment_shift;
	float fx3 = x1 + nx * half_w + alignment_shift;
	float fy3 = y1 + ny * half_w + alignment_shift;

#if defined(DEBUG_TELEMETRY_LINE)
	printf("-> Extruded Ribbon Quad Floats: V0(%.3f,%.3f), V1(%.3f,%.3f), V2(%.3f,%.3f), "
		   "V3(%.3f,%.3f)\n",
		   fx0, fy0, fx1, fy1, fx2, fy2, fx3, fy3);
#endif

	// Commit sub-primitives cleanly down to the pure float rasterizer loop
	DrawTriangleLikeMesa(buffer, atlas_w, atlas_h, fx0, fy0, fx2, fy2, fx3, fy3, color);
	DrawTriangleLikeMesa(buffer, atlas_w, atlas_h, fx0, fy0, fx3, fy3, fx1, fy1, color);
}

// ====================================================================
// SOFTWARE RASTERIZER TRIANGLE ENGINE (NATIVE SUB-PIXEL FLOAT PRIMITIVES)
// Evaluates exact sub-pixel cross products at pixel center spaces.
// Mostly based on Mesa's Gallium lp_setup_tri.c.
// ====================================================================

/* ============================================================================
 * MESA REFERENCE ENGINE RASTERIZATION TOPOGRAPHY (SCALAR FALLBACK PIPELINE)
 * ============================================================================
 * Phase 1: Coordinate Fixed-Point Snapping & Bounding Box Generation
 *   - Snaps raw floating-point input vertices to 24.8 fixed-point subpixel format via
 *     util_iround(FIXED_ONE * a) where FIXED_ONE = 256 (lp_setup_tri.c:69,1080).
 *   - Extracts a minimal, axis-aligned integer bounding box [bbox.x0..bbox.x1] and
 *     [bbox.y0..bbox.y1] using a direction adjustment bias flag (lp_setup_tri.c:311-323).
 *   - Maps standard CCW winding configurations relative to active primitive states.
 *
 * Phase 2: Primitive Rejection, Scissoring, & Window Intersection
 *   - Maps the macro-tile dimensions to local tile footprints (TILE_SIZE = 64)
 * (lp_setup_tri.c:821).
 *   - Intersects tile boundaries with global scissors via u_rect_find_intersection
 *     to yield tight, clamped destination bounding boxes (lp_setup_tri.c:815).
 *
 * Phase 3: Analytical Plane Coefficient Generation & Early Tie-Breaking Injection
 *   - Computes fixed-point directional edges: plane[i].dcdx = dy, plane[i].dcdy = dx
 * (lp_setup_tri.c:670-675).
 *   - Evaluates initial analytical 64-bit edge plane constants:
 *       C = (dcdx * x_start) - (dcdy * y_start) via IMUL64 macro (lp_setup_tri.c:681).
 *   - !!! CRITICAL TIE-BREAKER INJECTION !!!: Modifies C *early* during the setup phase
 *     to bake filling conventions directly into the baseline coefficients (lp_setup_tri.c:686-702):
 *       - Left Edges (dcdx < 0): Inclusive, unconditionally shifts C++
 *       - Top Horizontal Edges (dcdx == 0, bottom_edge_rule == 0, dcdy > 0): Inclusive, shifts C++
 *       - Bottom Horizontal Edges (dcdx == 0, bottom_edge_rule != 0, dcdy < 0): Inclusive, shifts
 * C++
 *   - Scales gradients into final match bit-widths via an 8-bit FIXED_ORDER shift
 * (lp_setup_tri.c:708).
 *   - Generates one-pixel sized trivial reject offsets: plane[i].eo (lp_setup_tri.c:716-718).
 *
 * Phase 4: Localized Macro-Tile Window Initial Loop Shifts & Linear Delta Projections
 *   - Projects global edge functions onto local macro-tile start points (lp_setup_tri.c:903-906).
 *   - Evaluates upper-left corner tile positions via c[i] = plane[i].c + (dcdy * iy0 * 64) - (dcdx
 * * ix0 * 64), carrying the early-baked tie-breaker directly into the active tile block matrix
 * (lp_setup_tri.c:909-911).
 *   - Establishes asymmetric grid iteration deltas scaled up by TILE_ORDER (<< 6) shifts:
 *       xstep[i] = -dcdx (subtractive column advances), ystep[i] = +dcdy (additive row descents)
 * (lp_setup_tri.c:918-919).
 *
 * Phase 5: Reject/Accept Block Assessment & Sign-Extension Masking
 *   - Sets up macro-tile test equations using trivial reject boundaries (`eo`) and partial tile
 *     parameters (`ei`) (lp_setup_tri.c:913-917).
 *   - Evaluates total block intersection states using branchless arithmetic right-shifts (`>> 63`)
 * (lp_setup_tri.c:940-944):
 *       - `out |= (planeout >> 63)` flag marks full tile cancellation when true.
 *       - `partial |= ((planepartial >> 63)) & (1<<i)` records localized plane crossovers.
 *   - Deducts a strict `- 1` integer boundary bias from `planepartial` to neutralize Phase 3's
 *     early `C++` inclusion shift, preventing edge-aligned boundaries from triggering a false
 * full-tile acceptance flag.
 *
 * Phase 6: Incremental Linear Delta Step Sweep & Sign Inversion Winding Test
 *   - Routes execution tracking to standard iteration loops when vector hardware pathways are
 *     unavailable (lp_rast_linear_fallback.c:112).
 *   - Truncates wide-range 64-bit analytical tile parameters down into local signed 32-bit
 *     registers (`int32_t c`) to evaluate grid spaces.
 *   - Accumulates linear stepping metrics across active row and column span metrics: R = c + (dcdy
 * * dy) - (dcdx * dx) (lp_rast_tri.c:75-82).
 *   - Evaluates final pixel coverage inclusion bounds by strictly testing negative sign boundaries
 * (R < 0) down the line equation.
 *
 * ============================================================================
 * CURRENT VERIFICATION STATUS & REGRESSION MATRIX
 * ============================================================================
 * [PASS] Core Subpixel Grid Snap Layer: Verified completely via Test Mode 9 (+0.5f fractional bias
 * shifts). [PASS] Pure Axis-Aligned / Orthogonal Edges: Verified via Test Modes 2, 6, 7, and 8.
 * [FAIL] Non-Orthogonal Diagonal Edge Gradients: Test Modes 1, 3, and 4 exhibit a distinct 1px
 * top-left drift under scaled layouts (2.00x context), pinning the bug to non-zero gradients (dcdx
 * != 0 && dcdy != 0).
 */

/* ---DrawTriangleLikeMesa--- */

// ====================================================================
// SOFTWARE RASTERIZER TRIANGLE ENGINE (NATIVE SUB-PIXEL FLOAT PRIMITIVES)
// Evaluates exact sub-pixel cross products at pixel center spaces.
// Mostly based on Mesa's Gallium lp_setup_tri.c.
// ====================================================================

#define FIXED_ORDER 8
#define FIXED_ONE (1 << FIXED_ORDER)

#define TILE_ORDER 6
#define TILE_SIZE (1 << TILE_ORDER) // 64

#define IMUL64(a, b) (((int64_t) (a)) * ((int64_t) (b)))

#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))
#define MAX3(a, b, c) ((a) > (b) ? ((a) > (c) ? (a) : (c)) : ((b) > (c) ? (b) : (c)))

struct fixed_position {
	int32_t x[4];
	int32_t y[4];
	int32_t dx01;
	int32_t dy01;
	int32_t dx20;
	int32_t dy20;
};

struct lp_mesa_plane {
	int64_t c;
	int32_t dcdx;
	int32_t dcdy;
	uint32_t eo;
	uint32_t pad;
};

struct lp_mesa_triangle {
	int x0, x1, y0, y1;
	struct lp_mesa_plane plane[3];
};

static inline void rotate_fixed_position_01(struct fixed_position* position) {
	int32_t x = position->x[1];
	int32_t y = position->y[1];

	position->x[1] = position->x[0];
	position->y[1] = position->y[0];
	position->x[0] = x;
	position->y[0] = y;

	position->dx01 = -position->dx01;
	position->dy01 = -position->dy01;
	position->dx20 = position->x[2] - position->x[0];
	position->dy20 = position->y[2] - position->y[0];
}

static inline void rotate_fixed_position_12(struct fixed_position* position) {
	int32_t x = position->x[2];
	int32_t y = position->y[2];

	position->x[2] = position->x[1];
	position->y[2] = position->y[1];
	position->x[1] = x;
	position->y[1] = y;

	x = position->dx01;
	y = position->dy01;
	position->dx01 = -position->dx20;
	position->dy01 = -position->dy20;
	position->dx20 = -x;
	position->dy20 = -y;
}

static inline int subpixel_snap(float coordinate) {
	// TRUE MESA SNAP: Aligns perfectly with util_iround(FIXED_ONE * a)
	return (int) lrintf(coordinate * FIXED_ONE);
}

static int8_t calc_fixed_position(float x0, float y0, float x1, float y1, float x2, float y2,
								  struct fixed_position* position) {
	// Verbatim Mesa Driver Snapping Invariant (lp_setup_tri.c)
	float pixel_offset = 0.5f;

	position->x[0] = subpixel_snap(x0 - pixel_offset);
	position->x[1] = subpixel_snap(x1 - pixel_offset);
	position->x[2] = subpixel_snap(x2 - pixel_offset);
	position->x[3] = 0;

	position->y[0] = subpixel_snap(y0 - pixel_offset);
	position->y[1] = subpixel_snap(y1 - pixel_offset);
	position->y[2] = subpixel_snap(y2 - pixel_offset);
	position->y[3] = 0;

	position->dx01 = position->x[0] - position->x[1];
	position->dy01 = position->y[0] - position->y[1];
	position->dx20 = position->x[2] - position->x[0];
	position->dy20 = position->y[2] - position->y[0];

	uint64_t area = IMUL64(position->dx01, position->dy20) - IMUL64(position->dx20, position->dy01);
	return area == 0 ? 0 : (area & (1ULL << 63)) ? -1 : 1;
}

static bool do_triangle_ccw(struct fixed_position* position, int bottom_edge_rule,
							struct lp_mesa_triangle* tri) {
	int adj = (bottom_edge_rule != 0) ? 1 : 0;

	// 1:1 Mesa Re-centering Invariant: Add half-pixel subpixels (+128)
	// to correct the pixel_offset subtraction before integer conversion
	tri->x0 = (MIN3(position->x[0], position->x[1], position->x[2]) + 128) >> FIXED_ORDER;
	tri->x1 = (MAX3(position->x[0], position->x[1], position->x[2]) + 128 - 1) >> FIXED_ORDER;
	tri->y0 = (MIN3(position->y[0], position->y[1], position->y[2]) + 128 + adj) >> FIXED_ORDER;
	tri->y1 = (MAX3(position->y[0], position->y[1], position->y[2]) + 128 - 1 + adj) >> FIXED_ORDER;

	// Diagnostics trace hooks
	printf("[TRACE-DO-TRI-CCW-BOUNDS] Raw snapped min_x: %d, max_x: %d | min_y: %d, max_y: %d\n",
		   MIN3(position->x[0], position->x[1], position->x[2]),
		   MAX3(position->x[0], position->x[1], position->x[2]),
		   MIN3(position->y[0], position->y[1], position->y[2]),
		   MAX3(position->y[0], position->y[1], position->y[2]));
	printf("[TRACE-DO-TRI-CCW-BOUNDS] Shifted raster outputs (adj=%d) -> x0: %d, x1: %d | y0: %d, "
		   "y1: %d\n",
		   adj, tri->x0, tri->x1, tri->y0, tri->y1);

	if (tri->x0 < 0)
		tri->x0 = 0;
	if (tri->y0 < 0)
		tri->y0 = 0;

	// 2. 1:1 Upstream Mesa Scalar Direct Assignments (lp_setup_tri.c lines 670-675)
	// Map edge delta properties into unshifted tracking variables first
	tri->plane[0].dcdy = position->dx01; // v0x - v1x
	tri->plane[1].dcdy = position->x[1] - position->x[2]; // v1x - v2x
	tri->plane[2].dcdy = position->dx20; // v2x - v0x

	tri->plane[0].dcdx = position->dy01; // v0y - v1y
	tri->plane[1].dcdx = position->y[1] - position->y[2]; // v1y - v2y
	tri->plane[2].dcdx = position->dy20; // v2y - v0y

	printf("[MESA-TRACE-SETUP] Snapped Vertices:\n");
	printf("  v0:(%d, %d) | v1:(%d, %d) | v2:(%d, %d)\n", position->x[0], position->y[0],
		   position->x[1], position->y[1], position->x[2], position->y[2]);

	// 3. Process Edge Constant Evaluations & Tie-Breaking Rules on Raw Values
	// 3. Process Edge Constant Evaluations & Tie-Breaking Rules on Raw Values
	for (int i = 0; i < 3; i++) {
		// Compute the native 64-bit cross-product line constant
		tri->plane[i].c = IMUL64(tri->plane[i].dcdx, position->x[i])
			- IMUL64(tri->plane[i].dcdy, position->y[i]);

		// Clean Mesa Fill Convention Rule Alignment (lp_setup_tri.c lines 686-702)
		if (tri->plane[i].dcdx < 0) {
			/* Both fill conventions flag left edges as inclusive */
			tri->plane[i].c++;
		} else if (tri->plane[i].dcdx == 0) {
			if (bottom_edge_rule == 0) {
				/* Top-left fill convention: top horizontal edges are inclusive */
				if (tri->plane[i].dcdy > 0)
					tri->plane[i].c++;
			} else {
				/* Bottom-left fill convention: bottom horizontal edges are inclusive */
				if (tri->plane[i].dcdy < 0)
					tri->plane[i].c++;
			}
		}

		// Conformance Realignment: Shift gradients up BEFORE calculating trivial reject eo
		tri->plane[i].dcdx <<= FIXED_ORDER;
		tri->plane[i].dcdy <<= FIXED_ORDER;

		// 4. Calculate Single-Pixel Trivial Reject Offsets (On scaled precision space)
		tri->plane[i].eo = 0;
		if (tri->plane[i].dcdx < 0)
			tri->plane[i].eo -= tri->plane[i].dcdx;
		if (tri->plane[i].dcdy > 0)
			tri->plane[i].eo += tri->plane[i].dcdy;

		printf(" [Plane %d Setup] Normals:(dx:%d, dy:%d) | Biased c:%lld | Trivial Reject eo:%u\n",
			   i, tri->plane[i].dcdx, tri->plane[i].dcdy, tri->plane[i].c, tri->plane[i].eo);
	}

	return true;
}

void lp_rast_triangle(struct lp_mesa_triangle* tri, int scissor_x0, int scissor_y0, int scissor_x1,
					  int scissor_y1, uint8_t* pixel_buffer, int stride, uint8_t color,
					  int bottom_edge_rule, int tile_x, int tile_y) {

	printf("[TRACE-LP-RAST-ENTRY] Target Triangle Box -> x0: %d, x1: %d | y0: %d, y1: %d\n",
		   tri->x0, tri->x1, tri->y0, tri->y1);

	int start_x = (tri->x0 < scissor_x0) ? scissor_x0 : tri->x0;
	int end_x = (tri->x1 > scissor_x1) ? scissor_x1 : tri->x1;
	int start_y = (tri->y0 < scissor_y0) ? scissor_y0 : tri->y0;
	int end_y = (tri->y1 > scissor_y1) ? scissor_y1 : tri->y1;

	if (end_x < start_x || end_y < start_y) {
		return;
	}

	int64_t c[3];
	printf("[DIAG-RAST-PHASE4] Extracted Tile local parameters -> start_x:%d, start_y:%d\n",
		   start_x, start_y);
	for (int i = 0; i < 3; i++) {
		c[i] = tri->plane[i].c;
	}

	// ============================================================================
	// PHASE 5: TRIVIAL BLOCK REJECTION
	// ============================================================================
	bool trivial_accept = true;
	printf("[DIAG-RAST-PHASE5] Evaluating Trivial Block Rejection:\n");
	for (int i = 0; i < 3; i++) {
		int32_t raw_dcdx = tri->plane[i].dcdx >> FIXED_ORDER;
		int32_t raw_dcdy = tri->plane[i].dcdy >> FIXED_ORDER;

		int64_t eo = (int64_t) tri->plane[i].eo << TILE_ORDER;
		int64_t ei = ((int64_t) raw_dcdy - raw_dcdx - (int64_t) tri->plane[i].eo) << TILE_ORDER;

		int64_t planeout = c[i] + eo;
		int64_t planepartial = c[i] + ei - 1;

		printf("  Plane %d -> eo: %" PRId64 " | ei: %" PRId64 " | planeout: %" PRId64
			   " | planepartial: %" PRId64 "\n",
			   i, eo, ei, planeout, planepartial);

		if (planeout < 0) {
			printf("  [BLOCK-REJECT] Plane %d triggered macro-tile rejection.\n", i);
			return;
		}
		if (planepartial < 0) {
			trivial_accept = false;
		}
	}
	printf("  Final Block Resolution -> trivial_accept = %s\n", trivial_accept ? "TRUE" : "FALSE");

	int tile_origin_x = (start_x / TILE_SIZE) * TILE_SIZE;
	int tile_origin_y = (start_y / TILE_SIZE) * TILE_SIZE;

	// ============================================================================
	// PHASE 6: PIXEL TRAVERSAL (Precise Fixed-Point Step Execution)
	// Anchors row evaluations relative to absolute macro-tile base columns
	// ============================================================================
	for (int p_y = start_y; p_y <= end_y; p_y++) {

		// Calculate coordinate offsets relative to the true binned tile origin block lines
		int local_y = p_y - tile_y;

		// Clean initialization: evaluate variables directly matching your binned constants.
		// Removes the duplicate center-bias shift that was pushing coverage to the top-left!
		int64_t cx0 = tri->plane[0].c + ((int64_t) tri->plane[0].dcdy * local_y)
			- ((int64_t) tri->plane[0].dcdx * (start_x - tile_x));

		int64_t cx1 = tri->plane[1].c + ((int64_t) tri->plane[1].dcdy * local_y)
			- ((int64_t) tri->plane[1].dcdx * (start_x - tile_x));

		int64_t cx2 = tri->plane[2].c + ((int64_t) tri->plane[2].dcdy * local_y)
			- ((int64_t) tri->plane[2].dcdx * (start_x - tile_x));

		for (int p_x = start_x; p_x <= end_x; p_x++) {
			// Inclusive coverage resolves strictly by checking the sign bit
			if (cx0 >= 0 && cx1 >= 0 && cx2 >= 0) {
				uint32_t dest_offset = p_y * stride + p_x;
				pixel_buffer[dest_offset] = color;
			}

			// Step horizontally across columns using raw integer pixel derivatives
			cx0 -= tri->plane[0].dcdx;
			cx1 -= tri->plane[1].dcdx;
			cx2 -= tri->plane[2].dcdx;
		}
	}
}

bool SetupTriangleMesaConvention(float x0, float y0, float x1, float y1, float x2, float y2,
								 int bottom_edge_rule, struct lp_mesa_triangle* tri) {
	struct fixed_position position;

	int8_t area_sign = calc_fixed_position(x0, y0, x1, y1, x2, y2, &position);

	if (area_sign == 0) {
		return false;
	}

	if (area_sign < 0) {
		rotate_fixed_position_01(&position);
	}

	return do_triangle_ccw(&position, bottom_edge_rule, tri);
}

void DrawTriangleLikeMesa(uint8_t* buffer, int atlas_w, int atlas_h, float x0, float y0, float x1,
						  float y1, float x2, float y2, uint8_t color) {
	struct lp_mesa_triangle tri;
	int bottom_edge_rule = 0;

	// ============================================================================
	// CANONICAL OPENGL NDC VIEWPORT MATRIX TRANSFORMATION
	// ============================================================================
	float half_w = (float) atlas_w / 2.0f;
	float half_h = (float) atlas_h / 2.0f;

	// Map NDC space [-1.0..1.0] to raw canvas window pixel ranges
	float x0_w = (x0 + 1.0f) * half_w;
	float y0_w = (1.0f - y0) * half_h;

	float x1_w = (x1 + 1.0f) * half_w;
	float y1_w = (1.0f - y1) * half_h;

	float x2_w = (x2 + 1.0f) * half_w;
	float y2_w = (1.0f - y2) * half_h;

	printf("\n[DIAG-BINNER-ENTRY] Incoming NDC Floats converted to Screen Window:\n");
	printf("  v0:(%.4f, %.4f) v1:(%.4f, %.4f) v2:(%.4f, %.4f)\n", x0_w, y0_w, x1_w, y1_w, x2_w,
		   y2_w);

	if (!SetupTriangleMesaConvention(x0_w, y0_w, x1_w, y1_w, x2_w, y2_w, bottom_edge_rule, &tri)) {
		printf("[DIAG-BINNER-REJECT] SetupTriangleMesaConvention rejected primitive.\n");
		return;
	}

	int start_x = (tri.x0 < 0) ? 0 : tri.x0;
	int end_x = (tri.x1 >= atlas_w) ? atlas_w - 1 : tri.x1;
	int start_y = (tri.y0 < 0) ? 0 : tri.y0;
	int end_y = (tri.y1 >= atlas_h) ? atlas_h - 1 : tri.y1;

	if (end_x < start_x || end_y < start_y) {
		printf("[DIAG-BINNER-REJECT] Out of canvas bounds scissor.\n");
		return;
	}

	int tile_x0 = start_x / TILE_SIZE;
	int tile_y0 = start_y / TILE_SIZE;
	int tile_x1 = end_x / TILE_SIZE;
	int tile_y1 = end_y / TILE_SIZE;

	printf("[DIAG-BINNER-GRID] Tiles: X[%d..%d], Y[%d..%d]\n", tile_x0, tile_x1, tile_y0, tile_y1);

	for (int ty = tile_y0; ty <= tile_y1; ty++) {
		for (int tx = tile_x0; tx <= tile_x1; tx++) {
			int tile_pixel_x0 = tx * TILE_SIZE;
			int tile_pixel_y0 = ty * TILE_SIZE;
			int tile_pixel_x1 = tile_pixel_x0 + TILE_SIZE - 1;
			int tile_pixel_y1 = tile_pixel_y0 + TILE_SIZE - 1;

			int local_start_x = (start_x > tile_pixel_x0) ? start_x : tile_pixel_x0;
			int local_end_x = (end_x < tile_pixel_x1) ? end_x : tile_pixel_x1;
			int local_start_y = (start_y > tile_pixel_y0) ? start_y : tile_pixel_y0;
			int local_end_y = (end_y < tile_pixel_y1) ? end_y : tile_pixel_y1;

			if (local_end_x < local_start_x || local_end_y < local_start_y) {
				continue;
			}

			struct lp_mesa_triangle binned_tri = tri;
			printf("[DIAG-BINNER-BIAS-STEP] Computing Tile Space Translations for tx:%d, ty:%d\n",
				   tx, ty);
			for (int i = 0; i < 3; i++) {
				int64_t bias_y = IMUL64(tri.plane[i].dcdy, ty) * TILE_SIZE;
				int64_t bias_x = IMUL64(tri.plane[i].dcdx, tx) * TILE_SIZE;
				binned_tri.plane[i].c = tri.plane[i].c + bias_y - bias_x;
				printf("  Plane %d -> tri.c: %lld | bias_y: %lld | bias_x: %lld | binned_c: %lld\n",
					   i, tri.plane[i].c, bias_y, bias_x, binned_tri.plane[i].c);
			}

			binned_tri.x0 = local_start_x;
			binned_tri.x1 = local_end_x;
			binned_tri.y0 = local_start_y;
			binned_tri.y1 = local_end_y;

			printf(
				"[DIAG-BINNER-CALL] Launching lp_rast_triangle for Scissor: X[%d..%d], Y[%d..%d]\n",
				local_start_x, local_end_x, local_start_y, local_end_y);

			lp_rast_triangle(&binned_tri, local_start_x, local_start_y, local_end_x, local_end_y,
							 buffer, atlas_w, color, bottom_edge_rule, tile_pixel_x0,
							 tile_pixel_y0);
		}
	}
}
