/* =============================================================================
   FILE: catclock_tail.c (Calibrated Uniform 96x96 Target Spacing Engine)
   ============================================================================= */
#include "catclock_shared.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

static const int TAIL_MATRIX_COLS = 8;

static OriginalPoint original_repository_tail[] = {
    { -9.0f, -10.0f }, { -9.0f, 20.0f }, {-10.5f, 40.0f }, {-11.5f, 55.0f },
    { -9.0f, 68.0f }, { 0.0f, 78.0f }, { 11.0f, 76.0f }, { 18.0f, 68.0f },
    { 20.0f, 54.0f }, { 16.5f, 44.0f }, { 9.0f, 42.0f }, { 4.5f, 50.0f },
    { 8.5f, 62.0f }, { 3.0f, 68.0f }, { -2.0f, 62.0f }, { -1.0f, 45.0f },
    { -4.5f, 32.0f }, { -6.0f, 15.0f }, { -6.0f, -10.0f }
};
/* ARRAY MACRO FIX: Correct element size division math rules */
static const int TOTAL_TAIL_POINTS = sizeof(original_repository_tail) / sizeof(original_repository_tail[0]);

static void LocalDrawHardwareTailPolygon(SDL_Renderer *renderer, float cx, float cy, float angle_deg, float scale, SDL_Color color) {
    float rad = (angle_deg * (float)M_PI) / 180.0f;
    float cos_a = cosf(rad);
    float sin_a = sinf(rad);

    SDL_Vertex *vertices = (SDL_Vertex*)SDL_malloc(sizeof(SDL_Vertex) * TOTAL_TAIL_POINTS);
    if (!vertices) return;

    SDL_FColor fcol = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };

    for (int i = 0; i < TOTAL_TAIL_POINTS; i++) {
        /* Map original relative geometry parameters onto scaled coordinate layout spaces */
        float tx = original_repository_tail[i].x * scale;
        float ty = original_repository_tail[i].y * scale;

        float rot_x = tx * cos_a - ty * sin_a;
        float rot_y = tx * sin_a + ty * cos_a;

        vertices[i] = (SDL_Vertex){ { cx + rot_x, cy + rot_y }, fcol, { 0.0f, 0.0f } };
    }

    int total_triangles = TOTAL_TAIL_POINTS - 2;
    int *indices = (int*)SDL_malloc(sizeof(int) * total_triangles * 3);
    if (indices) {
        for (int i = 0; i < total_triangles; i++) {
            indices[i * 3]     = 0;
            indices[i * 3 + 1] = i + 1;
            indices[i * 3 + 2] = i + 2;
        }

        SDL_RenderGeometry(renderer, NULL, vertices, TOTAL_TAIL_POINTS, indices, total_triangles * 3);
        SDL_free(indices);
    }
    SDL_free(vertices);
}

void REBUILD_pre_rendered_tail_atlas(SDL_Renderer *renderer, CatClock_TailAtlasMeta *atlas, SDL_Color color) {
    int win_w = 0, win_h = 0;
    SDL_GetRenderOutputSize(renderer, &win_w, &win_h);

    float scale = (float)win_w / 150.0f;
    if (scale <= 0.0f) scale = 1.0f;

    int required_phases = target_fps_limit <= 0 ? 60 : target_fps_limit;

    if (atlas->atlas_texture != NULL && scale == atlas->last_cached_scale && required_phases == atlas->allocated_phases) {
        return;
    }

    if (atlas->atlas_texture != NULL) {
        fprintf(stderr, "[VRAM_TARGET] Destroying 16-bit Tail Target Atlas (Scale: %f)\n", atlas->last_cached_scale);
        SDL_DestroyTexture(atlas->atlas_texture);
        atlas->atlas_texture = NULL;
    }
    if (atlas->phase_src_rects) SDL_free(atlas->phase_src_rects);

    atlas->phase_src_rects = (SDL_FRect*)SDL_malloc(sizeof(SDL_FRect) * required_phases);
    if (!atlas->phase_src_rects) return;

    atlas->allocated_phases = required_phases;
    atlas->last_cached_scale = scale;

    atlas->cell_w = (int)(96.0f * scale);
    atlas->cell_h = (int)(96.0f * scale);

    int cols = TAIL_MATRIX_COLS;
    int rows = (required_phases + cols - 1) / cols;
    int atlas_w = cols * atlas->cell_w;
    int atlas_h = rows * atlas->cell_h;

    fprintf(stderr, "[VRAM_TARGET] Allocating 16-bit Native Target Tail Atlas: %dx%d (Scale: %f)\n", atlas_w, atlas_h, scale);

    atlas->atlas_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA4444, SDL_TEXTUREACCESS_TARGET, atlas_w, atlas_h);
    if (!atlas->atlas_texture) return;

    SDL_SetTextureBlendMode(atlas->atlas_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(atlas->atlas_texture, SDL_SCALEMODE_LINEAR);

    SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, atlas->atlas_texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    /* ALIGNMENT FIX: Position the local pivot origin center at the top edge of each cell block */
    float pivot_x = (float)atlas->cell_w / 2.0f;
    float pivot_y = 12.0f * scale;

    for (int i = 0; i < required_phases; i++) {
        int cell_x = (i % cols) * atlas->cell_w;
        int cell_y = (i / cols) * atlas->cell_h;

        atlas->phase_src_rects[i] = (SDL_FRect){ (float)cell_x, (float)cell_y, (float)atlas->cell_w, (float)atlas->cell_h };

        float phase_ratio = (float)i / (float)required_phases;
        float current_angle = 19.5f * sinf(phase_ratio * 2.0f * (float)M_PI);

        LocalDrawHardwareTailPolygon(renderer, (float)cell_x + pivot_x, (float)cell_y + pivot_y, current_angle, scale, color);
    }

    SDL_SetRenderTarget(renderer, old_target);
}

void RUNTIME_blit_pre_rendered_tail(SDL_Renderer *renderer, CatClock_TailAtlasMeta *atlas, float centerX, float centerY, float current_sway_deg) {
    if (atlas->atlas_texture == NULL) return;
    (void)current_sway_deg;

    int total_frames = atlas->allocated_phases;
    uint64_t time_ms = SDL_GetTicks();
    float phase_ratio = (float)(time_ms % CYCLE_PERIOD_MS) / (float)CYCLE_PERIOD_MS;

    int frame_idx = (int)(phase_ratio * (float)total_frames);
    if (frame_idx >= total_frames) frame_idx = total_frames - 1;
    if (frame_idx < 0) frame_idx = 0;

    SDL_FRect src_rect = atlas->phase_src_rects[frame_idx];

    float base_draw_w = 96.0f;
    float base_draw_h = 96.0f;

    /* VERTICAL PIVOT POSITION RECALIBRATION: Anchors the top center section under the torso edge bounds */
    SDL_FRect dst_rect = { centerX - (base_draw_w / 2.0f), centerY - 12.0f, base_draw_w, base_draw_h };

    SDL_RenderTexture(renderer, atlas->atlas_texture, &src_rect, &dst_rect);
}

void CatClock_DestroyTailPipeline(CatClock_TailAtlasMeta *atlas) {
    if (atlas->atlas_texture) {
        fprintf(stderr, "[VRAM_TARGET] Destroying 16-bit Tail Target Atlas (Shutdown)\n");
        SDL_DestroyTexture(atlas->atlas_texture);
        atlas->atlas_texture = NULL;
    }
    if (atlas->phase_src_rects) {
        SDL_free(atlas->phase_src_rects);
        atlas->phase_src_rects = NULL;
    }
    atlas->allocated_phases = 0;
    atlas->last_cached_scale = 0.0f;
}
