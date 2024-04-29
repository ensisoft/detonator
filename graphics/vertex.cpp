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

#include "warnpush.h"
#  include <base64/base64.h>
#include "warnpop.h"

#include "base/hash.h"
#include "base/utility.h"
#include "data/writer.h"
#include "data/reader.h"
#include "graphics/vertex.h"

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

void VertexStream::IntoJson(data::Writer& writer) const
{
    mLayout.IntoJson(writer);

    writer.Write("byte_order", base::GetByteOrder());
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

    auto byte_order = base::ByteOrder::LE;

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

    if (byte_order != base::GetByteOrder())
        base::SwizzleBuffer<uint32_t>(mBuffer->data(), mBuffer->size());

    return ok;
}

void IndexStream::IntoJson(data::Writer& writer) const
{
    writer.Write("byte_order", base::GetByteOrder());
    writer.Write("index_type", mType);
    writer.Write("index_buffer", base64::Encode((const unsigned char*)mBuffer,
                                                mCount * GetIndexByteSize(mType)));
}

bool IndexBuffer::FromJson(const data::Reader& reader)
{
    bool ok = true;

    auto byte_order = base::ByteOrder::LE;

    std::string data;
    ok &= reader.Read("byte_order", &byte_order);
    ok &= reader.Read("index_type", &mType);
    ok &= reader.Read("index_buffer", &data);
    data = base64::Decode(data);

    const auto bytes = data.size();
    const auto count = bytes / GetIndexByteSize(mType);

    if (count == 0)
        return ok;

    mBuffer->resize(bytes);
    std::memcpy(mBuffer->data(), data.data(), data.size());

    if (byte_order != base::GetByteOrder())
    {
        if (mType == Type::Index16)
            base::SwizzleBuffer<uint16_t>(mBuffer->data(), mBuffer->size());
        else if (mType == Type::Index32)
            base::SwizzleBuffer<uint32_t>(mBuffer->data(), mBuffer->size());
        else BUG("Missing index type in index buffer swizzle.");
    }
    return ok;
}

} // namespace
