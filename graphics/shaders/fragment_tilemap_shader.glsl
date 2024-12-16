R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// Tilemap shader that renders based on a tilemap texture with
// regular grid layout. Each tile rectangle is indicated by
// an incoming tile index that is then used to sample the
// texture by mapping the the tile index to a tile row and col
// and then mapping those to actual texture coordinates.

#version 300 es

precision highp float;

// Tilemap texture
uniform sampler2D kTexture;

// The texture sub-rectangle inside a potentially
// bigger potentially texture. Used when dealing with
// textures packed inside other textures (atlasses)
uniform vec4 kTextureBox;

// The base color used to modulate the outgoing color value.
uniform vec4 kBaseColor;

// Alpha cutoff for controllling when to discard the fragment.
// This is useful for using "opaque" rendering with alpha
// cutout sprites.
uniform float kAlphaCutoff;

// Current material time in seconds.
uniform float kTime;

// Specific tile index or 0.0 to use the per vertex tile data.
// This is used to visualize the tile in cases where there's
// no actual tile vertex data.
uniform float kTileIndex;

// Tile size (width, height) in pixels (texels).
uniform vec2 kTileSize;

// The x,y offset into the texture where the actual tile
// data begins.
uniform vec2 kTileOffset;

// The extra space in pixels around each tile.
uniform vec2 kTilePadding;

// Runtime flag to indicate GL_POINTS and gl_PointCoord
// for texture coordinates.
uniform float kRenderPoints;

// Incoming per vertex texture coordinates
in vec2 vTexCoord;

// Incoming per vertex tile data.
in vec2 vTileData;

float GetTileIndex() {
    // to help debugging the material in the editor we're checking the flag
    // here whether we're editing or not and then either return a tile index
    // based on a uniform or a "real" tile index based on tile data.
    if (kTileIndex > 0.0)
        return kTileIndex - 1.0;
    return vTileData.x;
}

void FragmentShaderMain() {
    // the tile rendering can provide geometry also through GL_POINTS.
    // that means we must then use gl_PointCoord as the texture coordinates
    // for the fragment.
    vec2 frag_texture_coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);

    // apply texture box transformation
    vec2 texture_box_size      = kTextureBox.zw;
    vec2 texture_box_translate = kTextureBox.xy;

    vec2 texture_size = vec2(textureSize(kTexture, 0));

    vec2 tile_texture_offset   = kTileOffset / texture_size;
    vec2 tile_texture_size     = kTileSize / texture_size;
    vec2 tile_texture_padding  = kTilePadding / texture_size;
    vec2 tile_texture_box_size = tile_texture_size + (tile_texture_padding * 2.0);

    vec2 tile_map_size = texture_box_size - tile_texture_offset;
    vec2 tile_map_dims = tile_map_size / tile_texture_box_size; // rows and cols

    int tile_index = int(GetTileIndex());
    int tile_cols = int(tile_map_dims.x);
    int tile_rows = int(tile_map_dims.y);
    int tile_row = tile_index / tile_cols;
    int tile_col = tile_index - (tile_row * tile_cols);

    // build the final texture coordinate by successively adding offsets
    // in order to map the frag coord to the right texture position inside
    // the tile, inside the tile region of the tile-texture inside the
    // texture box inside the texture atlas.
    vec2 texture_coords = vec2(0.0, 0.0);

    // add texture box offset inside the texture.
    texture_coords += texture_box_translate;

    // add the tile offset inside the texture box to get to the tiles
    texture_coords += tile_texture_offset;

    // add the tile base coord to get to the top-left of
    // the tile coordinate.
    texture_coords += vec2(float(tile_col), float(tile_row)) * tile_texture_box_size;

    texture_coords += tile_texture_padding;

    // add the frag coordinate relative to the tile's top left
    texture_coords += tile_texture_size * frag_texture_coords;

    vec4 texture_sample = texture(kTexture, texture_coords);

    // produce color value.
    vec4 color = texture_sample * kBaseColor;

    if (color.a <= kAlphaCutoff)
        discard;

    fs_out.color = color;
}

)CPP_RAW_STRING"