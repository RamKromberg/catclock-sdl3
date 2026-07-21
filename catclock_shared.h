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

#ifndef TEST_MODE
#define TEST_MODE 0
#endif

#include "sokol/sokol_gfx.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

/* ==========================================================================
   1. EXPLICIT GEOMETRY, TIMING & VIEWPORT METRICS
   ========================================================================== */
#define BASELINE_CANVAS_W 101.0f
#define BASELINE_CANVAS_H 201.0f
#define DECORATED_CANVAS_W 150.0f
#define DECORATED_CANVAS_H 300.0f
#define CHOP_OFFSET_X 24.0f
#define CHOP_OFFSET_Y 12.0f

#define ASSET_BODY_W 101
#define ASSET_BODY_H 201
#define ASSET_TAIL_W 96
#define ASSET_TAIL_H 96
#define GPU_PADDED_BODY_W 103
#define GPU_PADDED_BODY_H 203

#define VRAM_TEX_WIDTH 128
#define VRAM_TEX_HEIGHT 290
#define OFFSET_X 24
#define OFFSET_Y 1

#define TOTAL_HAND_PHASES 60
#define DEFAULT_FPS 30

/* Native Palette Index Identifiers matching drawing routines */
#define PALETTE_TRANSPARENT 0
#define PALETTE_CAT_BODY 1 /* Body, nose, tail main mesh */
#define PALETTE_CAT_WHITE 2 /* White fur body details */
#define PALETTE_SCLERA 2 /* Sclera socket rectangles */
#define PALETTE_PUPIL 3 /* Moving eye pupils */
#define PALETTE_HAND_HOUR 4 /* Hour hand line geometry */
#define PALETTE_HAND_MINUTE 5 /* Minute hand line geometry */
#define PALETTE_HAND_SECOND 6 /* Second hand line geometry */
#define PALETTE_NECKTIE 7 /* Necktie structure */
#define PALETTE_HALO 8 /* 1px Outline / Tail halo background */

/* Hand Class Identifiers */
#define HAND_TYPE_HOUR 0
#define HAND_TYPE_MINUTE 1
#define HAND_TYPE_SECOND 2

/* Opaque forward declaration ensuring visibility across all headers */
struct CatClock_XbmLibrary;

typedef struct {
	float offset_x;
	float offset_y;
	bool force_halo_color;
} CatClock_TailShaderArgs;

/* ==========================================================================
   2. GPU-COMPATIBLE STRUCTS & VERTEX DATA LAYOUTS
   ========================================================================== */
typedef struct {
	float pos[2]; // x, y coordinate attributes array
	float uv[2]; // u, v mapping boundaries array
} CatClock_GpuVertex;

/* Cleaned, Sokol-ready framework-agnostic atlas context structure */
typedef struct {
	uint8_t* index_buffer; // 8-bit CPU palette configuration map
	int total_frames; // Total tracked animation frames
	int cell_w; // Scaled layout cell width
	int cell_h; // Scaled layout cell height
	int atlas_w; // Total width of VRAM atlas sheet
	int atlas_h; // Total height of VRAM atlas sheet
	uint32_t scale_half_steps; // Deterministic integer scale tracker (e.g. 2 = 1.0x, 3 = 1.5x)
	SDL_FRect* src_rects; // Frame clip boundary coordinate array
	float last_scale; // Scale factors checkpoint monitor
} CatClock_ComputeAtlas;

/* ==========================================================================
   3. UNIFIED GLOBAL APPLICATION CONTEXT (RESTORED TARGET)
   ========================================================================== */
typedef struct {
	SDL_Window* window;
	uint32_t current_half_steps; // Scale tracking state bound to step changes
	bool use_decorations;
	int target_fps;
	uint64_t current_frame_step;
	bool disable_outline;
	bool disable_always_on_top;
	bool hide_seconds;
	bool texture_cache_stale;

	/* Configured interface color parameters */
	SDL_Color fg_color;
	SDL_Color bg_color;
	SDL_Color cat_color;
	SDL_Color tie_color;
	SDL_Color pupil_color;
	SDL_Color sclera_color;
	SDL_Color hour_color;
	SDL_Color minute_color;
	SDL_Color second_color;
	SDL_Color detail_color;
	SDL_Color window_bg_color;

	/* Foundational asset layout bitmasks retained for alignment verification */
	uint8_t* hitbox_bits;
	uint8_t* master_silhouette;
	uint8_t* clean_eye_mask;
	int software_mask_w;
	int software_mask_h;
	uint8_t* software_eyes_bitmap;

	/* Dedicated component texture targets */
	sg_image body_mask_texture;
	sg_sampler body_mask_sampler;
	sg_view body_mask_view;
	//sg_image tail_atlas_texture;
	//sg_image eyes_atlas_texture;
	//sg_image hands_atlas_texture;
	sg_image cat_halo_mmg;
	sg_image buf_composite_img;

	/* --- MESH GEOMETRY & PIPELINE RESOURCE HANDLES --- */
	sg_buffer vertex_buffer;
	sg_buffer index_buffer;
	sg_pipeline draw_pipeline;

	/* Instanced layout computational states */
	CatClock_ComputeAtlas hours_atlas;
	CatClock_ComputeAtlas minutes_atlas;
	CatClock_ComputeAtlas seconds_atlas;
	CatClock_ComputeAtlas tail_atlas;
	CatClock_ComputeAtlas eyes_atlas;
	CatClock_ComputeAtlas hands_atlas;
} CatClock_AppContext;

extern CatClock_AppContext ctx;

/* ==========================================================================
   4. CANONICAL HOOKS & DIAGNOSTIC INTERFACE SYSTEM
   ========================================================================== */
typedef void (*CatClock_ShaderCallback)(void* r_dest, int cell_x, int cell_y, int sheet_w,
										int sheet_h, int frame_idx, void* userdata);

/* Window and asset state lifecycle managers */
void CatClock_OnWindowResize(SDL_WindowEvent* resize_event, CatClock_AppContext* context,
							 void* renderer);
void CatClock_DestroyComputeAtlas(CatClock_ComputeAtlas* atlas);
void CatClock_RebakeComputeAtlas(void* renderer, CatClock_ComputeAtlas* atlas, int cell_base_w,
								 int cell_base_h, int total_frames, int cols,
								 CatClock_ShaderCallback shader, void* userdata);

/* Material composition staging validation hooks */
uint8_t* CatClock_PreBakeTieMask(const uint8_t* raw_catback, const uint8_t* raw_tie);
void CatClock_BakeUnscaledMaterialIDStaging(uint8_t* target_buffer,
											struct CatClock_XbmLibrary* lib);
void Diagnostics_DumpMaterialCompositionToDisk(struct CatClock_XbmLibrary* lib);

/* Core primitive rasterization engine shared helpers */
int CompareFloats(const void* a, const void* b);
void PlotSoftwarePixel(uint8_t* buffer, int x, int y, int width, int height, uint8_t token);
void DrawTriangleFixed(uint8_t* buffer, int32_t fx_x0, int32_t fx_y0, int32_t fx_x1, int32_t fx_y1,
					   int32_t fx_x2, int32_t fx_y2, int width, int height, uint8_t token);
void DrawTriangleFloat(uint8_t* buffer, float x0, float y0, float x1, float y1, float x2, float y2,
					   int width, int height, uint8_t token);
void DrawTriangleLegacy(uint8_t* buffer, int x0, int y0, int x1, int y1, int x2, int y2, int width,
						int height, uint8_t token);
void DrawTriangleFixedSDF(uint8_t* buffer, int32_t fx_x0, int32_t fx_y0,
						  int32_t fx_x1, int32_t fx_y1, int32_t fx_x2, int32_t fx_y2,
						  int width, int height, uint8_t token);

/* Shader pipeline reference routines */
void CatClock_ShaderTail(void* target_buffer, int cell_x, int cell_y, int sheet_w, int sheet_h,
						 int frame_idx, void* userdata);
void CatClock_ShaderEyes(void* renderer, int cell_x, int cell_y, int sheet_w, int sheet_h,
						 int frame_idx, void* userdata);
void CatClock_ShaderHands(void* renderer, int cell_x, int cell_y, int sheet_w, int sheet_h,
						  int frame_idx, void* userdata);

/* Resource loader lifecycles */
struct CatClock_XbmLibrary* CatClock_InitXbmLibrary(void* renderer);
void CatClock_DestroyXbmLibrary(struct CatClock_XbmLibrary* lib);
void CatClock_GetCatbackData(struct CatClock_XbmLibrary* lib, uint8_t** bits, int* w, int* h);
void CatClock_GetCatwhiteData(struct CatClock_XbmLibrary* lib, uint8_t** bits, int* w, int* h);
void CatClock_GetCattieBodyData(struct CatClock_XbmLibrary* lib, uint8_t** bits, int* w, int* h);
void CatClock_GetEyesData(struct CatClock_XbmLibrary* lib, uint8_t** bits, int* w, int* h);
void CatClock_GetHitboxData(struct CatClock_XbmLibrary* lib, uint8_t** bits, int* w, int* h);

/* Critical XBM Asset Loader & Diagnostic validation sink dumps */
uint8_t* CatClock_LoadRawXbmBits(const char* filepath, int* out_w, int* out_h);
void CatClock_DebugPrintXbmToTerminal(const char* label, const uint8_t* bits, int w, int h);
void CatClock_DebugDumpSingleLayerToDisk(const char* filename, const uint8_t* buffer, int width,
										 int height);
void CatClock_DebugDumpPamToDisk(const char* filepath, const uint8_t* material_grid, int w, int h);
void CatClock_DebugDumpGenericAtlasToPam(const char* filepath, const uint8_t* raw_buffer, int w,
										 int h);

void CatClock_UnpackStaticAssetsToStagingBuffer(uint32_t* dest_rgba_buffer, const uint8_t* catback_bits,
												const uint8_t* tie_body_bits, const uint8_t* catwhite_bits,
												const uint8_t* eyes_bits);

/* Command-line and configuration subroutines */
void PrintHelpDocumentation(const char* program_name);
bool HelperParseHexColor(const char* hex_str, SDL_Color* out_color);
void ParseCommandLineArguments(int argc, char* argv[], CatClock_AppContext* context);

/* TELEMETRY REGISTRATION INTERFACES */
void Diagnostics_LogAssetLifecycle(const char* asset_name, size_t buffer_size, int dimensions_w,
								   int dimensions_h);
void Diagnostics_LogShufflerIndex(const char* pipeline_channel, int target_frame_index,
								  int calculated_phase_limit);
void Diagnostics_LogScaleBoundaryChange(uint32_t step_value, float derived_multiplier);

#endif /* CATCLOCK_SHARED_H */
