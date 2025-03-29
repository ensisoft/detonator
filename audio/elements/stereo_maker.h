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
    // Turn a possible mono audio stream into a stereo stream.
    // If the input is already a stereo stream nothing is done.
    class StereoMaker : public Element
    {
    public:
        enum class Channel {
            Left = 0, Right = 1, Both
        };
        StereoMaker(std::string name, std::string id, Channel which = Channel::Left);
        StereoMaker(std::string name, Channel which = Channel::Left);

        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        std::string GetType() const override
        { return "StereoMaker"; }
        bool Prepare(const Loader& loader, const PrepareParams& params) override;
        void Process(Allocator& allocator, EventQueue& events, unsigned milliseconds) override;
        unsigned GetNumInputPorts() const override
        { return 1; }
        unsigned GetNumOutputPorts() const override
        { return 1; }
        Port& GetInputPort(unsigned index) override
        {
            if (index == 0) return mIn;
            BUG("No such input port index.");
        }
        Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mOut;
            BUG("No such output port index.");
        }
    private:
        template<typename Type>
        void CopyMono(BufferAllocator& allocator, BufferHandle buffer);
    private:
        const std::string mName;
        const std::string mId;
        const Channel mChannel;
        SingleSlotPort mOut;
        SingleSlotPort mIn;
    };

} // namespace