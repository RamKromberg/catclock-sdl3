// === FILE: ./shaders/catclock.glsl ===
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
    frag_color = texture(sampler2D(texture_sheet, sampler_state), frag_uv) * cat_color;
}
#pragma sokol @end

#pragma sokol @program catclock vs fs
