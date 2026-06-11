/* ==========================================================================
   FILE: catclock_atlas.c (High-Res Native Target Blit Sync Framework)
   ========================================================================== */
#include "catclock_atlas.h"
#include "catclock_shared.h"
#include <stdint.h>
#include <stdlib.h>

// Helper to pack separate 8-bit color channels into a native 16-bit RGBA4444 word
static uint16_t PackRGBA4444(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (((r >> 4) & 0x0F) << 12) |
           (((g >> 4) & 0x0F) << 8)  |
           (((b >> 4) & 0x0F) << 4)  |
           ((a >> 4) & 0x0F);
}

// Safely extracts a single coordinate out of a standard 1-bit XBM data array (LSB first)
static int GetXbmBit(const unsigned char *bits, int width, int x, int y) {
    int bytes_per_row = (width + 7) / 8;
    int byte_index = (y * bytes_per_row) + (x / 8);
    int bit_index = x % 8;
    return (bits[byte_index] & (1 << bit_index)) ? 1 : 0;
}

/**
 * Generates the animated eye pupil atlas using an native 16-bit RGBA4444 format.
 * Uses the raw 1-bit dynamic mask array byte stream to mask pupil frame overflows.
 */
SDL_Texture* CatClock_CompileEyesAtlasRGBA4444(SDL_Renderer *renderer,
                                               const unsigned char *xbm_mask_bytes,
                                               const unsigned char *pupil_alpha_src,
                                               int mask_w, int mask_h,
                                               int total_frames,
                                               SDL_Color sclera_color,
                                               SDL_Color pupil_color)
{
    if (!renderer || !xbm_mask_bytes || !pupil_alpha_src) return NULL;

    int atlas_w = mask_w * total_frames;
    int atlas_h = mask_h;

    int pitch = atlas_w * sizeof(uint16_t);
    uint16_t *atlas_buffer = (uint16_t*)SDL_malloc(pitch * atlas_h);
    if (!atlas_buffer) return NULL;

    for (int y = 0; y < atlas_h; y++) {
        for (int x = 0; x < atlas_w; x++) {
            int atlas_idx = (y * atlas_w) + x;

            // Map layout positions horizontally back to the 53x23 dual-socket boundaries
            int local_mask_x = x % mask_w;
            int local_mask_y = y % mask_h;

            // Step 1: Check structural eye boundary validity using the 1-bit dynamic data array
            int in_socket = 0;

            // Left socket bounds range: 0 to 22. Right socket bounds range: 30 to 52.
            if (local_mask_x < 23 || local_mask_x >= 30) {
                in_socket = GetXbmBit(xbm_mask_bytes, mask_w, local_mask_x, local_mask_y);
            }

            if (!in_socket) {
                // Hard mask clip: force any pupil overflow outside the ovals to transparent black
                atlas_buffer[atlas_idx] = 0x0000;
                continue;
            }

            // Step 2: Extract moving pupil 8-bit alpha positions from your animation frame array
            uint8_t pupil_alpha = pupil_alpha_src[atlas_idx];

            if (pupil_alpha > 0) {
                // Pack our independent pupil color with proper smooth alpha visibility
                atlas_buffer[atlas_idx] = PackRGBA4444(pupil_color.r, pupil_color.g, pupil_color.b, pupil_alpha);
            } else {
                // Fill remaining valid socket spaces with our independent sclera color choice
                atlas_buffer[atlas_idx] = PackRGBA4444(sclera_color.r, sclera_color.g, sclera_color.b, 255);
            }
        }
    }

    // Allocate the static hardware texture footprint directly in native 16-bit graphics memory
    SDL_Texture *eyes_atlas_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA4444, SDL_TEXTUREACCESS_STATIC, atlas_w, atlas_h);
    if (eyes_atlas_tex) {
        SDL_UpdateTexture(eyes_atlas_tex, NULL, atlas_buffer, pitch);
        SDL_SetTextureBlendMode(eyes_atlas_tex, SDL_BLENDMODE_BLEND);
    }

    SDL_free(atlas_buffer);
    return eyes_atlas_tex;
}

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

    // ============================================================================
    // LOCALIZED FIX: PROTECT ALREADY-DRAWN COLORS FROM INCOMING TRANSPARENCY
    // ============================================================================
    // We configure the master target layer to protect its destination pixels.
    // This stops subsequent transparent layers from turning our white halo black!
    SDL_SetTextureBlendMode(ctx->master_composite_layer, SDL_BLENDMODE_NONE);

    /* Pass 1: Render the 1px white body halo background outline */
    CatClock_RenderHaloOutline(ctx->xbm_lib, renderer, ctx->bg_color);

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

    // Restore standard alpha blending right after compositing is complete
    // so it projects cleanly onto your transparent OS window environment!
    SDL_SetTextureBlendMode(ctx->master_composite_layer, SDL_BLENDMODE_BLEND);
    // ============================================================================

    /* Pass 6: Isolated eye socket whites base background (eyes) */
    CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "eyes", ctx->bg_color, 0.0f, 0.0f);

    /* Pass 7: Moving pupil tracking matrix elements vectors */
    SDL_Color pupil_color = { 0, 0, 0, 255 };
    RenderAuthenticOriginalEyes(renderer, pupil_color);

    /* Pass 8: Clock time hands blitted from texture atlas tracking coordinates */
    RUNTIME_blit_pre_rendered_hands(renderer, &ctx->hands_atlas_meta, 74.0f, 150.0f, hour_phase, minute_phase, second_phase);

    SDL_SetRenderTarget(renderer, old_target);
}

