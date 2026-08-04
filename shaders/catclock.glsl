#version 450
// ==============================================================================
// 1. UNIVERSAL PASS-THROUGH VERTEX PIPELINE
// ==============================================================================
#pragma sokol @vs vs_shared
in vec4 position;
in vec2 texcoord0;
out vec2 frag_uv;

void main() {
    gl_Position = position;
    frag_uv = texcoord0;
}
#pragma sokol @end

// ==============================================================================
// 2. STATIC ASSET OFFSCREEN BAKE FRAGMENT SHADER
// ==============================================================================
#pragma sokol @fs fs_bake
layout(binding = 0) uniform cb_params_bake {
    int hour_frame_idx;
    int min_frame_idx;
    int sec_frame_idx;
    int pendulum_frame_idx;
    int generation_mode_flag;
    int tail_rows;
    vec4 cat_color;
    vec4 tie_color;
    vec4 pupil_color;
    vec4 sclera_color;
    vec4 detail_color;
    vec4 outline_color;
};

layout(binding = 0) uniform texture2D texture_sheet;
layout(binding = 0) uniform sampler main_sampler;

in vec2 frag_uv;
out vec4 frag_color;

bool sample_any_geometry(vec2 uv) {
    vec4 m = texture(sampler2D(texture_sheet, main_sampler), uv);
    return (m.r > 0.5 || m.g > 0.5 || m.b > 0.5);
}

void main() {
    vec4 mask = texture(sampler2D(texture_sheet, main_sampler), frag_uv);
    bool is_solid_body = (mask.r > 0.5);
    bool is_tie = (mask.g > 0.5);
    bool is_detail = (mask.b > 0.5);
    bool is_sclera = (mask.a > 0.5);

    if (generation_mode_flag == 1) {
        bool is_inside_tail_width = (frag_uv.x >= (31.125 / 128.0) && frag_uv.x <= (118.875 / 128.0));
        if (is_inside_tail_width && frag_uv.y >= (202.005 / 288.0)) {
            frag_color = vec4(0.0);
            return;
        }

        vec2 dx = dFdx(frag_uv);
        vec2 dy = dFdy(frag_uv);

        bool outline_hit = false;
        outline_hit = outline_hit || sample_any_geometry(frag_uv - dx + dy);
        outline_hit = outline_hit || sample_any_geometry(frag_uv + dx);
        outline_hit = outline_hit || sample_any_geometry(frag_uv + dx + dy);
        outline_hit = outline_hit || sample_any_geometry(frag_uv - dx);
        outline_hit = outline_hit || sample_any_geometry(frag_uv + dx);
        outline_hit = outline_hit || sample_any_geometry(frag_uv - dx - dy);
        outline_hit = outline_hit || sample_any_geometry(frag_uv - dy);
        outline_hit = outline_hit || sample_any_geometry(frag_uv + dx - dy);

        if (is_solid_body && !is_detail && !is_tie) {
            frag_color = vec4(cat_color);
        } else if (outline_hit && !is_solid_body && !is_detail && !is_tie) {
            frag_color = vec4(outline_color);
        } else {
            frag_color = vec4(0.0);
        }
        return;
    }

    if (generation_mode_flag == 2) {
        if (is_sclera) {
            frag_color = vec4(sclera_color);
        } else if (is_detail) {
            frag_color = vec4(detail_color);
        } else if (is_tie) {
            frag_color = vec4(tie_color);
        } else if (is_solid_body) {
            frag_color = vec4(cat_color);
        } else {
            frag_color = vec4(0.0);
        }
        return;
    }

    frag_color = vec4(0.0);
}
#pragma sokol @end

// ============================================================================
// 3. SEPARATION PIPELINE: SEQUENTIAL MASK-CLIPPED BRIDGE TAIL SHADER
// ============================================================================
#pragma sokol @fs fs_tail
layout(binding = 0) uniform cb_tail_params {
    int tail_frame;
    int tail_rows;
    int tail_cols;
    int use_decorations_flag;
    vec4 cat_color;
    vec4 outline_color;
};
layout(binding = 5) uniform texture2D tail_sheet;
layout(binding = 0) uniform sampler main_sampler;

in vec2 frag_uv;
out vec4 frag_color;

vec2 CalculateAtlasUvClamped(vec2 local_box_uv, int frame_idx, float target_rows, float target_cols) {
    // Restrict local UV bounds slightly away from the absolute cell edges
    vec2 clean_uv = clamp(local_box_uv, vec2(0.0001), vec2(0.9999));

    // Find integer column and row positions first
    float col = floor(mod(float(frame_idx) + 0.05, target_cols));
    float row = floor((float(frame_idx) + 0.05) / target_cols);

    // Line up and scale coordinates concurrently to prevent fractional rounding drift
    return (vec2(col, row) + clean_uv) / vec2(target_cols, target_rows);
}

void main() {
    float CANVAS_W = 128.0;
    float CANVAS_H = 288.0;
    vec2 overlay_uv = frag_uv;

    float TAIL_BOX_X0 = 27.0;
    float TAIL_BOX_X1 = 123.0;
    float TAIL_BOX_Y0 = 192.0;
    float TAIL_BOX_Y1 = 288.0;

    float total_canvas_pixel_height = 1.0 / max(abs(dFdy(overlay_uv.y)), 0.00001);
    float target_baseline_y = 202.00001 / CANVAS_H;
    float pixel_delta_y = (overlay_uv.y - target_baseline_y) * total_canvas_pixel_height;
    float screen_target_y = gl_FragCoord.y - pixel_delta_y;
    float snapped_screen_y = round(screen_target_y);

    bool is_on_procedural_line = (abs(gl_FragCoord.y - snapped_screen_y) <= 0.5 &&
            overlay_uv.x >= (31.0 / CANVAS_W) &&
            overlay_uv.x <= (119.0 / CANVAS_W));

    vec2 tail_box_uv;
    tail_box_uv.x = (overlay_uv.x - (TAIL_BOX_X0 / CANVAS_W)) / ((TAIL_BOX_X1 - TAIL_BOX_X0) / CANVAS_W);
    tail_box_uv.y = (overlay_uv.y - (TAIL_BOX_Y0 / CANVAS_H)) / ((TAIL_BOX_Y1 - TAIL_BOX_Y0) / CANVAS_H);

    ivec2 texture_dimensions = textureSize(sampler2D(tail_sheet, main_sampler), 0);
    float rows = float(tail_rows);
    float cols = float(tail_cols);
    float center_sample = texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv, tail_frame, rows, cols)).x;
    bool is_solid_body = (center_sample > 0.0);

    bool valid_procedural_rim = is_on_procedural_line && !is_solid_body;

    if (overlay_uv.y < (202.005 / CANVAS_H) && !valid_procedural_rim) {
        discard;
    }

    vec2 pixel_step = 1.0 / vec2(texture_dimensions);
    vec2 step_offset = pixel_step * vec2(cols, rows);

    bool halo_hit = false;
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(-step_offset.x, step_offset.y), tail_frame, rows, cols)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(0.0, step_offset.y), tail_frame, rows, cols)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(step_offset.x, step_offset.y), tail_frame, rows, cols)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(-step_offset.x, 0.0), tail_frame, rows, cols)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(step_offset.x, 0.0), tail_frame, rows, cols)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(-step_offset.x, -step_offset.y), tail_frame, rows, cols)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(0.0, -step_offset.y), tail_frame, rows, cols)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(step_offset.x, -step_offset.y), tail_frame, rows, cols)).x > 0.0);

    vec4 output_color = vec4(0.0);

    if (is_solid_body) {
        output_color = cat_color;
    } else if (halo_hit || valid_procedural_rim) {
        output_color = outline_color;
    }

    if (output_color.a < 0.01) {
        discard;
    }

    if (is_solid_body) {
        float stacked_alpha = cat_color.a + cat_color.a * (1.0 - cat_color.a);
        if (stacked_alpha > 0.0) {
            output_color.rgb = (cat_color.rgb * cat_color.a + cat_color.rgb * cat_color.a * (1.0 - cat_color.a)) / stacked_alpha;
        }
        output_color.a = stacked_alpha;
    }

    frag_color = output_color;
}
#pragma sokol @end

// ==============================================================================
// 4. CLOCK HANDS TINTING FRAGMENT SHADER
// ==============================================================================
#pragma sokol @fs fs_hands
layout(binding = 1) uniform cb_hands_params {
    int hour_frame;
    int min_frame;
    int sec_frame;
    int use_decorations_flag;
    int hands_cols;
    int hands_rows;
    vec4 hour_color;
    vec4 minute_color;
    vec4 seconds_color;
};
layout(binding = 1) uniform texture2D hours_hand_sheet;
layout(binding = 2) uniform texture2D mins_hand_sheet;
layout(binding = 3) uniform texture2D seconds_hand_sheet;
layout(binding = 0) uniform sampler main_sampler;

in vec2 frag_uv;
out vec4 frag_color;

vec2 CalculateAtlasUvHands(vec2 local_box_uv, int frame_idx) {
    // Read both dimensions purely out of your hardware-aware pipeline uniforms
    float cols = float(hands_cols);
    float rows = float(hands_rows);

    vec2 segment_cell = vec2(1.0 / cols, 1.0 / rows);
    vec2 cell_offset = vec2(mod(float(frame_idx), cols), floor(float(frame_idx) / cols)) * segment_cell;
    return cell_offset + (local_box_uv * segment_cell);
}

void main() {
    float CANVAS_W = 128.0;

    // UNIFIED: Direct 1:1 mapping from our host viewport grid
    vec2 overlay_uv = frag_uv;

    // Check if within the absolute textel box boundary of the clock face
    bool is_clock_box = (overlay_uv.x >= (42.0 / CANVAS_W) && overlay_uv.x <= (106.0 / CANVAS_W) &&
            overlay_uv.y >= (95.0 / 288.0) && overlay_uv.y <= (191.0 / 288.0));
    if (!is_clock_box) {
        frag_color = vec4(0.0);
        return;
    }

    // Remap coordinates cleanly into hand cell local tracking space
    vec2 hand_box_uv = vec2((overlay_uv.x - (42.0 / CANVAS_W)) / (64.0 / CANVAS_W),
            (overlay_uv.y - (95.0 / 288.0)) / (96.0 / 288.0));

    bool hour_hit = texture(sampler2D(hours_hand_sheet, main_sampler), CalculateAtlasUvHands(hand_box_uv, hour_frame)).x > 0.001;
    bool min_hit = texture(sampler2D(mins_hand_sheet, main_sampler), CalculateAtlasUvHands(hand_box_uv, min_frame)).x > 0.001;
    bool sec_hit = texture(sampler2D(seconds_hand_sheet, main_sampler), CalculateAtlasUvHands(hand_box_uv, sec_frame)).x > 0.001;

    vec4 mixed_pixel = vec4(0.0);

    // Layer 1: Hour hand base
    if (hour_hit) {
        mixed_pixel = hour_color;
    }

    // Layer 2: Minute hand blending
    if (min_hit) {
        float out_a = minute_color.a + mixed_pixel.a * (1.0 - minute_color.a);
        if (out_a > 0.0) {
            mixed_pixel.rgb = (minute_color.rgb * minute_color.a + mixed_pixel.rgb * mixed_pixel.a * (1.0 - minute_color.a)) / out_a;
        }
        mixed_pixel.a = out_a;
    }

    // Layer 3: Seconds hand blending
    if (sec_hit) {
        float out_a = seconds_color.a + mixed_pixel.a * (1.0 - seconds_color.a);
        if (out_a > 0.0) {
            mixed_pixel.rgb = (seconds_color.rgb * seconds_color.a + mixed_pixel.rgb * mixed_pixel.a * (1.0 - seconds_color.a)) / out_a;
        }
        mixed_pixel.a = out_a;
    }

    if (mixed_pixel.a < 0.01) {
        frag_color = vec4(0.0);
        return;
    }

    frag_color = mixed_pixel;
}
#pragma sokol @end

// ==============================================================================
// 5. PUPIL OVERLAY FRAGMENT SHADER
// ==============================================================================
#pragma sokol @fs fs_pupils
layout(binding = 2) uniform cb_pupil_params {
    int pupil_frame;
    int eyes_rows;
    int eyes_cols;
    int use_decorations_flag;
    vec4 pupil_color;
};

layout(binding = 4) uniform texture2D eyes_sheet;
layout(binding = 0) uniform sampler main_sampler;

in vec2 frag_uv;
out vec4 frag_color;

vec2 CalculateAtlasUvPupils(vec2 local_box_uv, int frame_idx, float target_rows, float target_cols) {
    vec2 clean_uv = clamp(local_box_uv, vec2(0.0001), vec2(0.9999));

    float col = floor(mod(float(frame_idx) + 0.05, target_cols));
    float row = floor((float(frame_idx) + 0.05) / target_cols);

    return (vec2(col, row) + clean_uv) / vec2(target_cols, target_rows);
}

void main() {
    float CANVAS_W = 128.0;
    vec2 overlay_uv = frag_uv;

    bool is_eye_box = (overlay_uv.x >= (44.0 / CANVAS_W) && overlay_uv.x <= (108.0 / CANVAS_W) &&
            overlay_uv.y >= (17.0 / 288.0) && overlay_uv.y <= (49.0 / 288.0));

    if (!is_eye_box) {
        frag_color = vec4(0.0);
        return;
    }

    vec2 eyes_box_uv;
    eyes_box_uv.x = (overlay_uv.x - (44.0 / CANVAS_W)) / (64.0 / CANVAS_W);
    eyes_box_uv.y = (overlay_uv.y - (17.0 / 288.0)) / (32.0 / 288.0);

    float rows = float(eyes_rows);
    float cols = float(eyes_cols);
    vec4 pupil_sample = texture(sampler2D(eyes_sheet, main_sampler), CalculateAtlasUvPupils(eyes_box_uv, pupil_frame, rows, cols));

    if (int(round(pupil_sample.x * 255.0)) == 3) {
        frag_color = vec4(pupil_color);
    } else {
        frag_color = vec4(0.0);
    }
}
#pragma sokol @end

// ==============================================================================
// 6. FLAT OFFSCREEN LAYER COMPOSITOR MIXER
// ==============================================================================
#pragma sokol @fs fs_composite

layout(binding = 0) uniform sampler main_sampler;
layout(binding = 6) uniform texture2D rt_backdrop_tex;
layout(binding = 7) uniform texture2D rt_foreground_tex;

in vec2 frag_uv;
out vec4 frag_color;

void main() {
    // SOKOL_GLSL is natively recognized by sokol-shdc when building the GLSL source blocks.
    // This isolates the vertical texture correction exclusively to the OpenGL translation layer.
    #if defined(SOKOL_GLCORE)
    vec2 corrected_body_uv = vec2(frag_uv.x, 1.0 - frag_uv.y);
    #else
    vec2 corrected_body_uv = frag_uv;
    #endif

    vec4 backdrop = texture(sampler2D(rt_backdrop_tex, main_sampler), corrected_body_uv);
    vec4 foreground = texture(sampler2D(rt_foreground_tex, main_sampler), corrected_body_uv);

    vec4 composition = mix(backdrop, foreground, foreground.a);
    if (composition.a < 0.01) {
        discard;
    }

    frag_color = composition;
}

#pragma sokol @end

// ==============================================================================
// PROGRAM PIPELINES LINKAGE
// ==============================================================================
#pragma sokol @program catclock_bake vs_shared fs_bake
#pragma sokol @program catclock_tail vs_shared fs_tail
#pragma sokol @program catclock_hands vs_shared fs_hands
#pragma sokol @program catclock_pupils vs_shared fs_pupils
#pragma sokol @program catclock_composite vs_shared fs_composite
