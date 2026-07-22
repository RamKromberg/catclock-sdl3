/******************************************************************************
 * File Name:    catclock_rasterizer.c
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
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIXED_HALF 128

/* ==========================================================================
   1. CORE PRIMITIVE VECTOR RASTERIZATION ENGINE
   ========================================================================== */

/**
 * PlotSoftwarePixel
 * Direct un-interpolated index writer mapping 8-bit palette tokens to the canvas.
 * Isolated strictly to the texture atlas compilation pass.
 */
void PlotSoftwarePixel(uint8_t* buffer, int x, int y, int width, int height, uint8_t token) {
	if (x >= 0 && x < width && y >= 0 && y < height) {
		buffer[(y * width) + x] = token;
	}
}

/**
 * @brief Core 24.8 Fixed-Point Triangle Rasterization Engine
 * Evaluates edge equations at pixel centers with proper Top-Left fill rules.
 * Handles both clockwise (CW) and counter-clockwise (CCW) winding orders.
 */
void DrawTriangleFixed(uint8_t* buffer, int32_t fx_x0, int32_t fx_y0, int32_t fx_x1, int32_t fx_y1,
					   int32_t fx_x2, int32_t fx_y2, int width, int height, uint8_t token) {
	// Compute integer bounding box by shifting down, adding rounding margins
	int32_t min_x
		= (fx_x0 < fx_x1 ? (fx_x0 < fx_x2 ? fx_x0 : fx_x2) : (fx_x1 < fx_x2 ? fx_x1 : fx_x2)) >> 8;
	int32_t max_x
		= (fx_x0 > fx_x1 ? (fx_x0 > fx_x2 ? fx_x0 : fx_x2) : (fx_x1 > fx_x2 ? fx_x1 : fx_x2)) >> 8;
	int32_t min_y
		= (fx_y0 < fx_y1 ? (fx_y0 < fx_y2 ? fx_y0 : fx_y2) : (fx_y1 < fx_y2 ? fx_y1 : fx_y2)) >> 8;
	int32_t max_y
		= (fx_y0 > fx_y1 ? (fx_y0 > fx_y2 ? fx_y0 : fx_y2) : (fx_y1 > fx_y2 ? fx_y1 : fx_y2)) >> 8;

	// Guard against empty canvas frames or total off-screen primitives
	if (width <= 0 || height <= 0)
		return;

	// Strict clipping to the layout screen canvas bounds
	if (min_x < 0)
		min_x = 0;
	if (max_x >= width)
		max_x = width - 1;
	if (min_y < 0)
		min_y = 0;
	if (max_y >= height)
		max_y = height - 1;

	if (min_x > max_x || min_y > max_y)
		return;

	// Delta tracking calculations matching analytical cross-product pipelines
	int32_t dx0 = fx_x1 - fx_x0;
	int32_t dy0 = fx_y1 - fx_y0;
	int32_t dx1 = fx_x2 - fx_x1;
	int32_t dy1 = fx_y2 - fx_y1;
	int32_t dx2 = fx_x0 - fx_x2;
	int32_t dy2 = fx_y0 - fx_y2;

	// Top-Left Tie-Breaking Rule evaluation logic
	// A flat horizontal top edge moves left (dx < 0, dy == 0). Left edges move down (dy > 0).
	int tl0 = (dy0 > 0 || (dy0 == 0 && dx0 < 0));
	int tl1 = (dy1 > 0 || (dy1 == 0 && dx1 < 0));
	int tl2 = (dy2 > 0 || (dy2 == 0 && dx2 < 0));

#ifdef DEBUG_TELEMETRY_TRIANGLE_FIXED
	if (token == 2 || token == 3 || token == 4) {
		printf("[FIXED-POINT RASTER AUDIT] UNIT_START\n");
		printf("  Token ID: %u\n", token);
		printf("  Bounding Box: X[%d, %d] Y[%d, %d]\n", min_x, max_x, min_y, max_y);
		printf("  V0: (%.3f, %.3f)\n", (float) fx_x0 / 256.0f, (float) fx_y0 / 256.0f);
		printf("  V1: (%.3f, %.3f)\n", (float) fx_x1 / 256.0f, (float) fx_y1 / 256.0f);
		printf("  V2: (%.3f, %.3f)\n", (float) fx_x2 / 256.0f, (float) fx_y2 / 256.0f);
	}
#endif

	// Main structural row-major rasterization loops
	for (int32_t y = min_y; y <= max_y; ++y) {
		int32_t fx_center_y = (y << 8) + FIXED_HALF;
		int32_t pixels_generated_on_row = 0;

		for (int32_t x = min_x; x <= max_x; ++x) {
			int32_t fx_center_x = (x << 8) + FIXED_HALF;

			// Compute standard hardware-aligned edge metrics via 64-bit precision to prevent
			// overflow hazards
			int64_t w0 = (int64_t) (fx_center_x - fx_x0) * (int64_t) dy0
				- (int64_t) (fx_center_y - fx_y0) * (int64_t) dx0;
			int64_t w1 = (int64_t) (fx_center_x - fx_x1) * (int64_t) dy1
				- (int64_t) (fx_center_y - fx_y1) * (int64_t) dx1;
			int64_t w2 = (int64_t) (fx_center_x - fx_x2) * (int64_t) dy2
				- (int64_t) (fx_center_y - fx_y2) * (int64_t) dx2;

			// Evaluate winding-agnostic orientation layouts
			int inside_ccw = (w0 > 0 || (w0 == 0 && tl0)) && (w1 > 0 || (w1 == 0 && tl1))
				&& (w2 > 0 || (w2 == 0 && tl2));

			int inside_cw = (w0 < 0 || (w0 == 0 && !tl0)) && (w1 < 0 || (w1 == 0 && !tl1))
				&& (w2 < 0 || (w2 == 0 && !tl2));

			if (inside_ccw || inside_cw) {
				buffer[y * width + x] = token;
				pixels_generated_on_row++;
			}
		}

#ifdef DEBUG_TELEMETRY_TRIANGLE_FIXED
		if ((token == 2 || token == 3 || token == 4) && pixels_generated_on_row > 0) {
			printf("[FIXED-POINT RASTER AUDIT] ROW_ALLOC: Y=%d, Generated Span=%d\n", y,
				   pixels_generated_on_row);
		}
#endif
	}

#ifdef DEBUG_TELEMETRY_TRIANGLE_FIXED
	if (token == 2 || token == 3 || token == 4) {
		printf("[FIXED-POINT RASTER AUDIT] UNIT_COMPLETE\n");
	}
#endif
}

/**
 * @brief High-Precision Float Wrapper (OpenGL-Style subpixel pipeline path)
 */
void DrawTriangleFloat(uint8_t* buffer, float x0, float y0, float x1, float y1, float x2, float y2,
					   int width, int height, uint8_t token) {
	// Perform float scaling and round directly to nearest integer values
	int32_t fx_x0 = (int32_t) (x0 * 256.0f);
	int32_t fx_y0 = (int32_t) (y0 * 256.0f);
	int32_t fx_x1 = (int32_t) (x1 * 256.0f);
	int32_t fx_y1 = (int32_t) (y1 * 256.0f);
	int32_t fx_x2 = (int32_t) (x2 * 256.0f);
	int32_t fx_y2 = (int32_t) (y2 * 256.0f);

	DrawTriangleFixed(buffer, fx_x0, fx_y0, fx_x1, fx_y1, fx_x2, fx_y2, width, height, token);
}

/**
 * @brief Legacy Integer Wrapper
 */
void DrawTriangleLegacy(uint8_t* buffer, int x0, int y0, int x1, int y1, int x2, int y2, int width,
						int height, uint8_t token) {
	// Retain full legacy pixel alignment properties via classic bit-shifting
	int32_t fx_x0 = (int32_t) x0 << 8;
	int32_t fx_y0 = (int32_t) y0 << 8;
	int32_t fx_x1 = (int32_t) x1 << 8;
	int32_t fx_y1 = (int32_t) y1 << 8;
	int32_t fx_x2 = (int32_t) x2 << 8;
	int32_t fx_y2 = (int32_t) y2 << 8;

	DrawTriangleFixed(buffer, fx_x0, fx_y0, fx_x1, fx_y1, fx_x2, fx_y2, width, height, token);
}

/**
 * CompareFloats
 * Standard qsort comparison helper for floating-point bounds validation.
 */
int CompareFloats(const void* a, const void* b) {
	float fa = *(const float*) a;
	float fb = *(const float*) b;
	return (fa > fb) - (fa < fb);
}
