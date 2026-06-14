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

// clang-format off
#include "catclock_shared.h"
#include "catclock_atlas.h"
// clang-format on
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void PrintHelpDocumentation(const char* program_name) {
	printf("Kit-Cat Desktop Widget Clock (SDL3 Engine Re-Port)\n");
	printf("Usage: %s [flags]\n\n", program_name);
	printf("Available Flags:\n");
	printf("  --help              Display this interface parameter map.\n");
	printf("  -nosecorndsteps / -noseconds  Completely hide the sweeping second hand.\n");
	printf("  -nooutline          Disable the form-fitting 1px white halo background.\n");
	printf("  -decorations [hex]  Restore standard desktop borders & window title-bars with "
		   "optional background color.\n");
	printf("  -notop              Disable the forced 'Always on Top' window layer pinning.\n");
	printf("  -fps [1-120]        Set a custom target frame rate pacing limit (Default: 30).\n");
	printf(
		"  -catcolor [hex]     Override default black cat body base layout (Default: 000000).\n");
	printf("  -detailcolor [hex]  Override default white accents and sclera layout (Default: "
		   "ffffff).\n");
	printf("  -tiecolor [hex]     Override default necktie hex color channel (Default: fdfdfd).\n");
	printf(
		"  -pupilcolor [hex]   Override moving eye pupil hex color channel (Default: 000000).\n");
	printf("  -hourcolor [hex]    Override default hour clock hand hex color (Default: 000000).\n");
	printf(
		"  -minutecolor [hex]  Override default minute clock hand hex color (Default: 000000).\n");
	printf("  -secondcolor [hex]  Override default sweeping second hand hex color (Default: "
		   "ff0000).\n");
	printf("  -scleracolor [hex]  Override static eye background socket color layout (Default: "
		   "ffffff).\n");
}

bool HelperParseHexColor(const char* hex_str, SDL_Color* out_color) {
	if (hex_str[0] == '#')
		hex_str++;
	size_t len = strlen(hex_str);
	unsigned int r = 0, g = 0, b = 0;
	if (len == 6 && sscanf(hex_str, "%02x%02x%02x", &r, &g, &b) == 3) {
		out_color->r = (Uint8) r;
		out_color->g = (Uint8) g;
		out_color->b = (Uint8) b;
		out_color->a = 255;
		return true;
	} else if (len == 3 && sscanf(hex_str, "%1x%1x%1x", &r, &g, &b) == 3) {
		out_color->r = (Uint8) (r * 17);
		out_color->g = (Uint8) (g * 17);
		out_color->b = (Uint8) (b * 17);
		out_color->a = 255;
		return true;
	}
	return false;
}

void ParseCommandLineArguments(int argc, char* argv[], CatClock_AppContext* ctx) {
	ctx->fg_color = (SDL_Color) { 0, 0, 0, 255 };
	ctx->bg_color = (SDL_Color) { 255, 255, 255, 255 };
	ctx->cat_color = (SDL_Color) { 0, 0, 0, 255 };
	ctx->tie_color = (SDL_Color) { 255, 255, 255, 255 };
	ctx->pupil_color = (SDL_Color) { 0, 0, 0, 255 };

	ctx->hour_color = (SDL_Color) { 0, 0, 0, 255 };
	ctx->minute_color = (SDL_Color) { 0, 0, 0, 255 };
	ctx->second_color = (SDL_Color) { 255, 0, 0, 255 };
	ctx->detail_color = (SDL_Color) { 255, 255, 255, 255 };
	ctx->sclera_color = (SDL_Color) { 255, 255, 255, 255 };
	ctx->window_bg_color = (SDL_Color) { 255, 255, 255, 255 };
	ctx->current_scale = 1.0f;

	ctx->hide_seconds = false;
	ctx->disable_outline = false;
	ctx->use_decorations = false;
	ctx->disable_always_on_top = false;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			PrintHelpDocumentation(argv[0]);
			exit(0);
		} else if (strcmp(argv[i], "-nosecorndsteps") == 0 || strcmp(argv[i], "-noseconds") == 0) {
			ctx->hide_seconds = true;
		} else if (strcmp(argv[i], "-nooutline") == 0) {
			ctx->disable_outline = true;
		} else if (strcmp(argv[i], "-decorations") == 0) {
			ctx->use_decorations = true;
			if ((i + 1) < argc && argv[i + 1][0] != '-') {
				SDL_Color target_bg;
				if (HelperParseHexColor(argv[i + 1], &target_bg)) {
					ctx->window_bg_color = target_bg;
					i++;
				}
			}
		} else if (strcmp(argv[i], "-notop") == 0) {
			ctx->disable_always_on_top = true;
		} else if (strcmp(argv[i], "-catcolor") == 0) {
			if ((i + 1) < argc) {
				if (HelperParseHexColor(argv[++i], &ctx->cat_color)) {
					ctx->fg_color = ctx->cat_color;
				}
			}
		} else if (strcmp(argv[i], "-detailcolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &ctx->detail_color);
			}
		} else if (strcmp(argv[i], "-tiecolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &ctx->tie_color);
			}
		} else if (strcmp(argv[i], "-scleracolor") == 0) {
			if ((i + 1) < argc) {
				if (HelperParseHexColor(argv[++i], &ctx->sclera_color)) {
					ctx->texture_cache_stale = true;
				}
			}
		} else if (strcmp(argv[i], "-pupilcolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &ctx->pupil_color);
			}
		} else if (strcmp(argv[i], "-hourcolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &ctx->hour_color);
			}
		} else if (strcmp(argv[i], "-minutecolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &ctx->minute_color);
			}
		} else if (strcmp(argv[i], "-secondcolor") == 0) {
			if ((i + 1) < argc) {
				HelperParseHexColor(argv[++i], &ctx->second_color);
			}
		} else if (strcmp(argv[i], "-fps") == 0) {
			if ((i + 1) < argc) {
				int parsed_fps = atoi(argv[++i]);
				if (parsed_fps >= 1 && parsed_fps <= 120) {
					target_fps_limit = parsed_fps;
				}
			} else {
				PrintHelpDocumentation(argv[0]);
				exit(1);
			}
		} else {
			fprintf(stderr, "Unknown parameter layout flag detected: %s\n", argv[i]);
			PrintHelpDocumentation(argv[0]);
			exit(1);
		}
	}
}
