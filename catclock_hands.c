/******************************************************************************
 * File Name:    catclock_hands.c
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
#include "catclock_atlas.h"
// clang-format on
#include <math.h>
#include <stdlib.h>

/* Match the private layout from catclock_atlas.c to guarantee memory alignment */
typedef struct {
	int hand_type;
	SDL_Color color;
} HandShaderConfig;

/**
 * Renders a single clock hand frame using grid-snapped, sharp triangle geometry.
 * Bakes mask templates as a white layer to allow runtime GPU tinting.
 */
void CatClock_ShaderHands(SDL_Renderer* renderer, int cell_w, int cell_h, float scale,
						  int frame_idx, void* userdata) {
	HandShaderConfig* cfg = (HandShaderConfig*) userdata;
	if (!cfg)
		return;

	/* Spindle center axis local coordinates mapping */
	float local_cx = (float) cell_w / 2.0f;
	float local_cy = (float) cell_h / 2.0f;
	float angle_rad = ((float) frame_idx / 60.0f) * 2.0f * (float) M_PI;

	/* 1. Calculate direction vectors tracking the 2:3 vertical clock face aspect ratio */
	float raw_fx = sinf(angle_rad);
	float raw_fy = -cosf(angle_rad);
	float aspect_inv = (float) cell_h / (float) cell_w;
	float fx = raw_fx;
	float fy = raw_fy * aspect_inv;

	/* 2. Re-project structural rendering angle to eliminate quadrant warping */
	float projected_angle = atan2f(fx, -fy);
	float cos_a = cosf(projected_angle);
	float sin_a = sinf(projected_angle);
	float radial_scale = sqrtf(fx * fx + fy * fy);

	float base_length = 0.0f;
	float thickness = 0.0f;
	float pivot_back = 0.0f;

	/* Retrieve baseline hand metrics cleanly matching target configurations */
	if (cfg->hand_type == HAND_TYPE_HOUR) {
		base_length = (float) cell_w * 0.28f;
		thickness = 7.5f * scale;
		pivot_back = 4.0f * scale;
	} else if (cfg->hand_type == HAND_TYPE_MINUTE) {
		base_length = (float) cell_w * 0.42f;
		thickness = 5.0f * scale;
		pivot_back = 5.0f * scale;
	} else if (cfg->hand_type == HAND_TYPE_SECOND) {
		base_length = (float) cell_w * 0.46f;
		thickness = 2.5f * scale;
		pivot_back = 6.0f * scale;
	}

	float final_length = base_length * radial_scale;
	float final_pivot = pivot_back * radial_scale;

	/* 3. Snap boundaries to match whole integer units */
	float internal_unit_ratio = scale <= 0.01f ? 1.0f : scale;
	final_length = roundf(final_length / internal_unit_ratio) * internal_unit_ratio;
	final_pivot = roundf(final_pivot / internal_unit_ratio) * internal_unit_ratio;
	thickness = roundf(thickness / internal_unit_ratio) * internal_unit_ratio;

	/* 4. Calculate core triangle corner locations */
	float tip_x = local_cx + (final_length * sin_a);
	float tip_y = local_cy - (final_length * cos_a);
	float half_thick = thickness / 2.0f;
	float base_l_x = local_cx - (half_thick * cos_a) - (final_pivot * sin_a);
	float base_l_y = local_cy - (half_thick * sin_a) + (final_pivot * cos_a);
	float base_r_x = local_cx + (half_thick * cos_a) - (final_pivot * sin_a);
	float base_r_y = local_cy + (half_thick * sin_a) + (final_pivot * cos_a);

	/* 5. Force strict hardware integer-grid snapping to maintain sharp pixel edges */
	tip_x = roundf(tip_x);
	tip_y = roundf(tip_y);
	base_l_x = roundf(base_l_x);
	base_l_y = roundf(base_l_y);
	base_r_x = roundf(base_r_x);
	base_r_y = roundf(base_r_y);

	/* 6. Hardware Color Setup - Bake template masks as flat white layers */
	SDL_FColor fcol = { 1.0f, 1.0f, 1.0f, 1.0f };
	SDL_FPoint zero_uv = { 0.0f, 0.0f };

	/* FIX: Formulate explicitly as a 3-element struct layout array buffer */
	SDL_Vertex verts[3] = { { { tip_x, tip_y }, fcol, zero_uv },
							{ { base_l_x, base_l_y }, fcol, zero_uv },
							{ { base_r_x, base_r_y }, fcol, zero_uv } };

	/* Maintain unblended sharp pixel art edges inside atlas cells */
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
	SDL_RenderGeometry(renderer, NULL, verts, 3, NULL, 0);
}
