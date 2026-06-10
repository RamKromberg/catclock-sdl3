#include "catclock_shared.h"
#include <SDL3/SDL.h>
#include <math.h>

#define BASE_EYE_CELL_DIM 24.0f
static const int EYES_MATRIX_COLS = 10;
#define CYCLE_PERIOD_MS 2000

extern int target_fps_limit;

typedef struct {
    SDL_Texture *atlas_texture;
    SDL_FRect *look_src_rects;
    int allocated_frames;
    float last_cached_scale;
} CatClock_EyesInternalState;

static CatClock_EyesInternalState g_EyesState = { NULL, NULL, 0, 0.0f };

static void LowLevel_DrawSpecializedPupil(SDL_Renderer *renderer, float cx, float cy, float scale, float phase_ratio, SDL_Color color) {
    /* ------------------------------------------------------------- */
    /* CALIBRATED PUPIL GEOMETRY AND MOVEMENT VARIABLES              */
    /* ------------------------------------------------------------- */
    float base_w       = 2.5f * scale;
    float base_h       = 10.5f * scale;
    float max_offset_x = 7.0f * scale;

    float local_shift_x = 0.0f * scale;
    float local_shift_y = 0.0f * scale;
    /* ------------------------------------------------------------- */

    float swing_angle = phase_ratio * 2.0f * M_PI;
    float look_x = sinf(swing_angle);

    float ox = (look_x * max_offset_x) + local_shift_x;
    float oy = 0.0f + local_shift_y;

    float compression = sqrtf(fmaxf(0.1f, 1.0f - (look_x * look_x * 0.45f)));
    float pup_w = base_w * compression;
    float pup_h = base_h;

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    int start_y = (int)floorf(cy + oy - pup_h);
    int end_y = (int)ceilf(cy + oy + pup_h);

    for (int y = start_y; y <= end_y; y++) {
        float dy = (float)y - (cy + oy);
        float h_ratio = dy / pup_h;
        float width_factor = fmaxf(0.0f, 1.0f - (h_ratio * h_ratio));
        float width_at_y = pup_w * sqrtf(width_factor);

        SDL_RenderLine(renderer, cx + ox - width_at_y, (float)y, cx + ox + width_at_y, (float)y);
    }
}

static void REBUILD_pre_rendered_eyes_atlas(SDL_Renderer *renderer, float current_scale) {
    int required_frames = target_fps_limit <= 0 ? 60 : target_fps_limit;

    if (g_EyesState.atlas_texture) SDL_DestroyTexture(g_EyesState.atlas_texture);
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

    g_EyesState.atlas_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, atlas_w, atlas_h);

    if (!g_EyesState.atlas_texture) return;

    SDL_SetTextureBlendMode(g_EyesState.atlas_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(g_EyesState.atlas_texture, SDL_SCALEMODE_LINEAR);

    SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
    float old_scale_x = 0.0f, old_scale_y = 0.0f;
    SDL_GetRenderScale(renderer, &old_scale_x, &old_scale_y);

    SDL_SetRenderTarget(renderer, g_EyesState.atlas_texture);
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_Color pupil_color = { 0, 0, 0, 255 };
    for (int i = 0; i < required_frames; i++) {
        int cell_x = (i % cols) * cell_w;
        int cell_y = (i / cols) * cell_h;

        g_EyesState.look_src_rects[i] = (SDL_FRect){ (float)cell_x, (float)cell_y, (float)cell_w, (float)cell_h };

        SDL_Rect cell_viewport = { cell_x, cell_y, cell_w, cell_h };
        SDL_SetRenderViewport(renderer, &cell_viewport);

        /*
         * FIXING CENTER OFFSET SCALING MISMAPPING:
         * Shift the local drawing midpoint metrics to absolute integers (12.0f).
         * This pins the vector generation line layout loops safely inside
         * the bounding limits of the 24-pixel layout grid target blocks.
         */
        float local_cx = (float)(int)(cell_w / 2);
        float local_cy = (float)(int)(cell_h / 2);
        float phase_ratio = (float)i / (float)required_frames;

        LowLevel_DrawSpecializedPupil(renderer, local_cx, local_cy, current_scale, phase_ratio, pupil_color);
    }

    SDL_SetRenderViewport(renderer, NULL);
    SDL_SetRenderTarget(renderer, old_target);
    SDL_SetRenderScale(renderer, old_scale_x, old_scale_y);
}

void RenderAuthenticOriginalEyes(SDL_Renderer *renderer, SDL_Color color) {
    int output_w = 0, output_h = 0;
    bool rgos = SDL_GetRenderOutputSize(renderer, &output_w, &output_h);
    if (!rgos || output_w <= 0) output_w = 150;

    float current_scale = (float)output_w / 150.0f;
    int required_frames = target_fps_limit <= 0 ? 60 : target_fps_limit;

    if (!g_EyesState.atlas_texture || !g_EyesState.look_src_rects || g_EyesState.last_cached_scale != current_scale || g_EyesState.allocated_frames != required_frames) {
        REBUILD_pre_rendered_eyes_atlas(renderer, current_scale);
    }

    if (!g_EyesState.atlas_texture || !g_EyesState.look_src_rects || g_EyesState.allocated_frames <= 0) {
        return;
    }

    uint64_t current_ticks = SDL_GetTicks();
    int frame_index = (int)(((current_ticks % CYCLE_PERIOD_MS) * (uint64_t)g_EyesState.allocated_frames) / CYCLE_PERIOD_MS);
    if (frame_index >= g_EyesState.allocated_frames) frame_index = g_EyesState.allocated_frames - 1;

    SDL_FRect src_rect = g_EyesState.look_src_rects[frame_index];

    float left_lx  = 48.0f;
    float left_ly  = 30.0f;
    float right_rx = 80.0f;
    float right_ry = 30.0f;

    SDL_FRect left_dst  = { left_lx, left_ly, BASE_EYE_CELL_DIM, BASE_EYE_CELL_DIM };
    SDL_FRect right_dst = { right_rx, right_ry, BASE_EYE_CELL_DIM, BASE_EYE_CELL_DIM };

    SDL_SetTextureColorMod(g_EyesState.atlas_texture, color.r, color.g, color.b);

    SDL_RenderTexture(renderer, g_EyesState.atlas_texture, &src_rect, &left_dst);
    SDL_RenderTexture(renderer, g_EyesState.atlas_texture, &src_rect, &right_dst);
}

void CatClock_DestroyEyesPipeline(void) {
    if (g_EyesState.atlas_texture) {
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
