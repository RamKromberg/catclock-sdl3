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

// clang-format off
#ifndef SOKOL_IMPL
#define SOKOL_IMPL
#endif
#include "sokol_gfx.h"
#include "sokol_log.h"

#include "catclock_shared.h"
#include "catclock_atlas.h"
// clang-format on

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define localtime(X) (_X64_or_X86_struct_tm_fallback(X))
static struct tm* _X64_or_X86_struct_tm_fallback(const time_t* timer) {
	static struct tm result;
	localtime_s(&result, timer);
	return &result;
}
#endif

#ifndef _WIN32
#include <malloc.h>
#endif

CatClock_AppContext ctx = { 0 };

int target_fps_limit = 30;

/**
 * Pixel-Perfect Drag Interception Callback
 * Normalizes global cursor coordinates and executes a 3-nanosecond bitwise memory check.
 */
static SDL_HitTestResult SDLCALL WidgetWindowHitTest(SDL_Window* win, const SDL_Point* area,
													 void* data) {
	(void) win;
	(void) data;

	// FIX: If standard window decorations, borders, and title bars are active,
	// pass handling completely back to the OS window manager for normal window dragging.
	if (ctx.use_decorations) {
		return SDL_HITTEST_NORMAL;
	}

	// Fallback to full window dragging if the custom hitbox asset failed to load
	if (!ctx.hitbox_bitmask) {
		return SDL_HITTEST_DRAGGABLE;
	}

	// Normalize incoming screen mouse coordinates down to unscaled 1-bit image grid space
	int x = (int) (area->x / ctx.current_scale);
	int y = (int) (area->y / ctx.current_scale);

	// Boundary check: Out of bounds points are transparent and click through to desktop
	if (x < 0 || x >= ctx.hitbox_mask_w || y < 0 || y >= ctx.hitbox_mask_h) {
		return SDL_HITTEST_NORMAL;
	}

	// Standard XBM row packing calculation (bits arranged horizontally, LSB first)
	int bytes_per_row = (ctx.hitbox_mask_w + 7) / 8;
	int byte_index = (y * bytes_per_row) + (x / 8);
	int bit_position = x % 8;

	// Read the specific visibility state byte flag
	bool is_solid = (ctx.hitbox_bitmask[byte_index] & (1 << bit_position)) != 0;

	return is_solid ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}

int main(int argc, char* argv[]) {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		return 1;
	}

	ParseCommandLineArguments(argc, argv, &ctx);

#ifdef CATCLOCK_TELEMETRY
	/* Collect and output running performance averages once every 5 seconds */
	ctx.metrics.logging_frequency = (target_fps_limit <= 0 ? 30 : target_fps_limit) * 5;
#endif

	float baseline_w = ctx.use_decorations ? DECORATED_CANVAS_W : 103.0f;
	float baseline_h = ctx.use_decorations ? DECORATED_CANVAS_H : 288.0f;

	int target_w = (int) (baseline_w * ctx.current_scale);
	int target_h = (int) (baseline_h * ctx.current_scale);

	SDL_WindowFlags window_flags = 0;

	if (!ctx.use_decorations) {
		window_flags |= (SDL_WINDOW_BORDERLESS | SDL_WINDOW_TRANSPARENT);
	}
	if (!ctx.disable_always_on_top) {
		window_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
	}

	SDL_Window* window
		= SDL_CreateWindow("CatClock-SDL3 Widget Core", target_w, target_h, window_flags);
	if (!window) {
		SDL_Quit();
		return 1;
	}

	SDL_SetWindowHitTest(window, WidgetWindowHitTest, NULL);

#ifdef _WIN32
	if (ctx.use_decorations) {
		SDL_PropertiesID window_props = SDL_GetWindowProperties(window);
		HWND hwnd
			= (HWND) SDL_GetPointerProperty(window_props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
		if (hwnd) {
			LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
			style &= ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
			SetWindowLongPtr(hwnd, GWL_STYLE, style);
			SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
						 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		}
	}
#endif

	SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
	if (!renderer) {
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	// -------------------------------------------------------------------------
	// Stage 4: Bootstrap Native Sokol GFX Subsystem in Background Validation
	// -------------------------------------------------------------------------
	sg_desc sokol_description = { .logger.func = slog_func };
	sg_setup(&sokol_description);

	if (sg_isvalid()) {
		SDL_Log("[SOKOL BOOTSTRAP] Graphics backend initialized successfully!");
	} else {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
					"[SOKOL ERROR] Failed to bind background GPU context.");
	}

	// -------------------------------------------------------------------------
	// STAGE 4: Early-2024 Sokol GFX Quad Mesh & Shader Pipeline Configuration
	// -------------------------------------------------------------------------
	// 1. Viewport-Filling Mesh (2D Positions X,Y + UV Standard Coordinates)
	float quad_vertices[] = {
		-1.0f, 1.0f,  0.0f, 0.0f, // Top-Left Corner
		1.0f,  1.0f,  1.0f, 0.0f, // Top-Right Corner
		-1.0f, -1.0f, 0.0f, 1.0f, // Bottom-Left Corner
		1.0f,  -1.0f, 1.0f, 1.0f // Bottom-Right Corner
	};

	sg_buffer_desc vertex_buffer_desc
		= { .data = SG_RANGE(quad_vertices), .label = "viewport-filling-quad-mesh" };
	sg_buffer quad_vbuf = sg_make_buffer(&vertex_buffer_desc);

	// 2. Early-2024 Direct Backend Shader Specification (vs and fs blocks)
	sg_shader_desc shader_desc = { .vs.source = "#version 330\n"
												"layout(location=0) in vec2 position;\n"
												"layout(location=1) in vec2 texcoord;\n"
												"out vec2 uv;\n"
												"void main() {\n"
												"    gl_Position = vec4(position, 0.0, 1.0);\n"
												"    uv = texcoord;\n"
												"}\n",
								   .fs.source = "#version 330\n"
												"in vec2 uv;\n"
												"out vec4 frag_color;\n"
												"void main() {\n"
												"    frag_color = vec4(uv.x, uv.y, 0.5, 1.0);\n"
												"}\n",
								   .label = "prototype-headless-shader" };
	sg_shader quad_shader = sg_make_shader(&shader_desc);

	// 3. Early-2024 Immutable Pipeline State Object Configuration
	sg_pipeline_desc pipeline_desc = {
    .shader = quad_shader,
    .layout = {
        .attrs = {
            [0] = { .format = SG_VERTEXFORMAT_FLOAT2 }, // position mapping entry
            [1] = { .format = SG_VERTEXFORMAT_FLOAT2 }  // texcoord mapping entry
        }
    },
    .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    .colors = {
        [0] = {
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
            }
        }
    },
    // Remainder of constraints omitted to automatically inherit target pass pixel format context
    .label = "headless-quad-pipeline"
};
	sg_pipeline quad_pipeline = sg_make_pipeline(&pipeline_desc);

	// Prevent unused variables compiler complaints during this validation stage
	(void) quad_vbuf;
	(void) quad_pipeline;

	SDL_GetRenderOutputSize(renderer, &ctx.current_win_w, &ctx.current_win_h);
	ctx.xbm_lib = CatClock_InitXbmLibrary(renderer);
	ctx.texture_cache_stale = true;

	ctx.hitbox_bitmask
		= CatClock_LoadRawXbmBits("./assets/hitbox.xbm", &ctx.hitbox_mask_w, &ctx.hitbox_mask_h);
	if (!ctx.hitbox_bitmask) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
					"Failed to load hitbox map. Full window drag fallback enabled.");
	}

	bool running = true;
	SDL_Event event;

	/* High-precision monotonic frame pacing metrics initialization */
	Uint32 target_fps = (target_fps_limit <= 0 ? 30 : target_fps_limit);
	Uint64 frame_delay_ticks = SDL_GetPerformanceFrequency() / target_fps;
	Uint64 next_frame_ticks = SDL_GetPerformanceCounter() + frame_delay_ticks;

	while (running) {
		/* Non-blocking event pump drains window messages immediately to prevent input lag */
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			} else if (event.type == SDL_EVENT_KEY_DOWN) {
				if (event.key.key == SDLK_ESCAPE) {
					running = false;
				} else if (event.key.key == SDLK_EQUALS || event.key.key == SDLK_PLUS
						   || event.key.key == SDLK_KP_PLUS) {
					float old_scale = ctx.current_scale;
					ctx.current_scale += 0.5f;
					if (ctx.current_scale > 10.0f)
						ctx.current_scale = 10.0f;
					if (ctx.current_scale != old_scale) {
						SDL_SetWindowSize(window, (int) roundf(baseline_w * ctx.current_scale),
										  (int) roundf(baseline_h * ctx.current_scale));
						CatClock_OnWindowResize(NULL, &ctx, renderer);
					}
				} else if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS) {
					float old_scale = ctx.current_scale;
					ctx.current_scale -= 0.5f;
					if (ctx.current_scale < 0.5f)
						ctx.current_scale = 0.5f;
					if (ctx.current_scale != old_scale) {
						SDL_SetWindowSize(window, (int) roundf(baseline_w * ctx.current_scale),
										  (int) roundf(baseline_h * ctx.current_scale));
						CatClock_OnWindowResize(NULL, &ctx, renderer);
					}
				}
			} else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
				float old_scale = ctx.current_scale;
				if (event.wheel.y > 0.0f) {
					ctx.current_scale += 0.5f;
				} else if (event.wheel.y < 0.0f) {
					ctx.current_scale -= 0.5f;
				}
				if (ctx.current_scale < 0.5f)
					ctx.current_scale = 0.5f;
				if (ctx.current_scale > 10.0f)
					ctx.current_scale = 10.0f;
				if (ctx.current_scale != old_scale) {
					SDL_SetWindowSize(window, (int) roundf(baseline_w * ctx.current_scale),
									  (int) roundf(baseline_h * ctx.current_scale));
					CatClock_OnWindowResize(NULL, &ctx, renderer);
				}
			}
		}

		Uint64 current_ticks = SDL_GetPerformanceCounter();

		/* Process clock calculations and rendering functions only when the frame deadline matches
		 */
#if !defined(CATCLOCK_SHOT) && !defined(CATCLOCK_DEBUG)
		if (current_ticks >= next_frame_ticks) {
#else
		/* Under simulation testing, advance frames unconditionally independent of frozen hardware
		 * clocks */
		if (1) {
#endif
			/* 1. Advance continuous asset animation state indices */
			ctx.current_frame_step++;

			/* 2. Map system wall clock time straight into discrete phase integers */
			time_t raw_time = time(NULL);
			struct tm* time_info = localtime(&raw_time);

			int hour_phase = 0;
			int minute_phase = 0;
			int second_phase = 0;

			if (time_info) {
				second_phase = time_info->tm_sec % 60;
				minute_phase = time_info->tm_min % 60;
				hour_phase = ((time_info->tm_hour % 12) * 5) + (time_info->tm_min / 12);
			}

			/* 3. Flush asset draw calls directly to the master composite target texture */
			CatClock_SynchronizePipelineAtlases(&renderer, &ctx, 0.0f, hour_phase, minute_phase,
												second_phase);

			/* 4. Display the composite layer graphics onto the window backbuffer */
			if (!ctx.is_window_minimized) {
				SDL_SetRenderTarget(renderer, NULL);
				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
				SDL_RenderClear(renderer);

				if (ctx.master_composite_layer) {
					SDL_SetTextureBlendMode(ctx.master_composite_layer, SDL_BLENDMODE_BLEND);
					SDL_FRect display_rect
						= { 0.0f, 0.0f, (float) ctx.current_win_w, (float) ctx.current_win_h };
					SDL_RenderTexture(renderer, ctx.master_composite_layer, NULL, &display_rect);
				}
				SDL_RenderPresent(renderer);
/*
Dumps single frame.
Use with sweep_phases.sh.
*/
#ifdef CATCLOCK_SHOT
				// PACING DELAY: Slow down execution to prevent the renderer from skipping frames
				SDL_Delay(5);

				if (ctx.current_frame_step >= 120) {
					SDL_Surface* screen_surf = SDL_RenderReadPixels(renderer, NULL);
					if (screen_surf) {
						SDL_Surface* final_rgba
							= SDL_ConvertSurface(screen_surf, SDL_PIXELFORMAT_RGBA32);
						if (final_rgba) {
							SDL_SavePNG(final_rgba, "catclock_shot_target_raw.png");
							SDL_DestroySurface(final_rgba);
						}
						SDL_DestroySurface(screen_surf);
					}
					exit(0);
				}
#endif

/*
Dumps Frames.
Use with sweep_cycle.sh.
*/
#ifdef CATCLOCK_DEBUG
				// Isolated diagnostic block evaluating directly on the presentation backbuffer
				static int live_loop_sequence_counter = 0;

				if (live_loop_sequence_counter >= 60 && live_loop_sequence_counter <= 120) {
					SDL_Texture* old_target = SDL_GetRenderTarget(renderer);
					SDL_SetRenderTarget(renderer, NULL);

					SDL_Surface* screen_surf = SDL_RenderReadPixels(renderer, NULL);
					if (screen_surf) {
						SDL_Surface* final_rgba
							= SDL_ConvertSurface(screen_surf, SDL_PIXELFORMAT_RGBA32);
						if (final_rgba) {
							char filename_buffer[256];
							SDL_snprintf(filename_buffer, sizeof(filename_buffer),
										 "catclock_live_frame_%d.png", live_loop_sequence_counter);

							if (SDL_SavePNG(final_rgba, filename_buffer)) {
								SDL_Log("=== [MAIN DIAG] SUCCESS -> Wrote image file: %s ===",
										filename_buffer);
							}
							SDL_DestroySurface(final_rgba);
						}
						SDL_DestroySurface(screen_surf);
					}
					SDL_SetRenderTarget(renderer, old_target);

					// DELAY COOLDOWN: Gives the file system 50ms to finish writing to disk
					SDL_Delay(150);
				}
				live_loop_sequence_counter++;
#endif
			}

			/* Advance target timeline step cleanly */
			next_frame_ticks += frame_delay_ticks;
		}

		/* Restored Original Backup Loop Pacing */
		current_ticks = SDL_GetPerformanceCounter();

		if (current_ticks < next_frame_ticks) {
			Uint64 remaining_ticks = next_frame_ticks - current_ticks;
			Uint64 sleep_ns = (remaining_ticks * 1000000000ULL) / SDL_GetPerformanceFrequency();
			if (sleep_ns > 0) {
#if !defined(CATCLOCK_SHOT) && !defined(CATCLOCK_DEBUG)
				SDL_DelayNS(sleep_ns);
#else
				/* Prevent frozen time-deficit spinlocks from freezing application startup frames */
				(void) sleep_ns;
#endif
			}
		}
	}

	/* VRAM Cleanup passes */
	CatClock_DestroyComputeAtlas(&ctx.hands_atlas);
	CatClock_DestroyComputeAtlas(&ctx.minutes_atlas);
	CatClock_DestroyComputeAtlas(&ctx.seconds_atlas);
	CatClock_DestroyComputeAtlas(&ctx.eyes_atlas);
	CatClock_DestroyComputeAtlas(&ctx.tail_atlas);

	if (ctx.master_composite_layer) {
		SDL_DestroyTexture(ctx.master_composite_layer);
	}

	if (ctx.hitbox_bitmask) {
		SDL_free(ctx.hitbox_bitmask);
		ctx.hitbox_bitmask = NULL;
	}

	CatClock_DestroyXbmLibrary(ctx.xbm_lib);

	// CRITICAL PIVOT: Destroy the conflicting SDL renderer first.
	// This detaches the windows bindings so we can exit cleanly.
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	// Bypass explicit Sokol handle deletion calls to prevent driver-side latching.
	// The runtime environment handles total VRAM reclamation upon process exit.
	if (sg_isvalid()) {
		sg_shutdown();
	}

	SDL_Quit();
	return 0;
}
