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

#pragma once

#include "config.h"

#include <string>

#include "base/assert.h"
#include "base/format.h"
#include "data/fwd.h"

namespace audio
{
    // The underlying sample data type.
    enum class SampleType {
        NotSet,
        Float32,
        Int16,
        Int32
    };
    enum class Channels {
        Stereo = 2, Mono = 1
    };

    struct Format
    {
        SampleType sample_type = SampleType::NotSet;
        unsigned sample_rate   = 0;
        unsigned channel_count = 0;
    };

    void Serialize(data::Writer& writer, const char* name, const Format& format);
    bool Deserialize(const data::Reader& reader, const char* name, Format* format);

    bool operator==(const Format& lhs, const Format& rhs);
    bool operator!=(const Format& lhs, const Format& rhs);
    bool IsValid(const Format& format);
    std::string ToString(const Format& fmt);

    template<typename DataType, unsigned ChannelCount>
    struct Frame {
        DataType channels[ChannelCount];
    };
    template<typename DataType>
    using StereoFrame = Frame<DataType, 2>;
    template<typename DataType>
    using MonoFrame = Frame<DataType, 1>;

    template<typename T>
    struct SampleBits;

    template<>
    struct SampleBits<short> {
        static constexpr auto Bits = 0x7fff;
        static constexpr auto Type = SampleType::Int16;
    };

    template<>
    struct SampleBits<int> {
        static constexpr auto Bits = 0x7fffffff;
        static constexpr auto Type = SampleType::Int32;
    };

    template<SampleType type>
    struct SampleTraits;

    template<>
    struct SampleTraits<SampleType::Int32> {
        using UnderlyingType = int;
    };
    template<>
    struct SampleTraits<SampleType::Int16> {
        using UnderlyingType = short;
    };
    template<>
    struct SampleTraits<SampleType::Float32> {
        using UnderlyingType = float;
    };

    inline unsigned GetFrameSizeInBytes(const Format& format)
    {
        if (format.sample_type == SampleType::Float32)
            return format.channel_count * 4;
        else if (format.sample_type == SampleType::Int16)
            return format.channel_count * 2;
        else if (format.sample_type == SampleType::Int32)
            return format.channel_count * 4;
        else if (format.sample_type == SampleType::NotSet)
            BUG("Unset audio format data type.");
        else BUG("Unhandled sample type.");
        return 0;
    }

    inline unsigned GetMillisecondByteCount(const Format& format)
    { return (format.sample_rate / 1000) * GetFrameSizeInBytes(format); }

} // namespace