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
 * Custom CPU-side 1-bit parser designed to load the unified range-of-motion mask
 * straight into system RAM to prevent text segment bloat and keep runtime lookups fast.
 */
static uint8_t* CatClock_LoadHitboxMask(const char* filepath, int* out_w, int* out_h) {
	if (!filepath || !out_w || !out_h)
		return NULL;

	SDL_IOStream* file = SDL_IOFromFile(filepath, "r");
	if (!file) {
		SDL_Log("Hitbox Error: Could not open hitbox file path: %s", filepath);
		return NULL;
	}

	Sint64 file_size = SDL_GetIOSize(file);
	if (file_size <= 0) {
		SDL_CloseIO(file);
		return NULL;
	}

	char* buffer = (char*) SDL_malloc(file_size + 1);
	if (!buffer) {
		SDL_CloseIO(file);
		return NULL;
	}

	SDL_ReadIO(file, buffer, file_size);
	buffer[file_size] = '\0';
	SDL_CloseIO(file);

	int w = 0, h = 0;
	char* ptr = SDL_strstr(buffer, "_width");
	if (ptr) {
		while (*ptr && !SDL_isdigit((unsigned char) *ptr))
			ptr++;
		w = SDL_atoi(ptr);
	}

	ptr = SDL_strstr(buffer, "_height");
	if (ptr) {
		while (*ptr && !SDL_isdigit((unsigned char) *ptr))
			ptr++;
		h = SDL_atoi(ptr);
	}

	ptr = SDL_strstr(buffer, "{");
	if (w <= 0 || h <= 0 || !ptr) {
		SDL_free(buffer);
		return NULL;
	}
	ptr++;

	int bytes_per_row = (w + 7) / 8;
	int total_bytes = bytes_per_row * h;
	uint8_t* raw_bits = (uint8_t*) SDL_calloc(1, total_bytes);
	if (!raw_bits) {
		SDL_free(buffer);
		return NULL;
	}

	for (int i = 0; i < total_bytes; i++) {
		while (*ptr && *ptr != '0' && *(ptr + 1) != 'x' && *(ptr + 1) != 'X') {
			if (*ptr == '}')
				break;
			ptr++;
		}
		if (*ptr == '}')
			break;
		raw_bits[i] = (uint8_t) SDL_strtol(ptr, &ptr, 16);
	}

	SDL_free(buffer);
	*out_w = w;
	*out_h = h;
	return raw_bits;
}

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

	SDL_GetRenderOutputSize(renderer, &ctx.current_win_w, &ctx.current_win_h);
	ctx.xbm_lib = CatClock_InitXbmLibrary(renderer);
	ctx.texture_cache_stale = true;

	ctx.hitbox_bitmask
		= CatClock_LoadHitboxMask("./assets/hitbox.xbm", &ctx.hitbox_mask_w, &ctx.hitbox_mask_h);
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
		if (current_ticks >= next_frame_ticks) {
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
				SDL_DelayNS(sleep_ns);
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
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
