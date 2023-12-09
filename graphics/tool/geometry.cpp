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
#include "graphics/tool/geometry.h"

namespace gfx {
namespace tool {

void PolygonBuilder::Clear() noexcept
{
    mVertices.clear();
    mDrawCommands.clear();
}
void PolygonBuilder::ClearDrawCommands() noexcept
{
    mDrawCommands.clear();
}
void PolygonBuilder::ClearVertices() noexcept
{
    mVertices.clear();
}
void PolygonBuilder::AddVertices(const std::vector<Vertex>& vertices)
{
    std::copy(std::begin(vertices), std::end(vertices), std::back_inserter(mVertices));
}
void PolygonBuilder::AddVertices(std::vector<Vertex>&& vertices)
{
    std::move(std::begin(vertices), std::end(vertices), std::back_inserter(mVertices));
}
void PolygonBuilder::AddVertices(const Vertex* vertices, size_t num_vertices)
{
    for (size_t i=0; i<num_vertices; ++i)
        mVertices.push_back(vertices[i]);
}
void PolygonBuilder::AddDrawCommand(const DrawCommand& cmd)
{
    mDrawCommands.push_back(cmd);
}

void PolygonBuilder::UpdateVertex(const Vertex& vert, size_t index)
{
    ASSERT(index < mVertices.size());
    mVertices[index] = vert;
}

void PolygonBuilder::EraseVertex(size_t index)
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

void PolygonBuilder::InsertVertex(const Vertex& vertex, size_t cmd_index, size_t index)
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

void PolygonBuilder::UpdateDrawCommand(const DrawCommand& cmd, size_t index) noexcept
{
    ASSERT(index < mDrawCommands.size());
    mDrawCommands[index] = cmd;
}

size_t PolygonBuilder::FindDrawCommand(size_t vertex_index) const noexcept
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

size_t PolygonBuilder::GetContentHash() const noexcept
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


void PolygonBuilder::IntoJson(data::Writer& writer) const
{
    const VertexStream stream(gfx::GetVertexLayout<Vertex>(),
                              (const void*)mVertices.data(), mVertices.size() * sizeof(Vertex));
    stream.IntoJson(writer);

    writer.Write("static", mStatic);

    for (const auto& cmd : mDrawCommands)
    {
        auto chunk = writer.NewWriteChunk();
        chunk->Write("type",   cmd.type);
        chunk->Write("offset", (unsigned)cmd.offset);
        chunk->Write("count",  (unsigned)cmd.count);
        writer.AppendChunk("draws", std::move(chunk));
    }
}

bool PolygonBuilder::FromJson(const data::Reader& reader)
{
    bool ok = true;

    std::vector<uint8_t> buff;
    VertexBuffer buffer(&buff);
    ok &= buffer.FromJson(reader);

    ok &= reader.Read("static", &mStatic);

    for (unsigned i=0; i<reader.GetNumChunks("draws"); ++i)
    {
        const auto& chunk = reader.GetReadChunk("draws", i);

        unsigned offset = 0;
        unsigned count = 0;

        DrawCommand cmd;
        ok &= chunk->Read("type",   &cmd.type);
        ok &= chunk->Read("offset", &offset);
        ok &= chunk->Read("count",  &count);
        cmd.offset = offset;
        cmd.count  = count;
        mDrawCommands.push_back(cmd);
    }

    const auto bytes = buff.size();
    const auto count = bytes / sizeof(Vertex);
    mVertices.resize(count);
    std::memcpy(mVertices.data(), buff.data(), bytes);

    return ok;
}

void PolygonBuilder::BuildPoly(PolygonMeshClass& polygon) const
{
    const auto count = mVertices.size();
    const auto bytes = count * sizeof(Vertex2D);

    std::vector<uint8_t> buffer;
    buffer.resize(bytes);
    std::memcpy(buffer.data(), mVertices.data(), bytes);

    polygon.SetVertexBuffer(std::move(buffer));
    polygon.SetContentHash(GetContentHash());
    polygon.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    polygon.SetCommandBuffer(mDrawCommands);
    polygon.SetStatic(mStatic);
}

void PolygonBuilder::InitFrom(const PolygonMeshClass& polygon)
{
    if (polygon.HasInlineData())
    {
        ASSERT(*polygon.GetVertexLayout() == gfx::GetVertexLayout<gfx::Vertex2D>());

        const void* ptr = polygon.GetVertexBufferPtr();
        const auto bytes = polygon.GetVertexBufferSize();
        const auto count = bytes / sizeof(Vertex2D);

        mVertices.resize(count);
        std::memcpy(mVertices.data(), ptr, bytes);

        for (size_t i=0; i<polygon.GetNumDrawCmds(); ++i)
        {
            mDrawCommands.push_back(*polygon.GetDrawCmd(i));
        }
    }
    else
    {
        mDrawCommands.clear();
        mVertices.clear();
    }
    mStatic = polygon.IsStatic();
}

} // namespace
} // namespace