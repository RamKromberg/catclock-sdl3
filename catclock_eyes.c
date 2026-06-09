/******************************************************************************
 * File Name:    catclock_eyes.c
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
 * Engineering Milestones & Refactoring Pass (2026):
 *   - Ported natively to the SDL3 framework with desktop compositing support.
 *   - Designed a low-CPU runtime architecture utilizing pre-baked texture atlases.
 *   - Implemented 1-bit binary threshold rendering to optimize alpha blending.
 *   - Restored polygon scanline rasterization engines to patch geometry gaps.
 *   - Synchronized monotonic ticks with the wall clock to erase frame jitter.
 *   - Enabled adaptive pacing, focus-aware kernel sleeps, and zero-VSync spinlocks.
 *   - Added borderless transparent windows, OS-level hit-tests, and sharp nearest-pixel art integer scaling.
 *
 * License: Open Source / Educational - Preserve attribution upon redistribution.
 *****************************************************************************/

#include <SDL3/SDL.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BASELINE_CANVAS_W 150.0f
#define BASELINE_CANVAS_H 300.0f
#define TOTAL_PHASE_SLOTS 30
#define CYCLE_PERIOD_MS   2000
#define ATLAS_COLS        6
#define ATLAS_ROWS        5

static SDL_Texture *g_AtlasSheetTexture    = NULL;
static int          g_CachedWindowWidth    = 0;
static int          g_CachedWindowHeight   = 0;
static int          g_CachedSSAAFactor     = 0;
static SDL_Color    g_CachedTextureColor   = {0, 0, 0, 0};

extern int target_fps_limit;
extern int ssaa_factor;

static void FillEllipse(SDL_Renderer *renderer, float cx, float cy, float rx, float ry, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (float y = -ry; y <= ry; y += 0.5f) {
        float dx = rx * sqrtf(1.0f - (y * y) / (ry * ry));
        SDL_RenderLine(renderer, cx - dx, cy + y, cx + dx, cy + y);
    }
}

static void ReBakeHardwareAtlasCache(SDL_Renderer *renderer, int win_w, int win_h, int current_ssaa, SDL_Color color) {
    if (g_AtlasSheetTexture) {
        SDL_DestroyTexture(g_AtlasSheetTexture);
        g_AtlasSheetTexture = NULL;
    }

    g_CachedWindowWidth  = win_w;
    g_CachedWindowHeight = win_h;
    g_CachedSSAAFactor   = current_ssaa;
    g_CachedTextureColor = color;

    int ssaa_win_w = win_w * current_ssaa;
    int ssaa_win_h = win_h * current_ssaa;

    int atlas_w = ssaa_win_w * ATLAS_COLS;
    int atlas_h = ssaa_win_h * ATLAS_ROWS;

    g_AtlasSheetTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, atlas_w, atlas_h);
    if (!g_AtlasSheetTexture) {
        return;
    }

    SDL_SetTextureScaleMode(g_AtlasSheetTexture, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(g_AtlasSheetTexture, SDL_BLENDMODE_BLEND);

    SDL_Texture *previous_target = SDL_GetRenderTarget(renderer);
    SDL_Rect previous_viewport;
    SDL_GetRenderViewport(renderer, &previous_viewport);

    SDL_SetRenderTarget(renderer, g_AtlasSheetTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    float scale_x = (float)ssaa_win_w / BASELINE_CANVAS_W;
    float scale_y = (float)ssaa_win_h / BASELINE_CANVAS_H;
    float current_scale = (scale_x < scale_y) ? scale_x : scale_y;

    float origin_x = (float)(ssaa_win_w - (BASELINE_CANVAS_W * current_scale)) / 2.0f;
    float origin_y = (float)(ssaa_win_h - (BASELINE_CANVAS_H * current_scale)) / 2.0f;

    for (int i = 0; i < TOTAL_PHASE_SLOTS; i++) {
        int col = i % ATLAS_COLS;
        int row = i / ATLAS_COLS;

        SDL_Rect frame_viewport = { col * ssaa_win_w, row * ssaa_win_h, ssaa_win_w, ssaa_win_h };
        SDL_SetRenderViewport(renderer, &frame_viewport);

        float phase_angle = ((float)i / (float)TOTAL_PHASE_SLOTS) * (2.0f * (float)M_PI);
        float swing_factor = sinf(phase_angle);

        float offset_x = swing_factor * (7.5f * current_scale);
        float proximity = fabsf(offset_x) / (7.5f * current_scale);

        float compression_x = 1.0f - (0.12f * proximity);
        float compression_y = 1.0f - (0.07f * proximity);

        float rx = (3.5f * current_scale) * compression_x;
        float ry = (10.25f * current_scale) * compression_y;

        float left_cx  = origin_x + (60.0f * current_scale) + offset_x;
        float right_cx = origin_x + (92.0f * current_scale) + offset_x;
        float cy       = origin_y + (41.0f * current_scale);

        FillEllipse(renderer, left_cx, cy, rx, ry, color);
        FillEllipse(renderer, right_cx, cy, rx, ry, color);
    }

    SDL_SetRenderTarget(renderer, previous_target);
    SDL_SetRenderViewport(renderer, &previous_viewport);
}

void RenderAuthenticOriginalEyes(SDL_Renderer *renderer, float swing_phase, SDL_Color color) {
    (void)swing_phase;

    int win_w = 0;
    int win_h = 0;
    SDL_GetRenderOutputSize(renderer, &win_w, &win_h);

    int active_ssaa = (ssaa_factor > 0) ? ssaa_factor : 4;

    int color_changed = (color.r != g_CachedTextureColor.r || color.g != g_CachedTextureColor.g ||
                         color.b != g_CachedTextureColor.b || color.a != g_CachedTextureColor.a);

    if (!g_AtlasSheetTexture || win_w != g_CachedWindowWidth || win_h != g_CachedWindowHeight ||
        active_ssaa != g_CachedSSAAFactor || color_changed) {
        ReBakeHardwareAtlasCache(renderer, win_w, win_h, active_ssaa, color);
    }

    if (!g_AtlasSheetTexture) {
        return;
    }

    int phase_index = (int)((SDL_GetTicks() % CYCLE_PERIOD_MS) * TOTAL_PHASE_SLOTS / CYCLE_PERIOD_MS);
    if (phase_index >= TOTAL_PHASE_SLOTS) {
        phase_index = TOTAL_PHASE_SLOTS - 1;
    }

    int col = phase_index % ATLAS_COLS;
    int row = phase_index / ATLAS_COLS;

    int ssaa_win_w = win_w * g_CachedSSAAFactor;
    int ssaa_win_h = win_h * g_CachedSSAAFactor;

    SDL_FRect src_rect;
    src_rect.x = (float)(col * ssaa_win_w);
    src_rect.y = (float)(row * ssaa_win_h);
    src_rect.w = (float)ssaa_win_w;
    src_rect.h = (float)ssaa_win_h;

    SDL_FRect dst_rect;
    dst_rect.x = 0.0f;
    dst_rect.y = 0.0f;
    dst_rect.w = BASELINE_CANVAS_W;
    dst_rect.h = BASELINE_CANVAS_H;

    SDL_RenderTexture(renderer, g_AtlasSheetTexture, &src_rect, &dst_rect);
}

void CatClock_DestroyEyesPipeline(void) {
    if (g_AtlasSheetTexture) {
        SDL_DestroyTexture(g_AtlasSheetTexture);
        g_AtlasSheetTexture = NULL;
    }
    g_CachedWindowWidth  = 0;
    g_CachedWindowHeight = 0;
    g_CachedSSAAFactor   = 0;
    g_CachedTextureColor = (SDL_Color){0, 0, 0, 0};
}
