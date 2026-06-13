/* =============================================================================
   FILE: catclock_eyes.c (Stateless Mathematical Pupil Oval Shader)
   ============================================================================= */
#include "catclock_shared.h"
#include <math.h>
#include <stdlib.h>

static void LocalDrawHardwarePupilOval(SDL_Renderer *renderer, float cx, float cy, float r_base_w, float r_base_h, float look_x, float max_offset_x, SDL_Color color) {
    float ox = look_x * max_offset_x;
    float oy = 0.0f;

    float compression = sqrtf(fmaxf(0.1f, 1.0f - (look_x * look_x * 0.45f)));
    float pup_w = r_base_w * compression;
    float pup_h = r_base_h;

    int segments = 24;
    int total_vertices = segments + 1;
    SDL_Vertex *vertices = (SDL_Vertex *)SDL_malloc(sizeof(SDL_Vertex) * total_vertices);
    int *indices = (int *)SDL_malloc(sizeof(int) * segments * 3);
    if (!vertices || !indices) {
        if (vertices) SDL_free(vertices);
        if (indices) SDL_free(indices);
        return;
    }

    SDL_FColor fcol = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };
    vertices[0] = (SDL_Vertex){ { cx + ox, cy + oy }, fcol, { 0.0f, 0.0f } };

    for (int i = 0; i < segments; i++) {
        float angle = ((float)i / (float)segments) * 2.0f * (float)M_PI;
        float vx = (cx + ox) + (pup_w * cosf(angle));
        float vy = (cy + oy) + (pup_h * sinf(angle));
        vertices[i + 1] = (SDL_Vertex){ { vx, vy }, fcol, { 0.0f, 0.0f } };

        indices[i * 3]     = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = (i == segments - 1) ? 1 : i + 2;
    }

    SDL_RenderGeometry(renderer, NULL, vertices, total_vertices, indices, segments * 3);
    SDL_free(vertices);
    SDL_free(indices);
}

void CatClock_ShaderEyes(SDL_Renderer *renderer, int cell_w, int cell_h, float scale, int frame_idx, void *userdata) {
    (void)userdata;
    int req_frames = target_fps_limit <= 0 ? 60 : target_fps_limit;

    float phase_ratio = (float)frame_idx / (float)req_frames;
    float swing_angle = phase_ratio * 2.0f * (float)M_PI;

    float base_w = 2.5f * scale;
    float base_h = 10.5f * scale;
    float max_offset_x = 7.0f * scale;
    SDL_Color pure_white = { 255, 255, 255, 255 };

    LocalDrawHardwarePupilOval(renderer, (float)cell_w / 2.0f, (float)cell_h / 2.0f, base_w, base_h, sinf(swing_angle), max_offset_x, pure_white);
}
