/* =============================================================================
   FILE: catclock_hands.c (Stateless Aspect-Compensated Geometry Hands Shader)
   ============================================================================= */
#include "catclock_shared.h"
#include <math.h>
#include <stdlib.h>

typedef struct {
    int hand_type;
    SDL_Color color;
} HandShaderConfig;

void CatClock_ShaderHands(SDL_Renderer *renderer, int cell_w, int cell_h, float scale, int frame_idx, void *userdata) {
    HandShaderConfig *cfg = (HandShaderConfig *)userdata;
    if (!cfg) return;

    float local_cx = (float)cell_w / 2.0f;
    float local_cy = (float)cell_h / 2.0f;

    /* 0 to 60 uniform circular layout sweep */
    float angle_rad = ((float)frame_idx / 60.0f) * 2.0f * (float)M_PI;

    /* Extract lengths based on hand types using the texture base width */
    float base_length = 0.0f;
    float thickness = 0.0f;
    float pivot_back = 0.0f;

    if (cfg->hand_type == HAND_TYPE_HOUR) {
        base_length = (float)cell_w * 0.28f;
        thickness   = 7.5f * scale;
        pivot_back  = 4.0f * scale;
    }
    else if (cfg->hand_type == HAND_TYPE_MINUTE) {
        base_length = (float)cell_w * 0.42f;
        thickness   = 5.0f * scale;
        pivot_back  = 5.0f * scale;
    }
    else if (cfg->hand_type == HAND_TYPE_SECOND) {
        base_length = (float)cell_w * 0.46f;
        thickness   = 1.5f * scale;
        pivot_back  = 6.0f * scale;
    }

    /* 1. Calculate standard circular direction vectors */
    float raw_fx = sinf(angle_rad);
    float raw_fy = -cosf(angle_rad);

    /* 2. ACTUALLY USE CELL_H: Distort the vertical vectors by the 1.5x aspect ratio (96 / 64) */
    float aspect_inv = (float)cell_h / (float)cell_w;
    float fx = raw_fx;
    float fy = raw_fy * aspect_inv;

    /* 3. Re-project the rendering angle to prevent quadrant angular warping */
    float projected_angle = atan2f(fx, -fy);
    float cos_a = cosf(projected_angle);
    float sin_a = sinf(projected_angle);

    /* 4. Determine final elliptical scaled dimensions */
    float radial_scale = sqrtf(fx * fx + fy * fy);
    float final_length = base_length * radial_scale;
    float final_pivot  = pivot_back * radial_scale;

    /* 5. Draw the geometric triangle */
    float tip_x = local_cx + (final_length * sin_a);
    float tip_y = local_cy - (final_length * cos_a);

    float half_thick = thickness / 2.0f;
    float base_left_x = local_cx - (half_thick * cos_a) - (final_pivot * sin_a);
    float base_left_y = local_cy - (half_thick * sin_a) + (final_pivot * cos_a);
    float base_right_x = local_cx + (half_thick * cos_a) - (final_pivot * sin_a);
    float base_right_y = local_cy + (half_thick * sin_a) + (final_pivot * cos_a);

    SDL_Vertex vertices[3];
    SDL_FColor fcol = { cfg->color.r / 255.0f, cfg->color.g / 255.0f, cfg->color.b / 255.0f, cfg->color.a / 255.0f };

    vertices[0] = (SDL_Vertex){ { tip_x, tip_y }, fcol, { 0.0f, 0.0f } };
    vertices[1] = (SDL_Vertex){ { base_left_x, base_left_y }, fcol, { 0.0f, 0.0f } };
    vertices[2] = (SDL_Vertex){ { base_right_x, base_right_y }, fcol, { 0.0f, 0.0f } };

    SDL_RenderGeometry(renderer, NULL, vertices, 3, NULL, 0);
}
