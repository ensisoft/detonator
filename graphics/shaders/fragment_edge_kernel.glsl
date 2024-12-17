R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen

#version 100

precision highp float;

uniform vec2 kTextureSize;
uniform vec4 kEdgeColor;
uniform sampler2D kSrcTexture;

varying vec2 vTexCoord;

vec4 texel(vec2 offset)
{
   float w = 1.0 / kTextureSize.x;
   float h = 1.0 / kTextureSize.y;

   vec2 coord = vTexCoord + offset;
   if (coord.x < 2.0*w || coord.x > (1.0-2.0*w))
      return vec4(0.0);
   if (coord.y < 2.0*h || coord.y > (1.0-2.0*h))
      return vec4(0.0);

   return texture2D(kSrcTexture, coord);
}

void sample(inout vec4[9] samples)
{
   // texel size in normalized units.
   float w = 1.0 / kTextureSize.x;
   float h = 1.0 / kTextureSize.y;

   samples[0] = texel(vec2( -w, -h));
   samples[1] = texel(vec2(0.0, -h));
   samples[2] = texel(vec2(  w, -h));
   samples[3] = texel(vec2( -w, 0.0));
   samples[4] = texel(vec2(0.0, 0.0));
   samples[5] = texel(vec2(  w, 0.0));
   samples[6] = texel(vec2( -w, h));
   samples[7] = texel(vec2(0.0, h));
   samples[8] = texel(vec2(  w, h));
}

void main() {

   vec4 n[9];
   sample(n);

   vec4 sobel_edge_h = n[2] + (2.0*n[5]) + n[8] - (n[0] + (2.0*n[3]) + n[6]);
   vec4 sobel_edge_v = n[0] + (2.0*n[1]) + n[2] - (n[6] + (2.0*n[7]) + n[8]);
   vec4 sobel = sqrt((sobel_edge_h * sobel_edge_h) + (sobel_edge_v * sobel_edge_v));

   //gl_FragColor = vec4(1.0 - sobel.rgb, 1.0);
   gl_FragColor = vec4(kEdgeColor.rgb, kEdgeColor * sobel.a);

   //gl_FragColor = vec4(1.0) * (1.0 - sobel.a);

}

)CPP_RAW_STRING"