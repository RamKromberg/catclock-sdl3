/* ==========================================================================
   FILE: catclock_shared.h (Extended Tracker context Profile)
   ========================================================================== */
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

typedef struct {
    SDL_Texture* atlas_texture;
    SDL_FRect hour_src_rects[TOTAL_PHASES];
    SDL_FRect minute_src_rects[TOTAL_PHASES];
    SDL_FRect second_src_rects[TOTAL_PHASES];
    float last_scale;
    int cell_w;
    int cell_h;
} PreFlippedAtlas;

typedef struct CatClock_XbmLibrary CatClock_XbmLibrary;

typedef struct {
    float x;
    float y;
} OriginalPoint;

/* Central Tracking Structure */
typedef struct {
    int current_win_w;
    int current_win_h;
    int current_ssaa_factor;
    float current_scale;
    bool is_window_minimized;
    bool texture_cache_stale;
    SDL_Color fg_color;
    SDL_Color bg_color;
    PreFlippedAtlas hands_atlas_meta;
    CatClock_XbmLibrary *xbm_lib;
    SDL_Texture *master_composite_layer;
    SDL_Texture *halo_layer; /* Added: Persistent offscreen hardware cache pointer target */
} CatClock_AppContext;

extern int ssaa_factor;
extern int target_fps_limit;

int CompareFloats(const void *a, const void *b);
void CatClock_OnWindowResize(SDL_WindowEvent *resize_event, CatClock_AppContext *ctx, SDL_Renderer *renderer);
void CatClock_SynchronizePipelineAtlases(SDL_Renderer *renderer, CatClock_AppContext *ctx, float sway_deg, int hour_phase, int minute_phase, int second_phase);

/* XBM Downstream Components Inter-op Spec */
CatClock_XbmLibrary *CatClock_InitXbmLibrary(SDL_Renderer *renderer);
void CatClock_DestroyXbmLibrary(CatClock_XbmLibrary *lib);
void CatClock_RebakeXbmTextures(SDL_Renderer *renderer, CatClock_XbmLibrary *lib);
void CatClock_RenderXbmLayer(CatClock_XbmLibrary *lib, SDL_Renderer *renderer, const char *layer_id, SDL_Color color);

SDL_Texture* CatClock_LoadDynamicXbmToRGBA4444(SDL_Renderer *renderer, const char *path, SDL_Color color, bool invert_mask);
void CatClock_SetXbmLayerScaleMode(CatClock_XbmLibrary *lib, const char *layer_id, SDL_ScaleMode mode);
void CatClock_SetXbmLayerBlendMode(CatClock_XbmLibrary *lib, const char *layer_id, SDL_BlendMode mode);

void CatClock_RenderHaloOutline(CatClock_XbmLibrary *lib, SDL_Renderer *renderer, SDL_Color color);

/* Rendering Modules */
void REBUILD_pre_rendered_60phase_atlas(SDL_Renderer* renderer, PreFlippedAtlas* atlas, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void RUNTIME_blit_pre_rendered_hands(SDL_Renderer* renderer, PreFlippedAtlas* atlas, float centerX, float centerY, int hour, int minute, int second);
void RenderAuthenticOriginalEyes(SDL_Renderer *renderer, SDL_Color color);

void RenderOriginalThickSwayingTail(SDL_Renderer *renderer, float cx, float cy, float angle_deg, SDL_Color color, bool inflate_mode);

void CatClock_RenderXbmLayerOffset(CatClock_XbmLibrary *lib, SDL_Renderer *renderer, const char *layer_id, SDL_Color color, float offset_x, float offset_y);

void CatClock_DestroyEyesPipeline(void);

#endif /* CATCLOCK_SHARED_H */
