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

void Diagnostics_DumpMaterialCompositionToDisk(struct CatClock_XbmLibrary* lib) {
	static uint8_t* mask_staging = NULL;
	static bool multi_dump_committed = false;
	int plane_bytes = ASSET_BODY_W * ASSET_BODY_H;

	if (!mask_staging)
		mask_staging = (uint8_t*) malloc(plane_bytes);
	if (!mask_staging)
		return;

	/* RE-ALIGNED: Pass the handle token straight to target block loops */
	CatClock_BakeUnscaledMaterialIDStaging(mask_staging, lib);

	if (!multi_dump_committed) {
		CatClock_DebugDumpPamToDisk("dump_material_composition.pam", mask_staging, ASSET_BODY_W,
									ASSET_BODY_H);
		multi_dump_committed = true;
	}
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
	   AUTOMATED SYSTEM BLUEPRINT ATLAS COMPILATION TRIGGERS
	   ========================================================================== */
	printf("[Verification] Triggering automated offline compilation validation maps...\n");

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

	/* Execute the discrete sheet builds, writing binary maps to disk */
	CatClock_RebakeComputeAtlas(NULL, &ctx.hours_atlas, 64, 96, TOTAL_HAND_PHASES, 6,
								CatClock_ShaderHands, &hour_cfg);
	CatClock_RebakeComputeAtlas(NULL, &ctx.minutes_atlas, 64, 96, TOTAL_HAND_PHASES, 6,
								CatClock_ShaderHands, &min_cfg);
	CatClock_RebakeComputeAtlas(NULL, &ctx.seconds_atlas, 64, 96, TOTAL_HAND_PHASES, 6,
								CatClock_ShaderHands, &sec_cfg);
	CatClock_RebakeComputeAtlas(NULL, &ctx.eyes_atlas, 64, 32, (ctx.target_fps * 2), 10,
								CatClock_ShaderEyes, NULL);

	printf("[Trace] Compiling swinging tail system blueprints...\n");

	CatClock_TailShaderArgs tail_data = { 0.0f, 0.0f, false };
	CatClock_RebakeComputeAtlas(NULL, &ctx.tail_atlas, 96, 96, (ctx.target_fps * 2), 8,
								CatClock_ShaderTail, &tail_data);

	/* Instrumentation entries tracking execution shuffler indices */
	time_t raw_now = time(NULL);
	struct tm* t_struct = localtime(&raw_now);
	Diagnostics_LogShufflerIndex("Needle_Hour", (t_struct->tm_hour % 12), TOTAL_HAND_PHASES);
	Diagnostics_LogShufflerIndex("Needle_Minute", t_struct->tm_min, TOTAL_HAND_PHASES);
	Diagnostics_LogShufflerIndex("Needle_Second", t_struct->tm_sec, TOTAL_HAND_PHASES);
	Diagnostics_LogShufflerIndex("Appendage_Tail",
								 (int) (ctx.current_frame_step % (ctx.target_fps * 2)),
								 (ctx.target_fps * 2));
	printf("[Trace] Atlases processed. Committing material grid dumps...\n");
	Diagnostics_DumpMaterialCompositionToDisk(runtime_xbm_handle);

	printf("[Trace] Validation pass finished. Component layout extraction complete.\n");

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
