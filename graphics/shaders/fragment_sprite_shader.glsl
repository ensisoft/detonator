R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// Sprite shader that renders a sprite animation by
// interpolating between two subsequent animation frames.

#version 300 es

precision highp float;

// @uniforms

uniform uint kMaterialFlags;

// Texture sampler2 for the two sprite animation frames
// These may or may not be bound to same texture object
// depending on whether the frames are in separate textures
// or baked into a single "sprite sheet".
uniform sampler2D kTexture0;
uniform sampler2D kTexture1;

// The sub-rectangles inside the sprite textures that
// define the actual animation frame. These are used
// when animation frames are contained inside bigger
// textures in a texture atlas. Or in case of a sprite
// sheet the texture samplers both point to the same
// texture object but the rectangles define the frames.
uniform vec4 kTextureBox0;
uniform vec4 kTextureBox1;

// The base color used to modulate the outgoing color value.
uniform vec4 kBaseColor;

// Alpha cutoff for controllling when to discard the fragment.
// This is useful for using "opaque" rendering with alpha
// cutout sprites.
uniform float kAlphaCutoff;

// Current material time in seconds.
uniform float kTime;

// The blending coefficient for mixing/blending between
// the two animation frames.
uniform float kBlendCoeff;

// Particle effect enumerator. Used when rendering particles.
uniform int kParticleEffect;

// Texture coordinate scaling factor(s) for x and y axis.
// Applies on the incoming texture coordinates.
uniform vec2 kTextureScale;

// Linear texture coordinate velocity.
// Translates the incoming texture coordinates over time.
// Texture units per second.
uniform vec3 kTextureVelocity;

// Rotational texture coordinate velocity.
// Rotates the incoming texture coordinates over time.
// Radians per second.
uniform float kTextureRotation;

// Software texture wrapping mode. Needed when texture
// sub-rectangles are used and hardware texture wrapping
// mode cannot be used.
// 0 = disabled, nothing is done.
// 1 = clamp to sub-rect borders
// 2 = wrap around sub-rect borders
// 3 = wrap + mirror around sub-rect borders.
uniform ivec2 kTextureWrap;

// Flags to indicate whether the textures are alpha masks or not.
// 0.0 = texture is normal color data and has RGBA channels
// 1.0 = texture is alpha mask only.
uniform vec2 kAlphaMask;


// @varyings
#ifdef GEOMETRY_IS_PARTICLES
  // Incoming per particle random value.
  in float vParticleRandomValue;
  // Incoming per particle alpha value.
  in float vParticleAlpha;
#endif

vec2 RotateCoords(vec2 coords) {
    float random_angle = 0.0;

#ifdef GEOMETRY_IS_PARTICLES
    if (kParticleEffect == PARTICLE_EFFECT_ROTATE)
        random_angle = mix(0.0, 3.1415926, vParticleRandomValue);
#endif

    float angle = kTextureRotation + kTextureVelocity.z * kTime + random_angle;
    return RotateTextureCoords(coords, angle);
}

void FragmentShaderMain() {

    vec2 coords = GetTextureCoords();
    coords = RotateCoords(coords);

    coords += kTextureVelocity.xy * kTime;
    coords = coords * kTextureScale;

    // apply texture box transformation.
    vec2 scale_tex0 = kTextureBox0.zw;
    vec2 scale_tex1 = kTextureBox1.zw;
    vec2 trans_tex0 = kTextureBox0.xy;
    vec2 trans_tex1 = kTextureBox1.xy;

    // scale and transform based on texture box. (todo: maybe use texture matrix?)
    vec2 c1 = WrapTextureCoords(coords * scale_tex0, scale_tex0, kTextureWrap) + trans_tex0;
    vec2 c2 = WrapTextureCoords(coords * scale_tex1, scale_tex1, kTextureWrap) + trans_tex1;

    // sample textures, if texture is a just an alpha mask we use
    // only the alpha channel later.
    vec4 tex0 = texture(kTexture0, c1);
    vec4 tex1 = texture(kTexture1, c2);

    vec4 col0 = mix(kBaseColor * tex0, vec4(kBaseColor.rgb, kBaseColor.a * tex0.a), kAlphaMask[0]);
    vec4 col1 = mix(kBaseColor * tex1, vec4(kBaseColor.rgb, kBaseColor.a * tex1.a), kAlphaMask[1]);

    vec4 color = mix(col0, col1, kBlendCoeff);

#ifdef GEOMETRY_IS_PARTICLES
    color.a *= vParticleAlpha;
#endif

    if (color.a <= kAlphaCutoff)
        discard;

    fs_out.color = color;
    fs_out.flags = kMaterialFlags;
}

)CPP_RAW_STRING"