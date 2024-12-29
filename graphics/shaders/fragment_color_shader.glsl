R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// Simple color shader that only provides a single
// color as the diffuse color value for fragment shading.

#version 300 es

precision highp float;

// The incoming color value.
uniform vec4 kBaseColor;

// @varyings

#ifdef GEOMETRY_IS_PARTICLES
  // Incoming per particle alpha value.
  in float vParticleAlpha;
#endif

void FragmentShaderMain() {

    vec4 color = kBaseColor;

#ifdef GEOMETRY_IS_PARTICLES
    // modulate by alpha
    color.a *= vParticleAlpha;
#endif

    // out value.
    fs_out.color = color;
}

)CPP_RAW_STRING"