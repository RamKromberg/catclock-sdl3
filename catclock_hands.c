/******************************************************************************
 * File Name:    catclock_hands.c
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
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define PORTABLEGL_IMPLEMENTATION
#define PGL_PREFIX_TYPES
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif
#include "portablegl/portablegl.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

const int TEXTURE_CELL_W = 64;
const int TEXTURE_CELL_H = 96;
const int RELATIVE_FOCAL_X = 29;
const int RELATIVE_FOCAL_Y = 39;
const int ENVELOPE_PAD_X = 2;
const int ENVELOPE_PAD_Y = 6;
const int PIVOT_AXIS_X = ENVELOPE_PAD_X + RELATIVE_FOCAL_X;
const int PIVOT_AXIS_Y = ENVELOPE_PAD_Y + RELATIVE_FOCAL_Y;

// ========================================================================
// CONFIGURATION ROUTER
// 0 = WIP PRODUCTION WORK SHADER (ACTUAL RASTER PIPELINE)
// 1 = CENTERED TRIANGLE TEST
// 2 = PIVOT-ANCHORED TRIANGLE TEST
// 3 = CENTERED DIAMOND RASTER TEST
// 4 = EXPLICIT CCW DIAMOND RASTER TEST
// 5 = CROSS TEST (LINE TEST)
// 6 = ISOLATION TEST A: PURE COUNTER-CLOCKWISE (CCW) VERTEX ORDER
// 7 = ISOLATION TEST B: PURE CLOCKWISE (CW) VERTEX ORDER (FORCES ROTATION)
// 8 = ISOLATION TEST C: SUBPIXEL SNAP AT EXACT EXPLICIT INTEGER BOUNDS
// 9 = ISOLATION TEST D: FRACTIONAL BIAS OFFSET TEST (+0.5 PIXEL SHIFT)
// 10 = FRAME FILL
// 11 = BOUNDING BOX FILL
// ========================================================================
#ifndef TEST_MODE
#define TEST_MODE 0
#endif

// Forward declarations sharing an identical layout payload profile
static void DrawProductionFrame(uint8_t* buffer, int sheet_w, int sheet_h, int frame_idx,
								float scale, float px_f, float py_f, int px, int py,
								uint8_t hand_color, uint8_t hand_halo);
static void CatClock_CalculateHandEndpointsRaster(int phase, float pivot_xf, float pivot_yf,
												  float scale, int* out_piv_x, int* out_piv_y,
												  int* out_end_x, int* out_end_y);

// Unified Uniform Structure passing baseline rendering parameters to the shader
typedef struct {
	pgl_mat4 projection;
	pgl_mat4 translation;
	pgl_mat4 pivot;
	pgl_mat4 scale;
} TestUniforms;

// Instrumented Vertex Shader capturing internal sub-pixel coordinate steps
static void test_vert(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins,
					  void* uniforms) {
	TestUniforms* uni = (TestUniforms*) uniforms;
	pgl_vec4 local_pos = vertex_attribs[0];

	// Explicitly enforce the homogeneous coordinate to preserve matrix translation vectors
	local_pos.w = 1.0f;

	// 1. Compute the scaled pivot matrix directly via a matrix-matrix product transformation
	// (Reversing parameters multiplies the scale matrix onto the pivot translation vectors cleanly)
	pgl_mat4 scaled_pivot;
	mult_m4_m4(scaled_pivot, uni->scale, uni->pivot);

	// 2. Correct Right-to-Left OpenGL Post-Multiplication Flow Sequence:
	// Pass the raw local_pos to the scaled_pivot matrix to prevent local geometry double-scaling
	// errors
	pgl_vec4 pivoted_pos = mult_m4_v4(scaled_pivot, local_pos);
	pgl_vec4 world_pos = mult_m4_v4(uni->translation, pivoted_pos);
	pgl_vec4 ndc_pos = mult_m4_v4(uni->projection, world_pos);

#if (TEST_MODE != 0)
	static int vertex_trace_count = 0;
	if (vertex_trace_count++ < 4) {
		printf("[SHADER-VERT-TRACE] Vert ID: %d\n", vertex_trace_count);
		printf("  -> Input Local Float Pos: (%.2f, %.2f, %.2f)\n", local_pos.x, local_pos.y,
			   local_pos.z);
		printf("  -> Origin Zero Aligned:   (%.2f, %.2f)\n", pivoted_pos.x, pivoted_pos.y);
		printf("  -> World Space Atlas Pos: (%.2f, %.2f)\n", world_pos.x, world_pos.y);
		printf("  -> Final Transformed NDC: (%.2f, %.2f, %.2f, %.2f)\n", ndc_pos.x, ndc_pos.y,
			   ndc_pos.z, ndc_pos.w);
	}
#endif

	builtins->gl_Position = ndc_pos;

	// Retain downstream color pass-through varyings via safe full-width array tokens
	vs_output[0] = vertex_attribs[1].x;
	vs_output[1] = vertex_attribs[1].y;
	vs_output[2] = vertex_attribs[1].z;
	vs_output[3] = vertex_attribs[1].w;
}

// Standalone Fragment Shader Callback tracking standard palette states
static void test_frag(float* fs_input, Shader_Builtins* builtins, void* uniforms) {
	(void) uniforms;

	// Extract the cleanly rasterized interpolated color vectors directly from input varyings
	builtins->gl_FragColor.x = fs_input[0];
	builtins->gl_FragColor.y = fs_input[1];
	builtins->gl_FragColor.z = fs_input[2];
	builtins->gl_FragColor.w = fs_input[3];
}

// ============================================================================
// MAIN PIPELINE ENTRY INTERFACE
// ============================================================================
void CatClock_ShaderHands(void* renderer, int cell_x, int cell_y, int sheet_w, int sheet_h,
						  int frame_idx, void* userdata) {
	uint8_t* buffer = (uint8_t*) renderer;

	// Unified Core Metric Pipeline
	float scale = (float) ctx.current_half_steps / 2.0f;
	float px_f = (float) cell_x + ((float) PIVOT_AXIS_X * scale);
	float py_f = (float) cell_y + ((float) PIVOT_AXIS_Y * scale);
	int px = (int) roundf(px_f);
	int py = (int) roundf(py_f);

	// Unified Configuration & Palette Block
	struct {
		int type;
		SDL_Color color;
	}* hand_cfg = (typeof(hand_cfg)) userdata;

	int hand_type = hand_cfg ? hand_cfg->type : 0;
	uint8_t hand_color = PALETTE_HAND_SECOND;
	uint8_t hand_halo = PALETTE_HALO;

	if (hand_type == HAND_TYPE_HOUR) {
		hand_color = PALETTE_HAND_HOUR;
	} else if (hand_type == HAND_TYPE_MINUTE) {
		hand_color = PALETTE_HAND_MINUTE;
	}

	// Interchangeable calling interface pattern
	if (TEST_MODE == 0) {
		DrawProductionFrame(buffer, sheet_w, sheet_h, frame_idx, scale, px_f, py_f, px, py,
							hand_color, hand_halo);
		return;
	}

	// =================================================================
	// PORTABLEGL UNIFIED TEST FRAMEWORK (MODES 1-9)
	// =================================================================
	if (frame_idx != 0 || hand_type != HAND_TYPE_SECOND) {
		return;
	}

	printf("[DIAG-FRAME-START] FrameIdx: %d | Scale: %.4f\n", frame_idx, scale);
	printf("[DIAG-FRAME-PIVOT] Float: (%.4f, %.4f) | Rounded: (%d, %d)\n", px_f, py_f, px, py);
	printf("[DIAG-FRAME-CONFIG] Target Colors - Color: %u, Halo: %u\n", hand_color, hand_halo);

	GLfloat* target_vertices = NULL;
	GLsizei vertex_data_size = 0;
	GLsizei target_vertex_count = 3;

	float mode1_half = 10.0f;
	float mode2_size = 20.0f;

	/* clang-format off */
	// Test Mode 1: Symmetrical CCW Primitive Quad Configuration (Biases Removed)
	GLfloat triangle_vertices_mode1[] = {
		// Triangle 1 (CCW)
		-mode1_half, -mode1_half, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // Top-Left
		-mode1_half,  mode1_half, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // Bottom-Left
		 mode1_half, -mode1_half, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // Top-Right

		// Triangle 2 (CCW)
		 mode1_half, -mode1_half, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // Top-Right
		-mode1_half,  mode1_half, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // Bottom-Left
		 mode1_half,  mode1_half, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f  // Bottom-Right
	};

    // Test Mode 2: Pivot-Anchored Box Configuration projecting cleanly Down and Right
    GLfloat triangle_vertices_mode2[] = {
        // Triangle 1 (CCW)
        0.0f,        0.0f,       0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // Pivot Origin
        0.0f,        mode2_size, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // Downward extension
        mode2_size,  0.0f,       0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // Rightward extension

        // Triangle 2 (CCW)
        mode2_size,  0.0f,       0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // Rightward extension
        0.0f,        mode2_size, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // Downward extension
        mode2_size,  mode2_size, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f  // Bottom-Right Corner
    };
	/* clang-format on */

	float diamond_r = 15.0f;
	/* clang-format off */
	// Symmetrical Subpixel Edge Calibration Array
	GLfloat triangle_vertices_mode3[] = {
		// Triangle 1: Right-half face (Top, Right Tip, Bottom)
		// Pushing the vertical boundaries slightly right (+0.01f) breaks the tie-break column drop
		 0.01f,             diamond_r, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f, 
		 diamond_r + 0.01f, 0.0f,      0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f, 
		 0.01f,            -diamond_r, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f, 

		// Triangle 2: Left-half face (Top, Bottom, Left Tip)
		// Pulling the shared spine slightly left (-0.01f) forces coverage evaluation
		-0.01f,             diamond_r, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f, 
		-0.01f,            -diamond_r, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f, 
		-diamond_r - 0.01f, 0.0f,      0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f  
	};

	// Test Mode 4: Symmetrical Subpixel Edge-Calibrated Explicit CCW Diamond Configuration
	GLfloat triangle_vertices_mode4[] = {
		// Triangle 1 (CCW): Central Top, Left Tip, Central Bottom
		// Shifting the shared center spine slightly left (-0.01f) and outer tip left (-0.01f)
		 0.00f - 0.01f,  diamond_r,         0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // Central Top
		-diamond_r - 0.01f, 0.00f,         0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // Left Tip
		 0.00f - 0.01f, -diamond_r,         0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // Central Bottom

		// Triangle 2 (CCW): Central Top, Central Bottom, Right Tip
		// Shifting the shared center spine slightly right (+0.01f) and outer tip right (+0.01f)
		 0.00f + 0.01f,  diamond_r,         0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // Central Top
		 0.00f + 0.01f, -diamond_r,         0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // Central Bottom
		 diamond_r + 0.01f, 0.00f,         0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f  // Right Tip
	};
	/* clang-format on */

	float mode67_offset = 15.0f;
	// Test Mode 6: Front-Facing CCW Sequence projecting Down and Right
	GLfloat triangle_vertices_mode6[] = {
		0.0f,		   0.0f,		  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // 0: Origin Center Pivot
		0.0f,		   mode67_offset, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // 1: Downward leg
		mode67_offset, mode67_offset, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f // 2: Bottom-Right Corner
	};

	// Test Mode 7: Back-Facing CW Sequence projecting Down and Right
	GLfloat triangle_vertices_mode7[] = {
		0.0f,		   0.0f,		  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // 0: Origin Center Pivot
		mode67_offset, mode67_offset, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // 1: Bottom-Right Corner
		0.0f,		   mode67_offset, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f // 2: Downward leg
	};

	float mode89_offset = 10.0f;
	GLfloat triangle_vertices_mode8[] = {
		0.0f,		   0.0f,		  0.0f, 1.0f,
		1.0f,		   0.0f,		  0.0f, 1.0f, // Focal Center Pivot
		mode89_offset, 0.0f,		  0.0f, 1.0f,
		1.0f,		   0.0f,		  0.0f, 1.0f, // Extends Rightward
		0.0f,		   mode89_offset, 0.0f, 1.0f,
		1.0f,		   0.0f,		  0.0f, 1.0f // Extends Downward (Screen Space Symmetrical)
	};
	switch (TEST_MODE) {
	case 1:
		printf("[TEST-1-INPUT] Centered Triangle Primitive Bounding Configuration:\n");
		target_vertices = triangle_vertices_mode1;
		vertex_data_size = sizeof(triangle_vertices_mode1);
		target_vertex_count = 6;
		break;
	case 2:
		printf("[TEST-2-INPUT] Pivot-Anchored Triangle Box Configuration:\n");
		target_vertices = triangle_vertices_mode2;
		vertex_data_size = sizeof(triangle_vertices_mode2);
		target_vertex_count = 6;
		break;
	case 3:
		printf("[TEST-3-INPUT] Centered Diamond Configuration (Integer Anchored):\n");
		target_vertices = triangle_vertices_mode3;
		vertex_data_size = sizeof(triangle_vertices_mode3);
		target_vertex_count = 6;
		break;
	case 4:
		printf("[TEST-4-INPUT] Explicit CCW Diamond Configuration:\n");
		target_vertices = triangle_vertices_mode4;
		vertex_data_size = sizeof(triangle_vertices_mode4);
		target_vertex_count = 6;
		break;
	case 5: {
		printf("[TEST-5-INPUT] Pure Line Raster Field Check via PortableGL primitives:\n");

#define TEST5_ARM_LEN 15.0f

		/* clang-format off */
        static GLfloat line_cross_vertices[] = {
            // Line segment 1: Horizontal Arm (From Left to Right)
            -TEST5_ARM_LEN, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, //
            TEST5_ARM_LEN, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, //

            // Line segment 2: Vertical Arm (From Bottom to Top)
            0.0f, -TEST5_ARM_LEN, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, //
            0.0f, TEST5_ARM_LEN, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f //
        };
		/* clang-format on */

#undef TEST5_ARM_LEN

		target_vertices = line_cross_vertices;
		vertex_data_size = sizeof(line_cross_vertices);
		target_vertex_count = 4;
		break;
	}
	case 6:
		printf("[TEST-6-INPUT] CCW Verification Triangle:\n");
		target_vertices = triangle_vertices_mode6;
		vertex_data_size = sizeof(triangle_vertices_mode6);
		target_vertex_count = 3;
		break;
	case 7:
		printf("[TEST-7-INPUT] CW Verification Triangle:\n");
		target_vertices = triangle_vertices_mode7;
		vertex_data_size = sizeof(triangle_vertices_mode7);
		target_vertex_count = 3;
		break;
	case 8:
		printf("[TEST-8-INPUT] Integer Boundaries Grid Snap Alignment:\n");
		target_vertices = (GLfloat*) triangle_vertices_mode8;
		vertex_data_size = sizeof(triangle_vertices_mode8);
		target_vertex_count = 3;
		break;
	case 9: {
		printf("[TEST-9-INPUT] Fractional +0.5 Subpixel Shift Bias Alignment:\n");

		// 1. Construct the 4x4 translation bias matrix locally
		pgl_mat4 bias_matrix;
		translation_m4(bias_matrix, 0.5f, 0.5f, 0.0f);

		// 2. Cast the raw float pointers directly to pgl_vec4 blocks using the 8-float layout
		// stride index 0 = Vertex 0 Position, index 2 = Vertex 1 Position, index 4 = Vertex 2
		// Position
		pgl_vec4* v0_pos = (pgl_vec4*) &triangle_vertices_mode8[0];
		pgl_vec4* v1_pos = (pgl_vec4*) &triangle_vertices_mode8[8];
		pgl_vec4* v2_pos = (pgl_vec4*) &triangle_vertices_mode8[16];

		// 3. Multi-multiply positional channels cleanly via standard library wrappers, bypassing
		// colors
		*v0_pos = mult_m4_v4(bias_matrix, *v0_pos);
		*v1_pos = mult_m4_v4(bias_matrix, *v1_pos);
		*v2_pos = mult_m4_v4(bias_matrix, *v2_pos);

		target_vertices = (GLfloat*) triangle_vertices_mode8;
		vertex_data_size = sizeof(triangle_vertices_mode8);
		target_vertex_count = 3;
		break;
	}
	case 10: {
		printf("[TEST-10-INPUT] Rendering full scale-invariant cell envelope quad via PortableGL "
			   "Triangles:\n");

// Apply a negative bias matching the pivot offset to shift the local origin back to the cell
// boundaries
#define TEST10_MIN_X ((GLfloat) (1.0f - PIVOT_AXIS_X))
#define TEST10_MAX_X ((GLfloat) (TEXTURE_CELL_W - 1.0f - PIVOT_AXIS_X))
#define TEST10_MIN_Y ((GLfloat) (1.0f - PIVOT_AXIS_Y))
#define TEST10_MAX_Y ((GLfloat) (TEXTURE_CELL_H - 1.0f - PIVOT_AXIS_Y))

		/* clang-format off */
        static GLfloat cell_quad_vertices[] = {
            // Triangle 1 (Top-Left, Top-Right, Bottom-Left)
            TEST10_MIN_X, TEST10_MIN_Y, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
            TEST10_MAX_X, TEST10_MIN_Y, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
            TEST10_MIN_X, TEST10_MAX_Y, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,

            // Triangle 2 (Top-Right, Bottom-Right, Bottom-Left)
            TEST10_MAX_X, TEST10_MIN_Y, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
            TEST10_MAX_X, TEST10_MAX_Y, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
            TEST10_MIN_X, TEST10_MAX_Y, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f
        };
		/* clang-format on */

#undef TEST10_MIN_X
#undef TEST10_MAX_X
#undef TEST10_MIN_Y
#undef TEST10_MAX_Y

		target_vertices = cell_quad_vertices;
		vertex_data_size = sizeof(cell_quad_vertices);
		target_vertex_count = 6;
		break;
	}
	case 11: {
		printf("[TEST-11-INPUT] Rendering full production cell asset quad (59x83) via PioneerGL "
			   "Triangles:\n");

// Establish production asset bounds anchored to padding parameters and neutralized to pivot space
#define TEST11_MIN_X ((GLfloat) (ENVELOPE_PAD_X - PIVOT_AXIS_X))
#define TEST11_MAX_X ((GLfloat) (ENVELOPE_PAD_X + 59.0f - PIVOT_AXIS_X))
#define TEST11_MIN_Y ((GLfloat) (ENVELOPE_PAD_Y - PIVOT_AXIS_Y))
#define TEST11_MAX_Y ((GLfloat) (ENVELOPE_PAD_Y + 83.0f - PIVOT_AXIS_Y))

		/* clang-format off */
        static GLfloat production_quad_vertices[] = {
            // Triangle 1 (Top-Left, Top-Right, Bottom-Left)
            TEST11_MIN_X, TEST11_MIN_Y, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
            TEST11_MAX_X, TEST11_MIN_Y, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
            TEST11_MIN_X, TEST11_MAX_Y, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,

            // Triangle 2 (Top-Right, Bottom-Right, Bottom-Left)
            TEST11_MAX_X, TEST11_MIN_Y, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
            TEST11_MAX_X, TEST11_MAX_Y, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
            TEST11_MIN_X, TEST11_MAX_Y, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f
        };
		/* clang-format on */

#undef TEST11_MIN_X
#undef TEST11_MAX_X
#undef TEST11_MIN_Y
#undef TEST11_MAX_Y

		target_vertices = production_quad_vertices;
		vertex_data_size = sizeof(production_quad_vertices);
		target_vertex_count = 6;
		break;
	}
	default:
		return;
	}

	static glContext native_ctx;
	static uint32_t* intermediate_fb = NULL;
	static uint32_t* heap_fb_container = NULL;
	static int allocated_w = 0;
	static int allocated_h = 0;

	printf("[HOST-BUFFER-TRACE] Sheet Dimensions: %d x %d | Current Allocated: %d x %d\n", sheet_w,
		   sheet_h, allocated_w, allocated_h);

	if (!intermediate_fb || allocated_w != sheet_w || allocated_h != sheet_h) {
		if (intermediate_fb) {
			free(intermediate_fb);
		}
		int total_pixels = sheet_w * sheet_h;
		intermediate_fb = (uint32_t*) calloc(total_pixels, sizeof(uint32_t));
		heap_fb_container = intermediate_fb;

		allocated_w = sheet_w;
		allocated_h = sheet_h;

		printf("[HOST-BUFFER-TRACE] Invoking init_glContext allocation cell address: %p\n",
			   (void*) heap_fb_container);
		if (!init_glContext(&native_ctx, &heap_fb_container, sheet_w, sheet_h)) {
			fprintf(stderr, "[PortableGL Error] Failed to bind state machine layout properties\n");
			return;
		}
	}

	set_glContext(&native_ctx);

	glViewport(0, 0, sheet_w, sheet_h);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	GLuint vbo, vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertex_data_size, target_vertices, GL_STATIC_DRAW);

	// Position Channel (Location 0, Stride = 8 floats)
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), 0);

	// Color Channel (Location 1, Stride = 8 floats, Offset = 4 floats)
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat),
						  (void*) (4 * sizeof(GLfloat)));

	// Initialize the 4-float continuous interpolation array using PortableGL's native macros
	GLenum interpolation_map[4] = { PGL_SMOOTH4 };
	GLuint program_obj = pglCreateProgram(test_vert, test_frag, 4, interpolation_map, GL_FALSE);
	glUseProgram(program_obj);

	/* Host initialization block */

	TestUniforms uniform_data;

	// 1. Structural Scaling Transformation Stage
	scale_m4(uniform_data.scale, scale, scale, 1.0f);

	// 2. Base Asset Focal Pivot Translation Matrix (Pristine 1x Constants)
	translation_m4(uniform_data.pivot, (float) PIVOT_AXIS_X, (float) PIVOT_AXIS_Y, 0.0f);

	// 3. Atlas Master Canvas Cell Allocation Translation Stage (Pristine Integer Bounds)
	translation_m4(uniform_data.translation, (float) cell_x, (float) cell_y, 0.0f);

	// 4. Inverted Top-Down Orthographic Screen Space Projection Stage
	make_orthographic_m4(uniform_data.projection, 0.0f, (float) sheet_w, (float) sheet_h, 0.0f,
						 -1.0f, 1.0f);

	// Commit cleanly to the PortableGL pipeline context
	pglSetUniform(&uniform_data);

	if (TEST_MODE == 5) {
		glLineWidth(scale);
		glDrawArrays(GL_LINES, 0, target_vertex_count);
	} else {
		glDrawArrays(GL_TRIANGLES, 0, target_vertex_count);
	}

	printf("[HOST-DIAG-AFTER-DRAW] Checking rasterization inside intermediate buffer raw "
		   "bounds...\n");

	int filled_pixel_count = 0;
	for (int y = 0; y < sheet_h; ++y) {
		int target_y = y;
		for (int x = 0; x < sheet_w; ++x) {
			int src_offset_idx = y * sheet_w + x;
			int dst_offset_idx = target_y * sheet_w + x;
			uint32_t active_pixel = intermediate_fb[src_offset_idx];
			if (active_pixel != 0) {
				if (filled_pixel_count < 5) {
					printf("[BLIT-TRACE] Raster Source Row y=%d, x=%d maps to Target "
						   "buffer_y=%d\n",
						   y, x, target_y);
				}
				uint8_t red_component = (uint8_t) (active_pixel & 0x000000FF);
				uint8_t blue_component = (uint8_t) ((active_pixel & 0x00FF0000) >> 16);
				if (red_component == 0xFF) {
					buffer[dst_offset_idx] = hand_color;
				} else if (blue_component == 0xFF) {
					buffer[dst_offset_idx] = hand_halo;
				} else {
					buffer[dst_offset_idx] = hand_color;
				}
				filled_pixel_count++;
			}
		}
	}
	printf("[HOST-PALETTE-TRACE] Extraction Cycle Finished. Total Pixels Written = %d\n",
		   filled_pixel_count);

	glUseProgram(0);
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);

	if (scale == 2.0000f) {
		printf("\n[BUFFER-AUDIT-RLE] COMPRESSED FRAMEBUFFER PROFILE (W:128, H:192):\n");

		for (int test_y = 0; test_y < 192; test_y++) {
			// Only allocate processing time if the row isn't totally blank noise
			char row_chars[128];
			int is_empty_row = 1;

			for (int test_x = 0; test_x < 128; test_x++) {
				uint32_t target_offset = test_y * sheet_w + test_x;
				uint8_t pixel_val = buffer[target_offset];

				if (pixel_val == hand_color) {
					row_chars[test_x] = '1';
					is_empty_row = 0;
				} else if (pixel_val == hand_halo) {
					row_chars[test_x] = 'H';
					is_empty_row = 0;
				} else if (pixel_val != 0) {
					row_chars[test_x] = 'X';
					is_empty_row = 0;
				} else {
					row_chars[test_x] = '.';
				}
			}

			// Completely empty lines are compressed to a single token placeholder
			if (is_empty_row) {
				printf("R%03d:MT\n", test_y);
				continue;
			}

			// Perform structural Run-Length Encoding across active pixel data
			printf("R%03d:", test_y);
			int rle_idx = 0;
			while (rle_idx < 128) {
				char current_char = row_chars[rle_idx];
				int run_length = 1;

				while ((rle_idx + run_length < 128)
					   && (row_chars[rle_idx + run_length] == current_char)) {
					run_length++;
				}

				printf("[%d*%c]", run_length, current_char);
				rle_idx += run_length;
			}
			printf("\n");
		}
		printf("[BUFFER-AUDIT] Final Compressed Frame Pass Complete\n\n");
	}
	printf("[DIAG-FRAME-STOP] FrameIdx: %d Execution Matrix End\n\n", frame_idx);
}

/* --- Production Code --- */

typedef struct {
	int dx; // Exact 1x horizontal pixel offset from focal center
	int dy; // Exact 1x vertical pixel offset from focal center
} HandMasterOffset;

static const HandMasterOffset HAND_MASTER_OFFSETS[TOTAL_HAND_PHASES] = {
	{ 0, -39 }, // Phase 0
	{ 4, -39 }, // Phase 1
	{ 8, -38 }, // Phase 2
	{ 12, -37 }, // Phase 3
	{ 15, -35 }, // Phase 4
	{ 19, -32 }, // Phase 5
	{ 22, -30 }, // Phase 6
	{ 24, -27 }, // Phase 7
	{ 26, -24 }, // Phase 8
	{ 27, -20 }, // Phase 9
	{ 28, -16 }, // Phase 10
	{ 29, -13 }, // Phase 11
	{ 29, -10 }, // Phase 12
	{ 29, -6 }, // Phase 13
	{ 29, -3 }, // Phase 14
	{ 29, 0 }, // Phase 15
	{ 29, 3 }, // Phase 16
	{ 29, 6 }, // Phase 17
	{ 29, 10 }, // Phase 18
	{ 29, 13 }, // Phase 19
	{ 29, 17 }, // Phase 20
	{ 29, 21 }, // Phase 21
	{ 28, 25 }, // Phase 22
	{ 26, 29 }, // Phase 23
	{ 23, 32 }, // Phase 24
	{ 20, 35 }, // Phase 25
	{ 17, 38 }, // Phase 26
	{ 13, 40 }, // Phase 27
	{ 9, 42 }, // Phase 28
	{ 5, 43 }, // Phase 29
	{ 0, 43 }, // Phase 30
	{ -5, 43 }, // Phase 31
	{ -9, 42 }, // Phase 32
	{ -13, 40 }, // Phase 33
	{ -17, 38 }, // Phase 34
	{ -20, 35 }, // Phase 35
	{ -23, 32 }, // Phase 36
	{ -26, 29 }, // Phase 37
	{ -28, 25 }, // Phase 38
	{ -29, 21 }, // Phase 39
	{ -29, 17 }, // Phase 40
	{ -29, 13 }, // Phase 41
	{ -29, 10 }, // Phase 42
	{ -29, 6 }, // Phase 43
	{ -29, 3 }, // Phase 44
	{ -29, 0 }, // Phase 45
	{ -29, -3 }, // Phase 46
	{ -29, -6 }, // Phase 47
	{ -29, -10 }, // Phase 48
	{ -29, -13 }, // Phase 49
	{ -28, -16 }, // Phase 50
	{ -27, -20 }, // Phase 51
	{ -26, -24 }, // Phase 52
	{ -24, -26 }, // Phase 53
	{ -22, -30 }, // Phase 54
	{ -19, -33 }, // Phase 55
	{ -15, -35 }, // Phase 56
	{ -12, -37 }, // Phase 57
	{ -8, -38 }, // Phase 58
	{ -4, -39 } // Phase 59
};

typedef struct {
	float bias_x;
	float bias_y;
} PhaseRasterCorrection;

// Core Mapping Generator evaluating the discrete tie-break flips of the rasterizer
void CatClock_GeneratePhaseCorrections(float scale, const HandMasterOffset* master_offsets,
									   PhaseRasterCorrection* out_corrections) {
	float int_part;
	float frac_part = modff(scale, &int_part);
	int is_even_scale = (frac_part == 0.0f) && ((int) int_part % 2 == 0);

	// An inverse-scale micro-step guarantees that the final subpixel shift
	// evaluated inside the shader context remains exactly 0.01f at ALL resolutions.
	float micro_step = 0.01f / scale;

	for (int phase = 0; phase < TOTAL_HAND_PHASES; phase++) {
		HandMasterOffset offset = master_offsets[phase];

		// 1. Establish the base primitive phase assignment profile
		float bx = is_even_scale ? 0.50f : 0.00f;
		float by = is_even_scale ? 0.50f : 0.00f;

		// 2. Adjust for Angular Quadrant Skew Signs
		// We inject the micro-step to push edge equations cleanly past
		// the rasterizer's step-ladder rounding limits.
		if (offset.dx >= 0 && offset.dy < 0) { // Quadrant 1 (Up-Right)
			bx += micro_step;
			by -= micro_step;
		} else if (offset.dx >= 0 && offset.dy >= 0) { // Quadrant 2 (Down-Right)
			bx += micro_step;
			by += micro_step;
		} else if (offset.dx < 0 && offset.dy >= 0) { // Quadrant 3 (Down-Left)
			bx -= micro_step;
			by += micro_step;
		} else { // Quadrant 4 (Up-Left)
			bx -= micro_step;
			by -= micro_step;
		}

		// 3. Compensate for Shared Internal Spine Edge Truncations
		// If an arm coordinate falls directly on a grid symmetry node,
		// we pull it slightly to force consistent top-left coverage.
		if ((offset.dx == 0) && !is_even_scale) {
			bx += 0.25f / scale;
		}
		if ((offset.dy == 0) && !is_even_scale) {
			by += 0.25f / scale;
		}

		out_corrections[phase].bias_x = bx;
		out_corrections[phase].bias_y = by;
	}
}

/**
 * TIER 1: CORE GEOMETRIC FUNCTION (Floating-Point)
 * Computes pure continuous geometric endpoints.
 */
static void CatClock_CalculateHandEndpointsFloat(int phase, float pivot_x, float pivot_y,
												 float scale, float* out_end_x, float* out_end_y) {
	if (phase < 0 || phase >= TOTAL_HAND_PHASES) {
		if (out_end_x)
			*out_end_x = pivot_x;
		if (out_end_y)
			*out_end_y = pivot_y;
		return;
	}

	HandMasterOffset offset = HAND_MASTER_OFFSETS[phase];

	if (out_end_x) {
		*out_end_x = pivot_x + ((float) offset.dx * scale);
	}
	if (out_end_y) {
		*out_end_y = pivot_y + ((float) offset.dy * scale);
	}
}

/**
 * TIER 2: GEOMETRIC WRAPPER (Integer Pass-through)
 * Exposes raw un-biased scale metrics into integer coordinate properties.
 */
__attribute__((unused)) static void
CatClock_CalculateHandEndpoints(int phase, int pivot_x, int pivot_y, float scale, int cell_x,
								int cell_y, int* out_end_x, int* out_end_y) {
	float float_out_x = 0.0f;
	float float_out_y = 0.0f;

	CatClock_CalculateHandEndpointsFloat(phase, (float) pivot_x, (float) pivot_y, scale,
										 &float_out_x, &float_out_y);

	if (out_end_x)
		*out_end_x = (int) roundf(float_out_x);
	if (out_end_y)
		*out_end_y = (int) roundf(float_out_y);

	(void) cell_x;
	(void) cell_y;
}

/**
 * TIER 3: RASTERIZER COMPENSATION WRAPPER
 * Direct master asset extraction layout loop. Performs scale mapping first, then
 * rounds to the nearest grid step using roundf() to preserve geometric symmetry.
 */
static void CatClock_CalculateHandEndpointsRaster(int phase, float pivot_xf, float pivot_yf,
												  float scale, int* out_piv_x, int* out_piv_y,
												  int* out_end_x, int* out_end_y) {
	int final_piv_x = (int) floorf(pivot_xf + 0.5f);
	int final_piv_y = (int) floorf(pivot_yf + 0.5f);

	int safe_phase = phase % TOTAL_HAND_PHASES;
	if (safe_phase < 0)
		safe_phase += TOTAL_HAND_PHASES;
	HandMasterOffset offset = HAND_MASTER_OFFSETS[safe_phase];

	int scaled_dx = (int) roundf((float) offset.dx * scale);
	int scaled_dy = (int) roundf((float) offset.dy * scale);

	if (out_piv_x)
		*out_piv_x = final_piv_x;
	if (out_piv_y)
		*out_piv_y = final_piv_y;
	if (out_end_x)
		*out_end_x = final_piv_x + scaled_dx;
	if (out_end_y)
		*out_end_y = final_piv_y + scaled_dy;
}

static void DrawProductionFrame(uint8_t* buffer, int sheet_w, int sheet_h, int frame_idx,
								float scale, float px_f, float py_f, int px, int py,
								uint8_t hand_color, uint8_t hand_halo) {
	(void) px;
	(void) py;
	(void) hand_halo;

	int target_piv_x, target_piv_y, target_end_x, target_end_y;
	int phase = frame_idx % TOTAL_HAND_PHASES;

	CatClock_CalculateHandEndpointsRaster(phase, px_f, py_f, scale, &target_piv_x, &target_piv_y,
										  &target_end_x, &target_end_y);

	int int_scale_factor = (int) floorf(scale);
	if (int_scale_factor < 1) {
		int_scale_factor = 1;
	}
/*
	DrawLineLikeMesa(buffer, sheet_w, sheet_h, (float) target_piv_x, (float) target_piv_y,
					 (float) target_end_x, (float) target_end_y, (float) int_scale_factor,
					 hand_color);
*/
}
