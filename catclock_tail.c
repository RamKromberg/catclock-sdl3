/******************************************************************************
 * File Name:    catclock_tail.c
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

#define TAIL_VERTICES_COUNT 19

static OriginalPoint original_repository_tail[] = {
    { -9.0f, -10.0f }, { -9.0f,  20.0f }, {-10.5f,  40.0f }, {-11.5f,  55.0f }, 
    { -9.0f,  68.0f }, {  0.0f,  78.0f }, { 11.0f,  76.0f }, { 18.0f,  68.0f }, 
    { 20.0f,  54.0f }, { 16.5f,  44.0f }, {  9.0f,  42.0f }, {  4.5f,  50.0f }, 
    {  8.5f,  62.0f }, {  3.0f,  68.0f }, { -2.0f,  62.0f }, { -1.0f,  45.0f }, 
    {  1.0f,  30.0f }, {  4.5f,  15.0f }, {  7.0f, -10.0f }
};

void RenderOriginalThickSwayingTail(SDL_Renderer *renderer, float cx, float cy, float angle_deg, SDL_Color color, bool inflate_mode) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
    float rad = angle_deg * M_PI / 180.0f;
    float cos_a = cosf(rad);
    float sin_a = sinf(rad);

    OriginalPoint rotated_mesh[TAIL_VERTICES_COUNT];
    float min_y = 999.0f, max_y = -999.0f;

    for (int i = 0; i < TAIL_VERTICES_COUNT; i++) {
        float rx = original_repository_tail[i].x;
        float ry = original_repository_tail[i].y;

        /* 
         * NORMAL VECTOR GEOMETRY INFLATION ENGINE:
         * Calculates true perpendicular vectors for every segment joint. 
         * This forces the 1px white border to expand outward on concave curves,
         * resolving the missing inner-edge outline glitch at the tip.
         */
        if (inflate_mode) {
            int prev = (i - 1 + TAIL_VERTICES_COUNT) % TAIL_VERTICES_COUNT;
            int next = (i + 1) % TAIL_VERTICES_COUNT;

            /* Vector segment A (Incoming) */
            float dx1 = original_repository_tail[i].x - original_repository_tail[prev].x;
            float dy1 = original_repository_tail[i].y - original_repository_tail[prev].y;
            float len1 = sqrtf(dx1*dx1 + dy1*dy1);
            if (len1 > 0.0f) { dx1 /= len1; dy1 /= len1; }

            /* Vector segment B (Outgoing) */
            float dx2 = original_repository_tail[next].x - original_repository_tail[i].x;
            float dy2 = original_repository_tail[next].y - original_repository_tail[i].y;
            float len2 = sqrtf(dx2*dx2 + dy2*dy2);
            if (len2 > 0.0f) { dx2 /= len2; dy2 /= len2; }

            /* Averaged vertex normal vector calculations */
            float nx = -(dy1 + dy2);
            float ny = (dx1 + dx2);
            float n_len = sqrtf(nx*nx + ny*ny);
            
            if (n_len > 0.0f) {
                /* Push the coordinates outward relative to the curve profile by exactly 1.2px */
                rx += (nx / n_len) * 1.2f;
                ry += (ny / n_len) * 1.2f;
            }
        }

        rotated_mesh[i].x = rx * cos_a - ry * sin_a;
        rotated_mesh[i].y = rx * sin_a + ry * cos_a;

        float depth_factor = (ry + 10.0f) / 95.0f;
        float skew_factor = depth_factor * depth_factor * depth_factor;
        rotated_mesh[i].x += 32.0f * sin_a * skew_factor;

        if (rotated_mesh[i].y < min_y) min_y = rotated_mesh[i].y;
        if (rotated_mesh[i].y > max_y) max_y = rotated_mesh[i].y;
    }

    int start_scan_y = (int)floorf(min_y);
    int end_scan_y   = (int)ceilf(max_y);

    for (int y = start_scan_y; y <= end_scan_y; y++) {
        float intersections[TAIL_VERTICES_COUNT];
        int intersections_count = 0;

        for (int i = 0; i < TAIL_VERTICES_COUNT; i++) {
            int next_i = (i + 1) % TAIL_VERTICES_COUNT;
            float x1 = rotated_mesh[i].x; float y1 = rotated_mesh[i].y;
            float x2 = rotated_mesh[next_i].x; float y2 = rotated_mesh[next_i].y;

            if (((y1 <= y) && (y2 > y)) || ((y2 <= y) && (y1 > y))) {
                float t = (float)(y - y1) / (y2 - y1);
                intersections[intersections_count++] = x1 + t * (x2 - x1);
            }
        }
        qsort(intersections, intersections_count, sizeof(float), CompareFloats);
        for (int i = 0; i < intersections_count - 1; i += 2) {
            SDL_RenderLine(renderer, cx + intersections[i], cy + (float)y, cx + intersections[i+1], cy + (float)y);
        }
    }
}
