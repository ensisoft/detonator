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
#include "base/logging.h"
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
    std::vector<EditVertex> tmp;
    for (const auto& vertex : vertices)
    {
        EditVertex edit_vertex;
        edit_vertex.vertex = vertex;
        edit_vertex.flags  = 0;
        tmp.push_back(edit_vertex);
    }

    std::copy(std::begin(tmp), std::end(tmp), std::back_inserter(mVertices));
}

template<typename Vertex>
void PolygonBuilder<Vertex>::AddVertices(std::vector<Vertex>&& vertices)
{
    std::vector<EditVertex> tmp;
    for (const auto& vertex : vertices)
    {
        EditVertex edit_vertex;
        edit_vertex.vertex = vertex;
        edit_vertex.flags  = 0;
        tmp.push_back(edit_vertex);
    }

    std::move(std::begin(tmp), std::end(tmp), std::back_inserter(mVertices));
}

template<typename Vertex>
void PolygonBuilder<Vertex>::AddVertices(const Vertex* vertices, size_t num_vertices)
{
    for (size_t i=0; i<num_vertices; ++i)
    {
        EditVertex edit_vertex;
        edit_vertex.vertex = vertices[i];
        edit_vertex.flags  = 0;
        mVertices.push_back(edit_vertex);
    }
}

template<typename Vertex>
void PolygonBuilder<Vertex>::AddDrawCommand(const DrawCommand& cmd)
{
    mDrawCommands.push_back(cmd);
}

template<typename Vertex>
void PolygonBuilder<Vertex>::UpdateVertex(const Vertex& vertex, size_t index)
{
    ASSERT(index < mVertices.size());
    mVertices[index].vertex = vertex;
}

template<typename Vertex>
void PolygonBuilder<Vertex>::UpdateVertex(const void* vertex, size_t index)
{
    ASSERT(index < mVertices.size());
    mVertices[index].vertex = *static_cast<const Vertex*>(vertex);
}

template<typename Vertex>
void PolygonBuilder<Vertex>::EraseVertex(size_t index)
{
    base::SafeErase(mVertices, index);

    // remove the vertex from the draw commands.
    for (size_t i=0; i<mDrawCommands.size();)
    {
        auto& cmd = mDrawCommands[i];

        // if this is the command that contains the vertex being
        // erased then reduce the primitive count by one vertex.
        if (index >= cmd.offset && index < cmd.offset + cmd.count)
        {
            ASSERT(cmd.count > 0);
            // if the last vertex got erased then delete the whole command
            if (--cmd.count == 0)
            {
                base::SafeErase(mDrawCommands, i);
                continue;
            }
        }
        else if (index < cmd.offset)
        {
            // adjust the command offset of a command that
            // draws vertices later in the vertex array. they must all be
            // shifted down a notch now.
            cmd.offset--;
        }
        ++i;
    }
}

template<typename Vertex>
void PolygonBuilder<Vertex>::EraseCommand(const size_t index)
{
    ASSERT(index < mDrawCommands.size());

    const auto cmd = mDrawCommands[index];
    const auto vertex_it_beg = mVertices.begin() + cmd.offset;
    const auto vertex_it_end = mVertices.begin() + cmd.offset + cmd.count;
    const auto vertex_count = cmd.count;

    mVertices.erase(vertex_it_beg, vertex_it_end);

    for (auto& other_cmd : mDrawCommands)
    {
        if (other_cmd.offset > cmd.offset)
        {
            ASSERT(other_cmd.offset >= vertex_count);
            other_cmd.offset -= vertex_count;
        }
    }
    base::SafeErase(mDrawCommands, index);
}

template<typename Vertex>
void PolygonBuilder<Vertex>::InsertVertex(const Vertex& vertex, const size_t cmd_index, const size_t index)
{
    ASSERT(cmd_index < mDrawCommands.size());
    ASSERT(index <= mDrawCommands[cmd_index].count);

    // figure out the index where the put the new vertex in the vertex
    // array.
    auto& cmd = mDrawCommands[cmd_index];
    cmd.count = cmd.count + 1;

    const auto vertex_index = cmd.offset + index;

    EditVertex edit_vertex;
    edit_vertex.vertex = vertex;
    edit_vertex.flags  = 0;
    mVertices.insert(mVertices.begin() + vertex_index, edit_vertex);

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
    EditVertex edit_vertex;
    edit_vertex.vertex = *static_cast<const Vertex*>(vertex);
    edit_vertex.flags  = 0;
    mVertices.push_back(edit_vertex);
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
VertexLayout PolygonBuilder<Vertex>::GetEditVertexLayout() const noexcept
{
    auto layout = GetVertexLayout<Vertex>();
    VertexLayout::Attribute flags_attribute;
    flags_attribute.num_vector_components = 1;
    flags_attribute.name   = "flags";
    flags_attribute.type   = VertexLayout::Attribute::DataType::UnsignedInt;
    flags_attribute.offset = layout.vertex_struct_size;
    layout.AppendAttribute(flags_attribute);
    return layout;
}

template<typename Vertex>
void PolygonBuilder<Vertex>::IntoJson(data::Writer& writer) const
{
    const VertexStream  vertex_stream(GetEditVertexLayout(), mVertices);
    vertex_stream.IntoJson(writer);

    const CommandStream command_stream(mDrawCommands);
    command_stream.IntoJson(writer);

    writer.Write("static", mStatic);
    writer.Write("double_sided", mDoubleSided);
    writer.Write("version", 1u);
}

template<typename Vertex>
bool PolygonBuilder<Vertex>::FromJson(const data::Reader& reader)
{
    bool ok = true;

    unsigned version = 0;
    reader.Read("version", &version);
    if (version == 0)
    {
        VertexBuffer vertex_buffer;
        ok &= vertex_buffer.FromJson(reader);

        const VertexStream vertex_stream(vertex_buffer.GetLayout(),
            vertex_buffer.GetBufferPtr(),
            vertex_buffer.GetBufferSize());

        if (vertex_buffer.GetLayout() == GetVertexLayout<Vertex>())
        {
            INFO("Migrating polygon builder vertex data.");
            const auto vertex_count = vertex_buffer.GetCount();
            for (size_t i=0; i<vertex_count; ++i)
            {
                EditVertex edit_vertex;
                edit_vertex.vertex = *vertex_stream.GetVertex<Vertex>(i);
                edit_vertex.flags  = 0;
                mVertices.push_back(edit_vertex);
            }
        }
        else
        {
            ERROR("Polugon builder vertex layout mismatch.");
            ok = false;
        }
    }
    else if (version == 1)
    {
        VertexBuffer vertex_buffer;
        ok &= vertex_buffer.FromJson(reader);
        mVertices = vertex_buffer.CopyBuffer<EditVertex>();
    }

    CommandBuffer command_buffer(&mDrawCommands);
    ok &= command_buffer.FromJson(reader);

    ok &= reader.Read("static", &mStatic);
    ok &= reader.Read("double_sided", &mDoubleSided);
    return ok;
}

template<typename Vertex>
void PolygonBuilder<Vertex>::BuildPoly(PolygonMeshClass& polygon) const
{
    polygon.ClearContent();

    if (!mVertices.empty())
    {
        std::vector<uint8_t> byte_buffer;
        std::vector<uint8_t> flags_buffer;
        VertexBuffer vertex_buffer(GetVertexLayout<Vertex>(), &byte_buffer);

        for (const auto& edit_vertex : mVertices)
        {
            vertex_buffer.PushBack(&edit_vertex.vertex);
            flags_buffer.push_back(edit_vertex.flags);
        }

        polygon.SetVertexBuffer(std::move(byte_buffer));
        polygon.SetVertexFlagBuffer(std::move(flags_buffer));
    }

    polygon.SetContentHash(GetContentHash());
    polygon.SetVertexLayout(gfx::GetVertexLayout<Vertex>());
    polygon.SetCommandBuffer(mDrawCommands);
    polygon.SetStatic(mStatic);
    polygon.SetDoubleSided(mDoubleSided);
}

template<typename Vertex>
void PolygonBuilder<Vertex>::InitFrom(const PolygonMeshClass& polygon)
{
    mDrawCommands.clear();
    mVertices.clear();

    if (polygon.HasInlineData())
    {
        ASSERT(*polygon.GetVertexLayout() == gfx::GetVertexLayout<Vertex>());

        const void* vertex_buffer_ptr = polygon.GetVertexBufferPtr();
        const auto vertex_buffer_size = polygon.GetVertexBufferSize();
        const auto vertex_count = vertex_buffer_size / sizeof(Vertex);
        const auto* vertex_flags = polygon.GetVertexFlagBufferPtr();

        if (vertex_count)
        {
            const VertexStream vertex_stream(GetVertexLayout<Vertex>(),
                vertex_buffer_ptr, vertex_buffer_size);

            mVertices.resize(vertex_count);
            for (size_t i=0; i<vertex_count; ++i)
            {
                mVertices[i].vertex = *vertex_stream.GetVertex<Vertex>(i);
                mVertices[i].flags  = vertex_flags ? vertex_flags[i] : 0;
            }

        }

        for (size_t i=0; i<polygon.GetDrawCmdCount(); ++i)
        {
            mDrawCommands.push_back(*polygon.GetDrawCmd(i));
        }

        // post condition sanity.
        ASSERT(mVertices.size() == polygon.GetVertexCount());
        ASSERT(mDrawCommands.size() == polygon.GetDrawCmdCount());
    }
    mStatic = polygon.IsStatic();
    mDoubleSided = polygon.IsDoubleSided();
}

template class PolygonBuilder<Vertex2D>;
template class PolygonBuilder<Perceptual3DVertex>;
template class PolygonBuilder<ShardVertex2D>;

} // namespace
} // namespace