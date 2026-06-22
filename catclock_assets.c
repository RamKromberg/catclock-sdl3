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
#include <math.h>
#include <string.h>

typedef struct {
	SDL_Texture* texture;
	int w;
	int h;
	char layer_name[64];
} AssetNode;

struct CatClock_XbmLibrary {
	AssetNode catbackground;
	AssetNode catoutline;
	AssetNode necktie;
	AssetNode eyesocket;
};

static void InitAssetNode(SDL_Renderer* renderer, AssetNode* node, const char* filepath,
						  const char* name, SDL_Color color, bool invert, bool as_color_mask) {
	if (!node || !filepath || !name)
		return;
	// Forward the flag cleanly to the updated inline header utility function
	node->texture = XbmUtil_LoadToRGBA4444(renderer, filepath, color, invert, as_color_mask);
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
				  false, false);
	InitAssetNode(renderer, &lib->catoutline, "./assets/catwhite.xbm", "catwhite", color_base,
				  false, false);

	InitAssetNode(renderer, &lib->necktie, "./assets/cattie.xbm", "cattie", ctx.tie_color, false,
				  true);

	InitAssetNode(renderer, &lib->eyesocket, "./assets/eyes.xbm", "eyes", color_base, false, false);

	// Seed plain C software arrays into system RAM to enable seamless software masking
	ctx.software_eyes_bitmask
		= CatClock_LoadRawXbmBits("./assets/eyes.xbm", &ctx.software_mask_w, &ctx.software_mask_h);

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
		SDL_FRect dst;
		float runtime_scale = ctx.current_scale;

		if (SDL_strcmp(layer_id, "eyes") == 0) {
			dst.x = floorf((20.0f + offset_x) * runtime_scale);
			dst.y = floorf((16.0f + offset_y) * runtime_scale);
			dst.w = (float) target->w * runtime_scale;
			dst.h = (float) target->h * runtime_scale;

			SDL_SetTextureColorMod(target->texture, color.r, color.g, color.b);
			SDL_SetTextureAlphaMod(target->texture, color.a);
			SDL_RenderTexture(renderer, target->texture, NULL, &dst);
		} else if (SDL_strcmp(layer_id, "cattie") == 0) {
			// Establish rendering bounds matching the necktie dimension metrics
			dst.x = floorf((9.0f + offset_x) * runtime_scale);
			dst.y = floorf((75.0f + offset_y) * runtime_scale);
			dst.w = (float) target->w * runtime_scale;
			dst.h = (float) target->h * runtime_scale;

			// PASS 1: Blit the background cat body slice first
			if (lib->catbackground.texture) {
				SDL_FRect back_src = { 9.0f, 75.0f, (float) target->w, (float) target->h };

				SDL_SetTextureColorMod(lib->catbackground.texture, ctx.cat_color.r, ctx.cat_color.g,
									   ctx.cat_color.b);
				SDL_SetTextureAlphaMod(lib->catbackground.texture, 255);
				SDL_SetTextureBlendMode(lib->catbackground.texture, SDL_BLENDMODE_BLEND);
				SDL_RenderTexture(renderer, lib->catbackground.texture, &back_src, &dst);
			}

			// PASS 2: Tint selection using standard alpha blending
			// Instead of standard modulation over transparency, use standard source alpha blending
			// This forces transparent pixels (0x0000) to safely discard color values on the edges
			SDL_SetTextureColorMod(target->texture, color.r, color.g, color.b);
			SDL_SetTextureAlphaMod(target->texture, color.a);
			SDL_SetTextureBlendMode(target->texture, SDL_BLENDMODE_BLEND);
			SDL_RenderTexture(renderer, target->texture, NULL, &dst);
			return;
		} else {
			dst.x = floorf(offset_x * runtime_scale);
			dst.y = floorf(offset_y * runtime_scale);
			dst.w = ceilf(BASELINE_CANVAS_W * runtime_scale);
			dst.h = ceilf(BASELINE_CANVAS_H * runtime_scale);

			SDL_SetTextureColorMod(target->texture, color.r, color.g, color.b);
			SDL_SetTextureAlphaMod(target->texture, color.a);
			SDL_RenderTexture(renderer, target->texture, NULL, &dst);
		}
	}
}

void CatClock_RenderXbmLayer(CatClock_XbmLibrary* lib, SDL_Renderer* renderer, const char* layer_id,
							 SDL_Color color) {
	float render_pad_x = ctx.use_decorations ? CHOP_OFFSET_X : 0.0f;
	float render_pad_y = ctx.use_decorations ? CHOP_OFFSET_Y : 0.0f;
	float visual_pad = ctx.use_decorations ? 0.0f : 1.0f;

	CatClock_RenderXbmLayerOffset(lib, renderer, layer_id, color, render_pad_x + visual_pad,
								  render_pad_y + visual_pad);
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

	float render_pad_x = ctx.use_decorations ? CHOP_OFFSET_X : 0.0f;
	float render_pad_y = ctx.use_decorations ? CHOP_OFFSET_Y : 0.0f;
	float visual_pad = ctx.use_decorations ? 0.0f : 1.0f;

	float runtime_scale = ctx.current_scale;

	for (float dx = -1.0f; dx <= 1.0f; dx += 1.0f) {
		for (float dy = -1.0f; dy <= 1.0f; dy += 1.0f) {
			if (dx == 0.0f && dy == 0.0f)
				continue;

			SDL_FRect dst
				= { floorf((render_pad_x + visual_pad + dx) * runtime_scale),
					floorf((render_pad_y + visual_pad + dy) * runtime_scale),
					BASELINE_CANVAS_W * runtime_scale, BASELINE_CANVAS_H * runtime_scale };
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
	if (ctx.software_eyes_bitmask) {
		SDL_free(ctx.software_eyes_bitmask);
		ctx.software_eyes_bitmask = NULL;
	}
	SDL_free(lib);
}

SDL_Texture* CatClock_GetXbmTextureLayer(CatClock_XbmLibrary* lib, const char* layer_id) {
	if (!lib || !layer_id)
		return NULL;
	if (SDL_strcmp(layer_id, "catback") == 0)
		return lib->catbackground.texture;
	if (SDL_strcmp(layer_id, "catwhite") == 0)
		return lib->catoutline.texture;
	if (SDL_strcmp(layer_id, "cattie") == 0)
		return lib->necktie.texture;
	if (SDL_strcmp(layer_id, "eyes") == 0)
		return lib->eyesocket.texture;
	return NULL;
}

/**
 * Stateless Monochrome XBM Core Parser Utility
 * Parses raw 1-bit text representations directly into contiguous system RAM buffers
 */
uint8_t* CatClock_LoadRawXbmBits(const char* filepath, int* out_w, int* out_h) {
	if (!filepath || !out_w || !out_h)
		return NULL;

	SDL_IOStream* file = SDL_IOFromFile(filepath, "r");
	if (!file) {
		SDL_Log("XBM Error: Could not open bitmap file path: %s", filepath);
		return NULL;
	}

	Sint64 file_size = SDL_GetIOSize(file);
	if (file_size <= 0) {
		SDL_CloseIO(file);
		return NULL;
	}

	char* buffer = (char*) SDL_malloc(file_size + 1);
	if (!buffer) {
		SDL_CloseIO(file);
		return NULL;
	}

	SDL_ReadIO(file, buffer, file_size);
	buffer[file_size] = '\0';
	SDL_CloseIO(file);

	int w = 0, h = 0;
	char* ptr = SDL_strstr(buffer, "_width");
	if (ptr) {
		while (*ptr && !SDL_isdigit((unsigned char) *ptr))
			ptr++;
		w = SDL_atoi(ptr);
	}

	ptr = SDL_strstr(buffer, "_height");
	if (ptr) {
		while (*ptr && !SDL_isdigit((unsigned char) *ptr))
			ptr++;
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
	uint8_t* raw_bits = (uint8_t*) SDL_calloc(1, total_bytes);
	if (!raw_bits) {
		SDL_free(buffer);
		return NULL;
	}

	for (int i = 0; i < total_bytes; i++) {
		while (*ptr && *ptr != '0' && *(ptr + 1) != 'x' && *(ptr + 1) != 'X') {
			if (*ptr == '}')
				break;
			ptr++;
		}
		if (*ptr == '}')
			break;
		raw_bits[i] = (uint8_t) SDL_strtol(ptr, &ptr, 16);
	}

	SDL_free(buffer);
	*out_w = w;
	*out_h = h;
	return raw_bits;
}
