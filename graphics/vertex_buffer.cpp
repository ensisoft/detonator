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

#include "config.h"

#include "warnpush.h"
#  include <base64/base64.h>
#include "warnpop.h"

#include "base/hash.h"
#include "base/utility.h"
#include "data/writer.h"
#include "data/reader.h"
#include "graphics/vertex_buffer.h"

namespace gfx
{

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
