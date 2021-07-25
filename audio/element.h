// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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

#include <string>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>

#include "base/utility.h"
#include "base/assert.h"
#include "audio/buffer.h"
#include "audio/format.h"
#include "audio/command.h"

typedef struct SRC_STATE_tag SRC_STATE;

namespace audio
{
    class Decoder;

    using BufferHandle = std::shared_ptr<Buffer>;

    // Port provides an input/output /queueing abstraction.
    // I.e. a port is used to push and pull data in and out
    // in buffers. Each port additionally specifies the format
    // that it supports and understand. Each port then going
    // in and coming out of such port should have a matching
    // format setting.
    class Port
    {
    public:
        using BufferHandle = audio::BufferHandle;
        using Format = audio::Format;
        // dtor.
        virtual ~Port() = default;
        // Push a new buffer for into the port.
        // The audio graph will *pull* from the source output ports
        // and *push* into the destination input ports.
        // An element will *pull* from its input ports and *push*
        // into its output ports.
        // Returns false if the queue overflowed and buffer could
        // not be pushed.
        virtual bool PushBuffer(BufferHandle buffer) = 0;
        // Pull a buffer out of the port.
        // The audio graph will *pull* from the source output ports
        // and *push* into the destination input ports.
        // An element will *pull* from its input ports and *push*
        // into its output ports.
        // Returns false if the port was empty and no buffer was available.
        virtual bool PullBuffer(BufferHandle& buffer) = 0;
        // Get the human readable name of the port.
        virtual std::string GetName() const = 0;
        // Get the ports data format. Typically this is originally undefined
        // until the whole audio graph is prepared. When the audio graph
        // is prepared the port format negotiation happens. (See CanAccept)
        virtual Format GetFormat() const = 0;
        // Set the result of the port format negotiation.
        virtual void SetFormat(const Format& format) = 0;
        // Perform format compatibility check by checking against the given
        // suggested audio stream format. Should return true if the format
        // is accepted or false to indicate that the format is not supported.
        virtual bool CanAccept(const Format& format) const = 0;
        // Return true if there are pending buffers in the port's buffer queue.
        virtual bool HasBuffers() const = 0;
    private:
    };

    namespace detail {
        class SingleSlotQueue {
        public:
        protected:
            ~SingleSlotQueue() = default;
             bool Push(BufferHandle buffer)
             {
                 if (mBuffer) return false;
                 mBuffer = buffer;
                 return true;
             }
             bool Pull(BufferHandle& buffer)
             {
                 if (!mBuffer) return false;
                 buffer = mBuffer;
                 mBuffer.reset();
                 return true;
             }
             bool HasBuffers() const
             { return !!mBuffer; }
        private:
            BufferHandle mBuffer;
        };

        template<typename Queue>
        class BasicPort : public Port, public Queue {
        public:
            BasicPort(const std::string& name)
              : mName(name)
            {}
            virtual bool PushBuffer(BufferHandle buffer) override
            { return Queue::Push(buffer); }
            virtual bool PullBuffer(BufferHandle& buffer) override
            { return Queue::Pull(buffer); }
            virtual std::string GetName() const override
            { return mName; }
            virtual Format GetFormat() const override
            { return mFormat; }
            virtual void SetFormat(const Format& format) override
            { mFormat = format; }
            virtual bool CanAccept(const Format& format) const override
            {
                return true;
            }
            virtual bool HasBuffers() const override
            { return Queue::HasBuffers(); }
        private:
            const std::string mName;
            Format mFormat;
        };
    } // namespace

    using SingleSlotPort = detail::BasicPort<detail::SingleSlotQueue>;

    // Audio processing element. Each element can have multiple input
    // and output ports with various port format settings. During
    // audio processing the element will normally read a buffer of PCM
    // data from its input ports, perform processing on the data and
    // then push the results out into its output ports.
    class Element
    {
    public:
        using Command = audio::Command;
        using Event   = audio::Event;
        using EventQueue = std::queue<std::unique_ptr<Event>>;

        virtual ~Element() = default;
        // Get the machine generated immutable ID of the element.
        virtual std::string GetId() const = 0;
        // Get the human readable name of the element.
        virtual std::string GetName() const = 0;
        // Returns whether the element is done producing audio buffers.
        virtual bool IsSourceDone() const { return false; }
        // Return whether the element is a source element, i.e. producer of
        // audio buffers.
        virtual bool IsSource() const { return false; }
        // Prepare the element for processing. Returns true if
        // successful or false on error and error details are logged.
        virtual bool Prepare() { return true; }
        // Request the element to process 'milliseconds' worth of
        // audio data. Any non-source input will likely ignore the
        // milliseconds parameter and process data in whole buffers
        // instead. EventQueue is the queue of outgoing events generated
        // by the element (and it's descendants).
        virtual void Process(EventQueue& events, unsigned milliseconds) {}
        // Update the element by dt time step in seconds.
        virtual void Update(float dt) {}
        // Perform element shutdown/cleanup.
        virtual void Shutdown() {}
        // Get the number of input ports this element has.
        virtual unsigned GetNumInputPorts() const
        { return 0; }
        // Get the number of output ports this element has.
        virtual unsigned GetNumOutputPorts() const
        { return 0; }
        // Get an input port at the given index. The index must be valid.
        virtual Port& GetInputPort(unsigned index)
        { BUG("No such input port index."); }
        // Get an ouput port at the given index. The index must be valid.
        virtual Port& GetOutputPort(unsigned index)
        { BUG("No such output port index."); }
        // Receive and handle an element specific command.
        virtual void ReceiveCommand(Command& cmd)
        { BUG("Unexpected command."); }
        virtual bool DispatchCommand(const std::string& dest, Command& cmd)
        { return false; }

        // Find an input port by name. Returns nullptr if no such port.
        Port* FindInputPortByName(const std::string& name)
        { return FindInputPort([name](auto& port) { return port.GetName() == name; }); }
        // Find an input port by name. Returns nullptr if no such port.
        Port* FindOutputPortByName(const std::string& name)
        { return FindOutputPort([name](auto& port) { return port.GetName() == name; }); }

        bool HasInputPort(Port* port)
        { return FindInputPort([port](auto& other) { return &other == port; }); }
        bool HasOutputPort(Port* port)
        { return FindOutputPort([port](auto& other) { return &other == port; }); }

        template<typename CmdT> static
        std::unique_ptr<Command> MakeCommand(CmdT&& cmd)
        {
            using CmdType = detail::MessageImpl<std::remove_reference_t<CmdT>, 0>;
            auto ret = std::make_unique<CmdType>(std::forward<CmdT>(cmd));
            return ret;
        }

    private:
        template<typename Predicate>
        Port* FindInputPort(const Predicate& predicate)
        {
            for (unsigned i=0; i<GetNumInputPorts(); ++i)
            {
                auto& port = GetInputPort(i);
                if (predicate(port))
                    return &port;
            }
            return nullptr;
        }
        template<typename Predicate>
        Port* FindOutputPort(const Predicate& predicate)
        {
            for (unsigned i=0; i<GetNumOutputPorts(); ++i)
            {
                auto& port = GetOutputPort(i);
                if (predicate(port))
                    return &port;
            }
            return nullptr;
        }
    private:
    };

    // Turn a possible mono audio stream into a stereo stream.
    // If the input is already a stereo stream nothing is done.
    class StereoMaker : public Element
    {
    public:
        enum class Channel {
            Left = 0, Right = 1
        };
        StereoMaker(const std::string& name, Channel which = Channel::Left);
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual bool Prepare() override;
        virtual void Process(EventQueue& events, unsigned milliseconds) override;
        virtual unsigned GetNumInputPorts() const override
        { return 1; }
        virtual unsigned GetNumOutputPorts() const override
        { return 1; }
        virtual Port& GetInputPort(unsigned index) override
        {
            if (index == 0) return mIn;
            BUG("No such input port index.");
        }
        virtual Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mOut;
            BUG("No such output port index.");
        }
    private:
        template<typename Type>
        void CopyMono(BufferHandle buffer);
    private:
        const std::string mName;
        const std::string mId;
        const Channel mChannel;
        SingleSlotPort mOut;
        SingleSlotPort mIn;
    };

    // Join 2 mono streams into a stereo stream.
    // The streams to be joined must have the same
    // underlying type, sample rate and channel count.
    class StereoJoiner : public Element
    {
    public:
        StereoJoiner(const std::string& name);
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual bool Prepare() override;
        virtual void Process(EventQueue& events, unsigned milliseconds) override;
        virtual unsigned GetNumInputPorts() const override
        { return 1; }
        virtual unsigned GetNumOutputPorts() const override
        { return 2; }
        virtual Port& GetInputPort(unsigned index) override
        {
            if (index == 0) return mInLeft;
            if (index == 1) return mInRight;
            BUG("No such input port index.");
        }
        virtual Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mOut;
            BUG("No such output port index.");
        }
    private:
        template<typename Type>
        void Join(BufferHandle left, BufferHandle right);
    private:
        const std::string mName;
        const std::string mId;
        SingleSlotPort mOut;
        SingleSlotPort mInLeft;
        SingleSlotPort mInRight;
    };

    // Split a stereo stream into left and right channel streams.
    class StereoSplitter : public Element
    {
    public:
        StereoSplitter(const std::string& name);
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual bool Prepare() override;
        virtual void Process(EventQueue& events, unsigned milliseconds) override;
        virtual unsigned GetNumInputPorts() const override
        { return 1; }
        virtual unsigned GetNumOutputPorts() const override
        { return 2; }
        virtual Port& GetInputPort(unsigned index) override
        {
            if (index == 0) return mIn;
            BUG("No such input port index.");
        }
        virtual Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mOutLeft;
            if (index == 1) return mOutRight;
            BUG("No such output port index.");
        }
    private:
        template<typename Type>
        void Split(BufferHandle buffer);
    private:
        const std::string mName;
        const std::string mId;
        SingleSlotPort mIn;
        SingleSlotPort mOutLeft;
        SingleSlotPort mOutRight;
    };

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
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual bool Prepare() override
        { return true; }
        virtual void Process(EventQueue& events, unsigned milliseconds) override
        {
            BufferHandle buffer;
            mIn.PullBuffer(buffer);
        }
        virtual unsigned GetNumInputPorts() const override
        { return 1; }
        virtual Port& GetInputPort(unsigned index) override
        {
            if (index == 0) return mIn;
            BUG("No such input port index.");
        }
    private:
        const std::string mName;
        const std::string mId;
        SingleSlotPort mIn;
    };

    // Mix multiple audio streams into a single stream.
    // The streams must all have the same format, i.e. the same
    // underlying type, sample rate and channel count.
    // This compatibility can be achieved through resampler.
    class Mixer : public Element
    {
    public:
        Mixer(const std::string& name, unsigned num_srcs = 2);
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual bool Prepare() override;
        virtual void Process(EventQueue& events, unsigned milliseconds) override;
        virtual unsigned GetNumOutputPorts() const override
        { return 1; }
        virtual unsigned GetNumInputPorts() const override
        { return mSrcs.size(); }
        virtual Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mOut;
            BUG("No such output port index.");
        }
        virtual Port& GetInputPort(unsigned index) override
        { return base::SafeIndex(mSrcs, index); }
    private:
        const std::string mName;
        const std::string mId;
        std::vector<SingleSlotPort> mSrcs;
        SingleSlotPort mOut;
    };

    // Manipulate audio stream gain over time in order to create a
    // fade-in or fade-out effect.
    class Fade : public Element
    {
    public:
        // The possible effect.
        enum class Effect {
            // Ramp up the stream gain from 0.0f to 1.0f
            FadeIn,
            // Ramp down the stream gain from 1.0f to 0.0f
            FadeOut
        };
        // Command to configure and set a new fade effect
        // parameters. The command will take effect from the
        // next input buffer onwards.
        struct SetFadeCmd {
            float duration = 0.0f;
            Effect effect  = Effect::FadeIn;
        };

        Fade(const std::string& name);
        Fade(const std::string& name, float duration, Effect effect);
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual bool Prepare() override;
        virtual void Process(EventQueue& events, unsigned milliseconds) override;
        virtual unsigned GetNumOutputPorts() const override
        { return 1; }
        virtual unsigned GetNumInputPorts() const override
        { return 1; }
        virtual Port& GetOutputPort(unsigned index) override
        {
            if (index == 0)  return mOut;
            BUG("No such output port.");
        }
        virtual Port& GetInputPort(unsigned index) override
        {
            if (index == 0) return mIn;
            BUG("No such input port.");
        }
        virtual void ReceiveCommand(Command& cmd) override;

        // Set the fade effect parameters. The effect will start
        // to place from the next input buffer onwards.
        void SetFade(Effect effect, float duration);
    private:
        template<typename DataType, unsigned ChannelCount>
        void AdjustGain(BufferHandle buffer);
    private:
        const std::string mName;
        const std::string mId;
        SingleSlotPort mIn;
        SingleSlotPort mOut;
        // fading in (1.0) or out (-1.0f).
        float mFadeDirection = 1.0f;
        // duration of the fading effect.
        float mDuration   = 0.0;
        // how far into the effect are we.
        float mSampleTime = 0.0;
        // current stream sample rate.
        unsigned mSampleRate = 0;
    };

    // Adjust the stream's gain (volume) setting.
    class Gain : public Element
    {
    public:
        struct SetGainCmd {
            float gain = 1.0f;
        };

        Gain(const std::string& name, float gain = 1.0f);
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual bool Prepare() override;
        virtual void Process(EventQueue& events, unsigned milliseconds) override;
        virtual unsigned GetNumOutputPorts() const override
        { return 1; }
        virtual unsigned GetNumInputPorts() const override
        { return 1; }
        virtual Port& GetOutputPort(unsigned index) override
        {
            if (index == 0)  return mOut;
            BUG("No such output port.");
        }
        virtual Port& GetInputPort(unsigned index) override
        {
            if (index == 0) return mIn;
            BUG("No such input port.");
        }
        virtual void ReceiveCommand(Command& cmd) override;
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

    // Resample the input stream.
    class Resampler : public Element
    {
    public:
        Resampler(const std::string& name, unsigned sample_rate);
       ~Resampler();
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual bool Prepare() override;
        virtual void Process(EventQueue& events, unsigned milliseconds) override;

        virtual unsigned GetNumOutputPorts() const override
        { return 1; }
        virtual unsigned GetNumInputPorts() const override
        { return 1; }
        virtual Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mOut;
            BUG("No such output port.");
        }
        virtual Port& GetInputPort(unsigned index) override
        {
            if (index == 0) return mIn;
            BUG("No such input port.");
        }
    private:
        const std::string mName;
        const std::string mId;
        const unsigned mSampleRate = 0;
        SingleSlotPort mIn;
        SingleSlotPort mOut;
    private:
        SRC_STATE* mState = nullptr;
    };

    class FileSource : public Element
    {
    public:
        FileSource(const std::string& name, const std::string& file, SampleType type = SampleType::Int16);
        FileSource(FileSource&& other);
       ~FileSource();
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual bool Prepare() override;
        virtual void Process(EventQueue& events, unsigned milliseconds) override;
        virtual void Shutdown() override;
        virtual bool IsSourceDone() const override;
        virtual bool IsSource() const override
        { return true; }
        virtual unsigned GetNumOutputPorts() const override
        { return 1; }
        virtual Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mPort;
            BUG("No such output port.");
        }
    private:
        const std::string mName;
        const std::string mId;
        const std::string mFile;
        std::unique_ptr<Decoder> mDecoder;
        SingleSlotPort mPort;
        Format mFormat;
        unsigned mFramesRead = 0;
    };

    class BufferSource : public Element
    {
    public:
        enum class Format {
            Mp3, Ogg, Flac, Wav
        };
        BufferSource(const std::string& name, std::unique_ptr<Buffer> buffer,
                     Format format, SampleType type = SampleType::Int16);
        BufferSource(BufferSource&& other);
       ~BufferSource();
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual bool Prepare() override;
        virtual void Process(EventQueue& events, unsigned milliseconds) override;
        virtual void Shutdown() override;
        virtual bool IsSourceDone() const override;
        virtual bool IsSource() const override
        { return true; }
        virtual unsigned GetNumOutputPorts() const override
        { return 1; }
        virtual Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mPort;
            BUG("No such output port.");
        }
    private:
        const std::string mName;
        const std::string mId;
        const Format mInputFormat;
        std::unique_ptr<Buffer> mBuffer;
        std::unique_ptr<Decoder> mDecoder;
        SingleSlotPort mPort;
        audio::Format mOutputFormat;
        unsigned mFramesRead = 0;
    };

    // MixerSource wraps multiple (source) elements into a single
    // source. Each source to be added must have the same format
    // that has been set in the mixer. Once the sources have been
    // added their state can be controlled through mixer commands.
    class MixerSource : public Element
    {
    public:
        // Command to add a new source stream to the mixer.
        // The element needs to be a source, i.e. IsSource is true.
        // Subsequent commands can refer to the same source
        // through its name.
        struct AddSourceCmd {
            std::unique_ptr<Element> src;
            bool paused = false;
        };
        // Commands to modify the state of the source identified
        // by its name. If no such source is found then nothing
        // is done.

        // Delete the source.
        struct DeleteSourceCmd {
            std::string name;
        };
        // Add some input processing delay to the source.
        struct DelaySourceCmd {
            std::string name;
            float delay = 0.0f;
        };
        // Pause/Resume the source. When the source is paused
        // no buffers are pulled from it and the source
        // is no longer mixed into the output stream.
        struct PauseSourceCmd {
            std::string name;
            // Flag to indicate whether to pause / resume the src.
            bool paused = false;
        };

        struct SourceDoneEvent {
            std::string mixer;
            std::unique_ptr<Element> src;
        };

        // Create a new mixer with the given name and format.
        MixerSource(const std::string& name, const Format& format);
        MixerSource(MixerSource&& other);

        // Add a new source element to the mixer. The element must be a source
        // and have at least 1 output port with the same format that the
        // mixer source itself has.
        Element* AddSourcePtr(std::unique_ptr<Element> source, bool paused=false);

        template<typename Source>
        Source* AddSource(Source&& source, bool paused=false)
        {
            auto src = std::make_unique<std::remove_reference_t<Source>>(std::forward<Source>(source));
            auto* ret = AddSourcePtr(std::move(src));
            return static_cast<Source*>(ret);
        }

        // Delete the named source from the mixer.
        void DeleteSource(const std::string& name);
        // Add a processing delay to the source. No buffers are pulled
        // from the source until the delay elapses.
        void DelaySource(const std::string& name, float delay);
        // Pause/resume the named source. If already paused/playing nothing is done.
        void PauseSource(const std::string& name, bool paused);

        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual bool IsSource() const override { return true; }
        virtual bool IsSourceDone() const override;
        virtual bool Prepare() override;
        virtual void Update(float dt) override;
        virtual void Process(EventQueue& events, unsigned milliseconds) override;
        virtual unsigned GetNumOutputPorts() const override
        { return 1; }
        virtual Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mOut;
            BUG("No such output port index.");
        }
        virtual void ReceiveCommand(Command& cmd) override;
        virtual bool DispatchCommand(const std::string& dest, Command& cmd) override;
    private:
        const std::string mName;
        const std::string mId;
        const Format mFormat;
        struct Source {
            std::unique_ptr<Element> element;
            bool paused = false;
            float delay = 0.0;
        };
        std::unordered_map<std::string, Source> mSources;
        SingleSlotPort mOut;
    };

    // Generate endless audio buffers with 0 (silence) for audio content.
    class ZeroSource : public Element
    {
    public:
        ZeroSource(const std::string& name, const Format& format);
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual bool IsSource() const override { return true; }
        virtual bool IsSourceDone() const override { return false; }
        virtual bool Prepare() override;
        virtual void Process(EventQueue& events, unsigned milliseconds) override;
        virtual unsigned GetNumOutputPorts() const override
        { return 1; }
        virtual Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mOut;
            BUG("No such output port index.");
        }
    private:
        const std::string mName;
        const std::string mId;
        const Format mFormat;
        SingleSlotPort mOut;
    };

#ifdef AUDIO_ENABLE_TEST_SOUND
    class SineSource : public Element
    {
    public:
        SineSource(const std::string& name, unsigned frequency);
        SineSource(const std::string& name, unsigned frequency, unsigned millisecs);
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual bool IsSourceDone() const override
        {
            if (!mLimitDuration)
                return false;
            return mMilliSecs >= mDuration;
        }
        virtual bool IsSource() const override
        { return true; }
        virtual unsigned GetNumOutputPorts() const override
        { return 1; }
        virtual Port& GetOutputPort(unsigned index) override
        { return mPort; }
        virtual bool Prepare() override;
        virtual void Process(EventQueue& events, unsigned milliseconds) override;
        void SetSampleType(SampleType type)
        { mFormat.sample_type = type; }
    private:
        const std::string mName;
        const std::string mId;
        const unsigned mFrequency = 0;
        const unsigned mDuration  = 0;
        const bool mLimitDuration = false;
        unsigned mMilliSecs   = 0;
        unsigned mSampleCount = 0;
        SingleSlotPort mPort;
        Format mFormat;
    };
#endif

} // namespace
