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

        // Define the horizontal segment endpoints calculated by your layout geometry
        float start_x = cx + ox - width_at_y;
        float end_x   = cx + ox + width_at_y;

        // --------------------------------------------------------------------
        // LOCALIZED FIX: STATIONARY PRE-BAKE SHAPE MASK CLIP
        // --------------------------------------------------------------------
        // The maximum radius of a 23x23 eye socket is exactly 11.5px.
        // We use a tight 11.2f boundary to prevent edge bleeding on the canvas texture.
        float max_socket_radius = 11.2f * scale;
        float socket_dy = (float)y - cy; // Delta from stationary cell height center

        // Scan across individual horizontal coordinate points of the span
        for (float sx = start_x; sx <= end_x; sx += 1.0f) {
            float socket_dx = sx - cx; // Delta from stationary width center

            // Standard Circle Equation: x^2 + y^2 <= r^2
            if ((socket_dx * socket_dx) + (socket_dy * socket_dy) <= (max_socket_radius * max_socket_radius)) {
                SDL_RenderPoint(renderer, sx, (float)y);
            }
        }
        // --------------------------------------------------------------------
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

    SDL_Color pupil_color = { 255, 255, 255, 255 }; // Don't change color here. Use RenderAuthenticOriginalEyes(renderer, pupil_color) instead.
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

    Uint64 time_ms = SDL_GetTicks();
    float phase_ratio = (float)(time_ms % 2000) / 2000.0f;

    int frame_idx = (int)(phase_ratio * (float)required_frames);
    if (frame_idx >= required_frames) frame_idx = required_frames - 1;
    if (frame_idx < 0) frame_idx = 0;

    // Fixed unscaled layout baseline anchors
    float left_lx  = 48.0f;
    float left_ly  = 30.0f;
    float right_rx = 80.0f;
    float right_ry = 30.0f;

    // Grab pre-scaled source bounding boxes straight from your array cache
    SDL_FRect left_src = g_EyesState.look_src_rects[frame_idx];

    int right_frame_idx = (required_frames - 1 - frame_idx);
    if (right_frame_idx < 0) right_frame_idx = 0;

    SDL_FRect right_src = g_EyesState.look_src_rects[right_frame_idx];

    // LOCALIZED FIX:
    // Remove the double scale multiplication on destinations. The viewport matrix
    // handles coordinate matching natively while keeping the look_src_rects sizes intact.
    SDL_FRect left_dest  = { left_lx,  left_ly,  left_src.w / current_scale,  left_src.h / current_scale };
    SDL_FRect right_dest = { right_rx, right_ry, right_src.w / current_scale, right_src.h / current_scale };

    // Update color and alpha modulation parameters
    SDL_SetTextureColorMod(g_EyesState.atlas_texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(g_EyesState.atlas_texture, color.a);

    // Render Left Eye normally
    SDL_RenderTexture(renderer, g_EyesState.atlas_texture, &left_src, &left_dest);

    // Render Right Eye using the mirrored phase slice and hardware horizontal flip matrix
    SDL_RenderTextureRotated(
        renderer,
        g_EyesState.atlas_texture,
        &right_src,
        &right_dest,
        0.0,
        NULL,
        SDL_FLIP_HORIZONTAL
    );
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
