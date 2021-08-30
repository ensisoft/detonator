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

#include "base/format.h"
#include "data/writer.h"
#include "data/reader.h"
#include "audio/format.h"

namespace audio
{

void Serialize(data::Writer& writer, const char* name, const Format& format)
{
    auto chunk = writer.NewWriteChunk();
    chunk->Write("type", format.sample_type);
    chunk->Write("rate", format.sample_rate);
    chunk->Write("cc",   format.channel_count);
    writer.Write(name, std::move(chunk));
}

bool Deserialize(const data::Reader& reader, const char* name, Format* format)
{
    auto chunk = reader.GetReadChunk(name);
    if (!chunk) return false;
    if (!chunk->Read("type", &format->sample_type) ||
        !chunk->Read("rate", &format->sample_rate) ||
        !chunk->Read("cc",   &format->channel_count))
        return false;
    return true;
}

bool operator==(const Format& lhs, const Format& rhs)
{
    return lhs.sample_type == rhs.sample_type &&
           lhs.sample_rate == rhs.sample_rate &&
           lhs.channel_count == rhs.channel_count;
}
bool operator!=(const Format& lhs, const Format& rhs)
{ return !(lhs == rhs); }

bool IsValid(const Format& format)
{
    if (format.sample_rate == 0)
        return false; // todo: use some predefined list of valid formats ?
    else if (!(format.channel_count == 1 ||format.channel_count == 2))
        return false;
    return true;
}

std::string ToString(const Format& fmt)
{
    std::string cc;
    if (fmt.channel_count == 0)
        cc = "None";
    else if (fmt.channel_count == 1)
        cc = "Mono";
    else if (fmt.channel_count == 2)
        cc = "Stereo";
    else cc = std::to_string(fmt.channel_count);
    return base::FormatString("%1, %2 @ %3Hz", fmt.sample_type, cc, fmt.sample_rate);
}

} // namespace