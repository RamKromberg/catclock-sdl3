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

/* Converts standard context palette byte states cleanly to shader-ready normalized floating-point
 * channels arrays */
static void CatClock_NormalizeColorToUniform(SDL_Color src, float dest_array[4]) {
	dest_array[0] = (float) src.r / 255.0f;
	dest_array[1] = (float) src.g / 255.0f;
	dest_array[2] = (float) src.b / 255.0f;
	dest_array[3] = (float) src.a / 255.0f;
}

// File-scoped persistent texture handles isolating Sokol state away from the atlas engine
static sg_image hours_atlas_image_slot = { 0 };
static sg_image minutes_atlas_image_slot = { 0 };
static sg_image seconds_atlas_image_slot = { 0 };
static sg_image eyes_atlas_image_slot = { 0 };
static sg_image tail_atlas_image_slot = { 0 };

// REQUIREMENT STEP: Persistent view objects matching the pipeline binds array type requirements
static sg_view hours_atlas_view_slot = { 0 };
static sg_view minutes_atlas_view_slot = { 0 };
static sg_view seconds_atlas_view_slot = { 0 };
static sg_view eyes_atlas_view_slot = { 0 };
static sg_view tail_atlas_view_slot = { 0 };

void CatClock_BakeAtlasToVram(sg_image* target_vram_slot, sg_view* target_view_slot,
							  const uint8_t* raw_index_grid, int width, int height,
							  const char* label) {
	if (!raw_index_grid || width <= 0 || height <= 0)
		return;

	// 1. Clear any prior image and view allocations when a scale mutation occurs
	if (target_view_slot->id != SG_INVALID_ID) {
		if (sg_query_view_state(*target_view_slot) == SG_RESOURCESTATE_VALID) {
			sg_destroy_view(*target_view_slot);
		}
		target_view_slot->id = SG_INVALID_ID;
	}
	if (target_vram_slot->id != SG_INVALID_ID) {
		if (sg_query_image_state(*target_vram_slot) == SG_RESOURCESTATE_VALID) {
			sg_destroy_image(*target_vram_slot);
		}
		target_vram_slot->id = SG_INVALID_ID;
	}

	// 2. Build the uncompressed template sheet descriptor bounds
	sg_image_desc texture_blueprint
		= { .width = width,
			.height = height,
			.pixel_format = SG_PIXELFORMAT_R8,
			.data = { .mip_levels = { { .ptr = raw_index_grid,
										.size = (size_t) (width * height * sizeof(uint8_t)) } } },
			.label = label };

	// 3. Instantiate the image object allocation handle
	*target_vram_slot = sg_make_image(&texture_blueprint);

	// 4. REQUIREMENT STEP: Wrap the image handle safely inside an sg_view container matching page
	// 19 semantics
	if (sg_query_image_state(*target_vram_slot) == SG_RESOURCESTATE_VALID) {
		*target_view_slot = sg_make_view(
			&(sg_view_desc) { .texture = { .image = *target_vram_slot }, .label = label });
	}
}

// Dynamic layout packing helper ensuring sub-atlas bounds match scaled host footprints
void PackHandUvExtents(float* target_uv_array, int frame_index,
					   const CatClock_ComputeAtlas* atlas) {
	if (!atlas || !target_uv_array)
		return;

	int cols = 10; // Forced matrix column profile inside catclock_atlas.c [0.7]
	int col = frame_index % cols;
	int row = frame_index / cols;

	float c_w = (float) atlas->cell_w;
	float c_h = (float) atlas->cell_h;
	float a_w = (float) atlas->atlas_w;
	float a_h = (float) atlas->atlas_h;

	// Normalize coordinates smoothly to [0.0 - 1.0] texture space bounds
	target_uv_array[0] = (col * c_w) / a_w;
	target_uv_array[1] = (row * c_h) / a_h;
	target_uv_array[2] = ((col + 1) * c_w) / a_w;
	target_uv_array[3] = ((row + 1) * c_h) / a_h;
}

int main(int argc, char* argv[]) {
	printf("[Trace] Starting App Transition Runtime Execution Context.\n");
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fprintf(stderr, "[Fatal Error] SDL_Init subsystem initialization failure.\n");
		return 1;
	}

	/* Setup runtime configurations, custom frame limits, and color tokens */
	ParseCommandLineArguments(argc, argv, &ctx);

// =========================================================================
// COMPOSITOR DIRECT SURFACE ATTACHMENT HINTS (ZERO-HOST OVERHEAD)
// =========================================================================
// We disable host-side frame requests and let the compositor pull directly
// from the active VRAM context without forcing main loop thread cycles.
#if defined(__linux__) && !defined(__ANDROID__)
	SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, "0");
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
#endif

	/* RE-ALIGNED: Strictly capture unpadded asset bounds down to y-index 288 for tail clearance */
	float baseline_w = ctx.use_decorations ? DECORATED_CANVAS_W : 103.0f;
	float baseline_h = ctx.use_decorations ? DECORATED_CANVAS_H : 288.0f;

	float scale = (float) ctx.current_half_steps / 2.0f;
	int target_w = (int) lroundf(baseline_w * scale);
	int target_h = (int) lroundf(baseline_h * scale);

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
	printf("[Verification] Entering interactive runtime event processing layout loop...\n");

	/* --- SETUP METRIC ANCHORS --- */
	float current_scale = (float) ctx.current_half_steps / 2.0f;

	printf("\n[Diagnostic Metrics] === VIEWPORT BOUNDS VERIFICATION ===\n");
	printf("[Diagnostic Metrics] Initial Legacy Canvas Target Size: %.2f x %.2f px\n", baseline_w,
		   baseline_h);
	printf("[Diagnostic Metrics] Hardware Staging VRAM Atlas Canvas: %d x %d px\n", VRAM_TEX_WIDTH,
		   VRAM_TEX_HEIGHT);
	printf("[Diagnostic Metrics] Evaluated Vertex Map Max U Coordinate: %.4f\n", 103.0f / 128.0f);
	printf("[Diagnostic Metrics] Evaluated Vertex Map Max V Coordinate: %.4f\n", 284.0f / 290.0f);
	printf("[Diagnostic Metrics] Runtime Host Window Pixel Dimension: %d x %d px\n",
		   (int) (baseline_w * current_scale), (int) (baseline_h * current_scale));
	printf("[Diagnostic Metrics] =====================================\n\n");

	printf("\n[Diagnostic Audit] === SHADER UNIFORM BLOCK SIZE VERIFICATION ===\n");
	printf("[Diagnostic Audit] C Struct Layout Footprint (CatClock_ShaderUniforms): %zu bytes\n",
		   sizeof(CatClock_ShaderUniforms));
	printf("[Diagnostic Audit] Expected Shader Layout Footprint (cb_params_t):      %zu bytes\n",
		   sizeof(cb_params_t));
	printf("[Diagnostic Audit] =================================            =====\n\n");

	/* --- 1. COMMIT GEOMETRY MESHES TO GPU BUFFERS --- */
	CatClock_GpuVertex clock_vertices[] = {
		{ .pos = { -1.0f, 1.0f }, .uv = { 0.0f, 0.0f } }, /* Top Left */
		{ .pos = { 1.0f, 1.0f }, .uv = { 1.0f, 0.0f } }, /* Top Right */
		{ .pos = { -1.0f, -1.0f }, .uv = { 0.0f, 1.0f } }, /* Bottom Left */
		{ .pos = { 1.0f, -1.0f }, .uv = { 1.0f, 1.0f } } /* Bottom Right */
	};
	uint16_t clock_indices[] = { 0, 1, 2, 3 };

	ctx.vertex_buffer = sg_make_buffer(
		&(sg_buffer_desc) { .data = { .ptr = clock_vertices, .size = sizeof(clock_vertices) },
							.label = "ClockMeshVertexBuffer" });

	ctx.index_buffer = sg_make_buffer(
		&(sg_buffer_desc) { .usage = { .index_buffer = true },
							.data = { .ptr = clock_indices, .size = sizeof(clock_indices) },
							.label = "ClockMeshIndexBuffer" });

	/* --- 2. EXTRACT AND UNPACK STATIC ASSET SHARDS --- */
	uint8_t *catback_ptr = NULL, *tie_ptr = NULL, *catwhite_ptr = NULL, *eyes_ptr = NULL;
	int dummy_w = 0, dummy_h = 0;

	CatClock_GetCatbackData(runtime_xbm_handle, &catback_ptr, &dummy_w, &dummy_h);
	CatClock_GetCattieBodyData(runtime_xbm_handle, &tie_ptr, &dummy_w, &dummy_h);
	CatClock_GetCatwhiteData(runtime_xbm_handle, &catwhite_ptr, &dummy_w, &dummy_h);
	CatClock_GetEyesData(runtime_xbm_handle, &eyes_ptr, &dummy_w, &dummy_h);

	uint32_t* staging_pixels
		= (uint32_t*) malloc(VRAM_TEX_WIDTH * VRAM_TEX_HEIGHT * sizeof(uint32_t));
	if (!staging_pixels) {
		fprintf(stderr, "[Fatal] Failed to allocate staging VRAM unpack buffer.\n");
		return 1;
	}

	CatClock_UnpackStaticAssetsToStagingBuffer(staging_pixels, catback_ptr, tie_ptr, catwhite_ptr,
											   eyes_ptr);

#ifdef DEBUG_STAGING_PIXELS_DUMP
	/* --- PERSISTENT PAM SERIALIZATION DISK EXPORTER --- */
	FILE* audit_dump = fopen("vram_static_staging_dump.pam", "wb");
	if (audit_dump) {
		fputs("P7\n", audit_dump);
		fprintf(audit_dump, "WIDTH %d\n", VRAM_TEX_WIDTH);
		fprintf(audit_dump, "HEIGHT %d\n", VRAM_TEX_HEIGHT);
		fputs("DEPTH 4\n", audit_dump);
		fputs("MAXVAL 255\n", audit_dump);
		fputs("TUPLTYPE RGB_ALPHA\n", audit_dump);
		fputs("ENDHDR\n", audit_dump);

		long long r_act = 0, g_act = 0, b_act = 0, a_act = 0;
		for (int y = 0; y < VRAM_TEX_HEIGHT; y++) {
			for (int x = 0; x < VRAM_TEX_WIDTH; x++) {
				uint32_t raw_pixel = staging_pixels[y * VRAM_TEX_WIDTH + x];
				uint8_t body_mask = raw_pixel & 0xFF;
				uint8_t tie_mask = (raw_pixel >> 8) & 0xFF;
				uint8_t white_mask = (raw_pixel >> 16) & 0xFF;
				uint8_t eyes_mask = (raw_pixel >> 24) & 0xFF;

				if (body_mask > 0)
					r_act++;
				if (tie_mask > 0)
					g_act++;
				if (white_mask > 0)
					b_act++;
				if (eyes_mask > 0)
					a_act++;

				uint8_t out_pixel[4] = { 40, 40, 40, 0 };
				if (eyes_mask > 128) {
					out_pixel[0] = ctx.sclera_color.r;
					out_pixel[1] = ctx.sclera_color.g;
					out_pixel[2] = ctx.sclera_color.b;
					out_pixel[3] = 255;
				} else if (white_mask > 128) {
					out_pixel[0] = ctx.detail_color.r;
					out_pixel[1] = ctx.detail_color.g;
					out_pixel[2] = ctx.detail_color.b;
					out_pixel[3] = 255;
				} else if (tie_mask > 128) {
					out_pixel[0] = ctx.tie_color.r;
					out_pixel[1] = ctx.tie_color.g;
					out_pixel[2] = ctx.tie_color.b;
					out_pixel[3] = 255;
				} else if (body_mask > 128) {
					out_pixel[0] = ctx.cat_color.r;
					out_pixel[1] = ctx.cat_color.g;
					out_pixel[2] = ctx.cat_color.b;
					out_pixel[3] = 255;
				}
				fwrite(out_pixel, 1, 4, audit_dump);
			}
		}
		fclose(audit_dump);
		printf("[Pixel Integrity Audit] DETECTED MASK CHANNEL DENSITY REPORT:\n");
		printf("  -> R Channel (Body Nodes) Count   : %lld\n", r_act);
		printf("  -> G Channel (Necktie Nodes) Count: %lld\n", g_act);
		printf("  -> B Channel (White Nodes) Count  : %lld\n", b_act);
		printf("  -> A Channel (Eyes Sockets) Count : %lld\n", a_act);
	}
#endif

	/* --- 3. UPLOAD STATIC TEXTURES TO GPU VRAM --- */
	ctx.body_mask_texture = sg_make_image(&(sg_image_desc) {
		.width = VRAM_TEX_WIDTH,
		.height = VRAM_TEX_HEIGHT,
		.pixel_format = SG_PIXELFORMAT_RGBA8,
		.data
		= { .mip_levels[0] = { .ptr = staging_pixels,
							   .size = VRAM_TEX_WIDTH * VRAM_TEX_HEIGHT * sizeof(uint32_t) } },
		.label = "CatBodyMaskTexture" });

	ctx.body_mask_sampler = sg_make_sampler(&(sg_sampler_desc) { .min_filter = SG_FILTER_NEAREST,
																 .mag_filter = SG_FILTER_NEAREST,
																 .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
																 .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
																 .label = "CatBodyMaskSampler" });

	ctx.body_mask_view = sg_make_view(&(sg_view_desc) {
		.texture = { .image = ctx.body_mask_texture }, .label = "CatBodyMaskResourceView" });

	free(staging_pixels);
	printf("[VRAM Init] Static image buffer assets and samplers committed to GPU context.\n");

	/* --- 4. COMPILE GRAPHICS CONTEXT DRAW PIPELINE --- */
	sg_pipeline_desc pip_desc
		= { .shader = sg_make_shader(catclock_shader_desc(sg_query_backend())),
			.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
			.index_type = SG_INDEXTYPE_UINT16,
			.label = "ClockMainRenderingPipeline" };
	pip_desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
	pip_desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
	pip_desc.colors[0]
		= (sg_color_target_state) { .pixel_format = SG_PIXELFORMAT_RGBA8,
									.blend
									= { .enabled = true,
										.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
										.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
										.op_rgb = SG_BLENDOP_ADD,
										.src_factor_alpha = SG_BLENDFACTOR_ONE,
										.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
										.op_alpha = SG_BLENDOP_ADD } };

	ctx.draw_pipeline = sg_make_pipeline(&pip_desc);

	if (sg_query_pipeline_state(ctx.draw_pipeline) != SG_RESOURCESTATE_VALID) {
		fprintf(stderr, "[Fatal Error] GFX state pipeline compilation pass failed.\n");
		return 1;
	}
	printf("[Pipeline Init] Sokol drawing state maps compiled successfully.\n");

	/* --- 5. INITIALIZE ATLAS CONFIGURATIONS AND PRE-BAKE --- */
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

	ctx.texture_cache_stale = true;

	CatClock_RebakeComputeAtlas(NULL, &ctx.hours_atlas, 64, 96, TOTAL_HAND_PHASES, 10,
								CatClock_ShaderHands, &hour_cfg);
	CatClock_RebakeComputeAtlas(NULL, &ctx.minutes_atlas, 64, 96, TOTAL_HAND_PHASES, 10,
								CatClock_ShaderHands, &min_cfg);
	CatClock_RebakeComputeAtlas(NULL, &ctx.seconds_atlas, 64, 96, TOTAL_HAND_PHASES, 10,
								CatClock_ShaderHands, &sec_cfg);
	CatClock_RebakeComputeAtlas(NULL, &ctx.eyes_atlas, 64, 32, (ctx.target_fps * 2), 10,
								CatClock_ShaderEyes, NULL);
	CatClock_RebakeComputeAtlas(NULL, &ctx.tail_atlas, 96, 96, (ctx.target_fps * 2), 10,
								CatClock_ShaderTail, &tail_data);

#ifdef DEBUG_DUMP
	Diagnostics_DumpMaterialCompositionToDisk(runtime_xbm_handle);
	printf("[Trace] Dynamic textures cached and committed to disk files.\n");
#endif

	CatClock_BakeAtlasToVram(&hours_atlas_image_slot, &hours_atlas_view_slot,
							 ctx.hours_atlas.index_buffer, ctx.hours_atlas.atlas_w,
							 ctx.hours_atlas.atlas_h, "CatClock-HoursHands-Atlas");
	CatClock_BakeAtlasToVram(&minutes_atlas_image_slot, &minutes_atlas_view_slot,
							 ctx.minutes_atlas.index_buffer, ctx.minutes_atlas.atlas_w,
							 ctx.minutes_atlas.atlas_h, "CatClock-MinutesHands-Atlas");
	CatClock_BakeAtlasToVram(&seconds_atlas_image_slot, &seconds_atlas_view_slot,
							 ctx.seconds_atlas.index_buffer, ctx.seconds_atlas.atlas_w,
							 ctx.seconds_atlas.atlas_h, "CatClock-SecondsHands-Atlas");
	CatClock_BakeAtlasToVram(&eyes_atlas_image_slot, &eyes_atlas_view_slot,
							 ctx.eyes_atlas.index_buffer, ctx.eyes_atlas.atlas_w,
							 ctx.eyes_atlas.atlas_h, "CatClock-DynamicEyes-Atlas");
	CatClock_BakeAtlasToVram(&tail_atlas_image_slot, &tail_atlas_view_slot,
							 ctx.tail_atlas.index_buffer, ctx.tail_atlas.atlas_w,
							 ctx.tail_atlas.atlas_h, "CatClock-DynamicTail-Atlas");

	ctx.texture_cache_stale = false;

	// =========================================================================
	// PURE EVENT-DRIVEN CORE (Zero-Timer GPU Offloader Architecture)
	// =========================================================================
	/* === RESTORED RUNTIME TARGET BOUNDS === */
	bool running = true;
	SDL_Event event;

	// Calculate target frame delay duration in milliseconds
	int target_fps = (ctx.target_fps <= 0) ? DEFAULT_FPS : ctx.target_fps;
	Uint64 frame_delay_ms = 1000 / target_fps;

	Uint64 last_frame_time = SDL_GetTicks();
	bool force_redraw = true;

	printf("[Runtime Pacing] Main evaluation engine started. Target FPS: %d\n", target_fps);

	while (running) {
		Uint64 frame_start_ticks = SDL_GetTicks();

		/* 1. ASYNC NON-BLOCKING EVENT PUMP INTERFACE */
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			} else if (event.type == SDL_EVENT_KEY_DOWN) {
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
					float updated_scale = (float) ctx.current_half_steps / 2.0f;
					SDL_SetWindowSize(ctx.window, (int) lroundf(baseline_w * updated_scale),
									  (int) lroundf(baseline_h * updated_scale));
					force_redraw = true;
				}

			} else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
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
					float updated_scale = (float) ctx.current_half_steps / 2.0f;
					SDL_SetWindowSize(ctx.window, (int) lroundf(baseline_w * updated_scale),
									  (int) lroundf(baseline_h * updated_scale));
					force_redraw = true;
				}
			} else if (event.type == SDL_EVENT_WINDOW_EXPOSED
					   || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
					   || event.type == SDL_EVENT_WINDOW_SHOWN) {
				force_redraw = true;
			}
		}

		/* 2. DETERMINISTIC RENDERING PACING AND DISPATCH PASS */
		Uint64 current_ticks = SDL_GetTicks();

		// Enforce redraw if target frame timeline is due or explicitly requested by event
		if (force_redraw || (current_ticks - last_frame_time >= frame_delay_ms)) {
			force_redraw = false;
			last_frame_time = current_ticks;

			// ==========================================
			// 1. RE-BAKE AND CACHE SUB-ATLAS SHEETS IF TEXTURE STATE TRANSITIONS
			// ==========================================
			if (ctx.texture_cache_stale) {
				CatClock_RebakeComputeAtlas(NULL, &ctx.hours_atlas, 64, 96, TOTAL_HAND_PHASES, 10,
											CatClock_ShaderHands, &hour_cfg);
				CatClock_RebakeComputeAtlas(NULL, &ctx.minutes_atlas, 64, 96, TOTAL_HAND_PHASES, 10,
											CatClock_ShaderHands, &min_cfg);
				CatClock_RebakeComputeAtlas(NULL, &ctx.seconds_atlas, 64, 96, TOTAL_HAND_PHASES, 10,
											CatClock_ShaderHands, &sec_cfg);
				CatClock_RebakeComputeAtlas(NULL, &ctx.eyes_atlas, 64, 32, (ctx.target_fps * 2), 10,
											CatClock_ShaderEyes, NULL);
				CatClock_RebakeComputeAtlas(NULL, &ctx.tail_atlas, 96, 96, (ctx.target_fps * 2), 10,
											CatClock_ShaderTail, &tail_data);

#ifdef DEBUG_DUMP
				Diagnostics_DumpMaterialCompositionToDisk(runtime_xbm_handle);
				printf("[Trace] Dynamic textures cached and committed to disk files.\n");
#endif

				CatClock_BakeAtlasToVram(&hours_atlas_image_slot, &hours_atlas_view_slot,
										 ctx.hours_atlas.index_buffer, ctx.hours_atlas.atlas_w,
										 ctx.hours_atlas.atlas_h, "CatClock-HoursHands-Atlas");
				CatClock_BakeAtlasToVram(&minutes_atlas_image_slot, &minutes_atlas_view_slot,
										 ctx.minutes_atlas.index_buffer, ctx.minutes_atlas.atlas_w,
										 ctx.minutes_atlas.atlas_h, "CatClock-MinutesHands-Atlas");
				CatClock_BakeAtlasToVram(&seconds_atlas_image_slot, &seconds_atlas_view_slot,
										 ctx.seconds_atlas.index_buffer, ctx.seconds_atlas.atlas_w,
										 ctx.seconds_atlas.atlas_h, "CatClock-SecondsHands-Atlas");
				CatClock_BakeAtlasToVram(&eyes_atlas_image_slot, &eyes_atlas_view_slot,
										 ctx.eyes_atlas.index_buffer, ctx.eyes_atlas.atlas_w,
										 ctx.eyes_atlas.atlas_h, "CatClock-DynamicEyes-Atlas");
				CatClock_BakeAtlasToVram(&tail_atlas_image_slot, &tail_atlas_view_slot,
										 ctx.tail_atlas.index_buffer, ctx.tail_atlas.atlas_w,
										 ctx.tail_atlas.atlas_h, "CatClock-DynamicTail-Atlas");

				ctx.texture_cache_stale = false;
			}

			// ==========================================
			// 2. ALLOCATE AND CLEAR UNIFORM PAYLOAD MEMORY INSTANCE
			// ==========================================
			CatClock_ShaderUniforms shader_uniform_payload;
			memset(&shader_uniform_payload, 0, sizeof(CatClock_ShaderUniforms));

			// ==========================================
			// 3. HARDWARE-LOCKED INITIALIZATION TIME PASS
			// ==========================================
			static bool first_time_init = true;
			static Uint64 baseline_ticks = 0;
			static float cached_day_time_seconds = 0.0f;

			// Force calendar evaluation ONLY at boot or when scale factor is mutated (cache stale)
			if (first_time_init || ctx.texture_cache_stale) {
				time_t dynamic_raw_time = time(NULL);
				struct tm* local_time_segments = localtime(&dynamic_raw_time);

				cached_day_time_seconds
					= (float) (local_time_segments->tm_hour * 3600
							   + local_time_segments->tm_min * 60 + local_time_segments->tm_sec);

				baseline_ticks = SDL_GetTicks();
				first_time_init = false;
				printf("[Timer Sync] Hard resync against wall-time executed. Base Offset: %.2fs\n",
					   cached_day_time_seconds);
			}

			// Local memory timeline tracking via unified hardware timer deltas
			float elapsed_delta_seconds = (float) (SDL_GetTicks() - baseline_ticks) / 1000.0f;
			float computed_day_time_seconds = cached_day_time_seconds + elapsed_delta_seconds;

			// Translate monotonic timeline parameters cleanly into standard clock face frames
			int current_sec = (int) fmodf(computed_day_time_seconds, 60.0f);
			int current_min = (int) fmodf(computed_day_time_seconds / 60.0f, 60.0f);
			int current_hour = (int) fmodf(computed_day_time_seconds / 3600.0f, 12.0f);

			int sec_frame_idx = current_sec % 60;
			int min_frame_idx = current_min % 60;
			int hour_frame_idx = ((current_hour * 5) + (current_min / 12)) % 60;

			// ==========================================
			// 4. MAP AND NORMALIZE CORE MATERIAL GRAPHICS PALETTES
			// ==========================================
			CatClock_NormalizeColorToUniform(ctx.cat_color, shader_uniform_payload.cat_color);
			CatClock_NormalizeColorToUniform(ctx.tie_color, shader_uniform_payload.tie_color);
			CatClock_NormalizeColorToUniform(ctx.pupil_color, shader_uniform_payload.pupil_color);
			CatClock_NormalizeColorToUniform(ctx.sclera_color, shader_uniform_payload.sclera_color);
			CatClock_NormalizeColorToUniform(ctx.detail_color, shader_uniform_payload.detail_color);

			SDL_Color white_fallback = { 255, 255, 255, 255 };
			CatClock_NormalizeColorToUniform(white_fallback, shader_uniform_payload.halo_color);

			CatClock_NormalizeColorToUniform(ctx.hour_color, shader_uniform_payload.hour_color);
			CatClock_NormalizeColorToUniform(ctx.minute_color, shader_uniform_payload.minute_color);
			CatClock_NormalizeColorToUniform(ctx.second_color, shader_uniform_payload.second_color);

			// ====================================================================
			// 5. PACK EXPLICIT RENDER TARGET FRAME INDEXES (FULL CYCLE)
			// ====================================================================
			shader_uniform_payload.hour_frame_idx = hour_frame_idx;
			shader_uniform_payload.min_frame_idx = min_frame_idx;
			shader_uniform_payload.sec_frame_idx = sec_frame_idx;

			// Walk linearly through all 60 baked frames across the full 2-second period
			shader_uniform_payload.pendulum_frame_idx
				= (int) fmodf(computed_day_time_seconds * 30.0f, 60.0f);

			// ====================================================================
			// 6. TERMINAL PRINTF DIAGNOSTIC TELEMETRY CAPTURE MONITOR
			// ====================================================================
			static int dynamic_diagnostic_ticks = 0;
			if (dynamic_diagnostic_ticks++ % 30 == 0) {
				printf("\n[Pipeline Diagnostic Pass #%d]\n", dynamic_diagnostic_ticks);
				printf("  -> System Time Captured : %02d:%02d:%02d\n", current_hour, current_min,
					   current_sec);
				printf("  -> Target Frame Indexes : Hour=%d, Min=%d, Sec=%d, Pendulum=%d\n",
					   hour_frame_idx, min_frame_idx, sec_frame_idx,
					   shader_uniform_payload.pendulum_frame_idx);
				printf("  -> Hours Atlas Address  : %p | Shared hardware timelines synced.\n",
					   (void*) ctx.hours_atlas.index_buffer);
				fflush(stdout);
			}

			// ====================================================================
			// 7. RECORD AND DISPATCH GRAPHICS PIPELINE COMMANDS
			// ====================================================================
			int current_viewport_w = 0, current_viewport_h = 0;
			SDL_GetWindowSize(ctx.window, &current_viewport_w, &current_viewport_h);

			sg_pass_action clock_pass_clear_action = { 0 };
			clock_pass_clear_action.colors[0].load_action = SG_LOADACTION_CLEAR;
			clock_pass_clear_action.colors[0].clear_value = (sg_color) { 0.0f, 0.0f, 0.0f, 0.0f };

			sg_begin_pass(&(sg_pass) {
				.action = clock_pass_clear_action,
				.swapchain = { .width = current_viewport_w, .height = current_viewport_h } });

			sg_apply_pipeline(ctx.draw_pipeline);

			if (!ctx.use_decorations) {
				// Pin scale cleanly to individual monitor grid indices
				float single_pixel_scale = (float) current_viewport_w / 103.0f;

				// Shift left by 23 columns to leave a 1px spacer for the left outline path
				int negative_x_box = (int) lroundf(-23.0f * single_pixel_scale);
				int full_padded_w = (int) lroundf(128.0f * single_pixel_scale);

				sg_apply_viewport(negative_x_box, 0, full_padded_w, current_viewport_h, true);
			} else {
				sg_apply_viewport(0, 0, current_viewport_w, current_viewport_h, true);
			}

			sg_bindings clock_resource_bindings = { 0 };
			clock_resource_bindings.vertex_buffers[0] = ctx.vertex_buffer;
			clock_resource_bindings.index_buffer = ctx.index_buffer;
			clock_resource_bindings.samplers[0] = ctx.body_mask_sampler;

			clock_resource_bindings.views[VIEW_texture_sheet] = ctx.body_mask_view;
			clock_resource_bindings.views[VIEW_hours_hand_sheet] = hours_atlas_view_slot;
			clock_resource_bindings.views[VIEW_mins_hand_sheet] = minutes_atlas_view_slot;
			clock_resource_bindings.views[VIEW_seconds_hand_sheet] = seconds_atlas_view_slot;
			clock_resource_bindings.views[VIEW_eyes_sheet] = eyes_atlas_view_slot;
			clock_resource_bindings.views[VIEW_tail_sheet] = tail_atlas_view_slot;

			sg_apply_bindings(&clock_resource_bindings);
			sg_apply_uniforms(UB_cb_params, &SG_RANGE(shader_uniform_payload));

			sg_draw(0, 4, 1);
			sg_end_pass();
			sg_commit();

			SDL_GL_SwapWindow(ctx.window);
		}

		/* 3. CAP FRAME INTERVAL TO ENFORCE TARGET_FPS CAP (IF VSYNC IS DISABLED) */
		Uint64 loop_execution_duration = SDL_GetTicks() - frame_start_ticks;
		if (loop_execution_duration < frame_delay_ms) {
			SDL_Delay((Uint32) (frame_delay_ms - loop_execution_duration));
		}
	}

#ifdef DEBUG_DUMP
	printf("[Trace] Validation pass finished. Component layout extraction complete.\n");
#endif

	CatClock_DestroyComputeAtlas(&ctx.hours_atlas);
	CatClock_DestroyComputeAtlas(&ctx.minutes_atlas);
	CatClock_DestroyComputeAtlas(&ctx.seconds_atlas);
	CatClock_DestroyComputeAtlas(&ctx.eyes_atlas);
	CatClock_DestroyComputeAtlas(&ctx.tail_atlas);

	if (runtime_xbm_handle) {
		CatClock_DestroyXbmLibrary(runtime_xbm_handle);
		runtime_xbm_handle = NULL;
	}

	if (sg_query_view_state(hours_atlas_view_slot) == SG_RESOURCESTATE_VALID)
		sg_destroy_view(hours_atlas_view_slot);
	if (sg_query_view_state(minutes_atlas_view_slot) == SG_RESOURCESTATE_VALID)
		sg_destroy_view(minutes_atlas_view_slot);
	if (sg_query_view_state(seconds_atlas_view_slot) == SG_RESOURCESTATE_VALID)
		sg_destroy_view(seconds_atlas_view_slot);
	if (sg_query_view_state(eyes_atlas_view_slot) == SG_RESOURCESTATE_VALID)
		sg_destroy_view(eyes_atlas_view_slot);
	if (sg_query_view_state(tail_atlas_view_slot) == SG_RESOURCESTATE_VALID)
		sg_destroy_view(tail_atlas_view_slot);

	if (sg_query_pipeline_state(ctx.draw_pipeline) == SG_RESOURCESTATE_VALID) {
		sg_destroy_pipeline(ctx.draw_pipeline);
	}
	if (sg_query_buffer_state(ctx.vertex_buffer) == SG_RESOURCESTATE_VALID) {
		sg_destroy_buffer(ctx.vertex_buffer);
	}
	if (sg_query_buffer_state(ctx.index_buffer) == SG_RESOURCESTATE_VALID) {
		sg_destroy_buffer(ctx.index_buffer);
	}
	if (sg_query_image_state(ctx.body_mask_texture) == SG_RESOURCESTATE_VALID) {
		sg_destroy_image(ctx.body_mask_texture);
	}
	if (sg_query_sampler_state(ctx.body_mask_sampler) == SG_RESOURCESTATE_VALID) {
		sg_destroy_sampler(ctx.body_mask_sampler);
	}
	if (sg_query_view_state(ctx.body_mask_view) == SG_RESOURCESTATE_VALID) {
		sg_destroy_view(ctx.body_mask_view);
	}

	sg_shutdown();
	SDL_GL_MakeCurrent(ctx.window, NULL);
	SDL_GL_DestroyContext(gl_context);
	SDL_DestroyWindow(ctx.window);
	SDL_Quit();

	printf("[Trace] Execution Context terminated cleanly.\n");
	return 0;
}
