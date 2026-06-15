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

extern void CatClock_ShaderTail(SDL_Renderer* renderer, int cell_w, int cell_h, float scale,
								int frame_idx, const SDL_Color* color);
extern void CatClock_ShaderHands(SDL_Renderer* renderer, int cell_w, int cell_h, float scale,
								 int frame_idx, void* userdata);
extern void CatClock_ShaderEyes(SDL_Renderer* renderer, int cell_w, int cell_h, float scale,
								int frame_idx, void* userdata);

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

void CatClock_RebakeComputeAtlas(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas,
								 int cell_base_w, int cell_base_h, int total_frames, int cols,
								 CatClock_ShaderCallback shader, void* userdata) {
	int win_w = 0, win_h = 0;
	SDL_GetRenderOutputSize(renderer, &win_w, &win_h);
	float scale = (float) win_w / BASELINE_CANVAS_W;
	if (scale <= 0.0f)
		scale = 1.0f;

	if (atlas->texture && scale == atlas->last_scale && total_frames == atlas->total_frames) {
		return;
	}

	CatClock_DestroyComputeAtlas(atlas);

	atlas->total_frames = total_frames;
	atlas->last_scale = scale;
	atlas->cell_w = (int) ceilf((float) cell_base_w * scale);
	atlas->cell_h = (int) ceilf((float) cell_base_h * scale);

	atlas->src_rects = (SDL_FRect*) SDL_malloc(sizeof(SDL_FRect) * total_frames);
	if (!atlas->src_rects)
		return;

	int rows = (total_frames + cols - 1) / cols;
	int atlas_w = cols * atlas->cell_w;
	int atlas_h = rows * atlas->cell_h;

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

		atlas->src_rects[i] = (SDL_FRect) { (float) cell_x, (float) cell_y, (float) atlas->cell_w,
											(float) atlas->cell_h };

		SDL_Rect viewport = { cell_x, cell_y, atlas->cell_w, atlas->cell_h };
		SDL_SetRenderViewport(renderer, &viewport);

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);

		SDL_FRect local_cell_rect = { 0.0f, 0.0f, (float) atlas->cell_w, (float) atlas->cell_h };
		SDL_RenderFillRect(renderer, &local_cell_rect);

		shader(renderer, atlas->cell_w, atlas->cell_h, scale, i, userdata);
	}

	SDL_SetRenderViewport(renderer, NULL);
	SDL_SetRenderTarget(renderer, old_target);
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
	(void) userdata;
	SDL_Color white_cfg = { 255, 255, 255, 255 };

	float offsets_x[] = { -1.0f, 1.0f, 0.0f, 0.0f, -1.0f, -1.0f, 1.0f, 1.0f };
	float offsets_y[] = { 0.0f, 0.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f };

	SDL_Rect orig_viewport;
	SDL_GetRenderViewport(renderer, &orig_viewport);

	for (int i = 0; i < 8; i++) {
		SDL_Rect offset_viewport
			= { orig_viewport.x + (int) (offsets_x[i] * scale),
				orig_viewport.y + (int) (offsets_y[i] * scale), orig_viewport.w, orig_viewport.h };
		SDL_SetRenderViewport(renderer, &offset_viewport);
		CatClock_ShaderTail(renderer, cell_w, cell_h, scale, frame_idx, &white_cfg);
	}
	SDL_SetRenderViewport(renderer, &orig_viewport);
}

#ifdef CATCLOCK_DEBUG
void CatClock_DumpEyesAtlasPixels(SDL_Renderer* renderer, void* ctx_ptr) {
	(void) ctx_ptr;
	if (!renderer || !ctx.eyes_atlas.texture)
		return;

	SDL_FRect first_cell = ctx.eyes_atlas.src_rects;
	int cell_w = (int) roundf(first_cell.w);
	int cell_h = (int) roundf(first_cell.h);
	if (cell_w <= 0 || cell_h <= 0)
		return;

	SDL_Texture* target_backup = SDL_GetRenderTarget(renderer);
	SDL_SetRenderTarget(renderer, ctx.eyes_atlas.texture);

	SDL_Rect read_rect = { (int) roundf(first_cell.x), (int) roundf(first_cell.y), cell_w, cell_h };
	SDL_Surface* dump_surf = SDL_RenderReadPixels(renderer, &read_rect);
	if (dump_surf) {
		SDL_Surface* rgba_surf = SDL_ConvertSurface(dump_surf, SDL_PIXELFORMAT_RGBA8888);
		if (rgba_surf) {
			Uint32* pixel_buffer = (Uint32*) rgba_surf->pixels;
			SDL_Log("=== DYNAMIC REBAKE DIAGNOSTIC (SCALE: %.4f | PIXELS: %dx%d) ===",
					ctx.current_scale, cell_w, cell_h);
			for (int y = 0; y < cell_h; y++) {
				char line_buffer[1024] = { 0 };
				int offset = 0;
				for (int x = 0; x < cell_w; x++) {
					Uint32 pixel = pixel_buffer[y * cell_w + x];
					Uint8 alpha = (pixel >> 24) & 0xFF;
					if (alpha == 0)
						offset += snprintf(line_buffer + offset, sizeof(line_buffer) - offset, " ");
					else if (alpha < 64)
						offset += snprintf(line_buffer + offset, sizeof(line_buffer) - offset, ".");
					else if (alpha < 192)
						offset += snprintf(line_buffer + offset, sizeof(line_buffer) - offset, "x");
					else
						offset += snprintf(line_buffer + offset, sizeof(line_buffer) - offset, "M");
				}
				printf("%s\n", line_buffer);
			}
			SDL_Log("====================================================");
			SDL_DestroySurface(rgba_surf);
		}
		SDL_DestroySurface(dump_surf);
	}
	SDL_SetRenderTarget(renderer, target_backup);
}
#endif

void CatClock_SynchronizePipelineAtlases(SDL_Renderer* renderer, CatClock_AppContext* ctx,
										 float sway_deg, int hour_phase, int minute_phase,
										 int second_phase) {
#ifdef CATCLOCK_DEBUG
	// 1. CHRONOGRAPH ENTRY TIME CAPTURE
	uint64_t pipeline_start_ticks = SDL_GetPerformanceCounter();
#endif
	if (!ctx || !renderer)
		return;
	(void) sway_deg;

	bool actual_disable_outline = ctx->disable_outline || ctx->use_decorations;

	int win_w = 0, win_h = 0;
	SDL_GetRenderOutputSize(renderer, &win_w, &win_h);
	float runtime_scale = (float) win_w / BASELINE_CANVAS_W;
	if (runtime_scale <= 0.0f)
		runtime_scale = 1.0f;

	int calculated_cell_w = (int) ceilf(24.0f * runtime_scale);
	int calculated_cell_h = (int) ceilf(24.0f * runtime_scale);
	if (calculated_cell_w < 24)
		calculated_cell_w = 24;
	if (calculated_cell_h < 24)
		calculated_cell_h = 24;

	if (!ctx->master_composite_layer || ctx->texture_cache_stale || !ctx->eye_clipping_stencil) {
		if (ctx->master_composite_layer) {
			SDL_DestroyTexture(ctx->master_composite_layer);
			ctx->master_composite_layer = NULL;
		}
		if (ctx->eye_clipping_stencil) {
			SDL_DestroyTexture(ctx->eye_clipping_stencil);
			ctx->eye_clipping_stencil = NULL;
		}
		CatClock_DestroyComputeAtlas(&ctx->hands_atlas);
		CatClock_DestroyComputeAtlas(&ctx->minutes_atlas);
		CatClock_DestroyComputeAtlas(&ctx->seconds_atlas);
		CatClock_DestroyComputeAtlas(&ctx->eyes_atlas);
		CatClock_DestroyComputeAtlas(&ctx->tail_atlas);

		ctx->master_composite_layer
			= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
								ctx->current_win_w, ctx->current_win_h);

		ctx->eye_clipping_stencil
			= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
								calculated_cell_w, calculated_cell_h);

		ctx->texture_cache_stale = false;
	}

	SDL_Color atlas_base_mask = { 255, 255, 255, 255 };
	HandShaderConfig hour_cfg = { HAND_TYPE_HOUR, atlas_base_mask };
	HandShaderConfig minute_cfg = { HAND_TYPE_MINUTE, atlas_base_mask };
	HandShaderConfig second_cfg = { HAND_TYPE_SECOND, atlas_base_mask };

	CatClock_RebakeComputeAtlas(renderer, &ctx->hands_atlas, 64, 96, TOTAL_PHASES, 6,
								CatClock_ShaderHands, &hour_cfg);
	CatClock_RebakeComputeAtlas(renderer, &ctx->minutes_atlas, 64, 96, TOTAL_PHASES, 6,
								CatClock_ShaderHands, &minute_cfg);
	CatClock_RebakeComputeAtlas(renderer, &ctx->seconds_atlas, 64, 96, TOTAL_PHASES, 6,
								CatClock_ShaderHands, &second_cfg);
	CatClock_RebakeComputeAtlas(renderer, &ctx->eyes_atlas, 64, 32, target_fps_limit * 2, 10,
								CatClock_ShaderEyes, NULL);
	CatClock_RebakeComputeAtlas(renderer, &ctx->tail_atlas, 96, 96, 60, 8,
								(CatClock_ShaderCallback) CatClock_ShaderTail, &atlas_base_mask);

#ifdef CATCLOCK_DEBUG
	/* Trigger exclusively inside targeted diagnostic debug build scenarios */
	CatClock_DumpEyesAtlasPixels(renderer, NULL);
#endif

	static CatClock_ComputeAtlas tail_halo_blueprint = { 0 };
	if (!actual_disable_outline) {
		CatClock_RebakeComputeAtlas(renderer, &tail_halo_blueprint, 96, 96, 60, 8,
									CatClock_ShaderTailHaloBake, NULL);
	}

	SDL_Texture* old_target = SDL_GetRenderTarget(renderer);
	SDL_SetRenderTarget(renderer, ctx->master_composite_layer);

	if (ctx->use_decorations) {
		SDL_SetRenderDrawColor(renderer, ctx->window_bg_color.r, ctx->window_bg_color.g,
							   ctx->window_bg_color.b, ctx->window_bg_color.a);
	} else {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	}
	SDL_RenderClear(renderer);

	SDL_SetRenderLogicalPresentation(renderer, 150, 300, SDL_LOGICAL_PRESENTATION_LETTERBOX);

	if (!actual_disable_outline) {
		CatClock_RenderHaloOutline(ctx->xbm_lib, renderer, ctx->bg_color);
	}

	CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catwhite", ctx->detail_color, 0.0f,
								  0.0f);

	if (ctx->tail_atlas.texture) {
		float phase_ratio = (float) (SDL_GetTicks() % CYCLE_PERIOD_MS) / (float) CYCLE_PERIOD_MS;
		int tail_idx = (int) (phase_ratio * (float) ctx->tail_atlas.total_frames);
		if (tail_idx >= ctx->tail_atlas.total_frames)
			tail_idx = ctx->tail_atlas.total_frames - 1;
		if (tail_idx < 0)
			tail_idx = 0;

		SDL_FRect base_dst = { 27.0f, 200.0f, 96.0f, 96.0f };

		if (!actual_disable_outline && tail_halo_blueprint.texture) {
			SDL_SetTextureBlendMode(tail_halo_blueprint.texture, SDL_BLENDMODE_BLEND);
			SDL_SetTextureColorMod(tail_halo_blueprint.texture, 255, 255, 255);
			SDL_SetTextureAlphaMod(tail_halo_blueprint.texture, 255);
			SDL_RenderTexture(renderer, tail_halo_blueprint.texture,
							  &tail_halo_blueprint.src_rects[tail_idx], &base_dst);
		}

		SDL_SetTextureBlendMode(ctx->tail_atlas.texture, SDL_BLENDMODE_BLEND);
		SDL_SetTextureColorMod(ctx->tail_atlas.texture, ctx->cat_color.r, ctx->cat_color.g,
							   ctx->cat_color.b);
		SDL_SetTextureAlphaMod(ctx->tail_atlas.texture, ctx->cat_color.a);
		SDL_RenderTexture(renderer, ctx->tail_atlas.texture, &ctx->tail_atlas.src_rects[tail_idx],
						  &base_dst);
	}

	CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catback", ctx->cat_color, 0.0f, 0.0f);
	CatClock_RenderXbmLayer(ctx->xbm_lib, renderer, "cattie", ctx->tie_color);

	// ========================================================================
	// START ISOLATED TRACKING OF THE EYES PROCESSING BLOCK (COMBINED SINGLE-PASS)
	// ========================================================================
#ifdef CATCLOCK_DEBUG
	uint64_t eye_start_ticks = SDL_GetPerformanceCounter();
#endif

	if (ctx->eyes_atlas.texture) {
		int total_fps_frames = target_fps_limit <= 0 ? 30 : target_fps_limit;
		int total_cycle_frames = total_fps_frames * 2;

		int frame_idx = (int) (ctx->current_frame_step % total_cycle_frames);
		if (frame_idx < 0)
			frame_idx = 0;

		SDL_FRect eyes_src_rect = ctx->eyes_atlas.src_rects[frame_idx];

		/* --- THE STRUCTURAL PRESENTATION FIX ---
		   We force the static float coordinates to snap to strict integer pixels
		   at runtime, which completely removes fractional rendering gaps and white
		   edge line leaks across all zoom levels. */
		SDL_FRect eyes_dst_rect = { 44.0f, 26.0f, 64.0f, 32.0f };

		SDL_SetTextureBlendMode(ctx->eyes_atlas.texture, SDL_BLENDMODE_BLEND);
		SDL_SetTextureColorMod(ctx->eyes_atlas.texture, 255, 255, 255);
		SDL_SetTextureAlphaMod(ctx->eyes_atlas.texture, 255);

		/* Fast, single-pass runtime blit call execution */
		SDL_RenderTexture(renderer, ctx->eyes_atlas.texture, &eyes_src_rect, &eyes_dst_rect);

		/* Fast, single-pass VRAM draw call execution tracking */
		SDL_RenderTexture(renderer, ctx->eyes_atlas.texture, &eyes_src_rect, &eyes_dst_rect);
	}

#ifdef CATCLOCK_DEBUG
	/* --- SILENCE CHRONO FLOOD IN PRODUCTION ---
	   Enclose the log string print passes inside macro layouts to save console I/O */
	uint64_t eye_end_ticks = SDL_GetPerformanceCounter();
	double eye_ms = ((double) (eye_end_ticks - eye_start_ticks) * 1000.0)
		/ (double) SDL_GetPerformanceFrequency();
	double total_ms = ((double) (SDL_GetPerformanceCounter() - pipeline_start_ticks) * 1000.0)
		/ (double) SDL_GetPerformanceFrequency();
	SDL_Log("[PROFILE CHRONO] Total Pipeline: %.4f ms | Isolated Eyes: %.4f ms", total_ms, eye_ms);
#endif
	// ========================================================================
	// END ISOLATED TRACKING OF THE EYES PROCESSING BLOCK
	// ========================================================================

	/* --- PHASE 2: TIME HAND PRESENTATION CHANNELS --- */
	if (ctx->hands_atlas.texture && ctx->minutes_atlas.texture && ctx->seconds_atlas.texture) {
		int h_idx = hour_phase < 0 || hour_phase >= TOTAL_PHASES ? 0 : hour_phase;
		int m_idx = minute_phase < 0 || minute_phase >= TOTAL_PHASES ? 0 : minute_phase;
		int s_idx = second_phase < 0 || second_phase >= TOTAL_PHASES ? 0 : second_phase;

		SDL_FRect dst = { 74.0f - 32.0f, 150.0f - 48.0f, 64.0f, 96.0f };

		SDL_SetTextureColorMod(ctx->hands_atlas.texture, ctx->hour_color.r, ctx->hour_color.g,
							   ctx->hour_color.b);
		SDL_RenderTexture(renderer, ctx->hands_atlas.texture, &ctx->hands_atlas.src_rects[h_idx],
						  &dst);

		SDL_SetTextureColorMod(ctx->minutes_atlas.texture, ctx->minute_color.r, ctx->minute_color.g,
							   ctx->minute_color.b);
		SDL_RenderTexture(renderer, ctx->minutes_atlas.texture,
						  &ctx->minutes_atlas.src_rects[m_idx], &dst);

		if (!ctx->hide_seconds) {
			SDL_SetTextureColorMod(ctx->seconds_atlas.texture, ctx->second_color.r,
								   ctx->second_color.b, ctx->second_color.g);
			SDL_RenderTexture(renderer, ctx->seconds_atlas.texture,
							  &ctx->seconds_atlas.src_rects[s_idx], &dst);
		}
	}

	SDL_SetRenderTarget(renderer, old_target);

#ifdef CATCLOCK_DEBUG
	// ========================================================================
	// COMPUTE DELTAS AND PRINT UNCONDITIONED ONE-LINER REPORT
	// ========================================================================
	uint64_t pipeline_end_ticks = SDL_GetPerformanceCounter();
	uint64_t frequency = SDL_GetPerformanceFrequency();

	double total_pipeline_ms
		= ((double) (pipeline_end_ticks - pipeline_start_ticks) * 1000.0) / (double) frequency;
	double isolated_eyes_ms
		= ((double) (eye_end_ticks - eye_start_ticks) * 1000.0) / (double) frequency;

	printf("[PROFILE CHRONO] Total Pipeline: %.4f ms | Isolated Eyes: %.4f ms\n", total_pipeline_ms,
		   isolated_eyes_ms);
	fflush(stdout);
#endif
}
