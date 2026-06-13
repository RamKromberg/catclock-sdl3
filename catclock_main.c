/* =============================================================================
   FILE: catclock_main.c (Application Entrypoint Linker Module)
   ============================================================================= */
#include "catclock_shared.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <math.h>

int ssaa_factor = 2;
int target_fps_limit = 60;

static SDL_HitTestResult SDLCALL WidgetWindowHitTest(SDL_Window *win, const SDL_Point *area, void *data) {
    (void)win; (void)area; (void)data;
    return SDL_HITTEST_DRAGGABLE;
}

static void ParseCommandLineArguments(int argc, char *argv[], CatClock_AppContext *ctx) {
    ctx->fg_color = (SDL_Color){ 0, 0, 0, 255 };
    ctx->bg_color = (SDL_Color){ 255, 255, 255, 255 };
    ctx->current_scale = 1.0f;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-fps") == 0 && (i + 1) < argc) {
            target_fps_limit = atoi(argv[++i]);
            if (target_fps_limit < 1) target_fps_limit = 60;
        }
        else if (strcmp(argv[i], "-fg") == 0 && (i + 1) < argc) {
            char *color_str = argv[++i];
            if (strlen(color_str) == 6) {
                unsigned int r, g, b;
                if (sscanf(color_str, "%02x%02x%02x", &r, &g, &b) == 3) {
                    ctx->fg_color = (SDL_Color){ (Uint8)r, (Uint8)g, (Uint8)b, 255 };
                }
            }
        }
        else if (strcmp(argv[i], "-bg") == 0 && (i + 1) < argc) {
            char *color_str = argv[++i];
            if (strlen(color_str) == 6) {
                unsigned int r, g, b;
                if (sscanf(color_str, "%02x%02x%02x", &r, &g, &b) == 3) {
                    ctx->bg_color = (SDL_Color){ (Uint8)r, (Uint8)g, (Uint8)b, 255 };
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;

    CatClock_AppContext ctx = {0};
    ParseCommandLineArguments(argc, argv, &ctx);

    int target_w = (int)(BASELINE_CANVAS_W * ctx.current_scale);
    int target_h = (int)(BASELINE_CANVAS_H * ctx.current_scale);

    SDL_Window *window = SDL_CreateWindow(
        "CatClock-SDL3 Widget Core",
        target_w, target_h,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_TRANSPARENT | SDL_WINDOW_ALWAYS_ON_TOP
    );

    if (!window) {
        SDL_Quit();
        return 1;
    }

    SDL_SetWindowHitTest(window, WidgetWindowHitTest, NULL);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* Initialize Clean Stateless Targets */
    ctx.hands_atlas.texture = NULL;
    ctx.hands_atlas.src_rects = NULL;
    ctx.hands_atlas.last_scale = -1.0f;

    ctx.minutes_atlas.texture = NULL;
    ctx.minutes_atlas.src_rects = NULL;
    ctx.minutes_atlas.last_scale = -1.0f;

    ctx.seconds_atlas.texture = NULL;
    ctx.seconds_atlas.src_rects = NULL;
    ctx.seconds_atlas.last_scale = -1.0f;

    ctx.eyes_atlas.texture = NULL;
    ctx.eyes_atlas.src_rects = NULL;
    ctx.eyes_atlas.last_scale = -1.0f;

    ctx.tail_atlas.texture = NULL;
    ctx.tail_atlas.src_rects = NULL;
    ctx.tail_atlas.last_scale = -1.0f;

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
            }
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                /* Handle Precise Keyboard Zoom Bindings */
                else if (event.key.key == SDLK_EQUALS || event.key.key == SDLK_PLUS || event.key.key == SDLK_KP_PLUS) {
                    float old_scale = ctx.current_scale;
                    ctx.current_scale += 0.5f;
                    if (ctx.current_scale > 10.0f) ctx.current_scale = 10.0f;

                    if (ctx.current_scale != old_scale) {
                        int next_w = (int)(BASELINE_CANVAS_W * ctx.current_scale);
                        int next_h = (int)(BASELINE_CANVAS_H * ctx.current_scale);
                        SDL_SetWindowSize(window, next_w, next_h);
                        CatClock_OnWindowResize(NULL, &ctx, renderer);
                    }
                }
                else if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS) {
                    float old_scale = ctx.current_scale;
                    ctx.current_scale -= 0.5f;
                    if (ctx.current_scale < 0.5f) ctx.current_scale = 0.5f;

                    if (ctx.current_scale != old_scale) {
                        int next_w = (int)(BASELINE_CANVAS_W * ctx.current_scale);
                        int next_h = (int)(BASELINE_CANVAS_H * ctx.current_scale);
                        SDL_SetWindowSize(window, next_w, next_h);
                        CatClock_OnWindowResize(NULL, &ctx, renderer);
                    }
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                float old_scale = ctx.current_scale;
                if (event.wheel.y > 0.0f)      ctx.current_scale += 0.5f;
                else if (event.wheel.y < 0.0f) ctx.current_scale -= 0.5f;

                if (ctx.current_scale < 0.5f)  ctx.current_scale = 0.5f;
                if (ctx.current_scale > 10.0f) ctx.current_scale = 10.0f;

                if (ctx.current_scale != old_scale) {
                    int next_w = (int)(BASELINE_CANVAS_W * ctx.current_scale);
                    int next_h = (int)(BASELINE_CANVAS_H * ctx.current_scale);
                    SDL_SetWindowSize(window, next_w, next_h);
                    CatClock_OnWindowResize(NULL, &ctx, renderer);
                }
            }
        }

        time_t raw_time = time(NULL);
        struct tm *time_info = localtime(&raw_time);
        int hour_phase = ((time_info->tm_hour % 12) * 5) + (time_info->tm_min / 12);
        int minute_phase = time_info->tm_min % 60;
        int second_phase = time_info->tm_sec % 60;

        float current_phase_angle = (float)(SDL_GetTicks() % CYCLE_PERIOD_MS) / (float)CYCLE_PERIOD_MS * (2.0f * (float)M_PI);
        float sway_deg = sinf(current_phase_angle) * 8.0f;

        CatClock_SynchronizePipelineAtlases(renderer, &ctx, sway_deg, hour_phase, minute_phase, second_phase);

        if (!ctx.is_window_minimized) {
            SDL_SetRenderTarget(renderer, NULL);
            SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);

            if (ctx.master_composite_layer) {
                SDL_FRect display_rect = { 0.0f, 0.0f, (float)ctx.current_win_w, (float)ctx.current_win_h };
                SDL_RenderTexture(renderer, ctx.master_composite_layer, NULL, &display_rect);
            }
            SDL_RenderPresent(renderer);
        }

        Uint64 current_ticks = SDL_GetPerformanceCounter();
        Uint64 elapsed_ticks = current_ticks - last_frame_ticks;
        if (elapsed_ticks < frame_delay_ticks) {
            Uint64 remaining_ticks = frame_delay_ticks - elapsed_ticks;
            Uint64 sleep_ms = (remaining_ticks * 1000) / SDL_GetPerformanceFrequency();
            if (sleep_ms > 0) SDL_Delay((Uint32)sleep_ms);
        }
        last_frame_ticks = SDL_GetPerformanceCounter();
    }

    CatClock_DestroyComputeAtlas(&ctx.hands_atlas);
    CatClock_DestroyComputeAtlas(&ctx.minutes_atlas);
    CatClock_DestroyComputeAtlas(&ctx.seconds_atlas);
    CatClock_DestroyComputeAtlas(&ctx.eyes_atlas);
    CatClock_DestroyComputeAtlas(&ctx.tail_atlas);

    if (ctx.master_composite_layer) SDL_DestroyTexture(ctx.master_composite_layer);
    CatClock_DestroyXbmLibrary(ctx.xbm_lib);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
