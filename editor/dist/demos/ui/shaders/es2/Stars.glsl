#version 300 es
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
in float vParticleAlpha;
in float vParticleRandomValue;
in float vParticleTime;

// texture coordinates
in vec2 vTexCoord;

vec2 Motion(vec2 st)
{
    float angle = 2.0*3.1417 * sin(kTime*0.02);

    //float s = 1.0 - ((sin(kTime/5.0) * 0.5 + 0.5) * 0.3);

    st -= vec2(0.5, 0.5);
    //st *= s;
    st = mat2(cos(angle), -sin(angle),
              sin(angle), cos(angle)) * st;
    st += vec2(0.5, 0.5);

    st.x = st.x + sin(kTime*0.01);
    st.y = st.y + cos(kTime*0.05);
    return st;
}


void FragmentShaderMain()
{
    // your code here
    vec2 st = Motion(vTexCoord);
    st *= vec2(3.0, 3.0);

    fs_out.color = texture(kTexture, st);
}
