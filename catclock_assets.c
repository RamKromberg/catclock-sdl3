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

#include "catclock_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================================
   FOUNDATIONAL ASSET GEOMETRY & COORDINATE BLUEPRINTS
   ========================================================================== */
#define CATTIE_WIDTH 87
#define CATTIE_HEIGHT 20
#define TIE_OFFSET_X 9
#define TIE_OFFSET_Y 75
#define TIE_MASK_SIZE (CATTIE_WIDTH * CATTIE_HEIGHT)

struct CatClock_XbmLibrary {
	uint8_t* catback_bits;
	int catback_w, catback_h;
	uint8_t* catwhite_bits;
	int catwhite_w, catwhite_h;
	uint8_t* tie_body_bits;
	int tie_body_w, tie_body_h;
	uint8_t* tie_line_bits;
	int tie_line_w, tie_line_h;
	uint8_t* hitbox_bits;
	int hitbox_w, hitbox_h;
	uint8_t* eyes_bits;
	int eyes_w, eyes_h;
};

/* ==========================================================================
   DIAGNOSTIC VISUALIZATION & OUTPUT SUBSYSTEMS
   ========================================================================== */

void CatClock_DebugPrintXbmToTerminal(const char* label, const uint8_t* bits, int w, int h) {
	if (!bits || w <= 0 || h <= 0) {
		printf("\n[XBM Debug] %s: Cannot print (Empty or NULL array)\n", label);
		return;
	}
	printf("\n=== XBM TERMINAL PRINT: %s (%dx%d) ===\n", label, w, h);
	int bytes_per_row = (w + 7) / 8;
	for (int y = 0; y < h; y++) {
		printf("%03d | ", y);
		for (int x = 0; x < w; x++) {
			int byte_idx = (y * bytes_per_row) + (x / 8);
			if ((bits[byte_idx] & (1 << (x % 8))) != 0) {
				printf("##");
			} else {
				printf("  ");
			}
		}
		printf("\n");
	}
	printf("=====================================================\n\n");
}

void CatClock_DebugDumpSingleLayerToDisk(const char* filename, const uint8_t* buffer, int width,
										 int height) {
	if (!filename || !buffer)
		return;
	FILE* pgm_file = fopen(filename, "wb");
	if (!pgm_file)
		return;
	fprintf(pgm_file, "P5\n%d %d\n255\n", width, height);
	for (int i = 0; i < (width * height); i++) {
		uint8_t val = buffer[i];
		uint8_t enhanced_pixel = (val == 0) ? 0 : ((val == 255) ? 255 : (val * 30));
		fputc(enhanced_pixel, pgm_file);
	}
	fclose(pgm_file);
}

void CatClock_DebugDumpPamToDisk(const char* filepath, const uint8_t* material_grid, int w, int h) {
	FILE* f = fopen(filepath, "wb");
	if (!f)
		return;

	fprintf(f, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n", w, h);

	for (int i = 0; i < w * h; i++) {
		uint8_t token = material_grid[i];
		uint8_t r = 0, g = 0, b = 0, a = 255;

		if (token == 0x33) {
			r = 255;
			g = 255;
			b = 255;
		} else if (token == 0x66) {
			r = 255;
			g = 0;
			b = 0;
		} else if (token == 0x99) {
			r = 0;
			g = 0;
			b = 0;
		} else if (token == 0x55) {
			r = 255;
			g = 255;
			b = 0; /* High Contrast Target Yellow */
		} else {
			a = 0;
		}
		fputc(r, f);
		fputc(g, f);
		fputc(b, f);
		fputc(a, f);
	}
	fclose(f);
}

void CatClock_DebugDumpGenericAtlasToPam(const char* filepath, const uint8_t* raw_buffer, int w,
										 int h) {
	FILE* f = fopen(filepath, "wb");
	if (!f)
		return;

	fprintf(f, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n", w, h);

	for (int i = 0; i < w * h; i++) {
		uint8_t token = raw_buffer[i];
		uint8_t r = 0, g = 0, b = 0, a = 255;

		if (token == PALETTE_CAT_WHITE || token == PALETTE_HALO) {
			r = 255;
			g = 255;
			b = 255;
		} else if (token == PALETTE_NECKTIE || token == PALETTE_HAND_SECOND) {
			r = 255;
			g = 0;
			b = 0;
		} else if (token == PALETTE_CAT_BODY || token == PALETTE_HAND_HOUR
				   || token == PALETTE_HAND_MINUTE) {
			r = 0;
			g = 0;
			b = 0;
		} else if (token == PALETTE_PUPIL) {
			r = 0;
			g = 255;
			b = 0;
		} else if (token == PALETTE_SCLERA) {
			r = 255;
			g = 255;
			b = 0;
		} else {
			a = 0;
		}
		fputc(r, f);
		fputc(g, f);
		fputc(b, f);
		fputc(a, f);
	}
	fclose(f);
}

/* ==========================================================================
   TRUE TEXT-PARSING XBM ASSET LOADER ENGINE
   ========================================================================== */

uint8_t* CatClock_LoadRawXbmBits(const char* filepath, int* out_w, int* out_h) {
	SDL_IOStream* file = SDL_IOFromFile(filepath, "r");
	if (!file) {
		SDL_Log("XBM Error: Could not open bitmap file path: %s", filepath);
		*out_w = 0;
		*out_h = 0;
		return NULL;
	}

	Sint64 file_size = SDL_GetIOSize(file);
	if (file_size <= 0) {
		SDL_CloseIO(file);
		*out_w = 0;
		*out_h = 0;
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
		*out_w = 0;
		*out_h = 0;
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

uint8_t* CatClock_PreBakeTieMask(const uint8_t* raw_catback, const uint8_t* raw_tie) {
	(void) raw_catback;
	uint8_t* mask_buffer = (uint8_t*) malloc(TIE_MASK_SIZE);
	if (!mask_buffer)
		return NULL;
	memset(mask_buffer, 0, TIE_MASK_SIZE);

	int stride_tie = (CATTIE_WIDTH + 7) / 8;

	for (int y = 0; y < CATTIE_HEIGHT; y++) {
		for (int x = 0; x < CATTIE_WIDTH; x++) {
			int local_idx = (y * CATTIE_WIDTH) + x;

			int tie_byte_pos = (y * stride_tie) + (x / 8);
			bool is_tie_fabric = (raw_tie[tie_byte_pos] & (1 << (x % 8))) != 0;

			if (is_tie_fabric) {
				mask_buffer[local_idx] = 255;
			}
		}
	}
	return mask_buffer;
}

static void SoftBlitSilhouetteBits(uint8_t* dest, int dest_w, int dest_h, const uint8_t* src,
								   int src_w, int src_h, int offset_x, int offset_y) {
	int dest_stride = (dest_w + 7) / 8;
	int src_stride = (src_w + 7) / 8;
	for (int y = 0; y < src_h; y++) {
		int target_y = y + offset_y;
		if (target_y < 0 || target_y >= dest_h)
			continue;
		for (int x = 0; x < src_w; x++) {
			int target_x = x + offset_x;
			if (target_x < 0 || target_x >= dest_w)
				continue;
			int src_byte = src[(y * src_stride) + (x / 8)];
			if ((src_byte >> (x % 8)) & 1) {
				dest[(target_y * dest_stride) + (target_x / 8)] |= (1 << (target_x % 8));
			}
		}
	}
}

/**
 * CatClock_BakeUnscaledMaterialIDStaging
 * Restored line-for-line from your exact repository baseline rules.
 */
void CatClock_BakeUnscaledMaterialIDStaging(uint8_t* target_buffer,
											struct CatClock_XbmLibrary* lib) {
	if (!lib || !lib->catback_bits)
		return;

	uint8_t* baked_tie_mask = CatClock_PreBakeTieMask(lib->catback_bits, lib->tie_body_bits);

	int stride_back = (ASSET_BODY_W + 7) / 8;
	int stride_white = (ASSET_BODY_W + 7) / 8;

	int white_dx = 1, white_dy = 6;
	int tie_dx = TIE_OFFSET_X;
	int tie_dy = TIE_OFFSET_Y;

	for (int y = 0; y < lib->catback_h; y++) {
		for (int x = 0; x < lib->catback_w; x++) {
			uint8_t resolved_token = 0x00;

			int back_bit = (lib->catback_bits[(y * stride_back) + (x / 8)] >> (x % 8)) & 1;

			int white_x = x - white_dx;
			int white_y = y - white_dy;
			int white_bit = 0;
			if (lib->catwhite_bits && white_x >= 0 && white_x < lib->catwhite_w && white_y >= 0
				&& white_y < lib->catwhite_h) {
				white_bit = (lib->catwhite_bits[(white_y * stride_white) + (white_x / 8)]
							 >> (white_x % 8))
					& 1;
			}

			int tie_x = x - tie_dx;
			int tie_y = y - tie_dy;
			int tie_bit = 0;
			if (baked_tie_mask && tie_x >= 0 && tie_x < CATTIE_WIDTH && tie_y >= 0
				&& tie_y < CATTIE_HEIGHT) {
				tie_bit = (baked_tie_mask[(tie_y * CATTIE_WIDTH) + tie_x] == 255) ? 1 : 0;
			}

			if (white_bit) {
				resolved_token = 0x33;
			} else if (tie_bit) {
				resolved_token = 0x66;
			} else if (back_bit) {
				resolved_token = 0x99;
			}

			if (ctx.clean_eye_mask && x >= 25 && x < 79 && y >= 20 && y < 43) {
				int ex = x - 25;
				int ey = y - 20;
				int eye_bit
					= (ctx.clean_eye_mask[(ey * ((54 + 7) / 8)) + (ex / 8)] >> (ex % 8)) & 1;
				if (eye_bit == 0) {
					resolved_token = 0x55;
				}
			}
			target_buffer[(y * lib->catback_w) + x] = resolved_token;
		}
	}

	if (baked_tie_mask) {
		free(baked_tie_mask);
	}
}

struct CatClock_XbmLibrary* CatClock_InitXbmLibrary(void* renderer) {
	(void) renderer;
	struct CatClock_XbmLibrary* lib
		= (struct CatClock_XbmLibrary*) calloc(1, sizeof(struct CatClock_XbmLibrary));
	if (!lib)
		return NULL;

	lib->catback_bits
		= CatClock_LoadRawXbmBits("./assets/catback.xbm", &lib->catback_w, &lib->catback_h);
	lib->catwhite_bits
		= CatClock_LoadRawXbmBits("./assets/catwhite.xbm", &lib->catwhite_w, &lib->catwhite_h);
	lib->eyes_bits = CatClock_LoadRawXbmBits("./assets/eyes.xbm", &lib->eyes_w, &lib->eyes_h);
	lib->hitbox_bits
		= CatClock_LoadRawXbmBits("./assets/hitbox.xbm", &lib->hitbox_w, &lib->hitbox_h);

	int tie_w = 0, tie_h = 0;
	lib->tie_body_bits = CatClock_LoadRawXbmBits("./assets/cattie.xbm", &tie_w, &tie_h);
	lib->tie_body_w = tie_w;
	lib->tie_body_h = tie_h;

	int sil_stride = (ASSET_BODY_W + 7) / 8;
	size_t sil_alloc_bytes = sil_stride * ASSET_BODY_H;

	/* Compute highest required layout row counts for mouse interaction layers */
	int max_hitbox_h = ASSET_BODY_H;
	if (lib->hitbox_bits && lib->hitbox_h > max_hitbox_h) {
		max_hitbox_h = lib->hitbox_h;
	}
	size_t hitbox_alloc_bytes = sil_stride * max_hitbox_h;

	ctx.master_silhouette = (uint8_t*) calloc(1, sil_alloc_bytes);
	ctx.hitbox_bits = (uint8_t*) calloc(1, hitbox_alloc_bytes);
	ctx.software_mask_w = ASSET_BODY_W;
	ctx.software_mask_h = max_hitbox_h;

	if (ctx.master_silhouette && lib->catback_bits) {
		SoftBlitSilhouetteBits(ctx.master_silhouette, ASSET_BODY_W, ASSET_BODY_H, lib->catback_bits,
							   lib->catback_w, lib->catback_h, 0, 0);

		if (lib->hitbox_bits) {
			SoftBlitSilhouetteBits(ctx.hitbox_bits, ASSET_BODY_W, max_hitbox_h, lib->hitbox_bits,
								   lib->hitbox_w, lib->hitbox_h, 0, 0);
		} else {
			SoftBlitSilhouetteBits(ctx.hitbox_bits, ASSET_BODY_W, max_hitbox_h, lib->catback_bits,
								   lib->catback_w, lib->catback_h, 0, 0);
		}

		if (lib->catwhite_bits)
			SoftBlitSilhouetteBits(ctx.master_silhouette, ASSET_BODY_W, ASSET_BODY_H,
								   lib->catwhite_bits, lib->catwhite_w, lib->catwhite_h, 1, 6);
		if (lib->tie_body_bits)
			SoftBlitSilhouetteBits(ctx.master_silhouette, ASSET_BODY_W, ASSET_BODY_H,
								   lib->tie_body_bits, lib->tie_body_w, lib->tie_body_h, 9, 75);
	}

	if (lib->eyes_bits && lib->eyes_w == 54 && lib->eyes_h == 23) {
		int eye_stride = (54 + 7) / 8;
		ctx.clean_eye_mask = (uint8_t*) malloc(eye_stride * 23);
		if (ctx.clean_eye_mask) {
			memcpy(ctx.clean_eye_mask, lib->eyes_bits, eye_stride * 23);
			int patch_rows[] = { 21, 21, 21, 22, 22, 22, 22 };
			int patch_cols[] = { 24, 26, 28, 23, 25, 27, 29 };
			for (int i = 0; i < 7; i++)
				ctx.clean_eye_mask[(patch_rows[i] * eye_stride) + (patch_cols[i] / 8)]
					|= (1 << (patch_cols[i] % 8));
		}
	}

	Diagnostics_LogAssetLifecycle("Main_Body_Silhouette", sil_alloc_bytes, ASSET_BODY_W,
								  ASSET_BODY_H);
	if (lib->eyes_bits)
		Diagnostics_LogAssetLifecycle("Appendage_Moving_Eyes", (size_t) ((54 + 7) / 8 * 23),
									  lib->eyes_w, lib->eyes_h);
	if (lib->hitbox_bits)
		Diagnostics_LogAssetLifecycle("Window_Shaped_Hitbox", hitbox_alloc_bytes, lib->hitbox_w,
									  lib->hitbox_h);

	return lib;
}

void CatClock_GetCatbackData(struct CatClock_XbmLibrary* lib, uint8_t** bits, int* w, int* h) {
	if (lib) {
		*bits = lib->catback_bits;
		*w = lib->catback_w;
		*h = lib->catback_h;
	}
}
void CatClock_GetCatwhiteData(struct CatClock_XbmLibrary* lib, uint8_t** bits, int* w, int* h) {
	if (lib) {
		*bits = lib->catwhite_bits;
		*w = lib->catwhite_w;
		*h = lib->catwhite_h;
	}
}
void CatClock_GetCattieBodyData(struct CatClock_XbmLibrary* lib, uint8_t** bits, int* w, int* h) {
	if (lib) {
		*bits = lib->tie_body_bits;
		*w = lib->tie_body_w;
		*h = lib->tie_body_h;
	}
}
void CatClock_GetHitboxData(struct CatClock_XbmLibrary* lib, uint8_t** bits, int* w, int* h) {
	if (lib) {
		*bits = lib->hitbox_bits;
		*w = lib->hitbox_w;
		*h = lib->hitbox_h;
	}
}
void CatClock_DestroyXbmLibrary(struct CatClock_XbmLibrary* lib) {
	if (!lib)
		return;
	free(lib);
}
/* =========================================================================
   CORE TELEMETRY MONITOR CHANNELS SUB-SYSTEM IMPLEMENTATIONS
   ========================================================================= */
void Diagnostics_LogAssetLifecycle(const char* asset_name, size_t buffer_size, int dimensions_w,
								   int dimensions_h) {
	printf("[Telemetry][Lifecycle] Asset: '%s' | Staging Allocation: %zu bytes | Grid Footprint: "
		   "%dx%d\n",
		   asset_name, buffer_size, dimensions_w, dimensions_h);
}

void Diagnostics_LogShufflerIndex(const char* pipeline_channel, int target_frame_index,
								  int calculated_phase_limit) {
	printf("[Telemetry][Shuffler] Channel: '%s' -> Step Index Mapping Lookups: [%d / %d]\n",
		   pipeline_channel, target_frame_index, calculated_phase_limit);
}

void Diagnostics_LogScaleBoundaryChange(uint32_t step_value, float derived_multiplier) {
	printf("[Telemetry][Scale Event] Context Transformation Layer Modified: Scale Ratio Half-Steps "
		   "= %u | Factor Metric = %.2fx\n",
		   step_value, derived_multiplier);
}
