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

#include "catclock_atlas.h"
#include "catclock_shared.h"
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
 * CommitBufferToSDL
 * High-Performance Direct VRAM Lock-and-Blit 8-to-32-bit Semantic Palette Expansion
 * Updated: Enforces explicit row-stride pitch constraints and standard RGBA bit packing.
 */
void CommitBufferToSDL(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas) {
	if (!atlas || !atlas->texture || !atlas->index_buffer) {
		return;
	}
	(void) renderer;

	void* pixels = NULL;
	int pitch = 0;

	// Acquire direct pointer to raw 32-bit hardware pixel memory block
	if (!SDL_LockTexture(atlas->texture, NULL, &pixels, &pitch)) {
		SDL_Log("VRAM Lock Failure: %s", SDL_GetError());
		return;
	}

	int width = atlas->atlas_w;
	int height = atlas->atlas_h;
	Uint8* src_buf = atlas->index_buffer;

	// Explicit double loop preventing vertical skewing across hardware stride boundaries
	for (int y = 0; y < height; y++) {
		// Calculate the precise destination pointer using the runtime byte pitch
		Uint32* dst_row = (Uint32*) ((Uint8*) pixels + (y * pitch));
		int row_offset = y * width;

		for (int x = 0; x < width; x++) {
			Uint8 pal_idx = src_buf[row_offset + x];
			SDL_Color color = { 0, 0, 0, 0 };

			// Ultra-fast switch palette mapping look-up block
			switch (pal_idx) {
			case PALETTE_TRANSPARENT:
				dst_row[x] = 0x00000000; // Fully Clear Space
				continue;
			case PALETTE_CAT_BODY:
				color = ctx.cat_color;
				break;
			case PALETTE_SCLERA:
				color = ctx.sclera_color;
				break;
			case PALETTE_PUPIL:
				color = ctx.pupil_color;
				break;
			case PALETTE_HAND_HOUR:
				color = ctx.hour_color;
				break;
			case PALETTE_HAND_MINUTE:
				color = ctx.minute_color;
				break;
			case PALETTE_HAND_SECOND:
				color = ctx.second_color;
				break;
			case PALETTE_NECKTIE:
				color = ctx.tie_color;
				break;
			case PALETTE_HALO:
				color = ctx.detail_color;
				break;
			default:
				dst_row[x] = 0x00000000;
				continue;
			}

			// Map runtime color channels straight to uniform unswizzled 32-bit integer layouts
			// This matches standard SG_PIXELFORMAT_RGBA8 parsing logic on modern hardware contexts
			dst_row[x] = ((Uint32) color.r) | ((Uint32) color.g << 8) | ((Uint32) color.b << 16)
				| ((Uint32) color.a << 24);
		}
	}

	SDL_UnlockTexture(atlas->texture);
}

void CatClock_RebakeComputeAtlas(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas,
								 int cell_base_w, int cell_base_h, int total_frames, int cols,
								 CatClock_ShaderCallback shader, void* userdata) {
	int win_w = 0, win_h = 0;
	SDL_GetRenderOutputSize(renderer, &win_w, &win_h);

#ifdef CATCLOCK_DIAGNOSTIC
	if (ctx.current_frame_step % 60 == 0) {
		printf("[DIAG ATLAS_REBAKE] win_w: %d, win_h: %d, cell_base_w: %d, cell_base_h: %d\n",
			   win_w, win_h, cell_base_w, cell_base_h);
	}
#endif

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

	atlas->src_rects = (SDL_FRect*) SDL_malloc(sizeof(SDL_FRect) * total_frames);
	if (!atlas->src_rects)
		return;

	int rows = (total_frames + cols - 1) / cols;
	int atlas_w = cols * atlas->cell_w;
	int atlas_h = rows * atlas->cell_h;

	atlas->index_buffer = (Uint8*) SDL_calloc(1, atlas_w * atlas_h);
	if (!atlas->index_buffer) {
		SDL_free(atlas->src_rects);
		atlas->src_rects = NULL;
		return;
	}

	atlas->atlas_w = atlas_w;
	atlas->atlas_h = atlas_h;

	// Request hardware layer texture allocated directly in standard 32-bit streaming color layout
	atlas->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
									   SDL_TEXTUREACCESS_STREAMING, atlas_w, atlas_h);
	if (!atlas->texture) {
		SDL_free(atlas->src_rects);
		atlas->src_rects = NULL;
		SDL_free(atlas->index_buffer);
		atlas->index_buffer = NULL;
		return;
	}

	SDL_SetTextureBlendMode(atlas->texture, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(atlas->texture, SDL_SCALEMODE_NEAREST);

	for (int i = 0; i < total_frames; i++) {
		int cell_x = (i % cols) * atlas->cell_w;
		int cell_y = (i / cols) * atlas->cell_h;

		atlas->src_rects[i]
			= (SDL_FRect) { (float) cell_x + 1.0f, (float) cell_y + 1.0f,
							(float) atlas->cell_w - 2.0f, (float) atlas->cell_h - 2.0f };

		for (int cy = 0; cy < atlas->cell_h; cy++) {
			for (int cx = 0; cx < atlas->cell_w; cx++) {
				int clear_idx = ((cell_y + cy) * atlas_w) + (cell_x + cx);
				atlas->index_buffer[clear_idx] = PALETTE_TRANSPARENT;
			}
		}

		shader((void*) atlas->index_buffer, cell_x, cell_y, (float) atlas_w, i, userdata);
	}

	SDL_SetRenderViewport(renderer, NULL);
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
	if (atlas->index_buffer) {
		SDL_free(atlas->index_buffer);
		atlas->index_buffer = NULL;
	}
	atlas->total_frames = 0;
	atlas->cell_w = 0;
	atlas->cell_h = 0;
	atlas->last_scale = -1.0f;
}

void CatClock_ShaderTailHaloBake(void* renderer, int cell_x, int cell_y, float atlas_w_f,
								 int frame_idx, void* userdata) {
	Uint8* buffer = (Uint8*) renderer;
	CatClock_TailShaderArgs* blueprint_args = (CatClock_TailShaderArgs*) userdata;

	float offsets_x[] = { -1.0f, 1.0f, 0.0f, 0.0f, -1.0f, -1.0f, 1.0f, 1.0f };
	float offsets_y[] = { 0.0f, 0.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f };

	float real_scale = ctx.current_scale;

	for (int i = 0; i < 8; i++) {
		CatClock_TailShaderArgs pass_args;
		pass_args.app_ctx = blueprint_args ? blueprint_args->app_ctx : NULL;
		pass_args.offset_x = offsets_x[i] * real_scale;
		pass_args.offset_y = offsets_y[i] * real_scale;
		pass_args.force_halo_color = true;

		CatClock_ShaderTail(buffer, cell_x, cell_y, atlas_w_f, frame_idx, &pass_args);
	}
}

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

	runtime_scale = roundf(runtime_scale * 2.0f) / 2.0f;

	int calculated_cell_w = (int) ceilf(64.0f * runtime_scale);
	int calculated_cell_h = (int) ceilf(32.0f * runtime_scale);

	if (calculated_cell_w < 64)
		calculated_cell_w = 64;
	if (calculated_cell_h < 32)
		calculated_cell_h = 32;

#ifdef CATCLOCK_DIAGNOSTIC
	static float last_logged_scale = -1.0f;
	if (runtime_scale != last_logged_scale) {
		SDL_Log("[CATCLOCK LOG] Scale Multiplier Calibrated To: %.4f\n", runtime_scale);
		last_logged_scale = runtime_scale;
	}
	CATCLOCK_LOG_DIAG(ctx, "calculated_cell=%dx%d, decorations=%s", calculated_cell_w,
					  calculated_cell_h, ctx->use_decorations ? "TRUE" : "FALSE");
#endif

	static CatClock_ComputeAtlas tail_halo_blueprint = { 0 };

	// Context Recycling Entrypoint
	if (!ctx->master_composite_layer || ctx->texture_cache_stale || !ctx->eye_clipping_stencil) {
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

		if (ctx->xbm_lib) {
			CatClock_DestroyXbmLibrary(ctx->xbm_lib);
			ctx->xbm_lib = NULL;
		}

		SDL_Window* parent_window = SDL_GetRenderWindow(renderer);
		if (parent_window) {
			SDL_DestroyRenderer(renderer);
			renderer = SDL_CreateRenderer(parent_window, NULL);
			*renderer_ptr = renderer;
			ctx->xbm_lib = CatClock_InitXbmLibrary(renderer);
		}

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

		// RESTORED: Hour and Minute hand atlas generation hooks
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

	if (ctx->use_decorations) {
		SDL_SetRenderDrawColor(renderer, ctx->window_bg_color.r, ctx->window_bg_color.g,
							   ctx->window_bg_color.b, ctx->window_bg_color.a);
	} else {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	}

	SDL_RenderClear(renderer);
	SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);

	float render_pad_x = ctx->use_decorations ? CHOP_OFFSET_X : 0.0f;
	float render_pad_y = ctx->use_decorations ? CHOP_OFFSET_Y : 0.0f;
	float visual_pad = ctx->use_decorations ? 0.0f : 1.0f;

	if (!actual_disable_outline) {
		CatClock_RenderHaloOutline(ctx->xbm_lib, renderer, ctx->bg_color);
	}

	// 1. Pure Headless Software Rasterizer Streaming Present Pass
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
		SDL_Log("[DIAG LAYOUT TARGETS] pad_x=%.1f, pad_y=%.1f, tail_dst.x=%.1f, tail_dst.y=%.1f",
				render_pad_x, render_pad_y, base_dst.x, base_dst.y);
#endif

		if (!actual_disable_outline && tail_halo_blueprint.texture) {
			CommitBufferToSDL(renderer, &tail_halo_blueprint);
			SDL_RenderTexture(renderer, tail_halo_blueprint.texture,
							  &tail_halo_blueprint.src_rects[tail_idx], &base_dst);
		}
		CommitBufferToSDL(renderer, &ctx->tail_atlas);
		SDL_RenderTexture(renderer, ctx->tail_atlas.texture, &ctx->tail_atlas.src_rects[tail_idx],
						  &base_dst);
	}

	CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catback", ctx->cat_color,
								  render_pad_x + visual_pad, render_pad_y + visual_pad);
	CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "catwhite", ctx->detail_color,
								  render_pad_x + visual_pad, render_pad_y + visual_pad);
	CatClock_RenderXbmLayerOffset(ctx->xbm_lib, renderer, "cattie", ctx->tie_color,
								  render_pad_x + visual_pad, render_pad_y + visual_pad);

	// 2. Moving Eyes Streaming Present Pass
	if (ctx->eyes_atlas.texture) {
		int total_eye_frames = ctx->eyes_atlas.total_frames;
		int frame_idx = (int) (ctx->current_frame_step % total_eye_frames);
		if (frame_idx < 0)
			frame_idx = 0;

		SDL_FRect eyes_src_rect = ctx->eyes_atlas.src_rects[frame_idx];
		SDL_FRect eyes_dst_rect
			= { floorf((20.0f + render_pad_x + visual_pad) * runtime_scale),
				floorf((16.0f + render_pad_y + visual_pad) * runtime_scale),
				(float) ctx->eyes_atlas.cell_w, (float) ctx->eyes_atlas.cell_h };

		CommitBufferToSDL(renderer, &ctx->eyes_atlas);
		SDL_RenderTexture(renderer, ctx->eyes_atlas.texture, &eyes_src_rect, &eyes_dst_rect);
	}

	// 3. Baked Software Triangle Geometry Clock Hands Present Pass
	if (ctx->hands_atlas.texture && ctx->minutes_atlas.texture && ctx->seconds_atlas.texture) {
		int h_idx = hour_phase < 0 || hour_phase >= TOTAL_PHASES ? 0 : hour_phase;
		int m_idx = minute_phase < 0 || minute_phase >= TOTAL_PHASES ? 0 : minute_phase;
		int s_idx = second_phase < 0 || second_phase >= TOTAL_PHASES ? 0 : second_phase;

		SDL_FRect hands_dst_rect
			= { floorf((18.0f + render_pad_x + visual_pad) * runtime_scale),
				floorf((92.0f + render_pad_y + visual_pad) * runtime_scale),
				(float) ctx->hands_atlas.cell_w, (float) ctx->hands_atlas.cell_h };

		CommitBufferToSDL(renderer, &ctx->hands_atlas);
		SDL_RenderTexture(renderer, ctx->hands_atlas.texture, &ctx->hands_atlas.src_rects[h_idx],
						  &hands_dst_rect);

		CommitBufferToSDL(renderer, &ctx->minutes_atlas);
		SDL_RenderTexture(renderer, ctx->minutes_atlas.texture,
						  &ctx->minutes_atlas.src_rects[m_idx], &hands_dst_rect);

		if (!ctx->hide_seconds) {
			CommitBufferToSDL(renderer, &ctx->seconds_atlas);
			SDL_RenderTexture(renderer, ctx->seconds_atlas.texture,
							  &ctx->seconds_atlas.src_rects[s_idx], &hands_dst_rect);
		}
	}

	SDL_SetRenderTarget(renderer, old_target);
	*renderer_ptr = renderer;
}
