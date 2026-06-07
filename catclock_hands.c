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

#include "catclock_shared.h"

static SDL_Texture *hours_hand_atlas = NULL;
static SDL_Texture *minutes_hand_atlas = NULL;
static SDL_Texture *seconds_hand_atlas = NULL;

/*
 * RasterizeTriangularHandToTarget:
 * Computes pixel-solid scanline rows for the counterweighted triangles.
 */
static void RasterizeTriangularHandToTarget(SDL_Renderer *renderer, float angle, float length, float base_width, float target_cx, float target_cy) {
    float rad_tip = (angle - 90.0f) * M_PI / 180.0f;
    float rad_base_perpendicular = rad_tip + (M_PI / 2.0f);
    float tail_offset = length * 0.15f;
    float offset_cx = target_cx - tail_offset * cosf(rad_tip);
    float offset_cy = target_cy - tail_offset * sinf(rad_tip);

    float tip_x = target_cx + length * cosf(rad_tip);
    float tip_y = target_cy + length * sinf(rad_tip);
    float half_base = base_width / 2.0f;
    float base1_x = offset_cx - half_base * cosf(rad_base_perpendicular);
    float base1_y = offset_cy - half_base * sinf(rad_base_perpendicular);
    float base2_x = offset_cx + half_base * cosf(rad_base_perpendicular);
    float base2_y = offset_cy + half_base * sinf(rad_base_perpendicular);

    OriginalPoint t0 = { base1_x, base1_y };
    OriginalPoint t1 = { base2_x, base2_y };
    OriginalPoint t2 = { tip_x, tip_y };

    float min_y = t0.y; if (t1.y < min_y) min_y = t1.y; if (t2.y < min_y) min_y = t2.y;
    float max_y = t0.y; if (t1.y > max_y) max_y = t1.y; if (t2.y > max_y) max_y = t2.y;

    int start_y = (int)floorf(min_y), end_y = (int)ceilf(max_y);
    for (int y = start_y; y <= end_y; y++) {
        float inter0 = 0.0f, inter1 = 0.0f;
        int count = 0;

        if (((t0.y <= y) && (t1.y > y)) || ((t1.y <= y) && (t0.y > y))) {
            float t = (float)(y - t0.y) / (t1.y - t0.y);
            if (count == 0) inter0 = t0.x + t * (t1.x - t0.x);
            else inter1 = t0.x + t * (t1.x - t0.x);
            count++;
        }
        if (((t1.y <= y) && (t2.y > y)) || ((t2.y <= y) && (t1.y > y))) {
            float t = (float)(y - t1.y) / (t2.y - t1.y);
            if (count == 0) inter0 = t1.x + t * (t2.x - t1.x);
            else inter1 = t1.x + t * (t2.x - t1.x);
            count++;
        }
        if (((t2.y <= y) && (t0.y > y)) || ((t0.y <= y) && (t2.y > y))) {
            float t = (float)(y - t2.y) / (t0.y - t2.y);
            if (count == 0) inter0 = t2.x + t * (t0.x - t2.x);
            else inter1 = t2.x + t * (t0.x - t2.x);
            count++;
        }

        if (count >= 2) {
            float draw_min = inter0; float draw_max = inter1;
            if (draw_min > draw_max) { float tmp = draw_min; draw_min = draw_max; draw_max = tmp; }
            SDL_RenderLine(renderer, draw_min, (float)y, draw_max, (float)y);
        }
    }
    SDL_RenderLine(renderer, base1_x, base1_y, tip_x, tip_y);
    SDL_RenderLine(renderer, base2_x, base2_y, tip_x, tip_y);
    SDL_RenderLine(renderer, base1_x, base1_y, base2_x, base2_y);
}

/*
 * BakeClockHandsAtlases:
 * Renders all hand positions into VRAM strips exactly once on boot.
 */
void BakeClockHandsAtlases(SDL_Renderer *renderer) {
    hours_hand_atlas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, CLOCK_BOX_SIZE * 12, CLOCK_BOX_SIZE);
    minutes_hand_atlas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, CLOCK_BOX_SIZE * 60, CLOCK_BOX_SIZE);
    seconds_hand_atlas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, CLOCK_BOX_SIZE * 60, CLOCK_BOX_SIZE);

    SDL_SetTextureScaleMode(hours_hand_atlas, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureScaleMode(minutes_hand_atlas, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureScaleMode(seconds_hand_atlas, SDL_SCALEMODE_NEAREST);

    SDL_SetTextureBlendMode(hours_hand_atlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(minutes_hand_atlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(seconds_hand_atlas, SDL_BLENDMODE_BLEND);

    float t_cx = CLOCK_BOX_SIZE / 2.0f; float t_cy = CLOCK_BOX_SIZE / 2.0f;

    SDL_SetRenderTarget(renderer, hours_hand_atlas);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for (int h = 0; h < 12; h++) RasterizeTriangularHandToTarget(renderer, h * 30.0f, 17.5f, 9.5f, (h * CLOCK_BOX_SIZE) + t_cx, t_cy);

    SDL_SetRenderTarget(renderer, minutes_hand_atlas);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for (int m = 0; m < 60; m++) RasterizeTriangularHandToTarget(renderer, m * 6.0f, 27.5f, 8.5f, (m * CLOCK_BOX_SIZE) + t_cx, t_cy);

    SDL_SetRenderTarget(renderer, seconds_hand_atlas);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for (int s = 0; s < 60; s++) RasterizeTriangularHandToTarget(renderer, s * 6.0f, 30.0f, 1.5f, (s * CLOCK_BOX_SIZE) + t_cx, t_cy);

    SDL_SetRenderTarget(renderer, NULL);
}

void FreeClockHandsAtlases(void) {
    if (hours_hand_atlas) SDL_DestroyTexture(hours_hand_atlas);
    if (minutes_hand_atlas) SDL_DestroyTexture(minutes_hand_atlas);
    if (seconds_hand_atlas) SDL_DestroyTexture(seconds_hand_atlas);
}

void DrawBakedClockHand(SDL_Renderer *renderer, int hand_type, int position_index) {
    SDL_Texture *tex = (hand_type == 0) ? hours_hand_atlas : (hand_type == 1) ? minutes_hand_atlas : seconds_hand_atlas;
    if (!tex) return;
    SDL_FRect src = { (float)(position_index * CLOCK_BOX_SIZE), 0, CLOCK_BOX_SIZE, CLOCK_BOX_SIZE };
    SDL_FRect dst = { CLOCK_CENTER_X - (CLOCK_BOX_SIZE / 2.0f), CLOCK_CENTER_Y - (CLOCK_BOX_SIZE / 2.0f), CLOCK_BOX_SIZE, CLOCK_BOX_SIZE };
    SDL_RenderTexture(renderer, tex, &src, &dst);
}
