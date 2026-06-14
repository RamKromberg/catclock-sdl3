#include "catclock_shared.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <math.h>

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

CatClock_AppContext ctx = {0};

int ssaa_factor = 2;
int target_fps_limit = 30;

static SDL_HitTestResult SDLCALL WidgetWindowHitTest(SDL_Window *win, const SDL_Point *area, void *data) {
    (void)win; (void)area; (void)data;
    return SDL_HITTEST_DRAGGABLE;
}

static void PrintHelpDocumentation(const char *program_name) {
    printf("Kit-Cat Desktop Widget Clock (SDL3 Engine Re-Port)\n");
    printf("Usage: %s [flags]\n\n", program_name);
    printf("Available Flags:\n");
    printf("  --help              Display this interface parameter map.\n");
    printf("  -noseconds          Completely hide the sweeping seconds hand.\n");
    printf("  -nooutline          Disable the form-fitting 1px white halo background.\n");
    printf("  -decorations [hex]  Restore standard desktop borders & window title-bars with optional background color.\n");
    printf("  -notop              Disable the forced 'Always on Top' window layer pinning.\n");
    printf("  -fps [1-120]        Set a custom target frame rate pacing limit (Default: 30).\n");
    printf("  -catcolor [hex]     Override default black cat body base layout (Default: 000000).\n");
    printf("  -detailcolor [hex]  Override default white accents and sclera layout (Default: ffffff).\n");
    printf("  -tiecolor [hex]     Override default necktie hex color channel (Default: fdfdfd).\n");
    printf("  -pupilcolor [hex]   Override moving eye pupil hex color channel (Default: 000000).\n");
    printf("  -hourcolor [hex]    Override default hour clock hand hex color (Default: 000000).\n");
    printf("  -minutecolor [hex]  Override default minute clock hand hex color (Default: 000000).\n");
    printf("  -secondcolor [hex]  Override default sweeping second hand hex color (Default: ff0000).\n");
}

static bool HelperParseHexColor(const char *hex_str, SDL_Color *out_color) {
    if (hex_str[0] == '#') hex_str++;
    size_t len = strlen(hex_str);
    unsigned int r = 0, g = 0, b = 0;
    if (len == 6 && sscanf(hex_str, "%02x%02x%02x", &r, &g, &b) == 3) {
        out_color->r = (Uint8)r;
        out_color->g = (Uint8)g;
        out_color->b = (Uint8)b;
        out_color->a = 255;
        return true;
    } else if (len == 3 && sscanf(hex_str, "%1x%1x%1x", &r, &g, &b) == 3) {
        out_color->r = (Uint8)(r * 17);
        out_color->g = (Uint8)(g * 17);
        out_color->b = (Uint8)(b * 17);
        out_color->a = 255;
        return true;
    }
    return false;
}

static void ParseCommandLineArguments(int argc, char *argv[], CatClock_AppContext *ctx) {
    ctx->fg_color = (SDL_Color){ 0, 0, 0, 255 };
    ctx->bg_color = (SDL_Color){ 255, 255, 255, 255 };
    ctx->cat_color = (SDL_Color){ 0, 0, 0, 255 };
    ctx->tie_color = (SDL_Color){ 255, 255, 255, 255 };
    ctx->pupil_color = (SDL_Color){ 0, 0, 0, 255 };

    ctx->hour_color = (SDL_Color){ 0, 0, 0, 255 };
    ctx->minute_color = (SDL_Color){ 0, 0, 0, 255 };
    ctx->second_color = (SDL_Color){ 255, 0, 0, 255 };
    ctx->detail_color = (SDL_Color){ 255, 255, 255, 255 };
    ctx->sclera_color = (SDL_Color){ 255, 255, 255, 255 };
    ctx->window_bg_color = (SDL_Color){ 255, 255, 255, 255 };
    ctx->current_scale = 1.0f;

    ctx->hide_seconds = false;
    ctx->disable_outline = false;
    ctx->use_decorations = false;
    ctx->disable_always_on_top = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            PrintHelpDocumentation(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "-noseconds") == 0) {
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
                if (parsed_fps >= 1 && parsed_fps <= 120) target_fps_limit = parsed_fps;
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

int main(int argc, char *argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return 1;
    }

    ParseCommandLineArguments(argc, argv, &ctx);

    int target_w = (int)(BASELINE_CANVAS_W * ctx.current_scale);
    int target_h = (int)(BASELINE_CANVAS_H * ctx.current_scale);

    SDL_WindowFlags window_flags = 0;

    if (!ctx.use_decorations) {
        window_flags |=(SDL_WINDOW_BORDERLESS | SDL_WINDOW_TRANSPARENT);
    }
    if (!ctx.disable_always_on_top) {
        window_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    }

    SDL_Window *window = SDL_CreateWindow("CatClock-SDL3 Widget Core", target_w, target_h, window_flags);
    if (!window) {
        SDL_Quit();
        return 1;
    }

    SDL_SetWindowHitTest(window, WidgetWindowHitTest, NULL);

#ifdef _WIN32
    if (ctx.use_decorations) {
        SDL_PropertiesID window_props = SDL_GetWindowProperties(window);
        HWND hwnd = (HWND)SDL_GetPointerProperty(window_props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        if (hwnd) {
            LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
            style &= ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
            SetWindowLongPtr(hwnd, GWL_STYLE, style);
            SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
    }
#endif

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
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
                } else if (event.key.key == SDLK_EQUALS || event.key.key == SDLK_PLUS || event.key.key == SDLK_KP_PLUS) {
                    float old_scale = ctx.current_scale;
                    ctx.current_scale += 0.5f;
                    if (ctx.current_scale > 10.0f) ctx.current_scale = 10.0f;
                    if (ctx.current_scale != old_scale) {
                        SDL_SetWindowSize(window, (int)(BASELINE_CANVAS_W * ctx.current_scale), (int)(BASELINE_CANVAS_H * ctx.current_scale));
                        CatClock_OnWindowResize(NULL, &ctx, renderer);
                    }
                } else if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS) {
                    float old_scale = ctx.current_scale;
                    ctx.current_scale -= 0.5f;
                    if (ctx.current_scale < 0.5f) ctx.current_scale = 0.5f;
                    if (ctx.current_scale != old_scale) {
                        SDL_SetWindowSize(window, (int)(BASELINE_CANVAS_W * ctx.current_scale), (int)(BASELINE_CANVAS_H * ctx.current_scale));
                        CatClock_OnWindowResize(NULL, &ctx, renderer);
                    }
                }
        } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            float old_scale = ctx.current_scale;
            if (event.wheel.y > 0.0f)       ctx.current_scale += 0.5f;
            else if (event.wheel.y < 0.0f)  ctx.current_scale -= 0.5f;
            if (ctx.current_scale < 0.5f)   ctx.current_scale = 0.5f;
            if (ctx.current_scale > 10.0f)  ctx.current_scale = 10.0f;
            if (ctx.current_scale != old_scale) {
                SDL_SetWindowSize(window, (int)(BASELINE_CANVAS_W * ctx.current_scale), (int)(BASELINE_CANVAS_H * ctx.current_scale));
                CatClock_OnWindowResize(NULL, &ctx, renderer);
            }
        }
    }

    time_t raw_time = time(NULL);
    struct tm *time_info = localtime(&raw_time);
    int hour_phase = ((time_info->tm_hour % 12) * 5) + (time_info->tm_min / 12);
    int minute_phase = time_info->tm_min % 60;
    int second_phase = time_info->tm_sec % 60;

    float angle = (float)(SDL_GetTicks() % CYCLE_PERIOD_MS) / (float)CYCLE_PERIOD_MS * (2.0f * (float)M_PI);
    float sway_deg = sinf(angle) * 8.0f;

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
