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

	// CALIBRATED VERTICAL LOCK: Drops the pivot point down by 1.0 pixel
	// to perfectly balance the dynamic cell borders
	float local_cy = ((float) cell_h / 2.0f) + (1.0f * scale);

	float angle_rad = ((float) frame_idx / 60.0f) * 2.0f * (float) M_PI;

	// Anamorphic multipliers matching the 2:3 oval dial geometry
	float aspect_x = 1.0f;
	float aspect_y = 1.5f;

	float base_length = 0.0f;
	float thickness = 0.0f;
	float pivot_back = 0.0f;

	/* Retrieve baseline hand metrics cleanly matching target configurations */
	if (cfg->hand_type == HAND_TYPE_HOUR) {
		base_length = (float) cell_w * 0.27f; // Restored to 0.27f
		thickness = 7.5f * scale;
		pivot_back = 4.0f * scale;
	} else if (cfg->hand_type == HAND_TYPE_MINUTE) {
		base_length = (float) cell_w * 0.40f; // Balanced to 0.40f
		thickness = 5.0f * scale;
		pivot_back = 5.0f * scale;
	} else if (cfg->hand_type == HAND_TYPE_SECOND) {
		base_length = (float) cell_w * 0.44f; // Balanced to 0.44f
		thickness = 2.5f * scale;
		pivot_back = 6.0f * scale;
	}

	float norm_angle = angle_rad - ((float) M_PI / 2.0f);
	float cos_a = cosf(norm_angle);
	float sin_a = sinf(norm_angle);

	float perp_x = -sin_a;
	float perp_y = cos_a;

	// Step 2: Step lengthwise along separate X/Y anamorphic scale bounds
	float tip_offset_x = cos_a * base_length * aspect_x;
	float tip_offset_y = sin_a * base_length * aspect_y;

	// BALANCED TAIL EXTENSION: Keep the rear pivot extension un-squished
	// to prevent vertical length drift at oppositional angles
	float back_offset_x = -cos_a * pivot_back;
	float back_offset_y = -sin_a * pivot_back;

	float half_thick = thickness / 2.0f;

	float tip_x = local_cx + tip_offset_x;
	float tip_y = local_cy + tip_offset_y;

	float base_l_x = local_cx + back_offset_x + (perp_x * half_thick);
	float base_l_y = local_cy + back_offset_y + (perp_y * half_thick);

	float base_r_x = local_cx + back_offset_x - (perp_x * half_thick);
	float base_r_y = local_cy + back_offset_y - (perp_y * half_thick);

	/* Force strict hardware integer-grid snapping to maintain sharp pixel edges */
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
