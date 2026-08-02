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

#include "catclock_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifndef SOKOL_IMPL
#define SOKOL_IMPL
#endif
#if !defined(SOKOL_GLCORE) && !defined(SOKOL_D3D11) && !defined(SOKOL_METAL)
#define SOKOL_GLCORE
#endif

#include "sokol/sokol_gfx.h"
#include "sokol/sokol_log.h"
#if defined(SOKOL_D3D11)
#include "catclock_shaders_d3d11.h"
#else
#include "catclock_shaders_gl.h"
#endif

#if defined(SOKOL_D3D11)
#include <d3d11.h>

/* Global engine pointers needed to map our custom presentation pass handles */
static ID3D11Device* g_d3d11_device = NULL;
static ID3D11DeviceContext* g_d3d11_context = NULL;
static IDXGISwapChain* g_swap_chain = NULL;
static ID3D11RenderTargetView* g_render_target_view = NULL;
#endif

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

/* Global tracking state instance matching our shared interface declaration */
CatClock_AppContext ctx = { 0 };
int target_fps_limit = DEFAULT_FPS;

/* File-scoped persistent texture slots isolating Sokol resources */
static sg_image hours_atlas_image_slot = { .id = SG_INVALID_ID };
static sg_image minutes_atlas_image_slot = { .id = SG_INVALID_ID };
static sg_image seconds_atlas_image_slot = { .id = SG_INVALID_ID };
static sg_image eyes_atlas_image_slot = { .id = SG_INVALID_ID };
static sg_image tail_atlas_image_slot = { .id = SG_INVALID_ID };

static sg_view hours_atlas_view_slot = { .id = SG_INVALID_ID };
static sg_view minutes_atlas_view_slot = { .id = SG_INVALID_ID };
static sg_view seconds_atlas_view_slot = { .id = SG_INVALID_ID };
static sg_view eyes_atlas_view_slot = { .id = SG_INVALID_ID };
static sg_view tail_atlas_view_slot = { .id = SG_INVALID_ID };

/* Offscreen Render Targets for Cached Baked Layers */
static sg_image rt_layer1_backdrop = { .id = SG_INVALID_ID };
static sg_image rt_layer3_foreground = { .id = SG_INVALID_ID };

static sg_view rt_layer1_backdrop_pass_view = { 0 };
static sg_view rt_layer3_foreground_pass_view = { 0 };
static sg_view rt_layer1_backdrop_sample_view = { 0 };
static sg_view rt_layer3_foreground_sample_view = { 0 };

/* Pipeline States */
static sg_pipeline offscreen_bake_pip_backdrop = { 0 };
static sg_pipeline offscreen_bake_pip_foreground = { 0 };
static sg_pipeline draw_tail_pipeline = { 0 };
static sg_pipeline draw_hands_pipeline = { 0 };
static sg_pipeline draw_pupils_pipeline = { 0 };

static SDL_HitTestResult SDLCALL WidgetWindowHitTest(SDL_Window* win, const SDL_Point* area,
													 void* data) {
	(void) win;
	(void) data;
	if (ctx.use_decorations)
		return SDL_HITTEST_NORMAL;
	if (!ctx.hitbox_bits)
		return SDL_HITTEST_DRAGGABLE;

	float current_scale = (float) ctx.current_half_steps / 2.0f;
	int x = (int) ((float) area->x / current_scale);
	int y = (int) ((float) area->y / current_scale);

	if (x < 0 || x >= ctx.software_mask_w || y < 0 || y >= ctx.software_mask_h) {
		return SDL_HITTEST_NORMAL;
	}

	int bytes_per_row = (ctx.software_mask_w + 7) / 8;
	int byte_index = (y * bytes_per_row) + (x / 8);
	int bit_position = x % 8;
	bool is_solid = (ctx.hitbox_bits[byte_index] & (1 << bit_position)) != 0;
	return is_solid ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}

/* ==========================================================================
   APPLICATION ENTRY RUNTIME LAYER
   ========================================================================== */
void Diagnostics_DumpMaterialCompositionToDisk(struct CatClock_XbmLibrary* library) {
	if (!library)
		return;

	/* Calculate the active scale factor derived from the Stage 2 integer tracker */
	float active_scale = (float) ctx.current_half_steps / 2.0f;

	/* Fetch pristine unscaled original workspace sizing metrics from your assets metadata */
	int base_w = ASSET_BODY_W;
	int base_h = ASSET_BODY_H;

	/* Scale final destination sheet composition canvas boundaries */
	int comp_w = (int) ceilf((float) base_w * active_scale);
	int comp_h = (int) ceilf((float) base_h * active_scale);

	/* 1. Allocate a temporary buffer to capture the unscaled 1x pre-aligned layout mask */
	uint8_t* unscaled_staging = (uint8_t*) malloc(base_w * base_h);
	if (!unscaled_staging)
		return;

	/* Invoke the core engine routine to bake and resolve layered 1x token fields perfectly */
	CatClock_BakeUnscaledMaterialIDStaging(unscaled_staging, library);

	/* 2. Allocate the scaled composition target canvas buffer */
	uint8_t* composition_buffer = (uint8_t*) calloc(1, comp_w * comp_h);
	if (!composition_buffer) {
		free(unscaled_staging);
		return;
	}

	/* 3. Upsample the pre-aligned combined material index array to match integer steps */
	for (int y = 0; y < comp_h; y++) {
		int src_y = (int) ((float) y / active_scale);
		if (src_y >= base_h)
			continue;

		for (int x = 0; x < comp_w; x++) {
			int src_x = (int) ((float) x / active_scale);
			if (src_x >= base_w)
				continue;

			/* Direct index sampling from the pre-resolved unscaled mesh layout array */
			composition_buffer[(y * comp_w) + x] = unscaled_staging[(src_y * base_w) + src_x];
		}
	}

	/* Free the unscaled placeholder buffer since processing is complete */
	free(unscaled_staging);

	/* Write out the cleanly scaled PAM matrix directly using your canonical disk dump sink */
	CatClock_DebugDumpPamToDisk("dump_material_composition.pam", composition_buffer, comp_w,
								comp_h);
	free(composition_buffer);
}

static void CatClock_NormalizeColorToUniform(SDL_Color src, float dest_array[4]) {
	dest_array[0] = (float) src.r / 255.0f;
	dest_array[1] = (float) src.g / 255.0f;
	dest_array[2] = (float) src.b / 255.0f;
	dest_array[3] = (float) src.a / 255.0f;
}

void ReallocateOffscreenTargets(int w, int h) {
	(void) w;
	if (rt_layer1_backdrop_pass_view.id != SG_INVALID_ID)
		sg_destroy_view(rt_layer1_backdrop_pass_view);
	if (rt_layer1_backdrop_sample_view.id != SG_INVALID_ID)
		sg_destroy_view(rt_layer1_backdrop_sample_view);
	if (rt_layer3_foreground_pass_view.id != SG_INVALID_ID)
		sg_destroy_view(rt_layer3_foreground_pass_view);
	if (rt_layer3_foreground_sample_view.id != SG_INVALID_ID)
		sg_destroy_view(rt_layer3_foreground_sample_view);
	if (rt_layer1_backdrop.id != SG_INVALID_ID)
		sg_destroy_image(rt_layer1_backdrop);
	if (rt_layer3_foreground.id != SG_INVALID_ID)
		sg_destroy_image(rt_layer3_foreground);

	rt_layer1_backdrop_pass_view.id = rt_layer1_backdrop_sample_view.id = SG_INVALID_ID;
	rt_layer3_foreground_pass_view.id = rt_layer3_foreground_sample_view.id = SG_INVALID_ID;
	rt_layer1_backdrop.id = rt_layer3_foreground.id = SG_INVALID_ID;

	// Calculate target width strictly normalized to the full 128px structural layout stride.
	// This ensures our offscreen color attachments provide enough padded margin space.
	float structural_scale = (float) ctx.current_half_steps / 2.0f;
	int intermediate_stride_w = (int) lroundf(128.0f * structural_scale);

	sg_image_desc img_desc
		= { .usage.color_attachment = true,
			.width = intermediate_stride_w, // Bind structural layout width allocation
			.height = h,
			.pixel_format = SG_PIXELFORMAT_RGBA8,
			.label = "RT-Layer1-Static-Backdrop-Texture" };
	rt_layer1_backdrop = sg_make_image(&img_desc);

	img_desc.label = "RT-Layer3-Static-Foreground-Texture";
	rt_layer3_foreground = sg_make_image(&img_desc);

	if (sg_query_image_state(rt_layer1_backdrop) == SG_RESOURCESTATE_VALID) {
		rt_layer1_backdrop_pass_view
			= sg_make_view(&(sg_view_desc) { .color_attachment.image = rt_layer1_backdrop,
											 .label = "RT-Layer1-Backdrop-Pass-View" });
		rt_layer1_backdrop_sample_view = sg_make_view(&(sg_view_desc) {
			.texture.image = rt_layer1_backdrop, .label = "RT-Layer1-Backdrop-Sample-View" });
	}
	if (sg_query_image_state(rt_layer3_foreground) == SG_RESOURCESTATE_VALID) {
		rt_layer3_foreground_pass_view
			= sg_make_view(&(sg_view_desc) { .color_attachment.image = rt_layer3_foreground,
											 .label = "RT-Layer3-Foreground-Pass-View" });
		rt_layer3_foreground_sample_view = sg_make_view(&(sg_view_desc) {
			.texture.image = rt_layer3_foreground, .label = "RT-Layer3-Foreground-Sample-View" });
	}
}

void InitializeOffscreenGenerationPipelines(void) {
	// A. BASE PIPELINE FOR CACHED PRE-RENDERS
	sg_pipeline_desc pip_desc
		= { .shader = sg_make_shader(catclock_bake_shader_desc(sg_query_backend())),
			.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
			.index_type = SG_INDEXTYPE_UINT16,
			.label = "BakePipeline-Backdrop" };
	pip_desc.layout.attrs[ATTR_catclock_bake_position].format = SG_VERTEXFORMAT_FLOAT2;
	pip_desc.layout.attrs[ATTR_catclock_bake_texcoord0].format = SG_VERTEXFORMAT_FLOAT2;
	pip_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
	pip_desc.depth.pixel_format = SG_PIXELFORMAT_NONE;

	offscreen_bake_pip_backdrop = sg_make_pipeline(&pip_desc);
	pip_desc.label = "BakePipeline-Foreground";
	offscreen_bake_pip_foreground = sg_make_pipeline(&pip_desc);

	// B. DECENTRALIZED COMPONENT INTERPOLATION PIPELINES
	sg_pipeline_desc overlay_desc = { .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
									  .index_type = SG_INDEXTYPE_UINT16,
									  .label = "OverlayPipeline-Tail" };
	overlay_desc.layout.attrs[ATTR_catclock_tail_position].format = SG_VERTEXFORMAT_FLOAT2;
	overlay_desc.layout.attrs[ATTR_catclock_tail_texcoord0].format = SG_VERTEXFORMAT_FLOAT2;
	overlay_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
	overlay_desc.colors[0].blend
		= (sg_blend_state) { .enabled = true,
							 .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
							 .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
							 .src_factor_alpha = SG_BLENDFACTOR_ONE,
							 .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA };

	overlay_desc.shader = sg_make_shader(catclock_tail_shader_desc(sg_query_backend())),
	draw_tail_pipeline = sg_make_pipeline(&overlay_desc);

	overlay_desc.shader = sg_make_shader(catclock_hands_shader_desc(sg_query_backend()));
	overlay_desc.layout.attrs[ATTR_catclock_hands_position].format = SG_VERTEXFORMAT_FLOAT2;
	overlay_desc.layout.attrs[ATTR_catclock_hands_texcoord0].format = SG_VERTEXFORMAT_FLOAT2;
	overlay_desc.label = "OverlayPipeline-Hands";
	draw_hands_pipeline = sg_make_pipeline(&overlay_desc);

	overlay_desc.shader = sg_make_shader(catclock_pupils_shader_desc(sg_query_backend()));
	overlay_desc.layout.attrs[ATTR_catclock_pupils_position].format = SG_VERTEXFORMAT_FLOAT2;
	overlay_desc.layout.attrs[ATTR_catclock_pupils_texcoord0].format = SG_VERTEXFORMAT_FLOAT2;
	overlay_desc.label = "OverlayPipeline-Pupils";
	draw_pupils_pipeline = sg_make_pipeline(&overlay_desc);
}

void ExecuteOffscreenBakePasses(int w, int h, cb_params_bake_t* uniform_payload) {
	(void) w;
	sg_bindings base_bindings = { 0 };
	base_bindings.vertex_buffers[0] = ctx.vertex_buffer;
	base_bindings.index_buffer = ctx.index_buffer;
	base_bindings.samplers[SMP_main_sampler] = ctx.body_mask_sampler;
	base_bindings.views[VIEW_texture_sheet] = ctx.body_mask_view;

	sg_pass_action clear_action = { 0 };
	clear_action.colors[0].load_action = SG_LOADACTION_CLEAR;
	clear_action.colors[0].clear_value = (sg_color) { 0.0f, 0.0f, 0.0f, 0.0f };

	// The geometry spacing is anchored entirely to the 128px structural layout stride.
	// This uncouples offscreen calculations from the variable window parameters.
	float structural_scale = (float) ctx.current_half_steps / 2.0f;
	int intermediate_stride_w = (int) lroundf(128.0f * structural_scale);

	// The viewport starts at zero and spans the full width of the 128px target container sheet.
	// This completely eliminates out-of-bounds rendering scissors on Windows/Direct3D11.
	int bake_offset_x = 0;
	int bake_width = intermediate_stride_w;

	// --- Pass 1: Render Backdrop Target ---
	uniform_payload->generation_mode_flag = 1;

	sg_pass backdrop_pass = { 0 };
	backdrop_pass.action = clear_action;
	backdrop_pass.attachments.colors[0] = rt_layer1_backdrop_pass_view;

	sg_begin_pass(&backdrop_pass);
	sg_apply_pipeline(offscreen_bake_pip_backdrop);
	sg_apply_bindings(&base_bindings);
	sg_apply_viewport(bake_offset_x, 0, bake_width, h, true);
	sg_apply_uniforms(UB_cb_params_bake, &SG_RANGE(*uniform_payload));
	sg_draw(0, 4, 1);
	sg_end_pass();

	// --- Pass 2: Render Foreground Target ---
	uniform_payload->generation_mode_flag = 2;

	sg_pass foreground_pass = { 0 };
	foreground_pass.action = clear_action;
	foreground_pass.attachments.colors[0] = rt_layer3_foreground_pass_view;

	sg_begin_pass(&foreground_pass);
	sg_apply_pipeline(offscreen_bake_pip_foreground);
	sg_apply_bindings(&base_bindings);
	sg_apply_viewport(bake_offset_x, 0, bake_width, h, true);
	sg_apply_uniforms(UB_cb_params_bake, &SG_RANGE(*uniform_payload));
	sg_draw(0, 4, 1);
	sg_end_pass();
}

void CatClock_BakeAtlasToVram(sg_image* target_vram_slot, sg_view* target_view_slot,
							  const uint8_t* raw_index_grid, int width, int height,
							  const char* label) {
	if (!raw_index_grid || width <= 0 || height <= 0)
		return;

	if (target_view_slot->id != SG_INVALID_ID) {
		if (sg_query_view_state(*target_view_slot) == SG_RESOURCESTATE_VALID)
			sg_destroy_view(*target_view_slot);
		target_view_slot->id = SG_INVALID_ID;
	}
	if (target_vram_slot->id != SG_INVALID_ID) {
		if (sg_query_image_state(*target_vram_slot) == SG_RESOURCESTATE_VALID)
			sg_destroy_image(*target_vram_slot);
		target_vram_slot->id = SG_INVALID_ID;
	}

	sg_image_desc texture_blueprint
		= { .width = width,
			.height = height,
			.pixel_format = SG_PIXELFORMAT_R8,
			.label = label,
			.data = { .mip_levels = { { .ptr = raw_index_grid,
										.size = (size_t) (width * height * sizeof(uint8_t)) } } } };
	*target_vram_slot = sg_make_image(&texture_blueprint);

	if (sg_query_image_state(*target_vram_slot) == SG_RESOURCESTATE_VALID) {
		*target_view_slot = sg_make_view(
			&(sg_view_desc) { .texture = { .image = *target_vram_slot }, .label = label });
	}
}

void PackHandUvExtents(float* target_uv_array, int frame_index,
					   const CatClock_ComputeAtlas* atlas) {
	if (!atlas || !target_uv_array)
		return;

	int cols = 10;
	int col = frame_index % cols;
	int row = frame_index / cols;
	float c_w = (float) atlas->cell_w;
	float c_h = (float) atlas->cell_h;
	float a_w = (float) atlas->atlas_w;
	float a_h = (float) atlas->atlas_h;

	target_uv_array[0] = (col * c_w) / a_w;
	target_uv_array[1] = (row * c_h) / a_h;
	target_uv_array[2] = ((col + 1) * c_w) / a_w;
	target_uv_array[3] = ((row + 1) * c_h) / a_h;
}

void CatClock_ResizeWindow(SDL_Window* window, int base_w, int base_h, float scale) {
	/* Calculate target client area size in native screen coordinates */
	int requested_client_w = (int) lroundf((float) base_w * scale);
	int requested_client_h = (int) lroundf((float) base_h * scale);

	/* 1. Fire the imperative window frame bounds adjustment */
	SDL_SetWindowSize(window, requested_client_w, requested_client_h);

#if defined(SOKOL_D3D11)
	/* 2. TARGET-FENCED SWAPCHAIN SYNCHRONIZATION:
	 * Direct3D11 requires the physical backbuffer to scale cleanly with the window frame.
	 * If we don't resize the buffers here, DXGI stretches a stale 1x surface,
	 * causing the exponential double-scaling and body outline truncation bugs! */
	if (g_swap_chain) {
		/* Release the active render target view reference before changing sizes */
		if (g_render_target_view) {
			g_render_target_view->lpVtbl->Release(g_render_target_view);
			g_render_target_view = NULL;
		}

		/* Manually force DXGI to recreate the backbuffer to match the true physical window pixels
		 */
		g_swap_chain->lpVtbl->ResizeBuffers(g_swap_chain, 1, requested_client_w, requested_client_h,
											DXGI_FORMAT_R8G8B8A8_UNORM, 0);

		/* Query and re-link the fresh backbuffer reference safely */
		ID3D11Texture2D* back_buffer = NULL;
		HRESULT hr = g_swap_chain->lpVtbl->GetBuffer(g_swap_chain, 0, &IID_ID3D11Texture2D,
													 (void**) &back_buffer);
		if (SUCCEEDED(hr) && back_buffer) {
			g_d3d11_device->lpVtbl->CreateRenderTargetView(
				g_d3d11_device, (ID3D11Resource*) back_buffer, NULL, &g_render_target_view);
			back_buffer->lpVtbl->Release(back_buffer);
		}
	}
#endif
}

int main(int argc, char* argv[]) {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fprintf(stderr, "[Fatal Error] SDL_Init subsystem initialization failure.\n");
		return 1;
	}

	ParseCommandLineArguments(argc, argv, &ctx);

#if defined(__linux__) && !defined(__ANDROID__)
	SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, "0");
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
#endif

	float baseline_w = ctx.use_decorations ? DECORATED_CANVAS_W : UNDECORATED_CANVAS_W;
	float baseline_h = ctx.use_decorations ? DECORATED_CANVAS_H : UNCORATED_CANVAS_H;
	float scale = (float) ctx.current_half_steps / 2.0f;
	int target_w = (int) lroundf(baseline_w * scale);
	int target_h = (int) lroundf(baseline_h * scale);

	SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_TRANSPARENT;
	if (!ctx.use_decorations) {
		window_flags |= SDL_WINDOW_BORDERLESS;
	}
	if (!ctx.disable_always_on_top) {
		window_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
	}

	ctx.window = SDL_CreateWindow("CatClock", target_w, target_h, window_flags);
	if (!ctx.window) {
		fprintf(stderr, "[Fatal Error] Host Window abstraction layer failed to map.\n");
		SDL_Quit();
		return 1;
	}

	SDL_SetWindowHitTest(ctx.window, WidgetWindowHitTest, NULL);

#ifdef _WIN32
	if (ctx.use_decorations) {
		SDL_PropertiesID window_props = SDL_GetWindowProperties(ctx.window);
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

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GLContext gl_context = SDL_GL_CreateContext(ctx.window);
	if (!gl_context) {
		fprintf(stderr, "[Fatal Error] OpenGL Hardware Context instantiation dropped.\n");
		SDL_DestroyWindow(ctx.window);
		SDL_Quit();
		return 1;
	}
	SDL_GL_MakeCurrent(ctx.window, gl_context);

	sg_desc sokol_description
		= { .logger.func = slog_func,
			.environment = { .defaults = { .color_format = SG_PIXELFORMAT_RGBA8,
										   .depth_format = SG_PIXELFORMAT_NONE } } };

#if defined(SOKOL_D3D11)
#if defined(DEBUG)
	/* 1. Allocate a visible tracking console frame on Windows */
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
	setvbuf(stdout, NULL, _IONBF, 0); // Force unbuffered stdout streaming

	printf("\n========================================================\n");
	printf("   SOKOL DIRECTX HARDWARE DEBUGGING CONSOLE STAGE\n");
	printf("========================================================\n");
#endif

	SDL_PropertiesID win_props = SDL_GetWindowProperties(ctx.window);
	if (win_props) {
		HWND hwnd = (HWND) SDL_GetPointerProperty(win_props, "SDL.window.win32.hwnd", NULL);
		if (hwnd) {
			DXGI_SWAP_CHAIN_DESC scd;
			memset(&scd, 0, sizeof(scd));
			scd.BufferCount = 1;
			scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			scd.OutputWindow = hwnd;
			scd.SampleDesc.Count = 1;
			scd.Windowed = TRUE;

			D3D_FEATURE_LEVEL feature_level = (D3D_FEATURE_LEVEL) 0;
			D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
										   D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };

			/* 2. INJECT D3D11_CREATE_DEVICE_DEBUG FLAG TO FORCE HARDWARE VALIDATION LAYER CHANNELS
			 */
			UINT createDeviceFlags = 0;
#if defined(DEBUG)
			createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
			printf(
				"[Debug Subsystem] Attempting to hook native OS validation layer components...\n");
#endif

			HRESULT hr = D3D11CreateDeviceAndSwapChain(
				NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, levels, 4,
				D3D11_SDK_VERSION, &scd, &g_swap_chain, &g_d3d11_device, &feature_level,
				&g_d3d11_context);
#if defined(DEBUG)
			printf("[D3D11 Base Initialization Status]: HRESULT = 0x%08X\n", (unsigned int) hr);
#endif
			if (SUCCEEDED(hr) && g_d3d11_device && g_d3d11_context) {
				sokol_description.environment.d3d11.device = g_d3d11_device;
				sokol_description.environment.d3d11.device_context = g_d3d11_context;

				ID3D11Texture2D* back_buffer = NULL;
				hr = g_swap_chain->lpVtbl->GetBuffer(g_swap_chain, 0, &IID_ID3D11Texture2D,
													 (void**) &back_buffer);
#if defined(DEBUG)
				printf("[Swapchain Backbuffer Query Status]: HRESULT = 0x%08X\n",
					   (unsigned int) hr);
#endif

				if (SUCCEEDED(hr) && back_buffer) {
					hr = g_d3d11_device->lpVtbl->CreateRenderTargetView(
						g_d3d11_device, (ID3D11Resource*) back_buffer, NULL, &g_render_target_view);
#if defined(DEBUG)
					printf("[Render Target View Creation Status]: HRESULT = 0x%08X | Pointer: %p\n",
						   (unsigned int) hr, (void*) g_render_target_view);
#endif
					back_buffer->lpVtbl->Release(back_buffer);
				}
			}
		}
	}
#if defined(DEBUG)
	printf("========================================================\n\n");
#endif
#endif

	sg_setup(&sokol_description);

	if (!sg_isvalid()) {
		fprintf(stderr, "[Fatal Error] Sokol GFX framework context layer validation failure.\n");
		SDL_GL_MakeCurrent(ctx.window, NULL);
		SDL_GL_DestroyContext(gl_context);
		SDL_DestroyWindow(ctx.window);
		SDL_Quit();
		return 1;
	}
#if defined(DEBUG)
	printf("[Trace] Sokol Core GFX successfully attached to standard rendering pipeline.\n");
#endif

	struct CatClock_XbmLibrary* runtime_xbm_handle = CatClock_InitXbmLibrary(NULL);
	if (!runtime_xbm_handle) {
		fprintf(
			stderr,
			"[Fatal Error] Crucial system XBM assets directory mapping failed to initialize.\n");
		sg_shutdown();
		SDL_GL_MakeCurrent(ctx.window, NULL);
		SDL_GL_DestroyContext(gl_context);
		SDL_DestroyWindow(ctx.window);
		SDL_Quit();
		return 1;
	}

	CatClock_GpuVertex clock_vertices[] = { { .pos = { -1.0f, 1.0f }, .uv = { 0.0f, 0.0f } },
											{ .pos = { 1.0f, 1.0f }, .uv = { 1.0f, 0.0f } },
											{ .pos = { -1.0f, -1.0f }, .uv = { 0.0f, 1.0f } },
											{ .pos = { 1.0f, -1.0f }, .uv = { 1.0f, 1.0f } } };

	uint16_t clock_indices[] = { 0, 1, 2, 3 };

	ctx.vertex_buffer = sg_make_buffer(
		&(sg_buffer_desc) { .data = { .ptr = clock_vertices, .size = sizeof(clock_vertices) },
							.label = "ClockMeshVertexBuffer" });

	ctx.index_buffer = sg_make_buffer(
		&(sg_buffer_desc) { .usage = { .index_buffer = true },
							.data = { .ptr = clock_indices, .size = sizeof(clock_indices) },
							.label = "ClockMeshIndexBuffer" });

	uint8_t *catback_ptr = NULL, *tie_ptr = NULL, *catwhite_ptr = NULL, *eyes_ptr = NULL;
	int dummy_w = 0, dummy_h = 0;
	CatClock_GetCatbackData(runtime_xbm_handle, &catback_ptr, &dummy_w, &dummy_h);
	CatClock_GetCattieBodyData(runtime_xbm_handle, &tie_ptr, &dummy_w, &dummy_h);
	CatClock_GetCatwhiteData(runtime_xbm_handle, &catwhite_ptr, &dummy_w, &dummy_h);
	CatClock_GetEyesData(runtime_xbm_handle, &eyes_ptr, &dummy_w, &dummy_h);

	uint32_t* staging_pixels
		= (uint32_t*) malloc(VRAM_TEX_WIDTH * VRAM_TEX_HEIGHT * sizeof(uint32_t));
	if (!staging_pixels) {
		fprintf(stderr, "[Fatal] Failed to allocate staging VRAM unpack buffer.\n");
		return 1;
	}
	CatClock_UnpackStaticAssetsToStagingBuffer(staging_pixels, catback_ptr, tie_ptr, catwhite_ptr,
											   eyes_ptr);

// =========================================================================
// STAGING PIXELS DIAGNOSTIC DUMP FOR HARDWARE BUFFER VERIFICATION
// =========================================================================
#ifdef TRACE_PIXEL_INTEGRITY
	{
		long long body_count = 0, tie_count = 0, white_count = 0, eyes_count = 0;
		uint32_t* audit_pixels
			= (uint32_t*) malloc(VRAM_TEX_WIDTH * VRAM_TEX_HEIGHT * sizeof(uint32_t));

		if (audit_pixels) {
			/* Sample the exact state of the staging canvas backing payload */
			CatClock_UnpackStaticAssetsToStagingBuffer(audit_pixels, catback_ptr, tie_ptr,
													   catwhite_ptr, eyes_ptr);

			for (int y = 0; y < VRAM_TEX_HEIGHT; y++) {
				for (int x = 0; x < VRAM_TEX_WIDTH; x++) {
					uint32_t raw_pixel = audit_pixels[y * VRAM_TEX_WIDTH + x];
					if (raw_pixel & 0xFF)
						body_count++; /* R Channel */
					if ((raw_pixel >> 8) & 0xFF)
						tie_count++; /* G Channel */
					if ((raw_pixel >> 16) & 0xFF)
						white_count++; /* B Channel */
					if ((raw_pixel >> 24) & 0xFF)
						eyes_count++; /* A Channel */
				}
			}
			free(audit_pixels);

			printf("[VRAM Matrix Monitor] Scale Step: %u | Active Layout Footprint Counts:\n",
				   ctx.current_half_steps);
			printf("  -> Body Pixel Count  : %lld\n", body_count);
			printf("  -> Tie Pixel Count   : %lld\n", tie_count);
			printf("  -> White Pixel Count : %lld\n", white_count);
			printf("  -> Eyes Pixel Count  : %lld\n", eyes_count);
		}
	}
#endif

#ifdef DEBUG_DUMP_STAGING_PIXELS
	{
		FILE* audit_dump = fopen("vram_static_staging_dump.pam", "wb");
		if (audit_dump) {
			fputs("P7\n", audit_dump);
			fprintf(audit_dump, "WIDTH %d\n", VRAM_TEX_WIDTH);
			fprintf(audit_dump, "HEIGHT %d\n", VRAM_TEX_HEIGHT);
			fputs("DEPTH 4\n", audit_dump);
			fputs("MAXVAL 255\n", audit_dump);
			fputs("TUPLTYPE RGB_ALPHA\n", audit_dump);
			fputs("ENDHDR\n", audit_dump);

			long long r_act = 0, g_act = 0, b_act = 0, a_act = 0;

			// We must temporarily re-allocate the master pixels array to sample the
			// active texture state
			uint32_t* audit_pixels
				= (uint32_t*) malloc(VRAM_TEX_WIDTH * VRAM_TEX_HEIGHT * sizeof(uint32_t));
			if (audit_pixels) {
				// Re-fetch the layout masks using your original unpacking criteria
				CatClock_UnpackStaticAssetsToStagingBuffer(audit_pixels, catback_ptr, tie_ptr,
														   catwhite_ptr, eyes_ptr);

				for (int y = 0; y < VRAM_TEX_HEIGHT; y++) {
					for (int x = 0; x < VRAM_TEX_WIDTH; x++) {
						uint32_t raw_pixel = audit_pixels[y * VRAM_TEX_WIDTH + x];
						uint8_t body_mask = raw_pixel & 0xFF;
						uint8_t tie_mask = (raw_pixel >> 8) & 0xFF;
						uint8_t white_mask = (raw_pixel >> 16) & 0xFF;
						uint8_t eyes_mask = (raw_pixel >> 24) & 0xFF;

						if (body_mask > 0)
							r_act++;
						if (tie_mask > 0)
							g_act++;
						if (white_mask > 0)
							b_act++;
						if (eyes_mask > 0)
							a_act++;

						uint8_t out_pixel[4]
							= { 40, 40, 40, 0 }; // Default transparent workspace background

						if (eyes_mask > 128) {
							out_pixel[0] = ctx.sclera_color.r;
							out_pixel[1] = ctx.sclera_color.g;
							out_pixel[2] = ctx.sclera_color.b;
							out_pixel[3] = 255;
						} else if (white_mask > 128) {
							out_pixel[0] = ctx.detail_color.r;
							out_pixel[1] = ctx.detail_color.g;
							out_pixel[2] = ctx.detail_color.b;
							out_pixel[3] = 255;
						} else if (tie_mask > 128) {
							out_pixel[0] = ctx.tie_color.r;
							out_pixel[1] = ctx.tie_color.g;
							out_pixel[2] = ctx.tie_color.b;
							out_pixel[3] = 255;
						} else if (body_mask > 128) {
							out_pixel[0] = ctx.cat_color.r;
							out_pixel[1] = ctx.cat_color.g;
							out_pixel[2] = ctx.cat_color.b;
							out_pixel[3] = 255;
						}
						fwrite(out_pixel, 1, 4, audit_dump);
					}
				}
				free(audit_pixels);
			}
			fclose(audit_dump);
			printf("\n[Pixel Integrity Audit] DETECTED MASK CHANNEL DENSITY REPORT:\n");
			printf("  -> R Channel (Body Nodes) Count   : %lld\n", r_act);
			printf("  -> G Channel (Necktie Nodes) Count : %lld\n", g_act);
			printf("  -> B Channel (White Nodes) Count   : %lld\n", b_act);
			printf("  -> A Channel (Eyes Sockets) Count  : %lld\n\n", a_act);
		}
	}
#endif

	ctx.body_mask_texture = sg_make_image(&(sg_image_desc) {
		.width = VRAM_TEX_WIDTH,
		.height = VRAM_TEX_HEIGHT,
		.pixel_format = SG_PIXELFORMAT_RGBA8,
		.data
		= { .mip_levels = { { .ptr = staging_pixels,
							  .size = VRAM_TEX_WIDTH * VRAM_TEX_HEIGHT * sizeof(uint32_t) } } },
		.label = "CatBodyMaskTexture" });

	ctx.body_mask_sampler = sg_make_sampler(&(sg_sampler_desc) { .min_filter = SG_FILTER_NEAREST,
																 .mag_filter = SG_FILTER_NEAREST,
																 .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
																 .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
																 .label = "CatBodyMaskSampler" });

	ctx.body_mask_view = sg_make_view(&(sg_view_desc) {
		.texture = { .image = ctx.body_mask_texture }, .label = "CatBodyMaskResourceView" });
	free(staging_pixels);
#if defined(DEBUG)
	printf("[VRAM Init] Static image buffer assets and samplers committed to GPU context.\n");
#endif

	sg_pipeline_desc pip_desc
		= { .shader = sg_make_shader(catclock_composite_shader_desc(sg_query_backend())),
			.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
			.index_type = SG_INDEXTYPE_UINT16,
			.label = "ClockMainRenderingPipeline" };
	pip_desc.layout.attrs[ATTR_catclock_composite_position].format = SG_VERTEXFORMAT_FLOAT2;
	pip_desc.layout.attrs[ATTR_catclock_composite_texcoord0].format = SG_VERTEXFORMAT_FLOAT2;
	pip_desc.colors[0]
		= (sg_color_target_state) { .pixel_format = SG_PIXELFORMAT_RGBA8,
									.blend
									= { .enabled = true,
										.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
										.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
										.op_rgb = SG_BLENDOP_ADD,
										.src_factor_alpha = SG_BLENDFACTOR_ONE,
										.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
										.op_alpha = SG_BLENDOP_ADD } };

	ctx.draw_pipeline = sg_make_pipeline(&pip_desc);
	if (sg_query_pipeline_state(ctx.draw_pipeline) != SG_RESOURCESTATE_VALID) {
		fprintf(stderr, "[Fatal Error] GFX state pipeline compilation pass failed.\n");
		return 1;
	}
#if defined(DEBUG)
	printf("[Pipeline Init] Sokol drawing state maps compiled successfully.\n");
#endif

	InitializeOffscreenGenerationPipelines();

	struct {
		int type;
		SDL_Color color;
	} hour_cfg = { HAND_TYPE_HOUR, ctx.hour_color };
	struct {
		int type;
		SDL_Color color;
	} min_cfg = { HAND_TYPE_MINUTE, ctx.minute_color };
	struct {
		int type;
		SDL_Color color;
	} sec_cfg = { HAND_TYPE_SECOND, ctx.seconds_color };
	CatClock_TailShaderArgs tail_data = { 0.0f, 0.0f, false };

	int init_pixel_w = 0, init_pixel_h = 0;
	SDL_GetWindowSizeInPixels(ctx.window, &init_pixel_w, &init_pixel_h);
	if (init_pixel_w <= 0 || init_pixel_h <= 0) {
		float init_scale = (float) ctx.current_half_steps / 2.0f;
		int logical_w = (int) lroundf(
			(ctx.use_decorations ? DECORATED_CANVAS_W : UNDECORATED_CANVAS_W) * init_scale);
		int logical_h = (int) lroundf(
			(ctx.use_decorations ? DECORATED_CANVAS_H : UNCORATED_CANVAS_H) * init_scale);
		init_pixel_w = logical_w;
		init_pixel_h = logical_h;
	}

	ReallocateOffscreenTargets(init_pixel_w, init_pixel_h);
	ctx.texture_cache_stale = true;

	bool running = true;
	SDL_Event event;
	int target_fps = (ctx.target_fps <= 0) ? DEFAULT_FPS : ctx.target_fps;
	Uint64 frame_delay_ms = 1000 / target_fps;
	Uint64 last_frame_time = SDL_GetTicks();
	bool force_redraw = true;

	int active_viewport_w = init_pixel_w;
	int active_viewport_h = init_pixel_h;
#if defined(DEBUG)
	printf("[Runtime Pacing] Main evaluation engine started. Target FPS: %d\n", target_fps);
#endif

	while (running) {
		Uint64 frame_start_ticks = SDL_GetTicks();

		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_EVENT_QUIT:
				running = false;
				break;

			case SDL_EVENT_KEY_DOWN:
				if (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_Q) {
					running = false;
				} else if (event.key.key == SDLK_EQUALS || event.key.key == SDLK_PLUS
						   || event.key.key == SDLK_KP_PLUS
						   || event.key.scancode == SDL_SCANCODE_EQUALS) {
					ctx.current_half_steps++;
					ctx.texture_cache_stale = true;
					float updated_scale = (float) ctx.current_half_steps / 2.0f;
					CatClock_ResizeWindow(ctx.window, baseline_w, baseline_h, updated_scale);

					// --- STABILIZATION TRIGGER ---
					active_viewport_w = (int) lroundf(baseline_w * updated_scale);
					active_viewport_h = (int) lroundf(baseline_h * updated_scale);
					ReallocateOffscreenTargets(active_viewport_w, active_viewport_h);
					// -----------------------------

					force_redraw = true;
				} else if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS
						   || event.key.scancode == SDL_SCANCODE_MINUS) {
					if (ctx.current_half_steps > 1) {
						ctx.current_half_steps--;
						ctx.texture_cache_stale = true;
						float updated_scale = (float) ctx.current_half_steps / 2.0f;
						CatClock_ResizeWindow(ctx.window, baseline_w, baseline_h, updated_scale);

						// --- STABILIZATION TRIGGER ---
						active_viewport_w = (int) lroundf(baseline_w * updated_scale);
						active_viewport_h = (int) lroundf(baseline_h * updated_scale);
						ReallocateOffscreenTargets(active_viewport_w, active_viewport_h);
						// -----------------------------

						force_redraw = true;
					}
				}

				break;

			case SDL_EVENT_MOUSE_WHEEL:
				if (event.wheel.y > 0.0f) {
					ctx.current_half_steps++;
					ctx.texture_cache_stale = true;
					float updated_scale = (float) ctx.current_half_steps / 2.0f;
					CatClock_ResizeWindow(ctx.window, baseline_w, baseline_h, updated_scale);

					// --- STABILIZATION TRIGGER ---
					active_viewport_w = (int) lroundf(baseline_w * updated_scale);
					active_viewport_h = (int) lroundf(baseline_h * updated_scale);
					ReallocateOffscreenTargets(active_viewport_w, active_viewport_h);
					// -----------------------------

					force_redraw = true;
				} else if (event.wheel.y < 0.0f) {
					if (ctx.current_half_steps > 1) {
						ctx.current_half_steps--;
						ctx.texture_cache_stale = true;
						float updated_scale = (float) ctx.current_half_steps / 2.0f;
						CatClock_ResizeWindow(ctx.window, baseline_w, baseline_h, updated_scale);

						// --- STABILIZATION TRIGGER ---
						active_viewport_w = (int) lroundf(baseline_w * updated_scale);
						active_viewport_h = (int) lroundf(baseline_h * updated_scale);
						ReallocateOffscreenTargets(active_viewport_w, active_viewport_h);
						// -----------------------------

						force_redraw = true;
					}
				}
				break;
			default:
				break;
			}
		}

		Uint64 current_ticks = SDL_GetTicks();
		if (force_redraw || (current_ticks - last_frame_time >= frame_delay_ms)) {
			force_redraw = false;
			last_frame_time = current_ticks;

			SDL_GetWindowSizeInPixels(ctx.window, &active_viewport_w, &active_viewport_h);
			if (active_viewport_w <= 0 || active_viewport_h <= 0) {
				force_redraw = true;
				continue;
			}

			static cb_params_bake_t bake_uniform_payload;
			memset(&bake_uniform_payload, 0, sizeof(cb_params_bake_t));

			static Uint64 baseline_ticks = 0;
			static float cached_day_time_seconds = 0.0f;
			if (ctx.texture_cache_stale) {
				time_t dynamic_raw_time = time(NULL);
				struct tm* local_time_segments = localtime(&dynamic_raw_time);
				cached_day_time_seconds
					= (float) (local_time_segments->tm_hour * 3600
							   + local_time_segments->tm_min * 60 + local_time_segments->tm_sec);
				baseline_ticks = SDL_GetTicks();
			}
			float elapsed_delta_seconds = (float) (SDL_GetTicks() - baseline_ticks) / 1000.0f;
			float computed_day_time_seconds = cached_day_time_seconds + elapsed_delta_seconds;

			int current_sec = (int) fmodf(computed_day_time_seconds, 60.0f);
			int current_min = (int) fmodf(computed_day_time_seconds / 60.0f, 60.0f);
			int current_hour = (int) fmodf(computed_day_time_seconds / 3600.0f, 12.0f);
			int sec_frame_idx = current_sec % 60;
			int min_frame_idx = current_min % 60;
			int hour_frame_idx = ((current_hour * 5) + (current_min / 12)) % 60;
			float dynamic_fps = (float) ((ctx.target_fps <= 0) ? DEFAULT_FPS : ctx.target_fps);
			float total_anim_frames = dynamic_fps * 2.0f;
			int pendulum_frame_idx
				= (int) fmod(computed_day_time_seconds * dynamic_fps, total_anim_frames);
			int calculated_rows = ((ctx.target_fps * 2) + 9) / 10;
			int dec_flag_value = ctx.use_decorations ? 1 : 0;

			if (ctx.texture_cache_stale) {
				CatClock_RebakeComputeAtlas(NULL, &ctx.hours_atlas, HAND_CELL_W, HAND_CELL_H,
											TOTAL_HAND_PHASES, 10, CatClock_ShaderHands, &hour_cfg);
				CatClock_RebakeComputeAtlas(NULL, &ctx.minutes_atlas, HAND_CELL_W, HAND_CELL_H,
											TOTAL_HAND_PHASES, 10, CatClock_ShaderHands, &min_cfg);
				CatClock_RebakeComputeAtlas(NULL, &ctx.seconds_atlas, HAND_CELL_W, HAND_CELL_H,
											TOTAL_HAND_PHASES, 10, CatClock_ShaderHands, &sec_cfg);
				CatClock_RebakeComputeAtlas(NULL, &ctx.eyes_atlas, EYES_CELL_W, EYES_CELL_H,
											(ctx.target_fps * 2), 10, CatClock_ShaderEyes, NULL);
				CatClock_RebakeComputeAtlas(NULL, &ctx.tail_atlas, TAIL_CELL_W, TAIL_CELL_H,
											(ctx.target_fps * 2), 10, CatClock_ShaderTail,
											&tail_data);

#ifdef DEBUG_DUMP_ATLAS
				Diagnostics_DumpMaterialCompositionToDisk(runtime_xbm_handle);
				printf("[Trace] Dynamic textures cached and committed to disk files.\n");
#endif

				CatClock_BakeAtlasToVram(&hours_atlas_image_slot, &hours_atlas_view_slot,
										 ctx.hours_atlas.index_buffer, ctx.hours_atlas.atlas_w,
										 ctx.hours_atlas.atlas_h, "CatClock-HoursHands-Atlas");
				CatClock_BakeAtlasToVram(&minutes_atlas_image_slot, &minutes_atlas_view_slot,
										 ctx.minutes_atlas.index_buffer, ctx.minutes_atlas.atlas_w,
										 ctx.minutes_atlas.atlas_h, "CatClock-MinutesHands-Atlas");
				CatClock_BakeAtlasToVram(&seconds_atlas_image_slot, &seconds_atlas_view_slot,
										 ctx.seconds_atlas.index_buffer, ctx.seconds_atlas.atlas_w,
										 ctx.seconds_atlas.atlas_h, "CatClock-SecondsHands-Atlas");
				CatClock_BakeAtlasToVram(&eyes_atlas_image_slot, &eyes_atlas_view_slot,
										 ctx.eyes_atlas.index_buffer, ctx.eyes_atlas.atlas_w,
										 ctx.eyes_atlas.atlas_h, "CatClock-DynamicEyes-Atlas");
				CatClock_BakeAtlasToVram(&tail_atlas_image_slot, &tail_atlas_view_slot,
										 ctx.tail_atlas.index_buffer, ctx.tail_atlas.atlas_w,
										 ctx.tail_atlas.atlas_h, "CatClock-DynamicTail-Atlas");

				ReallocateOffscreenTargets(active_viewport_w, active_viewport_h);

				CatClock_NormalizeColorToUniform(ctx.cat_color, bake_uniform_payload.cat_color);
				CatClock_NormalizeColorToUniform(ctx.tie_color, bake_uniform_payload.tie_color);
				CatClock_NormalizeColorToUniform(ctx.pupil_color, bake_uniform_payload.pupil_color);
				CatClock_NormalizeColorToUniform(ctx.sclera_color,
												 bake_uniform_payload.sclera_color);
				CatClock_NormalizeColorToUniform(ctx.detail_color,
												 bake_uniform_payload.detail_color);
				CatClock_NormalizeColorToUniform(ctx.outline_color,
												 bake_uniform_payload.outline_color);

				bake_uniform_payload.hour_frame_idx = hour_frame_idx;
				bake_uniform_payload.min_frame_idx = min_frame_idx;
				bake_uniform_payload.sec_frame_idx = sec_frame_idx;
				bake_uniform_payload.pendulum_frame_idx = pendulum_frame_idx;
				bake_uniform_payload.tail_pupils_rows = calculated_rows;

				ExecuteOffscreenBakePasses(active_viewport_w, active_viewport_h,
										   &bake_uniform_payload);
				ctx.texture_cache_stale = false;
			}
			cb_tail_params_t tail_payload;
			memset(&tail_payload, 0, sizeof(cb_tail_params_t));
			tail_payload.tail_frame = pendulum_frame_idx;
			tail_payload.tail_pupils_rows = calculated_rows;
			tail_payload.use_decorations_flag = dec_flag_value;
			CatClock_NormalizeColorToUniform(ctx.cat_color, tail_payload.cat_color);
			CatClock_NormalizeColorToUniform(ctx.outline_color, tail_payload.outline_color);

			cb_hands_params_t hands_payload;
			memset(&hands_payload, 0, sizeof(cb_hands_params_t));
			hands_payload.hour_frame = hour_frame_idx;
			hands_payload.min_frame = min_frame_idx;
			hands_payload.sec_frame = sec_frame_idx;
			hands_payload.use_decorations_flag = dec_flag_value;
			CatClock_NormalizeColorToUniform(ctx.hour_color, hands_payload.hour_color);
			CatClock_NormalizeColorToUniform(ctx.minute_color, hands_payload.minute_color);
			CatClock_NormalizeColorToUniform(ctx.seconds_color, hands_payload.seconds_color);

			cb_pupil_params_t pupil_payload;
			memset(&pupil_payload, 0, sizeof(cb_pupil_params_t));
			pupil_payload.pupil_frame = pendulum_frame_idx;
			pupil_payload.tail_pupils_rows = calculated_rows;
			pupil_payload.use_decorations_flag = dec_flag_value;
			CatClock_NormalizeColorToUniform(ctx.pupil_color, pupil_payload.pupil_color);

			// =========================================================================
			// RECORD AND DISPATCH GRAPHICS PIPELINE COMMANDS (PURE TEXTURE SHUFFLING)
			// =========================================================================

			sg_pass_action clock_pass_clear_action = { 0 };
			clock_pass_clear_action.colors[0].load_action = SG_LOADACTION_CLEAR;

			if (ctx.use_decorations) {
				clock_pass_clear_action.colors[0].clear_value = (sg_color) {
					(float) ctx.window_bg_color.r / 255.0f, (float) ctx.window_bg_color.g / 255.0f,
					(float) ctx.window_bg_color.b / 255.0f, (float) ctx.window_bg_color.a / 255.0f
				};
			} else {
				clock_pass_clear_action.colors[0].clear_value
					= (sg_color) { 0.0f, 0.0f, 0.0f, 0.0f };
			}

			sg_pass swapchain_pass = { 0 };
			swapchain_pass.action = clock_pass_clear_action;
			swapchain_pass.swapchain.width = active_viewport_w;
			swapchain_pass.swapchain.height = active_viewport_h;

#if defined(SOKOL_D3D11)
			swapchain_pass.swapchain.sample_count = 1;
			swapchain_pass.swapchain.color_format = SG_PIXELFORMAT_RGBA8;
			swapchain_pass.swapchain.d3d11.render_view = (const void*) g_render_target_view;
#endif

			sg_begin_pass(&swapchain_pass);

			// Base bindings setup matching standard engine layout arrays
			sg_bindings base_bindings = { 0 };
			base_bindings.vertex_buffers[0] = ctx.vertex_buffer;
			base_bindings.index_buffer = ctx.index_buffer;
			base_bindings.samplers[SMP_main_sampler] = ctx.body_mask_sampler;
			base_bindings.views[VIEW_texture_sheet] = ctx.body_mask_view;
			base_bindings.views[VIEW_hours_hand_sheet] = hours_atlas_view_slot;
			base_bindings.views[VIEW_mins_hand_sheet] = minutes_atlas_view_slot;
			base_bindings.views[VIEW_seconds_hand_sheet] = seconds_atlas_view_slot;
			base_bindings.views[VIEW_eyes_sheet] = eyes_atlas_view_slot;
			base_bindings.views[VIEW_tail_sheet] = tail_atlas_view_slot;

			// ============================================================================
			// DYNAMIC WINDOW DECORATION CANVASSING & ALIGNMENT OVERRIDES
			// ============================================================================
			float presentation_scale = (float) ctx.current_half_steps / 2.0f;

			// Set core defaults matching canonical transparent borderless mode:
			int final_offset_x = (int) lroundf(-23.0f * presentation_scale);
			int final_offset_y = 0;
			int final_width = (int) lroundf(128.0f * presentation_scale);
			int final_height
				= (int) lroundf(288.0f * presentation_scale); // Added tracking height metric

			if (ctx.use_decorations) {
				// 1. Maintain a zeroed margin baseline. The 24px asset padding handles
				// horizontal stride spacing natively within the texture map.
				final_offset_x = 0;

				// 2. Clear out exactly 10px of top margin room below the title frame:
				final_offset_y = (int) lroundf(10.0f * presentation_scale);
			}

			// ----------------------------------------------------------------------------
			// --- LAYER 1: Body Outline (Lowest Level Stack) ---
			// ----------------------------------------------------------------------------
			base_bindings.views[VIEW_rt_backdrop_tex] = rt_layer1_backdrop_sample_view;
			base_bindings.views[VIEW_rt_foreground_tex] = rt_layer1_backdrop_sample_view;
			sg_apply_pipeline(ctx.draw_pipeline);
			sg_apply_bindings(&base_bindings);
			sg_apply_viewport(final_offset_x, final_offset_y, final_width, final_height, true);
			sg_draw(0, 4, 1);

			// ----------------------------------------------------------------------------
			// --- LAYERS 2 & 3: Tail Outline, then Tail (Middle Stack) ---
			// ----------------------------------------------------------------------------
			sg_apply_pipeline(draw_tail_pipeline);
			sg_apply_bindings(&base_bindings);
			// CRITICAL FIX: Lock tail viewport identically to the static mesh bounds
			sg_apply_viewport(final_offset_x, final_offset_y, final_width, final_height, true);
			sg_apply_uniforms(UB_cb_tail_params, &SG_RANGE(tail_payload));
			sg_draw(0, 4, 1);

			// ----------------------------------------------------------------------------
			// --- LAYER 4: Body (Highest Level Mesh Torso) ---
			// ----------------------------------------------------------------------------
			base_bindings.views[VIEW_rt_backdrop_tex] = rt_layer3_foreground_sample_view;
			base_bindings.views[VIEW_rt_foreground_tex] = rt_layer3_foreground_sample_view;
			sg_apply_pipeline(ctx.draw_pipeline);
			sg_apply_bindings(&base_bindings);
			sg_apply_viewport(final_offset_x, final_offset_y, final_width, final_height, true);
			sg_draw(0, 4, 1);

			// ----------------------------------------------------------------------------
			// --- OVERLAYS: Dynamic Facial Features (Eyes & Hands) ---
			// ----------------------------------------------------------------------------
			sg_apply_pipeline(draw_pupils_pipeline);
			sg_apply_bindings(&base_bindings);
			// CRITICAL FIX: Lock pupil viewport identically to the static mesh bounds
			sg_apply_viewport(final_offset_x, final_offset_y, final_width, final_height, true);
			sg_apply_uniforms(UB_cb_pupil_params, &SG_RANGE(pupil_payload));
			sg_draw(0, 4, 1);

			sg_apply_pipeline(draw_hands_pipeline);
			sg_apply_bindings(&base_bindings);
			// CRITICAL FIX: Lock clock hands viewport identically to the static mesh bounds
			sg_apply_viewport(final_offset_x, final_offset_y, final_width, final_height, true);
			sg_apply_uniforms(UB_cb_hands_params, &SG_RANGE(hands_payload));
			sg_draw(0, 4, 1);

			sg_end_pass();
			sg_commit();

#if defined(SOKOL_D3D11)
			if (g_swap_chain) {
				/* PRESENT INSTANTLY: Strips DXGI of VSync pacing authority to allow manual loop
				 * master timing */
				g_swap_chain->lpVtbl->Present(g_swap_chain, 0, DXGI_PRESENT_DO_NOT_WAIT);
			}
#else
			/* OpenGL handles standard context swaps natively on Linux/Unix */
			SDL_GL_SwapWindow(ctx.window);
#endif
		}

		Uint64 loop_execution_duration = SDL_GetTicks() - frame_start_ticks;
		if (loop_execution_duration < frame_delay_ms) {
			SDL_Delay((Uint32) (frame_delay_ms - loop_execution_duration));
		}
	}

#ifdef DEBUG_DUMP_ATLAS
	printf("[Trace] Validation pass finished. Component layout extraction complete.\n");
#endif

	CatClock_DestroyComputeAtlas(&ctx.hours_atlas);
	CatClock_DestroyComputeAtlas(&ctx.minutes_atlas);
	CatClock_DestroyComputeAtlas(&ctx.seconds_atlas);
	CatClock_DestroyComputeAtlas(&ctx.eyes_atlas);
	CatClock_DestroyComputeAtlas(&ctx.tail_atlas);
	if (runtime_xbm_handle) {
		CatClock_DestroyXbmLibrary(runtime_xbm_handle);
		runtime_xbm_handle = NULL;
	}

	if (sg_query_view_state(hours_atlas_view_slot) == SG_RESOURCESTATE_VALID)
		sg_destroy_view(hours_atlas_view_slot);
	if (sg_query_view_state(minutes_atlas_view_slot) == SG_RESOURCESTATE_VALID)
		sg_destroy_view(minutes_atlas_view_slot);
	if (sg_query_view_state(seconds_atlas_view_slot) == SG_RESOURCESTATE_VALID)
		sg_destroy_view(seconds_atlas_view_slot);
	if (sg_query_view_state(eyes_atlas_view_slot) == SG_RESOURCESTATE_VALID)
		sg_destroy_view(eyes_atlas_view_slot);
	if (sg_query_view_state(tail_atlas_view_slot) == SG_RESOURCESTATE_VALID)
		sg_destroy_view(tail_atlas_view_slot);

	if (sg_query_pipeline_state(ctx.draw_pipeline) == SG_RESOURCESTATE_VALID)
		sg_destroy_pipeline(ctx.draw_pipeline);
	if (sg_query_pipeline_state(draw_tail_pipeline) == SG_RESOURCESTATE_VALID)
		sg_destroy_pipeline(draw_tail_pipeline);
	if (sg_query_pipeline_state(draw_hands_pipeline) == SG_RESOURCESTATE_VALID)
		sg_destroy_pipeline(draw_hands_pipeline);
	if (sg_query_pipeline_state(draw_pupils_pipeline) == SG_RESOURCESTATE_VALID)
		sg_destroy_pipeline(draw_pupils_pipeline);

	if (sg_query_buffer_state(ctx.vertex_buffer) == SG_RESOURCESTATE_VALID)
		sg_destroy_buffer(ctx.vertex_buffer);
	if (sg_query_buffer_state(ctx.index_buffer) == SG_RESOURCESTATE_VALID)
		sg_destroy_buffer(ctx.index_buffer);
	if (sg_query_image_state(ctx.body_mask_texture) == SG_RESOURCESTATE_VALID)
		sg_destroy_image(ctx.body_mask_texture);
	if (sg_query_sampler_state(ctx.body_mask_sampler) == SG_RESOURCESTATE_VALID)
		sg_destroy_sampler(ctx.body_mask_sampler);
	if (sg_query_view_state(ctx.body_mask_view) == SG_RESOURCESTATE_VALID)
		sg_destroy_view(ctx.body_mask_view);

	sg_shutdown();
	SDL_GL_MakeCurrent(ctx.window, NULL);
	SDL_GL_DestroyContext(gl_context);
	SDL_DestroyWindow(ctx.window);
	SDL_Quit();
#if defined(DEBUG)
	printf("[Trace] Execution Context terminated cleanly.\n");
#endif
	return 0;
}
