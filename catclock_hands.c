/* ==========================================================================
   FILE: catclock_hands.c
   ========================================================================== */
#include "catclock_shared.h"
#include <stdlib.h>
#include <math.h>

static const int HANDS_MATRIX_COLS = 6;
static const int HANDS_MATRIX_ROWS_PER_GROUP = 10;

static const float BASE_CELL_W = 64.0f;
static const float BASE_CELL_H = 96.0f;

// Maximum scale safety constraint to prevent blowing past the 16384 GPU limit
static const float MAX_SAFE_SCALE = 5.50f;

// High-performance hardware geometry tapered triangle hand primitive
static void LocalDrawHandGeometry(SDL_Renderer* renderer, float cx, float cy, float length, float thickness, float pivot_back, float angle_rad, SDL_Color color) {
    float cos_a = cosf(angle_rad);
    float sin_a = sinf(angle_rad);

    float fx = sin_a;
    float fy = -cos_a;
    float rx = cos_a;
    float ry = sin_a;

    float tip_x = cx + (length * fx);
    float tip_y = cy + (length * fy);

    // Base thickness distribution offset vectors
    float dx = (thickness / 2.0f) * rx;
    float dy = (thickness / 2.0f) * ry;

    SDL_Vertex vertices[3];
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;

    // Vertex 0: Wide Base Left (Displaced backwards by the decoupled pivot parameter)
    vertices[0].position.x = cx - dx - (pivot_back * fx);
    vertices[0].position.y = cy - dy - (pivot_back * fy);
    vertices[0].color.r = r; vertices[0].color.g = g; vertices[0].color.b = b; vertices[0].color.a = a;
    vertices[0].tex_coord.x = 0.0f; vertices[0].tex_coord.y = 0.0f;

    // Vertex 1: Wide Base Right (Displaced backwards by the decoupled pivot parameter)
    vertices[1].position.x = cx + dx - (pivot_back * fx);
    vertices[1].position.y = cy + dy - (pivot_back * fy);
    vertices[1].color.r = r; vertices[1].color.g = g; vertices[1].color.b = b; vertices[1].color.a = a;
    vertices[1].tex_coord.x = 0.0f; vertices[1].tex_coord.y = 0.0f;

    // Vertex 2: Tapered Hand Tip Point
    vertices[2].position.x = tip_x;
    vertices[2].position.y = tip_y;
    vertices[2].color.r = r; vertices[2].color.g = g; vertices[2].color.b = b; vertices[2].color.a = a;
    vertices[2].tex_coord.x = 0.0f; vertices[2].tex_coord.y = 0.0f;

    const int indices[3] = { 0, 1, 2 };
    SDL_RenderGeometry(renderer, NULL, vertices, 3, indices, 3);
}

void REBUILD_pre_rendered_60phase_atlas(SDL_Renderer* renderer, PreFlippedAtlas* atlas, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int win_w = 0, win_h = 0;
    SDL_GetRenderOutputSize(renderer, &win_w, &win_h);

    float scale = (float)win_w / 150.0f;
    if (scale <= 0.0f) scale = 1.0f;

    float bake_scale = (scale > MAX_SAFE_SCALE) ? MAX_SAFE_SCALE : scale;

    if (atlas->atlas_texture != NULL && scale == atlas->last_scale) {
        return;
    }

    if (atlas->atlas_texture != NULL) {
        SDL_DestroyTexture(atlas->atlas_texture);
        atlas->atlas_texture = NULL;
    }

    atlas->cell_w = (int)(BASE_CELL_W * bake_scale);
    atlas->cell_h = (int)(BASE_CELL_H * bake_scale);

    int atlas_w = atlas->cell_w * HANDS_MATRIX_COLS;
    int atlas_h = atlas->cell_h * HANDS_MATRIX_ROWS_PER_GROUP * 3;

    atlas->atlas_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        atlas_w,
        atlas_h
    );

    if (!atlas->atlas_texture) {
        SDL_Log("Failed to allocate textures matrix target: %s", SDL_GetError());
        return;
    }

    SDL_SetTextureBlendMode(atlas->atlas_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(atlas->atlas_texture, SDL_SCALEMODE_LINEAR);

    SDL_Texture* prev_target = SDL_GetRenderTarget(renderer);
    float prev_scale_x = 1.0f, prev_scale_y = 1.0f;
    SDL_GetRenderScale(renderer, &prev_scale_x, &prev_scale_y);

    SDL_SetRenderTarget(renderer, atlas->atlas_texture);
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    float cx = (float)atlas->cell_w / 2.0f;
    float cy = (float)atlas->cell_h / 2.0f;

    SDL_Color hand_color = { r, g, b, a };
    SDL_Color second_color = { 255, 0, 0, 255 };

    // 3. Implement Proper 2D Grid Matrix Packing
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

        // --- CUSTOM HAND COSMETICS ---
        float hour_length = (float)atlas->cell_w * 0.30f;
        float hour_thick  = 10.0f * bake_scale;
        float hour_pivot  = 6.0f * bake_scale;

        float min_length  = (float)atlas->cell_h * 0.35f;
        float min_thick   = 7.5f * bake_scale;
        float min_pivot   = 7.0f * bake_scale;

        float sec_length  = (float)atlas->cell_h * 0.42f;
        float sec_thick   = 2.5f * bake_scale;
        float sec_pivot   = 10.0f * bake_scale;

        // --- Render Passes ---
        SDL_Rect viewport_hour = { (int)base_x, (int)base_y_hour, atlas->cell_w, atlas->cell_h };
        SDL_SetRenderViewport(renderer, &viewport_hour);
        LocalDrawHandGeometry(renderer, cx, cy, hour_length, hour_thick, hour_pivot, angle_rad, hand_color);

        SDL_Rect viewport_minute = { (int)base_x, (int)base_y_minute, atlas->cell_w, atlas->cell_h };
        SDL_SetRenderViewport(renderer, &viewport_minute);
        LocalDrawHandGeometry(renderer, cx, cy, min_length, min_thick, min_pivot, angle_rad, hand_color);

        SDL_Rect viewport_second = { (int)base_x, (int)base_y_second, atlas->cell_w, atlas->cell_h };
        SDL_SetRenderViewport(renderer, &viewport_second);
        LocalDrawHandGeometry(renderer, cx, cy, sec_length, sec_thick, sec_pivot, angle_rad, second_color);
    }

    SDL_SetRenderViewport(renderer, NULL);
    SDL_SetRenderTarget(renderer, prev_target);
    SDL_SetRenderScale(renderer, prev_scale_x, prev_scale_y);

    atlas->last_scale = scale;
}

void RUNTIME_blit_pre_rendered_hands(SDL_Renderer* renderer, PreFlippedAtlas* atlas, float centerX, float centerY, int hour, int minute, int second) {
    if (atlas->atlas_texture == NULL) {
        return;
    }

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
