/******************************************************************************
 * File Name:    catclock_shared.h
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

#ifndef CATCLOCK_SHARED_H
#define CATCLOCK_SHARED_H

// === CORRECTED INSERTION IN catclock_shared.h ===
#ifndef SOKOL_GFX_INCLUDED
#include "sokol_gfx.h"
#endif

/* Forward declare the external logging tracking function used by main() */
void slog_func(const char* tag, uint32_t log_level, uint32_t log_item_id, const char* message,
			   uint32_t line_nr, const char* filename, void* user_data);

#include <SDL3/SDL.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Absolute Physical Hardware Mapping Geometry Definitions */
#define BASELINE_CANVAS_W 101.0f
#define BASELINE_CANVAS_H 201.0f
#define DECORATED_CANVAS_W 150.0f
#define DECORATED_CANVAS_H 300.0f
#define CHOP_OFFSET_X 24.0f
#define CHOP_OFFSET_Y 10.0f

// Palette Index Constants (8-Bit Compressed Array Layers)
#define PALETTE_TRANSPARENT 0
#define PALETTE_CAT_BODY 1 // Body, nose, tail main mesh
#define PALETTE_SCLERA 2 // Sclera socket rectangles
#define PALETTE_PUPIL 3 // Moving eye pupils
#define PALETTE_HAND_HOUR 4 // Hour hand line geometry
#define PALETTE_HAND_MINUTE 5 // Minute hand line geometry
#define PALETTE_HAND_SECOND 6 // Second hand line geometry
#define PALETTE_NECKTIE 7 // Necktie structure
#define PALETTE_HALO 8 // 1px Outline / Tail halo background

#define CYCLE_PERIOD_MS 2000
#define TOTAL_PHASES 60

// Asset Layer Typings
#define HAND_TYPE_HOUR 0
#define HAND_TYPE_MINUTE 1
#define HAND_TYPE_SECOND 2

typedef struct CatClock_XbmLibrary CatClock_XbmLibrary;

/* --- GPU-COMPATIBLE VERTEX DATA LAYOUT --- */
typedef struct {
	float position[2]; // x, y
	float color[4]; // r, g, b, a
} CatClock_GpuVertex;

/* Unified 160-byte flat uniform block matching std140 layout bounds exactly */
typedef struct {
	float tail_uv[4]; // Offset 0:   x, y, w, h
	float eyes_uv[4]; // Offset 16:  x, y, w, h
	float hours_uv[4]; // Offset 32:  x, y, w, h
	float mins_uv[4]; // Offset 48:  x, y, w, h
	float secs_uv[4]; // Offset 64:  x, y, w, h
	float cat_color[4]; // Offset 80:  r, g, b, a
	float tie_color[4]; // Offset 96:  r, g, b, a
	float pupil_color[4]; // Offset 112: r, g, b, a
	float sclera_color[4]; // Offset 128: r, g, b, a
	float detail_color[4]; // Offset 144: r, g, b, a
} CatClock_ShaderUniforms;

/* --- 16-BYTE ALIGNED UNIFORM STRUCT --- */
typedef struct {
	float current_scale;
	float offset_x;
	float offset_y;
	float alignment_padding;
	float cat_color[4];
	float tie_color[4];
	float pupil_color[4];
	float sclera_color[4];
} CatClock_RenderUniforms;

typedef struct {
	SDL_Texture* texture; // Immediate 32-bit hardware target layer
	uint8_t* index_buffer; // Compressed 8-bit CPU palette memory sheet
	int total_frames; // Total animation phases tracked in space
	int cell_w; // Scaled cell boundary width matrix
	int cell_h; // Scaled cell boundary height matrix
	int atlas_w; // Total width of the continuous atlas sheet
	int atlas_h; // Total height of the continuous atlas sheet
	float last_scale; // Scale factors checkpoint monitor
	SDL_FRect* src_rects; // Frame clip boundary tracking coordinates
} CatClock_ComputeAtlas;

// Unified App Context with Feature Restoration Flags
typedef struct {
	int current_win_w;
	int current_win_h;
	float current_scale;
	Uint64 current_frame_step;
	bool is_window_minimized;
	bool texture_cache_stale;
	SDL_Color fg_color;
	SDL_Color bg_color;

	// Decoupled Lifecycles for Sharp Multi-Color Blending
	CatClock_ComputeAtlas hours_atlas;
	CatClock_ComputeAtlas minutes_atlas;
	CatClock_ComputeAtlas seconds_atlas;
	CatClock_ComputeAtlas eyes_atlas;
	CatClock_ComputeAtlas tail_atlas;

	CatClock_XbmLibrary* xbm_lib;
	SDL_Texture* master_composite_layer;
	SDL_Texture* halo_layer;

	// Dedicated Eye Mask Hardware VRAM Surface
	SDL_Texture* eye_clipping_stencil;

	// Flag Maps
	bool hide_seconds; /* -noseconds */
	bool disable_outline; /* -nooutline */
	bool use_decorations; /* -decorations */
	bool disable_always_on_top; /* -notop */

	// Dynamic Theme Overrides
	SDL_Color tie_color; /* -tiecolor */
	SDL_Color pupil_color; /* -pupilcolor */
	SDL_Color cat_color; /* -catcolor */
	SDL_Color hour_color; /* -hourcolor */
	SDL_Color minute_color; /* -minutecolor */
	SDL_Color second_color; /* -secondcolor */
	SDL_Color detail_color; /* -detailcolor */
	SDL_Color sclera_color; /* -scleracolor */
	SDL_Color window_bg_color; /* Optional -decorations hex override */

	// 1-bit static lookup geometry tables
	int hitbox_mask_w;
	int hitbox_mask_h;
	uint8_t* hitbox_bitmask;

	// Boot-Pass 1-Bit CPU Memory Lookup Tables (Total Footprint: 2,774 Bytes)
	uint8_t* master_silhouette; // Target Grid: 101x201 bits -> floor((101+7)/8)*201 = 2,613 Bytes
	uint8_t* clean_eye_mask; // Target Grid: 54x23 bits  -> floor((54+7)/8)*23  = 161 Bytes
	float last_applied_halo_scale;
	sg_image cat_halo_img;
	sg_image body_composite_img;

	// Plain system memory array for hardware-free asset mask lookups
	int software_mask_w;
	int software_mask_h;
	uint8_t* software_eyes_bitmask;
#ifdef CATCLOCK_TELEMETRY
	/* Hardware pipeline diagnostic telemetry metrics */
	CatClock_TelemetryContext metrics;
#endif
} CatClock_AppContext;

typedef struct {
	CatClock_AppContext* app_ctx;
	float offset_x;
	float offset_y;
	bool force_halo_color;
} CatClock_TailShaderArgs;

typedef void (*CatClock_ShaderCallback)(void* render_dest, int cell_x, int cell_y, int sheet_w,
										int sheet_h, int frame_idx, void* userdata);

CatClock_XbmLibrary* CatClock_InitXbmLibrary(SDL_Renderer* renderer);
void CatClock_GetCatbackData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h);
void CatClock_GetCatwhiteData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h);
void CatClock_GetCattieBodyData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h);
void CatClock_GetCattieLineData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h);
void CatClock_GetHitboxData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h);
void CatClock_GetEyesMaskData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h);
void CatClock_DestroyXbmLibrary(CatClock_XbmLibrary* lib);

struct CatClock_AppContext;

int QsortCompareFloats(const void* a, const void* b);
void CatClock_OnWindowResize(SDL_WindowEvent* resize_event, CatClock_AppContext* ctx,
							 SDL_Renderer* renderer);
void CatClock_ExecuteScaleDependentEdgeDilation(float current_scale);
void CatClock_SynchronizePipelineAtlases(SDL_Renderer** renderer_ptr, CatClock_AppContext* ctx,
										 float sway_deg, int hour_phase, int minute_phase,
										 int second_phase);

void CatClock_RebakeComputeAtlas(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas,
								 int cell_base_w, int cell_base_h, int total_frames, int cols,
								 CatClock_ShaderCallback shader, void* userdata);
void CatClock_DestroyComputeAtlas(CatClock_ComputeAtlas* atlas);

/* Safe integer context shader callbacks using unified 10-column grids */
void CatClock_ShaderHours(void* render_dest, int cell_x, int cell_y, int sheet_w, int sheet_h,
						  int frame_idx, void* userdata);
void CatClock_ShaderEyes(void* render_dest, int cell_x, int cell_y, int sheet_w, int sheet_h,
						 int frame_idx, void* userdata);
void CatClock_ShaderTail(void* render_dest, int cell_x, int cell_y, int sheet_w, int sheet_h,
						 int frame_idx, void* userdata);

/* Pure Hardware Sokol Presentation Management Interfaces */
void InitSokolRenderPipeline(void);
void AllocateStreamTextures(void);
void PushActiveIndexBuffersToVRAM(void);
void PushShaderUniforms(void);
void ExecuteEventDrivenMainLoop(SDL_Window* window);
void ReleaseSokolRenderPipeline(void);

/* --- PERFORMANCE TELEMETRY MONITORING SUBSYSTEM --- */
#ifdef CATCLOCK_TELEMETRY
typedef struct {
	Uint64 start_ticks;
	Uint64 rolling_accumulator;
	Uint32 sample_counter;
} CatClock_TelemetryFence;

typedef struct {
	CatClock_TelemetryFence tail_halo;
	CatClock_TelemetryFence eyes_pupils;
	CatClock_TelemetryFence clock_hands;
	Uint32 logging_frequency;
} CatClock_TelemetryContext;
#endif

/**
 * @brief Plots a single index byte into a palettized or 1-bit style texture sheet buffer.
 *
 * GPU MIGRATION REFERENCE NOTE:
 *   - This mimics standard fragment shader writing to a 2D render target texture (imageStore /
 * OutColor).
 *   - Coordinates are explicitly bounds-checked here to prevent horizontal line address wrapping
 *     or linear stride memory corruption.
 */
static inline void PlotSoftwarePixel(uint8_t* buffer, int x, int y, int atlas_w, int atlas_h,
									 uint8_t palette_idx) {
	if (x >= 0 && x < atlas_w && y >= 0 && y < atlas_h) {
		// Linear stride translation matching classic row-major texture addressing: Index = (Y *
		// Width) + X
		buffer[y * atlas_w + x] = palette_idx;
	}
}

/**
 * @brief Renders a solid polygon primitive using an industry-standard Bounding Box Half-Space (Edge
 * Function) algorithm.
 * @note  CRITICAL MATHEMATICAL RESOLUTION: Old vertical-sorting scanline blitters break when a
 * primitive's Y-coordinates collapse to zero height (y0 == y1 == y2). This GPU-style edge engine
 * evaluates every pixel center in a bounding envelope, ensuring perfectly symmetrical sub-pixel
 * coverage across all cardinal axes (0, 15, 30, 45 seconds).
 *
 * GPU MIGRATION BLUEPRINT:
 *   - This exact logic maps directly to a hardware GPU Rasterizer stage.
 *   - Variables w0, w1, and w2 correspond to Edge Functions (Barycentric coordinates) used to
 * determine fragment inclusion inside a primitive hull.
 */
static inline void FillSoftwareTriangle(uint8_t* buffer, int x0, int y0, int x1, int y1, int x2,
										int y2, int atlas_w, int atlas_h, uint8_t palette_idx) {
	/* 1. EXTRACT GEOMETRIC BOUNDING ENVELOPE
	 *    Determines the absolute outer dimensional boundaries of the vertex triplet.
	 *    Limits pixel processing loops strictly to the primitive's local canvas area.
	 */
	int min_x = x0;
	if (x1 < min_x)
		min_x = x1;
	if (x2 < min_x)
		min_x = x2;
	int max_x = x0;
	if (x1 > max_x)
		max_x = x1;
	if (x2 > max_x)
		max_x = x2;
	int min_y = y0;
	if (y1 < min_y)
		min_y = y1;
	if (y2 < min_y)
		min_y = y2;
	int max_y = y0;
	if (y1 > max_y)
		max_y = y1;
	if (y2 > max_y)
		max_y = y2;

	/* 2. SCISSOR CLIP PASS
	 *    Clips coordinates against the master texture atlas sheet boundaries.
	 *    Prevents thread out-of-bounds writing when drawing shapes near the cell edges.
	 */
	if (min_x < 0)
		min_x = 0;
	if (max_x >= atlas_w)
		max_x = atlas_w - 1;
	if (min_y < 0)
		min_y = 0;
	if (max_y >= atlas_h)
		max_y = atlas_h - 1;

	/* 3. PARALLELIZABLE HALF-SPACE LOOP MATRIX
	 *    Iterates through every discrete pixel center within the computed bounding box window.
	 */
	for (int py = min_y; py <= max_y; py++) {
		for (int px = min_x; px <= max_x; px++) {

			/* 2D Cross Product (Z-axis Determinant) of directed edge vectors relative to pixel
			 * center (px, py). Positive value = pixel sits to the "left" of the directed line
			 * segment. Negative value = pixel sits to the "right" of the directed line segment.
			 * Zero value     = pixel sits exactly on the edge boundary line.
			 *
			 * FORMULA ARCHITECTURE: W = (V1.x - V0.x) * (P.y - V0.y) - (V1.y - V0.y) * (P.x - V0.x)
			 */

			/* Edge function 1: Vector from v0 to v1 evaluated at pixel (px, py) */
			int w0 = (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0);
			/* Edge function 2: Vector from v1 to v2 evaluated at pixel (px, py) */
			int w1 = (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1);
			/* Edge function 3: Vector from v2 to v0 evaluated at pixel (px, py) */
			int w2 = (x0 - x2) * (py - y2) - (y0 - y2) * (px - x2);

			/* HALF-SPACE CONVENTION COVERAGE CHECK:
			 * A pixel center is inside the primitive hull if and only if all edge evaluations share
			 * the same sign.
			 *   - (w0 >= 0 && w1 >= 0 && w2 >= 0) catches Clockwise (CW) vertex winding layouts.
			 *   - (w0 <= 0 && w1 <= 0 && w2 <= 0) catches Counter-Clockwise (CCW) vertex winding
			 * layouts.
			 *
			 * This uniform verification is why horizontal and vertical flat shapes render
			 * cleanly--the edge function continuously evaluates the 2D bounding hull instead of
			 * relying on vertical scanline split offsets.
			 */
			if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
				PlotSoftwarePixel(buffer, px, py, atlas_w, atlas_h, palette_idx);
			}
		}
	}
}

extern CatClock_AppContext ctx;
extern int target_fps_limit;

int CompareFloats(const void* a, const void* b);
void CatClock_OnWindowResize(SDL_WindowEvent* resize_event, CatClock_AppContext* ctx,
							 SDL_Renderer* renderer);
void CatClock_SynchronizePipelineAtlases(SDL_Renderer** renderer_ptr, CatClock_AppContext* ctx,
										 float sway_deg, int hour_phase, int minute_phase,
										 int second_phase);
void CatClock_BakeMaterialCompositionMatrix(float current_scale);

void CatClock_GetCatbackData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h);
void CatClock_GetCatwhiteData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h);
void CatClock_GetCattieData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h);

void CatClock_RenderXbmLayer(CatClock_XbmLibrary* lib, SDL_Renderer* renderer, const char* layer_id,
							 SDL_Color color);
void CatClock_RenderXbmLayerOffset(CatClock_XbmLibrary* lib, SDL_Renderer* renderer,
								   const char* layer_id, SDL_Color color, float offset_x,
								   float offset_y);
void CatClock_RenderHaloOutline(CatClock_XbmLibrary* lib, SDL_Renderer* renderer, SDL_Color color);
void CatClock_DestroyXbmLibrary(CatClock_XbmLibrary* lib);
SDL_Texture* CatClock_GetXbmTextureLayer(CatClock_XbmLibrary* lib, const char* layer_id);
uint8_t* CatClock_LoadRawXbmBits(const char* filepath, int* out_w, int* out_h);

void CatClock_RebakeComputeAtlas(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas,
								 int cell_base_w, int cell_base_h, int total_frames, int cols,
								 CatClock_ShaderCallback shader, void* userdata);
void CatClock_DestroyComputeAtlas(CatClock_ComputeAtlas* atlas);

/* CLEAN UPDATED INT INTERFACES: Aligned matching our strict safe bounds blitting engine */
void CatClock_ShaderHands(void* render_dest, int cell_x, int cell_y, int sheet_w, int sheet_h,
						  int frame_idx, void* userdata);
void CatClock_ShaderEyes(void* render_dest, int cell_x, int cell_y, int sheet_w, int sheet_h,
						 int frame_idx, void* userdata);
void CatClock_ShaderTail(void* render_dest, int cell_x, int cell_y, int sheet_w, int sheet_h,
						 int frame_idx, void* userdata);
void CatClock_ShaderTailHaloBake(void* render_dest, int cell_x, int cell_y, int sheet_w,
								 int sheet_h, int frame_idx, void* userdata);

/* Command-line subsystem processing links */
void PrintHelpDocumentation(const char* program_name);
void CatClock_DebugDumpGenericAtlasToPam(const char* filepath, const uint8_t* raw_buffer, int w,
										 int h);
bool HelperParseHexColor(const char* hex_str, SDL_Color* out_color);
void ParseCommandLineArguments(int argc, char* argv[], CatClock_AppContext* ctx);
void CatClock_BakeUnscaledMaterialIDStaging(uint8_t* target_buffer);

#ifdef CATCLOCK_DIAGNOSTIC
#define CATCLOCK_LOG_DIAG(ctx_ptr, fmt, ...)                                                       \
	do {                                                                                           \
		if ((ctx_ptr)->current_frame_step % 60 == 0) {                                             \
			SDL_Log("[DIAG COMPRESSED] " fmt, ##__VA_ARGS__);                                      \
		}                                                                                          \
	} while (0)
#else
#define CATCLOCK_LOG_DIAG(ctx_ptr, fmt, ...)                                                       \
	do {                                                                                           \
	} while (0)
#endif

#endif /* CATCLOCK_SHARED_H */
