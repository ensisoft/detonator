R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// Color gradient shader that blends between
// 4 different colors.

#version 300 es

precision highp float;

// @uniforms
uniform uint kMaterialFlags;

// The gradient top left color value.
uniform vec4 kGradientColor0;
// The gradient top right color value.
uniform vec4 kGradientColor1;
// The gradient bottom left color value.
uniform vec4 kGradientColor2;
// The gradient bottom right color value.
uniform vec4 kGradientColor3;
// The gradient X and Y axis mixing/blending weights.
uniform vec2 kGradientWeight;
// The gradient type.
uniform uint kGradientType;
// the gamma (in)correction value. Typically 2.2 for
// the incorrect result that users expect and 1.0 for
// the physically correct mix but what users don't want.
uniform float kGradientGamma;

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

vec4 GammaEncode(vec4 color) {
    float g = kGradientGamma;
    return vec4(pow(color.rgb, vec3(1.0/g)), color.a);
}

vec4 GammaDecode(vec4 color) {
  float g = kGradientGamma;
  return vec4(pow(color.rgb, vec3(g)), color.a);
}

vec4 MixGradient(vec2 coords) {

  vec4 color;

  if (kGradientType == GRADIENT_TYPE_BILINEAR) {
      vec4 top = mix(GammaEncode(kGradientColor0), GammaEncode(kGradientColor1), coords.x);
      vec4 bot = mix(GammaEncode(kGradientColor2), GammaEncode(kGradientColor3), coords.x);
      color = mix(top, bot, coords.y);

  } else if (kGradientType == GRADIENT_TYPE_RADIAL) {
      float distance_from_center = length(vec2(0.5, 0.5) - coords);
      distance_from_center = min(distance_from_center, 1.0);
      color = mix(GammaEncode(kGradientColor0), GammaEncode(kGradientColor1), distance_from_center);

  } else if (kGradientType == GRADIENT_TYPE_CONICAL) {
      float angle = atan(coords.y - 0.5, coords.x - 0.5); // -pi to pi
      float mixer = (angle + PI) / (2.0 * PI);
      color = mix(GammaEncode(kGradientColor0), GammaEncode(kGradientColor1), mixer);
  }

  return GammaDecode(color);
}

#ifndef CUSTOM_FRAGMENT_MAIN
void FragmentShaderMain() {

  vec2 texture_coords = GetTextureCoords();

  vec2 gradient_coords;
  gradient_coords = (texture_coords - kGradientWeight) + vec2(0.5, 0.5);
  gradient_coords = clamp(gradient_coords, vec2(0.0, 0.0), vec2(1.0, 1.0));

  vec4 gradient_color  = MixGradient(gradient_coords);

  #if defined(ENABLE_SDF_SUPPORT)
    if ((kMaterialFlags & MATERIAL_FLAGS_ENABLE_SDF) == MATERIAL_FLAGS_ENABLE_SDF) {
      vec2 frag_p = texture_coords;

      frag_p -= vec2(0.5, 0.5);

      SDF_Shape shape;
      shape.shape         = uint(kSdfShape);
      shape.fill_mode     = uint(kSdfFillMode);
      shape.outline_width = kSdfOutlineWidth;
      shape.corner_radius = kSdfCornerRadius;
      shape.aspect_ratio  = kSdfAspectRatio;
      float alpha = Calculate_SDF_Alpha(shape, frag_p);

      gradient_color.a *= alpha;
    }
  #endif

  fs_out.color.rgb = gradient_color.rgb;
  fs_out.color.a   = gradient_color.a;

#ifdef GEOMETRY_IS_PARTICLES
  fs_out.color.a *= vParticleAlpha;
#endif

  fs_out.flags = kMaterialFlags;
}
#endif // CUSTOM_FRAGMENT_MAIN

)CPP_RAW_STRING"
