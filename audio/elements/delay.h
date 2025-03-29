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
    class Delay : public Element
    {
    public:
        Delay(std::string name, std::string id, unsigned delay = 0);
        Delay(std::string name, unsigned delay = 0);

        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        std::string GetType() const override
        { return "Delay"; }
        bool Prepare(const Loader& loader, const PrepareParams& params) override;
        void Process(Allocator& allocator, EventQueue& events, unsigned milliseconds) override;
        void Advance(unsigned milliseconds) override;
        unsigned GetNumOutputPorts() const override
        { return 1; }
        unsigned GetNumInputPorts() const override
        { return 1; }
        Port& GetOutputPort(unsigned index) override
        {
            if (index == 0)  return mOut;
            BUG("No such output port.");
        }
        Port& GetInputPort(unsigned index) override
        {
            if (index == 0) return mIn;
            BUG("No such input port.");
        }
    private:
        const std::string mName;
        const std::string mId;
        SingleSlotPort mIn;
        SingleSlotPort mOut;
        unsigned mDelay = 0;
    };


} // namespace