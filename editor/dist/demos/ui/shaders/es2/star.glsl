
// warning. do not delete the below line.
// MAPI=1

//// material uniforms ////

// material instance time in seconds.
uniform float kTime;

// The current texture to sample from.
uniform sampler2D kTexture;

// Texture box (sub-rectangle) from which to read
// the texture. This is a packed uniform so that xy
// components define the texture box translation
// and zw define the size of the box.
// You can use these data to transform the incoming
// texture coordinates so that the sampling happens
// from the desired area of the texture (the texture rect)
uniform vec4 kTextureBox;

// texture sampling scaling coefficients.
uniform vec2 kTextureScale;

// texture sampling velocity, xy and z = rotation (radians)
// per seconds.
uniform vec3 kTextureVelocity;

// Texture wrapping mode. xy = both x and y axis of the texture
// 0 = disabled
// 1 = clamp
// 2 = wrap
uniform ivec2 kTextureWrap;

// Texture sampling coordinate base rotation (in radians)
uniform float kTextureRotation;

// Base color set for material.
uniform vec4 kBaseColor;

// Flag to indicate whether the texture is an alpha mask
// or not. Alpha masks only have valid data for the alpha
// channel.
// 0.0 = texture is normal color data
// 1.0 = texture is an alpha mask
uniform float kAlphaMask;

// Flag to indicate whether the current rendering is
// rendering points. (GL_POINTS)
// When points are being rendered normal texture coordinates
// no longer apply but 'gl_PointCoord' must be used instead.
//
//  to cover both cases you can do
//
//  vec2 c = mix(vTexCoord, gl_PointCord, kRenderPoints);
//
uniform float kRenderPoints;

// Particle effect enum.
// 0 = off, 1 = rotate texture coordinates, 2 = custom
uniform int kParticleEffect;

// Alpha cutoff threshold value set on the material.
uniform float kAlphaCutoff;

//// varyings from vertex stage ////

// particle specific values. only used / needed
// when the material is used to render particles.
varying float vParticleAlpha;
varying float vParticleRandomValue;
varying float vParticleTime;

// texture coordinates
varying vec2 vTexCoord;

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

    vec4 texture_sample = texture2D(kTexture, texture_coord);

    //vec4 star_color  = vec4(kBaseColor.rgb, texture_sample.r * s);
    //fs_out.color = star_color;
    fs_out.color = kBaseColor;
    fs_out.color.rgb *= texture_sample.r * s;

}
