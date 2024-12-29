R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen

#version 300 es

// @attributes
in vec3 aTilePosition;
in vec2 aTileCorner;
in vec2 aTileData;

// @uniforms
uniform mat4 kTileTransform;
uniform mat4 kTileCoordinateSpaceTransform;

uniform vec3 kTileWorldSize;
uniform vec3 kTilePointOffset;
uniform vec2 kTileRenderSize;

// @varyings
out vec2 vTileData;
out vec2 vTexCoord;

void VertexShaderMain() {
  // transform tile col,row index into a tile position in tile world units in the tile x,y plane
  vec3 tile = aTilePosition * kTileWorldSize + kTilePointOffset;

  // transform the tile from tile space to rendering space
  vec4 vertex = kTileCoordinateSpaceTransform * vec4(tile.xyz, 1.0);

  // pull the corner vertices apart by adding a corner offset
  // for each vertex towards some corner/direction away from the
  // center point
  vertex.xy += (aTileCorner * kTileRenderSize);

  vs_out.clip_position = kTileTransform * vertex;
  vs_out.clip_position.z = 0.0;

  vTexCoord = aTileCorner + vec2(0.5, 1.0);
  vTileData = aTileData;
}

)CPP_RAW_STRING"