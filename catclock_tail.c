/* ==========================================================================
   FILE: catclock_tail.c (Ultra-Low Overhead Vector Outline Engine)
   ========================================================================== */
#include "catclock_shared.h"
#include <math.h>
#include <stdlib.h>

static OriginalPoint original_repository_tail[] = {
    { -9.0f, -10.0f }, { -9.0f, 20.0f }, {-10.5f, 40.0f }, {-11.5f, 55.0f },
    { -9.0f, 68.0f }, { 0.0f, 78.0f }, { 11.0f, 76.0f }, { 18.0f, 68.0f },
    { 20.0f, 54.0f }, { 16.5f, 44.0f }, { 9.0f, 42.0f }, { 4.5f, 50.0f },
    { 8.5f, 62.0f }, { 3.0f, 68.0f }, { -2.0f, 62.0f }, { -1.0f, 45.0f },
    { 1.0f, 30.0f }, { 4.5f, 15.0f }, { 7.0f, -10.0f }
};

#define LOCAL_TAIL_COUNT ((int)(sizeof(original_repository_tail) / sizeof(original_repository_tail[0])))

static void RasterizeTailPolygon(SDL_Renderer *renderer, OriginalPoint *mesh, float cx, float cy)
{
    float min_y = 999.0f, max_y = -999.0f;
    for (int i = 0; i < LOCAL_TAIL_COUNT; i++)
    {
        if (mesh[i].y < min_y) min_y = mesh[i].y;
        if (mesh[i].y > max_y) max_y = mesh[i].y;
    }

    int start_scan_y = (int)floorf(min_y);
    int end_scan_y   = (int)ceilf(max_y);

    for (int y = start_scan_y; y <= end_scan_y; y++)
    {
        float intersections[LOCAL_TAIL_COUNT];
        int intersections_count = 0;

        for (int i = 0; i < LOCAL_TAIL_COUNT; i++)
        {
            int next_i = (i + 1) % LOCAL_TAIL_COUNT;
            float x1 = mesh[i].x; float y1 = mesh[i].y;
            float x2 = mesh[next_i].x; float y2 = mesh[next_i].y;

            if (((y1 <= y) && (y2 > y)) || ((y2 <= y) && (y1 > y)))
            {
                float t = (float)(y - y1) / (y2 - y1);
                intersections[intersections_count++] = x1 + t * (x2 - x1);
            }
        }

        qsort(intersections, intersections_count, sizeof(float), CompareFloats);

        for (int i = 0; i < intersections_count - 1; i += 2)
        {
            SDL_RenderLine(renderer, cx + intersections[i], cy + (float)y, cx + intersections[i+1], cy + (float)y);
        }
    }
}

void RenderOriginalThickSwayingTail(SDL_Renderer *renderer, float cx, float cy, float angle_deg, SDL_Color color, bool inflate_mode)
{
    float rad = (angle_deg * 4.375f) * (float)M_PI / 180.0f;
    float cos_a = cosf(rad);
    float sin_a = sinf(rad);

    OriginalPoint rotated_mesh[LOCAL_TAIL_COUNT];

    for (int i = 0; i < LOCAL_TAIL_COUNT; i++)
    {
        float rx = original_repository_tail[i].x;
        float ry = original_repository_tail[i].y;

        if (inflate_mode)
        {
            int prev = (i - 1 + LOCAL_TAIL_COUNT) % LOCAL_TAIL_COUNT;
            int next = (i + 1) % LOCAL_TAIL_COUNT;

            float dx1 = original_repository_tail[i].x - original_repository_tail[prev].x;
            float dy1 = original_repository_tail[i].y - original_repository_tail[prev].y;
            float len1 = sqrtf(dx1*dx1 + dy1*dy1);
            if (len1 > 0.0f) { dx1 /= len1; dy1 /= len1; }

            float dx2 = original_repository_tail[next].x - original_repository_tail[i].x;
            float dy2 = original_repository_tail[next].y - original_repository_tail[i].y;
            float len2 = sqrtf(dx2*dx2 + dy2*dy2);
            if (len2 > 0.0f) { dx2 /= len2; dy2 /= len2; }

            float nx = -(dy1 + dy2);
            float ny = (dx1 + dx2);
            float n_len = sqrtf(nx*nx + ny*ny);
            if (n_len > 0.0f)
            {
                rx += (nx / n_len) * 1.2f;
                ry += (ny / n_len) * 1.2f;
            }
        }

        rotated_mesh[i].x = rx * cos_a - ry * sin_a;
        rotated_mesh[i].y = rx * sin_a + ry * cos_a;

        float depth_factor = (ry + 10.0f) / 95.0f;
        float skew_factor = depth_factor * depth_factor * depth_factor;
        rotated_mesh[i].x += 32.0f * sin_a * skew_factor;
    }

    /*
       Optimization Fix:
       If drawing fg_color (black core), trace a simple 1px thick wireframe border
       using white lines instead of re-running the heavy scanline loop 8 times.
       This drops CPU utilization back down to your native ~1.5% range.
    */
    if (color.r == 0 && color.g == 0 && color.b == 0)
    {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (int i = 0; i < LOCAL_TAIL_COUNT; i++)
        {
            int next_i = (i + 1) % LOCAL_TAIL_COUNT;

            /* Thicken the boundary lines by drawing a minimal outer shell wireframe trace */
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) continue;
                    SDL_RenderLine(renderer,
                        cx + rotated_mesh[i].x + (float)dx, cy + rotated_mesh[i].y + (float)dy,
                        cx + rotated_mesh[next_i].x + (float)dx, cy + rotated_mesh[next_i].y + (float)dy
                    );
                }
            }
        }
    }

    /* Draw the final core polygon mesh shape over the wireframe trace footprint */
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    RasterizeTailPolygon(renderer, rotated_mesh, cx, cy);
}
