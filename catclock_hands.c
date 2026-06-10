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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern int ssaa_factor;

typedef struct {
    SDL_Texture *atlas_texture;
    SDL_FRect *hour_src_rects;
    SDL_FRect *second_src_rects;
    int total_frames;
    int frame_width;
    int frame_height;
} PreFlippedAtlas;

SDL_Texture *g_hands_atlas = NULL;
static SDL_Texture *g_hour_tex = NULL;
static SDL_Texture *g_minute_tex = NULL;
static SDL_Texture *g_second_tex = NULL;

static int cached_win_w = 0;
static int cached_win_h = 0;
static int cached_ssaa_factor = 0;

void destroy_clock_hands_atlas(void) {
    if (g_hour_tex != NULL) { SDL_DestroyTexture(g_hour_tex); g_hour_tex = NULL; }
    if (g_minute_tex != NULL) { SDL_DestroyTexture(g_minute_tex); g_minute_tex = NULL; }
    if (g_second_tex != NULL) { SDL_DestroyTexture(g_second_tex); g_second_tex = NULL; }
    g_hands_atlas = NULL;
    cached_win_w = 0;
    cached_win_h = 0;
    cached_ssaa_factor = 0;
}

bool REBUILD_pre_rendered_60phase_atlas(SDL_Renderer *renderer, PreFlippedAtlas *atlas, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    (void)r; (void)g; (void)b; (void)a;
    if (!atlas) {
        return false;
    }

    int win_w = 0, win_h = 0;
    if (!SDL_GetRenderOutputSize(renderer, &win_w, &win_h)) {
        return false;
    }

    int current_ssaa = (ssaa_factor > 0) ? ssaa_factor : 1;

    if (g_hour_tex != NULL && g_minute_tex != NULL && g_second_tex != NULL &&
        win_w == cached_win_w &&
        win_h == cached_win_h &&
        current_ssaa == cached_ssaa_factor) {
        g_hands_atlas = g_hour_tex;
        atlas->atlas_texture = g_hour_tex;
        return true;
    }

    destroy_clock_hands_atlas();

    int ssaa_win_w = win_w * current_ssaa;
    int ssaa_win_h = win_h * current_ssaa;

    atlas->total_frames = 60;
    atlas->frame_width = ssaa_win_w;
    atlas->frame_height = ssaa_win_h;

    int grid_w = ssaa_win_w * 8;
    int grid_h = ssaa_win_h * 8;

    g_hour_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, grid_w, grid_h);
    g_minute_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, grid_w, grid_h);
    g_second_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, grid_w, grid_h);

    if (!g_hour_tex || !g_minute_tex || !g_second_tex) {
        destroy_clock_hands_atlas();
        return false;
    }

    g_hands_atlas = g_hour_tex;
    atlas->atlas_texture = g_hour_tex;

    SDL_SetTextureScaleMode(g_hour_tex, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(g_hour_tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(g_minute_tex, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(g_minute_tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(g_second_tex, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(g_second_tex, SDL_BLENDMODE_BLEND);

    SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);

    SDL_SetRenderTarget(renderer, g_hour_tex); SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, g_minute_tex); SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, g_second_tex); SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); SDL_RenderClear(renderer);

    float center_x_base = 73.0f;
    float center_y_base = 150.0f;

    float scale_x = (float)ssaa_win_w / 150.0f;
    float scale_y = (float)ssaa_win_h / 300.0f;

    for (int i = 0; i < 60; ++i) {
        int col = i % 8;
        int row = i / 8;
        float slot_offset_x = (float)(col * ssaa_win_w);
        float slot_offset_y = (float)(row * ssaa_win_h);

        float cx = (center_x_base * scale_x) + slot_offset_x;
        float cy = (center_y_base * scale_y) + slot_offset_y;

        double angle = (i / 60.0) * 2.0 * M_PI;
        float cos_a = (float)cos(angle);
        float sin_a = (float)sin(angle);

        SDL_SetRenderTarget(renderer, g_hour_tex);
        float h_w = 4.5f;
        float h_back = 5.0f;
        float h_front = 18.0f;

        float h_dir_x = (sin_a * h_front) * scale_x;
        float h_dir_y = (-cos_a * h_front) * scale_y;
        float h_side_x = (cos_a * h_w) * scale_x;
        float h_side_y = (sin_a * h_w) * scale_y;
        float h_tail_x = (-sin_a * h_back) * scale_x;
        float h_tail_y = (cos_a * h_back) * scale_y;

        SDL_Vertex h_verts[3];
        h_verts[0].position.x = cx + h_dir_x;
        h_verts[0].position.y = cy + h_dir_y;
        h_verts[0].color = (SDL_FColor){0.0f, 0.0f, 0.0f, 1.0f};

        h_verts[1].position.x = cx + h_tail_x + h_side_x;
        h_verts[1].position.y = cy + h_tail_y + h_side_y;
        h_verts[1].color = (SDL_FColor){0.0f, 0.0f, 0.0f, 1.0f};

        h_verts[2].position.x = cx + h_tail_x - h_side_x;
        h_verts[2].position.y = cy + h_tail_y - h_side_y;
        h_verts[2].color = (SDL_FColor){0.0f, 0.0f, 0.0f, 1.0f};
        SDL_RenderGeometry(renderer, NULL, h_verts, 3, NULL, 0);

        if (atlas->hour_src_rects) {
            atlas->hour_src_rects[i].x = slot_offset_x;
            atlas->hour_src_rects[i].y = slot_offset_y;
            atlas->hour_src_rects[i].w = (float)ssaa_win_w;
            atlas->hour_src_rects[i].h = (float)ssaa_win_h;
        }

        SDL_SetRenderTarget(renderer, g_minute_tex);
        float m_w = 3.5f;
        float m_back = 6.0f;
        float m_front = 28.0f;

        float m_dir_x = (sin_a * m_front) * scale_x;
        float m_dir_y = (-cos_a * m_front) * scale_y;
        float m_side_x = (cos_a * m_w) * scale_x;
        float m_side_y = (sin_a * m_w) * scale_y;
        float m_tail_x = (-sin_a * m_back) * scale_x;
        float m_tail_y = (cos_a * m_back) * scale_y;

        SDL_Vertex m_verts[3];
        m_verts[0].position.x = cx + m_dir_x;
        m_verts[0].position.y = cy + m_dir_y;
        m_verts[0].color = (SDL_FColor){0.0f, 0.0f, 0.0f, 1.0f};

        m_verts[1].position.x = cx + m_tail_x + m_side_x;
        m_verts[1].position.y = cy + m_tail_y + m_side_y;
        m_verts[1].color = (SDL_FColor){0.0f, 0.0f, 0.0f, 1.0f};

        m_verts[2].position.x = cx + m_tail_x - m_side_x;
        m_verts[2].position.y = cy + m_tail_y - m_side_y;
        m_verts[2].color = (SDL_FColor){0.0f, 0.0f, 0.0f, 1.0f};
        SDL_RenderGeometry(renderer, NULL, m_verts, 3, NULL, 0);

        SDL_SetRenderTarget(renderer, g_second_tex);
        float s_w = 2.0f;
        float s_back = 7.0f;
        float s_front = 38.0f;

        float s_dir_x = (sin_a * s_front) * scale_x;
        float s_dir_y = (-cos_a * s_front) * scale_y;
        float s_side_x = (cos_a * s_w) * scale_x;
        float s_side_y = (sin_a * s_w) * scale_y;
        float s_tail_x = (-sin_a * s_back) * scale_x;
        float s_tail_y = (cos_a * s_back) * scale_y;

        SDL_Vertex s_verts[3];
        s_verts[0].position.x = cx + s_dir_x;
        s_verts[0].position.y = cy + s_dir_y;
        s_verts[0].color = (SDL_FColor){0.86f, 0.0f, 0.0f, 1.0f};

        s_verts[1].position.x = cx + s_tail_x + s_side_x;
        s_verts[1].position.y = cy + s_tail_y + s_side_y;
        s_verts[1].color = (SDL_FColor){0.86f, 0.0f, 0.0f, 1.0f};

        s_verts[2].position.x = cx + s_tail_x - s_side_x;
        s_verts[2].position.y = cy + s_tail_y - s_side_y;
        s_verts[2].color = (SDL_FColor){0.86f, 0.0f, 0.0f, 1.0f};
        SDL_RenderGeometry(renderer, NULL, s_verts, 3, NULL, 0);

        if (atlas->second_src_rects) {
            atlas->second_src_rects[i].x = slot_offset_x;
            atlas->second_src_rects[i].y = slot_offset_y;
            atlas->second_src_rects[i].w = (float)ssaa_win_w;
            atlas->second_src_rects[i].h = (float)ssaa_win_h;
        }
    }

    SDL_SetRenderTarget(renderer, prev_target);
    SDL_SetRenderLogicalPresentation(renderer, 150, 300, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    cached_win_w = win_w;
    cached_win_h = win_h;
    cached_ssaa_factor = current_ssaa;

    return true;
}

void RUNTIME_blit_pre_rendered_hands(
    SDL_Renderer *renderer,
    PreFlippedAtlas *atlas,
    float center_x,
    float center_y,
    int hour_phase,
    int minute_phase,
    int second_phase
) {
    if (!atlas || !g_hour_tex || !g_minute_tex || !g_second_tex) {
        return;
    }

    int h_idx = (hour_phase >= 0 && hour_phase < 60) ? hour_phase : 0;
    int m_idx = (minute_phase >= 0 && minute_phase < 60) ? minute_phase : 0;
    int s_idx = (second_phase >= 0 && second_phase < 60) ? second_phase : 0;

    SDL_FRect dest_pos;
    dest_pos.x = center_x - 73.0f;
    dest_pos.y = center_y - 150.0f;
    dest_pos.w = 150.0f;
    dest_pos.h = 300.0f;

    float ssaa_scale = (ssaa_factor > 0) ? (float)ssaa_factor : 1.0f;
    int win_w = 0, win_h = 0;
    SDL_GetRenderOutputSize(renderer, &win_w, &win_h);
    float ssaa_win_w = (float)win_w * ssaa_scale;
    float ssaa_win_h = (float)win_h * ssaa_scale;

    int h_col = h_idx % 8; int h_row = h_idx / 8;
    int m_col = m_idx % 8; int m_row = m_idx / 8;
    int s_col = s_idx % 8; int s_row = s_idx / 8;

    SDL_FRect h_src = { (float)h_col * ssaa_win_w, (float)h_row * ssaa_win_h, ssaa_win_w, ssaa_win_h };
    SDL_FRect m_src = { (float)m_col * ssaa_win_w, (float)m_row * ssaa_win_h, ssaa_win_w, ssaa_win_h };
    SDL_FRect s_src = { (float)s_col * ssaa_win_w, (float)s_row * ssaa_win_h, ssaa_win_w, ssaa_win_h };

    SDL_RenderTexture(renderer, g_hour_tex, &h_src, &dest_pos);
    SDL_RenderTexture(renderer, g_minute_tex, &m_src, &dest_pos);
    if (second_phase >= 0) {
        SDL_RenderTexture(renderer, g_second_tex, &s_src, &dest_pos);
    }
}

void CatClock_DrawHands(SDL_Renderer *renderer, int hours, int minutes) {
    int h_phase = ((hours % 12) * 5) + (minutes / 12);
    int m_phase = minutes % 60;
(void)renderer; (void)h_phase; (void)m_phase;}
