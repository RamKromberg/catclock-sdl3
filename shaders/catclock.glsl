// ============================================================================
// VERTEX SHADER
// ============================================================================
#pragma sokol @vs vs
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv;

out vec2 frag_uv;

void main() {
    gl_Position = vec4(pos, 0.0, 1.0);
    frag_uv = uv;
}
#pragma sokol @end

// ============================================================================
// FRAGMENT SHADER
// ============================================================================
#pragma sokol @fs fs
in vec2 frag_uv;
out vec4 frag_color;

layout(binding = 0) uniform cb_params {
    int hour_frame_idx;
    int min_frame_idx;
    int sec_frame_idx;
    int pendulum_frame_idx;

    vec4 cat_color;
    vec4 tie_color;
    vec4 pupil_color;
    vec4 sclera_color;
    vec4 detail_color;
    vec4 halo_color;
    vec4 hour_color;
    vec4 minute_color;
    vec4 second_color;
};

// Explicit binding slots restored to satisfy your compiler rules
layout(binding = 0) uniform texture2D texture_sheet;
layout(binding = 1) uniform texture2D hours_hand_sheet;
layout(binding = 2) uniform texture2D mins_hand_sheet;
layout(binding = 3) uniform texture2D seconds_hand_sheet;
layout(binding = 4) uniform texture2D eyes_sheet;
layout(binding = 5) uniform texture2D tail_sheet;

layout(binding = 0) uniform sampler sampler_state;

/**
 * Translates an explicit target frame index cleanly into absolute atlas sheet
 * UV coordinates matching a standardized 10-column spreadsheet template profile.
 */
vec2 CalculateAtlasUv(vec2 local_box_uv, int frame_idx) {
    vec2 segment_cell = vec2(1.0 / 10.0, 1.0 / 6.0);

    float col = mod(float(frame_idx), 10.0);
    float row = floor(float(frame_idx) / 10.0);

    vec2 cell_offset = vec2(col, row) * segment_cell;
    return cell_offset + (local_box_uv * segment_cell);
}

void main() {
    // 1. Fetch multi-channel structural base template blueprints
    vec4 mask = texture(sampler2D(texture_sheet, sampler_state), frag_uv);
    bool catbody = mask.r > 0.5;
    bool cattie = mask.g > 0.5;
    bool catwhite = mask.b > 0.5;
    bool cateyes = mask.a > 0.5;

    // 2. Clock Hands Layer (LIFTED BY 9 PIXELS: 104-9=95, 200-9=191)
    bool in_hand_box = (frag_uv.x >= (42.0 / 128.0) && frag_uv.x <= (106.0 / 128.0) &&
            frag_uv.y >= (95.0 / 290.0) && frag_uv.y <= (191.0 / 290.0));
    bool hour_hand = false;
    bool min_hand = false;
    bool sec_hand = false;

    if (in_hand_box) {
        vec2 hand_box_uv;
        hand_box_uv.x = (frag_uv.x - (42.0 / 128.0)) / (64.0 / 128.0);
        hand_box_uv.y = (frag_uv.y - (95.0 / 290.0)) / (96.0 / 290.0);

        hour_hand = texture(sampler2D(hours_hand_sheet, sampler_state), CalculateAtlasUv(hand_box_uv, hour_frame_idx)).r > 0.001;
        min_hand = texture(sampler2D(mins_hand_sheet, sampler_state), CalculateAtlasUv(hand_box_uv, min_frame_idx)).r > 0.001;
        sec_hand = texture(sampler2D(seconds_hand_sheet, sampler_state), CalculateAtlasUv(hand_box_uv, sec_frame_idx)).r > 0.001;
    }

    // 3. Eye Sockets Layer (LIFTED BY 9 PIXELS: 26-9=17, 58-9=49)
    vec4 eyes_pixel = vec4(0.0);
    bool in_eyes_box = (frag_uv.x >= (44.0 / 128.0) && frag_uv.x <= (108.0 / 128.0) &&
            frag_uv.y >= (17.0 / 290.0) && frag_uv.y <= (49.0 / 290.0));
    if (in_eyes_box) {
        vec2 eyes_box_uv;
        eyes_box_uv.x = (frag_uv.x - (44.0 / 128.0)) / (64.0 / 128.0);
        eyes_box_uv.y = (frag_uv.y - (17.0 / 290.0)) / (32.0 / 290.0);
        eyes_pixel = texture(sampler2D(eyes_sheet, sampler_state), CalculateAtlasUv(eyes_box_uv, pendulum_frame_idx));
    }

    // 4. Dynamic Swinging Tail Layer (Pivot script baseline adjusted by 1px top padding rule)
    vec4 tail_pixel = vec4(0.0);
    bool in_tail_box = (frag_uv.x >= (27.0 / 128.0) && frag_uv.x <= (123.0 / 128.0) &&
            frag_uv.y >= (192.0 / 290.0) && frag_uv.y <= (288.0 / 290.0));

    bool tail_halo_hit = false;
    if (in_tail_box) {
        vec2 tail_box_uv;
        tail_box_uv.x = (frag_uv.x - (27.0 / 128.0)) / (96.0 / 128.0);
        tail_box_uv.y = (frag_uv.y - (192.0 / 290.0)) / (96.0 / 290.0);

        // Sample current target fragment center point
        tail_pixel = texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv, pendulum_frame_idx));

        // Compute 1 screen-pixel steps scaled cleanly to texture space coordinates
        vec2 tail_texel_step_x = dFdx(tail_box_uv);
        vec2 tail_texel_step_y = dFdy(tail_box_uv);

        // Sample the 8 immediate screen-pixel neighbors relative to the rendering viewport
        int n0 = int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv - tail_texel_step_x + tail_texel_step_y, pendulum_frame_idx)).r * 255.0));
        int n1 = int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv + tail_texel_step_y, pendulum_frame_idx)).r * 255.0));
        int n2 = int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv + tail_texel_step_x + tail_texel_step_y, pendulum_frame_idx)).r * 255.0));
        int n3 = int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv - tail_texel_step_x, pendulum_frame_idx)).r * 255.0));
        int n4 = int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv + tail_texel_step_x, pendulum_frame_idx)).r * 255.0));
        int n5 = int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv - tail_texel_step_x - tail_texel_step_y, pendulum_frame_idx)).r * 255.0));
        int n6 = int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv - tail_texel_step_y, pendulum_frame_idx)).r * 255.0));
        int n7 = int(round(texture(sampler2D(tail_sheet, sampler_state), CalculateAtlasUv(tail_box_uv + tail_texel_step_x - tail_texel_step_y, pendulum_frame_idx)).r * 255.0));

        // Evaluate if any neighbor contains a solid tail asset token channel match
        if ((n0 == 1 || n0 == 8) || (n1 == 1 || n1 == 8) || (n2 == 1 || n2 == 8) || (n3 == 1 || n3 == 8) ||
                (n4 == 1 || n4 == 8) || (n5 == 1 || n5 == 8) || (n6 == 1 || n6 == 8) || (n7 == 1 || n7 == 8)) {
            tail_halo_hit = true;
        }
    }

    // 5. Composite layout blend stack channels
    vec4 final_color = vec4(0.0);

    // Apply white background halo glow layer bounds via screen derivatives
    vec2 texel_step_x = dFdx(frag_uv);
    vec2 texel_step_y = dFdy(frag_uv);
    bool n_tl = texture(sampler2D(texture_sheet, sampler_state), frag_uv - texel_step_x + texel_step_y).r > 0.5;
    bool n_tc = texture(sampler2D(texture_sheet, sampler_state), frag_uv + texel_step_y).r > 0.5;
    bool n_tr = texture(sampler2D(texture_sheet, sampler_state), frag_uv + texel_step_x + texel_step_y).r > 0.5;
    bool n_ml = texture(sampler2D(texture_sheet, sampler_state), frag_uv - texel_step_x).r > 0.5;
    bool n_mr = texture(sampler2D(texture_sheet, sampler_state), frag_uv + texel_step_x).r > 0.5;
    bool n_bl = texture(sampler2D(texture_sheet, sampler_state), frag_uv - texel_step_x - texel_step_y).r > 0.5;
    bool n_bc = texture(sampler2D(texture_sheet, sampler_state), frag_uv - texel_step_y).r > 0.5;
    bool n_br = texture(sampler2D(texture_sheet, sampler_state), frag_uv + texel_step_x - texel_step_y).r > 0.5;

    // Draw master unified static asset halo outline base
    if (n_tl || n_tc || n_tr || n_ml || n_mr || n_bl || n_bc || n_br) {
        final_color = vec4(halo_color.rgb, 1.0);
    }

    // LAYER 1b: Draw moving tail background halo boundary (true 1px screen space)
    if (tail_halo_hit) {
        final_color = vec4(halo_color.rgb, 1.0);
    }

    // LAYER 2: Draw swinging tail main black body layout section
    if (in_tail_box) {
        int tail_token = int(round(tail_pixel.r * 255.0));
        if (tail_token == 1 || tail_token == 8) {
            final_color = vec4(cat_color.rgb, 1.0);
        }
    }

    // Overlay core foreground material shapes shapes over the tail layer
    if (catbody) final_color = vec4(cat_color.rgb, 1.0);
    if (cateyes) final_color = vec4(sclera_color.rgb, 1.0);
    if (cattie) final_color = vec4(tie_color.rgb, 1.0);
    if (catwhite) final_color = vec4(detail_color.rgb, 1.0);

    if (in_eyes_box) {
        int palette_token = int(round(eyes_pixel.r * 255.0));
        if (palette_token == 3) {
            final_color = vec4(pupil_color.rgb, 1.0);
        } else if (cateyes) {
            final_color = vec4(sclera_color.rgb, 1.0);
        }
    }

    if (hour_hand) final_color = vec4(hour_color.rgb, 1.0);
    if (min_hand) final_color = vec4(minute_color.rgb, 1.0);
    if (sec_hand) final_color = vec4(second_color.rgb, 1.0);

    frag_color = final_color;
}
#pragma sokol @end

#pragma sokol @program catclock vs fs
