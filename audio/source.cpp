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

#include "config.h"

#include <algorithm>
#include <stdexcept>
#include <cstring>

#include "base/assert.h"
#include "base/logging.h"
#include "base/utility.h"
#include "audio/source.h"
#include "audio/sndfile.h"
#include "audio/mpg123.h"
#include "audio/loader.h"

namespace audio
{

AudioFile::AudioFile(const std::string& filename, const std::string& name, Format format)
    : mFilename(filename)
    , mName(name)
    , mFormat(format)
{}
AudioFile::~AudioFile() = default;

unsigned AudioFile::GetRateHz() const noexcept
{ return mDecoder->GetSampleRate(); }

unsigned AudioFile::GetNumChannels() const noexcept
{ return mDecoder->GetNumChannels(); }

Source::Format AudioFile::GetFormat() const noexcept
{ return mFormat; }

std::string AudioFile::GetName() const noexcept
{ return mName; }

unsigned AudioFile::FillBuffer(void* buff, unsigned max_bytes) 
{
    const auto num_channels = mDecoder->GetNumChannels();
    const auto frame_size = num_channels * ByteSize(mFormat);
    const auto possible_num_frames = max_bytes / frame_size;
    const auto frames_available = mDecoder->GetNumFrames() - mFrames;
    const auto frames_to_read = std::min((unsigned)frames_available,
                                         (unsigned)possible_num_frames);
    // change the output format into floats.
    // see this bug that confirms crackling playback wrt ogg files.
    // https://github.com/UniversityRadioYork/ury-playd/issues/111
    // sndfile-play however uses floats and is able to play
    // the same test ogg (mentioned in the bug) without crackles.
    size_t ret = 0;
    if (mFormat == Format::Float32)
        ret = mDecoder->ReadFrames((float*)buff, frames_to_read);
    else if (mFormat == Format::Int32)
        ret = mDecoder->ReadFrames((int*)buff, frames_to_read);
    else if (mFormat == Format::Int16)
        ret = mDecoder->ReadFrames((short*)buff, frames_to_read);
    if (ret != frames_to_read)
        WARN("Unexpected number of audio frames. %1 read vs. %2 expected.", ret, frames_to_read);
    mFrames += ret;

    if (mFrames == mDecoder->GetNumFrames())
    {
        if (++mPlayCount != mLoopCount)
        {
            mDecoder->Reset();
            mFrames = 0;
            DEBUG("Audio file '%1' reset for looped playback (#%2).", mFilename, mPlayCount+1);
        }
    }
    return ret * frame_size;
}

bool AudioFile::HasMore(std::uint64_t num_bytes_read) const noexcept
{
    if (mFrames < mDecoder->GetNumFrames())
        return true;

    if (mPlayCount < mLoopCount)
        return true;

    return false;
}

void AudioFile::Shutdown() noexcept
{
    mDecoder.reset();
}

bool AudioFile::Open()
{
    auto stream = audio::OpenFileStream(mFilename);
    if (!stream)
        return false;

    std::unique_ptr<Decoder> decoder;

    const auto& upper = base::ToUpperUtf8(mFilename);
    if (base::EndsWith(upper, ".MP3"))
    {
        SampleType type = SampleType::Float32;
        if (mFormat == Format::Float32)
            type = SampleType::Float32;
        else if (mFormat == Format::Int32)
            type = SampleType::Int32;
        else if (mFormat == Format::Int16)
            type = SampleType::Int16;
        else BUG("Unsupported format.");
        auto dec = std::make_unique<Mpg123Decoder>();
        if (!dec->Open(stream, type))
            return false;
        decoder = std::move(dec);
    }
    else if (base::EndsWith(upper, ".OGG") ||
             base::EndsWith(upper, ".WAV") ||
             base::EndsWith(upper, ".FLAC"))
    {
        auto dec = std::make_unique<SndFileDecoder>();
        if (!dec->Open(stream))
            return false;
        decoder = std::move(dec);
    }
    else
    {
        ERROR("Unsupported audio file format. [file='%1']", mFilename);
        return false;
    }
    mDecoder = std::move(decoder);
    mFrames = 0;
    return true;
}

} // namespace
