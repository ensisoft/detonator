#define EDGE_WIDTH  0.05

// build-in uniforms
// material time in seconds.
uniform float kTime;

// when rendering particles with points the material
// shader must sample gl_PointCoord for texture coordinates
// instead of the texcoord varying from the vertex shader.
// the value kRenderPoints will be set to 1.0 so for portability
// the material shader can do:
//   vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
uniform float kRenderPoints;

// custom uniforms that need to match the
// json description
uniform float kGamma;

uniform float kValue;

// foreground color (healthbar)
uniform vec4 kFColor;
// background color
uniform vec4 kBColor;

// varyings from vertex stage.
varying vec2 vTexCoord;

// per particle data.
// these are only written when the drawable is a particle engine
varying float vParticleAlpha;
// particle random value.
varying float vParticleRandomValue;
// normalized particle lifetime.
varying float vParticleTime;

float Line(float width, float pos, float x)
{
    return smoothstep(pos-width*0.5, pos, x) -
           smoothstep(pos, pos+width*0.5, x);
}

void FragmentShaderMain()
{
    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);

    float vert_width = EDGE_WIDTH * 1.0;
    float hori_width = EDGE_WIDTH * 4.0;

    // smoothstep takes edge0, edge1, x and produces a value between
    // 0.0 and 1.0 with a smooth transition from edge0 to edge1.
    // when x < edge0 smoothstep = 0.0
    // when x > edge1 smoothstep = 1.0
    float foreground = 1.0 - smoothstep(kValue-0.05, kValue, coords.x);

    float background = clamp(
        Line(vert_width, 0.0+vert_width*0.5, coords.x) +
        Line(vert_width, 1.0-vert_width*0.5, coords.x) +
        Line(hori_width, 0.0+hori_width*0.5, coords.y) +
        Line(hori_width, 1.0-hori_width*0.5, coords.y),
        0.0, 1.0);

    fs_out.color  = kFColor * foreground + kBColor * background;
}
