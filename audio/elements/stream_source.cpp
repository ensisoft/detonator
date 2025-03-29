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

#include "base/assert.h"
#include "base/logging.h"
#include "base/trace.h"
#include "base/utility.h"
#include "audio/mpg123.h"
#include "audio/sndfile.h"
#include "audio/elements/stream_source.h"

namespace audio
{

StreamSource::StreamSource(std::string name, std::shared_ptr<const SourceStream> buffer,
                           Format format, SampleType type)
  : mName(std::move(name))
  , mId(base::RandomString(10))
  , mInputFormat(format)
  , mBuffer(std::move(buffer))
  , mPort("out")
{
    mOutputFormat.sample_type = type;
}

StreamSource::StreamSource(StreamSource&& other)
  : mName(other.mName)
  , mId(other.mId)
  , mInputFormat(other.mInputFormat)
  , mBuffer(std::move(other.mBuffer))
  , mDecoder(std::move(other.mDecoder))
  , mPort(std::move(other.mPort))
  , mOutputFormat(std::move(other.mOutputFormat))
  , mFramesRead(other.mFramesRead)
{}

StreamSource::~StreamSource() = default;

bool StreamSource::Prepare(const Loader& loader, const PrepareParams& params)
{
    std::unique_ptr<Decoder> decoder;
    if (mInputFormat == Format::Mp3)
    {
        auto dec = std::make_unique<Mpg123Decoder>();
        if (!dec->Open(mBuffer, mOutputFormat.sample_type))
            return false;
        decoder = std::move(dec);
    }
    else
    {
        auto dec = std::make_unique<SndFileDecoder>();
        if (!dec->Open(mBuffer))
            return false;
        decoder = std::move(dec);
    }
    audio::Format format;
    format.channel_count = decoder->GetNumChannels();
    format.sample_rate   = decoder->GetSampleRate();
    format.sample_type   = mOutputFormat.sample_type;
    DEBUG("Audio buffer source prepared successfully. [elem=%1, output=%2]", mName, format);
    mDecoder = std::move(decoder);
    mPort.SetFormat(format);
    mOutputFormat = format;
    return true;
}
void StreamSource::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    const auto frame_size = GetFrameSizeInBytes(mOutputFormat);
    const auto frames_per_ms = mOutputFormat.sample_rate / 1000;
    const auto frames_wanted = (unsigned)(frames_per_ms * milliseconds);
    const auto frames_available = mDecoder->GetNumFrames();
    const auto frames = std::min(frames_available - mFramesRead, frames_wanted);

    auto buffer = allocator.Allocate(frame_size * frames);
    buffer->SetFormat(mOutputFormat);
    buffer->SetByteSize(frame_size * frames);
    void* buff = buffer->GetPtr();

    size_t ret = 0;
    if (mOutputFormat.sample_type == SampleType::Float32)
        ret = mDecoder->ReadFrames((float*)buff, frames);
    else if (mOutputFormat.sample_type == SampleType::Int32)
        ret = mDecoder->ReadFrames((int*)buff, frames);
    else if (mOutputFormat.sample_type == SampleType::Int16)
        ret = mDecoder->ReadFrames((short*)buff, frames);

    if (ret != frames)
    {
        WARN("Unexpected number of audio frames decoded. [elem=%1, expected=%2, decoded=%3]", mName, frames, ret);
    }

    mFramesRead += ret;
    if (mFramesRead == frames_available)
    {
        DEBUG("Audio buffer source is done. [elem=%1]", mName);
    }

    mPort.PushBuffer(buffer);
}

void StreamSource::Shutdown()
{
    mDecoder.reset();
}

bool StreamSource::IsSourceDone() const
{
    return mFramesRead == mDecoder->GetNumFrames();
}

} // namespace