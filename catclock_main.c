/******************************************************************************
 * File Name:    catclock_main.c
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
#include <time.h>
#include <math.h>

#ifndef SOKOL_IMPL
#define SOKOL_IMPL
#endif
#ifndef SOKOL_GLCORE
#define SOKOL_GLCORE
#endif

#include "sokol/sokol_gfx.h"
#include "sokol/sokol_log.h"
#include "catclock_shaders.h"

/* Global tracking state instance matching our shared interface declaration */
CatClock_AppContext ctx = { 0 };

int target_fps_limit = DEFAULT_FPS;

/* ==========================================================================
   OS WINDOW LEVEL EVENT CAPTURE & INTERACTION HOOKS
   ========================================================================== */

/**
 * WidgetWindowHitTest
 * Captures custom desktop mouse interactions. Uses the unscaled 1-bit XBM
 * hitbox mask to make non-rectangular transparent windows safely draggable.
 */
static SDL_HitTestResult SDLCALL WidgetWindowHitTest(SDL_Window* win, const SDL_Point* area,
													 void* data) {
	(void) win;
	(void) data;

	if (ctx.use_decorations) {
		return SDL_HITTEST_NORMAL;
	}
	if (!ctx.hitbox_bits) {
		return SDL_HITTEST_DRAGGABLE;
	}

	/* RE-ALIGNED: Calculate scale factor from deterministic tracking counter steps */
	float current_scale = (float) ctx.current_half_steps / 2.0f;
	int x = (int) ((float) area->x / current_scale);
	int y = (int) ((float) area->y / current_scale);

	/* RE-ALIGNED: Target the standardized context boundary parameters */
	if (x < 0 || x >= ctx.software_mask_w || y < 0 || y >= ctx.software_mask_h) {
		return SDL_HITTEST_NORMAL;
	}

	int bytes_per_row = (ctx.software_mask_w + 7) / 8;
	int byte_index = (y * bytes_per_row) + (x / 8);
	int bit_position = x % 8;
	bool is_solid = (ctx.hitbox_bits[byte_index] & (1 << bit_position)) != 0;

	return is_solid ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}

/* ==========================================================================
   APPLICATION ENTRY RUNTIME LAYER
   ========================================================================== */
void Diagnostics_DumpMaterialCompositionToDisk(struct CatClock_XbmLibrary* library) {
	if (!library)
		return;

	/* Calculate the active scale factor derived from the Stage 2 integer tracker */
	float active_scale = (float) ctx.current_half_steps / 2.0f;

	/* Fetch pristine unscaled original workspace sizing metrics from your assets metadata */
	int base_w = ASSET_BODY_W;
	int base_h = ASSET_BODY_H;

	/* Scale final destination sheet composition canvas boundaries */
	int comp_w = (int) ceilf((float) base_w * active_scale);
	int comp_h = (int) ceilf((float) base_h * active_scale);

	/* 1. Allocate a temporary buffer to capture the unscaled 1x pre-aligned layout mask */
	uint8_t* unscaled_staging = (uint8_t*) malloc(base_w * base_h);
	if (!unscaled_staging)
		return;

	/* Invoke the core engine routine to bake and resolve layered 1x token fields perfectly */
	CatClock_BakeUnscaledMaterialIDStaging(unscaled_staging, library);

	/* 2. Allocate the scaled composition target canvas buffer */
	uint8_t* composition_buffer = (uint8_t*) calloc(1, comp_w * comp_h);
	if (!composition_buffer) {
		free(unscaled_staging);
		return;
	}

	/* 3. Upsample the pre-aligned combined material index array to match integer steps */
	for (int y = 0; y < comp_h; y++) {
		int src_y = (int) ((float) y / active_scale);
		if (src_y >= base_h)
			continue;

		for (int x = 0; x < comp_w; x++) {
			int src_x = (int) ((float) x / active_scale);
			if (src_x >= base_w)
				continue;

			/* Direct index sampling from the pre-resolved unscaled mesh layout array */
			composition_buffer[(y * comp_w) + x] = unscaled_staging[(src_y * base_w) + src_x];
		}
	}

	/* Free the unscaled placeholder buffer since processing is complete */
	free(unscaled_staging);

	/* Write out the cleanly scaled PAM matrix directly using your canonical disk dump sink */
	CatClock_DebugDumpPamToDisk("dump_material_composition.pam", composition_buffer, comp_w,
								comp_h);
	free(composition_buffer);
}

int main(int argc, char* argv[]) {
	printf("[Trace] Starting App Transition Runtime Execution Context.\n");
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fprintf(stderr, "[Fatal Error] SDL_Init subsystem initialization failure.\n");
		return 1;
	}

	/* Setup runtime configurations, custom frame limits, and color tokens */
	ParseCommandLineArguments(argc, argv, &ctx);

	/* RE-ALIGNED: References the modernized canvas constants from shared header pass */
	float baseline_w = ctx.use_decorations ? DECORATED_CANVAS_W : 103.0f;
	float baseline_h = ctx.use_decorations ? DECORATED_CANVAS_H : 284.0f;
	float scale = (float) ctx.current_half_steps / 2.0f;

	int target_w = (int) (baseline_w * scale);
	int target_h = (int) (baseline_h * scale);

	SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL;
	if (!ctx.use_decorations) {
		window_flags |= (SDL_WINDOW_BORDERLESS | SDL_WINDOW_TRANSPARENT);
	}
	if (!ctx.disable_always_on_top) {
		window_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
	}

	printf("[Trace] Spawning System Widget Context at Dimensions: %dx%d\n", target_w, target_h);
	ctx.window = SDL_CreateWindow("CatClock-SDL3 Widget Core", target_w, target_h, window_flags);
	if (!ctx.window) {
		fprintf(stderr, "[Fatal Error] Host Window abstraction layer failed to map.\n");
		SDL_Quit();
		return 1;
	}

	/* Anchor our 1-bit custom hit tester loop onto the window abstraction */
	SDL_SetWindowHitTest(ctx.window, WidgetWindowHitTest, NULL);

	/* Enforce rigid modern Core Profile attributes across the graphics layer */
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GLContext gl_context = SDL_GL_CreateContext(ctx.window);
	if (!gl_context) {
		fprintf(stderr, "[Fatal Error] OpenGL Hardware Context instantiation dropped.\n");
		SDL_DestroyWindow(ctx.window);
		SDL_Quit();
		return 1;
	}
	SDL_GL_MakeCurrent(ctx.window, gl_context);

	/* Setup Sokol Framework Context Backend */
	sg_desc sokol_description
		= { .logger.func = slog_func,
			.environment = { .defaults = { .color_format = SG_PIXELFORMAT_RGBA8 } } };
	sg_setup(&sokol_description);
	if (!sg_isvalid()) {
		fprintf(stderr, "[Fatal Error] Sokol GFX framework context layer validation failure.\n");
		SDL_GL_MakeCurrent(ctx.window, NULL);
		SDL_GL_DestroyContext(gl_context);
		SDL_DestroyWindow(ctx.window);
		SDL_Quit();
		return 1;
	}
	printf("[Trace] Sokol Core GFX successfully attached to standard rendering pipeline.\n");

	// === FILE: catclock_main.c ===
	// Adjunct lines: Replace the block starting right after sg_setup(&sokol_description); down to
	// the cleanup phase before return 0;

	struct CatClock_XbmLibrary* runtime_xbm_handle = CatClock_InitXbmLibrary(NULL);
	if (!runtime_xbm_handle) {
		fprintf(
			stderr,
			"[Fatal Error] Crucial system XBM assets directory mapping failed to initialize.\n");
		sg_shutdown();
		SDL_GL_MakeCurrent(ctx.window, NULL);
		SDL_GL_DestroyContext(gl_context);
		SDL_DestroyWindow(ctx.window);
		SDL_Quit();
		return 1;
	}

	/* ==========================================================================
	STAGE 2: DETERMINISTIC INTERACTIVE STEP VALIDATION LOOP
	========================================================================== */
	printf("[Verification] Entering interactive runtime event processing layout loop...\n");

	struct {
		int type;
		SDL_Color color;
	} hour_cfg = { HAND_TYPE_HOUR, ctx.hour_color };
	struct {
		int type;
		SDL_Color color;
	} min_cfg = { HAND_TYPE_MINUTE, ctx.minute_color };
	struct {
		int type;
		SDL_Color color;
	} sec_cfg = { HAND_TYPE_SECOND, ctx.second_color };
	CatClock_TailShaderArgs tail_data = { 0.0f, 0.0f, false };

	/* Force baseline allocation synchronization trace on initialization */
	ctx.texture_cache_stale = true;

	bool running = true;
	SDL_Event event;

	while (running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}
			/* Capture keyboard step tracking scale transformations */
			else if (event.type == SDL_EVENT_KEY_DOWN) {
				/* Immediate termination check via Escape shortcut key pattern */
				if (event.key.key == SDLK_ESCAPE) {
					running = false;
					break;
				}

				uint32_t old_steps = ctx.current_half_steps;
				if (event.key.key == SDLK_EQUALS || event.key.key == SDLK_KP_PLUS) {
					if (ctx.current_half_steps < 20)
						ctx.current_half_steps++;
				} else if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS) {
					if (ctx.current_half_steps > 1)
						ctx.current_half_steps--;
				}

				if (ctx.current_half_steps != old_steps) {
					ctx.texture_cache_stale = true;
					Diagnostics_LogScaleBoundaryChange(ctx.current_half_steps,
													   ((float) ctx.current_half_steps / 2.0f));

					/* Explicitly resize the desktop window framework to match integer scale factors
					 */
					float updated_scale = (float) ctx.current_half_steps / 2.0f;
					int next_w = (int) (baseline_w * updated_scale);
					int next_h = (int) (baseline_h * updated_scale);
					SDL_SetWindowSize(ctx.window, next_w, next_h);
				}
			}
			/* Capture continuous mouse wheel scale modifications */
			else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
				uint32_t old_steps = ctx.current_half_steps;
				if (event.wheel.y > 0.0f) {
					if (ctx.current_half_steps < 20)
						ctx.current_half_steps++;
				} else if (event.wheel.y < 0.0f) {
					if (ctx.current_half_steps > 1)
						ctx.current_half_steps--;
				}

				if (ctx.current_half_steps != old_steps) {
					ctx.texture_cache_stale = true;
					Diagnostics_LogScaleBoundaryChange(ctx.current_half_steps,
													   ((float) ctx.current_half_steps / 2.0f));

					float updated_scale = (float) ctx.current_half_steps / 2.0f;
					int next_w = (int) (baseline_w * updated_scale);
					int next_h = (int) (baseline_h * updated_scale);
					SDL_SetWindowSize(ctx.window, next_w, next_h);
				}
			}
		}

		/* Regenerate asset textures on the CPU only when cache check state flags shift */
		if (ctx.texture_cache_stale) {
			CatClock_RebakeComputeAtlas(NULL, &ctx.hours_atlas, 64, 96, TOTAL_HAND_PHASES, 10,
										CatClock_ShaderHands, &hour_cfg);
			CatClock_RebakeComputeAtlas(NULL, &ctx.minutes_atlas, 64, 96, TOTAL_HAND_PHASES, 10,
										CatClock_ShaderHands, &min_cfg);
			CatClock_RebakeComputeAtlas(NULL, &ctx.seconds_atlas, 64, 96, TOTAL_HAND_PHASES, 10,
										CatClock_ShaderHands, &sec_cfg);
			CatClock_RebakeComputeAtlas(NULL, &ctx.eyes_atlas, 64, 32, ctx.target_fps, 10,
										CatClock_ShaderEyes, NULL); // Fixed to ctx.target_fps
			CatClock_RebakeComputeAtlas(NULL, &ctx.tail_atlas, 96, 96, (ctx.target_fps * 2), 10,
										CatClock_ShaderTail, &tail_data);

			Diagnostics_DumpMaterialCompositionToDisk(runtime_xbm_handle);
			ctx.texture_cache_stale = false;
			printf("[Trace] Dynamic textures cached and committed to disk files at half-step: %u\n",
				   ctx.current_half_steps);
		}

		/* Placeholder Graphics Render Pass: Clear the window using the context background token */
		glClearColor((float) ctx.window_bg_color.r / 255.0f, (float) ctx.window_bg_color.g / 255.0f,
					 (float) ctx.window_bg_color.b / 255.0f,
					 (float) ctx.window_bg_color.a / 255.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		SDL_GL_SwapWindow(ctx.window);

		/* Standard execution logging step tracking loop metrics */
		time_t raw_now = time(NULL);
		struct tm* t_struct = localtime(&raw_now);
		Diagnostics_LogShufflerIndex("Needle_Hour", (t_struct->tm_hour % 12), TOTAL_HAND_PHASES);
		Diagnostics_LogShufflerIndex("Needle_Minute", t_struct->tm_min, TOTAL_HAND_PHASES);
		Diagnostics_LogShufflerIndex("Needle_Second", t_struct->tm_sec, TOTAL_HAND_PHASES);
		Diagnostics_LogShufflerIndex("Appendage_Tail",
									 (int) (ctx.current_frame_step % (ctx.target_fps * 2)),
									 (ctx.target_fps * 2));

		SDL_Delay(1000 / ctx.target_fps);
		ctx.current_frame_step++;
	}

	printf("[Trace] Validation pass finished. Component layout extraction complete.\n");
	/* Release computing resources clean before teardown exit */

	/* Release computing resources clean before teardown exit */
	CatClock_DestroyComputeAtlas(&ctx.hours_atlas);
	CatClock_DestroyComputeAtlas(&ctx.minutes_atlas);
	CatClock_DestroyComputeAtlas(&ctx.seconds_atlas);
	CatClock_DestroyComputeAtlas(&ctx.eyes_atlas);
	CatClock_DestroyComputeAtlas(&ctx.tail_atlas);

	/* RE-ALIGNED: Free resources clean via local stack container */
	if (runtime_xbm_handle) {
		CatClock_DestroyXbmLibrary(runtime_xbm_handle);
		runtime_xbm_handle = NULL;
	}

	sg_shutdown();
	SDL_GL_MakeCurrent(ctx.window, NULL);
	SDL_GL_DestroyContext(gl_context);
	SDL_DestroyWindow(ctx.window);
	SDL_Quit();

	printf("[Trace] Execution Context terminated cleanly.\n");
	return 0;
}
