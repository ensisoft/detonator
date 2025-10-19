// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include <cstring> // for memcpy

#include "base/assert.h"
#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "graphics/drawable.h"
#include "graphics/drawcmd.h"
#include "graphics/polygon_mesh.h"
#include "graphics/tool/polygon.h"

namespace gfx {
namespace tool {

template<typename Vertex>
void PolygonBuilder<Vertex>::ClearAll() noexcept
{
    mVertices.clear();
    mDrawCommands.clear();
}

template<typename Vertex>
void PolygonBuilder<Vertex>::ClearDrawCommands() noexcept
{
    mDrawCommands.clear();
}

template<typename Vertex>
void PolygonBuilder<Vertex>::ClearVertices() noexcept
{
    mVertices.clear();
}

template<typename Vertex>
void PolygonBuilder<Vertex>::AddVertices(const std::vector<Vertex>& vertices)
{
    std::copy(std::begin(vertices), std::end(vertices), std::back_inserter(mVertices));
}

template<typename Vertex>
void PolygonBuilder<Vertex>::AddVertices(std::vector<Vertex>&& vertices)
{
    std::move(std::begin(vertices), std::end(vertices), std::back_inserter(mVertices));
}

template<typename Vertex>
void PolygonBuilder<Vertex>::AddVertices(const Vertex* vertices, size_t num_vertices)
{
    for (size_t i=0; i<num_vertices; ++i)
        mVertices.push_back(vertices[i]);
}

template<typename Vertex>
void PolygonBuilder<Vertex>::AddDrawCommand(const DrawCommand& cmd)
{
    mDrawCommands.push_back(cmd);
}

template<typename Vertex>
void PolygonBuilder<Vertex>::UpdateVertex(const Vertex& vert, size_t index)
{
    ASSERT(index < mVertices.size());
    mVertices[index] = vert;
}

template<typename Vertex>
void PolygonBuilder<Vertex>::UpdateVertex(const void* vertex, size_t index)
{
    UpdateVertex(*static_cast<const Vertex*>(vertex), index);
}

template<typename Vertex>
void PolygonBuilder<Vertex>::EraseVertex(size_t index)
{
    ASSERT(index < mVertices.size());
    auto it = mVertices.begin();
    std::advance(it, index);
    mVertices.erase(it);
    // remove the vertex from the draw commands.
    for (size_t i=0; i<mDrawCommands.size();)
    {
        auto& cmd = mDrawCommands[i];
        if (index >= cmd.offset && index < cmd.offset + cmd.count) {
            if (--cmd.count == 0)
            {
                auto it = mDrawCommands.begin();
                std::advance(it, i);
                mDrawCommands.erase(it);
                continue;
            }
        }
        else if (index < cmd.offset)
            cmd.offset--;
        ++i;
    }
}

template<typename Vertex>
void PolygonBuilder<Vertex>::InsertVertex(const Vertex& vertex, size_t cmd_index, size_t index)
{
    ASSERT(cmd_index < mDrawCommands.size());
    ASSERT(index <= mDrawCommands[cmd_index].count);

    // figure out the index where the put the new vertex in the vertex
    // array.
    auto& cmd = mDrawCommands[cmd_index];
    cmd.count = cmd.count + 1;

    const auto vertex_index = cmd.offset + index;
    mVertices.insert(mVertices.begin() + vertex_index, vertex);

    for (size_t i=0; i<mDrawCommands.size(); ++i)
    {
        if (i == cmd_index)
            continue;
        auto& cmd = mDrawCommands[i];
        if (vertex_index <= cmd.offset)
            cmd.offset++;
    }
}

template<typename Vertex>
void PolygonBuilder<Vertex>::InsertVertex(const void* vertex, size_t cmd_index, size_t index)
{
    InsertVertex(*static_cast<const Vertex*>(vertex), cmd_index, index);
}

template<typename Vertex>
void PolygonBuilder<Vertex>::AppendVertex(const void* vertex)
{
    mVertices.push_back(*static_cast<const Vertex*>(vertex));
}

template<typename Vertex>
void PolygonBuilder<Vertex>::UpdateDrawCommand(const DrawCommand& cmd, size_t index) noexcept
{
    ASSERT(index < mDrawCommands.size());
    mDrawCommands[index] = cmd;
}

template<typename Vertex>
size_t PolygonBuilder<Vertex>::FindDrawCommand(size_t vertex_index) const noexcept
{
    for (size_t i=0; i<mDrawCommands.size(); ++i)
    {
        const auto& cmd = mDrawCommands[i];
        // first and last index in the vertex buffer
        // for this draw command.
        const auto vertex_index_first = cmd.offset;
        const auto vertex_index_last  = cmd.offset + cmd.count;
        if (vertex_index >= vertex_index_first && vertex_index < vertex_index_last)
            return i;
    }
    BUG("no draw command found.");
}

template<typename Vertex>
size_t PolygonBuilder<Vertex>::GetContentHash() const noexcept
{
    size_t hash = 0;
    for (const auto& vertex : mVertices)
    {
        hash = base::hash_combine(hash, vertex);
    }
    for (const auto& cmd : mDrawCommands)
    {
        hash = base::hash_combine(hash, cmd);
    }
    return hash;
}


template<typename Vertex>
void PolygonBuilder<Vertex>::IntoJson(data::Writer& writer) const
{
    const VertexStream  vertex_stream(gfx::GetVertexLayout<Vertex>(), mVertices);
    const CommandStream command_stream(mDrawCommands);

    vertex_stream.IntoJson(writer);
    command_stream.IntoJson(writer);

    writer.Write("static", mStatic);
}

template<typename Vertex>
bool PolygonBuilder<Vertex>::FromJson(const data::Reader& reader)
{
    bool ok = true;

    VertexBuffer vertex_buffer;

    CommandBuffer command_buffer(&mDrawCommands);

    ok &= vertex_buffer.FromJson(reader);
    ok &= command_buffer.FromJson(reader);
    ok &= reader.Read("static", &mStatic);

    mVertices = vertex_buffer.CopyBuffer<Vertex>();
    return ok;
}

template<typename Vertex>
void PolygonBuilder<Vertex>::BuildPoly(PolygonMeshClass& polygon) const
{
    polygon.ClearContent();

    const auto count = mVertices.size();
    const auto bytes = count * sizeof(Vertex);

    if (count)
    {
        std::vector<uint8_t> buffer;
        buffer.resize(bytes);
        std::memcpy(buffer.data(), mVertices.data(), bytes);

        polygon.SetVertexBuffer(std::move(buffer));
    }

    polygon.SetContentHash(GetContentHash());
    polygon.SetVertexLayout(gfx::GetVertexLayout<Vertex>());
    polygon.SetCommandBuffer(mDrawCommands);
    polygon.SetStatic(mStatic);
}

template<typename Vertex>
void PolygonBuilder<Vertex>::InitFrom(const PolygonMeshClass& polygon)
{
    mDrawCommands.clear();
    mVertices.clear();

    if (polygon.HasInlineData())
    {
        ASSERT(*polygon.GetVertexLayout() == gfx::GetVertexLayout<Vertex>());

        const void* ptr = polygon.GetVertexBufferPtr();
        const auto bytes = polygon.GetVertexBufferSize();
        const auto count = bytes / sizeof(Vertex);

        mVertices.resize(count);
        if (count)
            std::memcpy(mVertices.data(), ptr, bytes);

        for (size_t i=0; i<polygon.GetDrawCmdCount(); ++i)
        {
            mDrawCommands.push_back(*polygon.GetDrawCmd(i));
        }
    }
    mStatic = polygon.IsStatic();
}

template class PolygonBuilder<Vertex2D>;
template class PolygonBuilder<Perceptual3DVertex>;
template class PolygonBuilder<ShardVertex2D>;

} // namespace
} // namespace