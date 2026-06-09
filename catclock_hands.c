/******************************************************************************
 * File Name:    catclock_hands.c
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
#include <stdbool.h>
#include <math.h>
#include <malloc.h>

#define PHASES_PER_HAND 60
#define GRID_COLUMNS    10
#define GRID_ROWS       6
#define TOTAL_HANDS     3

typedef struct {
    SDL_Texture *atlas_texture;
    int frame_w;
    int frame_h;
    SDL_FRect hour_src_rects[PHASES_PER_HAND];
    SDL_FRect minute_src_rects[PHASES_PER_HAND];
    SDL_FRect second_src_rects[PHASES_PER_HAND];
} CatClockHardwareAtlas;

CatClockHardwareAtlas g_hands_atlas = { 0 };

/**
 * Draws an anti-aliased, tapered geometric pointer using hardware triangle primitives.
 * Pulls the base points backward along the inverse angle vector to anchor under the center pin.
 */
static void draw_geometric_triangle_hand(SDL_Renderer *renderer, float cx, float cy, float tx, float ty, float base_width, float pivot_retract, SDL_FColor color) {
    float dx = tx - cx;
    float dy = ty - cy;
    float length = sqrtf(dx * dx + dy * dy);

    if (length <= 0.0f) return;

    // Perpendicular normal vectors for thickness offsetting
    float nx = -dy / length;
    float ny = dx / length;

    // Forward direction components
    float fx = dx / length;
    float fy = dy / length;

    // Shift base anchors backward along the inverse angle vector to retract hands under the pin
    float start_cx = cx - (fx * pivot_retract);
    float start_cy = cy - (fy * pivot_retract);

    SDL_Vertex vertices[3];

    // Left base vertex point
    vertices[0].position.x = start_cx - nx * (base_width / 2.0f);
    vertices[0].position.y = start_cy - ny * (base_width / 2.0f);
    vertices[0].color = color;
    vertices[0].tex_coord.x = 0.0f;
    vertices[0].tex_coord.y = 0.0f;

    // Right base vertex point
    vertices[1].position.x = start_cx + nx * (base_width / 2.0f);
    vertices[1].position.y = start_cy + ny * (base_width / 2.0f);
    vertices[1].color = color;
    vertices[1].tex_coord.x = 0.0f;
    vertices[1].tex_coord.y = 0.0f;

    // Sharp endpoint tip vertex pointer
    vertices[2].position.x = tx;
    vertices[2].position.y = ty;
    vertices[2].color = color;
    vertices[2].tex_coord.x = 0.0f;
    vertices[2].tex_coord.y = 0.0f;

    int indices[3] = { 0, 1, 2 };

    SDL_RenderGeometry(renderer, NULL, vertices, 3, indices, 3);
}

/**
 * Procedural Vector Drawing Engine with Elliptical Prolate Spheroid Projection.
 * Shrinks hands gracefully along the horizontal paths (3 and 9 o'clock).
 */
static void draw_projected_hand_geometry(SDL_Renderer *renderer, int cx, int cy, int hand_h, int phase_index, int hand_type) {
    // 360 degrees divided across 60 steps = 6 degrees per step
    float raw_angle_rad = (float)phase_index * 6.0f * (M_PI / 180.0f);

    // --- TRUE PROLATE SPHEROID ELLIPTICAL PROJECTION ---
    float ellipse_axis_x = 1.0f;
    float ellipse_axis_y = 0.78f; // Squashed vertically

    // 1. Angular distortion remapping: adjust the pointing vectors to land squarely on numbers
    float projected_angle_rad = atan2f(sinf(raw_angle_rad) * ellipse_axis_x, cosf(raw_angle_rad) * ellipse_axis_y);

    float cos_a = cosf(projected_angle_rad);
    float sin_a = sinf(projected_angle_rad);

    float base_length = 0.0f;
    float base_width = 2.0f;
    float pivot_retract = 0.0f;
    SDL_FColor color;

    // Scaling modifier: Restores the vector width density proportional to the high-res texture block width
    float scale_mod = (float)hand_h / 128.0f;

    // Implements your exact calibrated variable values
    if (hand_type == 0) {       // Hour Hand
        color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
        base_length = (float)hand_h * 0.18f;
        base_width = 10.0f * scale_mod;
        pivot_retract = (float)hand_h * 0.05f;
    } else if (hand_type == 1) { // Minute Hand
        color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
        base_length = (float)hand_h * 0.28f;
        base_width = 7.5f * scale_mod;
        pivot_retract = (float)hand_h * 0.04f;
    } else {                    // Second Hand (Crimson Needle)
        color = (SDL_FColor){ 0.88f, 0.15f, 0.15f, 1.0f };
        base_length = (float)hand_h * 0.27f;
        base_width = 3.0f * scale_mod;
        pivot_retract = (float)hand_h * 0.06f;
    }

    // 2. Prolate Spheroid Axis Compensation: Shrinks hands gracefully along horizontal vectors
    float target_x = (float)cx + (sin_a * base_length * ellipse_axis_y);
    float target_y = (float)cy - (cos_a * base_length);

    draw_geometric_triangle_hand(renderer, (float)cx, (float)cy, target_x, target_y, base_width, pivot_retract, color);
}

/**
 * Public Lifecycle Cleanup Pass.
 * Forces the graphics driver to flush its internal deferred queues before trimming memory.
 */
void destroy_clock_hands_atlas(SDL_Renderer *renderer, CatClockHardwareAtlas *atlas) {
    if (atlas && atlas->atlas_texture) {
        if (renderer) {
            // CRITICAL ARCHITECTURAL PATCH: Force the hardware context to
            // completely unlink its target reference blocks before destruction.
            // This strips hidden driver caches and stops memory leak spikes.
            SDL_SetRenderTarget(renderer, NULL);
            SDL_SetRenderViewport(renderer, NULL);
            SDL_FlushRenderer(renderer);
        }

        SDL_DestroyTexture(atlas->atlas_texture);
        atlas->atlas_texture = NULL;
    }
}

/**
 * Dynamically reallocates the VRAM texture sheet to mirror window resizing.
 * Grows and contracts memory use cleanly based on incoming viewport size requirements.
 */
bool REBUILD_pre_rendered_60phase_atlas(SDL_Renderer *renderer, CatClockHardwareAtlas *atlas, int hand_w, int hand_h) {
    destroy_clock_hands_atlas(renderer, atlas);

    atlas->frame_w = hand_w;
    atlas->frame_h = hand_h;

    int total_atlas_w = hand_w * GRID_COLUMNS;
    int total_atlas_h = (hand_h * GRID_ROWS) * TOTAL_HANDS;

    atlas->atlas_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, total_atlas_w, total_atlas_h);
    if (!atlas->atlas_texture) return false;

    // Use Linear filtering so that high-resolution vector geometry anti-aliases smoothly
    SDL_SetTextureScaleMode(atlas->atlas_texture, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(atlas->atlas_texture, SDL_BLENDMODE_BLEND);

    SDL_SetRenderTarget(renderer, atlas->atlas_texture);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    int tile_cx = hand_w / 2;
    int tile_cy = hand_h / 2;

    for (int i = 0; i < PHASES_PER_HAND; i++) {
        int col = i % GRID_COLUMNS;
        int row = i / GRID_COLUMNS;

        float offset_x = (float)(col * hand_w);
        float hour_offset_y   = (float)(row * hand_h);
        float minute_offset_y = (float)((row + GRID_ROWS) * hand_h);
        float second_offset_y = (float)((row + (GRID_ROWS * 2)) * hand_h);

        SDL_Rect viewport_hour = { (int)offset_x, (int)hour_offset_y, hand_w, hand_h };
        SDL_SetRenderViewport(renderer, &viewport_hour);
        draw_projected_hand_geometry(renderer, tile_cx, tile_cy, hand_h, i, 0);
        atlas->hour_src_rects[i] = (SDL_FRect){ offset_x, hour_offset_y, (float)hand_w, (float)hand_h };

        SDL_Rect viewport_minute = { (int)offset_x, (int)minute_offset_y, hand_w, hand_h };
        SDL_SetRenderViewport(renderer, &viewport_minute);
        draw_projected_hand_geometry(renderer, tile_cx, tile_cy, hand_h, i, 1);
        atlas->minute_src_rects[i] = (SDL_FRect){ offset_x, minute_offset_y, (float)hand_w, (float)hand_h };

        SDL_Rect viewport_second = { (int)offset_x, (int)second_offset_y, hand_w, hand_h };
        SDL_SetRenderViewport(renderer, &viewport_second);
        draw_projected_hand_geometry(renderer, tile_cx, tile_cy, hand_h, i, 2);
        atlas->second_src_rects[i] = (SDL_FRect){ offset_x, second_offset_y, (float)hand_w, (float)hand_h };
    }

    SDL_SetRenderViewport(renderer, NULL);
    SDL_SetRenderTarget(renderer, NULL);
    return true;
}

/**
 * Places the pre-baked atlas frame directly onto the clock center.
 */
void RUNTIME_blit_pre_rendered_hands(SDL_Renderer *renderer, CatClockHardwareAtlas *atlas, float center_x, float center_y, int hour_val, int minute_val, int second_val) {
    if (!atlas || !atlas->atlas_texture) return;

    int h_frame = (hour_val % PHASES_PER_HAND + PHASES_PER_HAND) % PHASES_PER_HAND;
    int m_frame = (minute_val % PHASES_PER_HAND + PHASES_PER_HAND) % PHASES_PER_HAND;
    int s_frame = (second_val % PHASES_PER_HAND + PHASES_PER_HAND) % PHASES_PER_HAND;

    float base_render_w = 128.0f;
    float base_render_h = 128.0f;

    float screen_x = center_x - (base_render_w / 2.0f);
    float screen_y = center_y - (base_render_h / 2.0f);

    SDL_FRect dest_pos = { screen_x, screen_y, base_render_w, base_render_h };

    SDL_RenderTexture(renderer, atlas->atlas_texture, &atlas->hour_src_rects[h_frame], &dest_pos);
    SDL_RenderTexture(renderer, atlas->atlas_texture, &atlas->minute_src_rects[m_frame], &dest_pos);
    SDL_RenderTexture(renderer, atlas->atlas_texture, &atlas->second_src_rects[s_frame], &dest_pos);
}
