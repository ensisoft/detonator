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

#include <cstring>

#include "warnpush.h"
#  include <base64/base64.h>
#include "warnpop.h"

#include "base/hash.h"
#include "base/logging.h"
#include "data/reader.h"
#include "data/writer.h"
#include "graphics/geometry.h"

namespace {
    void AddLine(gfx::VertexBuffer& buffer, const void* v0, const void* v1)
    {
        buffer.PushBack(v0);
        buffer.PushBack(v1);
    }

    enum class ByteOrder {
        LE, BE
    };

    ByteOrder GetByteOrder() noexcept
    {
        constexpr uint32_t value = 1;
        if (*(const char*)&value == 1)
            return ByteOrder::LE;

        return ByteOrder::BE;
    }

    template<typename T>
    void SwizzleBuffer(void* buffer, size_t bytes)
    {
        const auto count = bytes / sizeof(T);

        auto* ptr = reinterpret_cast<T*>(buffer);

        for (size_t i=0; i<count; ++i)
        {
            unsigned char buffer[sizeof(T)];
            std::memcpy(buffer, &ptr[i], sizeof(T));

            if constexpr (sizeof(T) == 8)
            {
                std::swap(buffer[0], buffer[7]);
                std::swap(buffer[1], buffer[6]);
                std::swap(buffer[2], buffer[5]);
                std::swap(buffer[3], buffer[4]);
            }
            else if constexpr (sizeof(T) == 4)
            {
                std::swap(buffer[0], buffer[3]);
                std::swap(buffer[1], buffer[2]);
            }
            else if constexpr (sizeof(T) == 2)
            {
                std::swap(buffer[0], buffer[1]);
            }
            std::memcpy(&ptr[i], buffer, sizeof(T));
        }

    }

} // namespace

namespace gfx
{

bool operator==(const VertexLayout& lhs, const VertexLayout& rhs) noexcept
{
    if (lhs.vertex_struct_size != rhs.vertex_struct_size)
        return false;
    if (lhs.attributes.size() != rhs.attributes.size())
        return false;
    for (size_t i=0; i<lhs.attributes.size(); ++i)
    {
        const auto& l = lhs.attributes[i];
        const auto& r = rhs.attributes[i];

        if (l.num_vector_components != r.num_vector_components) return false;
        else if (l.name != r.name) return false;
        else if (l.offset != r.offset) return false;
        else if (l.divisor != r.divisor) return false;
        else if (l.index != r.index) return false;
    }
    return true;
}

bool operator!=(const VertexLayout& lhs, const VertexLayout& rhs) noexcept
{
    return !(lhs == rhs);
}

bool VertexLayout::FromJson(const data::Reader& reader) noexcept
{
    const auto& layout = reader.GetReadChunk("vertex_layout");
    if (!layout)
        return false;

    bool ok = true;
    ok &= layout->Read("bytes", &vertex_struct_size);

    for (unsigned i=0; i<layout->GetNumChunks("attributes"); ++i)
    {
        const auto& chunk = layout->GetReadChunk("attributes", i);
        Attribute attr;
        ok &= chunk->Read("name",    &attr.name);
        ok &= chunk->Read("index",   &attr.index);
        ok &= chunk->Read("size",    &attr.num_vector_components);
        ok &= chunk->Read("divisor", &attr.divisor);
        ok &= chunk->Read("offset",  &attr.offset);
        attributes.push_back(std::move(attr));
    }
    return ok;
}

void VertexLayout::IntoJson(data::Writer& writer) const
{
    auto layout = writer.NewWriteChunk();

    layout->Write("bytes", vertex_struct_size);

    for (const auto& attr : attributes)
    {
        auto chunk = layout->NewWriteChunk();
        chunk->Write("name",    attr.name);
        chunk->Write("index",   attr.index);
        chunk->Write("size",    attr.num_vector_components);
        chunk->Write("divisor", attr.divisor);
        chunk->Write("offset",  attr.offset);
        layout->AppendChunk("attributes", std::move(chunk));
    }
    writer.Write("vertex_layout", std::move(layout));
}


size_t VertexLayout::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, vertex_struct_size);
    for (const auto& attr : attributes)
    {
        hash = base::hash_combine(hash, attr.name);
        hash = base::hash_combine(hash, attr.index);
        hash = base::hash_combine(hash, attr.num_vector_components);
        hash = base::hash_combine(hash, attr.divisor);
        hash = base::hash_combine(hash, attr.offset);
    }
    return hash;
}

void IndexStream::IntoJson(data::Writer& writer) const
{
    writer.Write("byte_order", GetByteOrder());
    writer.Write("index_type", mType);
    writer.Write("index_buffer", base64::Encode((const unsigned char*)mBuffer,
                                                mCount * Geometry::GetIndexByteSize(mType)));
}

bool IndexBuffer::FromJson(const data::Reader& reader)
{
    bool ok = true;

    ByteOrder byte_order = ByteOrder::LE;

    std::string data;
    ok &= reader.Read("byte_order", &byte_order);
    ok &= reader.Read("index_type", &mType);
    ok &= reader.Read("index_buffer", &data);
    data = base64::Decode(data);

    const auto bytes = data.size();
    const auto count = bytes / Geometry::GetIndexByteSize(mType);

    if (count == 0)
        return ok;

    mBuffer->resize(bytes);
    std::memcpy(mBuffer->data(), data.data(), data.size());

    if (byte_order != GetByteOrder())
    {
        if (mType == Geometry::IndexType::Index16)
            SwizzleBuffer<uint16_t>(mBuffer->data(), mBuffer->size());
        else if (mType == Geometry::IndexType::Index32)
            SwizzleBuffer<uint32_t>(mBuffer->data(), mBuffer->size());
        else BUG("Missing index type in index buffer swizzle.");
    }
    return ok;
}


void VertexStream::IntoJson(data::Writer& writer) const
{
    mLayout.IntoJson(writer);

    writer.Write("byte_order", GetByteOrder());
    writer.Write("vertex_buffer", base64::Encode((const unsigned char*)mBuffer,
                                                 mCount * mLayout.vertex_struct_size));
}

bool VertexBuffer::Validate() const noexcept
{
    if (mLayout.vertex_struct_size == 0 ||
        mLayout.vertex_struct_size > 10 * sizeof(Vec4))
        return false;

    const auto bytes = mBuffer->size();
    if (bytes % mLayout.vertex_struct_size)
        return false;

    // todo: check attribute offsets

    return true;
}

bool VertexBuffer::FromJson(const data::Reader& reader)
{
    bool ok = true;

    ByteOrder byte_order = ByteOrder::LE;

    std::string data;
    ok &= mLayout.FromJson(reader);
    ok &= reader.Read("vertex_buffer", &data);
    ok &= reader.Read("byte_order", &byte_order);
    data = base64::Decode(data);

    // todo: get rid of this temporary copy..
    mBuffer->resize(data.size());
    if (mBuffer->size() == 0)
        return ok;

    std::memcpy(mBuffer->data(), data.data(), data.size());

    if (byte_order != GetByteOrder())
        SwizzleBuffer<uint32_t>(mBuffer->data(), mBuffer->size());

    return ok;
}

void CommandStream::IntoJson(data::Writer& writer) const
{
    // expect tightly packed and 32bits for the type enum
    // in order to make the serialization simple
    static_assert(sizeof(DrawCommand) == sizeof(uint32_t) * 3);

    const auto bytes = mCount * sizeof(DrawCommand);
    writer.Write("byte_order", GetByteOrder());
    writer.Write("command_buffer", base64::Encode((const unsigned char*)mCommands, bytes));
}

bool CommandBuffer::FromJson(const data::Reader& reader)
{
    bool ok = true;

    ByteOrder byte_order = ByteOrder::LE;

    std::string data;
    ok &= reader.Read("byte_order", &byte_order);
    ok &= reader.Read("command_buffer", &data);
    data = base64::Decode(data);

    const auto count = data.size() / sizeof(DrawCommand);
    mBuffer->resize(count);

    if (count == 0)
        return ok;

    // expect tightly packed and 32bits for the type enum
    // in order to make the serialization simple
    static_assert(sizeof(Geometry::DrawCommand) == sizeof(uint32_t) * 3);

    std::memcpy(mBuffer->data(), data.data(), data.size());

    if (byte_order != GetByteOrder())
        SwizzleBuffer<uint32_t>(mBuffer->data(), mBuffer->size() * sizeof(DrawCommand));
    return ok;
}

void CreateWireframe(const GeometryBuffer& geometry, GeometryBuffer& wireframe)
{
    const VertexStream vertices(geometry.GetLayout(),
                                geometry.GetVertexDataPtr(),
                                geometry.GetVertexBytes());

    const IndexStream indices(geometry.GetIndexDataPtr(),
                              geometry.GetIndexBytes(),
                              geometry.GetIndexType());

    std::vector<uint8_t> vertex_data;
    VertexBuffer vertex_writer(geometry.GetLayout(), &vertex_data);

    const auto vertex_count = vertices.GetCount();
    const auto index_count  = indices.GetCount();
    const auto has_index = indices.IsValid();

    for (size_t i=0; i<geometry.GetNumDrawCmds(); ++i)
    {
        const auto& cmd = geometry.GetDrawCmd(i);
        const auto primitive_count = cmd.count != std::numeric_limits<uint32_t>::max()
                           ? (cmd.count)
                           : (has_index ? index_count : vertex_count);

        if (cmd.type == Geometry::DrawType::Triangles)
        {
            ASSERT((primitive_count % 3) == 0);
            const auto triangles = primitive_count / 3;

            for (size_t j=0; j<triangles; ++j)
            {
                const auto start = cmd.offset + j * 3;
                const uint32_t i0 = has_index ? indices.GetIndex(start+0) : start+0;
                const uint32_t i1 = has_index ? indices.GetIndex(start+1) : start+1;
                const uint32_t i2 = has_index ? indices.GetIndex(start+2) : start+2;
                const void* v0 = vertices.GetVertexPtr(i0);
                const void* v1 = vertices.GetVertexPtr(i1);
                const void* v2 = vertices.GetVertexPtr(i2);

                AddLine(vertex_writer, v0, v1);
                AddLine(vertex_writer, v1, v2);
                AddLine(vertex_writer, v2, v0);
            }
        }
        else if (cmd.type == Geometry::DrawType::TriangleFan)
        {
            ASSERT(primitive_count >= 3);
            // the first 3 vertices form a triangle and then
            // every subsequent vertex creates another triangle with
            // the first and previous vertex.
            const uint32_t i0 = has_index ? indices.GetIndex(cmd.offset+0) : cmd.offset+0;
            const uint32_t i1 = has_index ? indices.GetIndex(cmd.offset+1) : cmd.offset+1;
            const uint32_t i2 = has_index ? indices.GetIndex(cmd.offset+2) : cmd.offset+2;
            const void* v0 = vertices.GetVertexPtr(i0);
            const void* v1 = vertices.GetVertexPtr(i1);
            const void* v2 = vertices.GetVertexPtr(i2);

            AddLine(vertex_writer, v0, v1);
            AddLine(vertex_writer, v1, v2);
            AddLine(vertex_writer, v2, v0);

            for (size_t j=3; j<primitive_count; ++j)
            {
                const auto start = cmd.offset + j;
                const uint32_t iPrev = has_index ? indices.GetIndex(start-1) : start-1;
                const uint32_t iCurr = has_index ? indices.GetIndex(start-0) : start-0;
                const void* vPrev = vertices.GetVertexPtr(iPrev);
                const void* vCurr = vertices.GetVertexPtr(iCurr);

                AddLine(vertex_writer, vCurr, vPrev);
                AddLine(vertex_writer, vCurr, v0);
            }
        }
    }

    wireframe.SetVertexBuffer(std::move(vertex_data));
    wireframe.SetVertexLayout(geometry.GetLayout());
    wireframe.AddDrawCmd(Geometry::DrawType::Lines);
}

} // namespace
