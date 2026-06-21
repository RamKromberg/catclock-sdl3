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

#include <SDL3/SDL.h>
#include <stdbool.h>

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

#define CYCLE_PERIOD_MS 2000
#define TOTAL_PHASES 60

// Asset Layer Typings
#define HAND_TYPE_HOUR 0
#define HAND_TYPE_MINUTE 1
#define HAND_TYPE_SECOND 2

/* --- GPU-COMPATIBLE VERTEX DATA LAYOUT --- */
typedef struct {
	float position[2]; // x, y
	float color[4]; // r, g, b, a
} CatClock_GpuVertex;

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

typedef void (*CatClock_ShaderCallback)(SDL_Renderer* renderer, int cell_w, int cell_h, float scale,
										int frame_idx, void* userdata);

typedef struct {
	SDL_Texture* texture;
	int total_frames;
	int cell_w;
	int cell_h;
	float last_scale;
	SDL_FRect* src_rects;
} CatClock_ComputeAtlas;

typedef struct CatClock_XbmLibrary CatClock_XbmLibrary;

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
	CatClock_ComputeAtlas hands_atlas; // Restored to hands_atlas
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
#ifdef CATCLOCK_TELEMETRY
	/* Hardware pipeline diagnostic telemetry metrics */
	CatClock_TelemetryContext metrics;
#endif
} CatClock_AppContext;

struct CatClock_AppContext;

typedef struct {
	CatClock_AppContext* app_ctx;
	float offset_x;
	float offset_y;
	bool force_halo_color;
} CatClock_TailShaderArgs;

/* CRITICAL LINK FIX: Broaden visibility context scope to global compiler translation units */
extern CatClock_AppContext ctx;
extern int target_fps_limit;

int CompareFloats(const void* a, const void* b);
void CatClock_OnWindowResize(SDL_WindowEvent* resize_event, CatClock_AppContext* ctx,
							 SDL_Renderer* renderer);
void CatClock_SynchronizePipelineAtlases(SDL_Renderer** renderer_ptr, CatClock_AppContext* ctx,
										 float sway_deg, int hour_phase, int minute_phase,
										 int second_phase);

CatClock_XbmLibrary* CatClock_InitXbmLibrary(SDL_Renderer* renderer);
void CatClock_RenderXbmLayer(CatClock_XbmLibrary* lib, SDL_Renderer* renderer, const char* layer_id,
							 SDL_Color color);
void CatClock_RenderXbmLayerOffset(CatClock_XbmLibrary* lib, SDL_Renderer* renderer,
								   const char* layer_id, SDL_Color color, float offset_x,
								   float offset_y);
void CatClock_RenderHaloOutline(CatClock_XbmLibrary* lib, SDL_Renderer* renderer, SDL_Color color);
void CatClock_DestroyXbmLibrary(CatClock_XbmLibrary* lib);
SDL_Texture* CatClock_GetXbmTextureLayer(CatClock_XbmLibrary* lib, const char* layer_id);

void CatClock_RebakeComputeAtlas(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas,
								 int cell_base_w, int cell_base_h, int total_frames, int cols,
								 CatClock_ShaderCallback shader, void* userdata);
void CatClock_DestroyComputeAtlas(CatClock_ComputeAtlas* atlas);
void CatClock_ShaderTailHaloBake(SDL_Renderer* renderer, int cell_w, int cell_h, float scale,
								 int frame_idx, void* userdata);

/* Command-line subsystem processing links */
void PrintHelpDocumentation(const char* program_name);
bool HelperParseHexColor(const char* hex_str, SDL_Color* out_color);
void ParseCommandLineArguments(int argc, char* argv[], CatClock_AppContext* ctx);

void CatClock_ShaderHands(SDL_Renderer* renderer, int cell_w, int cell_h, float scale,
						  int frame_idx, void* userdata);
void CatClock_ShaderEyes(SDL_Renderer* renderer, int cell_w, int cell_h, float scale, int frame_idx,
						 void* userdata);
void CatClock_ShaderTail(SDL_Renderer* renderer, int cell_w, int cell_h, float scale, int frame_idx,
						 void* userdata);

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
