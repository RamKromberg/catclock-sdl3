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
	printf("Kit-Cat Desktop Widget Clock (SDL3, Sokol and Freetype Port)\n");
	printf("https://github.com/RamKromberg/catclock-sdl3\n");
	printf("Usage: %s [flags]\n\n", program_name);
	printf("Available Flags:\n");
	printf("  --help                   Print this usage and exit.\n");
	printf("  --notop                  Disable the forced 'Always on Top' window layer pinning. "
		   "(Default: %s)\n",
		   ctx.disable_always_on_top ? "True" : "False");
	printf("  --decorations            Restore standard desktop borders & window title-bars. "
		   "(Default: %s)\n",
		   ctx.use_decorations ? "True" : "False");
	printf("  --fps [1-120]            Set a custom target frame rate limit. (Default: %d)\n",
		   DEFAULT_FPS);
	printf("  --scale [0.5,...,10.0]   Set the initial scale multiplier. (Default: %.1f)\n",
		   (float) ctx.current_half_steps / 2.0f);
	printf("  --decorationscolor [hex] Override background window color. (Default: "
		   "%02x%02x%02x%02x)\n",
		   ctx.window_bg_color.r, ctx.window_bg_color.g, ctx.window_bg_color.b,
		   ctx.window_bg_color.a);
	printf(
		"  --catcolor [hex]         Override foreground body color. (Default: %02x%02x%02x%02x)\n",
		ctx.cat_color.r, ctx.cat_color.g, ctx.cat_color.b, ctx.cat_color.a);
	printf(
		"  --detailcolor [hex]      Override background body color. (Default: %02x%02x%02x%02x)\n",
		ctx.detail_color.r, ctx.detail_color.g, ctx.detail_color.b, ctx.detail_color.a);
	printf("  --tiecolor [hex]         Override necktie color. (Default: %02x%02x%02x%02x)\n",
		   ctx.tie_color.r, ctx.tie_color.g, ctx.tie_color.b, ctx.tie_color.a);
	printf("  --pupilcolor [hex]       Override eye pupil color. (Default: %02x%02x%02x%02x)\n",
		   ctx.pupil_color.r, ctx.pupil_color.g, ctx.pupil_color.b, ctx.pupil_color.a);
	printf("  --scleracolor [hex]      Override eye socket color. (Default: %02x%02x%02x%02x)\n",
		   ctx.sclera_color.r, ctx.sclera_color.g, ctx.sclera_color.b, ctx.sclera_color.a);
	printf(
		"  --hourscolor [hex]       Override hours hand hex color. (Default: %02x%02x%02x%02x)\n",
		ctx.hour_color.r, ctx.hour_color.g, ctx.hour_color.b, ctx.hour_color.a);
	printf("  --minutescolor [hex]     Override minutes hand color. (Default: %02x%02x%02x%02x)\n",
		   ctx.minute_color.r, ctx.minute_color.g, ctx.minute_color.b, ctx.minute_color.a);
	printf("  --secondscolor [hex]     Override seconds hand color. (Default: %02x%02x%02x%02x)\n",
		   ctx.seconds_color.r, ctx.seconds_color.g, ctx.seconds_color.b, ctx.seconds_color.a);
	printf("  --outlinecolor [hex]     Override outline color. (Default: %02x%02x%02x%02x)\n",
		   ctx.outline_color.r, ctx.outline_color.g, ctx.outline_color.b, ctx.outline_color.a);
}

/**
 * Helper Parse Hex Color
 * Parses 4-digit or 8-digit hexadecimal strings (with or without leading '#')
 * directly into SDL_Color structures. Used for interface color customization overrides.
 */
bool HelperParseHexColor(const char* hex_str, SDL_Color* out_color) {
	if (hex_str[0] == '#') {
		hex_str++;
	}

	size_t len = strlen(hex_str);
	unsigned int r = 0, g = 0, b = 0, a = 0;

	if (len == 8 && sscanf(hex_str, "%02x%02x%02x%02x", &r, &g, &b, &a) == 4) {
		out_color->r = (uint8_t) r;
		out_color->g = (uint8_t) g;
		out_color->b = (uint8_t) b;
		out_color->a = (uint8_t) a;
		return true;
	} else if (len == 4 && sscanf(hex_str, "%1x%1x%1x%1x", &r, &g, &b, &a) == 4) {
		out_color->r = (uint8_t) (r * 17);
		out_color->g = (uint8_t) (g * 17);
		out_color->b = (uint8_t) (b * 17);
		out_color->a = (uint8_t) (a * 17);
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
	context->seconds_color = (SDL_Color) { 255, 0, 0, 255 };
	context->detail_color = (SDL_Color) { 255, 255, 255, 255 };
	context->sclera_color = (SDL_Color) { 255, 255, 255, 255 };
	context->outline_color = (SDL_Color) { 255, 255, 255, 255 };
	context->window_bg_color = (SDL_Color) { 255, 255, 255, 255 };
	context->current_half_steps = 2;
	context->target_fps = DEFAULT_FPS;
	context->use_decorations = false;
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
#if defined(DEBUG)
				Diagnostics_LogScaleBoundaryChange(context->current_half_steps,
												   ((float) context->current_half_steps / 2.0f));
#endif
			}
		} else if (strcmp(argv[i], "--notop") == 0) {
			context->disable_always_on_top = true;
		} else if (strcmp(argv[i], "--decorations") == 0) {
			context->use_decorations = true;
		} else if (strcmp(argv[i], "--decorationscolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &context->window_bg_color);
			}
		} else if (strcmp(argv[i], "--catcolor") == 0) {
			if ((i + 1) < argc) {
				if (HelperParseHexColor(argv[++i], &context->cat_color)) {
					context->fg_color = context->cat_color;
				}
			}
		} else if (strcmp(argv[i], "--detailcolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &context->detail_color);
				context->bg_color = context->detail_color;
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
				HelperParseHexColor(argv[++i], &context->seconds_color);
			}
		} else if (strcmp(argv[i], "--outlinecolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &context->outline_color);
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
