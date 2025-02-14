R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen

#version 300 es

// @uniforms

// the monotonic material instance time in seconds.
uniform float kTime;

uniform uint kMaterialFlags;
// current surface type value.
uniform uint kSurfaceType;
// particle alpha mask
uniform sampler2D kMask;
// the sub-rectangle in the texture from where to sample
uniform vec4 kMaskRect;
// particle start color when time is 0.0
uniform vec4 kParticleStartColor;
// particle mid phase color when time is 0.5
uniform vec4 kParticleMidColor;
// particle end color when time is 1.0
uniform vec4 kParticleEndColor;
// base particle (texture coordinate) rotation.
uniform float kParticleBaseRotation;
// particle rotation mode.
uniform uint kParticleRotation;

// @varyings

#ifdef GEOMETRY_IS_PARTICLES
  // Normalized lifetime of the particle
  in float vParticleTime;
  // Particle alpha value.
  in float vParticleAlpha;
  // Particle random value. [0.0, 1.0]
  in float vParticleRandomValue;
  // Angle of the particle's direction vector
  // relative to the x axis
  in float vParticleAngle;
#else
  // currently only used when in the material editor
  // make these dummies
  #define vParticleTime kTime
  #define vParticleAlpha 1.0
  #define vParticleRandomValue 0.0
  #define vParticleAngle 0.0
#endif

#ifndef DRAW_POINTS
  // Vertex texture coordinates.
  in vec2 vTexCoord;
#endif


void MixColors(float alpha) {

    float first_half_time = min(vParticleTime, 0.5) / 0.5;
    float second_half_time = max(0.0, vParticleTime-0.5) / 0.5;
    vec4 first_half = mix(kParticleStartColor, kParticleMidColor, first_half_time);

    vec4 color = mix(first_half, kParticleEndColor, second_half_time);

    if (kSurfaceType == MATERIAL_SURFACE_TYPE_TRANSPARENT) {
        // blend with straight alpha and with transparent surface
        // alpha is the particle transparency value
        fs_out.color.rgb = color.rgb;
        fs_out.color.a   = (alpha * vParticleAlpha * color.a);
    } else if (kSurfaceType == MATERIAL_SURFACE_TYPE_EMISSIVE) {
        // not blending transparently, alpha does not control
        // transparency but modulates the intensity of the color
        fs_out.color.rgb = color.rgb * alpha * vParticleAlpha;
        fs_out.color.a = 1.0;
    } else {
        fs_out.color.rgb = color.rgb;
    }
}

float ReadTextureAlpha(vec2 coord) {

    float angle = 0.0;

    if (kParticleRotation == PARTICLE_ROTATION_BASE) {
        angle = kParticleBaseRotation;
    } else if (kParticleRotation == PARTICLE_ROTATION_RANDOM) {
        angle = vParticleRandomValue * PI * 2.0;
    } else if (kParticleRotation == PARTICLE_ROTATION_DIRECTION) {
        angle = vParticleAngle;
    } else if (kParticleRotation == PARTICLE_ROTATION_DIRECTION_AND_BASE) {
        float base_rotation = kParticleBaseRotation * PI * 2.0;
        angle = base_rotation + vParticleAngle;
    }

    // rotate texture coords
    if (kParticleRotation != PARTICLE_ROTATION_NONE) {
        // discard fragments if the rotated texture coordinates would
        // fall outside of the 0.0-1.0 range.
        float len = length(coord - vec2(0.5, 0.5));
        if (len > 0.5)
            discard;

        coord -= vec2(0.5, 0.5);
        coord  = mat2(cos(angle), -sin(angle),
                      sin(angle),  cos(angle)) * coord;
        coord += vec2(0.5, 0.5);
    }

    // apply texture rect transform to get the final coordinates
    // for sampling the texture. this transformation allows for
    // texture packing to be done.
    vec2 texture_trans = kMaskRect.xy;
    vec2 texture_scale = kMaskRect.zw;
    coord = coord * texture_scale + texture_trans;

    float alpha = texture(kMask,  coord).r;
    return alpha;
}

void FragmentShaderMain() {

#ifdef DRAW_POINTS
    // this is the normal particle rendering path
    // using point rendering. texture sampling works
    // but since the geometry is points the texture
    // coordinates are not from vertex data but provided
    // by the rasterizer in gl_PointCoord
    float alpha = ReadTextureAlpha(gl_PointCoord);

    MixColors(alpha);
#endif

#ifdef DRAW_LINES
    // right now we're not supporting texture sampling
    // or rather.. the particle engine doesn't provide
    // texture coordinates for non point geometry (lines)
    MixColors(1.0);
#endif

#ifdef DRAW_TRIANGLES
    // this path is only taken when the material widget renders
    // the preview, right now it doesn't know about point rendering
    // yet so it uses a textured shape.
    float alpha = ReadTextureAlpha(vTexCoord);

    MixColors(alpha);
#endif

    fs_out.flags = kMaterialFlags;
}

)CPP_RAW_STRING"