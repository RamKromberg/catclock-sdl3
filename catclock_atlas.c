#include "catclock_shared.h"
#include "catclock_atlas.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

extern void CatClock_ShaderTail(SDL_Renderer *renderer, int cell_w, int cell_h, float scale, int frame_idx, const SDL_Color *color);
extern void CatClock_ShaderHands(SDL_Renderer *renderer, int cell_w, int cell_h, float scale, int frame_idx, void *userdata);
extern void CatClock_ShaderEyes(SDL_Renderer *renderer, int cell_w, int cell_h, float scale, int frame_idx, void *userdata);

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

    atlas->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA4444, SDL_TEXTUREACCESS_TARGET, atlas_w, atlas_h);
    if (!atlas->texture) {
        SDL_free(atlas->src_rects);
        atlas->src_rects = NULL;
        return;
    }

    SDL_SetTextureBlendMode(atlas->texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(atlas->texture, SDL_SCALEMODE_NEAREST);

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

void CatClock_ShaderTailHaloBake(SDL_Renderer *renderer, int cell_w, int cell_h, float scale, int frame_idx, void *userdata) {
    (void)userdata;
    SDL_Color white_cfg = { 255, 255, 255, 255 };

    float offsets_x[] = { -1.0f,  1.0f,  0.0f,  0.0f, -1.0f, -1.0f,  1.0f,  1.0f };
    float offsets_y[] = {  0.0f,  0.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f,  1.0f };

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
    SDL_SetRenderViewport(renderer, &orig_viewport);
}

void CatClock_SynchronizePipelineAtlases(SDL_Renderer *renderer, CatClock_AppContext *ctx, float sway_deg, int hour_phase, int minute_phase, int second_phase) {
    if (!ctx || !renderer) return;
    (void)sway_deg;

    int req_frames = target_fps_limit <= 0 ? 60 : target_fps_limit;
    bool actual_disable_outline = ctx->disable_outline || ctx->use_decorations;

    /* Compute raw operational engine scale factor directly from current window boundaries */
    int win_w = 0, win_h = 0;
    SDL_GetRenderOutputSize(renderer, &win_w, &win_h);
    float runtime_scale = (float)win_w / BASELINE_CANVAS_W;
    if (runtime_scale <= 0.0f) runtime_scale = 1.0f;

    /* Calculate specialized high-DPI stencil dimensions matching the strict 24x24 cell bounds worth of live data */
    int calculated_cell_w = (int)ceilf(24.0f * runtime_scale);
    int calculated_cell_h = (int)ceilf(24.0f * runtime_scale);
    if (calculated_cell_w < 24) calculated_cell_w = 24;
    if (calculated_cell_h < 24) calculated_cell_h = 24;

    if (!ctx->master_composite_layer || ctx->texture_cache_stale || !ctx->eye_clipping_stencil) {
        if (ctx->master_composite_layer) {
            SDL_DestroyTexture(ctx->master_composite_layer);
            ctx->master_composite_layer = NULL;
        }
        if (ctx->eye_clipping_stencil) {
            SDL_DestroyTexture(ctx->eye_clipping_stencil);
            ctx->eye_clipping_stencil = NULL;
        }
        CatClock_DestroyComputeAtlas(&ctx->hands_atlas);
        CatClock_DestroyComputeAtlas(&ctx->minutes_atlas);
        CatClock_DestroyComputeAtlas(&ctx->seconds_atlas);
        CatClock_DestroyComputeAtlas(&ctx->eyes_atlas);
        CatClock_DestroyComputeAtlas(&ctx->tail_atlas);

        ctx->master_composite_layer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, ctx->current_win_w, ctx->current_win_h);

        /* ALLOCATING HIGH-RESOLUTION SCRATCHPAD SUB-SURFACE */
        ctx->eye_clipping_stencil = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, calculated_cell_w, calculated_cell_h);

        /* DIAGNOSTIC VERIFICATION PROBE TRAILER TRACKING RUNTIME TEXTURE ENGINES */
        SDL_Log("[CatClock Stencil Diagnostic] Initialized Pupil Target Surface -> Width: %ddpx, Height: %ddpx, Active Scale Factor: %.4ffi",
                calculated_cell_w, calculated_cell_h, runtime_scale);

        ctx->texture_cache_stale = false;
    }

    SDL_Color atlas_base_mask = { 255, 255, 255, 255 };
    HandShaderConfig hour_cfg   = { HAND_TYPE_HOUR, atlas_base_mask };
    HandShaderConfig minute_cfg = { HAND_TYPE_MINUTE, atlas_base_mask };
    HandShaderConfig second_cfg = { HAND_TYPE_SECOND, atlas_base_mask };

    CatClock_RebakeComputeAtlas(renderer, &ctx->hands_atlas, 64, 96, TOTAL_PHASES, 6, CatClock_ShaderHands, &hour_cfg);
    CatClock_RebakeComputeAtlas(renderer, &ctx->minutes_atlas, 64, 96, TOTAL_PHASES, 6, CatClock_ShaderHands, &minute_cfg);
    CatClock_RebakeComputeAtlas(renderer, &ctx->seconds_atlas, 64, 96, TOTAL_PHASES, 6, CatClock_ShaderHands, &second_cfg);
    CatClock_RebakeComputeAtlas(renderer, &ctx->eyes_atlas, 24, 24, req_frames, 10, CatClock_ShaderEyes, NULL);
    CatClock_RebakeComputeAtlas(renderer, &ctx->tail_atlas, 96, 96, 60, 8, (CatClock_ShaderCallback)CatClock_ShaderTail, &atlas_base_mask);

    static CatClock_ComputeAtlas tail_halo_blueprint = {0};
    if (!actual_disable_outline) {
        CatClock_RebakeComputeAtlas(renderer, &tail_halo_blueprint, 96, 96, 60, 8, CatClock_ShaderTailHaloBake, NULL);
    }

    SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, ctx->master_composite_layer);

    if (ctx->use_decorations) {
        SDL_SetRenderDrawColor(renderer, ctx->window_bg_color.r, ctx->window_bg_color.g, ctx->window_bg_color.b, ctx->window_bg_color.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    }
    SDL_RenderClear(renderer);

    SDL_SetRenderLogicalPresentation(renderer, 150, 300, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    if (!actual_disable_outline) {
        CatClock_RenderHaloOutline(ctx->xbm_lib, renderer, ctx->bg_color);
    }
    CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catwhite", ctx->detail_color, 0.0f, 0.0f);

        if (ctx->tail_atlas.texture) {
            float phase_ratio = (float)(SDL_GetTicks() % CYCLE_PERIOD_MS) / (float)CYCLE_PERIOD_MS;
            int tail_idx = (int)(phase_ratio * (float)ctx->tail_atlas.total_frames);
            if (tail_idx >= ctx->tail_atlas.total_frames) tail_idx = ctx->tail_atlas.total_frames - 1;
            if (tail_idx < 0) tail_idx = 0;

            SDL_FRect base_dst = { 27.0f, 200.0f, 96.0f, 96.0f };

            if (!actual_disable_outline && tail_halo_blueprint.texture) {
                SDL_SetTextureBlendMode(tail_halo_blueprint.texture, SDL_BLENDMODE_BLEND);
                SDL_SetTextureColorMod(tail_halo_blueprint.texture, 255, 255, 255);
            SDL_SetTextureAlphaMod(tail_halo_blueprint.texture, 255);
            SDL_RenderTexture(renderer, tail_halo_blueprint.texture, &tail_halo_blueprint.src_rects[tail_idx], &base_dst);
            }

            SDL_SetTextureBlendMode(ctx->tail_atlas.texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(ctx->tail_atlas.texture, ctx->cat_color.r, ctx->cat_color.g, ctx->cat_color.b);
            SDL_SetTextureAlphaMod(ctx->tail_atlas.texture, ctx->cat_color.a);
            SDL_RenderTexture(renderer, ctx->tail_atlas.texture, &ctx->tail_atlas.src_rects[tail_idx], &base_dst);
        }

        CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catback", ctx->cat_color, 0.0f, 0.0f);
        CatClock_RenderXbmLayer(ctx->xbm_lib, renderer, "cattie", ctx->tie_color);

        /* --- PHASE 1: HIGH-DPI SCALED 24x24 MASK PRESENTATION ENGINE --- */
        if (ctx->eyes_atlas.texture && ctx->eye_clipping_stencil) {
            float phase_ratio = (float)(SDL_GetTicks() % CYCLE_PERIOD_MS) / (float)CYCLE_PERIOD_MS;
            int left_idx = (int)(phase_ratio * (float)req_frames);
            if (left_idx >= req_frames) left_idx = req_frames - 1;
            if (left_idx < 0) left_idx = 0;
            int right_idx = req_frames - 1 - left_idx;
            if (right_idx < 0) right_idx = 0;

            /* Clean unmultiplied screen targets aligned securely to layout boundaries */
            SDL_FRect left_dst  = { 49.0f, 30.0f, 24.0f, 24.0f };
            SDL_FRect right_dst = { 79.0f, 30.0f, 24.0f, 24.0f };

            /* Local unscaled logical boundaries for both pupils inside our 24x24 target matrix */
            SDL_FRect pupil_dst_local = { 0.0f, 0.0f, 24.0f, 24.0f };

            // Save parent logical display viewport configuration cleanly
            SDL_Rect viewport_save;
            SDL_GetRenderViewport(renderer, &viewport_save);

            /* Paint baseline eyes mask directly onto the main presentation layer first using detail_color instead of bg_color */
            CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "eyes", ctx->detail_color, 0.0f, 0.0f);

            // --- SUB-PHASE A: COMPOSITING SCREEN'S LEFT EYE SOCKET ---
            SDL_SetRenderTarget(renderer, ctx->eye_clipping_stencil);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);

            /* GPU CROP LOCK: Set logical presentation size to 24x24 to truncate anything outside our local boundaries */
            SDL_SetRenderLogicalPresentation(renderer, 24, 24, SDL_LOGICAL_PRESENTATION_STRETCH);

            /* Shift by exactly -49.0f to center the left socket hole inside our unscaled logical box */
            CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "eyes", (SDL_Color){255,255,255,255}, -49.0f, -30.0f);

            /*
             * FIX 1: USE MODULATION INTERSECTION
             * Changing this to SDL_BLENDMODE_MOD multiplies the multi-color cell from your shader
             * against the white XBM mask footprint. This crops your custom yellow background
             * perfectly within the eye sockets, leaving the nose bridge completely untouched!
             */
            SDL_SetTextureBlendMode(ctx->eyes_atlas.texture, SDL_BLENDMODE_MOD);
            SDL_RenderTexture(renderer, ctx->eyes_atlas.texture, &ctx->eyes_atlas.src_rects[left_idx], &pupil_dst_local);

            /* Blitz left completed target back onto main canvas presentation layer */
            SDL_SetRenderViewport(renderer, &viewport_save);
            SDL_SetRenderLogicalPresentation(renderer, 150, 300, SDL_LOGICAL_PRESENTATION_LETTERBOX);
            SDL_SetRenderTarget(renderer, ctx->master_composite_layer);
            SDL_SetTextureBlendMode(ctx->eye_clipping_stencil, SDL_BLENDMODE_BLEND);

            /* Bypass the restrictive single-color pupil blackouts */
            SDL_SetTextureColorMod(ctx->eye_clipping_stencil, 255, 255, 255);
            SDL_RenderTexture(renderer, ctx->eye_clipping_stencil, NULL, &left_dst);

            // --- SUB-PHASE B: COMPOSITING SCREEN'S RIGHT EYE SOCKET ---
            SDL_SetRenderTarget(renderer, ctx->eye_clipping_stencil);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);

            /* Re-enforce GPU crop lock constraints for the right eye cell pass */
            SDL_SetRenderLogicalPresentation(renderer, 24, 24, SDL_LOGICAL_PRESENTATION_STRETCH);

            /* AVOID RE-GUESSING OFFSET: Instantiate a temporary scratch buffer to capture the exact left eye XBM chunk */
            SDL_Texture *old_target_sub = SDL_GetRenderTarget(renderer);
            SDL_Texture *eyes_scratch_target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, calculated_cell_w, calculated_cell_h);
            SDL_SetRenderTarget(renderer, eyes_scratch_target);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);
            SDL_SetRenderLogicalPresentation(renderer, 24, 24, SDL_LOGICAL_PRESENTATION_STRETCH);

            /* Render the left eye socket plate data (-49.0f) exactly like Sub-Phase A */
            CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "eyes", (SDL_Color){255,255,255,255}, -49.0f, -30.0f);

            /* Switch context back to our main stencil compilation layer */
            SDL_SetRenderTarget(renderer, old_target_sub);
            SDL_SetRenderLogicalPresentation(renderer, 24, 24, SDL_LOGICAL_PRESENTATION_STRETCH);

            /* FIXED COMPOSITION SYMMETRY: Blitz the left mask chunk flipped horizontally into the stencil target. */
            SDL_RenderTextureRotated(renderer, eyes_scratch_target, NULL, &pupil_dst_local, 0.0, NULL, SDL_FLIP_HORIZONTAL);
            SDL_DestroyTexture(eyes_scratch_target);

            /* FIX 2: Apply modulation intersection to the right eye cell pass as well */
            SDL_SetTextureBlendMode(ctx->eyes_atlas.texture, SDL_BLENDMODE_MOD);
            SDL_RenderTextureRotated(renderer, ctx->eyes_atlas.texture, &ctx->eyes_atlas.src_rects[right_idx], &pupil_dst_local, 0.0, NULL, SDL_FLIP_HORIZONTAL);

            /* Blitz right completed target back onto main canvas presentation layer */
            SDL_SetRenderViewport(renderer, &viewport_save);
            SDL_SetRenderLogicalPresentation(renderer, 150, 300, SDL_LOGICAL_PRESENTATION_LETTERBOX);
            SDL_SetRenderTarget(renderer, ctx->master_composite_layer);
            SDL_SetTextureBlendMode(ctx->eye_clipping_stencil, SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(ctx->eye_clipping_stencil, 255, 255, 255);
            SDL_RenderTexture(renderer, ctx->eye_clipping_stencil, NULL, &right_dst);

        }
        /* --- PHASE 2: TIME HAND PRESENTATION CHANNELS --- */
        if (ctx->hands_atlas.texture && ctx->minutes_atlas.texture && ctx->seconds_atlas.texture) {
            int h_idx = hour_phase < 0 || hour_phase >= TOTAL_PHASES ? 0 : hour_phase;
            int m_idx = minute_phase < 0 || minute_phase >= TOTAL_PHASES ? 0 : minute_phase;
            int s_idx = second_phase < 0 || second_phase >= TOTAL_PHASES ? 0 : second_phase;

            SDL_FRect dst = { 74.0f - 32.0f, 150.0f - 48.0f, 64.0f, 96.0f };

            SDL_SetTextureColorMod(ctx->hands_atlas.texture, ctx->hour_color.r, ctx->hour_color.g, ctx->hour_color.b);
            SDL_RenderTexture(renderer, ctx->hands_atlas.texture, &ctx->hands_atlas.src_rects[h_idx], &dst);

            SDL_SetTextureColorMod(ctx->minutes_atlas.texture, ctx->minute_color.r, ctx->minute_color.g, ctx->minute_color.b);
            SDL_RenderTexture(renderer, ctx->minutes_atlas.texture, &ctx->minutes_atlas.src_rects[m_idx], &dst);


            if (!ctx->hide_seconds) {
                SDL_SetTextureColorMod(ctx->seconds_atlas.texture, ctx->second_color.r, ctx->second_color.b, ctx->second_color.g);
                SDL_RenderTexture(renderer, ctx->seconds_atlas.texture, &ctx->seconds_atlas.src_rects[s_idx], &dst);
            }
        }

        SDL_SetRenderTarget(renderer, old_target);
    }
