R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen

#version 300 es

// @attributes
in vec3 aTilePosition;
in vec2 aTileCorner;
in vec2 aTileData;

// @uniforms
uniform mat4 kTileViewTransform;
uniform mat4 kTileCoordinateSpaceTransform;
uniform mat4 kProjectionMatrix;

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

    vec4 view_vertex = kTileViewTransform * vertex;
    view_vertex.z = 0.0;

    vs_out.view_position = view_vertex;
    vs_out.clip_position = kProjectionMatrix * view_vertex;
    vs_out.clip_position.z = 0.0;

    if (vs_out.need_tbn) {
        // with point rendering we can simply hardcode the
        // tangent vectors since the rasterized surface is always
        // parallel to the projection plane and in view coordinates
        // the only thing changes is the translation
        const vec3 normal = vec3(0.0, 0.0, 1.0);
        const vec3 tangent = vec3(1.0, 0.0, 0.0);
        const vec3 bitangent = vec3(0.0, 1.0, 0.0);
        vs_out.view_normal    = normal;
        vs_out.view_tangent   = tangent;
        vs_out.view_bitangent = bitangent;
        vs_out.have_tbn = true;
    }

    vTexCoord = aTileCorner + vec2(0.5, 1.0);
    vTileData = aTileData;
}

)CPP_RAW_STRING"
