R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen

#version 300 es

in vec3 aTilePosition;
in vec2 aTileData;

uniform mat4 kTileTransform;
uniform mat4 kTileCoordinateSpaceTransform;

uniform vec3 kTileWorldSize;
uniform vec3 kTilePointOffset;
uniform vec2 kTileRenderSize;

out float vParticleAlpha;
out float vParticleRandomValue;
out vec2 vTileData;
out vec2 vTexCoord;

void VertexShaderMain() {
  // transform tile row,col index into a tile position in units in the x,y plane,
  vec3 tile = aTilePosition * kTileWorldSize + kTilePointOffset;

  vec4 vertex = kTileCoordinateSpaceTransform * vec4(tile.xyz, 1.0);

  gl_Position = kTileTransform * vertex;
  gl_Position.z = 0.0;
  gl_PointSize = kTileRenderSize.x;

  vTileData = aTileData;
  // dummy, this shader requires gl_PointCoord
  vTexCoord = vec2(0.0, 0.0);

  // dummy out.
  vParticleAlpha = 1.0;
  vParticleRandomValue = 1.0;
}

)CPP_RAW_STRING"