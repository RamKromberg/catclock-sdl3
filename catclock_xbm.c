/* ============================================================================
   FILE: catclock_xbm.c (Optimized 32-Byte Struct Memory Packing)
   ============================================================================ */
#include "catclock_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SDL_Texture *texture; // 8 bytes
    int w;                // 4 bytes
    int h;                // 4 bytes
    char layer_name[16];  // 16 bytes. Total structure = exactly 32 bytes.
} XbmAssetNode;

struct CatClock_XbmLibrary {
    XbmAssetNode catback;
    XbmAssetNode catwhite;
    XbmAssetNode cattie;
    XbmAssetNode eyes;
};

/* --- CRITICAL ARCHITECTURAL FIX: EXPLICIT FORWARD DECLARATION --- */
SDL_Texture* CatClock_LoadDynamicXbmToRGBA4444(SDL_Renderer *renderer, const char *path, SDL_Color color, bool invert_mask);

static void BuildRuntimeNodeTextureRGBA4444(SDL_Renderer *renderer, XbmAssetNode *node, const char *path, const char *name, SDL_Color color, bool invert_mask)
{
    if (!node || !path || !name) return;

    node->texture = CatClock_LoadDynamicXbmToRGBA4444(renderer, path, color, invert_mask);

    if (node->texture) {
        float w, h;
        SDL_GetTextureSize(node->texture, &w, &h);
        node->w = (int)w;
        node->h = (int)h;
    }
    SDL_strlcpy(node->layer_name, name, sizeof(node->layer_name));
}

CatClock_XbmLibrary *CatClock_InitXbmLibrary(SDL_Renderer *renderer)
{
    CatClock_XbmLibrary *lib = calloc(1, sizeof(CatClock_XbmLibrary));
    if (!lib) return NULL;

    SDL_Color texture_mask_base = { 255, 255, 255, 255 };
    SDL_Color eyes_mask         = { 255, 255, 255, 255 };
    SDL_Color neck_tie          = ctx.tie_color;

    /* FIXED: Compile catback with standard mask base so alpha color modding traces correctly */
    BuildRuntimeNodeTextureRGBA4444(renderer, &lib->catback,  "./assets/catback.xbm",  "catback",  texture_mask_base, false);
    BuildRuntimeNodeTextureRGBA4444(renderer, &lib->catwhite, "./assets/catwhite.xbm", "catwhite", texture_mask_base, false);
    BuildRuntimeNodeTextureRGBA4444(renderer, &lib->cattie,   "./assets/cattie.xbm",   "cattie",   neck_tie,          false);
    BuildRuntimeNodeTextureRGBA4444(renderer, &lib->eyes,     "./assets/eyes.xbm",     "eyes",     eyes_mask,         true);

    return lib;
}


void CatClock_RebakeXbmTextures(SDL_Renderer *renderer, CatClock_XbmLibrary *lib)
{
    (void)renderer; (void)lib;
}

void CatClock_RenderXbmLayer(CatClock_XbmLibrary *lib, SDL_Renderer *renderer, const char *layer_id, SDL_Color color)
{
    CatClock_RenderXbmLayerOffset(lib, renderer, layer_id, color, 0.0f, 0.0f);
}

void CatClock_RenderXbmLayerOffset(CatClock_XbmLibrary *lib, SDL_Renderer *renderer, const char *layer_id, SDL_Color color, float offset_x, float offset_y)
{
    if (!lib || !renderer) return;

    XbmAssetNode *target_node = NULL;
    if (strcmp(layer_id, "catback") == 0)       target_node = &lib->catback;
    else if (strcmp(layer_id, "catwhite") == 0) target_node = &lib->catwhite;
    else if (strcmp(layer_id, "cattie") == 0)   target_node = &lib->cattie;
    else if (strcmp(layer_id, "eyes") == 0)     target_node = &lib->eyes;

    if (target_node && target_node->texture)
    {
        SDL_SetTextureColorMod(target_node->texture, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(target_node->texture, color.a);

        SDL_FRect dst;
        if (strcmp(layer_id, "eyes") == 0)
        {
            dst.x = 49.0f + offset_x; dst.y = 30.0f + offset_y;
            dst.w = (float)target_node->w; dst.h = (float)target_node->h;
        }
        else
        {
            dst.x = offset_x; dst.y = offset_y;
            dst.w = BASELINE_CANVAS_W; dst.h = BASELINE_CANVAS_H;
        }
        SDL_RenderTexture(renderer, target_node->texture, NULL, &dst);
    }
}

void CatClock_DestroyXbmLibrary(CatClock_XbmLibrary *lib)
{
    if (!lib) return;
    if (lib->catback.texture)  SDL_DestroyTexture(lib->catback.texture);
    if (lib->catwhite.texture) SDL_DestroyTexture(lib->catwhite.texture);
    if (lib->cattie.texture)   SDL_DestroyTexture(lib->cattie.texture);
    if (lib->eyes.texture)     SDL_DestroyTexture(lib->eyes.texture);
    free(lib);
}

/*
 * Dynamic XBM parser that compiles directly into an SDL_PIXELFORMAT_RGBA4444 texture.
 * Bypasses uncompressed 32-bit surfaces, generating low-overhead 16-bit packed pixels.
 */
SDL_Texture* CatClock_LoadDynamicXbmToRGBA4444(SDL_Renderer *renderer, const char *path, SDL_Color color, bool invert_mask)
{
    if (!renderer || !path) return NULL;

    SDL_IOStream *file = SDL_IOFromFile(path, "r");
    if (!file) {
        SDL_Log("Error: Could not open dynamic XBM asset path: %s", path);
        return NULL;
    }

    Sint64 file_size = SDL_GetIOSize(file);
    if (file_size <= 0) {
        SDL_CloseIO(file);
        return NULL;
    }

    char *buffer = (char*)SDL_malloc(file_size + 1);
    if (!buffer) {
        SDL_CloseIO(file);
        return NULL;
    }
    SDL_ReadIO(file, buffer, file_size);
    buffer[file_size] = '\0';
    SDL_CloseIO(file);

    int w = 0, h = 0;
    char *ptr = SDL_strstr(buffer, "_width");
    if (ptr) {
        while (*ptr && !SDL_isdigit((unsigned char)*ptr)) ptr++;
        w = SDL_atoi(ptr);
    }
    ptr = SDL_strstr(buffer, "_height");
    if (ptr) {
        while (*ptr && !SDL_isdigit((unsigned char)*ptr)) ptr++;
        h = SDL_atoi(ptr);
    }

    ptr = SDL_strstr(buffer, "{");
    if (w <= 0 || h <= 0 || !ptr) {
        SDL_free(buffer);
        return NULL;
    }
    ptr++;

    int bytes_per_row = (w + 7) / 8;
    int total_bytes = bytes_per_row * h;
    unsigned char *raw_bits = (unsigned char*)SDL_calloc(1, total_bytes);
    if (!raw_bits) {
        SDL_free(buffer);
        return NULL;
    }

    for (int i = 0; i < total_bytes; i++) {
        while (*ptr && *ptr != '0' && *(ptr+1) != 'x' && *(ptr+1) != 'X') {
            if (*ptr == '}') break;
            ptr++;
        }
        if (*ptr == '}') break;
        raw_bits[i] = (unsigned char)SDL_strtol(ptr, &ptr, 16);
    }
    SDL_free(buffer);

    int pitch = w * sizeof(uint16_t);
    uint16_t *staging_pixels = (uint16_t*)SDL_malloc(pitch * h);
    if (!staging_pixels) {
        SDL_free(raw_bits);
        return NULL;
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = (y * w) + x;
            int byte_idx = (y * bytes_per_row) + (x / 8);
            int bit_index = x % 8;
            int bit_active = (raw_bits[byte_idx] & (1 << bit_index)) ? 1 : 0;

            if (invert_mask) bit_active = !bit_active;

            if (bit_active) {
                // Solid uncompromised binary packing down to 4-bits with full alpha
                staging_pixels[idx] = (((color.r >> 4) & 0x0F) << 12) |
                                      (((color.g >> 4) & 0x0F) << 8)  |
                                      (((color.b >> 4) & 0x0F) << 4)  |
                                      0x000F;
            } else {
                staging_pixels[idx] = 0x0000; // Absolute transparency
            }
        }
    }
    SDL_free(raw_bits);

    SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA4444, SDL_TEXTUREACCESS_STATIC, w, h);
    if (tex) {
        SDL_UpdateTexture(tex, NULL, staging_pixels, pitch);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

        // Force the GPU to sample edges sharply using nearest-neighbor
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    }

    SDL_free(staging_pixels);
    return tex;
}

void CatClock_SetXbmLayerScaleMode(CatClock_XbmLibrary *lib, const char *layer_id, SDL_ScaleMode mode) {
    if (!lib || !layer_id) return;

    XbmAssetNode *target = NULL;
    if (SDL_strcmp(layer_id, "catback") == 0)       target = &lib->catback;
    else if (SDL_strcmp(layer_id, "catwhite") == 0) target = &lib->catwhite;
    else if (SDL_strcmp(layer_id, "cattie") == 0)   target = &lib->cattie;
    else if (SDL_strcmp(layer_id, "eyes") == 0)     target = &lib->eyes;

    if (target && target->texture) {
        SDL_SetTextureScaleMode(target->texture, mode);
    }
}

void CatClock_SetXbmLayerBlendMode(CatClock_XbmLibrary *lib, const char *layer_id, SDL_BlendMode mode) {
    if (!lib || !layer_id) return;

    XbmAssetNode *target = NULL;
    if (SDL_strcmp(layer_id, "catback") == 0)     target = &lib->catback;
    else if (SDL_strcmp(layer_id, "catwhite") == 0) target = &lib->catwhite;
    else if (SDL_strcmp(layer_id, "cattie") == 0)   target = &lib->cattie;
    else if (SDL_strcmp(layer_id, "eyes") == 0)     target = &lib->eyes;

    if (target && target->texture) {
        SDL_SetTextureBlendMode(target->texture, mode);
    }
}

void CatClock_RenderHaloOutline(CatClock_XbmLibrary *lib, SDL_Renderer *renderer, SDL_Color color) {
    if (!lib || !renderer || !lib->catback.texture) return;

    SDL_SetTextureScaleMode(lib->catback.texture, SDL_SCALEMODE_NEAREST);

    SDL_BlendMode protective_halo_mode = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE,                 // Source Color Factor
        SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, // Destination Color Factor
        SDL_BLENDOPERATION_ADD,              // Color Operation
        SDL_BLENDFACTOR_ONE,                 // Source Alpha Factor
        SDL_BLENDFACTOR_ONE,                 // Destination Alpha Factor
        SDL_BLENDOPERATION_ADD               // Alpha Operation
    );
    SDL_SetTextureBlendMode(lib->catback.texture, protective_halo_mode);

    SDL_SetTextureColorMod(lib->catback.texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(lib->catback.texture, color.a);

    for (float dx = -1.0f; dx <= 1.0f; dx += 1.0f) {
        for (float dy = -1.0f; dy <= 1.0f; dy += 1.0f) {
            if (dx == 0.0f && dy == 0.0f) continue;

            SDL_FRect dst = { dx, dy, BASELINE_CANVAS_W, BASELINE_CANVAS_H };
            SDL_RenderTexture(renderer, lib->catback.texture, NULL, &dst);
        }
    }

    SDL_SetTextureBlendMode(lib->catback.texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(lib->catback.texture, SDL_SCALEMODE_NEAREST);
}
