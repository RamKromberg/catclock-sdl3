#include "catclock_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct CatClock_XbmLibrary {
	Uint8* catback_bits;
	int catback_w, catback_h;
	Uint8* catwhite_bits;
	int catwhite_w, catwhite_h;
	Uint8* tie_body_bits;
	int tie_body_w, tie_body_h;
	Uint8* tie_line_bits;
	int tie_line_w, tie_line_h;
	Uint8* hitbox_bits;
	int hitbox_w, hitbox_h;
	Uint8* eyes_bits;
	int eyes_w, eyes_h;
};

void CatClock_DebugPrintXbmToTerminal(const char* label, const Uint8* bits, int w, int h);

CatClock_XbmLibrary* CatClock_InitXbmLibrary(SDL_Renderer* renderer) {
	(void) renderer;
	CatClock_XbmLibrary* lib = (CatClock_XbmLibrary*) SDL_calloc(1, sizeof(CatClock_XbmLibrary));
	if (!lib)
		return NULL;

	/* 1. Core Background Character Outlines & Fill Splines */
	lib->catback_bits
		= CatClock_LoadRawXbmBits("./assets/catback.xbm", &lib->catback_w, &lib->catback_h);
	lib->catwhite_bits
		= CatClock_LoadRawXbmBits("./assets/catwhite.xbm", &lib->catwhite_w, &lib->catwhite_h);

	/* 2. Centralized Pupil Clipping Mask Configuration */
	lib->eyes_bits = CatClock_LoadRawXbmBits("./assets/eyes.xbm", &lib->eyes_w, &lib->eyes_h);

	/* 3. Centralized High-Performance Input Boundary Mask Configuration */
	printf("[Trace] Loading hitbox...\n");
	lib->hitbox_bits
		= CatClock_LoadRawXbmBits("./assets/hitbox.xbm", &lib->hitbox_w, &lib->hitbox_h);
	if (!lib->hitbox_bits) {
		printf("[Warning] Failed to load hitbox map. Full window drag fallback enabled.\n");
	}

	/* 4. Pre-process the necktie asset to split body fill spaces from structural lines */
	int raw_tie_w = 0, raw_tie_h = 0;
	Uint8* raw_tie_bits = CatClock_LoadRawXbmBits("./assets/cattie.xbm", &raw_tie_w, &raw_tie_h);

	if (raw_tie_bits) {
		int total_bytes = ((raw_tie_w + 7) / 8) * raw_tie_h;
		lib->tie_body_bits = (Uint8*) SDL_calloc(1, total_bytes);
		lib->tie_line_bits = (Uint8*) SDL_calloc(1, total_bytes);
		lib->tie_body_w = lib->tie_line_w = raw_tie_w;
		lib->tie_body_h = lib->tie_line_h = raw_tie_h;

		for (int i = 0; i < total_bytes; i++) {
			Uint8 raw_byte = raw_tie_bits[i];
			/* Split bits based on fill properties */
			lib->tie_line_bits[i] = raw_byte;
			lib->tie_body_bits[i] = ~raw_byte;
		}
		SDL_free(raw_tie_bits);
	}
	return lib;
}

void CatClock_GetCatbackData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h) {
	if (!lib) {
		*bits = NULL;
		*w = 0;
		*h = 0;
		return;
	}
	*bits = lib->catback_bits;
	*w = lib->catback_w;
	*h = lib->catback_h;
}

void CatClock_GetCatwhiteData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h) {
	if (!lib) {
		*bits = NULL;
		*w = 0;
		*h = 0;
		return;
	}
	*bits = lib->catwhite_bits;
	*w = lib->catwhite_w;
	*h = lib->catwhite_h;
}

void CatClock_GetCattieBodyData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h) {
	if (!lib) {
		*bits = NULL;
		*w = 0;
		*h = 0;
		return;
	}
	*bits = lib->tie_body_bits;
	*w = lib->tie_body_w;
	*h = lib->tie_body_h;
}

void CatClock_GetCattieLineData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h) {
	if (!lib) {
		*bits = NULL;
		*w = 0;
		*h = 0;
		return;
	}
	*bits = lib->tie_line_bits;
	*w = lib->tie_line_w;
	*h = lib->tie_line_h;
}

void CatClock_GetHitboxData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h) {
	if (!lib) {
		*bits = NULL;
		*w = 0;
		*h = 0;
		return;
	}
	*bits = lib->hitbox_bits;
	*w = lib->hitbox_w;
	*h = lib->hitbox_h;
}

void CatClock_GetEyesMaskData(CatClock_XbmLibrary* lib, Uint8** bits, int* w, int* h) {
	if (!lib) {
		*bits = NULL;
		*w = 0;
		*h = 0;
		return;
	}
	*bits = lib->eyes_bits;
	*w = lib->eyes_w;
	*h = lib->eyes_h;
}

void CatClock_DestroyXbmLibrary(CatClock_XbmLibrary* lib) {
	if (!lib)
		return;
	SDL_free(lib->catback_bits);
	SDL_free(lib->catwhite_bits);
	SDL_free(lib->tie_body_bits);
	SDL_free(lib->tie_line_bits);
	SDL_free(lib->hitbox_bits);
	SDL_free(lib->eyes_bits);
	SDL_free(lib);
}

Uint8* CatClock_LoadRawXbmBits(const char* filepath, int* out_w, int* out_h) {
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
	Uint8* raw_bits = (Uint8*) SDL_calloc(1, total_bytes);
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
		raw_bits[i] = (Uint8) SDL_strtol(ptr, &ptr, 16);
	}

	SDL_free(buffer);
	*out_w = w;
	*out_h = h;
	return raw_bits;
}

void CatClock_RenderXbmLayer(CatClock_XbmLibrary* lib, SDL_Renderer* renderer, const char* layer_id,
							 SDL_Color color) {
	(void) lib;
	(void) renderer;
	(void) layer_id;
	(void) color;
}
void CatClock_RenderXbmLayerOffset(CatClock_XbmLibrary* lib, SDL_Renderer* renderer,
								   const char* layer_id, SDL_Color color, float offset_x,
								   float offset_y) {
	(void) lib;
	(void) renderer;
	(void) layer_id;
	(void) color;
	(void) offset_x;
	(void) offset_y;
}
void CatClock_RenderHaloOutline(CatClock_XbmLibrary* lib, SDL_Renderer* renderer, SDL_Color color) {
	(void) lib;
	(void) renderer;
	(void) color;
}
SDL_Texture* CatClock_GetXbmTextureLayer(CatClock_XbmLibrary* lib, const char* layer_id) {
	(void) lib;
	(void) layer_id;
	return NULL;
}
void CatClock_DebugPrintXbmToTerminal(const char* label, const Uint8* bits, int w, int h) {
	if (!bits || w <= 0 || h <= 0) {
		printf("\n[XBM Debug] %s: Cannot print (Empty or NULL array)\n", label);
		return;
	}

	printf("\n=== XBM TERMINAL PRINT: %s (%dx%d) ===\n", label, w, h);
	int bytes_per_row = (w + 7) / 8;

	for (int y = 0; y < h; y++) {
		// Print line numbers on the left side as a visual scale track
		printf("%03d | ", y);

		for (int x = 0; x < w; x++) {
			int byte_idx = (y * bytes_per_row) + (x / 8);
			int bit_pos = x % 8;

			// Check if the bit is flipped on (1-bit asset inverted check)
			// In XBM, a 1-bit indicates foreground/ink, 0 indicates canvas
			if ((bits[byte_idx] & (1 << bit_pos)) != 0) {
				printf("██"); // Filled block character for solid ink
			} else {
				printf("  "); // Two spaces for empty canvas background
			}
		}
		printf("\n");
	}
	printf("=============================================\n\n");
}
