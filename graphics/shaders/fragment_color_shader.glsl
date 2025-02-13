R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// Simple color shader that only provides a single
// color as the diffuse color value for fragment shading.

#version 300 es

precision highp float;

uniform uint kMaterialFlags;

// The incoming color value.
uniform vec4 kBaseColor;

// current material time.
uniform float kTime;

// @varyings

#ifdef GEOMETRY_IS_PARTICLES
  // Incoming per particle alpha value.
  in float vParticleAlpha;
#endif

// @code

#ifndef CUSTOM_FRAGMENT_MAIN
void FragmentShaderMain() {
    vec4 color = kBaseColor;

    #ifdef GEOMETRY_IS_PARTICLES
        // modulate by alpha
        color.a *= vParticleAlpha;
    #endif

    // out value.
    fs_out.color = color;
    fs_out.flags = kMaterialFlags;
}
#endif // CUSTOM_FRAGMENT_MAIN

)CPP_RAW_STRING"