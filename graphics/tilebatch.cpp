// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "base/format.h"
#include "graphics/tilebatch.h"
#include "graphics/program.h"
#include "graphics/shader_source.h"
#include "graphics/geometry.h"

namespace gfx
{

void TileBatch::ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& raster) const
{
    const auto pixel_scale = std::min(env.pixel_ratio.x, env.pixel_ratio.y);

    const auto shape = ResolveTileShape();

    // Choose a point on the tile for projecting the tile onto the
    // rendering surface.

    // if the tile shape is square we're rendering point sprites which
    // are always centered around the vertex when rasterized by OpenGL.
    // This means that the projection plays a role when choosing the vertex
    // around which to rasterize the point when using point sprites.
    //
    //  a) square + dimetric
    //    In this case the tile's top left corner maps directly to the
    //    center of the square tile when rendered.
    //
    //  b) square + axis aligned.
    //    In this case the center of the tile yields the center of the
    //    square when projected.
    //
    glm::vec3 tile_point_offset = {0.0f, 0.0f, 0.0f};
    if (mProjection == Projection::AxisAligned && shape == TileShape::Square)
        tile_point_offset = mTileWorldSize * glm::vec3{0.5f, 0.5f, 0.0f};
    else if (mProjection == Projection::Dimetric && shape == TileShape::Rectangle)
        tile_point_offset = mTileWorldSize * glm::vec3{1.0f, 1.0f, 0.0f};  // bottom right corner is the basis for the billboard
    else if (mProjection == Projection::AxisAligned && shape == TileShape::Rectangle)
        tile_point_offset = mTileWorldSize * glm::vec3{0.5f, 1.0f, 0.0f}; // middle of the bottom edge

    glm::vec2 tile_render_size = mTileRenderSize;
    if (shape == TileShape::Square)
        tile_render_size *= pixel_scale;

    program.SetUniform("kTileWorldSize", mTileWorldSize);
    // This is the offset in units to add to the top left tile corner (row, col)
    // for projecting the tile into the render surface coordinates.
    program.SetUniform("kTilePointOffset", tile_point_offset);
    program.SetUniform("kTileRenderSize", tile_render_size);
    program.SetUniform("kTileTransform", *env.proj_matrix * *env.view_matrix);
    program.SetUniform("kTileCoordinateSpaceTransform", *env.model_matrix);
}

ShaderSource TileBatch::GetShader(const Environment& env, const Device& device) const
{
    // the shader uses dummy varyings vParticleAlpha, vParticleRandomValue
    // and vTexCoord. Even though we're now rendering GL_POINTS this isn't
    // a particle vertex shader. However, if a material shader refers to those
    // varyings we might get GLSL program build errors on some platforms.

    const auto shape = ResolveTileShape();

    static constexpr const char*  square_tile_source = {
#include "shaders/vertex_tilebatch_point_shader.glsl"
    };
    static constexpr const char* rectangle_tile_source = {
#include "shaders/vertex_tilebatch_quad_shader.glsl"
    };

    const char* src = nullptr;
    if (shape == TileShape::Square)
        src = square_tile_source;
    else if (shape == TileShape::Rectangle)
        src = rectangle_tile_source;
    else BUG("Missing tile batch shader source.");

    ShaderSource source;
    source.SetType(ShaderSource::Type::Vertex);
    source.SetVersion(ShaderSource::Version::GLSL_300);
    source.LoadRawSource(src);
    source.AddShaderName("Tile Vertex Shader");
    source.AddDebugInfo("Tile shape", base::ToString(shape));
    return source;
}

std::string TileBatch::GetShaderId(const Environment& env) const
{
    const auto shape = ResolveTileShape();
    if (shape == TileShape::Square)
        return "square-tile-batch-program";
    else if (shape == TileShape::Rectangle)
        return "rectangle-tile-batch-program";
    else BUG("Missing tile batch shader id.");
    return "";
}

std::string TileBatch::GetShaderName(const Environment& env) const
{
    const auto shape = ResolveTileShape();

    if (shape == TileShape::Square)
        return "SquareTileBatchShader";
    else if (shape == TileShape::Rectangle)
        return "RectangleTileBatchShader";
    else BUG("Missing tile batch shader name.");
    return "";
}

std::string TileBatch::GetGeometryId(const Environment& env) const
{
    return "tile-buffer";
}

bool TileBatch::Construct(const Environment& env, Geometry::CreateArgs& create) const
{
    const auto shape = ResolveTileShape();
    if (shape == TileShape::Square)
    {
        using TileVertex = Tile;
        static const VertexLayout layout(sizeof(TileVertex), {
            {"aTilePosition", 0, 4, 0, offsetof(TileVertex, pos)},
            {"aTileData",     0, 2, 0, offsetof(TileVertex, data)}
        });

        create.content_name = "TileBatch";
        create.usage = Geometry::Usage ::Stream;
        auto& geometry = create.buffer;

        geometry.SetVertexBuffer(mTiles);
        geometry.SetVertexLayout(layout);
        geometry.AddDrawCmd(Geometry::DrawType::Points);
    }
    else if (shape == TileShape::Rectangle)
    {
        struct TileVertex {
            Vec3 position;
            Vec2 data;
            Vec2 corner;
        };
        static const VertexLayout layout(sizeof(TileVertex), {
            {"aTilePosition", 0, 3, 0, offsetof(TileVertex, position)},
            {"aTileData",     0, 2, 0, offsetof(TileVertex, data)},
            {"aTileCorner",   0, 2, 0, offsetof(TileVertex, corner)},
        });
        std::vector<TileVertex> vertices;
        vertices.reserve(6 * mTiles.size());
        for (const auto& tile : mTiles)
        {
            const TileVertex top_left  = {tile.pos, tile.data, {-0.5f, -1.0f}};
            const TileVertex top_right = {tile.pos, tile.data, { 0.5f, -1.0f}};
            const TileVertex bot_left  = {tile.pos, tile.data, {-0.5f,  0.0f}};
            const TileVertex bot_right = {tile.pos, tile.data, { 0.5f,  0.0f}};
            vertices.push_back(top_left);
            vertices.push_back(bot_left);
            vertices.push_back(bot_right);

            vertices.push_back(top_left);
            vertices.push_back(bot_right);
            vertices.push_back(top_right);
        }
        create.content_name = "TileBatch";
        create.usage = Geometry::Usage::Stream;
        auto& geometry = create.buffer;

        geometry.SetVertexBuffer(vertices);
        geometry.SetVertexLayout(layout);
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    }
    else BUG("Unknown tile shape!");
    return true;
}


Drawable::DrawPrimitive TileBatch::GetDrawPrimitive() const
{
    const auto shape = ResolveTileShape();
    if (shape == TileShape::Square)
        return DrawPrimitive::Points;
    else if (shape == TileShape::Rectangle)
        return DrawPrimitive::Triangles;
    else BUG("Unknown tile batch tile shape");
    return DrawPrimitive::Points;
}

} // namespace