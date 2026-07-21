// ==============================================================================
// 1. VERTEX SHADER FOR THE OFFSCREEN BAKE PIPELINE
// ==============================================================================
#pragma sokol @vs vs_bake
in vec4 position;
in vec2 texcoord0;
out vec2 frag_uv;

void main() {
    // Invert clip space Y to ensure the offscreen pass writes the texture right-side up
    gl_Position = vec4(position.x, -position.y, position.z, position.w);
    frag_uv = texcoord0;
}
#pragma sokol @end

// ==============================================================================
// 2. VERTEX SHADER FOR THE MAIN RUNTIME DISPLAY LOOP
// ==============================================================================
#pragma sokol @vs vs_composite
in vec4 position;
in vec2 texcoord0;
out vec2 frag_uv;

void main() {
    gl_Position = position;
    frag_uv = texcoord0;
}
#pragma sokol @end

// ==============================================================================
// 3. OFFSCREEN BAKE FRAGMENT SHADER (Binds core raw asset sheets)
// ==============================================================================
#pragma sokol @fs fs_bake
layout(binding = 0) uniform cb_params_bake {
    int hour_frame_idx;
    int min_frame_idx;
    int sec_frame_idx;
    int pendulum_frame_idx;
    int generation_mode_flag; // 1 = Bake Backdrop, 2 = Bake Foreground
    int tail_pupils_rows;
    vec4 cat_color;
    vec4 tie_color;
    vec4 pupil_color;
    vec4 sclera_color;
    vec4 detail_color;
    vec4 halo_color;
};

layout(binding = 0) uniform texture2D texture_sheet;
layout(binding = 4) uniform texture2D eyes_sheet;
layout(binding = 0) uniform sampler sampler_state;

in vec2 frag_uv;
out vec4 frag_color;

vec2 CalculateAtlasUv(vec2 local_box_uv, int frame_idx, float target_rows) {
    float cols = 10.0;
    vec2 segment_cell = vec2(1.0 / cols, 1.0 / target_rows);

    vec2 cell_offset = vec2(mod(float(frame_idx), cols), floor(float(frame_idx) / cols)) * segment_cell;
    return cell_offset + (local_box_uv * segment_cell);
}

void main() {
    vec4 mask = texture(sampler2D(texture_sheet, sampler_state), frag_uv);
    bool is_solid_body = (mask.r > 0.5);
    bool is_tie = (mask.g > 0.5);
    bool is_detail = (mask.b > 0.5);

    bool is_eye_box = (frag_uv.x >= (44.0 / 128.0) && frag_uv.x <= (108.0 / 128.0) &&
            frag_uv.y >= (17.0 / 290.0) && frag_uv.y <= (49.0 / 290.0));
    bool is_sclera = false;
    if (is_eye_box) {
        vec2 eyes_local_uv;
        eyes_local_uv.x = (frag_uv.x - (44.0 / 128.0)) / (64.0 / 128.0);
        eyes_local_uv.y = (frag_uv.y - (17.0 / 290.0)) / (32.0 / 290.0);
        vec4 eyes_raw = texture(sampler2D(eyes_sheet, sampler_state), CalculateAtlasUv(eyes_local_uv, pendulum_frame_idx, float(tail_pupils_rows)));
        is_sclera = (int(round(eyes_raw.x * 255.0)) != 3 && mask.a > 0.5);
    }

    // --- MODE 1: BAKE BACKDROP PASS ---
    if (generation_mode_flag == 1) {
        vec2 dx = dFdx(frag_uv);
        vec2 dy = dFdy(frag_uv);

        bool outline_hit = false;
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, sampler_state), frag_uv - dx + dy).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, sampler_state), frag_uv + dy).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, sampler_state), frag_uv + dx + dy).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, sampler_state), frag_uv - dx).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, sampler_state), frag_uv + dx).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, sampler_state), frag_uv - dx - dy).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, sampler_state), frag_uv - dy).r > 0.5);
        outline_hit = outline_hit || (texture(sampler2D(texture_sheet, sampler_state), frag_uv + dx - dy).r > 0.5);

        if (is_solid_body) {
            frag_color = vec4(cat_color.xyz, 1.0);
        } else if (outline_hit) {
            frag_color = vec4(halo_color.xyz, 1.0);
        } else {
            frag_color = vec4(0.0);
        }
        return;
    }

    // --- MODE 2: BAKE FOREGROUND PASS ---
    if (generation_mode_flag == 2) {
        if (is_detail) {
            frag_color = vec4(detail_color.xyz, 1.0);
        } else if (is_tie) {
            frag_color = vec4(tie_color.xyz, 1.0);
        } else if (is_sclera) {
            frag_color = vec4(sclera_color.xyz, 1.0);
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
// 4. MAIN RUNTIME COMPOSITOR FRAGMENT SHADER
// ==============================================================================
#pragma sokol @fs fs_composite
layout(binding = 0) uniform cb_params_composite {
    int hour_frame_idx;
    int min_frame_idx;
    int sec_frame_idx;
    int pendulum_frame_idx;
    int tail_pupils_rows;
    vec4 cat_color;
    vec4 pupil_color;
    vec4 halo_color;
    vec4 hour_color;
    vec4 minute_color;
    vec4 second_color;
};

layout(binding = 1) uniform texture2D hours_hand_sheet;
layout(binding = 2) uniform texture2D mins_hand_sheet;
layout(binding = 3) uniform texture2D seconds_hand_sheet;
layout(binding = 4) uniform texture2D eyes_sheet;
layout(binding = 5) uniform texture2D tail_sheet;
layout(binding = 6) uniform texture2D rt_backdrop_tex;
layout(binding = 7) uniform texture2D rt_foreground_tex;
layout(binding = 0) uniform sampler sampler_state;

in vec2 frag_uv;
out vec4 frag_color;

vec2 CalculateAtlasUv(vec2 local_box_uv, int frame_idx, float target_rows) {
    float cols = 10.0;
    vec2 segment_cell = vec2(1.0 / cols, 1.0 / target_rows);

    vec2 cell_offset = vec2(mod(float(frame_idx), cols), floor(float(frame_idx) / cols)) * segment_cell;
    return cell_offset + (local_box_uv * segment_cell);
}

void main() {
    // 1:1 native offscreen target texture sampling (Clean)
    vec4 final_pixel = texture(sampler2D(rt_backdrop_tex, sampler_state), frag_uv);

    // FIX: Map screen-space width back out to uncompressed layout pixels,
    // restoring the exact 23-pixel horizontal offset.
    vec2 overlay_uv = vec2(((frag_uv.x * 103.0) + 23.0) / 128.0, frag_uv.y);

    // Leave all downstream box checks exactly as they are...
    bool is_tail_box = (overlay_uv.x >= (27.0 / 128.0) && overlay_uv.x <= (123.0 / 128.0) &&
            overlay_uv.y >= (192.0 / 290.0) && overlay_uv.y <= (288.0 / 290.0));
    if (is_tail_box) {
        vec2 tail_box_uv;
        tail_box_uv.x = (overlay_uv.x - (27.0 / 128.0)) / (96.0 / 128.0);
        tail_box_uv.y = (overlay_uv.y - (192.0 / 290.0)) / (96.0 / 290.0);

        vec4 tail_raw = texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv, pendulum_frame_idx, float(tail_pupils_rows)));
        int tail_token = int(round(tail_raw.x * 255.0));

        vec2 t_dx = dFdx(tail_box_uv);
        vec2 t_dy = dFdy(tail_box_uv);
        bool tail_halo_hit = false;
        tail_halo_hit = tail_halo_hit || (int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv - t_dx + t_dy, pendulum_frame_idx, float(tail_pupils_rows))).x * 255.0)) == 1);
        tail_halo_hit = tail_halo_hit || (int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv + t_dy, pendulum_frame_idx, float(tail_pupils_rows))).x * 255.0)) == 1);
        tail_halo_hit = tail_halo_hit || (int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv + t_dx + t_dy, pendulum_frame_idx, float(tail_pupils_rows))).x * 255.0)) == 1);
        tail_halo_hit = tail_halo_hit || (int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv - t_dx, pendulum_frame_idx, float(tail_pupils_rows))).x * 255.0)) == 1);
        tail_halo_hit = tail_halo_hit || (int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv + t_dx, pendulum_frame_idx, float(tail_pupils_rows))).x * 255.0)) == 1);
        tail_halo_hit = tail_halo_hit || (int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv - t_dx - t_dy, pendulum_frame_idx, float(tail_pupils_rows))).x * 255.0)) == 1);
        tail_halo_hit = tail_halo_hit || (int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv - t_dy, pendulum_frame_idx, float(tail_pupils_rows))).x * 255.0)) == 1);
        tail_halo_hit = tail_halo_hit || (int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv + t_dx - t_dy, pendulum_frame_idx, float(tail_pupils_rows))).x * 255.0)) == 1);

        if (tail_token == 1 || tail_token == 8) {
            final_pixel = vec4(cat_color.xyz, 1.0);
        } else if (tail_halo_hit) {
            final_pixel = vec4(halo_color.xyz, 1.0);
        }
    }

    vec4 foreground = texture(sampler2D(rt_foreground_tex, sampler_state), frag_uv);
    if (foreground.a > 0.5) {
        final_pixel = foreground;
    }

    // Update the eye box check to evaluate using our shifted coordinate space
    bool is_eye_box = (overlay_uv.x >= (44.0 / 128.0) && overlay_uv.x <= (108.0 / 128.0) &&
            overlay_uv.y >= (17.0 / 290.0) && overlay_uv.y <= (49.0 / 290.0));
    if (is_eye_box) {
        vec2 eyes_box_uv;
        eyes_box_uv.x = (overlay_uv.x - (44.0 / 128.0)) / (64.0 / 128.0);
        eyes_box_uv.y = (overlay_uv.y - (17.0 / 290.0)) / (32.0 / 290.0);
        vec4 eyes_raw = texture(sampler2D(eyes_sheet, sampler_state), CalculateAtlasUv(eyes_box_uv, pendulum_frame_idx, float(tail_pupils_rows)));
        if (int(round(eyes_raw.x * 255.0)) == 3) {
            final_pixel = vec4(pupil_color.xyz, 1.0);
        }
    }

    // Update the clock face check to evaluate using our shifted coordinate space
    bool is_clock_box = (overlay_uv.x >= (42.0 / 128.0) && overlay_uv.x <= (106.0 / 128.0) &&
            overlay_uv.y >= (95.0 / 290.0) && overlay_uv.y <= (191.0 / 290.0));
    if (is_clock_box) {
        vec2 hand_box_uv;
        hand_box_uv.x = (overlay_uv.x - (42.0 / 128.0)) / (64.0 / 128.0);
        hand_box_uv.y = (overlay_uv.y - (95.0 / 290.0)) / (96.0 / 290.0);
        bool hour_hand = texture(sampler2D(hours_hand_sheet, sampler_state), CalculateAtlasUv(hand_box_uv, hour_frame_idx, 6.0)).x > 0.001;
        bool min_hand = texture(sampler2D(mins_hand_sheet, sampler_state), CalculateAtlasUv(hand_box_uv, min_frame_idx, 6.0)).x > 0.001;
        bool sec_hand = texture(sampler2D(seconds_hand_sheet, sampler_state), CalculateAtlasUv(hand_box_uv, sec_frame_idx, 6.0)).x > 0.001;
        if (hour_hand) final_pixel = vec4(hour_color.xyz, 1.0);
        if (min_hand) final_pixel = vec4(minute_color.xyz, 1.0);
        if (sec_hand) final_pixel = vec4(second_color.xyz, 1.0);
    }

    frag_color = final_pixel;
}
#pragma sokol @end

// ==============================================================================
// LINK TWO ENTIRELY INDEPENDENT SHADER PROGRAMS
// ==============================================================================
#pragma sokol @program catclock_bake vs_bake fs_bake
#pragma sokol @program catclock_composite vs_composite fs_composite
