/* =============================================================================
   FILE: catclock_hands.c (Hardware Accelerated Native 16-bit Hands Target Engine)
   ============================================================================= */
#include "catclock_shared.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

static const int HANDS_MATRIX_COLS = 6;
static const int HANDS_MATRIX_ROWS_PER_GROUP = 10;
static const float BASE_CELL_W = 64.0f;
static const float BASE_CELL_H = 96.0f;

static void LocalDrawHandGeometry(SDL_Renderer *renderer, float cx, float cy, float length, float thickness, float pivot_back, float angle_rad, SDL_Color col) {
    float cos_a = cosf(angle_rad);
    float sin_a = sinf(angle_rad);

    float fx = sin_a;
    float fy = -cos_a;
    float rx = cos_a;
    float ry = sin_a;

    float tip_x = cx + (length * fx);
    float tip_y = cy + (length * fy);

    float half_thick = thickness / 2.0f;
    float base_left_x = cx - (half_thick * rx) - (pivot_back * fx);
    float base_left_y = cy - (half_thick * ry) - (pivot_back * fy);
    float base_right_x = cx + (half_thick * rx) - (pivot_back * fx);
    float base_right_y = cy + (half_thick * ry) - (pivot_back * fy);

    SDL_Vertex vertices[3];
    SDL_FColor fcol = { col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, col.a / 255.0f };

    vertices[0] = (SDL_Vertex){ { tip_x, tip_y }, fcol, { 0.0f, 0.0f } };
    vertices[1] = (SDL_Vertex){ { base_left_x, base_left_y }, fcol, { 0.0f, 0.0f } };
    vertices[2] = (SDL_Vertex){ { base_right_x, base_right_y }, fcol, { 0.0f, 0.0f } };

    SDL_RenderGeometry(renderer, NULL, vertices, 3, NULL, 0);
}

void REBUILD_pre_rendered_60phase_atlas(SDL_Renderer* renderer, PreFlippedAtlas* atlas, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int win_w = 0, win_h = 0;
    SDL_GetRenderOutputSize(renderer, &win_w, &win_h);

    float scale = (float)win_w / 150.0f;
    if (scale <= 0.0f) scale = 1.0f;

    if (atlas->atlas_texture != NULL && scale == atlas->last_scale) {
        return;
    }

    if (atlas->atlas_texture != NULL) {
        fprintf(stderr, "[VRAM_TARGET] Destroying 16-bit Hands Target Atlas (Scale: %f)\n", atlas->last_scale);
        SDL_DestroyTexture(atlas->atlas_texture);
        atlas->atlas_texture = NULL;
    }

    atlas->cell_w = (int)(BASE_CELL_W * scale);
    atlas->cell_h = (int)(BASE_CELL_H * scale);

    int atlas_w = atlas->cell_w * HANDS_MATRIX_COLS;
    int atlas_h = atlas->cell_h * HANDS_MATRIX_ROWS_PER_GROUP * 3;

    fprintf(stderr, "[VRAM_TARGET] Allocating 16-bit Native Target Hands Atlas: %dx%d (Scale: %f)\n", atlas_w, atlas_h, scale);

    /* Allocate the texture directly as an on-GPU 16-bit Render Target */
    atlas->atlas_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA4444, SDL_TEXTUREACCESS_TARGET, atlas_w, atlas_h);
    if (!atlas->atlas_texture) return;

    SDL_SetTextureBlendMode(atlas->atlas_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(atlas->atlas_texture, SDL_SCALEMODE_LINEAR);

    /* Bind and bake directly within VRAM without reading back any data blocks to the CPU */
    SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, atlas->atlas_texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    SDL_Color color_hands = { r, g, b, a };
    SDL_Color color_secs  = { 255, 0, 0, 255 };

    float local_cx = (float)atlas->cell_w / 2.0f;
    float local_cy = (float)atlas->cell_h / 2.0f;

    for (int phase = 0; phase < TOTAL_PHASES; phase++) {
        float angle_rad = ((float)phase / 60.0f) * 2.0f * (float)M_PI;
        int col = phase % HANDS_MATRIX_COLS;
        int row = phase / HANDS_MATRIX_COLS;

        float base_x = (float)(col * atlas->cell_w);
        float base_y_hour   = (float)(row * atlas->cell_h);
        float base_y_minute = (float)((row + HANDS_MATRIX_ROWS_PER_GROUP) * atlas->cell_h);
        float base_y_second = (float)((row + (HANDS_MATRIX_ROWS_PER_GROUP * 2)) * atlas->cell_h);

        atlas->hour_src_rects[phase]   = (SDL_FRect){ base_x, base_y_hour,   (float)atlas->cell_w, (float)atlas->cell_h };
        atlas->minute_src_rects[phase] = (SDL_FRect){ base_x, base_y_minute, (float)atlas->cell_w, (float)atlas->cell_h };
        atlas->second_src_rects[phase] = (SDL_FRect){ base_x, base_y_second, (float)atlas->cell_w, (float)atlas->cell_h };

        LocalDrawHandGeometry(renderer, base_x + local_cx, base_y_hour + local_cy, (float)atlas->cell_w * 0.30f, 10.0f * scale, 6.0f * scale, angle_rad, color_hands);
        LocalDrawHandGeometry(renderer, base_x + local_cx, base_y_minute + local_cy, (float)atlas->cell_h * 0.35f, 7.5f * scale, 7.0f * scale, angle_rad, color_hands);
        LocalDrawHandGeometry(renderer, base_x + local_cx, base_y_second + local_cy, (float)atlas->cell_h * 0.42f, 2.5f * scale, 10.0f * scale, angle_rad, color_secs);
    }

    SDL_SetRenderTarget(renderer, old_target);
    atlas->last_scale = scale;
}

void RUNTIME_blit_pre_rendered_hands(SDL_Renderer* renderer, PreFlippedAtlas* atlas, float centerX, float centerY, int hour, int minute, int second) {
    if (atlas->atlas_texture == NULL) return;

    int hour_phase   = (hour < 0 || hour >= TOTAL_PHASES) ? 0 : hour;
    int minute_phase = (minute < 0 || minute >= TOTAL_PHASES) ? 0 : minute;
    int second_phase = (second < 0 || second >= TOTAL_PHASES) ? 0 : second;

    float half_w = BASE_CELL_W / 2.0f;
    float half_h = BASE_CELL_H / 2.0f;
    SDL_FRect dst_rect = { centerX - half_w, centerY - half_h, BASE_CELL_W, BASE_CELL_H };

    SDL_RenderTexture(renderer, atlas->atlas_texture, &atlas->hour_src_rects[hour_phase], &dst_rect);
    SDL_RenderTexture(renderer, atlas->atlas_texture, &atlas->minute_src_rects[minute_phase], &dst_rect);
    SDL_RenderTexture(renderer, atlas->atlas_texture, &atlas->second_src_rects[second_phase], &dst_rect);
}
