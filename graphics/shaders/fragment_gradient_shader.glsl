R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// Color gradient shader that blends between
// 4 different colors.

#version 300 es

precision highp float;

// The gradient top left color value.
uniform vec4 kColor0;
// The gradient top right color value.
uniform vec4 kColor1;
// The gradient bottom left color value.
uniform vec4 kColor2;
// The gradient bottom right color value.
uniform vec4 kColor3;
// The gradient X and Y axis mixing/blending weights.
uniform vec2 kOffset;

// Incoming texture coordinate from vertex shader.
in vec2 vTexCoord;

// Incoming per particle alpha value.
in float vParticleAlpha;

vec4 MixGradient(vec2 coords) {
  vec4 top = mix(kColor0, kColor1, coords.x);
  vec4 bot = mix(kColor2, kColor3, coords.x);
  vec4 color = mix(top, bot, coords.y);
  return color;
}

void FragmentShaderMain() {

  vec2 coords = GetTextureCoords();
  coords = (coords - kOffset) + vec2(0.5, 0.5);
  coords = clamp(coords, vec2(0.0, 0.0), vec2(1.0, 1.0));
  vec4 color  = MixGradient(coords);

  fs_out.color.rgb = vec3(pow(color.rgb, vec3(2.2)));
  fs_out.color.a   = color.a * vParticleAlpha;
}

)CPP_RAW_STRING"