/* =============================================================================
   FILE: catclock_tail.c (Stateless Polygon Pendulum Matrix Shader)
   ============================================================================= */
#include "catclock_shared.h"
#include <math.h>
#include <stdlib.h>

static OriginalPoint original_repository_tail[] = {
    { -9.0f, -10.0f }, { -9.0f, 20.0f }, {-10.5f, 40.0f }, {-11.5f, 55.0f },
    { -9.0f, 68.0f },  { 0.0f, 78.0f },  { 11.0f, 76.0f },  { 18.0f, 68.0f },
    { 20.0f, 54.0f },  { 16.5f, 44.0f }, { 9.0f, 42.0f },   { 4.5f, 50.0f },
    { 8.5f, 62.0f },   { 3.0f, 68.0f },  { -2.0f, 62.0f },  { -1.0f, 45.0f },
    { -4.5f, 32.0f },  { -6.0f, 15.0f }, { -6.0f, -10.0f }
};

static const int TOTAL_TAIL_POINTS = sizeof(original_repository_tail) / sizeof(original_repository_tail[0]);

static void LocalDrawHardwareTailPolygon(SDL_Renderer *renderer, float cx, float cy, float angle_deg, float scale, SDL_Color color) {
    float rad = (angle_deg * (float)M_PI) / 180.0f;
    float cos_a = cosf(rad);
    float sin_a = sinf(rad);

    SDL_Vertex *vertices = (SDL_Vertex *)SDL_malloc(sizeof(SDL_Vertex) * TOTAL_TAIL_POINTS);
    if (!vertices) return;

    SDL_FColor fcol = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };

    for (int i = 0; i < TOTAL_TAIL_POINTS; i++) {
        float tx = original_repository_tail[i].x * scale;
        float ty = original_repository_tail[i].y * scale;

        float rot_x = tx * cos_a - ty * sin_a;
        float rot_y = tx * sin_a + ty * cos_a;

        vertices[i] = (SDL_Vertex){ { cx + rot_x, cy + rot_y }, fcol, { 0.0f, 0.0f } };
    }

    int total_triangles = TOTAL_TAIL_POINTS - 2;
    int *indices = (int *)SDL_malloc(sizeof(int) * total_triangles * 3);
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

void CatClock_ShaderTail(SDL_Renderer *renderer, int cell_w, int cell_h, float scale, int frame_idx, void *userdata) {
    (void)cell_h;

    SDL_Color *fg_color = (SDL_Color *)userdata;
    SDL_Color color = fg_color ? *fg_color : (SDL_Color){ 0, 0, 0, 255 };

    int req_frames = target_fps_limit <= 0 ? 60 : target_fps_limit;
    float phase_ratio = (float)frame_idx / (float)req_frames;
    float current_angle = 19.5f * sinf(phase_ratio * 2.0f * (float)M_PI);

    float pivot_x = (float)cell_w / 2.0f;
    float pivot_y = 12.0f * scale;

    LocalDrawHardwareTailPolygon(renderer, pivot_x, pivot_y, current_angle, scale, color);
}
