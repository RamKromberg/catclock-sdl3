#pragma sokol @vs vs
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv;
out vec2 frag_uv;
void main() {
    gl_Position = vec4(pos, 0.0, 1.0);
    frag_uv = uv;
}
#pragma sokol @end

#pragma sokol @fs fs
in vec2 frag_uv;
out vec4 frag_color;

layout(binding = 0) uniform cb_params {
    vec4 tail_uv;
    vec4 eyes_uv;
    vec4 hours_uv;
    vec4 mins_uv;
    vec4 secs_uv;
    vec4 cat_color;
    vec4 tie_color;
    vec4 pupil_color;
    vec4 sclera_color;
    vec4 detail_color;
    vec4 halo_color;
};

layout(binding = 0) uniform texture2D texture_sheet;
layout(binding = 0) uniform sampler sampler_state;

void main() {
    // 1. COMPUTE PIXEL STEP VIA NATIVE GPU SCREEN DERIVATIVES
    // dFdx/dFdy automatically gives the exact texture coordinate delta for a 1-pixel screen step.
    // This perfectly insulates the sampling offsets from fractional subpixel rounding errors.
    vec2 texel_step_x = dFdx(frag_uv);
    vec2 texel_step_y = dFdy(frag_uv);

    // 2. AUDIT THE 8-WAY NEIGHBORHOOD AT AN EXACT 1-SCREEN-PIXEL BOUNDARY
    bool n_tl = texture(sampler2D(texture_sheet, sampler_state), frag_uv - texel_step_x + texel_step_y).r > 0.5;
    bool n_tc = texture(sampler2D(texture_sheet, sampler_state), frag_uv + texel_step_y).r > 0.5;
    bool n_tr = texture(sampler2D(texture_sheet, sampler_state), frag_uv + texel_step_x + texel_step_y).r > 0.5;

    bool n_ml = texture(sampler2D(texture_sheet, sampler_state), frag_uv - texel_step_x).r > 0.5;
    bool n_mr = texture(sampler2D(texture_sheet, sampler_state), frag_uv + texel_step_x).r > 0.5;

    bool n_bl = texture(sampler2D(texture_sheet, sampler_state), frag_uv - texel_step_x - texel_step_y).r > 0.5;
    bool n_bc = texture(sampler2D(texture_sheet, sampler_state), frag_uv - texel_step_y).r > 0.5;
    bool n_br = texture(sampler2D(texture_sheet, sampler_state), frag_uv + texel_step_x - texel_step_y).r > 0.5;

    // A pixel qualifies for the expanded boundary if any neighbor hits the cat body asset
    bool neighbor_hit = (n_tl || n_tc || n_tr || n_ml || n_mr || n_bl || n_bc || n_br);

    // 3. SAMPLE CORE STRUCTURAL LAYER MASKS
    vec4 mask = texture(sampler2D(texture_sheet, sampler_state), frag_uv);
    bool catback = mask.r > 0.5; // True black cat body layout line
    bool cattie = mask.g > 0.5; // Necktie asset overlay layer
    bool catwhite = mask.b > 0.5; // True white details mask
    bool eyes = mask.a > 0.5; // Eye socket aperture clip lines

    // Start with a fully transparent backdrop matching desktop window bounds
    vec4 final_color = vec4(0.0, 0.0, 0.0, 0.0);

    // --- PIPELINE PRIORITY COMPOSITING CASCADE ---
    // Rule 1: Apply the 1px halo expansion if an edge neighbor hits but we aren't drawing the body
    if (neighbor_hit) {
        final_color = vec4(halo_color.rgb, 1.0);
    }
    // Rule 2: Overlay the core solid cat body silhouette directly on top
    if (catback) {
        final_color = vec4(cat_color.rgb, 1.0);
    }
    // Rule 3: Stamp down the necktie component layout
    if (cattie) {
        final_color = vec4(tie_color.rgb, 1.0);
    }
    // Rule 4: Overlay white fur details safely within body boundaries
    if (catwhite) {
        final_color = vec4(detail_color.rgb, 1.0);
    }
    // Rule 5: Open eye socket apertures down to the underlying background layers
    if (eyes) {
        final_color = vec4(sclera_color.rgb, 1.0);
    }

    // Force a runtime link across all uniform vectors to satisfy shdc block signatures
    vec4 dynamic_sanity_guard = tail_uv + eyes_uv + hours_uv + mins_uv + secs_uv + pupil_color;
    frag_color = final_color + (dynamic_sanity_guard * 0.000001);
}

#pragma sokol @end

#pragma sokol @program catclock vs fs
