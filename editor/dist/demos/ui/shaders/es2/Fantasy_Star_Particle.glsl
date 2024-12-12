#version 300 es

uniform sampler2D kTexture;
uniform vec4 kBaseColor;
uniform float kTime;
uniform float kRenderPoints;

//// varyings from vertex stage ////

// particle specific values. only used / needed
// when the material is used to render particles.
in float vParticleAlpha;
in float vParticleRandomValue;
in float vParticleTime;

// texture coordinates
in vec2 vTexCoord;

vec2 Motion(vec2 st)
{
    float sine = sin(kTime) * 0.5 + 0.5;
    float angle = sine; //2.0*3.1417 * sin(kTime);


    st -= vec2(0.5, 0.5);
    st = mat2(cos(angle), -sin(angle),
              sin(angle), cos(angle)) * st;
    st *= vec2(1.0 - 0.3 * sine);
    st += vec2(0.5, 0.5);

    //st.x = st.x + sin(kTime*0.1);
    //st.y = st.y + cos(kTime*0.5);
    return st;
}

vec4 BlendTransparency(vec4 srcColor, vec4 dstColor) {
    // Compute the alpha blending factors
    float srcAlpha = srcColor.a;
    float dstAlpha = 1.0 - srcAlpha;

    // Blend the source and destination colors
    vec3 blendedColor = srcColor.rgb * srcAlpha + dstColor.rgb * dstAlpha;

    // Compute the resulting alpha
    float blendedAlpha = srcColor.a + dstColor.a * (1.0 - srcColor.a);

    // Return the blended color with the blended alpha
    return vec4(blendedColor, blendedAlpha);
}

void FragmentShaderMain()
{
    // your code here
    //float sine = sin(kTime*10.0) * 0.5 + 0.5;

    float t = mod(kTime + vParticleRandomValue * 3.0, 3.0);
    float s = smoothstep(1.5, 2.0, t) - smoothstep(2.5, 3.0, t);

    vec2 coord = mix(vTexCoord, gl_PointCoord, kRenderPoints);

    vec2 texture_coord = Motion(coord); // * texture_box_scale + texture_box_translate;

    vec4 texture_sample = texture(kTexture, texture_coord);

    //vec4 star_color  = vec4(kBaseColor.rgb, texture_sample.r * s);
    //fs_out.color = star_color;
    fs_out.color = kBaseColor;
    fs_out.color.rgb *= texture_sample.r * s;

}
