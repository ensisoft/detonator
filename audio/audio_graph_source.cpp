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

#include "config.h"

#include "base/format.h"
#include "base/logging.h"
#include "base/hash.h"
#include "base/trace.h"

#include "audio/element.h"
#include "audio/command.h"
#include "audio/audio_graph_source.h"

namespace audio
{
struct AudioGraphSource::GraphCmd {
    std::unique_ptr<Element::Command> cmd;
    std::string dest;
};

AudioGraphSource::AudioGraphSource(const std::string& name)
  : mName(name)
  , mGraph(name)
{}

AudioGraphSource::AudioGraphSource(std::string name, Graph&& graph)
  : mName(std::move(name))
  , mGraph(std::move(graph))
{}

AudioGraphSource::AudioGraphSource(AudioGraphSource&& other) noexcept
  : mName(std::move(other.mName))
  , mGraph(std::move(other.mGraph))
{}

bool AudioGraphSource::Prepare(const Loader& loader, const PrepareParams& params)
{
    if (!mGraph.Prepare(loader, params))
        return false;
    auto& out = mGraph.GetOutputPort(0);
    mFormat = out.GetFormat();
    return true;
}

unsigned AudioGraphSource::GetRateHz() const noexcept
{ return mFormat.sample_rate; }

unsigned AudioGraphSource::GetNumChannels() const noexcept
{ return mFormat.channel_count; }

Source::Format AudioGraphSource::GetFormat() const noexcept
{
    if (mFormat.sample_type == SampleType::Int16)
        return Source::Format::Int16;
    else if (mFormat.sample_type == SampleType::Int32)
        return Source::Format::Int32;
    else if (mFormat.sample_type == SampleType::Float32)
        return Source::Format::Float32;
    else BUG("Incorrect audio format.");
    return Source::Format::Float32;
}
std::string AudioGraphSource::GetName() const noexcept
{ return mName; }

unsigned AudioGraphSource::FillBuffer(void* buff, unsigned max_bytes)
{
    // todo: figure out a way to get rid of this copying whenever possible.

    // compute how many milliseconds worth of data can we
    // stuff into the current buffer. todo: should the buffering
    // sizes be fixed somewhere somehow? I.e. we're always dispatching
    // let's say 5ms worth of audio or whatever ?
    const auto millis_in_bytes = GetMillisecondByteCount(mFormat);
    const auto milliseconds = max_bytes / millis_in_bytes;
    const auto bytes = millis_in_bytes * milliseconds;
    ASSERT(bytes <= max_bytes);

    if (mPendingBuffer)
    {
        TRACE_SCOPE("PendingBuffer");
        const auto pending = mPendingBuffer->GetByteSize() - mPendingOffset;
        const auto min_bytes = std::min(pending, (size_t)max_bytes);
        const auto* ptr = static_cast<const uint8_t*>(mPendingBuffer->GetPtr());
        std::memcpy(buff, ptr + mPendingOffset, min_bytes);
        mPendingOffset += min_bytes;
        if (mPendingOffset == mPendingBuffer->GetByteSize())
        {
            mPendingBuffer.reset();
            mPendingOffset = 0;
        }
        return min_bytes;
    }

    class Allocator : public BufferAllocator {
    public:
        Allocator(void* native_buffer, unsigned native_buffer_size)
        {
            mDeviceBuffer = std::make_shared<BufferView>(native_buffer,
                                                         native_buffer_size);
        }
        BufferHandle Allocate(size_t bytes) override
        {
            TRACE_SCOPE("AllocateBuffer");
            if (mDeviceBuffer && mDeviceBuffer->GetCapacity() >= bytes)
            {
                auto ret  = mDeviceBuffer;
                mDeviceBuffer.reset();
                return ret;
            }
            return std::make_shared<VectorBuffer>(bytes);
            mDeviceBuffer.reset();
        }
    private:
        BufferHandle mDeviceBuffer;
    } allocator(buff, max_bytes);

    TRACE_CALL("Graph::Process", mGraph.Process(allocator, mEvents, milliseconds));
    TRACE_CALL("Graph::Advance", mGraph.Advance(milliseconds));
    mMillisecs += milliseconds;

    BufferHandle buffer;
    auto& port = mGraph.GetOutputPort(0);
    if (port.PullBuffer(buffer))
    {
        const auto min_bytes = std::min(buffer->GetByteSize(), (size_t)max_bytes);
        // if the graph's output buffer is a buffer view for the buffer
        // given to us by the audio API then we don't need to copy any
        // data around and we're on the happy path!
        if (buffer->GetPtr() == buff)
            return min_bytes;
        // if the graph's output buffer is not the native buffer then
        // a copy must be done unfortunately. it's also possible that the
        // output buffer is now larger than the maximum device's PCM buffer.
        // This can happen if the graph delivers a late buffer that has been
        // queued before.
        std::memcpy(buff, buffer->GetPtr(), min_bytes);
        if (min_bytes < buffer->GetByteSize())
        {
            ASSERT(!mPendingBuffer && !mPendingOffset);
            mPendingBuffer = buffer;
            mPendingOffset = min_bytes;
        }
        return min_bytes;
    }
    else if (!mGraph.IsSourceDone())
    {
        // currently if the audio graph isn't producing any data the pulseaudio
        // playback stream will automatically go into paused state. (done by pulseaudio)
        // right now we don't have a mechanism to signal the resumption of the
        // playback i.e. resume the PA stream in order to have another stream
        // write callback on time when the graph is producing data again.
        // So in case there's no graph output but the graph is not yet finished
        // we just fill the buffer with 0s and return that.
        std::memset(buff, 0, max_bytes);
        return max_bytes;
    }

    WARN("Audio graph has no output audio buffer available. [graph=%1]", mName);
    return 0;
}
bool AudioGraphSource::HasMore(std::uint64_t num_bytes_read) const noexcept
{
    return mPendingBuffer || !mGraph.IsSourceDone();
}

void AudioGraphSource::Shutdown() noexcept
{
    mGraph.Shutdown();
}

void AudioGraphSource::RecvCommand(std::unique_ptr<Command> cmd) noexcept
{
    if (auto* ptr = cmd->GetIf<GraphCmd>())
    {
        if (!mGraph.DispatchCommand(ptr->dest, *ptr->cmd))
            WARN("Audio graph command receiver element not found. [graph=%1, elem=%2]", mName, ptr->dest);
    }
    else BUG("Unexpected command.");
}

std::unique_ptr<Event> AudioGraphSource::GetEvent() noexcept
{
    if (mEvents.empty()) return nullptr;
    auto ret = std::move(mEvents.front());
    mEvents.pop();
    return ret;
}

// static
std::unique_ptr<Command> AudioGraphSource::MakeCommandPtr(const std::string& destination, std::unique_ptr<Element::Command>&& cmd)
{
    GraphCmd foo;
    foo.dest = destination;
    foo.cmd  = std::move(cmd);
    return audio::MakeCommand(std::move(foo));
}

} // namespace
