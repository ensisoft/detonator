R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen

#version 300 es

// @attributes
in vec3 aTilePosition;
in vec2 aTileData;

// @uniforms
uniform mat4 kTileTransform;
uniform mat4 kTileCoordinateSpaceTransform;

uniform vec3 kTileWorldSize;
uniform vec3 kTilePointOffset;
uniform vec2 kTileRenderSize;

// @varyings
out vec2 vTileData;

void VertexShaderMain() {
    // transform tile row,col index into a tile position in units in the x,y plane,
    vec3 tile = aTilePosition * kTileWorldSize + kTilePointOffset;

    vec4 vertex = kTileCoordinateSpaceTransform * vec4(tile.xyz, 1.0);

    vs_out.clip_position = kTileTransform * vertex;
    vs_out.clip_position.z = 0.0;
    vs_out.point_size = kTileRenderSize.x;

    vTileData = aTileData;
}

)CPP_RAW_STRING"