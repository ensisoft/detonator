// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "device/vertex.h"

namespace dev
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

        if (l.num_vector_components != r.num_vector_components)
            return false;
        if (l.name != r.name)
            return false;
        if (l.offset != r.offset)
            return false;
        if (l.divisor != r.divisor)
            return false;
        if (l.index != r.index)
            return false;
        if (l.type != r.type)
            return false;
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
        ok &= chunk->Read("type",    &attr.type);
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
        chunk->Write("type",    attr.type);
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
        hash = base::hash_combine(hash, attr.type);
    }
    return hash;
}

} // namespace