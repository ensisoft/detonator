// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#  include <base64/base64.h>
#include "warnpop.h"

#include <functional> // for hash
#include <cmath>
#include <cstdio>

#include "base/logging.h"
#include "base/utility.h"
#include "base/threadpool.h"
#include "base/math.h"
#include "base/json.h"
#include "data/reader.h"
#include "data/writer.h"
#include "data/json.h"
#include "graphics/drawcmd.h"
#include "graphics/drawable.h"
#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/geometry.h"
#include "graphics/resource.h"
#include "graphics/transform.h"
#include "graphics/loader.h"
#include "graphics/shadersource.h"
#include "graphics/simple_shape.h"
#include "graphics/polygon_mesh.h"
#include "graphics/particle_engine.h"
#include "graphics/utility.h"

namespace gfx {

void Grid::ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const
{
    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = *env.proj_matrix;
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
}

std::string Grid::GetShaderId(const Environment&) const
{
    return "simple-2D-vertex-shader";
}

ShaderSource Grid::GetShader(const Environment&, const Device& device) const
{
    return MakeSimple2DVertexShader(device);
}

std::string Grid::GetShaderName(const Environment& env) const
{
    return "Simple2DVertexShader";
}

std::string Grid::GetGeometryId(const Environment& env) const
{
    // use the content properties to generate a name for the
    // gpu side geometry.
    size_t hash = 0;
    hash = base::hash_combine(hash, mNumVerticalLines);
    hash = base::hash_combine(hash, mNumHorizontalLines);
    hash = base::hash_combine(hash, mBorderLines);
    return std::to_string(hash);
}

bool Grid::Construct(const Environment&, Geometry::CreateArgs& create) const
{
    std::vector<Vertex2D> verts;

    const float yadvance = 1.0f / (mNumHorizontalLines + 1);
    const float xadvance = 1.0f / (mNumVerticalLines + 1);
    for (unsigned i=1; i<=mNumVerticalLines; ++i)
    {
        const float x = i * xadvance;
        const Vertex2D line[2] = {
            {{x,  0.0f}, {x, 0.0f}},
            {{x, -1.0f}, {x, 1.0f}}
        };
        verts.push_back(line[0]);
        verts.push_back(line[1]);
    }
    for (unsigned i=1; i<=mNumHorizontalLines; ++i)
    {
        const float y = i * yadvance;
        const Vertex2D line[2] = {
            {{0.0f, y*-1.0f}, {0.0f, y}},
            {{1.0f, y*-1.0f}, {1.0f, y}},
        };
        verts.push_back(line[0]);
        verts.push_back(line[1]);
    }
    if (mBorderLines)
    {
        const Vertex2D corners[4] = {
            // top left
            {{0.0f, 0.0f}, {0.0f, 0.0f}},
            // top right
            {{1.0f, 0.0f}, {1.0f, 0.0f}},

            // bottom left
            {{0.0f, -1.0f}, {0.0f, 1.0f}},
            // bottom right
            {{1.0f, -1.0f}, {1.0f, 1.0f}}
        };
        verts.push_back(corners[0]);
        verts.push_back(corners[1]);
        verts.push_back(corners[2]);
        verts.push_back(corners[3]);
        verts.push_back(corners[0]);
        verts.push_back(corners[2]);
        verts.push_back(corners[1]);
        verts.push_back(corners[3]);
    }
    auto& geometry = create.buffer;
    create.content_name = base::FormatString("Grid %1x%2", mNumVerticalLines+1, mNumHorizontalLines+1);
    create.usage = GeometryBuffer::Usage::Static;
    geometry.SetVertexBuffer(std::move(verts));
    geometry.AddDrawCmd(Geometry::DrawType::Lines);
    geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
    return true;
}

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

    constexpr const auto*  square_tile_source = R"(
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

void VertexShaderMain()
{
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
)";


    constexpr const auto* rectangle_tile_source = R"(
#version 300 es

in vec3 aTilePosition;
in vec2 aTileCorner;
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

void VertexShaderMain()
{
  // transform tile col,row index into a tile position in tile world units in the tile x,y plane
  vec3 tile = aTilePosition * kTileWorldSize + kTilePointOffset;

  // transform the tile from tile space to rendering space
  vec4 vertex = kTileCoordinateSpaceTransform * vec4(tile.xyz, 1.0);

  // pull the corner vertices apart by adding a corner offset
  // for each vertex towards some corner/direction away from the
  // center point
  vertex.xy += (aTileCorner * kTileRenderSize);

  gl_Position = kTileTransform * vertex;
  gl_Position.z = 0.0;

  vTexCoord = aTileCorner + vec2(0.5, 1.0);
  vTileData = aTileData;

  // dummy out
  vParticleAlpha = 1.0;
  vParticleRandomValue = 1.0;
}
)";
    if (shape == TileShape::Square)
        return ShaderSource::FromRawSource(square_tile_source, ShaderSource::Type::Vertex);
    else if (shape == TileShape::Rectangle)
        return ShaderSource::FromRawSource(rectangle_tile_source, ShaderSource::Type::Vertex);
    else BUG("Missing tile batch shader source.");
    return ShaderSource();
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

Drawable::Primitive TileBatch::GetPrimitive() const
{
    const auto shape = ResolveTileShape();
    if (shape == TileShape::Square)
        return Primitive::Points;
    else if (shape == TileShape::Rectangle)
        return Primitive::Triangles;
    else BUG("Unknown tile batch tile shape");
    return Primitive::Points;
}

void LineBatch2D::ApplyDynamicState(const Environment &environment, ProgramState &program, RasterState &state) const
{
    program.SetUniform("kProjectionMatrix",  *environment.proj_matrix);
    program.SetUniform("kModelViewMatrix", *environment.view_matrix * *environment.model_matrix);
}

ShaderSource LineBatch2D::GetShader(const Environment& environment, const Device& device) const
{
    return MakeSimple2DVertexShader(device);
}

std::string LineBatch2D::GetShaderId(const Environment& environment) const
{
    return "simple-2D-vertex-shader";
}

std::string LineBatch2D::GetShaderName(const Environment& environment) const
{
    return "Simple2DVertexShader";
}

std::string LineBatch2D::GetGeometryId(const Environment &environment) const
{
    return "line-buffer-2d";
}
bool LineBatch2D::Construct(const Environment& environment, Geometry::CreateArgs& create) const
{
    std::vector<Vertex2D> vertices;
    for (const auto& line : mLines)
    {
        Vertex2D a;
        // the -y hack exists because we're using the generic 2D vertex
        // shader that the shapes with triangle rasterization also use.
        a.aPosition = Vec2 { line.start.x, -line.start.y };
        Vertex2D b;
        b.aPosition = Vec2 { line.end.x, -line.end.y };
        vertices.push_back(a);
        vertices.push_back(b);
    }
    create.content_name = "2D Line Batch";
    create.usage = Geometry::Usage::Stream;
    auto& geometry = create.buffer;

    geometry.SetVertexBuffer(vertices);
    geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
    geometry.AddDrawCmd(Geometry::DrawType::Lines);
    return true;
}

void LineBatch3D::ApplyDynamicState(const Environment& environment, ProgramState& program, RasterState& state) const
{
    program.SetUniform("kProjectionMatrix",  *environment.proj_matrix);
    program.SetUniform("kModelViewMatrix", *environment.view_matrix * *environment.model_matrix);
}

ShaderSource LineBatch3D::GetShader(const Environment& environment, const Device& device) const
{
    return MakeSimple3DVertexShader(device);
}

std::string LineBatch3D::GetShaderId(const Environment& environment) const
{
    return "simple-3D-vertex-shader";
}

std::string LineBatch3D::GetShaderName(const Environment& environment) const
{
    return "Simple3DVertexShader";
}

std::string LineBatch3D::GetGeometryId(const Environment& environment) const
{
    return "line-buffer-3D";
}

bool LineBatch3D::Construct(const Environment& environment, Geometry::CreateArgs& create) const
{
    // it's also possible to draw without generating geometry by simply having
    // the two line end points as uniforms in the vertex shader and then using
    // gl_VertexID (which is not available in GL ES2) to distinguish the vertex
    // invocation and use that ID to choose the right vertex end point.
    std::vector<Vertex3D> vertices;
    for (const auto& line : mLines)
    {
        Vertex3D a;
        a.aPosition = Vec3 { line.start.x, line.start.y, line.start.z };
        Vertex3D b;
        b.aPosition = Vec3 { line.end.x, line.end.y, line.end.z };

        vertices.push_back(a);
        vertices.push_back(b);
    }

    create.content_name = "3D Line Batch";
    create.usage = Geometry::Usage::Stream;
    auto& geometry = create.buffer;

    geometry.SetVertexBuffer(vertices);
    geometry.SetVertexLayout(GetVertexLayout<Vertex3D>());
    geometry.AddDrawCmd(Geometry::DrawType::Lines);
    return true;
}

void DebugDrawableBase::ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const
{
    mDrawable->ApplyDynamicState(env, program, state);
}

ShaderSource DebugDrawableBase::GetShader(const Environment& env, const Device& device) const
{
    return mDrawable->GetShader(env, device);
}
std::string DebugDrawableBase::GetShaderId(const Environment& env) const
{
    return mDrawable->GetShaderId(env);
}
std::string DebugDrawableBase::GetShaderName(const Environment& env) const
{
    return mDrawable->GetShaderName(env);
}
std::string DebugDrawableBase::GetGeometryId(const Environment& env) const
{
    std::string id;
    id += mDrawable->GetGeometryId(env);
    id += base::ToString(mFeature);
    return id;
}

bool DebugDrawableBase::Construct(const Environment& env, Geometry::CreateArgs& create) const
{
    if (mDrawable->GetPrimitive() != Primitive::Triangles)
    {
        return mDrawable->Construct(env, create);
    }

    if (mFeature == Feature::Wireframe)
    {
        Geometry::CreateArgs temp;
        if (!mDrawable->Construct(env, temp))
            return false;

        if (temp.buffer.HasData())
        {
            GeometryBuffer wireframe;
            CreateWireframe(temp.buffer, wireframe);

            create.buffer = std::move(wireframe);
            create.content_name = "Wireframe/" + temp.content_name;
            create.content_hash = temp.content_hash;
        }
    }
    return true;
}

Drawable::Usage DebugDrawableBase::GetUsage() const
{
    return mDrawable->GetUsage();
}

size_t DebugDrawableBase::GetContentHash() const
{
    return mDrawable->GetContentHash();
}

bool Is3DShape(const Drawable& drawable) noexcept
{
    const auto type = drawable.GetType();
    if (type == Drawable::Type::Polygon)
    {
        if (const auto* instance = dynamic_cast<const PolygonMeshInstance*>(&drawable))
        {
            const auto mesh = instance->GetMeshType();
            if (mesh == PolygonMeshInstance::MeshType::Simple3D ||
                mesh == PolygonMeshInstance::MeshType::Model3D)
                return true;
        }
    }

    if (type != Drawable::Type::SimpleShape)
        return false;
    if (const auto* instance = dynamic_cast<const SimpleShapeInstance*>(&drawable))
        return Is3DShape(instance->GetShape());
    else if (const auto* instance = dynamic_cast<const SimpleShape*>(&drawable))
        return Is3DShape(instance->GetShape());
    else BUG("Unknown drawable shape type.");
}

bool Is3DShape(const DrawableClass& klass) noexcept
{
    const auto type = klass.GetType();
    if (type == Drawable::Type::Polygon)
    {
        if (const auto* polygon = dynamic_cast<const PolygonMeshClass*>(&klass))
        {
            const auto mesh = polygon->GetMeshType();
            if (mesh == PolygonMeshClass::MeshType::Simple3D ||
                mesh == PolygonMeshClass::MeshType::Model3D)
                return true;
        }
    }
    if (type != DrawableClass::Type::SimpleShape)
        return false;
    if (const auto* simple = dynamic_cast<const SimpleShapeClass*>(&klass))
        return Is3DShape(simple->GetShapeType());
    else BUG("Unknown drawable shape type");
}

std::unique_ptr<Drawable> CreateDrawableInstance(const std::shared_ptr<const DrawableClass>& klass)
{
    // factory function based on type switching.
    // Alternative way would be to have a virtual function in the DrawableClass
    // but this would have 2 problems: creating based shared_ptr of the drawable
    // class would require shared_from_this which I consider to be bad practice and
    // it'd cause some problems at some point.
    // secondly it'd create a circular dependency between class and the instance types
    // which is going to cause some problems at some point.
    const auto type = klass->GetType();

    if (type == DrawableClass::Type::SimpleShape)
        return std::make_unique<SimpleShapeInstance>(std::static_pointer_cast<const SimpleShapeClass>(klass));
    else if (type == DrawableClass::Type::ParticleEngine)
        return std::make_unique<ParticleEngineInstance>(std::static_pointer_cast<const ParticleEngineClass>(klass));
    else if (type == DrawableClass::Type::Polygon)
        return std::make_unique<PolygonMeshInstance>(std::static_pointer_cast<const PolygonMeshClass>(klass));
    else BUG("Unhandled drawable class type");
    return nullptr;
}

} // namespace
