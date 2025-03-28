#version 300 es
// material system will provide these.
// kTime will be the current material instance runtime in second.s
uniform float kTime;

uniform vec4  kBaseColor;
uniform vec4  kColor0;
uniform sampler2D kTexture0;

uniform vec4 kTextureRect;

// data from vertex stage.
in vec2 vTexCoord;

vec2 TransformTextureCoords(vec2 coord)
{
    vec2 trans = kTextureRect.xy;
    vec2 scale = kTextureRect.zw;
    return coord * scale + trans;
}

float ring_alpha(float ring_distance_from_origin, float fragment_distance_from_origin)
{
    float ring_width = 0.04;
    float rd = ring_distance_from_origin;
    float fd = fragment_distance_from_origin;
    return smoothstep(rd - ring_width, rd, fd) - smoothstep(rd, rd + ring_width, fd);
}

void FragmentShaderMain()
{
    float time = 0.6;
    float s = mod(kTime, time) / time;
    float disc_radius = 0.01 + s * (0.5-0.015);
    disc_radius = clamp(disc_radius, 0.01, 0.45);


    vec2 fragment_pos = vTexCoord - vec2(0.5, 0.5);
    vec2 fragment_ori = vec2(0.0, 0.0);
    float dist = length(fragment_ori - fragment_pos);
    dist = clamp(dist, 0.0, 0.5);

    float disc_alpha = 1.0 - smoothstep(disc_radius-0.01, disc_radius, dist);

    float fadeout = smoothstep(disc_radius-0.02, 0.01, dist);


    float alpha = clamp(disc_alpha - fadeout, 0.0, 1.0);

    vec4 texture_sample = texture(kTexture0, TransformTextureCoords(vTexCoord));
    vec4 base_color = kBaseColor;

    float ring_alpha = ring_alpha(disc_radius, dist) * 0.3 * (1.0-s);
    vec4 ring_color = kColor0 * ring_alpha;

    base_color.a = alpha * 0.8 * (1.0-s) * texture_sample.r;

    fs_out.color = base_color + ring_color;
}
