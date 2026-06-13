/* =============================================================================
   FILE: catclock_atlas.c (Centralized Lifecycle Driver Engine)
   ============================================================================= */
#include "catclock_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    int hand_type;
    SDL_Color color;
} HandShaderConfig;

int CompareFloats(const void *a, const void *b) {
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

void CatClock_OnWindowResize(SDL_WindowEvent *resize_event, CatClock_AppContext *ctx, SDL_Renderer *renderer) {
    (void)resize_event;
    int output_w = 0, output_h = 0;
    if (SDL_GetRenderOutputSize(renderer, &output_w, &output_h)) {
        if (output_w != ctx->current_win_w || output_h != ctx->current_win_h) {
            fprintf(stderr, "[DIAG_EVENT] Window Resize: %dx%d -> %dx%d\n", ctx->current_win_w, ctx->current_win_h, output_w, output_h);
            ctx->current_win_w = output_w;
            ctx->current_win_h = output_h;
            ctx->texture_cache_stale = true;
        }
    }
}

void CatClock_RebakeComputeAtlas(SDL_Renderer *renderer, CatClock_ComputeAtlas *atlas, int cell_base_w, int cell_base_h, int total_frames, int cols, CatClock_ShaderCallback shader, void *userdata) {
    int win_w = 0, win_h = 0;
    SDL_GetRenderOutputSize(renderer, &win_w, &win_h);
    float scale = (float)win_w / BASELINE_CANVAS_W;
    if (scale <= 0.0f) scale = 1.0f;

    if (atlas->texture && scale == atlas->last_scale && total_frames == atlas->total_frames) {
        return;
    }

    CatClock_DestroyComputeAtlas(atlas);

    atlas->total_frames = total_frames;
    atlas->last_scale = scale;
    atlas->cell_w = (int)ceilf((float)cell_base_w * scale);
    atlas->cell_h = (int)ceilf((float)cell_base_h * scale);

    atlas->src_rects = (SDL_FRect *)SDL_malloc(sizeof(SDL_FRect) * total_frames);
    if (!atlas->src_rects) return;

    int rows = (total_frames + cols - 1) / cols;
    int atlas_w = cols * atlas->cell_w;
    int atlas_h = rows * atlas->cell_h;

    fprintf(stderr, "[VRAM_ALLOC] 16-bit Target Pass Grid Created: %dx%d (Frames: %d, Scale: %.2f)\n", atlas_w, atlas_h, total_frames, scale);

    atlas->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA4444, SDL_TEXTUREACCESS_TARGET, atlas_w, atlas_h);
    if (!atlas->texture) {
        SDL_free(atlas->src_rects);
        atlas->src_rects = NULL;
        return;
    }

    SDL_SetTextureBlendMode(atlas->texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(atlas->texture, SDL_SCALEMODE_LINEAR);

    SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, atlas->texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    for (int i = 0; i < total_frames; i++) {
        int cell_x = (i % cols) * atlas->cell_w;
        int cell_y = (i / cols) * atlas->cell_h;

        atlas->src_rects[i] = (SDL_FRect){ (float)cell_x, (float)cell_y, (float)atlas->cell_w, (float)atlas->cell_h };

        SDL_Rect viewport = { cell_x, cell_y, atlas->cell_w, atlas->cell_h };
        SDL_SetRenderViewport(renderer, &viewport);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_FRect local_cell_rect = { 0.0f, 0.0f, (float)atlas->cell_w, (float)atlas->cell_h };
        SDL_RenderFillRect(renderer, &local_cell_rect);

        shader(renderer, atlas->cell_w, atlas->cell_h, scale, i, userdata);
    }

    SDL_SetRenderViewport(renderer, NULL);
    SDL_SetRenderTarget(renderer, old_target);
}

void CatClock_DestroyComputeAtlas(CatClock_ComputeAtlas *atlas) {
    if (atlas->texture) {
        SDL_DestroyTexture(atlas->texture);
        atlas->texture = NULL;
    }
    if (atlas->src_rects) {
        SDL_free(atlas->src_rects);
        atlas->src_rects = NULL;
    }
    atlas->total_frames = 0;
    atlas->cell_w = 0;
    atlas->cell_h = 0;
    atlas->last_scale = -1.0f;
}

/**
 * Specialized Sub-Pass Shader that replicates an unclipped 8-way geometric offset trace
 * to pre-bake a thick white background silhouette mask inside our temporary rendering buffer.
 */
void CatClock_ShaderTailHaloBake(SDL_Renderer *renderer, int cell_w, int cell_h, float scale, int frame_idx, void *userdata) {
    (void)userdata;
    SDL_Color white_cfg = { 255, 255, 255, 255 };

    /* 8-Way micro-offsets mapped during geometry rasterization phase */
    float offsets_x[] = { -1.0f,  1.0f,  0.0f,  0.0f, -1.0f, -1.0f,  1.0f,  1.0f };
    float offsets_y[] = {  0.0f,  0.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f,  1.0f };

    /* Preserve baseline viewport variables */
    SDL_Rect orig_viewport;
    SDL_GetRenderViewport(renderer, &orig_viewport);

    for (int i = 0; i < 8; i++) {
        SDL_Rect offset_viewport = {
            orig_viewport.x + (int)(offsets_x[i] * scale),
            orig_viewport.y + (int)(offsets_y[i] * scale),
            orig_viewport.w,
            orig_viewport.h
        };
        SDL_SetRenderViewport(renderer, &offset_viewport);
        CatClock_ShaderTail(renderer, cell_w, cell_h, scale, frame_idx, &white_cfg);
    }

    /* Restore clean cell viewport positioning */
    SDL_SetRenderViewport(renderer, &orig_viewport);
}

void CatClock_SynchronizePipelineAtlases(SDL_Renderer *renderer, CatClock_AppContext *ctx, float sway_deg, int hour_phase, int minute_phase, int second_phase) {
    if (!ctx || !renderer) return;
    (void)sway_deg;

    int req_frames = target_fps_limit <= 0 ? 60 : target_fps_limit;

    if (!ctx->master_composite_layer || ctx->texture_cache_stale) {
        if (ctx->master_composite_layer) {
            SDL_DestroyTexture(ctx->master_composite_layer);
            ctx->master_composite_layer = NULL;
        }

        /* Force a clean, synchronized purge of old layouts across ALL active slots on scale event */
        CatClock_DestroyComputeAtlas(&ctx->hands_atlas);
        CatClock_DestroyComputeAtlas(&ctx->minutes_atlas);
        CatClock_DestroyComputeAtlas(&ctx->seconds_atlas);
        CatClock_DestroyComputeAtlas(&ctx->eyes_atlas);
        CatClock_DestroyComputeAtlas(&ctx->tail_atlas);

        /* If halo_layer was used as a texture raw pointer, clean it or reset metrics */
        if (ctx->halo_layer) {
            SDL_DestroyTexture(ctx->halo_layer);
            ctx->halo_layer = NULL;
        }

        ctx->master_composite_layer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, ctx->current_win_w, ctx->current_win_h);
        if (ctx->master_composite_layer) {
            SDL_SetTextureBlendMode(ctx->master_composite_layer, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(ctx->master_composite_layer, SDL_SCALEMODE_LINEAR);
        }
        ctx->texture_cache_stale = false;
    }

    /* Isolated Bake Pass: Pre-bake each layout EXACTLY once safely outside blit loops */
    HandShaderConfig hour_cfg   = { HAND_TYPE_HOUR, ctx->fg_color };
    HandShaderConfig minute_cfg = { HAND_TYPE_MINUTE, ctx->fg_color };
    HandShaderConfig second_cfg = { HAND_TYPE_SECOND, (SDL_Color){ 255, 0, 0, 255 } };

    CatClock_RebakeComputeAtlas(renderer, &ctx->hands_atlas, 64, 96, TOTAL_PHASES, 6, CatClock_ShaderHands, &hour_cfg);
    CatClock_RebakeComputeAtlas(renderer, &ctx->minutes_atlas, 64, 96, TOTAL_PHASES, 6, CatClock_ShaderHands, &minute_cfg);
    CatClock_RebakeComputeAtlas(renderer, &ctx->seconds_atlas, 64, 96, TOTAL_PHASES, 6, CatClock_ShaderHands, &second_cfg);
    CatClock_RebakeComputeAtlas(renderer, &ctx->eyes_atlas, 24, 24, req_frames, 10, CatClock_ShaderEyes, NULL);
    CatClock_RebakeComputeAtlas(renderer, &ctx->tail_atlas, 96, 96, req_frames, 8, CatClock_ShaderTail, &ctx->fg_color);

    /* REUSE HALO TEXTURE OVER SENSE SLOT: Temporary allocation to store our outline atlas metrics */
    static CatClock_ComputeAtlas tail_halo_blueprint = {0};
    CatClock_RebakeComputeAtlas(renderer, &tail_halo_blueprint, 96, 96, req_frames, 8, CatClock_ShaderTailHaloBake, NULL);

    SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, ctx->master_composite_layer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    SDL_SetRenderLogicalPresentation(renderer, 150, 300, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    /* Pass 1 & 2: Outer Backdrops */
    CatClock_RenderHaloOutline(ctx->xbm_lib, renderer, ctx->bg_color);
    CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catwhite", ctx->bg_color, 0.0f, 0.0f);

    /* =========================================================================
       Pass 3: Pre-Baked 16-bit Tail with Hardware 8-Way White Halo Silhouette Blit Mapping
       ========================================================================= */
    if (ctx->tail_atlas.texture && tail_halo_blueprint.texture) {
        float phase_ratio = (float)(SDL_GetTicks() % CYCLE_PERIOD_MS) / (float)CYCLE_PERIOD_MS;
        int tail_idx = (int)(phase_ratio * (float)req_frames);
        if (tail_idx >= req_frames) tail_idx = req_frames - 1;
        if (tail_idx < 0) tail_idx = 0;

        /* Re-verified centerline positioning metrics matching baseline grounds */
        SDL_FRect base_dst = { 27.0f, 200.0f, 96.0f, 96.0f };

        /* Render Pass 3A: White Pre-Baked Outline Backdrop Layer */
        SDL_SetTextureBlendMode(tail_halo_blueprint.texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(tail_halo_blueprint.texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(tail_halo_blueprint.texture, 255);
        SDL_RenderTexture(renderer, tail_halo_blueprint.texture, &tail_halo_blueprint.src_rects[tail_idx], &base_dst);

        /* Render Pass 3B: Foreground Inner Core Overlay Layer */
        SDL_SetTextureBlendMode(ctx->tail_atlas.texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(ctx->tail_atlas.texture, ctx->fg_color.r, ctx->fg_color.g, ctx->fg_color.b);
        SDL_SetTextureAlphaMod(ctx->tail_atlas.texture, ctx->fg_color.a);
        SDL_RenderTexture(renderer, ctx->tail_atlas.texture, &ctx->tail_atlas.src_rects[tail_idx], &base_dst);
    }

    /* Pass 4 & 5: Outer Linework Outlines and Neckties */
    CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catback", ctx->fg_color, 0.0f, 0.0f);
    CatClock_RenderXbmLayer(ctx->xbm_lib, renderer, "cattie", ctx->bg_color);

    /* Pass 6 & 7: Eye socket base structure and moving pupil blits */
    CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "eyes", ctx->bg_color, 0.0f, 0.0f);
    if (ctx->eyes_atlas.texture) {
        float phase_ratio = (float)(SDL_GetTicks() % CYCLE_PERIOD_MS) / (float)CYCLE_PERIOD_MS;
        int left_idx = (int)(phase_ratio * (float)req_frames);
        if (left_idx >= req_frames) left_idx = req_frames - 1;
        if (left_idx < 0) left_idx = 0;
        int right_idx = req_frames - 1 - left_idx;
        if (right_idx < 0) right_idx = 0;

        SDL_FRect left_dst = { 48.0f, 30.0f, 24.0f, 24.0f };
        SDL_FRect right_dst = { 80.0f, 30.0f, 24.0f, 24.0f };

        SDL_SetTextureColorMod(ctx->eyes_atlas.texture, 0, 0, 0);
        SDL_RenderTexture(renderer, ctx->eyes_atlas.texture, &ctx->eyes_atlas.src_rects[left_idx], &left_dst);
        SDL_RenderTextureRotated(renderer, ctx->eyes_atlas.texture, &ctx->eyes_atlas.src_rects[right_idx], &right_dst, 0.0, NULL, SDL_FLIP_HORIZONTAL);
    }

    /* Pass 8: Pure, Stateless Time Tracking Hands Overlay (Zero Inline Re-baking) */
    if (ctx->hands_atlas.texture && ctx->minutes_atlas.texture && ctx->seconds_atlas.texture) {
        int h_idx = hour_phase < 0 || hour_phase >= TOTAL_PHASES ? 0 : hour_phase;
        int m_idx = minute_phase < 0 || minute_phase >= TOTAL_PHASES ? 0 : minute_phase;
        int s_idx = second_phase < 0 || second_phase >= TOTAL_PHASES ? 0 : second_phase;

        SDL_FRect dst = { 74.0f - 32.0f, 150.0f - 48.0f, 64.0f, 96.0f };
        SDL_SetTextureColorMod(ctx->hands_atlas.texture, 255, 255, 255);
        SDL_SetTextureColorMod(ctx->minutes_atlas.texture, 255, 255, 255);
        SDL_SetTextureColorMod(ctx->seconds_atlas.texture, 255, 255, 255);

        /* Hour Blit Pass */
        SDL_RenderTexture(renderer, ctx->hands_atlas.texture, &ctx->hands_atlas.src_rects[h_idx], &dst);

        /* Minute Blit Pass */
        SDL_RenderTexture(renderer, ctx->minutes_atlas.texture, &ctx->minutes_atlas.src_rects[m_idx], &dst);

        /* Dedicated Red Tracker Seconds Blit Pass */
        SDL_RenderTexture(renderer, ctx->seconds_atlas.texture, &ctx->seconds_atlas.src_rects[s_idx], &dst);
    }

    SDL_SetRenderTarget(renderer, old_target);
}

