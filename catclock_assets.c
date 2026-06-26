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

static void SoftBlitSilhouetteBits(uint8_t* dest, int dest_w, const uint8_t* src, int src_w,
								   int src_h, int offset_x, int offset_y) {
	int dest_stride = (dest_w + 7) / 8;
	int src_stride = (src_w + 7) / 8;

	for (int y = 0; y < src_h; y++) {
		int target_y = y + offset_y;
		for (int x = 0; x < src_w; x++) {
			int target_x = x + offset_x;

			// Extract single bit data matching LSB-first XBM alignments
			int src_byte = src[(y * src_stride) + (x / 8)];
			int src_bit = (src_byte >> (x % 8)) & 1;

			if (src_bit) {
				int dest_byte_idx = (target_y * dest_stride) + (target_x / 8);
				int dest_bit_pos = target_x % 8;

				// Bitwise OR aggregation pass to build the master mask template
				dest[dest_byte_idx] |= (1 << dest_bit_pos);
			}
		}
	}
}

CatClock_XbmLibrary* CatClock_InitXbmLibrary(SDL_Renderer* renderer) {
	(void) renderer;
	// Switch to standard calloc
	CatClock_XbmLibrary* lib = (CatClock_XbmLibrary*) calloc(1, sizeof(CatClock_XbmLibrary));
	if (!lib)
		return NULL;

	lib->catback_bits
		= CatClock_LoadRawXbmBits("./assets/catback.xbm", &lib->catback_w, &lib->catback_h);
	lib->catwhite_bits
		= CatClock_LoadRawXbmBits("./assets/catwhite.xbm", &lib->catwhite_w, &lib->catwhite_h);
	lib->eyes_bits = CatClock_LoadRawXbmBits("./assets/eyes.xbm", &lib->eyes_w, &lib->eyes_h);

	int tie_w = 0, tie_h = 0;
	lib->tie_body_bits = CatClock_LoadRawXbmBits("./assets/cattie.xbm", &tie_w, &tie_h);
	lib->tie_body_w = tie_w;
	lib->tie_body_h = tie_h;

	int sil_stride = (101 + 7) / 8;
	size_t sil_alloc_bytes = sil_stride * 201;
	// Core master bit canvas moved to native calloc
	ctx.master_silhouette = (uint8_t*) calloc(1, sil_alloc_bytes);

	if (ctx.master_silhouette && lib->catback_bits) {
		SoftBlitSilhouetteBits(ctx.master_silhouette, 101, lib->catback_bits, lib->catback_w,
							   lib->catback_h, 0, 0);

		if (lib->catwhite_bits) {
			SoftBlitSilhouetteBits(ctx.master_silhouette, 101, lib->catwhite_bits, lib->catwhite_w,
								   lib->catwhite_h, 2, 7);
		}

		if (lib->tie_body_bits && lib->tie_body_w == 87 && lib->tie_body_h == 20) {
			SoftBlitSilhouetteBits(ctx.master_silhouette, 101, lib->tie_body_bits, lib->tie_body_w,
								   lib->tie_body_h, 9, 75);
		}
	}

	if (lib->eyes_bits && lib->eyes_w == 54 && lib->eyes_h == 23) {
		int eye_stride = (54 + 7) / 8;
		size_t eye_alloc_bytes = eye_stride * 23;
		// Clean eye mask moved to native malloc
		ctx.clean_eye_mask = (uint8_t*) malloc(eye_alloc_bytes);

		if (ctx.clean_eye_mask) {
			// Standard string function library execution pass
			memcpy(ctx.clean_eye_mask, lib->eyes_bits, eye_alloc_bytes);

			int patch_rows[] = { 21, 21, 21, 22, 22, 22, 22 };
			int patch_cols[] = { 24, 26, 28, 23, 25, 27, 29 };
			int patch_count = 7;

			for (int i = 0; i < patch_count; i++) {
				int r = patch_rows[i];
				int c = patch_cols[i];
				int target_byte = (r * eye_stride) + (c / 8);
				int target_bit = c % 8;

				ctx.clean_eye_mask[target_byte] |= (1 << target_bit);
			}

			SDL_Log("[Diag] Printing Patched 1-Bit Dual-Cavity Eye Mask to Terminal:");
			CatClock_DebugPrintXbmToTerminal("Clean Eye Mask", ctx.clean_eye_mask, 54, 23);
		}
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

	// Release Stage 1 CPU buffers natively
	if (ctx.master_silhouette) {
		free(ctx.master_silhouette);
		ctx.master_silhouette = NULL;
	}
	if (ctx.clean_eye_mask) {
		free(ctx.clean_eye_mask);
		ctx.clean_eye_mask = NULL;
	}

	// Release baseline assets natively
	if (lib->catback_bits)
		free(lib->catback_bits);
	if (lib->catwhite_bits)
		free(lib->catwhite_bits);
	if (lib->tie_body_bits)
		free(lib->tie_body_bits);
	if (lib->eyes_bits)
		free(lib->eyes_bits);

	// Dismantle structural library wrapper shell context
	free(lib);
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

void CatClock_ExecuteScaleDependentEdgeDilation(float current_scale) {
	// Lock logic out from running on identical scale thresholds to save CPU cycles
	if (current_scale == ctx.last_applied_halo_scale || !ctx.master_silhouette)
		return;

	int base_w = 101;
	int base_h = 201;

	// Direct Snapping Dimension Snap Rules
	int scaled_w = (int) ((float) base_w * current_scale);
	int scaled_h = (int) ((float) base_h * current_scale);
	if (scaled_w <= 0 || scaled_h <= 0)
		return;

	size_t canvas_bytes = (size_t) (scaled_w * scaled_h);

	// Directive 2: Sized to exact integer dimensions via native heap buffers
	uint8_t* scaled_silhouette = (uint8_t*) calloc(1, canvas_bytes);
	uint8_t* halo_buffer = (uint8_t*) calloc(1, canvas_bytes);
	if (!scaled_silhouette || !halo_buffer) {
		if (scaled_silhouette)
			free(scaled_silhouette);
		if (halo_buffer)
			free(halo_buffer);
		return;
	}

	int src_stride = (base_w + 7) / 8;

	// Step A: Scaling Interpolation (Map 1-bit master onto integer scaled canvas)
	for (int y = 0; y < scaled_h; y++) {
		int src_y = (int) ((float) y / current_scale);
		if (src_y >= base_h)
			src_y = base_h - 1;

		for (int x = 0; x < scaled_w; x++) {
			int src_x = (int) ((float) x / current_scale);
			if (src_x >= base_w)
				src_x = base_w - 1;

			int byte_idx = (src_y * src_stride) + (src_x / 8);
			int bit_pos = src_x % 8;
			int bit_val = (ctx.master_silhouette[byte_idx] >> bit_pos) & 1;

			if (bit_val) {
				scaled_silhouette[(y * scaled_w) + x] = 1;
			}
		}
	}

	// Directive 3: Execute 1-Pixel Clean Edge Dilation Pass
	for (int y = 0; y < scaled_h; y++) {
		for (int x = 0; x < scaled_w; x++) {
			// Check only empty background pixels
			if (scaled_silhouette[(y * scaled_w) + x] == 0) {
				bool adjacent_to_foreground = false;

				// 4-way orthogonal neighborhood kernel sweep
				int dx[] = { 0, 0, -1, 1 };
				int dy[] = { -1, 1, 0, 0 };

				for (int i = 0; i < 4; i++) {
					int nx = x + dx[i];
					int ny = y + dy[i];

					if (nx >= 0 && nx < scaled_w && ny >= 0 && ny < scaled_h) {
						if (scaled_silhouette[(ny * scaled_w) + nx] == 1) {
							adjacent_to_foreground = true;
							break;
						}
					}
				}

				if (adjacent_to_foreground) {
					// Turn on pixel to full R8 scale color density limit indicator
					halo_buffer[(y * scaled_w) + x] = 255;
				}
			}
		}
	}

	// Directive 4: Stream Transfer via single upload push block
	sg_image_data stream_payload = { 0 };
	stream_payload.subimage[0][0].ptr = halo_buffer;
	stream_payload.subimage[0][0].size = canvas_bytes;
	sg_update_image(ctx.cat_halo_img, &stream_payload);

	// Free the temporary staging canvas immediately
	free(scaled_silhouette);
	free(halo_buffer);

	// Cache current scale configuration checkpoint metrics
	ctx.last_applied_halo_scale = current_scale;
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
