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

namespace audio
{

AudioFile::AudioFile(const std::string& filename, const std::string& name)
    : filename_(filename)
    , name_(name)
{}
AudioFile::~AudioFile() = default;

unsigned AudioFile::GetRateHz() const noexcept
{ return decoder_->GetSampleRate(); }

unsigned AudioFile::GetNumChannels() const noexcept
{ return decoder_->GetNumChannels(); }

Source::Format AudioFile::GetFormat() const noexcept
{ return format_; }

std::string AudioFile::GetName() const noexcept
{ return name_; }

unsigned AudioFile::FillBuffer(void* buff, unsigned max_bytes) 
{
    const auto num_channels = decoder_->GetNumChannels();
    const auto frame_size = num_channels * ByteSize(format_);
    const auto possible_num_frames = max_bytes / frame_size;
    const auto frames_available = decoder_->GetNumFrames() - frames_;
    const auto frames_to_read = std::min((unsigned)frames_available,
                                         (unsigned)possible_num_frames);
    // change the output format into floats.
    // see this bug that confirms crackling playback wrt ogg files.
    // https://github.com/UniversityRadioYork/ury-playd/issues/111
    // sndfile-play however uses floats and is able to play
    // the same test ogg (mentioned in the bug) without crackles.
    size_t ret = 0;
    if (format_ == Format::Float32)
        ret = decoder_->ReadFrames((float*)buff, frames_to_read);
    else if (format_ == Format::Int32)
        ret = decoder_->ReadFrames((int*)buff, frames_to_read);
    else if (format_ == Format::Int16)
        ret = decoder_->ReadFrames((short*)buff, frames_to_read);
    if (ret != frames_to_read)
        WARN("Unexpected number of audio frames. %1 read vs. %2 expected.", ret, frames_to_read);
    frames_ += ret;
    return ret * frame_size;
}

bool AudioFile::HasNextBuffer(std::uint64_t num_bytes_read) const noexcept
{ return frames_ < decoder_->GetNumFrames(); }

bool AudioFile::Reset() noexcept
{
    return Open();
}

bool AudioFile::Open()
{
    std::unique_ptr<Decoder> decoder;
    const auto& upper = base::ToUpperUtf8(filename_);
    if (base::EndsWith(upper, ".MP3"))
    {
        SampleType type = SampleType::NotSet;
        if (format_ == Format::Float32)
            type = SampleType::Float32;
        else if (format_ == Format::Int32)
            type = SampleType::Int32;
        else if (format_ == Format::Int16)
            type = SampleType::Int16;
        else BUG("Unsupported format.");
        auto dec = std::make_unique<Mpg123Decoder>();
        if (!dec->Open(filename_, type))
            return false;
        decoder = std::move(dec);
    }
    else if (base::EndsWith(upper, ".OGG") ||
             base::EndsWith(upper, ".WAV") ||
             base::EndsWith(upper, ".FLAC"))
    {
        auto stream = std::make_unique<SndFileInputStream>();
        if (!stream->OpenFile(filename_))
            return false;
        decoder = std::make_unique<SndFileDecoder>(std::move(stream));
    }
    else
    {
        ERROR("Unsupported audio file '%1'", filename_);
        return false;
    }
    decoder_ = std::move(decoder);
    frames_ = 0;
    return true;
}

} // namespace
