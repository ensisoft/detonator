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

    // Adjust the stream's gain (volume) setting.
    class Gain : public Element
    {
    public:
        struct SetGainCmd {
            float gain = 1.0f;
        };

        Gain(const std::string& name, float gain = 1.0f);
        Gain(const std::string& name, const std::string& id, float gain = 1.0f);

        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        std::string GetType() const override
        { return "Gain"; }
        bool Prepare(const Loader& loader, const PrepareParams& params) override;
        void Process(Allocator& allocator, EventQueue& events, unsigned milliseconds) override;
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
        void ReceiveCommand(Command& cmd) override;

        void SetGain(float gain)
        { mGain = gain; }
    private:
        template<typename DataType, unsigned ChannelCount>
        void AdjustGain(BufferHandle buffer);
    private:
        const std::string mName;
        const std::string mId;
        SingleSlotPort mIn;
        SingleSlotPort mOut;
        float mGain = 1.0f;
    };


} // namespace