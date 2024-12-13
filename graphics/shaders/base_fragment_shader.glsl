R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// shader base functionality

// Runtime flag to indicate GL_POINTS and gl_PointCoord
// for texture coordinates.
uniform float kRenderPoints;

vec2 GetTextureCoords() {
#ifdef STATIC_SHADER_SOURCE
  #ifdef DRAW_POINTS
    return gl_PointCoord;
  #else
    return vTexCoord;
  #endif
#else
  return mix(vTexCoord, gl_PointCoord, kRenderPoints);
#endif
}

)CPP_RAW_STRING"