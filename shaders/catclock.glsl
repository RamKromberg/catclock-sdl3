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
    int tail_pupils_rows;

    // MATCHED HOSTRUNTIME PACKING ORDER
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

void main() {
    // Read the master unpacked canvas channels out of texture memory
    vec4 mask = texture(sampler2D(texture_sheet, main_sampler), frag_uv);
    bool is_solid_body = (mask.r > 0.5);
    bool is_tie = (mask.g > 0.5);
    bool is_detail = (mask.b > 0.5);
    bool is_sclera = (mask.a > 0.5);

    // --- MODE 1: BAKE BACKDROP PASS ---
    if (generation_mode_flag == 1) {
        vec2 dx = dFdx(frag_uv);
        vec2 dy = dFdy(frag_uv);
        bool outline_hit = false;
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, main_sampler), frag_uv - dx + dy).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, main_sampler), frag_uv + dy).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, main_sampler), frag_uv + dx + dy).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, main_sampler), frag_uv - dx).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, main_sampler), frag_uv + dx).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, main_sampler), frag_uv - dx - dy).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, main_sampler), frag_uv - dy).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, main_sampler), frag_uv + dx - dy).r > 0.5);

        if (is_solid_body) {
            frag_color = vec4(cat_color.xyz, 1.0);
        } else if (outline_hit) {
            frag_color = vec4(outline_color.xyz, 1.0);
        } else {
            frag_color = vec4(0.0);
        }
        return;
    }

    // --- MODE 2: BAKE FOREGROUND PASS ---
    if (generation_mode_flag == 2) {
        if (is_sclera) {
            frag_color = vec4(sclera_color.xyz, 1.0);
        } else if (is_detail) {
            frag_color = vec4(detail_color.xyz, 1.0);
        } else if (is_tie) {
            frag_color = vec4(tie_color.xyz, 1.0);
        } else if (is_solid_body) {
            frag_color = vec4(cat_color.xyz, 1.0);
        } else {
            frag_color = vec4(0.0);
        }
        return;
    }

    frag_color = vec4(0.0);
}
#pragma sokol @end

// ==============================================================================
// 3. REFACACTOR PIPELINE: DETACHED 1PX ABSOLUTE UN-SCALED PENDULUM TAIL SHADER
// ==============================================================================
#pragma sokol @fs fs_tail
layout(binding = 0) uniform cb_tail_params {
    int tail_frame;
    int tail_pupils_rows;
    vec4 cat_color;
    vec4 outline_color;
};
layout(binding = 5) uniform texture2D tail_sheet;
layout(binding = 0) uniform sampler main_sampler;

in vec2 frag_uv;
out vec4 frag_color;

vec2 CalculateAtlasUvClamped(vec2 local_box_uv, int frame_idx, float target_rows) {
    float cols = 10.0;
    vec2 clean_uv = clamp(local_box_uv, vec2(0.001), vec2(0.999));
    // Scaled properly using the current dynamic rows parameter
    vec2 segment_cell = vec2(1.0 / cols, 1.0 / target_rows);
    vec2 cell_offset = vec2(mod(float(frame_idx), cols), floor(float(frame_idx) / cols)) * segment_cell;
    return cell_offset + (clean_uv * segment_cell);
}

void main() {
    vec2 overlay_uv = vec2(((frag_uv.x * 103.0) + 23.0) / 128.0, frag_uv.y);
    bool is_tail_box = (overlay_uv.x >= (27.0 / 128.0) && overlay_uv.x <= (123.0 / 128.0) &&
            overlay_uv.y >= (192.0 / 288.0) && overlay_uv.y <= (288.0 / 288.0));
    if (!is_tail_box) {
        frag_color = vec4(0.0);
        return;
    }

    vec2 tail_box_uv;
    tail_box_uv.x = (overlay_uv.x - (27.0 / 128.0)) / (96.0 / 128.0);
    tail_box_uv.y = (overlay_uv.y - (192.0 / 288.0)) / (96.0 / 288.0);

    float rows = float(tail_pupils_rows);
    float center_sample = texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv, tail_frame, rows)).x;
    bool is_solid_body = (center_sample > 0.0);

    ivec2 texture_dimensions = textureSize(sampler2D(tail_sheet, main_sampler), 0);
    vec2 pixel_step = 1.0 / vec2(texture_dimensions);
    // Stepping parameters adjust relative to the active runtime layout matrix height
    vec2 step_offset = pixel_step * vec2(10.0, rows);

    bool halo_hit = false;
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(-step_offset.x, step_offset.y), tail_frame, rows)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(0.0, step_offset.y), tail_frame, rows)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(step_offset.x, step_offset.y), tail_frame, rows)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(-step_offset.x, 0.0), tail_frame, rows)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(step_offset.x, 0.0), tail_frame, rows)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(-step_offset.x, -step_offset.y), tail_frame, rows)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(0.0, -step_offset.y), tail_frame, rows)).x > 0.0);
    halo_hit = halo_hit || (texture(sampler2D(tail_sheet, main_sampler), CalculateAtlasUvClamped(tail_box_uv + vec2(step_offset.x, -step_offset.y), tail_frame, rows)).x > 0.0);

    if (is_solid_body) {
        frag_color = vec4(cat_color.xyz, 1.0);
    } else if (halo_hit) {
        frag_color = vec4(outline_color.xyz, 1.0);
    } else {
        frag_color = vec4(0.0);
    }
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
    vec4 hour_color;
    vec4 minute_color;
    vec4 second_color;
};
layout(binding = 1) uniform texture2D hours_hand_sheet;
layout(binding = 2) uniform texture2D mins_hand_sheet;
layout(binding = 3) uniform texture2D seconds_hand_sheet;
layout(binding = 0) uniform sampler main_sampler;

in vec2 frag_uv;
out vec4 frag_color;

vec2 CalculateAtlasUvHands(vec2 local_box_uv, int frame_idx, float target_rows) {
    float cols = 10.0;
    vec2 segment_cell = vec2(1.0 / cols, 1.0 / target_rows);
    vec2 cell_offset = vec2(mod(float(frame_idx), cols), floor(float(frame_idx) / cols)) * segment_cell;
    return cell_offset + (local_box_uv * segment_cell);
}

void main() {
    vec2 overlay_uv = vec2(((frag_uv.x * 103.0) + 23.0) / 128.0, frag_uv.y);
    bool is_clock_box = (overlay_uv.x >= (42.0 / 128.0) && overlay_uv.x <= (106.0 / 128.0) &&
            overlay_uv.y >= (95.0 / 288.0) && overlay_uv.y <= (191.0 / 288.0));
    if (!is_clock_box) {
        frag_color = vec4(0.0);
        return;
    }

    vec2 hand_box_uv = vec2((overlay_uv.x - (42.0 / 128.0)) / (64.0 / 128.0), (overlay_uv.y - (95.0 / 288.0)) / (96.0 / 288.0));
    bool hour_hit = texture(sampler2D(hours_hand_sheet, main_sampler), CalculateAtlasUvHands(hand_box_uv, hour_frame, 6.0)).x > 0.001;
    bool min_hit = texture(sampler2D(mins_hand_sheet, main_sampler), CalculateAtlasUvHands(hand_box_uv, min_frame, 6.0)).x > 0.001;
    bool sec_hit = texture(sampler2D(seconds_hand_sheet, main_sampler), CalculateAtlasUvHands(hand_box_uv, sec_frame, 6.0)).x > 0.001;
    vec4 mixed_pixel = vec4(0.0);
    if (hour_hit) mixed_pixel = vec4(hour_color.xyz, 1.0);
    if (min_hit) mixed_pixel = vec4(minute_color.xyz, 1.0);
    if (sec_hit) mixed_pixel = vec4(second_color.xyz, 1.0);

    if (mixed_pixel.a < 0.5) {
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
    int tail_pupils_rows;
    vec4 pupil_color;
};

layout(binding = 4) uniform texture2D eyes_sheet;
layout(binding = 0) uniform sampler main_sampler;

in vec2 frag_uv;
out vec4 frag_color;

vec2 CalculateAtlasUvPupils(vec2 local_box_uv, int frame_idx, float target_rows) {
    float cols = 10.0;
    vec2 segment_cell = vec2(1.0 / cols, 1.0 / target_rows);
    vec2 cell_offset = vec2(mod(float(frame_idx), cols), floor(float(frame_idx) / cols)) * segment_cell;
    return cell_offset + (local_box_uv * segment_cell);
}

void main() {
    // Normalized screen coordinate mapping down to legacy box bounds matrix
    vec2 overlay_uv = vec2(((frag_uv.x * 103.0) + 23.0) / 128.0, frag_uv.y);
    bool is_eye_box = (overlay_uv.x >= (44.0 / 128.0) && overlay_uv.x <= (108.0 / 128.0) &&
            overlay_uv.y >= (17.0 / 288.0) && overlay_uv.y <= (49.0 / 288.0));

    if (!is_eye_box) {
        frag_color = vec4(0.0);
        return;
    }

    // Remap local UV frames to sample directly from the generated dynamic atlas tracking slots
    vec2 eyes_box_uv;
    eyes_box_uv.x = (overlay_uv.x - (44.0 / 128.0)) / (64.0 / 128.0);
    eyes_box_uv.y = (overlay_uv.y - (17.0 / 288.0)) / (32.0 / 288.0);

    float rows = float(tail_pupils_rows);
    vec4 pupil_sample = texture(sampler2D(eyes_sheet, main_sampler), CalculateAtlasUvPupils(eyes_box_uv, pupil_frame, rows));

    // Purely evaluate and paint the pupil vector token channel match (Token ID 3)
    if (int(round(pupil_sample.x * 255.0)) == 3) {
        frag_color = vec4(pupil_color.xyz, 1.0);
    } else {
        // Output clean empty space alpha so the static baked sclera backdrop underneath is visible
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
