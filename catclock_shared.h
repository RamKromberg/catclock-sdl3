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

#define BASELINE_CANVAS_W 150.0f
#define BASELINE_CANVAS_H 300.0f
#define CYCLE_PERIOD_MS 2000
#define TOTAL_PHASES 60

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

/* Unified App Context with Feature Restoration Flags */
typedef struct {
	int current_win_w;
	int current_win_h;
	int current_ssaa_factor;
	float current_scale;
	bool is_window_minimized;
	bool texture_cache_stale;
	SDL_Color fg_color;
	SDL_Color bg_color;

	/* Decoupled Lifecycles for Sharp Multi-Color Blending */
	CatClock_ComputeAtlas hands_atlas;
	CatClock_ComputeAtlas minutes_atlas;
	CatClock_ComputeAtlas seconds_atlas;
	CatClock_ComputeAtlas eyes_atlas;
	CatClock_ComputeAtlas tail_atlas;

	CatClock_XbmLibrary* xbm_lib;
	SDL_Texture* master_composite_layer;
	SDL_Texture* halo_layer;

	/* Dedicated Eye Mask Hardware VRAM Surface */
	SDL_Texture* eye_clipping_stencil;

	/* Task 1 & 2 Flag Maps */
	bool hide_seconds; /* -noseconds */
	bool disable_outline; /* -nooutline */
	bool use_decorations; /* -decorations */
	bool disable_always_on_top; /* -notop */

	/* Dynamic Theme Overrides */
	SDL_Color tie_color; /* -tiecolor */
	SDL_Color pupil_color; /* -pupilcolor */
	SDL_Color cat_color; /* -catcolor */
	SDL_Color hour_color; /* -hourcolor */
	SDL_Color minute_color; /* -minutecolor */
	SDL_Color second_color; /* -secondcolor */
	SDL_Color detail_color; /* -detailcolor */
	SDL_Color sclera_color; /* -scleracolor */
	SDL_Color window_bg_color; /* Optional -decorations hex override */
} CatClock_AppContext;

/* CRITICAL LINK FIX: Broaden visibility context scope to global compiler translation units */
extern CatClock_AppContext ctx;
extern int ssaa_factor;
extern int target_fps_limit;

int CompareFloats(const void* a, const void* b);
void CatClock_OnWindowResize(SDL_WindowEvent* resize_event, CatClock_AppContext* ctx,
							 SDL_Renderer* renderer);
void CatClock_SynchronizePipelineAtlases(SDL_Renderer* renderer, CatClock_AppContext* ctx,
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

void CatClock_RebakeComputeAtlas(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas,
								 int cell_base_w, int cell_base_h, int total_frames, int cols,
								 CatClock_ShaderCallback shader, void* userdata);
void CatClock_DestroyComputeAtlas(CatClock_ComputeAtlas* atlas);

#endif /* CATCLOCK_SHARED_H */
