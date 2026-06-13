/* =============================================================================
   FILE: catclock_atlas.c (Pipeline Compositor Linker Core)
   ============================================================================= */
#include "catclock_atlas.h"
#include "catclock_shared.h"
#include <stdio.h>

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
            fprintf(stderr, "[DIAG_EVENT] Window Resize Old: %dx%d -> New: %dx%d\n", ctx->current_win_w, ctx->current_win_h, output_w, output_h);
            ctx->current_win_w = output_w;
            ctx->current_win_h = output_h;
            ctx->texture_cache_stale = true;
        }
    }
}

void CatClock_SynchronizePipelineAtlases(SDL_Renderer *renderer, CatClock_AppContext *ctx, float sway_deg, int hour_phase, int minute_phase, int second_phase) {
    if (!ctx || !renderer) return;

    if (!ctx->master_composite_layer || ctx->texture_cache_stale) {
        if (ctx->master_composite_layer) {
            fprintf(stderr, "[DIAG_ALLOC] Destroying Master TARGET Layer\n");
            SDL_DestroyTexture(ctx->master_composite_layer);
            ctx->master_composite_layer = NULL;
        }

        fprintf(stderr, "[DIAG_ALLOC] Creating Master TARGET Layer: %dx%d (Format: RGBA32)\n", ctx->current_win_w, ctx->current_win_h);
        ctx->master_composite_layer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, ctx->current_win_w, ctx->current_win_h);
        if (ctx->master_composite_layer) {
            SDL_SetTextureBlendMode(ctx->master_composite_layer, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(ctx->master_composite_layer, SDL_SCALEMODE_LINEAR);
        }

        /* Rebuild all 16-bit On-GPU Target components synchronously */
        REBUILD_pre_rendered_60phase_atlas(renderer, &ctx->hands_atlas_meta, ctx->fg_color.r, ctx->fg_color.g, ctx->fg_color.b, ctx->fg_color.a);
        REBUILD_pre_rendered_tail_atlas(renderer, &ctx->tail_atlas_meta, ctx->fg_color);

        ctx->texture_cache_stale = false;
    }

    SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, ctx->master_composite_layer);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    SDL_SetRenderLogicalPresentation(renderer, 150, 300, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetTextureBlendMode(ctx->master_composite_layer, SDL_BLENDMODE_NONE);

    /* Pass 1 & 2: Outer Body Outlines and Base Silhouette Layers Shape Shapes */
    CatClock_RenderHaloOutline(ctx->xbm_lib, renderer, ctx->bg_color);
    CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catwhite", ctx->bg_color, 0.0f, 0.0f);

    /* Pass 3: Pre-Baked 16-bit Tail Target Atlas Blit (Replacing raw multi-point runtime loops!) */
    RUNTIME_blit_pre_rendered_tail(renderer, &ctx->tail_atlas_meta, 74.0f, 210.5f, sway_deg);

    /* Pass 4 & 5: Detailed Structural Framework Linework boundaries and Necktie decorations */
    CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catback", ctx->fg_color, 0.0f, 0.0f);
    CatClock_RenderXbmLayer(ctx->xbm_lib, renderer, "cattie", ctx->bg_color);

    SDL_SetTextureColorMod(ctx->master_composite_layer, 255, 255, 255);
    SDL_SetTextureBlendMode(ctx->master_composite_layer, SDL_BLENDMODE_BLEND);

    /* Pass 6 & 7: Eye Socket Whites Base and Moving Pupils Target Layer */
    CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "eyes", ctx->bg_color, 0.0f, 0.0f);

    SDL_Color pupil_color = { 0, 0, 0, 255 };
    RUNTIME_blit_pre_rendered_eyes(renderer, pupil_color);

    /* Pass 8: Clock time hands tracking coordinates */
    RUNTIME_blit_pre_rendered_hands(renderer, &ctx->hands_atlas_meta, 74.0f, 150.0f, hour_phase, minute_phase, second_phase);

    SDL_SetRenderTarget(renderer, old_target);
}
