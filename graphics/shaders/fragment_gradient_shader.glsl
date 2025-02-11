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

uniform uint kGradientType;

// @varyings
#ifdef GEOMETRY_IS_PARTICLES
  // Incoming per particle alpha value.
  in float vParticleAlpha;
#endif

vec4 MixGradient(vec2 coords) {

  vec4 color;

  if (kGradientType == GRADIENT_TYPE_BILINEAR) {
    vec4 top = mix(kGradientColor0, kGradientColor1, coords.x);
    vec4 bot = mix(kGradientColor2, kGradientColor3, coords.x);
    color = mix(top, bot, coords.y);
  }

  return color;
}

void FragmentShaderMain() {

  vec2 coords = GetTextureCoords();
  coords = (coords - kGradientWeight) + vec2(0.5, 0.5);
  coords = clamp(coords, vec2(0.0, 0.0), vec2(1.0, 1.0));
  vec4 color  = MixGradient(coords);

  fs_out.color.rgb = vec3(pow(color.rgb, vec3(2.2)));
  fs_out.color.a   = color.a;

#ifdef GEOMETRY_IS_PARTICLES
  fs_out.color.a *= vParticleAlpha;
#endif
  fs_out.flags = kMaterialFlags;
}

)CPP_RAW_STRING"
