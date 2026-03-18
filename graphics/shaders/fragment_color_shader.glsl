R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// Simple color shader that only provides a single
// color as the diffuse color value for fragment shading.

#version 300 es

precision highp float;

// @uniforms

uniform uint kMaterialFlags;

// The incoming color value.
uniform vec4 kBaseColor;

// current material time.
uniform float kTime;

#if defined(ENABLE_SDF_SUPPORT)
  uniform int kSdfShape;
  uniform int kSdfFillMode;
  uniform float kSdfOutlineWidth;
  uniform float kSdfCornerRadius;
  uniform float kSdfAspectRatio;
#endif

// @varyings

#if defined(GEOMETRY_IS_PARTICLES)
  // Incoming per particle alpha value.
  in float vParticleAlpha;
#endif

// @code

#ifndef CUSTOM_FRAGMENT_MAIN
void FragmentShaderMain() {

  vec4 color = kBaseColor;

  #if defined(ENABLE_SDF_SUPPORT)
    if ((kMaterialFlags & MATERIAL_FLAGS_ENABLE_SDF) == MATERIAL_FLAGS_ENABLE_SDF) {

      vec2 frag_p = GetTextureCoords();
      // offset so that 0.0, 0.0 is the center.
      frag_p -= vec2(0.5, 0.5);

      SDF_Shape shape;
      shape.shape         = uint(kSdfShape);
      shape.fill_mode     = uint(kSdfFillMode);
      shape.outline_width = kSdfOutlineWidth;
      shape.corner_radius = kSdfCornerRadius;
      shape.aspect_ratio  = kSdfAspectRatio;
      float alpha = Calculate_SDF_Alpha(shape, frag_p);

      color.rgb = kBaseColor.rgb;
      color.a = kBaseColor.a * alpha;
    }
  #endif

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
