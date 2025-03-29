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

    // Split a stereo stream into left and right channel streams.
    class StereoSplitter : public Element
    {
    public:
        StereoSplitter(std::string name, std::string id);
        StereoSplitter(std::string name);

        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        std::string GetType() const override
        { return "StereoSplitter"; }
        bool Prepare(const Loader& loader, const PrepareParams& params) override;
        void Process(Allocator& allocator, EventQueue& events, unsigned milliseconds) override;
        unsigned GetNumInputPorts() const override
        { return 1; }
        unsigned GetNumOutputPorts() const override
        { return 2; }
        Port& GetInputPort(unsigned index) override
        {
            if (index == 0) return mIn;
            BUG("No such input port index.");
        }
        Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mOutLeft;
            if (index == 1) return mOutRight;
            BUG("No such output port index.");
        }
    private:
        template<typename Type>
        void Split(Allocator& allocator, BufferHandle buffer);
    private:
        const std::string mName;
        const std::string mId;
        SingleSlotPort mIn;
        SingleSlotPort mOutLeft;
        SingleSlotPort mOutRight;
    };

} // namespace