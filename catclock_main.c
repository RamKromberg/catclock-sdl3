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

#define SOKOL_IMPL
#include "sokol_gfx.h"
#include "sokol_log.h"

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

const char* kitcat_fs_src;
const char* kitcat_vs_src;

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

void CatClock_DebugDumpPaletteSheetToDisk(const char* filepath, const Uint8* index_buffer, int w,
										  int h) {
	if (!index_buffer || w <= 0 || h <= 0)
		return;

	FILE* f = fopen(filepath, "wb");
	if (!f)
		return;

	// Write a standard PGM (Portable GrayMap) P5 binary header
	fprintf(f, "P5\n%d %d\n255\n", w, h);

	for (int i = 0; i < (w * h); i++) {
		Uint8 val = index_buffer[i];

		// Map the palette IDs (0-8) out to high-contrast grayscale spans (0-255)
		// This makes empty space pitch black, and masks brightly visible shapes
		Uint8 pixel_intensity = (val == 0) ? 0 : (val * 28);

		fputc(pixel_intensity, f);
	}

	fclose(f);
}

static sg_buffer quad_vbuf;
static sg_pipeline quad_pipeline;
static sg_shader quad_shader;

static sg_image
	cat_body_mask_img; /* Formerly body_array_img / halo - solid background silhouette mask */
static sg_image cat_body_img; /* Raw structural lines from catback.xbm */
static sg_image necktie_img; /* Raw processed lines & fills from cattie.xbm */

static sg_image tail_img;
static sg_image eyes_img;
static sg_image hours_img;
static sg_image mins_img;
static sg_image secs_img;

static sg_sampler nearest_sampler;

void InitSokolRenderPipeline(void) {
	/* 1. Cleanly set up your shader descriptions */
	sg_shader_desc shader_desc;
	memset(&shader_desc, 0, sizeof(shader_desc));
	shader_desc.vs.source = kitcat_vs_src;
	shader_desc.fs.source = kitcat_fs_src;
	shader_desc.label = "kitcat-compositor-inline-shader";

	shader_desc.attrs[0].name = "position";
	shader_desc.attrs[1].name = "texcoord";

	shader_desc.fs.uniform_blocks[0].size = sizeof(CatClock_ShaderUniforms);
	shader_desc.fs.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_STD140;
	shader_desc.fs.uniform_blocks[0].uniforms[0]
		= (sg_shader_uniform_desc) { .name = "tail_uv", .type = SG_UNIFORMTYPE_FLOAT4 };
	shader_desc.fs.uniform_blocks[0].uniforms[1]
		= (sg_shader_uniform_desc) { .name = "eyes_uv", .type = SG_UNIFORMTYPE_FLOAT4 };
	shader_desc.fs.uniform_blocks[0].uniforms[2]
		= (sg_shader_uniform_desc) { .name = "hours_uv", .type = SG_UNIFORMTYPE_FLOAT4 };
	shader_desc.fs.uniform_blocks[0].uniforms[3]
		= (sg_shader_uniform_desc) { .name = "mins_uv", .type = SG_UNIFORMTYPE_FLOAT4 };
	shader_desc.fs.uniform_blocks[0].uniforms[4]
		= (sg_shader_uniform_desc) { .name = "secs_uv", .type = SG_UNIFORMTYPE_FLOAT4 };
	shader_desc.fs.uniform_blocks[0].uniforms[5]
		= (sg_shader_uniform_desc) { .name = "cat_color", .type = SG_UNIFORMTYPE_FLOAT4 };
	shader_desc.fs.uniform_blocks[0].uniforms[6]
		= (sg_shader_uniform_desc) { .name = "tie_color", .type = SG_UNIFORMTYPE_FLOAT4 };
	shader_desc.fs.uniform_blocks[0].uniforms[7]
		= (sg_shader_uniform_desc) { .name = "pupil_color", .type = SG_UNIFORMTYPE_FLOAT4 };
	shader_desc.fs.uniform_blocks[0].uniforms[8]
		= (sg_shader_uniform_desc) { .name = "sclera_color", .type = SG_UNIFORMTYPE_FLOAT4 };
	shader_desc.fs.uniform_blocks[0].uniforms[9]
		= (sg_shader_uniform_desc) { .name = "detail_color", .type = SG_UNIFORMTYPE_FLOAT4 };

	/* Declare all 8 images explicitly as independent standard 2D texture samplers */
	for (int i = 0; i < 8; i++) {
		shader_desc.fs.images[i].used = true;
		shader_desc.fs.images[i].image_type = SG_IMAGETYPE_2D;
	}

	shader_desc.fs.samplers[0].used = true;
	shader_desc.fs.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;

	/* Explicitly align every texture uniform target name to its active binding matrix slot */
	shader_desc.fs.image_sampler_pairs[0] = (sg_shader_image_sampler_pair_desc) {
		.used = true, .glsl_name = "tex_body_mask", .image_slot = 0, .sampler_slot = 0
	};
	shader_desc.fs.image_sampler_pairs[1] = (sg_shader_image_sampler_pair_desc) {
		.used = true, .glsl_name = "tex_body", .image_slot = 1, .sampler_slot = 0
	};
	shader_desc.fs.image_sampler_pairs[2] = (sg_shader_image_sampler_pair_desc) {
		.used = true, .glsl_name = "tex_tie", .image_slot = 2, .sampler_slot = 0
	};
	shader_desc.fs.image_sampler_pairs[3] = (sg_shader_image_sampler_pair_desc) {
		.used = true, .glsl_name = "tex_tail", .image_slot = 3, .sampler_slot = 0
	};
	shader_desc.fs.image_sampler_pairs[4] = (sg_shader_image_sampler_pair_desc) {
		.used = true, .glsl_name = "tex_eyes", .image_slot = 4, .sampler_slot = 0
	};
	shader_desc.fs.image_sampler_pairs[5] = (sg_shader_image_sampler_pair_desc) {
		.used = true, .glsl_name = "tex_hours", .image_slot = 5, .sampler_slot = 0
	};
	shader_desc.fs.image_sampler_pairs[6] = (sg_shader_image_sampler_pair_desc) {
		.used = true, .glsl_name = "tex_minutes", .image_slot = 6, .sampler_slot = 0
	};
	shader_desc.fs.image_sampler_pairs[7] = (sg_shader_image_sampler_pair_desc) {
		.used = true, .glsl_name = "tex_seconds", .image_slot = 7, .sampler_slot = 0
	};

	quad_shader = sg_make_shader(&shader_desc);

	/* 2. Fallback Geometry Generator: Ensure the vertex buffer is explicitly baked in VRAM */
	float quad_vertices[] = {
		-1.0f, 1.0f,  0.0f, 0.0f, // Top-Left
		1.0f,  1.0f,  1.0f, 0.0f, // Top-Right
		-1.0f, -1.0f, 0.0f, 1.0f, // Bottom-Left
		1.0f,  -1.0f, 1.0f, 1.0f // Bottom-Right
	};

	extern sg_buffer quad_vbuf;
	if (quad_vbuf.id == 0) {
		printf("[Bake] quad_vbuf was uninitialized! Generating static quad vertex stream "
			   "metrics...\n");
		quad_vbuf = sg_make_buffer(&(sg_buffer_desc) { .size = sizeof(quad_vertices),
													   .type = SG_BUFFERTYPE_VERTEXBUFFER,
													   .usage = SG_USAGE_IMMUTABLE,
													   .data = SG_RANGE(quad_vertices),
													   .label = "kitcat-static-quad-vbuf" });
		printf("[Bake] Successfully baked quad_vbuf with assigned handle id = %u\n", quad_vbuf.id);
	}

	/* 3. Assemble Pipeline Profiles with explicit subscript array mapping slots */
	sg_pipeline_desc pipeline_desc;
	memset(&pipeline_desc, 0, sizeof(pipeline_desc));
	pipeline_desc.shader = quad_shader;
	pipeline_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
	pipeline_desc.label = "headless-quad-pipeline";

	pipeline_desc.layout.buffers[0].stride = 4 * sizeof(float);
	pipeline_desc.layout.buffers[0].step_func = SG_VERTEXSTEP_PER_VERTEX;

	pipeline_desc.layout.attrs[0].buffer_index = 0;
	pipeline_desc.layout.attrs[0].offset = 0;
	pipeline_desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;

	pipeline_desc.layout.attrs[1].buffer_index = 0;
	pipeline_desc.layout.attrs[1].offset = 2 * sizeof(float);
	pipeline_desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;

	pipeline_desc.colors[0].blend.enabled = true;
	pipeline_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
	pipeline_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

	quad_pipeline = sg_make_pipeline(&pipeline_desc);
	printf("[Bake] Active quad_pipeline created with handle id = %u\n", quad_pipeline.id);
}

void AllocateStreamTextures(void) {
	nearest_sampler = sg_make_sampler(&(sg_sampler_desc) { .min_filter = SG_FILTER_NEAREST,
														   .mag_filter = SG_FILTER_NEAREST,
														   .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
														   .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
														   .label = "nearest-pixel-art-sampler" });

	/* Explicitly assigns into the global state context handle */
	cat_body_mask_img = sg_make_image(&(sg_image_desc) { .width = 101,
														 .height = 201,
														 .usage = SG_USAGE_DYNAMIC,
														 .pixel_format = SG_PIXELFORMAT_R8 });

	cat_body_img = sg_make_image(&(sg_image_desc) { .width = 101,
													.height = 201,
													.usage = SG_USAGE_DYNAMIC,
													.pixel_format = SG_PIXELFORMAT_R8 });
	ctx.cat_halo_img = sg_make_image(&(sg_image_desc) { .width = 101,
														.height = 201,
														.usage = SG_USAGE_DYNAMIC,
														.pixel_format = SG_PIXELFORMAT_R8,
														.label = "cat-edge-halo-texture" });

	Uint8* dynamic_tie_src = NULL;
	int dynamic_tie_w = 0, dynamic_tie_h = 0;

	if (ctx.xbm_lib) {
		extern void CatClock_GetCattieBodyData(CatClock_XbmLibrary * lib, Uint8 * *bits, int* w,
											   int* h);
		CatClock_GetCattieBodyData(ctx.xbm_lib, &dynamic_tie_src, &dynamic_tie_w, &dynamic_tie_h);
	}

	necktie_img = sg_make_image(&(sg_image_desc) {
		.type = SG_IMAGETYPE_2D,
		.width = 87, // Corrected trimmed bounding box metric
		.height = 20, // Corrected trimmed bounding box metric
		.usage = SG_USAGE_IMMUTABLE, // High performance static VRAM footprint
		.pixel_format = SG_PIXELFORMAT_R8, // Safe 1 byte per pixel foot map alignment
		.label = "cattie_alpha_tint_mask",
		.data.subimage[0][0] = { // Fully explicit Sokol layout matrix definition
								 .ptr = dynamic_tie_src,
								 .size = 87 * 20 } });
	// =========================================================================

	/* Force explicit dynamic streaming allocations over standard R8 targets */
	tail_img = sg_make_image(&(sg_image_desc) { .width = ctx.tail_atlas.atlas_w,
												.height = ctx.tail_atlas.atlas_h,
												.pixel_format = SG_PIXELFORMAT_R8,
												.usage = SG_USAGE_STREAM,
												.label = "dynamic-tail-stream" });

	eyes_img = sg_make_image(&(sg_image_desc) { .width = ctx.eyes_atlas.atlas_w,
												.height = ctx.eyes_atlas.atlas_h,
												.pixel_format = SG_PIXELFORMAT_R8,
												.usage = SG_USAGE_STREAM,
												.label = "dynamic-eyes-stream" });

	hours_img = sg_make_image(&(sg_image_desc) { .width = ctx.hours_atlas.atlas_w,
												 .height = ctx.hours_atlas.atlas_h,
												 .pixel_format = SG_PIXELFORMAT_R8,
												 .usage = SG_USAGE_STREAM,
												 .label = "dynamic-hours-stream" });

	mins_img = sg_make_image(&(sg_image_desc) { .width = ctx.minutes_atlas.atlas_w,
												.height = ctx.minutes_atlas.atlas_h,
												.pixel_format = SG_PIXELFORMAT_R8,
												.usage = SG_USAGE_STREAM,
												.label = "dynamic-minutes-stream" });

	secs_img = sg_make_image(&(sg_image_desc) { .width = ctx.seconds_atlas.atlas_w,
												.height = ctx.seconds_atlas.atlas_h,
												.pixel_format = SG_PIXELFORMAT_R8,
												.usage = SG_USAGE_STREAM,
												.label = "dynamic-seconds-stream" });
}

void PushShaderUniforms(void) {
	time_t raw_time = time(NULL);
	struct tm* time_info = localtime(&raw_time);

	int hour_phase = 0, minute_phase = 0, second_phase = 0;
	if (time_info) {
		second_phase = time_info->tm_sec % 60;
		minute_phase = time_info->tm_min % 60;
		hour_phase = ((time_info->tm_hour % 12) * 5) + (time_info->tm_min / 12);
	}

	int total_tail_frames = ctx.tail_atlas.total_frames > 0 ? ctx.tail_atlas.total_frames : 1;
	int total_eye_frames = ctx.eyes_atlas.total_frames > 0 ? ctx.eyes_atlas.total_frames : 1;

	int hour_idx = hour_phase % TOTAL_PHASES;
	int mins_idx = minute_phase % TOTAL_PHASES;
	int secs_idx = second_phase % TOTAL_PHASES;
	int eyes_idx = (int) (ctx.current_frame_step % total_eye_frames);
	int tail_idx = (int) (ctx.current_frame_step % total_tail_frames);

	CatClock_ShaderUniforms block;
	memset(&block, 0, sizeof(block));

	// Assign and normalize directly to the GPU structure parameters
	if (ctx.tail_atlas.atlas_w > 0 && ctx.tail_atlas.src_rects) {
		SDL_FRect r = ctx.tail_atlas.src_rects[tail_idx];
		block.tail_uv[0] = r.x / (float) ctx.tail_atlas.atlas_w;
		block.tail_uv[1] = r.y / (float) ctx.tail_atlas.atlas_h;
		block.tail_uv[2] = r.w / (float) ctx.tail_atlas.atlas_w;
		block.tail_uv[3] = r.h / (float) ctx.tail_atlas.atlas_h;
	}

	if (ctx.eyes_atlas.atlas_w > 0 && ctx.eyes_atlas.src_rects) {
		SDL_FRect r = ctx.eyes_atlas.src_rects[eyes_idx];
		block.eyes_uv[0] = r.x / (float) ctx.eyes_atlas.atlas_w;
		block.eyes_uv[1] = r.y / (float) ctx.eyes_atlas.atlas_h;
		block.eyes_uv[2] = r.w / (float) ctx.eyes_atlas.atlas_w;
		block.eyes_uv[3] = r.h / (float) ctx.eyes_atlas.atlas_h;
	}

	if (ctx.hours_atlas.atlas_w > 0 && ctx.hours_atlas.src_rects) {
		SDL_FRect r = ctx.hours_atlas.src_rects[hour_idx];
		block.hours_uv[0] = r.x / (float) ctx.hours_atlas.atlas_w;
		block.hours_uv[1] = r.y / (float) ctx.hours_atlas.atlas_h;
		block.hours_uv[2] = r.w / (float) ctx.hours_atlas.atlas_w;
		block.hours_uv[3] = r.h / (float) ctx.hours_atlas.atlas_h;
	}

	if (ctx.minutes_atlas.atlas_w > 0 && ctx.minutes_atlas.src_rects) {
		SDL_FRect r = ctx.minutes_atlas.src_rects[mins_idx];
		block.mins_uv[0] = r.x / (float) ctx.minutes_atlas.atlas_w;
		block.mins_uv[1] = r.y / (float) ctx.minutes_atlas.atlas_h;
		block.mins_uv[2] = r.w / (float) ctx.minutes_atlas.atlas_w;
		block.mins_uv[3] = r.h / (float) ctx.minutes_atlas.atlas_h;
	}

	if (ctx.seconds_atlas.atlas_w > 0 && ctx.seconds_atlas.src_rects) {
		SDL_FRect r = ctx.seconds_atlas.src_rects[secs_idx];
		block.secs_uv[0] = r.x / (float) ctx.seconds_atlas.atlas_w;
		block.secs_uv[1] = r.y / (float) ctx.seconds_atlas.atlas_h;
		block.secs_uv[2] = r.w / (float) ctx.seconds_atlas.atlas_w;
		block.secs_uv[3] = r.h / (float) ctx.seconds_atlas.atlas_h;
	}

	// Tint parameters mapping profiles
	block.cat_color[0] = (float) ctx.cat_color.r / 255.0f;
	block.cat_color[1] = (float) ctx.cat_color.g / 255.0f;
	block.cat_color[2] = (float) ctx.cat_color.b / 255.0f;
	block.cat_color[3] = 1.0f;

	block.tie_color[0] = (float) ctx.tie_color.r / 255.0f;
	block.tie_color[1] = (float) ctx.tie_color.g / 255.0f;
	block.tie_color[2] = (float) ctx.tie_color.b / 255.0f;
	block.tie_color[3] = 1.0f;

	block.pupil_color[0] = (float) ctx.pupil_color.r / 255.0f;
	block.pupil_color[1] = (float) ctx.pupil_color.g / 255.0f;
	block.pupil_color[2] = (float) ctx.pupil_color.b / 255.0f;
	block.pupil_color[3] = 1.0f;

	block.sclera_color[0] = (float) ctx.sclera_color.r / 255.0f;
	block.sclera_color[1] = (float) ctx.sclera_color.g / 255.0f;
	block.sclera_color[2] = (float) ctx.sclera_color.b / 255.0f;
	block.sclera_color[3] = 1.0f;

	block.detail_color[0] = (float) ctx.detail_color.r / 255.0f;
	block.detail_color[1] = (float) ctx.detail_color.g / 255.0f;
	block.detail_color[2] = (float) ctx.detail_color.b / 255.0f;
	block.detail_color[3] = 1.0f;

	sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &SG_RANGE(block));
}

/* Forward declaration or direct implementation of the 1-bit static mask compiler blitter */
static void Rasterize1BitMaskToSheet(Uint8* dest_sheet, Uint8* bits, int mask_w, int mask_h,
									 int offset_x, int offset_y, int sheet_w, int sheet_h,
									 Uint8 palette_idx) {
	if (!bits || !dest_sheet || sheet_w <= 0 || sheet_h <= 0)
		return;

	int bytes_per_row = (mask_w + 7) / 8;
	for (int y = 0; y < mask_h; y++) {
		int target_y = y + offset_y;
		if (target_y < 0 || target_y >= sheet_h)
			continue;

		for (int x = 0; x < mask_w; x++) {
			int target_x = x + offset_x;
			if (target_x < 0 || target_x >= sheet_w)
				continue;

			int byte_idx = (y * bytes_per_row) + (x / 8);
			int bit_pos = x % 8;
			if ((bits[byte_idx] & (1 << bit_pos)) != 0) {
				/* Use the real dynamic layout stride instead of hardcoded 1024 bounds */
				dest_sheet[(target_y * sheet_w) + target_x] = palette_idx;
			}
		}
	}
}

void ReleaseSokolRenderPipeline(void) {
	sg_destroy_image(cat_body_mask_img);
	sg_destroy_image(cat_body_img);
	sg_destroy_image(necktie_img);
	sg_destroy_image(tail_img);
	sg_destroy_image(eyes_img);
	sg_destroy_image(hours_img);
	sg_destroy_image(mins_img);
	sg_destroy_image(secs_img);
}

/**
 * Writes an uncompressed P5 binary PGM grayscale map to disk.
 * Signature order corrected to: (filename, buffer, width, height) to match legacy calls.
 */
void CatClock_DebugDumpSingleLayerToDisk(const char* filename, const uint8_t* buffer, int width,
										 int height) {
	if (!filename || !buffer)
		return;
	FILE* pgm_file = fopen(filename, "wb");
	if (!pgm_file) {
		fprintf(stderr, "[ERROR] Failed to open diagnostic snapshot target path: %s\n", filename);
		return;
	}

	// P5 header layout: Magic ID, Dimensions, Max Channel Intensity
	fprintf(pgm_file, "P5\n%d %d\n255\n", width, height);

	// Localized Fix: Amplify low contrast palette indices (0-8) to high-visibility scales (0-255)
	for (int i = 0; i < (width * height); i++) {
		uint8_t val = buffer[i];
		// If it is raw uncompressed grayscale (255), leave it bright.
		// If it is a tiny index channel value (1-8), scale it up by 30 so it pops visually
		uint8_t enhanced_pixel = (val == 0) ? 0 : ((val == 255) ? 255 : (val * 30));
		fputc(enhanced_pixel, pgm_file);
	}

	fclose(pgm_file);
}

void PushActiveIndexBuffersToVRAM(void) {
	if (!ctx.tail_atlas.index_buffer || ctx.tail_atlas.atlas_w <= 0)
		return;

	float runtime_scale = ctx.current_scale;
	if (runtime_scale <= 0.01f)
		runtime_scale = 1.0f;
	runtime_scale = roundf(runtime_scale * 2.0f) / 2.0f;

	float pad_x = ctx.use_decorations ? CHOP_OFFSET_X : 0.0f;
	float pad_y = ctx.use_decorations ? CHOP_OFFSET_Y : 0.0f;
	float visual_pad = ctx.use_decorations ? 0.0f : 1.0f;

	int s_w = ctx.tail_atlas.atlas_w;
	int s_h = ctx.tail_atlas.atlas_h;
	(void) s_w;
	(void) s_h;

	/* 1. COMPOSITE MAIN BACKGROUNDS ONTO HARDWARE STAGING CHANNELS */
	if (ctx.xbm_lib) {
		Uint8* bits = NULL;
		int b_w = 0, b_h = 0;
		int plane_bytes = 101 * 201;
		int tie_bytes = 87 * 20; // Localized Fix: Isolate absolute byte size for the tight tie box

		static Uint8* mask_staging = NULL;
		static Uint8* body_staging = NULL;
		static Uint8* tie_staging = NULL;

		if (!mask_staging) {
			mask_staging = (Uint8*) SDL_calloc(1, plane_bytes);
			body_staging = (Uint8*) SDL_calloc(1, plane_bytes);
			tie_staging
				= (Uint8*) SDL_calloc(1, tie_bytes); // Allocate a clean, tight 1740-byte block
		}

		SDL_memset(mask_staging, PALETTE_TRANSPARENT, plane_bytes);
		SDL_memset(body_staging, PALETTE_TRANSPARENT, plane_bytes);
		SDL_memset(tie_staging, PALETTE_TRANSPARENT, tie_bytes);

		/* A. Parse & Update Cat Body Channel */
		CatClock_GetCatbackData(ctx.xbm_lib, &bits, &b_w, &b_h);
		Rasterize1BitMaskToSheet(
			body_staging, bits, b_w, b_h, (int) ((pad_x + visual_pad) * runtime_scale),
			(int) ((pad_y + visual_pad) * runtime_scale), 101, 201, PALETTE_CAT_BODY);

		/* B. Parse & Update Necktie Channel */
		CatClock_GetCattieBodyData(ctx.xbm_lib, &bits, &b_w, &b_h);
		if (bits) {
			// Localized Fix: Map raw 255 alpha values to the true engine palette ID
			// BEFORE the file writing process occurs.
			for (int i = 0; i < tie_bytes; i++) {
				if (bits[i] == 255) {
					tie_staging[i] = PALETTE_NECKTIE; // Set cleanly to index 7
				} else {
					tie_staging[i] = PALETTE_TRANSPARENT; // Set cleanly to index 0
				}
			}
		}

		/* C. Parse & Build Silhouette Channel */
		CatClock_GetCatwhiteData(ctx.xbm_lib, &bits, &b_w, &b_h);
		Rasterize1BitMaskToSheet(
			mask_staging, bits, b_w, b_h, (int) ((pad_x + visual_pad) * runtime_scale),
			(int) ((pad_y + visual_pad) * runtime_scale), 101, 201, PALETTE_HALO);

		for (int i = 0; i < plane_bytes; i++) {
			if (body_staging[i] == PALETTE_CAT_BODY) {
				mask_staging[i] = PALETTE_HALO;
			}
		}

		/* D. Push All 3 Independent Layers to VRAM */
		sg_image_data m_payload = { 0 };
		m_payload.subimage[0][0].ptr = mask_staging;
		m_payload.subimage[0][0].size = plane_bytes;
		sg_update_image(cat_body_mask_img, &m_payload);

		sg_image_data b_payload = { 0 };
		b_payload.subimage[0][0].ptr = body_staging;
		b_payload.subimage[0][0].size = plane_bytes;
		sg_update_image(cat_body_img, &b_payload);

		/* NOTE: necktie_img legacy dynamic blit remains deactivated since it is a static R8 texture
		 */

		/* E. Expanded Tracking Suite: Output all independent layers on the first frame pass */
		static bool multi_dump_committed = false;
		if (!multi_dump_committed) {
			printf("--- INITIATING FULL DIAGNOSTIC CPU TEXTURE SHEET MULTI-DUMP ---\n");

			CatClock_DebugDumpSingleLayerToDisk("dump_tail.pgm", ctx.tail_atlas.index_buffer,
												ctx.tail_atlas.atlas_w, ctx.tail_atlas.atlas_h);
			CatClock_DebugDumpSingleLayerToDisk("dump_eyes.pgm", ctx.eyes_atlas.index_buffer,
												ctx.eyes_atlas.atlas_w, ctx.eyes_atlas.atlas_h);
			CatClock_DebugDumpSingleLayerToDisk("dump_hours.pgm", ctx.hours_atlas.index_buffer,
												ctx.hours_atlas.atlas_w, ctx.hours_atlas.atlas_h);
			CatClock_DebugDumpSingleLayerToDisk("dump_minutes.pgm", ctx.minutes_atlas.index_buffer,
												ctx.minutes_atlas.atlas_w,
												ctx.minutes_atlas.atlas_h);
			CatClock_DebugDumpSingleLayerToDisk("dump_seconds.pgm", ctx.seconds_atlas.index_buffer,
												ctx.seconds_atlas.atlas_w,
												ctx.seconds_atlas.atlas_h);

			CatClock_DebugDumpSingleLayerToDisk("dump_static_halo_layer0.pgm", mask_staging, 101,
												201);
			CatClock_DebugDumpSingleLayerToDisk("dump_static_body_layer1.pgm", body_staging, 101,
												201);
			CatClock_DebugDumpSingleLayerToDisk("dump_static_tie_layer2.pgm", tie_staging, 101,
												201);

			// Extract and verify our clean 87x20 production necktie mask
			Uint8* live_tie_buffer = NULL;
			int live_tie_w = 0, live_tie_h = 0;
			extern void CatClock_GetCattieBodyData(CatClock_XbmLibrary * lib, Uint8 * *bits, int* w,
												   int* h);
			CatClock_GetCattieBodyData(ctx.xbm_lib, &live_tie_buffer, &live_tie_w, &live_tie_h);

			if (live_tie_buffer && live_tie_w == 87 && live_tie_h == 20) {
				CatClock_DebugDumpSingleLayerToDisk("dump_production_tie_mask.pgm", live_tie_buffer,
													87, 20);
				printf("[SUCCESS] Dumped pristine 87x20 production tie alpha mask.\n");
			}

			printf("---------------------------------------------------------------\n");
			multi_dump_committed = true;
		}
	}

	/* 2. HARDWARE STREAM COMMITMENT PASS FOR ALL FIVE SEPARATE CHANNELS */
	size_t tail_sz = (size_t) ctx.tail_atlas.atlas_w * ctx.tail_atlas.atlas_h;
	size_t eyes_sz = (size_t) ctx.eyes_atlas.atlas_w * ctx.eyes_atlas.atlas_h;
	size_t hours_sz = (size_t) ctx.hours_atlas.atlas_w * ctx.hours_atlas.atlas_h;
	size_t mins_sz = (size_t) ctx.minutes_atlas.atlas_w * ctx.minutes_atlas.atlas_h;
	size_t secs_sz = (size_t) ctx.seconds_atlas.atlas_w * ctx.seconds_atlas.atlas_h;

	sg_image_data tail_data = { 0 };
	tail_data.subimage[0][0].ptr = ctx.tail_atlas.index_buffer;
	tail_data.subimage[0][0].size = tail_sz;

	sg_image_data eyes_data = { 0 };
	eyes_data.subimage[0][0].ptr = ctx.eyes_atlas.index_buffer;
	eyes_data.subimage[0][0].size = eyes_sz;

	sg_image_data hours_data = { 0 };
	hours_data.subimage[0][0].ptr = ctx.hours_atlas.index_buffer;
	hours_data.subimage[0][0].size = hours_sz;

	sg_image_data mins_data = { 0 };
	mins_data.subimage[0][0].ptr = ctx.minutes_atlas.index_buffer;
	mins_data.subimage[0][0].size = mins_sz;

	sg_image_data secs_data = { 0 };
	secs_data.subimage[0][0].ptr = ctx.seconds_atlas.index_buffer;
	secs_data.subimage[0][0].size = secs_sz;

	sg_update_image(tail_img, &tail_data);
	sg_update_image(eyes_img, &eyes_data);
	sg_update_image(hours_img, &hours_data);
	sg_update_image(mins_img, &mins_data);
	sg_update_image(secs_img, &secs_data);
}

int main(int argc, char* argv[]) {
	printf("[Trace] Starting main()...\n");
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		printf("[Error] SDL_Init failed\n");
		return 1;
	}
	printf("[Trace] SDL_Init success\n");

	ParseCommandLineArguments(argc, argv, &ctx);
	printf("[Trace] ParseCommandLineArguments complete\n");

#ifdef CATCLOCK_TELEMETRY
	ctx.metrics.logging_frequency = (target_fps_limit <= 0 ? 30 : target_fps_limit) * 5;
#endif

	float baseline_w = ctx.use_decorations ? DECORATED_CANVAS_W : 103.0f;
	float baseline_h = ctx.use_decorations ? DECORATED_CANVAS_H : 288.0f;

	int target_w = (int) (baseline_w * ctx.current_scale);
	int target_h = (int) (baseline_h * ctx.current_scale);

	SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL;
	if (!ctx.use_decorations) {
		window_flags |= (SDL_WINDOW_BORDERLESS | SDL_WINDOW_TRANSPARENT);
	}
	if (!ctx.disable_always_on_top) {
		window_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
	}

	printf("[Trace] Creating window with dimensions: %dx%d...\n", target_w, target_h);
	SDL_Window* window
		= SDL_CreateWindow("CatClock-SDL3 Widget Core", target_w, target_h, window_flags);
	if (!window) {
		printf("[Error] SDL_CreateWindow failed\n");
		SDL_Quit();
		return 1;
	}
	printf("[Trace] Window creation successful\n");

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

	printf("[Trace] Setting up OpenGL attributes...\n");
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	printf("[Trace] Creating OpenGL context...\n");
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	if (!gl_context) {
		printf("[Error] SDL_GL_CreateContext failed\n");
		SDL_DestroyWindow(window);
		SDL_Quit();
		// === RESTARTING TRANSMISSION FROM TRUNCATION POINT IN catclock_main.c ===
		return 1;
	}
	printf("[Trace] OpenGL context created successfully\n");
	SDL_GL_MakeCurrent(window, gl_context);

	printf("[Trace] Initializing Sokol GFX...\n");
	sg_desc sokol_description = { .logger.func = slog_func };
	sg_setup(&sokol_description);

	if (!sg_isvalid()) {
		printf("[Error] Sokol GFX setup is invalid\n");
		SDL_GL_DestroyContext(gl_context);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}
	printf("[Trace] Sokol GFX successfully initialized\n");

	printf("--- CATCLOCK HARDWARE ALIGNMENT DIAGNOSTICS ---\n");
	printf("[Size] CatClock_ShaderUniforms Total Struct Size: %zu bytes\n",
		   sizeof(CatClock_ShaderUniforms));
	printf("[Size] Offset of tail_uv      : %zu\n", offsetof(CatClock_ShaderUniforms, tail_uv));
	printf("[Size] Offset of eyes_uv      : %zu\n", offsetof(CatClock_ShaderUniforms, eyes_uv));
	printf("[Size] Offset of hours_uv     : %zu\n", offsetof(CatClock_ShaderUniforms, hours_uv));
	printf("[Size] Offset of mins_uv      : %zu\n", offsetof(CatClock_ShaderUniforms, mins_uv));
	printf("[Size] Offset of secs_uv      : %zu\n", offsetof(CatClock_ShaderUniforms, secs_uv));
	printf("[Size] Offset of cat_color    : %zu\n", offsetof(CatClock_ShaderUniforms, cat_color));
	printf("[Size] Offset of tie_color    : %zu\n", offsetof(CatClock_ShaderUniforms, tie_color));
	printf("-----------------------------------------------\n");

	printf("[Trace] Calling InitSokolRenderPipeline()...\n");
	InitSokolRenderPipeline();

	ctx.current_win_w = target_w;
	ctx.current_win_h = target_h;
	printf("[Trace] Initializing XBM library...\n");
	/* Initialize the centralized asset library layer */
	ctx.xbm_lib = CatClock_InitXbmLibrary(NULL);
	if (ctx.xbm_lib) {
		Uint8* hb_bits = NULL;
		/* Pull the centralized hitbox allocation configurations out of our library tracker */
		CatClock_GetHitboxData(ctx.xbm_lib, &hb_bits, &ctx.hitbox_mask_w, &ctx.hitbox_mask_h);
		ctx.hitbox_bitmask = hb_bits;
	}
	ctx.texture_cache_stale = true;

	/* Perform an immediate initial synchronization pass to safely bake the atlas sizing properties
	 */
	time_t startup_raw_time = time(NULL);
	struct tm* startup_time_info = localtime(&startup_raw_time);
	int init_h_p = 0, init_m_p = 0, init_s_p = 0;
	if (startup_time_info) {
		init_s_p = startup_time_info->tm_sec % 60;
		init_m_p = startup_time_info->tm_min % 60;
		init_h_p = ((startup_time_info->tm_hour % 12) * 5) + (startup_time_info->tm_min / 12);
	}
	SDL_Renderer* init_placeholder = NULL;
	CatClock_SynchronizePipelineAtlases(&init_placeholder, &ctx, 0.0f, init_h_p, init_m_p,
										init_s_p);

	/* FIX: Allocate texture buffer handles only AFTER sizes are non-zero and validated */
	printf("[Trace] Calling AllocateStreamTextures() with populated boundaries...\n");
	AllocateStreamTextures();

	printf("--- GEOMETRY & ATLAS SHEETS MEMORY PRINT ---\n");
	printf("[Atlas] Tail Sheet  Dimensions: %dx%d\n", ctx.tail_atlas.atlas_w,
		   ctx.tail_atlas.atlas_h);
	printf("[Atlas] Eyes Sheet  Dimensions: %dx%d\n", ctx.eyes_atlas.atlas_w,
		   ctx.eyes_atlas.atlas_h);
	printf("[Atlas] Hours Sheet Dimensions: %dx%d\n", ctx.hours_atlas.atlas_w,
		   ctx.hours_atlas.atlas_h);

	if (ctx.tail_atlas.index_buffer) {
		size_t total_bytes = ctx.tail_atlas.atlas_w * ctx.tail_atlas.atlas_h;
		size_t non_zero_count = 0;
		size_t first_non_zero_offset = 0;
		bool found_first = false;

		for (size_t i = 0; i < total_bytes; i++) {
			if (ctx.tail_atlas.index_buffer[i] != 0) {
				non_zero_count++;
				if (!found_first) {
					first_non_zero_offset = i;
					found_first = true;
				}
			}
		}

		printf("[Buffer Scan] Non-zero palette indexes found in Tail Buffer: %zu / %zu bytes\n",
			   non_zero_count, total_bytes);

		if (found_first) {
			/* Rewind back a bit from the first hit to show the transition from zeros to structural
			 * indices */
			size_t start_print = (first_non_zero_offset > 8) ? (first_non_zero_offset - 8) : 0;
			printf("[Buffer Sample] Structural slice near offset %zu:\n  ", start_print);
			for (size_t i = 0; i < 32 && (start_print + i) < total_bytes; i++) {
				printf("0x%02X ", ctx.tail_atlas.index_buffer[start_print + i]);
			}
			printf("\n");
		} else {
			printf("[Buffer Alert] Entire tail sheet is completely empty (all 0x00)!\n");
		}
	} else {
		printf("[Buffer Alert] Tail index buffer pointer is completely NULL!\n");
	}
	printf("---------------------------------------------\n");

	bool running = true;
	SDL_Event event;

	int current_fps = target_fps_limit <= 0 ? 30 : target_fps_limit;
	Sint32 sleep_timeout_interval = 1000 / current_fps;
	sg_pass_action clear_pass_action
		= { .colors = { [0] = {
							.load_action = SG_LOADACTION_CLEAR,
							/* If this remains gray, we know SDL is hiding our output */
							.clear_value = { 0.0f, 0.4f, 0.9f, 1.0f } // Bright Cobalt Blue
						} } };

	printf("[Trace] Entering main loop with timeout: %d ms...\n", sleep_timeout_interval);
	uint64_t loop_counter = 0;

	while (running) {
		loop_counter++;
		if (loop_counter % 30 == 0) {
			printf("[Trace Loop] Iteration %lu\n", loop_counter);
		}

		if (SDL_WaitEventTimeout(&event, sleep_timeout_interval)) {
			if (event.type == SDL_EVENT_QUIT) {
				printf("[Trace Loop] Quit event detected\n");
				running = false;
				continue;
			}
			if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
				printf("[Trace Loop] Escape key detected\n");
				running = false;
				continue;
			}
			if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
				int w, h;
				SDL_GetWindowSize(window, &w, &h);
				ctx.current_win_w = w;
				ctx.current_win_h = h;
				ctx.texture_cache_stale = true;

				// Recalculate baseline multiplier and execute edge dilation instantly
				float baseline_w = ctx.use_decorations ? 150.0f : 103.0f;
				ctx.current_scale = (float) w / baseline_w;

				CatClock_ExecuteScaleDependentEdgeDilation(ctx.current_scale);

				printf("[Trace Loop] Size changed to: %dx%d\n", w, h);
			}
		}

		ctx.current_frame_step++;
		time_t raw_time = time(NULL);
		struct tm* time_info = localtime(&raw_time);
		int h_p = 0, m_p = 0, s_p = 0;
		if (time_info) {
			s_p = time_info->tm_sec % 60;
			m_p = time_info->tm_min % 60;
			h_p = ((time_info->tm_hour % 12) * 5) + (time_info->tm_min / 12);
		}

		SDL_Renderer* placeholder = NULL;
		CatClock_SynchronizePipelineAtlases(&placeholder, &ctx, 0.0f, h_p, m_p, s_p);

		PushActiveIndexBuffersToVRAM();

		sg_begin_default_pass(&clear_pass_action, ctx.current_win_w, ctx.current_win_h);
		sg_apply_pipeline(quad_pipeline);

		// 1. REMOVE the aggressive multi-slot initialization loop that populates
		// vertex_buffers[1..N] with zero identifiers. Instead, zero out the whole structure cleanly
		// via standard memory clear:
		sg_bindings resource_bindings;
		memset(&resource_bindings, 0, sizeof(resource_bindings));

		/* Bind the static global vertex buffer directly to hardware slot 0 */
		resource_bindings.vertex_buffers[0] = quad_vbuf;
		resource_bindings.vertex_buffer_offsets[0] = 0;

		/* Bind independent texture channels to the Fragment Shader active slots */
		resource_bindings.fs.images[0] = cat_body_mask_img;
		resource_bindings.fs.images[1] = cat_body_img;
		resource_bindings.fs.images[2] = necktie_img;
		resource_bindings.fs.images[3] = tail_img;
		resource_bindings.fs.images[4] = eyes_img;
		resource_bindings.fs.images[5] = hours_img;
		resource_bindings.fs.images[6] = mins_img;
		resource_bindings.fs.images[7] = secs_img;
		resource_bindings.fs.samplers[0] = nearest_sampler;

		// ======================================================
		// --- ONE-TIME PIPELINE GPU RESOURCE MAPPING MATRIX ---
		// ======================================================
		static bool pipeline_diagnostics_logged = false;
		if (!pipeline_diagnostics_logged) {
			printf("\n======================================================\n");
			printf("--- ONE-TIME PIPELINE GPU RESOURCE MAPPING MATRIX ---\n");
			printf("[Pipeline] Active Pipeline ID             : %u\n", quad_pipeline.id);
			printf("[Bindings] Sizeof sg_bindings            : %zu bytes\n", sizeof(sg_bindings));

			size_t reflected_max_vbs = sizeof(resource_bindings.vertex_buffers)
				/ sizeof(resource_bindings.vertex_buffers[0]);
			printf("[Bindings] Deduced Max VB Array Slots    : %zu\n", reflected_max_vbs);

			int runtime_active_vbs = 0;
			for (size_t b = 0; b < reflected_max_vbs; b++) {
				if (resource_bindings.vertex_buffers[b].id != 0) {
					runtime_active_vbs++;
				}
			}
			printf("[Bindings] Active Bound Buffer Count     : %d buffer(s)\n", runtime_active_vbs);
			printf("[Bindings] Mapped Slot 0 Buffer ID       : %u\n",
				   resource_bindings.vertex_buffers[0].id);

			/* Reflect live texture slot assignment values across all 8 independent slots */
			for (int t = 0; t < 8; t++) {
				printf("[Bindings] Resource Texture Unit Slot %d  : Image ID = %u\n", t,
					   resource_bindings.fs.images[t].id);
			}
			printf("[Bindings] Bound Fragment Sampler Unit   : Sampler ID = %u\n",
				   resource_bindings.fs.samplers[0].id);

			// Safely verify context theme variables once
			printf("[Theme] Cat Base Tint Color RGB        : %.2f, %.2f, %.2f\n",
				   (float) ctx.cat_color.r / 255.0f, (float) ctx.cat_color.g / 255.0f,
				   (float) ctx.cat_color.b / 255.0f);
			printf("[Theme] Necktie Tint Color RGB         : %.2f, %.2f, %.2f\n",
				   (float) ctx.tie_color.r / 255.0f, (float) ctx.tie_color.g / 255.0f,
				   (float) ctx.tie_color.b / 255.0f);
			printf("======================================================\n\n");

			pipeline_diagnostics_logged = true;
		}
		// ====================================================================

		// Enforce stable execution scheduling order
		sg_apply_bindings(&resource_bindings);
		PushShaderUniforms();
		sg_draw(0, 4, 1);
		sg_end_pass();
		sg_commit();

		SDL_GL_SwapWindow(window);

#ifdef CATCLOCK_SHOT
		/* Lightning Batch Pass: Dump all 60 index positions in a single instant burst */
		for (int p = 0; p < 60; p++) {
			int h_p_loop = p, m_p_loop = p, s_p_loop = p;
			SDL_Renderer* placeholder_loop = NULL;
			CatClock_SynchronizePipelineAtlases(&placeholder_loop, &ctx, 0.0f, h_p_loop, m_p_loop,
												s_p_loop);
			PushActiveIndexBuffersToVRAM();

			sg_begin_default_pass(&clear_pass_action, ctx.current_win_w, ctx.current_win_h);
			sg_apply_pipeline(quad_pipeline);
			sg_apply_bindings(&resource_bindings);
			PushShaderUniforms();
			sg_draw(0, 4, 1);
			sg_end_pass();
			sg_commit();

			unsigned char* pixels
				= (unsigned char*) SDL_malloc(ctx.current_win_w * ctx.current_win_h * 4);
			if (pixels) {
				glReadPixels(0, 0, ctx.current_win_w, ctx.current_win_h, GL_RGBA, GL_UNSIGNED_BYTE,
							 pixels);
				char path[64];
				sprintf(path, "catclock_phase_%02d.ppm", p);
				FILE* f = fopen(path, "wb");
				if (f) {
					fprintf(f, "P6\n%d %d\n255\n", ctx.current_win_w, ctx.current_win_h);
					for (int y = 0; y < ctx.current_win_h; y++) {
						int sy = ctx.current_win_h - 1 - y;
						fwrite(&pixels[(sy * ctx.current_win_w) * 4], 1, ctx.current_win_w * 3, f);
					}
					fclose(f);
				}
				SDL_free(pixels);
			}
		}
		printf("[DIAG] All 60 phases generated under hardware chroma context. Exiting.\n");
		exit(0);
#endif

#ifdef CATCLOCK_DEBUG
		/* Single-run continuous cycle batch dumper */
		static int debug_frame_count = 0;
		if (debug_frame_count >= 60 && debug_frame_count <= 120) {
			unsigned char* pixels
				= (unsigned char*) SDL_malloc(ctx.current_win_w * ctx.current_win_h * 4);
			if (pixels) {
				glReadPixels(0, 0, ctx.current_win_w, ctx.current_win_h, GL_RGBA, GL_UNSIGNED_BYTE,
							 pixels);
				char path[64];
				sprintf(path, "catclock_live_frame_%d.ppm", debug_frame_count);
				FILE* f = fopen(path, "wb");
				if (f) {
					fprintf(f, "P6\n%d %d\n255\n", ctx.current_win_w, ctx.current_win_h);
					for (int y = 0; y < ctx.current_win_h; y++) {
						int sy = ctx.current_win_h - 1 - y;
						fwrite(&pixels[(sy * ctx.current_win_w) * 4], 1, ctx.current_win_w * 3, f);
					}
					fclose(f);
				}
				SDL_free(pixels);
			}
		}
		debug_frame_count++;
		if (debug_frame_count > 120) {
			printf("[DIAG] Continuous cycle frames captured successfully. Exiting.\n");
			exit(0);
		}
#endif
	}

	printf("[Trace] Cleaning up and exiting...\n");
	ReleaseSokolRenderPipeline();
	sg_shutdown();
	SDL_GL_DestroyContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}

/* ===================================================================
   KITCAT GRAPHICS PIPELINE - FRAGMENT SHADER RUNTIME SOURCE (.fs)
   =================================================================== */

const char* kitcat_fs_src
	= "#version 330 core\n"
	  "in vec2 uv;\n"
	  "out vec4 frag_color;\n"
	  "\n"
	  "uniform sampler2D tex_body_mask;\n"
	  "uniform sampler2D tex_body;\n"
	  "uniform sampler2D tex_tie;\n"
	  "uniform sampler2D tex_tail;\n"
	  "uniform sampler2D tex_eyes;\n"
	  "uniform sampler2D tex_hours;\n"
	  "uniform sampler2D tex_minutes;\n"
	  "uniform sampler2D tex_seconds;\n"
	  "\n"
	  "uniform ub_theme {\n"
	  "    vec4 tail_uv;\n"
	  "    vec4 eyes_uv;\n"
	  "    vec4 hours_uv;\n"
	  "    vec4 mins_uv;\n"
	  "    vec4 secs_uv;\n"
	  "    vec4 cat_color;\n"
	  "    vec4 tie_color;\n"
	  "    vec4 pupil_color;\n"
	  "    vec4 sclera_color;\n"
	  "    vec4 detail_color;\n"
	  "};\n"
	  "\n"
	  "vec4 map_palette(float red_channel) {\n"
	  "    int p = int(red_channel * 255.0 + 0.5);\n"
	  "    if (p == 0) return vec4(0.0);\n" // PALETTE_TRANSPARENT
	  "    if (p == 1) return cat_color;\n" // PALETTE_CAT_BODY
	  "    if (p == 2) return sclera_color;\n" // PALETTE_SCLERA
	  "    if (p == 3) return pupil_color;\n" // PALETTE_PUPIL
	  "    if (p == 4) return cat_color;\n" // PALETTE_HAND_HOUR
	  "    if (p == 5) return cat_color;\n" // PALETTE_HAND_MINUTE
	  "    if (p == 6) return detail_color;\n" // PALETTE_HAND_SECOND
	  "    if (p == 7) return tie_color;\n" // PALETTE_NECKTIE
	  "    if (p == 8) return detail_color;\n" // PALETTE_HALO
	  "    return vec4(0.0);\n"
	  "}\n"
	  "\n"
	  "void main() {\n"
	  "    /* Headless GPU Pipeline Channel Isolation Diagnostics */\n"
	  "    float val_mask = texture(tex_body_mask, uv).r;\n"
	  "    float val_body = texture(tex_body, uv).r;\n"
	  "    float val_tie  = texture(tex_tie, uv).r;\n"
	  "    float val_tail = texture(tex_tail, uv).r;\n"
	  "\n"
	  "    if (val_body > 0.0) {\n"
	  "        frag_color = vec4(0.0, 1.0, 0.0, 1.0); /* Neon Green for Body Lines */\n"
	  "    } else if (val_tie > 0.0) {\n"
	  "        frag_color = vec4(0.0, 0.5, 1.0, 1.0); /* Cyan-Blue for Necktie Layer */\n"
	  "    } else if (val_tail > 0.0) {\n"
	  "        frag_color = vec4(1.0, 0.0, 1.0, 1.0); /* Hot Pink for Tail Animations */\n"
	  "    } else if (val_mask > 0.0) {\n"
	  "        frag_color = vec4(0.3, 0.3, 0.3, 1.0); /* Dark Gray for Silhouette Base Footprint "
	  "*/\n"
	  "    } else {\n"
	  "        frag_color = vec4(1.0, 1.0, 0.0, 1.0); /* Bright Solid Yellow Geometry Fallback */\n"
	  "    }\n"
	  "}\n";

const char* kitcat_vs_src = "#version 330 core\n"
							"layout(location=0) in vec2 position;\n"
							"layout(location=1) in vec2 texcoord;\n"
							"out vec2 uv;\n"
							"void main() {\n"
							"    gl_Position = vec4(position, 0.0, 1.0);\n"
							"    uv = texcoord;\n"
							"}\n";
