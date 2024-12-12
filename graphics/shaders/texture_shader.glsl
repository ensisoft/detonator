R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// Texture shader that samples a texture for color data.

#version 300 es

precision highp float;

// The texture to sample from
uniform sampler2D kTexture;
// The texture sub-rectangle inside a potentially
// bigger potentially texture. Used when dealing with
// textures packed inside other textures (atlasses)
uniform vec4 kTextureBox;

// The base color used to modulate the outgoing color value.
uniform vec4 kBaseColor;

// Flag to indicate whether the texturess is an alpha masks or not.
// 0.0 = texture is normal color data and has RGBA channels
// 1.0 = texture is alpha mask only.
uniform float kAlphaMask;

// Runtime flag to indicate GL_POINTS and gl_PointCoord
// for texture coordinates.
uniform float kRenderPoints;

// Alpha cutoff for controllling when to discard the fragment.
// This is useful for using "opaque" rendering with alpha
// cutout sprites.
uniform float kAlphaCutoff;

// Particle effect enumerator. Used when rendering particles.
uniform int kParticleEffect;

// Current material time in seconds.
uniform float kTime;

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

// Incoming vertex texture coordinates.
in vec2 vTexCoord;

// Incoming per particle random value.
in float vParticleRandomValue;

// Incoming per particle alpha value.
in float vParticleAlpha;

// do software wrapping for texture coordinates.
// used for cases when texture sub-rects are used.
float Wrap(float x, float w, int mode) {
    if (mode == 1) {
        return clamp(x, 0.0, w);
    } else if (mode == 2) {
        return fract(x / w) * w;
    } else if (mode == 3) {
        float p = floor(x / w);
        float f = fract(x / w);
        int m = int(floor(mod(p, 2.0)));
        if (m == 0)
           return f * w;

        return w - f * w;
    }
    return x;
}

vec2 WrapTextureCoords(vec2 coords, vec2 box) {
    float x = Wrap(coords.x, box.x, kTextureWrap.x);
    float y = Wrap(coords.y, box.y, kTextureWrap.y);
    return vec2(x, y);
}

vec2 RotateCoords(vec2 coords) {
    float random_angle = 0.0;
    if (kParticleEffect == 2)
        random_angle = mix(0.0, 3.1415926, vParticleRandomValue);

    float angle = kTextureRotation + kTextureVelocity.z * kTime + random_angle;
    coords = coords - vec2(0.5, 0.5);
    coords = mat2(cos(angle), -sin(angle),
                  sin(angle),  cos(angle)) * coords;
    coords += vec2(0.5, 0.5);
    return coords;
}

void FragmentShaderMain() {

    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
    coords = RotateCoords(coords);

    coords += kTextureVelocity.xy * kTime;
    coords = coords * kTextureScale;

    // apply texture box transformation.
    vec2 scale_tex = kTextureBox.zw;
    vec2 trans_tex = kTextureBox.xy;

    // scale and transform based on texture box. (todo: maybe use texture matrix?)
    coords = WrapTextureCoords(coords * scale_tex, scale_tex) + trans_tex;

    // sample textures, if texture is a just an alpha mask we use
    // only the alpha channel later.
    vec4 texel = texture(kTexture, coords);

    // either modulate/mask texture color with base color
    // or modulate base color with texture's alpha value if
    // texture is an alpha mask
    vec4 color = mix(kBaseColor * texel, vec4(kBaseColor.rgb, kBaseColor.a * texel.a), kAlphaMask);
    color.a *= vParticleAlpha;

    if (color.a <= kAlphaCutoff)
        discard;

    fs_out.color = color;
}

)CPP_RAW_STRING"