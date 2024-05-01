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

#include "data/writer.h"
#include "data/reader.h"
#include "graphics/drawcmd.h"

namespace gfx
{

void CommandStream::IntoJson(data::Writer& writer) const
{
    // expect tightly packed and 32bits for the type enum
    // in order to make the serialization simple
    static_assert(sizeof(DrawCommand) == sizeof(uint32_t) * 3);

    const auto bytes = mCount * sizeof(DrawCommand);
    writer.Write("byte_order", base::GetByteOrder());
    writer.Write("command_buffer", base64::Encode((const unsigned char*)mCommands, bytes));
}

bool CommandBuffer::FromJson(const data::Reader& reader)
{
    bool ok = true;

    auto byte_order = base::ByteOrder::LE;

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

    if (byte_order != base::GetByteOrder())
        base::SwizzleBuffer<uint32_t>(mBuffer->data(), mBuffer->size() * sizeof(DrawCommand));
    return ok;
}

} // namespace
