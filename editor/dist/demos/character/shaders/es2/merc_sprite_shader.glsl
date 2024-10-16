#version 300 es
uniform sampler2D kSpriteSheet;

uniform float kAlphaCutoff;
uniform float kAimAngle;
uniform int kState;

uniform float kTime;

// varyings from vertex stage.
in vec2 vTexCoord;

#define TEX_WIDTH   276.0
#define TEX_HEIGHT  2208.0
#define CELL_WIDTH  46.0
#define CELL_HEIGHT 46.0
#define NUM_COLS    6.0
#define NUM_ROWS    48.0

#define STATE_STAND 0
#define STATE_KNEEL 1
#define STATE_RUN   2

vec4 SampleCell(float col, float row,  vec2 coord)
{
    // normalized cell size (in texture units)
    vec2 cell_size = vec2(CELL_WIDTH, CELL_HEIGHT) / vec2(TEX_WIDTH, TEX_HEIGHT);

    // compute cell position (top left) in texture coords
    vec2 cell_pos = vec2(col, row) * cell_size;

    //coord = clamp(coord, 0.0, 0.98);

    vec2 sample_pos = cell_pos + coord * cell_size;

    return texture(kSpriteSheet, sample_pos);

}

float Aim()
{
    float index = clamp(kAimAngle, 0.0, 1.0);
    return ceil(index * (NUM_ROWS - 1.0));
}


vec4 Stand()
{
    float row = Aim();
    float col = 0.0;

    return SampleCell(col, row, vTexCoord.xy);
}

vec4 Kneel()
{
    float row = Aim();
    float col = 1.0;

    return SampleCell(col, row, vTexCoord.xy);
}

vec4 Run()
{
    // the run cycles are at cell 2,3,4,5
    float row = Aim();
    float col = 2.0;

    // run cycle duration in seconds.
    float duration = 0.5;
    // normalized cycle position
    float t = (mod(kTime, duration)) / duration;

    float i = ceil(t * 4.0) - 1.0;

    return SampleCell(col + i, row, vTexCoord.xy);
}

void FragmentShaderMain()
{
    vec4 ret;

    if (kState == STATE_STAND)
    {
        ret = Stand();
    }
    else if (kState == STATE_KNEEL)
    {
        ret = Kneel();
    }
    else if (kState == STATE_RUN)
    {
        ret = Run();
    }
    fs_out.color = ret;
}
