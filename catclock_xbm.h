/******************************************************************************
 * File Name:    catclock_xbm.h
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

#ifndef CATCLOCK_XBM_H
#define CATCLOCK_XBM_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * Packs raw 1-bit XBM data lines straight into a 16-bit RGBA4444 texture.
 * Returns a valid SDL_Texture pointer on success, or NULL on failure.
 */
static inline SDL_Texture* XbmUtil_LoadToRGBA4444(SDL_Renderer* renderer, const char* filepath,
												  SDL_Color color, bool invert_mask,
												  bool as_color_mask) {
	if (!renderer || !filepath)
		return NULL;

	SDL_IOStream* file = SDL_IOFromFile(filepath, "r");
	if (!file) {
		SDL_Log("XBM Error: Could not open file path: %s", filepath);
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
	unsigned char* raw_bits = (unsigned char*) SDL_calloc(1, total_bytes);
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
		raw_bits[i] = (unsigned char) SDL_strtol(ptr, &ptr, 16);
	}
	SDL_free(buffer);

	int pitch = w * sizeof(Uint16);
	Uint16* staging_pixels = (Uint16*) SDL_malloc(pitch * h);
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

			if (invert_mask) {
				bit_active = !bit_active;
			}

			if (as_color_mask) {
				// ============================================================
				// SYSTEM 1: THE TIE COLOR MASK
				// ============================================================
				if (bit_active) {
					// Active Fabric Shape: Turn on every color channel to make it solid white.
					staging_pixels[idx] = 0xFFFF;
				} else {
					// Folds & Outside Corners: Turn off all channels to make it pure clear space.
					staging_pixels[idx] = 0x0000;
				}
			} else {
				// ============================================================
				// SYSTEM 2: STANDARD BACKGROUND TRANSPARENCY
				// ============================================================
				if (bit_active) {
					// Pack the default color channels natively for body assets
					Uint16 r_channel = ((color.r >> 4) & 0x0F) << 12;
					Uint16 g_channel = ((color.g >> 4) & 0x0F) << 8;
					Uint16 b_channel = ((color.b >> 4) & 0x0F) << 4;
					Uint16 a_channel = 0x000F; // Solid Alpha

					staging_pixels[idx] = r_channel | g_channel | b_channel | a_channel;
				} else {
					staging_pixels[idx] = 0x0000; // Empty clear space
				}
			}
		}
	}
	SDL_free(raw_bits);

	SDL_Texture* tex
		= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA4444, SDL_TEXTUREACCESS_STATIC, w, h);
	if (tex) {
		SDL_UpdateTexture(tex, NULL, staging_pixels, pitch);
		SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
	}

	SDL_free(staging_pixels);
	return tex;
}

#endif // CATCLOCK_XBM_H
