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
    // Go over a list of sources pulling audio buffers one source
    // at a time until the buffer's meta information indicates
    // that the source has finished and then advance to the next
    // source.
    class Playlist : public Element
    {
    public:
        Playlist(std::string name, std::string id, const std::vector<PortDesc>& srcs);
        Playlist(std::string, const std::vector<PortDesc>& srcs);
        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        std::string GetType() const override
        { return "Playlist"; }
        bool Prepare(const Loader& loader, const PrepareParams& params) override;
        void Process(Allocator& allocator, EventQueue& events, unsigned milliseconds) override;
        unsigned GetNumOutputPorts() const override
        { return 1; }
        unsigned GetNumInputPorts() const override
        { return mSrcs.size(); }
        Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mOut;
            BUG("No such output port index.");
        }
        Port& GetInputPort(unsigned index) override
        { return base::SafeIndex(mSrcs, index); }
    private:
        const std::string mName;
        const std::string mId;
        std::vector<SingleSlotPort> mSrcs;
        std::size_t mSrcIndex = 0;
        SingleSlotPort mOut;
    };
} // namespace