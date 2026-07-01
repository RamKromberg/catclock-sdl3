/******************************************************************************
 * File Name:    catclock_args.c
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
   COMMAND-LINE ARCHITECTURE & VALUE VALIDATION SUBSYSTEM
   ========================================================================== */

/**
 * Print Help Documentation
 * Outputs all valid application flags, parameter maps, and usage guides
 * to standard output. Retained entirely to serve as core user-facing documentation.
 */
void PrintHelpDocumentation(const char* program_name) {
	printf("Kit-Cat Desktop Widget Clock (SDL3 Engine Re-Port)\n");
	printf("https://github.com/RamKromberg/catclock-sdl3\n");
	printf("Usage: %s [flags]\n\n", program_name);
	printf("Available Flags:\n");
	printf("  --help                  Display this interface parameter map.\n");
	printf("  --notop                 Disable the forced 'Always on Top' window layer pinning.\n");
	printf("  --noseconds             Completely hide the sweeping second hand.\n");
	printf("  --nooutline             Disable the form-fitting 1px white halo background.\n");
	printf(
		"  --fps [1-120]           Set a custom target frame rate pacing limit (Default: 30).\n");
	printf("  --scale [0.5,10.0]      Set the initial scale multiplier (Default: 1.0).\n");
	printf("  --decorations           Restore standard desktop borders & window title-bars.\n");
	printf("  --catcolor [hex]        Override default black cat body base layout.\n");
	printf("  --detailcolor [hex]     Override default white accents and sclera layout.\n");
	printf("  --tiecolor [hex]        Override default necktie hex color channel.\n");
	printf("  --pupilcolor [hex]      Override moving eye pupil hex color channel.\n");
	printf("  --scleracolor [hex]     Override static eye background socket color layout.\n");
	printf("  --hourscolor [hex]      Override default hour clock hand hex color.\n");
	printf("  --minutescolor [hex]    Override default minute clock hand hex color.\n");
	printf("  --secondscolor [hex]    Override default sweeping second hand hex color.\n");
}

/**
 * Helper Parse Hex Color
 * Parses 3-digit or 6-digit hexadecimal strings (with or without leading '#')
 * directly into SDL_Color structures. Used for interface color customization overrides.
 */
bool HelperParseHexColor(const char* hex_str, SDL_Color* out_color) {
	if (hex_str[0] == '#') {
		hex_str++;
	}

	size_t len = strlen(hex_str);
	unsigned int r = 0, g = 0, b = 0;

	if (len == 6 && sscanf(hex_str, "%02x%02x%02x", &r, &g, &b) == 3) {
		out_color->r = (uint8_t) r;
		out_color->g = (uint8_t) g;
		out_color->b = (uint8_t) b;
		out_color->a = 255;
		return true;
	} else if (len == 3 && sscanf(hex_str, "%1x%1x%1x", &r, &g, &b) == 3) {
		out_color->r = (uint8_t) (r * 17);
		out_color->g = (uint8_t) (g * 17);
		out_color->b = (uint8_t) (b * 17);
		out_color->a = 255;
		return true;
	}

	return false;
}

/**
 * Parse Command Line Arguments
 * Processes all incoming argv strings, sets context defaults, and performs
 * explicit error checks. Adapted safely to handle integer scale tracking.
 */
void ParseCommandLineArguments(int argc, char* argv[], CatClock_AppContext* context) {
	/* Initialize default widget theme palette parameters */
	context->fg_color = (SDL_Color) { 0, 0, 0, 255 };
	context->bg_color = (SDL_Color) { 255, 255, 255, 255 };
	context->cat_color = (SDL_Color) { 0, 0, 0, 255 };
	context->tie_color = (SDL_Color) { 255, 255, 255, 255 };
	context->pupil_color = (SDL_Color) { 0, 0, 0, 255 };
	context->hour_color = (SDL_Color) { 0, 0, 0, 255 };
	context->minute_color = (SDL_Color) { 0, 0, 0, 255 };
	context->second_color = (SDL_Color) { 255, 0, 0, 255 };
	context->detail_color = (SDL_Color) { 255, 255, 255, 255 };
	context->sclera_color = (SDL_Color) { 255, 255, 255, 255 };
	context->window_bg_color = (SDL_Color) { 255, 255, 255, 255 };
	context->current_half_steps = 2;
	context->hide_seconds = false;
	context->target_fps = DEFAULT_FPS;
	context->use_decorations = false;
	context->disable_outline = false;
	context->disable_always_on_top = false;
	context->texture_cache_stale = false;
	context->current_frame_step = 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			PrintHelpDocumentation(argv[0]);
			exit(0);
		} else if (strcmp(argv[i], "--scale") == 0) {
			if ((i + 1) < argc) {
				float sc = (float) atof(argv[++i]);
				int calculated_steps = (int) (sc * 2.0f + 0.5f);
				if (calculated_steps < 1)
					calculated_steps = 1;
				if (calculated_steps > 20)
					calculated_steps = 20;
				context->current_half_steps = (uint32_t) calculated_steps;
				Diagnostics_LogScaleBoundaryChange(context->current_half_steps,
												   ((float) context->current_half_steps / 2.0f));
			}
		} else if (strcmp(argv[i], "--noseconds") == 0) {
			context->hide_seconds = true;
		} else if (strcmp(argv[i], "--nooutline") == 0) {
			context->disable_outline = true;
		} else if (strcmp(argv[i], "--decorations") == 0) {
			context->use_decorations = true;
		} else if (strcmp(argv[i], "--notop") == 0) {
			context->disable_always_on_top = true;
		} else if (strcmp(argv[i], "--catcolor") == 0) {
			if ((i + 1) < argc) {
				if (HelperParseHexColor(argv[++i], &context->cat_color)) {
					context->fg_color = context->cat_color;
				}
			}
		} else if (strcmp(argv[i], "--detailcolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &context->detail_color);
			}
		} else if (strcmp(argv[i], "--tiecolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &context->tie_color);
			}
		} else if (strcmp(argv[i], "--scleracolor") == 0) {
			if ((i + 1) < argc) {
				if (HelperParseHexColor(argv[++i], &context->sclera_color)) {
					context->texture_cache_stale = true;
				}
			}
		} else if (strcmp(argv[i], "--pupilcolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &context->pupil_color);
			}
		} else if (strcmp(argv[i], "--hourscolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &context->hour_color);
			}
		} else if (strcmp(argv[i], "--minutescolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &context->minute_color);
			}
		} else if (strcmp(argv[i], "--secondscolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &context->second_color);
			}
		} else if (strcmp(argv[i], "--fps") == 0) {
			if ((i + 1) < argc) {
				int parsed_fps = atoi(argv[++i]);
				if (parsed_fps >= 1 && parsed_fps <= 120) {
					context->target_fps = parsed_fps;
				}
			}
		} else {
			fprintf(stderr, "Unknown parameter layout flag detected: %s\n", argv[i]);
			PrintHelpDocumentation(argv[0]);
			exit(1);
		}
	}
}
