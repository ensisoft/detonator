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

float VLine(float thickness, float position, float x)
{
    return smoothstep(position-thickness*0.5, position, x) -
        smoothstep(position, position+thickness*0.5, x);
}

float HLine(float thickness, float position, float x, float y)
{
    if (x >= 0.02 && x < 0.97)
        return smoothstep(position-thickness*0.5, position, y) -
            smoothstep(position, position+thickness*0.5, y);
    return 0.0;
}

void FragmentShaderMain()
{
    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);

    float vert_width = EDGE_WIDTH * 2.0;
    float hori_width = EDGE_WIDTH * 2.0;

    // smoothstep takes edge0, edge1, x and produces a value between
    // 0.0 and 1.0 with a smooth transition from edge0 to edge1.
    // when x < edge0 smoothstep = 0.0
    // when x > edge1 smoothstep = 1.0
    float foreground = 1.0 - smoothstep(kValue-0.05, kValue, coords.x);

    float background = clamp(
        VLine(0.02, 0.01, coords.x) +
        VLine(0.02, 0.98, coords.x) +
        HLine(0.04, 0.02, coords.x, coords.y) +
        HLine(0.04, 0.98, coords.x, coords.y),
        0.0, 1.0);

    fs_out.color  = kFColor * foreground + kBColor * background;
}
