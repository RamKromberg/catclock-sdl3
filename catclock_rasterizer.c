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

/* =========================================================================
   FUTURE SHADER PORT INTEGRITY & COMPATIBILITY NOTATIONS
   =========================================================================
   This software function mimics an accelerated GPU fragment shader execution
   path exactly. When migrating this geometry onto a GPU hardware pipeline
   (e.g., in './shaders/catclock.glsl'), ensure the following mappings are kept:

   1. ANCHOR COORDINATES TO SUBPIXEL CENTERS (THE DRIFT TRAP):
	  - In software, leftward drift was eliminated by strictly mapping evaluations
		to 'y << 8' and 'x << 8' (the 0.0 top-left grid intersection bounds).
	  - On the GPU, fragment shaders natively sample from the EXACT subpixel
		center via 'gl_FragCoord.xy' (which resolves to coordinate 'X.5').
	  - TRAP FOR PORTING: Do NOT manually offset or subtract 0.5 inside the
		GPU fragment shader. Let gl_FragCoord anchor natively, which matches
		this corrected alignment layout exactly.

   2. ARITHMETIC ORIENTATION (THE WINNING SIGN SWITCH):
	  - Screen space coordinates use an inverted Y-axis layout (downward positive).
		The variable 'sign' tracks vertex winding order via determinants.
	  - To keep distances positive inside the shape boundaries, the formula scales
		via 'w0 * -sign'. Keep this negative multiplier intact on the GPU if using
		a standard 2D vector cross-product matrix setup.

   3. BIT-DEPTH AND SCALE ELIMINATION:
	  - The divisions by '256.0' (un-quantizing fixed point) and '65536.0' (neutralizing
		cross product scaling) are artifacts of the CPU's fixed-point design.
	  - On a GPU fragment shader, normalize your verified coordinates directly into
		un-shifted float resolutions (0.0 to Viewport Dimensions), allowing you to
		drop the integer bit-shifts completely while using the native 'length()' vector function.

   4. MATHEMATICALLY FLUID HARDWARE ANTI-ALIASING:
	  - This software pass uses a hard boundary limit ('coverage >= 0.5') to match
		the clock's 8-bit index palette.
	  - Once ported to the GPU, swap the hard cutoff line for an analytical screenspace
		derivative filter to achieve beautiful subpixel anti-aliasing:
			float edge_delta = fwidth(min_dist);
			float alpha = smoothstep(edge_delta, -edge_delta, min_dist);
   ========================================================================= */
/**
 * @brief High-Precision Triangular Signed Distance Field Software Rasterizer
 * Includes integrated subpixel scale normalization and winding orientation drift fixes.
 */
void DrawTriangleFixedSDF(uint8_t* buffer, int32_t fx_x0, int32_t fx_y0, int32_t fx_x1,
						  int32_t fx_y1, int32_t fx_x2, int32_t fx_y2, int width, int height,
						  uint8_t token) {

	// 1. Extract exact bounding boxes using your existing fixed-point layout rules
	int32_t min_x
		= (fx_x0 < fx_x1 ? (fx_x0 < fx_x2 ? fx_x0 : fx_x2) : (fx_x1 < fx_x2 ? fx_x1 : fx_x2)) >> 8;
	int32_t max_x
		= (fx_x0 > fx_x1 ? (fx_x0 > fx_x2 ? fx_x0 : fx_x2) : (fx_x1 > fx_x2 ? fx_x1 : fx_x2)) >> 8;
	int32_t min_y
		= (fx_y0 < fx_y1 ? (fx_y0 < fx_y2 ? fx_y0 : fx_y2) : (fx_y1 < fx_y2 ? fx_y1 : fx_y2)) >> 8;
	int32_t max_y
		= (fx_y0 > fx_y1 ? (fx_y0 > fx_y2 ? fx_y0 : fx_y2) : (fx_y1 > fx_y2 ? fx_y1 : fx_y2)) >> 8;

	if (width <= 0 || height <= 0)
		return;

	// Strict viewport canvas boundary clipping
	if (min_x < 0)
		min_x = 0;
	if (max_x >= width)
		max_x = width - 1;
	if (min_y < 0)
		min_y = 0;
	if (max_y >= height)
		max_y = height - 1;

	// Diagnostic indicator tracking if any pixels attempt to pass constraints
	int32_t total_scanned_pixels = 0;
	int32_t total_rendered_pixels = 0;

#ifdef DEBUG_TELEMETRY_TRIANGLE_SDF
	if (min_x > max_x || min_y > max_y) {
		printf("[SDF DIAGNOSTIC] Primitive fully clipped out. Bounding Box: X[%d to %d], Y[%d to "
			   "%d]\n",
			   min_x, max_x, min_y, max_y);
		return;
	}
#endif

	// 2. Pre-calculate fixed-point delta components
	int32_t dx0 = fx_x1 - fx_x0;
	int32_t dy0 = fx_y1 - fx_y0;
	int32_t dx1 = fx_x2 - fx_x1;
	int32_t dy1 = fx_y2 - fx_y1;
	int32_t dx2 = fx_x0 - fx_x2;
	int32_t dy2 = fx_y0 - fx_y2;

	// Determine winding orientation to match point tracking layout
	int64_t det = (int64_t) dx0 * dy1 - (int64_t) dy0 * dx1;
	if (det == 0)
		return;
	int64_t sign = (det > 0) ? 1 : -1;

	// Calculate integer lengths in 24.8 scaling to evaluate true alignment distances
	double len0 = sqrt((double) dx0 * dx0 + (double) dy0 * dy0);
	double len1 = sqrt((double) dx1 * dx1 + (double) dy1 * dy1);
	double len2 = sqrt((double) dx2 * dx2 + (double) dy2 * dy2);

	if (len0 < 0.0001 || len1 < 0.0001 || len2 < 0.0001)
		return;

	// 3. Coordinate-Aligned Rasterization Scan
	for (int32_t y = min_y; y <= max_y; ++y) {
		int32_t fx_eval_y = (y << 8);
		int32_t row_offset = y * width;

		for (int32_t x = min_x; x <= max_x; ++x) {
			int32_t fx_eval_x = (x << 8);
			total_scanned_pixels++;

			// Cross product edge calculation matching top-left corner rules
			int64_t w0 = (int64_t) (fx_eval_x - fx_x0) * dy0 - (int64_t) (fx_eval_y - fx_y0) * dx0;
			int64_t w1 = (int64_t) (fx_eval_x - fx_x1) * dy1 - (int64_t) (fx_eval_y - fx_y1) * dx1;
			int64_t w2 = (int64_t) (fx_eval_x - fx_x2) * dy2 - (int64_t) (fx_eval_y - fx_y2) * dx2;

			// Invert the tracking sign here to ensure points inside the triangle scale positively
			double d0 = (double) (w0 * -sign) / (len0 * 256.0);
			double d1 = (double) (w1 * -sign) / (len1 * 256.0);
			double d2 = (double) (w2 * -sign) / (len2 * 256.0);

			// Extract closest boundary metric
			double min_d = d0;
			if (d1 < min_d)
				min_d = d1;
			if (d2 < min_d)
				min_d = d2;

			// Shift distance profile threshold from center to cover subpixel boundaries
			double coverage = min_d + 1.0;
			if (coverage < 0.0)
				coverage = 0.0;
			if (coverage > 1.0)
				coverage = 1.0;

			// Lock pixel on if the mathematical boundary intersects the target footprint cleanly
			if (coverage >= 0.5) {
				uint8_t* pixel_dest = buffer + row_offset + x;
				*pixel_dest = token;
				total_rendered_pixels++;
			}
		}
	}

#ifdef DEBUG_TELEMETRY_TRIANGLE_SDF
	// Optional console print telemetry block to inspect execution values at scale
	// Change to '#if 1' to display active boundary values in your build pipeline
	printf("[SDF TELEMETRY] Verts: (%d,%d) (%d,%d) (%d,%d) | Scanned: %d | Rendered: %d\n",
		   fx_x0 >> 8, fx_y0 >> 8, fx_x1 >> 8, fx_y1 >> 8, fx_x2 >> 8, fx_y2 >> 8,
		   total_scanned_pixels, total_rendered_pixels);
#endif
}
/* =========================================================================
   FUTURE SHADER PORT NOTE: ANTI-ALIASING UPGRADE (fwidth / smoothstep)
   =========================================================================
   Right now, this software engine uses a hard binary threshold to match our
   8-bit indexed palette asset buffer sheet:

		if (coverage >= 0.5) { *pixel_dest = token; }

   When you eventually migrate this exact math to a GPU fragment shader, you
   MUST discard this binary check and replace it with screenspace derivatives.
   Otherwise, your clock hands will have distracting jagged/pixelated edges
   when scaled up to high-resolution viewports.

   HOW TO IMPLEMENT THE GPU TRANSITION:
   1. The variable 'min_d' represents the analytical distance to the edge.
   2. Instead of tracking a 0.0 to 1.0 coverage window, pass the raw minimum
	  distance to the GPU's native derivative engine:

		  float edge_delta = fwidth(min_dist);

   3. Use screenspace 'fwidth' to determine the pixel delta width. This
	  tells the GPU exactly how many units 'min_dist' changes from one
	  individual pixel to the next on your monitor grid.
   4. Map this delta window directly through a smoothstep filter to generate
	  flawless subpixel anti-aliasing:

		  float alpha = smoothstep(-edge_delta, edge_delta, min_dist);

   This replacement provides mathematically perfect, resolution-independent
   anti-aliasing seamlessly, without requiring any alterations to your
   verified vertex geometry equations downstream.
   ========================================================================= */
