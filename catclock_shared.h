/* =============================================================================
   FILE: catclock_shared.h (Standardized Cross-Module Type Architecture)
   ============================================================================= */
#ifndef CATCLOCK_SHARED_H
#define CATCLOCK_SHARED_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BASELINE_CANVAS_W 150.0f
#define BASELINE_CANVAS_H 300.0f
#define CYCLE_PERIOD_MS   2000
#define TOTAL_PHASES      60

/* Asset Layer Hand Typings */
#define HAND_TYPE_HOUR    0
#define HAND_TYPE_MINUTE  1
#define HAND_TYPE_SECOND  2

typedef struct {
    float x;
    float y;
} OriginalPoint;

/* Standardized Compute Pass Layout Structure */
typedef void (*CatClock_ShaderCallback)(SDL_Renderer *renderer, int cell_w, int cell_h, float scale, int frame_idx, void *userdata);

/* Pure, Scale-Aware Pre-Baked Texture Metadata Framework */
typedef struct {
    SDL_Texture *texture;
    int total_frames;
    int cell_w;
    int cell_h;
    float last_scale;
    SDL_FRect *src_rects;
} CatClock_ComputeAtlas;

typedef struct CatClock_XbmLibrary CatClock_XbmLibrary;

typedef struct {
    int current_win_w;
    int current_win_h;
    int current_ssaa_factor;
    float current_scale;
    bool is_window_minimized;
    bool texture_cache_stale;
    SDL_Color fg_color;
    SDL_Color bg_color;

    /* Decoupled Lifecycles to Ensure Sharp Multi-Color Blending Profiles */
    CatClock_ComputeAtlas hands_atlas;
    CatClock_ComputeAtlas minutes_atlas;
    CatClock_ComputeAtlas seconds_atlas;
    CatClock_ComputeAtlas eyes_atlas;
    CatClock_ComputeAtlas tail_atlas;

    CatClock_XbmLibrary *xbm_lib;
    SDL_Texture *master_composite_layer;
    SDL_Texture *halo_layer;
} CatClock_AppContext;

extern int ssaa_factor;
extern int target_fps_limit;

int CompareFloats(const void *a, const void *b);
void CatClock_OnWindowResize(SDL_WindowEvent *resize_event, CatClock_AppContext *ctx, SDL_Renderer *renderer);
void CatClock_SynchronizePipelineAtlases(SDL_Renderer *renderer, CatClock_AppContext *ctx, float sway_deg, int hour_phase, int minute_phase, int second_phase);

CatClock_XbmLibrary *CatClock_InitXbmLibrary(SDL_Renderer *renderer);
void CatClock_DestroyXbmLibrary(CatClock_XbmLibrary *lib);
void CatClock_RenderXbmLayer(CatClock_XbmLibrary *lib, SDL_Renderer *renderer, const char *layer_id, SDL_Color color);
void CatClock_RenderXbmLayerOffset(CatClock_XbmLibrary *lib, SDL_Renderer *renderer, const char *layer_id, SDL_Color color, float offset_x, float offset_y);
void CatClock_RenderHaloOutline(CatClock_XbmLibrary *lib, SDL_Renderer *renderer, SDL_Color color);
SDL_Texture* CatClock_LoadDynamicXbmToRGBA4444(SDL_Renderer *renderer, const char *path, SDL_Color color, bool invert_mask);

/* Centralized Lifecycle Manager Pipeline Drivers */
void CatClock_RebakeComputeAtlas(SDL_Renderer *renderer, CatClock_ComputeAtlas *atlas, int cell_base_w, int cell_base_h, int total_frames, int cols, CatClock_ShaderCallback shader, void *userdata);
void CatClock_DestroyComputeAtlas(CatClock_ComputeAtlas *atlas);

/* Stateless Asset Pass Callbacks */
void CatClock_ShaderHands(SDL_Renderer *renderer, int cell_w, int cell_h, float scale, int frame_idx, void *userdata);
void CatClock_ShaderEyes(SDL_Renderer *renderer, int cell_w, int cell_h, float scale, int frame_idx, void *userdata);
void CatClock_ShaderTail(SDL_Renderer *renderer, int cell_w, int cell_h, float scale, int frame_idx, void *userdata);

#endif /* CATCLOCK_SHARED_H */
