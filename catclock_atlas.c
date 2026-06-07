/******************************************************************************
 * File Name:    catclock_atlas.c
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

#include "catclock_shared.h"
#include <string.h>

#include "assets/catback.xbm"
#include "assets/catwhite.xbm"
#include "assets/cattie.xbm"
#include "assets/eyes.xbm"

static SDL_Texture *assets_layer0 = NULL;
static SDL_Texture *assets_layer1 = NULL;
static SDL_Texture *assets_layer2 = NULL;
static SDL_Texture *assets_layer3 = NULL;
static SDL_Texture *assets_layer_halo = NULL;

int CompareFloats(const void *a, const void *b) {
    float fa = *(const float *)a; float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

/*
 * WidgetWindowHitTestCallback:
 * Feeds current scaled width/height boxes directly into the OS drag instruction channel.
 */
SDL_HitTestResult WidgetWindowHitTestCallback(SDL_Window *win, const SDL_Point *area, void *data) {
    (void)data;
    
    /* Instantly fallback to standard processing if the window has normal frames */
    if (use_window_decorations) {
        return SDL_HITTEST_NORMAL;
    }
    
    int current_win_w = 150, current_win_h = 300;
    SDL_GetWindowSize(win, &current_win_w, &current_win_h);
    
    if (area->x >= 0 && area->x <= current_win_w && area->y >= 0 && area->y <= current_win_h) {
        return SDL_HITTEST_DRAGGABLE;
    }
    return SDL_HITTEST_NORMAL;
}

static SDL_Texture *ConvertXBMOntoTexture(SDL_Renderer *renderer, int w, int h, const unsigned char *bits) {
    SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!tex) return NULL;
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    Uint32 *pixels; int pitch;
    SDL_LockTexture(tex, NULL, (void**)&pixels, &pitch);
    memset(pixels, 0, w * h * sizeof(Uint32));
    int xbm_pitch = (w + 7) / 8;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (bits[y * xbm_pitch + (x / 8)] & (1 << (x % 8))) pixels[y * w + x] = 0xFFFFFFFF;
        }
    }
    SDL_UnlockTexture(tex);
    return tex;
}

void InitPreflippedTextureAtlases(SDL_Renderer *renderer) {
    BakeClockHandsAtlases(renderer);
    assets_layer0 = ConvertXBMOntoTexture(renderer, catback_width, catback_height, (const unsigned char *)catback_bits);
    assets_layer1 = ConvertXBMOntoTexture(renderer, catwhite_width, catwhite_height, (const unsigned char *)catwhite_bits);
    assets_layer2 = ConvertXBMOntoTexture(renderer, eyes_width, eyes_height, (const unsigned char *)eyes_bits);
    assets_layer3 = ConvertXBMOntoTexture(renderer, cattie_width, cattie_height, (const unsigned char *)cattie_bits);

    assets_layer_halo = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 150, 300);
    if (assets_layer_halo && assets_layer0) {
        SDL_SetTextureScaleMode(assets_layer_halo, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(assets_layer_halo, SDL_BLENDMODE_NONE);
        SDL_SetRenderTarget(renderer, assets_layer_halo);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); SDL_RenderClear(renderer);
        SDL_SetTextureColorMod(assets_layer0, 255, 255, 255);
        SDL_FRect offsets[] = { {-1,0,150,300}, {1,0,150,300}, {0,-1,150,300}, {0,1,150,300} };
        for (int i = 0; i < 4; i++) SDL_RenderTexture(renderer, assets_layer0, NULL, &offsets[i]);
        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetTextureBlendMode(assets_layer_halo, SDL_BLENDMODE_BLEND);
    }
}

void FreePreflippedTextureAtlases(void) {
    FreeClockHandsAtlases();
    if (assets_layer0) SDL_DestroyTexture(assets_layer0);
    if (assets_layer1) SDL_DestroyTexture(assets_layer1);
    if (assets_layer2) SDL_DestroyTexture(assets_layer2);
    if (assets_layer3) SDL_DestroyTexture(assets_layer3);
    if (assets_layer_halo) SDL_DestroyTexture(assets_layer_halo);
}

void DrawStaticAssetLayer(SDL_Renderer *renderer, int layer_id) {
    SDL_Texture *tex = (layer_id == 0) ? assets_layer0 : (layer_id == 1) ? assets_layer1 : (layer_id == 2) ? assets_layer2 : assets_layer3;
    SDL_FRect dst = { 0, 0, 150.0f, 300.0f };
    if (layer_id == 2) { dst.x = EYES_MASK_DX; dst.y = EYES_MASK_DY; dst.w = eyes_width; dst.h = eyes_height; }
    if (tex) {
        if (layer_id == 0 || layer_id == 2) SDL_SetTextureColorMod(tex, 0, 0, 0);
        else SDL_SetTextureColorMod(tex, 255, 255, 255);
        SDL_RenderTexture(renderer, tex, NULL, &dst);
    }
}

void DrawPrebakedOutlineLayer(SDL_Renderer *renderer) {
    if (assets_layer_halo) {
        SDL_FRect dst = { 0, 0, 150.0f, 300.0f };
        SDL_RenderTexture(renderer, assets_layer_halo, NULL, &dst);
    }
}
