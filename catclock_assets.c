#include "catclock_shared.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Bounding box parameters matching specification definitions
#define CATBACK_WIDTH 101
#define CATBACK_HEIGHT 201
#define CATTIE_WIDTH 87
#define CATTIE_HEIGHT 20
#define TIE_OFFSET_X 9
#define TIE_OFFSET_Y 75
#define TIE_MASK_SIZE (CATTIE_WIDTH * CATTIE_HEIGHT)

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

// Global engine verification snapshot module matching the correct parameter signature
extern void CatClock_DebugDumpSingleLayerToDisk(const char* filename, const uint8_t* buffer,
												int width, int height);
extern Uint8* CatClock_LoadRawXbmBits(const char* filepath, int* out_w, int* out_h);

/**
 * CPU-side asset transformation pass utilizing dynamic heap-loaded XBM arrays.
 */
uint8_t* CatClock_PreBakeTieMask(const Uint8* raw_catback, const Uint8* raw_tie) {
	uint8_t* mask_buffer = (uint8_t*) malloc(TIE_MASK_SIZE);
	if (!mask_buffer) {
		fprintf(stderr, "[FATAL] Out of memory allocating 1740-byte tie mask buffer.\n");
		return NULL;
	}
	memset(mask_buffer, 0, TIE_MASK_SIZE);

	for (int y = 0; y < CATTIE_HEIGHT; y++) {
		for (int x = 0; x < CATTIE_WIDTH; x++) {
			int local_idx = (y * CATTIE_WIDTH) + x;

			// Step A: Extract tie bit from packed LSB-first stream
			int tie_byte_pos = (y * ((CATTIE_WIDTH + 7) / 8)) + (x / 8);
			int tie_bit_pos = x % 8;
			bool is_tie_fabric = (raw_tie[tie_byte_pos] & (1 << tie_bit_pos)) != 0;

			// Step B: Calculate projection site onto global catback coordinate canvas
			int target_x = x + TIE_OFFSET_X;
			int target_y = y + TIE_OFFSET_Y;

			int back_byte_pos = (target_y * ((CATBACK_WIDTH + 7) / 8)) + (target_x / 8);
			int back_bit_pos = target_x % 8;
			bool is_catback_outline = (raw_catback[back_byte_pos] & (1 << back_bit_pos)) != 0;

			// Step C: Polarity Trap Gate Intersection Pass
			if (is_tie_fabric && !is_catback_outline) {
				mask_buffer[local_idx] = 255; // Valid interior tie fabric opacity
			} else {
				mask_buffer[local_idx] = 0; // Transparent outline details and margin padding
			}
		}
	}

	return mask_buffer;
}

/**
 * Modernized XBM Initialization Library Hook.
 */
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

	/* 4. Pre-process the necktie asset to execute coordinate-aligned intersections */
	int raw_tie_w = 0, raw_tie_h = 0;
	Uint8* raw_tie_bits = CatClock_LoadRawXbmBits("./assets/cattie.xbm", &raw_tie_w, &raw_tie_h);

	if (raw_tie_bits && lib->catback_bits) {
		// Enforce boundary confirmation logic against 87x20 specifications
		if (raw_tie_w == CATTIE_WIDTH && raw_tie_h == CATTIE_HEIGHT) {
			lib->tie_body_bits = CatClock_PreBakeTieMask(lib->catback_bits, raw_tie_bits);

			// Set the dimension tracking registers inside the library context to true trimmed size
			lib->tie_body_w = CATTIE_WIDTH; // Set cleanly to 87
			lib->tie_body_h = CATTIE_HEIGHT; // Set cleanly to 20

			// Keep the fallback line bits tracking structure fully synchronized to the new bounds
			lib->tie_line_bits = (Uint8*) SDL_calloc(1, TIE_MASK_SIZE);
			if (lib->tie_body_bits && lib->tie_line_bits) {
				memcpy(lib->tie_line_bits, lib->tie_body_bits, TIE_MASK_SIZE);
				lib->tie_line_w = CATTIE_WIDTH;
				lib->tie_line_h = CATTIE_HEIGHT;
			}
		} else {
			fprintf(
				stderr,
				"[ERROR] Trimmed bounding dimensions mismatch on cattie.xbm. Expected 87x20.\n");
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
	if (lib->tie_body_bits)
		free(lib->tie_body_bits); // Managed through free() heap layout
	if (lib->tie_line_bits)
		free(lib->tie_line_bits);
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
