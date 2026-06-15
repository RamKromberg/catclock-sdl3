/******************************************************************************
 * File Name:    catclock_assets.c
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
 * License: Open Source / Educational - Preserve attribution upon redistribution.
 *****************************************************************************/

// clang-format off
#include "catclock_shared.h"
#include "catclock_xbm.h"
// clang-format on
#include <string.h>

typedef struct {
	SDL_Texture* texture;
	int w;
	int h;
	char layer_name[16];
} AssetNode;

struct CatClock_XbmLibrary {
	AssetNode catbackground;
	AssetNode catoutline;
	AssetNode necktie;
	AssetNode eyesocket;
};

static void InitAssetNode(SDL_Renderer* renderer, AssetNode* node, const char* filepath,
						  const char* name, SDL_Color color, bool invert) {
	if (!node || !filepath || !name)
		return;

	node->texture = XbmUtil_LoadToRGBA4444(renderer, filepath, color, invert);
	if (node->texture) {
		float w, h;
		SDL_GetTextureSize(node->texture, &w, &h);
		node->w = (int) w;
		node->h = (int) h;
	}
	SDL_strlcpy(node->layer_name, name, sizeof(node->layer_name));
}

CatClock_XbmLibrary* CatClock_InitXbmLibrary(SDL_Renderer* renderer) {
	CatClock_XbmLibrary* lib = (CatClock_XbmLibrary*) SDL_calloc(1, sizeof(CatClock_XbmLibrary));
	if (!lib)
		return NULL;

	SDL_Color color_base = { 255, 255, 255, 255 };

	InitAssetNode(renderer, &lib->catbackground, "./assets/catback.xbm", "catback", color_base,
				  false);
	InitAssetNode(renderer, &lib->catoutline, "./assets/catwhite.xbm", "catwhite", color_base,
				  false);
	InitAssetNode(renderer, &lib->necktie, "./assets/cattie.xbm", "cattie", ctx.tie_color, false);
	InitAssetNode(renderer, &lib->eyesocket, "./assets/eyes.xbm", "eyes", color_base, false);

	return lib;
}

void CatClock_RenderXbmLayerOffset(CatClock_XbmLibrary* lib, SDL_Renderer* renderer,
								   const char* layer_id, SDL_Color color, float offset_x,
								   float offset_y) {
	if (!lib || !renderer || !layer_id)
		return;

	AssetNode* target = NULL;
	if (SDL_strcmp(layer_id, "catback") == 0)
		target = &lib->catbackground;
	else if (SDL_strcmp(layer_id, "catwhite") == 0)
		target = &lib->catoutline;
	else if (SDL_strcmp(layer_id, "cattie") == 0)
		target = &lib->necktie;
	else if (SDL_strcmp(layer_id, "eyes") == 0)
		target = &lib->eyesocket;

	if (target && target->texture) {
		SDL_SetTextureColorMod(target->texture, color.r, color.g, color.b);
		SDL_SetTextureAlphaMod(target->texture, color.a);

		SDL_FRect dst;
		if (SDL_strcmp(layer_id, "eyes") == 0) {
			dst.x = 49.0f + offset_x;
			dst.y = 30.0f + offset_y;
			dst.w = (float) target->w;
			dst.h = (float) target->h;
		} else {
			dst.x = offset_x;
			dst.y = offset_y;
			dst.w = BASELINE_CANVAS_W;
			dst.h = BASELINE_CANVAS_H;
		}
		SDL_RenderTexture(renderer, target->texture, NULL, &dst);
	}
}

void CatClock_RenderXbmLayer(CatClock_XbmLibrary* lib, SDL_Renderer* renderer, const char* layer_id,
							 SDL_Color color) {
	CatClock_RenderXbmLayerOffset(lib, renderer, layer_id, color, 0.0f, 0.0f);
}

void CatClock_RenderHaloOutline(CatClock_XbmLibrary* lib, SDL_Renderer* renderer, SDL_Color color) {
	if (!lib || !renderer || !lib->catbackground.texture)
		return;

	SDL_BlendMode protective_halo_mode = SDL_ComposeCustomBlendMode(
		SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
		SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);

	SDL_SetTextureBlendMode(lib->catbackground.texture, protective_halo_mode);
	SDL_SetTextureColorMod(lib->catbackground.texture, color.r, color.g, color.b);
	SDL_SetTextureAlphaMod(lib->catbackground.texture, color.a);

	for (float dx = -1.0f; dx <= 1.0f; dx += 1.0f) {
		for (float dy = -1.0f; dy <= 1.0f; dy += 1.0f) {
			if (dx == 0.0f && dy == 0.0f)
				continue;
			SDL_FRect dst = { dx, dy, BASELINE_CANVAS_W, BASELINE_CANVAS_H };
			SDL_RenderTexture(renderer, lib->catbackground.texture, NULL, &dst);
		}
	}

	SDL_SetTextureBlendMode(lib->catbackground.texture, SDL_BLENDMODE_BLEND);
}

void CatClock_DestroyXbmLibrary(CatClock_XbmLibrary* lib) {
	if (!lib)
		return;
	if (lib->catbackground.texture)
		SDL_DestroyTexture(lib->catbackground.texture);
	if (lib->catoutline.texture)
		SDL_DestroyTexture(lib->catoutline.texture);
	if (lib->necktie.texture)
		SDL_DestroyTexture(lib->necktie.texture);
	if (lib->eyesocket.texture)
		SDL_DestroyTexture(lib->eyesocket.texture);
	SDL_free(lib);
}

SDL_Texture* CatClock_GetXbmTextureLayer(CatClock_XbmLibrary* lib, const char* layer_id) {
	if (!lib || !layer_id)
		return NULL;
	if (strcmp(layer_id, "catback") == 0)
		return lib->catbackground.texture;
	if (strcmp(layer_id, "catwhite") == 0)
		return lib->catoutline.texture;
	if (strcmp(layer_id, "cattie") == 0)
		return lib->necktie.texture;
	if (strcmp(layer_id, "eyes") == 0)
		return lib->eyesocket.texture;
	return NULL;
}
