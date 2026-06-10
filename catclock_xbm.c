/* ==========================================================================
   FILE: catclock_xbm.c (Optimized 32-Byte Struct Memory Packing)
   ========================================================================== */
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

static SDL_Surface *LoadXbmFileToSurface(const char *path, const char *layer_name)
{
    FILE *f = fopen(path, "r");
    if (!f) return SDL_CreateSurface(150, 300, SDL_PIXELFORMAT_RGBA32);

    char line[256];
    int width = 0, height = 0;

    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "_width"))
        {
            char *ptr = strrchr(line, ' ');
            if (ptr) width = atoi(ptr);
        }
        else if (strstr(line, "_height"))
        {
            char *ptr = strrchr(line, ' ');
            if (ptr) height = atoi(ptr);
        }
        if (width > 0 && height > 0) break;
    }

    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "static char") || strstr(line, "static unsigned char") || strstr(line, "_bits")) break;
    }

    SDL_Surface *surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
    if (!surface) { fclose(f); return NULL; }

    SDL_LockSurface(surface);
    Uint32 *pixels = (Uint32 *)surface->pixels;
    int stride = surface->pitch / 4;
    int bytes_per_row = (width + 7) / 8;
    bool is_eyes = (strcmp(layer_name, "eyes") == 0);

    for (int y = 0; y < height; y++)
    {
        for (int b = 0; b < bytes_per_row; b++)
        {
            unsigned int byte_val = 0;
            if (fscanf(f, " 0x%x ,", &byte_val) != 1)
            {
                if (fscanf(f, " %x ,", &byte_val) != 1) byte_val = 0;
            }

            for (int bit = 0; bit < 8; bit++)
            {
                int pixel_x = (b * 8) + bit;
                if (pixel_x < width)
                {
                    bool is_set = (byte_val & (1 << bit)) != 0;
                    if (is_eyes)
                    {
                        if (!is_set) pixels[(y * stride) + pixel_x] = 0xFFFFFFFF;
                        else         pixels[(y * stride) + pixel_x] = 0x00000000;
                    }
                    else
                    {
                        if (is_set)  pixels[(y * stride) + pixel_x] = 0xFFFFFFFF;
                        else         pixels[(y * stride) + pixel_x] = 0x00000000;
                    }
                }
            }
        }
    }

    SDL_UnlockSurface(surface);
    fclose(f);
    return surface;
}

static void BuildRuntimeNodeTexture(SDL_Renderer *renderer, XbmAssetNode *node, const char *path, const char *name)
{
    if (node->texture) SDL_DestroyTexture(node->texture);
    SDL_Surface *surf = LoadXbmFileToSurface(path, name);
    if (surf)
    {
        node->w = surf->w;
        node->h = surf->h;
        SDL_strlcpy(node->layer_name, name, sizeof(node->layer_name));
        node->texture = SDL_CreateTextureFromSurface(renderer, surf);
        if (node->texture)
        {
            SDL_SetTextureBlendMode(node->texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(node->texture, SDL_SCALEMODE_NEAREST);
            fprintf(stderr, "[DIAGNOSTIC OK] Dynamically Loaded Runtime Layer: %s (%dx%d)\n", name, node->w, node->h);
        }
        SDL_DestroySurface(surf);
    }
}

CatClock_XbmLibrary *CatClock_InitXbmLibrary(SDL_Renderer *renderer)
{
    fprintf(stderr, "[DIAGNOSTIC INFO] Initializing runtime filesystem XBM asset parser...\n");
    CatClock_XbmLibrary *lib = calloc(1, sizeof(CatClock_XbmLibrary));
    if (!lib) return NULL;

    BuildRuntimeNodeTexture(renderer, &lib->catback,  "./assets/catback.xbm",  "catback");
    BuildRuntimeNodeTexture(renderer, &lib->catwhite, "./assets/catwhite.xbm", "catwhite");
    BuildRuntimeNodeTexture(renderer, &lib->cattie,   "./assets/cattie.xbm",   "cattie");
    BuildRuntimeNodeTexture(renderer, &lib->eyes,     "./assets/eyes.xbm",     "eyes");
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
