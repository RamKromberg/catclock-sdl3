/******************************************************************************
 * File Name:    catclock_atlas.c
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

/* Match the private layout from catclock_hands.c to guarantee memory alignment */
typedef struct {
	int hand_type;
	SDL_Color color;
} HandShaderConfig;

int CompareFloats(const void* a, const void* b) {
	float fa = *(const float*) a;
	float fb = *(const float*) b;
	return (fa > fb) - (fa < fb);
}

void CatClock_OnWindowResize(SDL_WindowEvent* resize_event, CatClock_AppContext* ctx,
							 SDL_Renderer* renderer) {
	(void) resize_event;
	int output_w = 0, output_h = 0;
	if (SDL_GetRenderOutputSize(renderer, &output_w, &output_h)) {
		if (output_w != ctx->current_win_w || output_h != ctx->current_win_h) {
			ctx->current_win_w = output_w;
			ctx->current_win_h = output_h;
			ctx->texture_cache_stale = true;
		}
	}
}

/**
 * CommitBufferToSDL (Refactoring Phase Stage 3 Bridge)
 * Translates palette indices into 32-bit colors.
 * Note: Temporarily implemented as a non-breaking pipeline shim
 * to preserve complete application compilation.
 */
void CommitBufferToSDL(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas) {
	if (!atlas || !atlas->texture) {
		return;
	}

	(void) renderer; // Silence unused parameter warning safely

	// Initialize with a default fallback color (Cat Body Color)
	SDL_Color uniform_palette = ctx.cat_color;

	// Check specific atlas identity bindings to synchronize hand and eye overrides cleanly
	if (atlas == &ctx.hands_atlas) {
		uniform_palette = ctx.hour_color;
	} else if (atlas == &ctx.minutes_atlas) {
		uniform_palette = ctx.minute_color;
	} else if (atlas == &ctx.seconds_atlas) {
		uniform_palette = ctx.second_color;
	} else if (atlas == &ctx.tail_atlas) {
		uniform_palette = ctx.cat_color; // Maps to -catcolor
	}

	// Apply color modulations safely to immediate hardware texture layers
	if (atlas == &ctx.hands_atlas || atlas == &ctx.minutes_atlas || atlas == &ctx.seconds_atlas
		|| atlas == &ctx.tail_atlas) {
		SDL_SetTextureColorMod(atlas->texture, uniform_palette.r, uniform_palette.g,
							   uniform_palette.b);
		SDL_SetTextureAlphaMod(atlas->texture, uniform_palette.a);
	} else if (atlas == &ctx.eyes_atlas) {
		// BYPASS MODULATION: The eyes rely on internal procedural transparency masks.
		// We restore original blending by resetting modulation to full white/opaque
		// until the 8-bit palette buffer refactoring is implemented.
		SDL_SetTextureColorMod(atlas->texture, 255, 255, 255);
		SDL_SetTextureAlphaMod(atlas->texture, 255);
	}
}

void CatClock_RebakeComputeAtlas(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas,
								 int cell_base_w, int cell_base_h, int total_frames, int cols,
								 CatClock_ShaderCallback shader, void* userdata) {
	int win_w = 0, win_h = 0;
	SDL_GetRenderOutputSize(renderer, &win_w, &win_h);

	CATCLOCK_LOG_DIAG(&ctx, "win_w=%d, win_h=%d, scale=%.4f, frames=%d", win_w, win_h,
					  ctx.current_scale, total_frames);

	float baseline_w = ctx.use_decorations ? DECORATED_CANVAS_W : 103.0f;
	float scale = (float) win_w / baseline_w;
	if (scale <= 0.0f)
		scale = 1.0f;
	scale = roundf(scale * 2.0f) / 2.0f;

	if (atlas->texture && scale == atlas->last_scale && total_frames == atlas->total_frames) {
		return;
	}

	CatClock_DestroyComputeAtlas(atlas);

	atlas->total_frames = total_frames;
	atlas->last_scale = scale;

	atlas->cell_w = (int) ceilf((float) cell_base_w * scale) + 2;
	atlas->cell_h = (int) ceilf((float) cell_base_h * scale) + 2;

#ifdef CATCLOCK_DIAGNOSTIC
	/* --- ATLAS DIMENSION MONITOR --- */
	SDL_Log("[DIAG REBAKE CELL] base=%dx%d -> calculated=%dx%d, final_scale=%.4f", cell_base_w,
			cell_base_h, atlas->cell_w, atlas->cell_h, scale);
	/* -------------------------------- */
#endif

	atlas->src_rects = (SDL_FRect*) SDL_malloc(sizeof(SDL_FRect) * total_frames);
	if (!atlas->src_rects)
		return;

	int rows = (total_frames + cols - 1) / cols;
	int atlas_w = cols * atlas->cell_w;
	int atlas_h = rows * atlas->cell_h;

	// Sticking to 16-bit format to ensure perfect texture alpha blending and pixel art downscaling
	atlas->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA4444, SDL_TEXTUREACCESS_TARGET,
									   atlas_w, atlas_h);
	if (!atlas->texture) {
		SDL_free(atlas->src_rects);
		atlas->src_rects = NULL;
		return;
	}

	SDL_SetTextureBlendMode(atlas->texture, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(atlas->texture, SDL_SCALEMODE_NEAREST);

	SDL_Texture* old_target = SDL_GetRenderTarget(renderer);
	SDL_SetRenderTarget(renderer, atlas->texture);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	for (int i = 0; i < total_frames; i++) {
		int cell_x = (i % cols) * atlas->cell_w;
		int cell_y = (i / cols) * atlas->cell_h;

		atlas->src_rects[i]
			= (SDL_FRect) { (float) cell_x + 1.0f, (float) cell_y + 1.0f,
							(float) atlas->cell_w - 2.0f, (float) atlas->cell_h - 2.0f };

		SDL_Rect viewport = { cell_x, cell_y, atlas->cell_w, atlas->cell_h };
		SDL_SetRenderViewport(renderer, &viewport);

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
		SDL_FRect local_cell_rect = { 0.0f, 0.0f, (float) atlas->cell_w, (float) atlas->cell_h };
		SDL_RenderFillRect(renderer, &local_cell_rect);

		SDL_Rect inner_render_viewport
			= { cell_x + 1, cell_y + 1, atlas->cell_w - 2, atlas->cell_h - 2 };
		SDL_SetRenderViewport(renderer, &inner_render_viewport);

		shader(renderer, atlas->cell_w - 2, atlas->cell_h - 2, scale, i, userdata);
	}

	SDL_SetRenderViewport(renderer, NULL);
	SDL_SetRenderTarget(renderer, old_target);

	// CRITICAL LIFE CYCLE HOOK: Invoke the translation bridge pass to synchronize colors cleanly
	CommitBufferToSDL(renderer, atlas);
}

void CatClock_DestroyComputeAtlas(CatClock_ComputeAtlas* atlas) {
	if (atlas->texture) {
		SDL_DestroyTexture(atlas->texture);
		atlas->texture = NULL;
	}
	if (atlas->src_rects) {
		SDL_free(atlas->src_rects);
		atlas->src_rects = NULL;
	}
	atlas->total_frames = 0;
	atlas->cell_w = 0;
	atlas->cell_h = 0;
	atlas->last_scale = -1.0f;
}

void CatClock_ShaderTailHaloBake(SDL_Renderer* renderer, int cell_w, int cell_h, float scale,
								 int frame_idx, void* userdata) {
	CatClock_TailShaderArgs* blueprint_args = (CatClock_TailShaderArgs*) userdata;

	float offsets_x[] = { -1.0f, 1.0f, 0.0f, 0.0f, -1.0f, -1.0f, 1.0f, 1.0f };
	float offsets_y[] = { 0.0f, 0.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f };

	for (int i = 0; i < 8; i++) {
		CatClock_TailShaderArgs pass_args;
		pass_args.app_ctx = blueprint_args ? blueprint_args->app_ctx : NULL;
		pass_args.offset_x = offsets_x[i] * scale;
		pass_args.offset_y = offsets_y[i] * scale;
		pass_args.force_halo_color = true;

		CatClock_ShaderTail(renderer, cell_w, cell_h, scale, frame_idx, &pass_args);
	}
}

#ifdef CATCLOCK_SHOT
// Clean duplication of the diagnostic dumper stripped of real-time wall-clock uptime limits
void CatClock_DiagnosticShotDump(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas) {
	if (!renderer || !atlas)
		return;

	static int multi_frame_sequence_counter = 0;

	// Unconditionally evaluate frame counts and capture exactly on frame index tick 60
	if (multi_frame_sequence_counter == 60) {
		SDL_Texture* old_target = SDL_GetRenderTarget(renderer);
		SDL_SetRenderTarget(renderer, NULL);

		SDL_Surface* screen_surf = SDL_RenderReadPixels(renderer, NULL);
		if (screen_surf) {
			SDL_Surface* final_rgba = SDL_ConvertSurface(screen_surf, SDL_PIXELFORMAT_RGBA8888);
			if (final_rgba) {
				char filename_buffer[256];
				snprintf(filename_buffer, sizeof(filename_buffer), "catclock_shot_target_raw.png");

				SDL_SavePNG(final_rgba, filename_buffer);
				SDL_Log("=== [SHOT ENGINE] SINGLE-FRAME PERSPECTIVE CAPTURED -> %s ===",
						filename_buffer);
				SDL_DestroySurface(final_rgba);
			}
			SDL_DestroySurface(screen_surf);
		}
		SDL_SetRenderTarget(renderer, old_target);
	}
	multi_frame_sequence_counter++;
}
#endif

#ifdef CATCLOCK_TELEMETRY
void CatClock_TelemetryBegin(CatClock_TelemetryFence* fence) {
	if (!fence)
		return;
	fence->start_ticks = SDL_GetPerformanceCounter();
}

void CatClock_TelemetryEnd(CatClock_TelemetryFence* fence, Uint32 report_interval,
						   const char* token_identifier) {
	if (!fence || fence->start_ticks == 0)
		return;

	Uint64 end_ticks = SDL_GetPerformanceCounter();
	Uint64 elapsed = end_ticks - fence->start_ticks;

	fence->rolling_accumulator += elapsed;
	fence->sample_counter++;

	if (fence->sample_counter >= report_interval) {
		Uint64 avg_ticks = fence->rolling_accumulator / fence->sample_counter;
		double frequency = (double) SDL_GetPerformanceFrequency();
		double avg_milliseconds = ((double) avg_ticks * 1000.0) / frequency;

		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
					"[PERF_TELEMETRY] %s over %u frames: Avg Ticks: %llu | Avg Time: %.4f ms\n",
					token_identifier, report_interval, (unsigned long long) avg_ticks,
					avg_milliseconds);

		fence->rolling_accumulator = 0;
		fence->sample_counter = 0;
	}
	fence->start_ticks = 0;
}
#endif

void CatClock_SynchronizePipelineAtlases(SDL_Renderer** renderer_ptr, CatClock_AppContext* ctx,
										 float sway_deg, int hour_phase, int minute_phase,
										 int second_phase) {
	if (!ctx || !renderer_ptr || !*renderer_ptr)
		return;

	SDL_Renderer* renderer = *renderer_ptr;
	(void) sway_deg;
	bool actual_disable_outline = ctx->disable_outline;

	int win_w = 0, win_h = 0;
	SDL_GetRenderOutputSize(renderer, &win_w, &win_h);
	float baseline_w = ctx->use_decorations ? DECORATED_CANVAS_W : 103.0f;
	float runtime_scale = (float) win_w / baseline_w;
	if (runtime_scale <= 0.01f)
		runtime_scale = 1.0f;

	/* Snap the primary scale factor to eliminate fractional window rounding noise */
	runtime_scale = roundf(runtime_scale * 2.0f) / 2.0f;

	int calculated_cell_w = (int) ceilf(64.0f * runtime_scale);
	int calculated_cell_h = (int) ceilf(32.0f * runtime_scale);

	if (calculated_cell_w < 64)
		calculated_cell_w = 64;
	if (calculated_cell_h < 32)
		calculated_cell_h = 32;

#ifdef CATCLOCK_DIAGNOSTIC
	/* --- RUNTIME LAYOUT SCALE DIAGNOSTIC --- */
	static float last_logged_scale = -1.0f;
	if (runtime_scale != last_logged_scale) {
		SDL_Log("[CATCLOCK LOG] Scale Multiplier Calibrated To: %.4f\n", runtime_scale);
		last_logged_scale = runtime_scale;
	}
	CATCLOCK_LOG_DIAG(ctx, "calculated_cell=%dx%d, decorations=%s", calculated_cell_w,
					  calculated_cell_h, ctx->use_decorations ? "TRUE" : "FALSE");
	/* --------------------------------------- */
#endif

	static CatClock_ComputeAtlas tail_halo_blueprint = { 0 };

	if (!ctx->master_composite_layer || ctx->texture_cache_stale || !ctx->eye_clipping_stencil) {

		/* ----------------------------------------------------------------- */
		/* CONTEXT RECYCLING ENG-BLOCK: FLUSH EVERY ANONYMOUS DRIVER SHEET  */
		/* ----------------------------------------------------------------- */
		/* 1. Demolish logical pre-baked compute atlases */
		CatClock_DestroyComputeAtlas(&ctx->hands_atlas);
		CatClock_DestroyComputeAtlas(&ctx->minutes_atlas);
		CatClock_DestroyComputeAtlas(&ctx->seconds_atlas);
		CatClock_DestroyComputeAtlas(&ctx->eyes_atlas);
		CatClock_DestroyComputeAtlas(&ctx->tail_atlas);
		CatClock_DestroyComputeAtlas(&tail_halo_blueprint);

		if (ctx->master_composite_layer) {
			SDL_DestroyTexture(ctx->master_composite_layer);
			ctx->master_composite_layer = NULL;
		}
		if (ctx->eye_clipping_stencil) {
			SDL_DestroyTexture(ctx->eye_clipping_stencil);
			ctx->eye_clipping_stencil = NULL;
		}

		/* 2. Discard original 1x structural texture templates */
		if (ctx->xbm_lib) {
			CatClock_DestroyXbmLibrary(ctx->xbm_lib);
			ctx->xbm_lib = NULL;
		}

		/* 3. Re-instantiate the core hardware renderer engine instance */
		SDL_Window* parent_window = SDL_GetRenderWindow(renderer);
		if (parent_window) {
			SDL_DestroyRenderer(renderer);
			renderer = SDL_CreateRenderer(parent_window, NULL);
			*renderer_ptr = renderer; /* Export updated driver address back to main */

			/* Reload base XBM templates into the pristine context */
			ctx->xbm_lib = CatClock_InitXbmLibrary(renderer);
		}
		/* ----------------------------------------------------------------- */

		ctx->master_composite_layer
			= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
								ctx->current_win_w, ctx->current_win_h);
		ctx->eye_clipping_stencil
			= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
								calculated_cell_w, calculated_cell_h);

		HandShaderConfig hour_cfg = { HAND_TYPE_HOUR, ctx->hour_color };
		HandShaderConfig minute_cfg = { HAND_TYPE_MINUTE, ctx->minute_color };
		HandShaderConfig second_cfg = { HAND_TYPE_SECOND, ctx->second_color };

		int calculated_fps_limit = target_fps_limit <= 0 ? 30 : target_fps_limit;
		int dynamic_cycle_frames = calculated_fps_limit * 2;

		CatClock_RebakeComputeAtlas(renderer, &ctx->hands_atlas, 64, 96, TOTAL_PHASES, 6,
									CatClock_ShaderHands, &hour_cfg);
		CatClock_RebakeComputeAtlas(renderer, &ctx->minutes_atlas, 64, 96, TOTAL_PHASES, 6,
									CatClock_ShaderHands, &minute_cfg);
		CatClock_RebakeComputeAtlas(renderer, &ctx->seconds_atlas, 64, 96, TOTAL_PHASES, 6,
									CatClock_ShaderHands, &second_cfg);
		CatClock_RebakeComputeAtlas(renderer, &ctx->eyes_atlas, 64, 32, dynamic_cycle_frames, 10,
									CatClock_ShaderEyes, NULL);

		CatClock_TailShaderArgs main_tail_args = { ctx, 0.0f, 0.0f, false };
		main_tail_args.app_ctx->cat_color = ctx->cat_color;
		CatClock_RebakeComputeAtlas(renderer, &ctx->tail_atlas, 96, 96, dynamic_cycle_frames, 8,
									CatClock_ShaderTail, &main_tail_args);

		if (!actual_disable_outline) {
			CatClock_TailShaderArgs halo_bake_args = { ctx, 0.0f, 0.0f, true };
			CatClock_RebakeComputeAtlas(renderer, &tail_halo_blueprint, 96, 96,
										dynamic_cycle_frames, 8, CatClock_ShaderTailHaloBake,
										&halo_bake_args);
		}

		ctx->texture_cache_stale = false;
	}

	SDL_Texture* old_target = SDL_GetRenderTarget(renderer);
	SDL_SetRenderTarget(renderer, ctx->master_composite_layer);

#ifdef CATCLOCK_CHROMA
	// CHROMA KEY BACKDROP: Force a solid, intense blue backdrop pass with full opaque alpha!
	// This turns all empty transparent space into an unambiguous high-contrast visual signal.
	SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
#else
	if (ctx->use_decorations) {
		SDL_SetRenderDrawColor(renderer, ctx->window_bg_color.r, ctx->window_bg_color.g,
							   ctx->window_bg_color.b, ctx->window_bg_color.a);
	} else {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	}
#endif

	SDL_RenderClear(renderer);

	SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);

	float render_pad_x = ctx->use_decorations ? CHOP_OFFSET_X : 0.0f;
	float render_pad_y = ctx->use_decorations ? CHOP_OFFSET_Y : 0.0f;
	float visual_pad = ctx->use_decorations ? 0.0f : 1.0f;

	if (!actual_disable_outline) {
		CatClock_RenderHaloOutline(ctx->xbm_lib, renderer, ctx->bg_color);
	}

	/* 1. Pre-Baked Pendulum & Halo Blit Pass */
#ifdef CATCLOCK_TELEMETRY
	CatClock_TelemetryBegin(&ctx->metrics.tail_halo);
#endif
	if (ctx->tail_atlas.texture) {
		int total_tail_frames = ctx->tail_atlas.total_frames;
		int tail_idx = (int) (ctx->current_frame_step % total_tail_frames);
		if (tail_idx >= total_tail_frames)
			tail_idx = total_tail_frames - 1;
		if (tail_idx < 0)
			tail_idx = 0;

		SDL_FRect base_dst = { floorf((3.0f + render_pad_x + visual_pad) * runtime_scale),
							   floorf((190.0f + render_pad_y + visual_pad) * runtime_scale),
							   96.0f * runtime_scale, 96.0f * runtime_scale };
#ifdef CATCLOCK_DIAGNOSTIC
		/* --- COORDINATE TARGET PASSPORT DIAGNOSTIC --- */
		SDL_Log("[DIAG LAYOUT TARGETS] pad_x=%.1f, pad_y=%.1f, tail_dst.x=%.1f, tail_dst.y=%.1f",
				render_pad_x, render_pad_y, base_dst.x, base_dst.y);
		/* --------------------------------------------- */
#endif
		if (!actual_disable_outline && tail_halo_blueprint.texture) {
			SDL_RenderTexture(renderer, tail_halo_blueprint.texture,
							  &tail_halo_blueprint.src_rects[tail_idx], &base_dst);
		}
		SDL_RenderTexture(renderer, ctx->tail_atlas.texture, &ctx->tail_atlas.src_rects[tail_idx],
						  &base_dst);
	}
#ifdef CATCLOCK_TELEMETRY
	CatClock_TelemetryEnd(&ctx->metrics.tail_halo, ctx->metrics.logging_frequency,
						  "SHUFFLE_TAIL_HALO");
#endif
	CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catback", ctx->cat_color,
								  render_pad_x + visual_pad, render_pad_y + visual_pad);
	CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catwhite", ctx->detail_color,
								  render_pad_x + visual_pad, render_pad_y + visual_pad);
	CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "cattie", ctx->tie_color,
								  render_pad_x + visual_pad, render_pad_y + visual_pad);

	/* 2. Moving Eyes Blit Shuffling Pass */
#ifdef CATCLOCK_TELEMETRY
	CatClock_TelemetryBegin(&ctx->metrics.eyes_pupils);
#endif
	if (ctx->eyes_atlas.texture) {
		int total_eye_frames = ctx->eyes_atlas.total_frames;
		int frame_idx = (int) (ctx->current_frame_step % total_eye_frames);
		if (frame_idx < 0)
			frame_idx = 0;

		SDL_FRect eyes_src_rect = ctx->eyes_atlas.src_rects[frame_idx];
		SDL_FRect eyes_dst_rect
			= { floorf((20.0f + render_pad_x + visual_pad) * runtime_scale),
				floorf((16.0f + render_pad_y + visual_pad) * runtime_scale),
				((float) ctx->eyes_atlas.cell_w - 1.0f), ((float) ctx->eyes_atlas.cell_h - 1.0f) };

		SDL_RenderTexture(renderer, ctx->eyes_atlas.texture, &eyes_src_rect, &eyes_dst_rect);
	}
#ifdef CATCLOCK_TELEMETRY
	CatClock_TelemetryEnd(&ctx->metrics.eyes_pupils, ctx->metrics.logging_frequency,
						  "SHUFFLE_EYES_PUPILS");
#endif
	/* 3. Pre-Baked Clock Hands Blit Pass */
#ifdef CATCLOCK_TELEMETRY
	CatClock_TelemetryBegin(&ctx->metrics.clock_hands);
#endif
	if (ctx->hands_atlas.texture && ctx->minutes_atlas.texture && ctx->seconds_atlas.texture) {
		int h_idx = hour_phase < 0 || hour_phase >= TOTAL_PHASES ? 0 : hour_phase;
		int m_idx = minute_phase < 0 || minute_phase >= TOTAL_PHASES ? 0 : minute_phase;
		int s_idx = second_phase < 0 || second_phase >= TOTAL_PHASES ? 0 : second_phase;

		SDL_FRect hands_dst_rect
			= { floorf((18.0f + render_pad_x + visual_pad) * runtime_scale),
				floorf((92.0f + render_pad_y + visual_pad) * runtime_scale),
				(float) ctx->hands_atlas.cell_w, (float) ctx->hands_atlas.cell_h };

		/* Optimized Zero-Cost Shuffling Loop Pass */
		SDL_RenderTexture(renderer, ctx->hands_atlas.texture, &ctx->hands_atlas.src_rects[h_idx],
						  &hands_dst_rect);
		SDL_RenderTexture(renderer, ctx->minutes_atlas.texture,
						  &ctx->minutes_atlas.src_rects[m_idx], &hands_dst_rect);

		if (!ctx->hide_seconds) {
			SDL_RenderTexture(renderer, ctx->seconds_atlas.texture,
							  &ctx->seconds_atlas.src_rects[s_idx], &hands_dst_rect);
		}
	}
#ifdef CATCLOCK_TELEMETRY
	CatClock_TelemetryEnd(&ctx->metrics.clock_hands, ctx->metrics.logging_frequency,
						  "SHUFFLE_CLOCK_HANDS");
#endif

#ifdef CATCLOCK_SHOT
	if (ctx->tail_atlas.texture) {
		CatClock_DiagnosticShotDump(renderer, &ctx->tail_atlas);
	}
#endif

#ifdef CATCLOCK_SHOT
	if (ctx->tail_atlas.texture) {
		CatClock_DiagnosticShotDump(renderer, &ctx->tail_atlas);
	}
#endif

	SDL_SetRenderTarget(renderer, old_target);
	*renderer_ptr = renderer;
}
