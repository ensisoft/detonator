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
    class Decoder;
    class SourceStream;

    class StreamSource : public Element
    {
    public:
        enum class Format {
            Mp3, Ogg, Flac, Wav
        };
        StreamSource(const std::string& name, std::shared_ptr<const SourceStream> buffer,
                     Format format, SampleType type = SampleType::Int16);
        StreamSource(StreamSource&& other);
       ~StreamSource();

        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        std::string GetType() const override
        { return "StreamSource"; }
        bool Prepare(const Loader& loader, const PrepareParams& params) override;
        void Process(Allocator& allocator, EventQueue& events, unsigned milliseconds) override;
        void Shutdown() override;
        bool IsSourceDone() const override;
        bool IsSource() const override
        { return true; }
        unsigned GetNumOutputPorts() const override
        { return 1; }
        Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mPort;
            BUG("No such output port.");
        }
    private:
        const std::string mName;
        const std::string mId;
        const Format mInputFormat;
        std::shared_ptr<const SourceStream> mBuffer;
        std::unique_ptr<Decoder> mDecoder;
        SingleSlotPort mPort;
        audio::Format mOutputFormat;
        unsigned mFramesRead = 0;
    };

} // namespace