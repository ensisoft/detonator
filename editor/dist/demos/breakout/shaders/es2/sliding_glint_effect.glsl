#version 300 es

uniform float kTime;

in vec2 vTexCoord;

void FragmentShaderMain()
{
    vec2 uv = vTexCoord;

    float time  = 1.4;
    float cycle = mod(kTime / time, time) / time;

    float glint_width  = 0.8;
    float left  = -1.0 + cycle * 2.0 + uv.y * 0.1;
    float right = left + glint_width;

    if (!(uv.x > left && uv.x < right))
        discard;

    float d = (uv.x - left) / glint_width;
    float v = sin(radians(180.0*d));
    fs_out.color = vec4(v, v, v, v) * 0.9;
}


