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

#include "catclock_atlas.h"
#include "catclock_shared.h"
#include <stddef.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Generalized instance array managing hand pipeline memory layers */
static AtlasPipelineLayer g_HandPipelineLayers[3] = {
    { NULL, 60, 64, 64 }, /* FIX: Restored to 60 total frames for smooth fractional hour phases */
    { NULL, 60, 64, 64 }, /* Minute Hand Sheet Spec */
    { NULL, 60, 64, 64 }  /* Second Hand Sheet Spec */
};

static float g_CachedPipelineScaleX = -1.0f;
static float g_CachedPipelineScaleY = -1.0f;
static int g_PipelineAtlasesInitialized = 0;

void ResetAtlasPipelineScales(void) {
    g_CachedPipelineScaleX = -1.0f;
    g_CachedPipelineScaleY = -1.0f;
    g_PipelineAtlasesInitialized = 0;
}

void DestroyHandTextureAtlases(void) {
    for (int i = 0; i < 3; i++) {
        if (g_HandPipelineLayers[i].texture) {
            SDL_DestroyTexture(g_HandPipelineLayers[i].texture);
            g_HandPipelineLayers[i].texture = NULL;
        }
    }
    ResetAtlasPipelineScales();
}

void InitHandTextureAtlases(SDL_Renderer *renderer) {
    int pixel_w = 0, pixel_h = 0;
    SDL_GetRenderOutputSize(renderer, &pixel_w, &pixel_h);

    float actual_scale_x = (float)pixel_w / 150.0f;
    float actual_scale_y = (float)pixel_h / 250.0f;
    if (actual_scale_x <= 0.0f) actual_scale_x = 1.0f;
    if (actual_scale_y <= 0.0f) actual_scale_y = 1.0f;

    g_CachedPipelineScaleX = actual_scale_x;
    g_CachedPipelineScaleY = actual_scale_y;

    for (int hand_type = 0; hand_type < 3; hand_type++) {
        AtlasPipelineLayer *layer = &g_HandPipelineLayers[hand_type];

        int scaled_slot_w = (int)ceilf(layer->base_cell_w * g_CachedPipelineScaleX);
        int scaled_slot_h = (int)ceilf(layer->base_cell_h * g_CachedPipelineScaleY);
        int total_sheet_w = scaled_slot_w * layer->total_frames;
        int total_sheet_h = scaled_slot_h;

        SDL_Surface *surface = SDL_CreateSurface(total_sheet_w, total_sheet_h, SDL_PIXELFORMAT_RGBA8888);
        if (!surface) continue;

        SDL_ClearSurface(surface, 0.0f, 0.0f, 0.0f, 0.0f);
        SDL_Renderer *sw_renderer = SDL_CreateSoftwareRenderer(surface);
        if (!sw_renderer) {
            SDL_DestroySurface(surface);
            continue;
        }

        float tip_len = 0.0f, base_width = 0.0f, back_ext = 0.0f;
        if (hand_type == HAND_TYPE_HOUR) {
            tip_len = 18.0f; base_width = 10.0f; back_ext = -6.0f;
        } else if (hand_type == HAND_TYPE_MINUTE) {
            tip_len = 28.0f; base_width = 7.5f; back_ext = -6.0f;
        } else {
            tip_len = 27.0f; base_width = 3.0f; back_ext = -12.0f;
        }

        float half_width = base_width / 2.0f;
        int steps_count = layer->total_frames;

        for (int step = 0; step < steps_count; step++) {
            float angle_rad = ((float)step * (2.0f * M_PI / (float)steps_count)) - (M_PI / 2.0f);
            float cos_a = cosf(angle_rad);
            float sin_a = sinf(angle_rad);

            float cx = ((float)step * scaled_slot_w) + (scaled_slot_w / 2.0f);
            float cy = scaled_slot_h / 2.0f;

            /* Elliptical projection math for the prolate spheroid clock face layout */
            float vector_len = sqrtf(cos_a * cos_a * g_CachedPipelineScaleX * g_CachedPipelineScaleX +
                                     sin_a * sin_a * g_CachedPipelineScaleY * g_CachedPipelineScaleY);
            float true_cos = (cos_a * g_CachedPipelineScaleX) / vector_len;
            float true_sin = (sin_a * g_CachedPipelineScaleY) / vector_len;

            float tx = cx + (tip_len * true_cos * g_CachedPipelineScaleX);
            float ty = cy + (tip_len * true_sin * g_CachedPipelineScaleY);
            float bx = cx + (back_ext * true_cos * g_CachedPipelineScaleX);
            float by = cy + (back_ext * true_sin * g_CachedPipelineScaleY);

            /* Perfect, non-sheared isosceles base alignment calculations */
            float dx = -true_sin * half_width * g_CachedPipelineScaleX;
            float dy = true_cos * half_width * g_CachedPipelineScaleY;

            SDL_Vertex vertices[3];

            /* Vertex 0: Tip (Solid Black Vector Fill) */
            vertices[0].position.x = tx; vertices[0].position.y = ty;
            vertices[0].color.r = 0.0f; vertices[0].color.g = 0.0f; vertices[0].color.b = 0.0f; vertices[0].color.a = 1.0f;

            /* Vertex 1: Base Left Corner Vertex */
            vertices[1].position.x = bx - dx; vertices[1].position.y = by - dy;
            vertices[1].color.r = 0.0f; vertices[1].color.g = 0.0f; vertices[1].color.b = 0.0f; vertices[1].color.a = 1.0f;

            /* Vertex 2: Base Right Corner Vertex */
            vertices[2].position.x = bx + dx; vertices[2].position.y = by + dy;
            vertices[2].color.r = 0.0f; vertices[2].color.g = 0.0f; vertices[2].color.b = 0.0f; vertices[2].color.a = 1.0f;

            SDL_RenderGeometry(sw_renderer, NULL, vertices, 3, NULL, 0);
        }

        SDL_DestroyRenderer(sw_renderer);
        layer->texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_DestroySurface(surface);
    }
    g_PipelineAtlasesInitialized = 1;
}

void RenderBakedClockHandToPipeline(SDL_Renderer *renderer, int hand_type, int position_index) {
    if (hand_type < 0 || hand_type > 2) return;

    int pixel_w = 0, pixel_h = 0;
    SDL_GetRenderOutputSize(renderer, &pixel_w, &pixel_h);
    float currentScaleX = (float)pixel_w / 150.0f;
    float currentScaleY = (float)pixel_h / 250.0f;

    if (!g_PipelineAtlasesInitialized || currentScaleX != g_CachedPipelineScaleX || currentScaleY != g_CachedPipelineScaleY) {
        DestroyHandTextureAtlases();
        InitHandTextureAtlases(renderer);
    }

    AtlasPipelineLayer *layer = &g_HandPipelineLayers[hand_type];
    if (!layer->texture) return;

    int scaled_slot_w = (int)ceilf(layer->base_cell_w * g_CachedPipelineScaleX);
    int scaled_slot_h = (int)ceilf(layer->base_cell_h * g_CachedPipelineScaleY);

    int step_index = position_index;
    if (step_index >= layer->total_frames) step_index = layer->total_frames - 1;
    if (step_index < 0) step_index = 0;

    SDL_FRect src_rect;
    src_rect.x = (float)(step_index * scaled_slot_w);
    src_rect.y = 0.0f;
    src_rect.w = (float)scaled_slot_w;
    src_rect.h = (float)scaled_slot_h;

    SDL_FRect dst_rect;
    dst_rect.x = 73.0f - ((float)layer->base_cell_w / 2.0f);
    dst_rect.y = 150.0f - ((float)layer->base_cell_h / 2.0f);
    dst_rect.w = (float)layer->base_cell_w;
    dst_rect.h = (float)layer->base_cell_h;

    SDL_RenderTexture(renderer, layer->texture, &src_rect, &dst_rect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
}

void DrawBakedClockHand(SDL_Renderer *renderer, int hand_type, int position_index) {
    int step_index = position_index;

    if (hand_type == HAND_TYPE_HOUR) {
        time_t raw_time = time(NULL);
        struct tm *live_time = localtime(&raw_time);
        int current_minute = (live_time) ? live_time->tm_min : 0;

        /* Standard 12-hour translation mapping + minute fraction step positioning */
        step_index = ((position_index % 12) * 5) + (current_minute / 12);
    }

    RenderBakedClockHandToPipeline(renderer, hand_type, step_index);
}

/* Framework Main Loop Linker Mappings */
void BakeClockHandsAtlases(SDL_Renderer *renderer) {
    InitHandTextureAtlases(renderer);
}

void FreeClockHandsAtlases(void) {
    DestroyHandTextureAtlases();
}
