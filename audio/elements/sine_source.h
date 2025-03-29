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

#pragma once

#include "config.h"

#include "audio/elements/element.h"

namespace audio
{
    class SineSource : public Element
    {
    public:
        SineSource(const std::string& name,
                   const std::string& id,
                   const Format& format,
                   unsigned frequency,
                   unsigned millisecs = 0);
        SineSource(const std::string& name,
                   const Format& format,
                   unsigned frequency,
                   unsigned millisecs = 0);
        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        std::string GetType() const override
        { return "SineSource"; }
        bool IsSourceDone() const override
        {
            if (!mDuration)
                return false;
            return mMilliSecs >= mDuration;
        }
        bool IsSource() const override
        { return true; }
        unsigned GetNumOutputPorts() const override
        { return 1; }
        Port& GetOutputPort(unsigned index) override
        { return mPort; }
        bool Prepare(const Loader& loader, const PrepareParams& params) override;
        void Process(Allocator& allocator, EventQueue& events, unsigned milliseconds) override;

        void SetSampleType(SampleType type)
        { mFormat.sample_type = type; }
    private:
        template<typename DataType, unsigned ChannelCount>
        void Generate(BufferHandle buffer, unsigned frames);
        template<unsigned ChannelCount>
        void GenerateFrame(Frame<float, ChannelCount>* frame, float value);
        template<typename Type, unsigned ChannelCount>
        void GenerateFrame(Frame<Type, ChannelCount>* frame, float value);
    private:
        const std::string mName;
        const std::string mId;
        const unsigned mDuration  = 0;
        unsigned mFrequency = 0;
        unsigned mMilliSecs   = 0;
        unsigned mSampleCount = 0;
        SingleSlotPort mPort;
        Format mFormat;
    };

} // namespace