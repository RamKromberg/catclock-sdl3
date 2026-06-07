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

#include "catclock_shared.h"

#define RESOLUTION_VERTICES 32

void RenderAuthenticOriginalEyes(SDL_Renderer *renderer, float translation_x, float scale_factor, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    float target_centers[] = { LEFT_EYE_AXIS_X, RIGHT_EYE_AXIS_X };

    float base_radius_x = 3.5f;
    float base_radius_y = 6.0f;

    for (int eye = 0; eye < 2; eye++) {
        float cx = target_centers[eye];
        float cy = EYE_AXIS_Y;

        OriginalPoint transformed_vertices[RESOLUTION_VERTICES];
        float min_y = 999.0f;
        float max_y = -999.0f;

        for (int i = 0; i < RESOLUTION_VERTICES; i++) {
            float angle = ((float)i / (float)RESOLUTION_VERTICES) * 2.0f * M_PI;

            float relative_x = base_radius_x * cosf(angle);
            float relative_y = base_radius_y * sinf(angle);

            transformed_vertices[i].x = cx + (relative_x * scale_factor) + translation_x;
            transformed_vertices[i].y = cy + (relative_y * 1.75f);

            if (transformed_vertices[i].y < min_y) min_y = transformed_vertices[i].y;
            if (transformed_vertices[i].y > max_y) max_y = transformed_vertices[i].y;
        }

        int start_y = (int)floorf(min_y);
        int end_y   = (int)ceilf(max_y);

        for (int y = start_y; y <= end_y; y++) {
            float intersections[RESOLUTION_VERTICES];
            int intersections_count = 0;

            for (int i = 0; i < RESOLUTION_VERTICES; i++) {
                int next_i = (i + 1) % RESOLUTION_VERTICES;
                float x1 = transformed_vertices[i].x;
                float y1 = transformed_vertices[i].y;
                float x2 = transformed_vertices[next_i].x;
                float y2 = transformed_vertices[next_i].y;

                if (((y1 <= y) && (y2 > y)) || ((y2 <= y) && (y1 > y))) {
                    float t = (float)(y - y1) / (y2 - y1);
                    intersections[intersections_count++] = x1 + t * (x2 - x1);
                }
            }

            qsort(intersections, intersections_count, sizeof(float), CompareFloats);

            for (int i = 0; i < intersections_count - 1; i += 2) {
                SDL_RenderLine(renderer, intersections[i], (float)y, intersections[i+1], (float)y);
            }
        }
    }
}
