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
#include <cstdio>

#include "base/assert.h"
#include "audio/source.h"
#include "audio/sndfile.h"

namespace audio
{

AudioFile::AudioFile(const std::string& filename, const std::string& name)
    : filename_(filename)
    , name_(name)
{
    Open();
}

unsigned AudioFile::GetRateHz() const 
{ return device_->GetSampleRate(); }

unsigned AudioFile::GetNumChannels() const 
{ return device_->GetNumChannels(); }

std::string AudioFile::GetName() const 
{
    if (name_.empty())
        return filename_;
    return name_;
}

unsigned AudioFile::FillBuffer(void* buff, unsigned max_bytes) 
{
    const auto num_channels = device_->GetNumChannels();
    const auto frame_size = num_channels * ByteSize(mFormat);
    const auto possible_num_frames = max_bytes / frame_size;
    // change the output format into floats.
    // see this bug that confirms crackling playback wrt ogg files.
    // https://github.com/UniversityRadioYork/ury-playd/issues/111
    // sndfile-play however uses floats and is able to play
    // the same test ogg (mentioned in the bug) without crackles.
    size_t ret = 0;
    if (mFormat == Format::Float32)
        ret = device_->ReadFrames((float*)buff, possible_num_frames);
    else if (mFormat == Format::Int32)
        ret = device_->ReadFrames((int*)buff, possible_num_frames);
    else if (mFormat == Format::Int16)
        ret = device_->ReadFrames((short*)buff, possible_num_frames);
    return ret * frame_size;
}

bool AudioFile::HasNextBuffer(std::uint64_t num_bytes_read) const 
{
    const auto num_frames = device_->GetNumFrames();
    const auto num_channels = device_->GetNumChannels();
    const auto num_bytes = num_frames * num_channels * sizeof(float);
    if (num_bytes == num_bytes_read)
        return false;
    return true;
}

void AudioFile::Reset()
{
    Open();
}

void AudioFile::Open()
{
    FILE* fptr = std::fopen(filename_.c_str(), "rb");
    if (!fptr)
        throw std::runtime_error("open audio file failed");

    // read the whole file into a single buffer.
    std::fseek(fptr, 0, SEEK_END);
    const long size = std::ftell(fptr);
    std::fseek(fptr, 0, SEEK_SET);

    std::vector<std::uint8_t> buff;
    buff.resize(size);
    std::fread(&buff[0], 1, size, fptr);
    std::fclose(fptr);

    // allocate just a single buffer that does the
    // data conversion from the buffer that we read from the file
    // and which contains the encoded data (for example in ogg vorbis)
    auto buffer = std::make_unique<SndFileBuffer>(std::move(buff));
    auto device = std::make_unique<SndFileVirtualDevice>(std::move(buffer));
    device_ = std::move(device);
}

} // namespace
