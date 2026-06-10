/* ==========================================================================
   FILE: catclock_atlas.c (High-Res Native Target Blit Sync Framework)
   ========================================================================== */
#include "catclock_shared.h"

int CompareFloats(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

void CatClock_OnWindowResize(SDL_WindowEvent *resize_event, CatClock_AppContext *ctx, SDL_Renderer *renderer)
{
    (void)resize_event;
    int output_w = 0, output_h = 0;
    if (SDL_GetRenderOutputSize(renderer, &output_w, &output_h))
    {
        if (output_w != ctx->current_win_w || output_h != ctx->current_win_h)
        {
            ctx->current_win_w = output_w;
            ctx->current_win_h = output_h;
            ctx->texture_cache_stale = true;
        }
    }
}

void CatClock_SynchronizePipelineAtlases(SDL_Renderer *renderer, CatClock_AppContext *ctx, float sway_deg, int hour_phase, int minute_phase, int second_phase)
{
    if (!ctx || !renderer) return;

    /*
       Fix: The backing offscreen composite texture canvas now dynamic tracks and allocates
       to match the exact physical output dimensions of your scaled window widget viewport.
       This provides high-resolution pre-baking without exceeding hardware bounds.
    */
    if (!ctx->master_composite_layer || ctx->texture_cache_stale)
    {
        if (ctx->master_composite_layer) SDL_DestroyTexture(ctx->master_composite_layer);

        ctx->master_composite_layer = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_TARGET,
            ctx->current_win_w,
            ctx->current_win_h
        );

        if (ctx->master_composite_layer)
        {
            SDL_SetTextureBlendMode(ctx->master_composite_layer, SDL_BLENDMODE_BLEND);
            /* Flip scale mode to linear so the GPU anti-aliases edge sampling points */
            SDL_SetTextureScaleMode(ctx->master_composite_layer, SDL_SCALEMODE_LINEAR);
        }

        REBUILD_pre_rendered_60phase_atlas(renderer, &ctx->hands_atlas_meta, ctx->fg_color.r, ctx->fg_color.g, ctx->fg_color.b, ctx->fg_color.a);
        ctx->texture_cache_stale = false;
    }

    SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, ctx->master_composite_layer);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    /*
       Enforce logical coordinate resolution mapping inside the high-res canvas target.
       The drawing functions can continue using baseline metrics (150x300 coordinates),
       while the hardware rendering pipeline draws them directly to the updated high-res target bounds.
    */
    SDL_SetRenderLogicalPresentation(renderer, 150, 300, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    /* Pass 1: Render the 1px white body halo background outline */
    for (float dx = -1.0f; dx <= 1.0f; dx += 1.0f)
    {
        for (float dy = -1.0f; dy <= 1.0f; dy += 1.0f)
        {
            if (dx == 0.0f && dy == 0.0f) continue;
            CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catback", ctx->bg_color, dx, dy);
        }
    }

    /* Pass 2: Solid background mask silhouette canvas shape (catwhite) */
    CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catwhite", ctx->bg_color, 0.0f, 0.0f);

    /* Pass 3: Polygon scanning dynamic tail vector primitive core */
    RenderOriginalThickSwayingTail(renderer, 74.0f, 210.5f, sway_deg, ctx->fg_color, true);

    /* Pass 4: Detailed structural artwork lines framework boundaries (catback) */
    CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catback", ctx->fg_color, 0.0f, 0.0f);

    /* Draw the necktie using the background color (white) */
    CatClock_RenderXbmLayer(ctx->xbm_lib, renderer, "cattie", ctx->bg_color);

    /* Absolute minimal state safeguard: Reset texture color mod properties immediately */
    SDL_SetTextureColorMod(ctx->master_composite_layer, 255, 255, 255);


    /* Pass 6: Isolated eye socket whites base background (eyes) */
    CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "eyes", ctx->bg_color, 0.0f, 0.0f);

    /* Pass 7: Moving pupil tracking matrix elements vectors */
    RenderAuthenticOriginalEyes(renderer, ctx->fg_color);

    /* Pass 8: Clock time hands blitted from texture atlas tracking coordinates */
    RUNTIME_blit_pre_rendered_hands(renderer, &ctx->hands_atlas_meta, 74.0f, 150.0f, hour_phase, minute_phase, second_phase);

    SDL_SetRenderTarget(renderer, old_target);
}
