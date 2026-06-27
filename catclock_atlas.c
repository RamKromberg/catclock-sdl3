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

int CompareFloats(const void* a, const void* b) {
	float fa = *(const float*) a;
	float fb = *(const float*) b;
	return (fa > fb) - (fa < fb);
}

void CatClock_OnWindowResize(SDL_WindowEvent* resize_event, CatClock_AppContext* ctx,
							 SDL_Renderer* renderer) {
	(void) resize_event;
	(void) renderer;
	if (ctx) {
		ctx->texture_cache_stale = true;
	}
}

void CommitBufferToSDL(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas) {
	(void) renderer;
	(void) atlas;
}

void CatClock_RebakeComputeAtlas(SDL_Renderer* renderer, CatClock_ComputeAtlas* atlas,
								 int cell_base_w, int cell_base_h, int total_frames, int cols,
								 CatClock_ShaderCallback shader, void* userdata) {
	(void) renderer;
	(void) cols; /* Override columns parameter to enforce universal 10-column standard */
	if (!atlas)
		return;

	float scale = ctx.current_scale;
	if (scale <= 0.01f)
		scale = 1.0f;
	scale = roundf(scale * 2.0f) / 2.0f;

	/* Force universal 10-column stride across every multi-frame layer */
	int forced_cols = 10;

	if (!atlas->index_buffer || scale != atlas->last_scale || total_frames != atlas->total_frames) {
		CatClock_DestroyComputeAtlas(atlas);

		atlas->total_frames = total_frames;
		atlas->last_scale = scale;

		atlas->cell_w = (int) ceilf((float) cell_base_w * scale);
		atlas->cell_h = (int) ceilf((float) cell_base_h * scale);

		atlas->src_rects = (SDL_FRect*) SDL_malloc(sizeof(SDL_FRect) * total_frames);
		if (!atlas->src_rects)
			return;

		int rows = (total_frames + forced_cols - 1) / forced_cols;
		int atlas_w = forced_cols * atlas->cell_w;
		int atlas_h = rows * atlas->cell_h;

		atlas->atlas_w = atlas_w;
		atlas->atlas_h = atlas_h;
		atlas->texture = NULL;

		atlas->index_buffer = (Uint8*) SDL_calloc(1, atlas_w * atlas_h);
		if (!atlas->index_buffer) {
			SDL_free(atlas->src_rects);
			atlas->src_rects = NULL;
			return;
		}

		for (int i = 0; i < total_frames; i++) {
			int cell_x = (i % forced_cols) * atlas->cell_w;
			int cell_y = (i / forced_cols) * atlas->cell_h;
			atlas->src_rects[i] = (SDL_FRect) { (float) cell_x, (float) cell_y,
												(float) atlas->cell_w, (float) atlas->cell_h };
		}
	}

	int atlas_w = atlas->atlas_w;
	int atlas_h = atlas->atlas_h;

	for (int i = 0; i < total_frames; i++) {
		int cell_x = (i % forced_cols) * atlas->cell_w;
		int cell_y = (i / forced_cols) * atlas->cell_h;

		for (int cy = 0; cy < atlas->cell_h; cy++) {
			for (int cx = 0; cx < atlas->cell_w; cx++) {
				int clear_idx = ((cell_y + cy) * atlas_w) + (cell_x + cx);
				atlas->index_buffer[clear_idx] = PALETTE_TRANSPARENT;
			}
		}
		shader((void*) atlas->index_buffer, cell_x, cell_y, atlas_w, atlas_h, i, userdata);
	}
	// AUTOMATED UNIVERSAL VALIDATION BLUEPRINT: Dump every layer to disk for analysis
	static bool diagnostic_atlases_dumped = false;
	if (!diagnostic_atlases_dumped) {
		if (cell_base_w == 64 && cell_base_h == 96 && total_frames == TOTAL_PHASES) {
			// Unpack userdata parameters inline to isolate the specific clock hand sheets
			struct {
				int type;
			}* hand_ptr = userdata;
			if (hand_ptr && hand_ptr->type == HAND_TYPE_HOUR) {
				CatClock_DebugDumpGenericAtlasToPam("dump_hours_atlas.pam", atlas->index_buffer,
													atlas_w, atlas_h);
			} else if (hand_ptr && hand_ptr->type == HAND_TYPE_MINUTE) {
				CatClock_DebugDumpGenericAtlasToPam("dump_minutes_atlas.pam", atlas->index_buffer,
													atlas_w, atlas_h);
			} else if (hand_ptr && hand_ptr->type == HAND_TYPE_SECOND) {
				CatClock_DebugDumpGenericAtlasToPam("dump_seconds_atlas.pam", atlas->index_buffer,
													atlas_w, atlas_h);
			}
		} else if (cell_base_w == 64 && cell_base_h == 32) {
			CatClock_DebugDumpGenericAtlasToPam("dump_eyes_atlas.pam", atlas->index_buffer, atlas_w,
												atlas_h);
		} else if (cell_base_w == 96 && cell_base_h == 96) {
			struct {
				void* app;
				float ox;
				float oy;
				bool is_halo;
			}* tail_ptr = userdata;
			if (tail_ptr && tail_ptr->is_halo) {
				CatClock_DebugDumpGenericAtlasToPam("dump_tail_halo_atlas.pam", atlas->index_buffer,
													atlas_w, atlas_h);
			} else {
				CatClock_DebugDumpGenericAtlasToPam("dump_tail_body_atlas.pam", atlas->index_buffer,
													atlas_w, atlas_h);
				diagnostic_atlases_dumped = true; // Final checkpoint complete
			}
		}
	}
}

void CatClock_DestroyComputeAtlas(CatClock_ComputeAtlas* atlas) {
	if (!atlas)
		return;
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
	atlas->atlas_w = 0;
	atlas->atlas_h = 0;
	atlas->last_scale = -1.0f;
}

void CatClock_ShaderTailHaloBake(void* render_dest, int cell_x, int cell_y, int sheet_w,
								 int sheet_h, int frame_idx, void* userdata) {
	Uint8* buffer = (Uint8*) render_dest;
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

		/* Forward parameters down to our safe bounded compositor pass */
		CatClock_ShaderTail(buffer, cell_x, cell_y, sheet_w, sheet_h, frame_idx, &pass_args);
	}
}

void CatClock_SynchronizePipelineAtlases(SDL_Renderer** renderer_ptr,
										 CatClock_AppContext* app_context, float sway_deg,
										 int hour_phase, int minute_phase, int second_phase) {
	(void) renderer_ptr;
	(void) sway_deg;
	(void) hour_phase;
	(void) minute_phase;
	(void) second_phase;
	if (!app_context)
		return;

	bool actual_disable_outline = app_context->disable_outline;

	int win_w = app_context->current_win_w;
	float baseline_w = app_context->use_decorations ? DECORATED_CANVAS_W : 103.0f;
	float runtime_scale = (float) win_w / baseline_w;
	if (runtime_scale <= 0.01f)
		runtime_scale = 1.0f;
	runtime_scale = roundf(runtime_scale * 2.0f) / 2.0f;

	int calculated_fps_limit = target_fps_limit <= 0 ? 30 : target_fps_limit;
	int dynamic_cycle_frames = calculated_fps_limit * 2;

	static CatClock_ComputeAtlas tail_halo_blueprint = { 0 };

	if (app_context->texture_cache_stale || !app_context->hours_atlas.index_buffer) {
		CatClock_DestroyComputeAtlas(&app_context->hours_atlas);
		CatClock_DestroyComputeAtlas(&app_context->minutes_atlas);
		CatClock_DestroyComputeAtlas(&app_context->seconds_atlas);
		CatClock_DestroyComputeAtlas(&app_context->eyes_atlas);
		CatClock_DestroyComputeAtlas(&app_context->tail_atlas);
		CatClock_DestroyComputeAtlas(&tail_halo_blueprint);

		/* Cast configuration objects inline to avoid configuration layout mismatch errors */
		struct {
			int type;
			SDL_Color color;
		} hour_cfg = { HAND_TYPE_HOUR, app_context->hour_color };
		struct {
			int type;
			SDL_Color color;
		} minute_cfg = { HAND_TYPE_MINUTE, app_context->minute_color };
		struct {
			int type;
			SDL_Color color;
		} second_cfg = { HAND_TYPE_SECOND, app_context->second_color };

		CatClock_RebakeComputeAtlas(NULL, &app_context->hours_atlas, 64, 96, TOTAL_PHASES, 6,
									CatClock_ShaderHands, &hour_cfg);
		CatClock_RebakeComputeAtlas(NULL, &app_context->minutes_atlas, 64, 96, TOTAL_PHASES, 6,
									CatClock_ShaderHands, &minute_cfg);
		CatClock_RebakeComputeAtlas(NULL, &app_context->seconds_atlas, 64, 96, TOTAL_PHASES, 6,
									CatClock_ShaderHands, &second_cfg);
		CatClock_RebakeComputeAtlas(NULL, &app_context->eyes_atlas, 64, 32, dynamic_cycle_frames,
									10, CatClock_ShaderEyes, NULL);

		CatClock_TailShaderArgs main_tail_args = { app_context, 0.0f, 0.0f, false };
		main_tail_args.app_ctx->cat_color = app_context->cat_color;
		CatClock_RebakeComputeAtlas(NULL, &app_context->tail_atlas, 96, 96, dynamic_cycle_frames, 8,
									CatClock_ShaderTail, &main_tail_args);

		if (!actual_disable_outline) {
			CatClock_TailShaderArgs halo_bake_args = { app_context, 0.0f, 0.0f, true };
			CatClock_RebakeComputeAtlas(NULL, &tail_halo_blueprint, 96, 96, dynamic_cycle_frames, 8,
										CatClock_ShaderTailHaloBake, &halo_bake_args);
		}

		app_context->texture_cache_stale = false;
	}
}

void CatClock_DebugDumpGenericAtlasToPam(const char* filepath, const uint8_t* raw_buffer, int w,
										 int h) {
	FILE* f = fopen(filepath, "wb");
	if (!f)
		return;

	fprintf(f, "P7\n");
	fprintf(f, "WIDTH %d\n", w);
	fprintf(f, "HEIGHT %d\n", h);
	fprintf(f, "DEPTH 4\n");
	fprintf(f, "MAXVAL 255\n");
	fprintf(f, "TUPLTYPE RGB_ALPHA\n");
	fprintf(f, "ENDHDR\n");

	for (int i = 0; i < w * h; i++) {
		uint8_t token = raw_buffer[i];
		uint8_t r = 0, g = 0, b = 0, a = 255;

		/* Unified Token Evaluation Rules matching all composite atlas structures */
		if (token == 0x33 || token == PALETTE_HALO) {
			/* Solid White */
			r = 255;
			g = 255;
			b = 255;
		} else if (token == 0x66 || token == PALETTE_HAND_SECOND) {
			/* Solid Red */
			r = 255;
			g = 0;
			b = 0;
		} else if (token == 0x99 || token == PALETTE_CAT_BODY || token == PALETTE_HAND_HOUR
				   || token == PALETTE_HAND_MINUTE) {
			/* Solid Black */
			r = 0;
			g = 0;
			b = 0;
		} else if (token == 0xCC) {
			/* Soild Green  Pupil*/
			r = 0;
			g = 255;
			b = 0;
		} else {
			/* Clear Void Alpha Spaces */
			a = 0;
		}

		fputc(r, f);
		fputc(g, f);
		fputc(b, f);
		fputc(a, f);
	}

	fclose(f);
}
