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

int ssaa_factor = 2;
int target_fps_limit = 30;

static SDL_HitTestResult SDLCALL WidgetWindowHitTest(SDL_Window* win, const SDL_Point* area,
													 void* data) {
	(void) win;
	(void) area;
	(void) data;
	return SDL_HITTEST_DRAGGABLE;
}

int main(int argc, char* argv[]) {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		return 1;
	}

	/* Hand off parsing to dedicated parsing channel component */
	ParseCommandLineArguments(argc, argv, &ctx);

	int target_w = (int) (BASELINE_CANVAS_W * ctx.current_scale);
	int target_h = (int) (BASELINE_CANVAS_H * ctx.current_scale);

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

	bool running = true;
	SDL_Event event;
	Uint64 frame_delay_ticks = SDL_GetPerformanceFrequency() / target_fps_limit;
	Uint64 last_frame_ticks = SDL_GetPerformanceCounter();

	while (running) {
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
						SDL_SetWindowSize(window, (int) (BASELINE_CANVAS_W * ctx.current_scale),
										  (int) (BASELINE_CANVAS_H * ctx.current_scale));
						CatClock_OnWindowResize(NULL, &ctx, renderer);
					}
				} else if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS) {
					float old_scale = ctx.current_scale;
					ctx.current_scale -= 0.5f;
					if (ctx.current_scale < 0.5f)
						ctx.current_scale = 0.5f;
					if (ctx.current_scale != old_scale) {
						SDL_SetWindowSize(window, (int) (BASELINE_CANVAS_W * ctx.current_scale),
										  (int) (BASELINE_CANVAS_H * ctx.current_scale));
						CatClock_OnWindowResize(NULL, &ctx, renderer);
					}
				}
			} else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
				float old_scale = ctx.current_scale;
				if (event.wheel.y > 0.0f)
					ctx.current_scale += 0.5f;
				else if (event.wheel.y < 0.0f)
					ctx.current_scale -= 0.5f;
				if (ctx.current_scale < 0.5f)
					ctx.current_scale = 0.5f;
				if (ctx.current_scale > 10.0f)
					ctx.current_scale = 10.0f;
				if (ctx.current_scale != old_scale) {
					SDL_SetWindowSize(window, (int) (BASELINE_CANVAS_W * ctx.current_scale),
									  (int) (BASELINE_CANVAS_H * ctx.current_scale));
					CatClock_OnWindowResize(NULL, &ctx, renderer);
				}
			}
		}

		time_t raw_time = time(NULL);
		struct tm* time_info = localtime(&raw_time);
		int hour_phase = ((time_info->tm_hour % 12) * 5) + (time_info->tm_min / 12);
		int minute_phase = time_info->tm_min % 60;
		int second_phase = time_info->tm_sec % 60;

		float angle = (float) (SDL_GetTicks() % CYCLE_PERIOD_MS) / (float) CYCLE_PERIOD_MS
			* (2.0f * (float) M_PI);
		float sway_deg = sinf(angle) * 8.0f;

		CatClock_SynchronizePipelineAtlases(renderer, &ctx, sway_deg, hour_phase, minute_phase,
											second_phase);

		if (!ctx.is_window_minimized) {
			SDL_SetRenderTarget(renderer, NULL);
			SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
			SDL_RenderClear(renderer);

			if (ctx.master_composite_layer) {
				SDL_FRect display_rect
					= { 0.0f, 0.0f, (float) ctx.current_win_w, (float) ctx.current_win_h };
				SDL_RenderTexture(renderer, ctx.master_composite_layer, NULL, &display_rect);
			}
			SDL_RenderPresent(renderer);
		}

		Uint64 current_ticks = SDL_GetPerformanceCounter();
		Uint64 elapsed_ticks = current_ticks - last_frame_ticks;
		if (elapsed_ticks < frame_delay_ticks) {
			Uint64 remaining_ticks = frame_delay_ticks - elapsed_ticks;
			Uint64 sleep_ms = (remaining_ticks * 1000) / SDL_GetPerformanceFrequency();
			if (sleep_ms > 0)
				SDL_Delay((Uint32) sleep_ms);
		}
		last_frame_ticks = SDL_GetPerformanceCounter();
	}

	CatClock_DestroyComputeAtlas(&ctx.hands_atlas);
	CatClock_DestroyComputeAtlas(&ctx.minutes_atlas);
	CatClock_DestroyComputeAtlas(&ctx.seconds_atlas);
	CatClock_DestroyComputeAtlas(&ctx.eyes_atlas);
	CatClock_DestroyComputeAtlas(&ctx.tail_atlas);

	if (ctx.master_composite_layer)
		SDL_DestroyTexture(ctx.master_composite_layer);
	CatClock_DestroyXbmLibrary(ctx.xbm_lib);

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
