R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen

#version 100
precision highp float;
varying vec2 vTexCoord;
uniform vec2 kTextureSize;
uniform sampler2D kTexture;

uniform int kDirection;

void main() {
  vec2 texel_size = vec2(1.0, 1.0) / vec2(256.0);

  // The total sum of weights w=weights[0] + 2*(weights[1] + weights[2] + ... weights[n])
  // W should then be approximately 1.0
  float weights[5];
  weights[0] = 0.227027;
  weights[1] = 0.1945946;
  weights[2] = 0.1216216;
  weights[3] = 0.054054;
  weights[4] = 0.016216;

  vec4 color = texture2D(kTexture, vTexCoord) * weights[0];

  if (kDirection == 0)
  {
    for (int i=1; i<5; ++i)
    {
      float left  = vTexCoord.x - float(i) * texel_size.x;
      float right = vTexCoord.x + float(i) * texel_size.x;
      float y = vTexCoord.y;
      vec4 left_sample  = texture2D(kTexture, vec2(left, y)) * weights[i];
      vec4 right_sample = texture2D(kTexture, vec2(right, y)) * weights[i];
      color += left_sample;
      color += right_sample;
    }
  }
  else if (kDirection == 1)
  {
    for (int i=1; i<5; ++i)
    {
      float below = vTexCoord.y - float(i) * texel_size.y;
      float above = vTexCoord.y + float(i) * texel_size.y;
      float x = vTexCoord.x;
      vec4 below_sample = texture2D(kTexture, vec2(x, below)) * weights[i];
      vec4 above_sample = texture2D(kTexture, vec2(x, above)) * weights[i];
      color += below_sample;
      color += above_sample;
    }
  }
  gl_FragColor = color;
}

)CPP_RAW_STRING"