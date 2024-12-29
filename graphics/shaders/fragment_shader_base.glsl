R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// shader base functionality

// @varyings
#ifndef DRAW_POINTS
  // Incoming texture coordinate from vertex shader.
  in vec2 vTexCoord;
#endif

vec2 GetTextureCoords() {
#ifdef DRAW_POINTS
    return gl_PointCoord;
#else
    return vTexCoord;
#endif
}

)CPP_RAW_STRING"