/* =============================================================================
   FILE: catclock_eyes.c (Hardware Accelerated Native 16-bit Eyes Target Engine)
   ============================================================================= */
#include "catclock_shared.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define BASE_EYE_CELL_DIM 24.0f
static const int EYES_MATRIX_COLS = 10;

typedef struct {
    SDL_Texture *atlas_texture;
    SDL_FRect *look_src_rects;
    int allocated_frames;
    float last_cached_scale;
} CatClock_EyesInternalState;

static CatClock_EyesInternalState g_EyesState = { NULL, NULL, 0, 0.0f };

static void LocalDrawHardwarePupilOval(SDL_Renderer *renderer, float cx, float cy, float r_base_w, float r_base_h, float look_x, float max_offset_x, SDL_Color color) {
    float ox = look_x * max_offset_x;
    float oy = 0.0f;

    float compression = sqrtf(fmaxf(0.1f, 1.0f - (look_x * look_x * 0.45f)));
    float pup_w = r_base_w * compression;
    float pup_h = r_base_h;

    int segments = 24;
    int total_vertices = segments + 1;
    SDL_Vertex *vertices = (SDL_Vertex*)SDL_malloc(sizeof(SDL_Vertex) * total_vertices);
    int *indices = (int*)SDL_malloc(sizeof(int) * segments * 3);
    if (!vertices || !indices) {
        if (vertices) SDL_free(vertices);
        if (indices) SDL_free(indices);
        return;
    }

    /* TYPO FIX: Consistently extract color channels from the parameter object */
    SDL_FColor fcol = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };
    vertices[0] = (SDL_Vertex){ { cx + ox, cy + oy }, fcol, { 0.0f, 0.0f } };

    for (int i = 0; i < segments; i++) {
        float angle = ((float)i / (float)segments) * 2.0f * (float)M_PI;
        float vx = (cx + ox) + (pup_w * cosf(angle));
        float vy = (cy + oy) + (pup_h * sinf(angle));
        vertices[i + 1] = (SDL_Vertex){ { vx, vy }, fcol, { 0.0f, 0.0f } };

        indices[i * 3]     = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = (i == segments - 1) ? 1 : i + 2;
    }

    SDL_RenderGeometry(renderer, NULL, vertices, total_vertices, indices, segments * 3);
    SDL_free(vertices);
    SDL_free(indices);
}

void REBUILD_static_eyes_atlas(SDL_Renderer *renderer, float current_scale) {
    int required_frames = target_fps_limit <= 0 ? 60 : target_fps_limit;

    if (g_EyesState.atlas_texture) {
        fprintf(stderr, "[VRAM_TARGET] Destroying 16-bit Eyes Target Atlas (Scale: %f)\n", g_EyesState.last_cached_scale);
        SDL_DestroyTexture(g_EyesState.atlas_texture);
        g_EyesState.atlas_texture = NULL;
    }
    if (g_EyesState.look_src_rects) SDL_free(g_EyesState.look_src_rects);

    g_EyesState.look_src_rects = (SDL_FRect *)SDL_malloc(sizeof(SDL_FRect) * required_frames);
    if (!g_EyesState.look_src_rects) return;

    g_EyesState.allocated_frames = required_frames;
    g_EyesState.last_cached_scale = current_scale;

    int cell_w = (int)ceilf(BASE_EYE_CELL_DIM * current_scale);
    int cell_h = (int)ceilf(BASE_EYE_CELL_DIM * current_scale);
    int cols = EYES_MATRIX_COLS;
    int rows = (required_frames + cols - 1) / cols;
    int atlas_w = cols * cell_w;
    int atlas_h = rows * cell_h;

    fprintf(stderr, "[VRAM_TARGET] Allocating 16-bit Native Target Eyes Atlas: %dx%d (Scale: %f)\n", atlas_w, atlas_h, current_scale);

    g_EyesState.atlas_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA4444, SDL_TEXTUREACCESS_TARGET, atlas_w, atlas_h);
    if (!g_EyesState.atlas_texture) return;

    SDL_SetTextureBlendMode(g_EyesState.atlas_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(g_EyesState.atlas_texture, SDL_SCALEMODE_LINEAR);

    SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, g_EyesState.atlas_texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    float base_w       = 2.5f * current_scale;
    float base_h       = 10.5f * current_scale;
    float max_offset_x = 7.0f * current_scale;
    SDL_Color pure_white = { 255, 255, 255, 255 };

    for (int i = 0; i < required_frames; i++) {
        int cell_x = (i % cols) * cell_w;
        int cell_y = (i / cols) * cell_h;

        g_EyesState.look_src_rects[i] = (SDL_FRect){ (float)cell_x, (float)cell_y, (float)cell_w, (float)cell_h };
        float phase_ratio = (float)i / (float)required_frames;
        float swing_angle = phase_ratio * 2.0f * (float)M_PI;

        LocalDrawHardwarePupilOval(renderer, (float)cell_x + ((float)cell_w / 2.0f), (float)cell_y + ((float)cell_h / 2.0f),
                                   base_w, base_h, sinf(swing_angle), max_offset_x, pure_white);
    }

    SDL_SetRenderTarget(renderer, old_target);
}

void RUNTIME_blit_pre_rendered_eyes(SDL_Renderer *renderer, SDL_Color color) {
    int win_w = 0, win_h = 0;
    if (!SDL_GetRenderOutputSize(renderer, &win_w, &win_h) || win_w <= 0) win_w = 150;

    float current_scale = (float)win_w / 150.0f;
    int required_frames = target_fps_limit <= 0 ? 60 : target_fps_limit;

    if (!g_EyesState.atlas_texture || !g_EyesState.look_src_rects || g_EyesState.last_cached_scale != current_scale || g_EyesState.allocated_frames != required_frames) {
        REBUILD_static_eyes_atlas(renderer, current_scale);
    }

    uint64_t time_ms = SDL_GetTicks();
    float phase_ratio = (float)(time_ms % CYCLE_PERIOD_MS) / (float)CYCLE_PERIOD_MS;

    int frame_idx = (int)(phase_ratio * (float)required_frames);
    if (frame_idx >= required_frames) frame_idx = required_frames - 1;
    if (frame_idx < 0) frame_idx = 0;

    float left_lx  = 48.0f;
    float left_ly  = 30.0f;
    float right_rx = 80.0f;
    float right_ry = 30.0f;

    SDL_FRect left_src = g_EyesState.look_src_rects[frame_idx];
    int right_frame_idx = (required_frames - 1 - frame_idx);
    if (right_frame_idx < 0) right_frame_idx = 0;
    SDL_FRect right_src = g_EyesState.look_src_rects[right_frame_idx];

    SDL_FRect left_dest  = { left_lx,  left_ly,  left_src.w / current_scale,  left_src.h / current_scale };
    SDL_FRect right_dest = { right_rx, right_ry, right_src.w / current_scale, right_src.h / current_scale };

    SDL_SetTextureColorMod(g_EyesState.atlas_texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(g_EyesState.atlas_texture, color.a);

    SDL_RenderTexture(renderer, g_EyesState.atlas_texture, &left_src, &left_dest);
    SDL_RenderTextureRotated(renderer, g_EyesState.atlas_texture, &right_src, &right_dest, 0.0, NULL, SDL_FLIP_HORIZONTAL);
}

void CatClock_DestroyEyesPipeline(void) {
    if (g_EyesState.atlas_texture) {
        fprintf(stderr, "[VRAM_TARGET] Destroying 16-bit Eyes Target Atlas (Shutdown)\n");
        SDL_DestroyTexture(g_EyesState.atlas_texture);
        g_EyesState.atlas_texture = NULL;
    }
    if (g_EyesState.look_src_rects) {
        SDL_free(g_EyesState.look_src_rects);
        g_EyesState.look_src_rects = NULL;
    }
    g_EyesState.allocated_frames = 0;
    g_EyesState.last_cached_scale = 0.0f;
}
