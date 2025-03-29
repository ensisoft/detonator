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

#include "base/utility.h"
#include "audio/elements/element.h"

namespace audio
{
    // Null element discards any input stream buffers pushed
    // into its input port.
    class Null : public Element
    {
    public:
        Null(const std::string& name)
          : mName(name)
          , mId(base::RandomString(10))
          , mIn("in")
        {}
        Null(const std::string& name, const std::string& id)
          : mName(name)
          , mId(id)
          , mIn("in")
        {}
        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        std::string GetType() const override
        { return "Null"; }
        bool Prepare(const Loader&, const PrepareParams& params) override
        { return true; }
        void Process(Allocator& allocator, EventQueue& events, unsigned milliseconds) override
        {
            BufferHandle buffer;
            mIn.PullBuffer(buffer);
        }
        unsigned GetNumInputPorts() const override
        { return 1; }
        Port& GetInputPort(unsigned index) override
        {
            if (index == 0) return mIn;
            BUG("No such input port index.");
        }
    private:
        const std::string mName;
        const std::string mId;
        SingleSlotPort mIn;
    };

} // namespace