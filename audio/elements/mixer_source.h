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

#include <variant>
#include <vector>

#include "audio/elements/element.h"

namespace audio
{

    // MixerSource wraps multiple (source) elements into a single
    // source. Each source to be added must have the same format
    // that has been set in the mixer. Once the sources have been
    // added their state can be controlled through mixer commands.
    class MixerSource : public Element
    {
    public:
        class Effect
        {
        public:
            virtual ~Effect() = default;
            virtual void Apply(BufferHandle buffer) = 0;
            virtual bool IsDone() const = 0;
            virtual std::string GetName() const = 0;
        private:
        };
        // Ramp up the source gain from 0.0f to 1.0f
        class FadeIn : public Effect
        {
        public:
            FadeIn(float seconds) : mDuration(seconds * 1000.0f)
            {}
            FadeIn(unsigned millisecs) : mDuration(millisecs)
            {}
            void Apply(BufferHandle buffer) override;
            bool IsDone() const override
            { return mTime >= mDuration; }
            std::string GetName() const override
            { return "FadeIn"; }
        private:
            template<typename Type, unsigned ChannelCount>
            void ApplyFadeIn(BufferHandle);
        private:
            float mDuration = 0.0f;
            float mTime = 0.0f;
        };
        // Ramp down the source gain from 1.0f to 0.0f
        class FadeOut : public Effect
        {
        public:
            FadeOut(float seconds) : mDuration(seconds * 1000.0f)
            {}
            FadeOut(unsigned millisecs) : mDuration(millisecs)
            {}
            void Apply(BufferHandle buffer) override;
            bool IsDone() const override
            { return mTime >= mDuration; }
            std::string GetName() const override
            { return "FadeOut"; }
        private:
            template<typename Type, unsigned ChannelCount>
            void ApplyFadeOut(BufferHandle);
        private:
            float mDuration = 0.0f;
            float mTime = 0.0f;
        };

        // Command to add a new source stream to the mixer.
        // The element needs to be a source, i.e. IsSource is true.
        // Subsequent commands can refer to the same source
        // through its name.
        struct AddSourceCmd {
            std::unique_ptr<Element> src;
            bool paused = false;
        };
        // Commands to modify the state of the source identified
        // by its name. If no such source is found then nothing is done.

        struct DeleteAllSrcCmd {
            std::string name; // not used.
            unsigned millisecs = 0;
        };

        // Delete the source.
        struct DeleteSourceCmd {
            std::string name;
            unsigned millisecs = 0;
        };
        // Pause/Resume the source. When the source is paused
        // no buffers are pulled from it and the source
        // is no longer mixed into the output stream.
        struct PauseSourceCmd {
            std::string name;
            // Flag to indicate whether to pause / resume the src.
            bool paused = false;
            unsigned millisecs = 0;
        };
        // Cancel pending commands on a source.
        struct CancelSourceCmdCmd {
            std::string name;
        };

        struct SetEffectCmd {
            std::string src;
            std::unique_ptr<Effect> effect;
        };

        struct SourceDoneEvent {
            std::string mixer;
            std::unique_ptr<Element> src;
        };
        struct EffectDoneEvent {
            std::string mixer;
            std::string src;
            std::unique_ptr<Effect> effect;
        };

        // Create a new mixer with the given name and format.
        MixerSource(std::string name, const Format& format);
        MixerSource(MixerSource&& other);

        // Add a new source element to the mixer. The element must be a source
        // and have at least 1 output port with the same format that the
        // mixer source itself has.
        Element* AddSourcePtr(std::unique_ptr<Element> source, bool paused=false);

        // Enable/disable never done flag. When never done flag is one
        // the source is never considered done regardless of whether the
        // current sources are done or not.
        void SetNeverDone(bool on_off)
        { mNeverDone = on_off; }

        template<typename Source>
        Source* AddSource(Source&& source, bool paused=false)
        {
            auto src = std::make_unique<std::remove_reference_t<Source>>(std::forward<Source>(source));
            auto* ret = AddSourcePtr(std::move(src));
            return static_cast<Source*>(ret);
        }
        // Cancel any pending commands on the named source.
        void CancelSourceCommands(const std::string& name);
        // Delete all sources from the mixer.
        void DeleteSources();
        // Delete the named source from the mixer.
        void DeleteSource(const std::string& name);
        // Pause/resume the named source. If already paused/playing nothing is done.
        void PauseSource(const std::string& name, bool paused);
        // Set an effect on the source. This will take place immediately
        // and override any possible previous effect thus creating a discontinuity.
        void SetSourceEffect(const std::string& name, std::unique_ptr<Effect> effect);

        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        std::string GetType() const override
        { return "MixerSource"; }
        bool IsSource() const override { return true; }
        bool IsSourceDone() const override;
        bool Prepare(const Loader& loader, const PrepareParams& params) override;
        void Advance(unsigned int ms) override;
        void Process(Allocator& allocator, EventQueue& events, unsigned milliseconds) override;
        unsigned GetNumOutputPorts() const override
        { return 1; }
        Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mOut;
            BUG("No such output port index.");
        }
        void ReceiveCommand(Command& cmd) override;
        bool DispatchCommand(const std::string& dest, Command& cmd) override;
    private:
        void ExecuteCommand(const DeleteAllSrcCmd& cmd);
        void ExecuteCommand(const DeleteSourceCmd& cmd);
        void ExecuteCommand(const PauseSourceCmd& cmd);
        void RemoveDoneEffects(EventQueue& events);
        void RemoveDoneSources(EventQueue& events);
    private:
        const std::string mName;
        const std::string mId;
        const Format mFormat;
        struct Source {
            std::unique_ptr<Element> element;
            std::unique_ptr<Effect> effect;
            bool paused = false;
        };
        using LateCommand = std::variant<PauseSourceCmd,
                DeleteSourceCmd,
                DeleteAllSrcCmd>;
        std::vector<LateCommand> mCommands;
        std::unordered_map<std::string, Source> mSources;
        SingleSlotPort mOut;
        bool mNeverDone = false;
    };

} // namespace