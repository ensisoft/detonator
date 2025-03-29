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

    // Manipulate audio stream gain over time in order to create a
    // some kind of an audio effect.
    class Effect : public Element
    {
    public:
        // The possible effect.
        enum class Kind {
            // Ramp up the stream gain from 0.0f to 1.0f
            FadeIn,
            // Ramp down the stream gain from 1.0f to 0.0f
            FadeOut
        };
        struct SetEffectCmd {
            unsigned time     = 0;
            unsigned duration = 0;
            Kind effect = Kind::FadeIn;
        };

        Effect(std::string name, std::string id, unsigned time, unsigned duration, Kind effect);
        Effect(std::string name, unsigned time, unsigned duration, Kind effect);
        explicit Effect(std::string name);

        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        std::string GetType() const override
        { return "Effect"; }
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

        void SetEffect(Kind effect, unsigned time, unsigned duration);
    private:
        template<typename DataType, unsigned ChannelCount>
        void FadeInOut(BufferHandle buffer);
    private:
        const std::string mName;
        const std::string mId;
        SingleSlotPort mIn;
        SingleSlotPort mOut;
        Kind mEffect = Kind::FadeIn;
        // duration of the fading effect.
        unsigned mDuration  = 0;
        unsigned mStartTime = 0;
        // how far into the effect are we.
        float mSampleTime = 0.0;
        // current stream sample rate.
        unsigned mSampleRate = 0;
    };

} // namespace