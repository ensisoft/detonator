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
#include <cstring> // for memcpy
#include <cstdio>
#include <sndfile.h>

#include "base/assert.h"
#include "audio/source.h"

namespace audio
{

// Audio buffer using and reading buffers of encoded data in various
// formats and converting into PCM using sndfile library.
class SndfileAudioBuffer
{
public:
    using u8 = std::uint8_t;

    // Given the buffer of incoming "raw" data, use sndfile library
    // to convert the contents into PCM.
    // After the constructor returns ptr can be freed.
    SndfileAudioBuffer(std::vector<u8>&& buffer) : buffer_(std::move(buffer))
    {}
    
    SndfileAudioBuffer(const SndfileAudioBuffer&) = delete;
   ~SndfileAudioBuffer()
    {
        ::sf_close(file_);
    }

    unsigned FillBuffer(void* buff, unsigned max_bytes) const 
    {
        const auto frame_size = num_channels_ * sizeof(float);
        const auto possible_num_frames = max_bytes / frame_size;
        // change the output format into floats.
        // see this bug that confirms crackling playback wrt ogg files.
        // https://github.com/UniversityRadioYork/ury-playd/issues/111
        // sndfile-play however uses floats and is able to play
        // the same test ogg (mentioned in the bug) without crackles.
        const auto actual_num_frames = sf_readf_float(file_, (float*)buff, 
            possible_num_frames);
        return actual_num_frames * frame_size;
    }
    unsigned GetNumFrames() const
    { return num_frames_; }
    unsigned GetSize() const
    { return buffer_.size(); }
    unsigned GetSampleRate() const
    { return sample_rate_; }
    unsigned GetNumChannels() const
    { return num_channels_; }

    void Open()
    {
        if (file_)
            ::sf_close(file_);

        offset_ = 0;

        // set up a virtual IO device for libsound and then read all the frames
        // into a conversion buffer so we have the raw data.
        SF_VIRTUAL_IO io = {};
        io.get_filelen = ioGetLength;
        io.seek        = ioSeek;
        io.read        = ioRead;
        io.tell        = ioTell;

        SF_INFO sfinfo  = {};
        file_ = sf_open_virtual(&io, SFM_READ, &sfinfo, (void*)this);
        if (!file_)
            throw std::runtime_error("sf_open_virtual failed");

        sample_rate_  = sfinfo.samplerate;
        num_channels_ = sfinfo.channels;
        num_frames_   = sfinfo.frames;                   
    }

private:
    static sf_count_t ioGetLength(void* user)
    {
        const auto* this_ = static_cast<SndfileAudioBuffer*>(user);
        return this_->buffer_.size();
    }

    static sf_count_t ioSeek(sf_count_t offset, int whence, void* user)
    {
        auto* this_ = static_cast<SndfileAudioBuffer*>(user);
        switch (whence)
        {
            case SEEK_CUR:
                this_->offset_ += offset;
                break;
            case SEEK_SET:
                this_->offset_ = offset;
                break;
            case SEEK_END:
                this_->offset_ = this_->buffer_.size() - offset;
                break;
        }
        return this_->offset_;
    }

    static sf_count_t ioRead(void* ptr, sf_count_t count, void* user)
    {
        auto* this_ = static_cast<SndfileAudioBuffer*>(user);

        const auto num_bytes = this_->buffer_.size();
        const auto num_avail = num_bytes - this_->offset_;
        const auto num_bytes_to_read = std::min((sf_count_t)num_avail, count);
        if (num_bytes_to_read == 0)
            return 0;

        std::memcpy(ptr, &this_->buffer_[this_->offset_], num_bytes_to_read);
        this_->offset_ += num_bytes_to_read;
        return num_bytes_to_read;
    }

    static sf_count_t ioTell(void* user)
    {
        const auto* this_ = static_cast<SndfileAudioBuffer*>(user);
        return this_->offset_;
    }
private:
    SNDFILE* file_ = nullptr;
private:
    unsigned sample_rate_  = 0;
    unsigned num_channels_ = 0;
    unsigned num_frames_   = 0;
private:
    // Buffer containing the source data.
    const std::vector<u8> buffer_;
    sf_count_t offset_ = 0; // offset into the buffer data above
};

AudioFile::AudioFile(const std::string& filename, const std::string& name)
    : filename_(filename)
    , name_(name)
{
    FILE* fptr = std::fopen(filename.c_str(), "rb");
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
    buffer_ = std::make_unique<SndfileAudioBuffer>(std::move(buff));
    buffer_->Open();
}

AudioFile::~AudioFile() = default;

unsigned AudioFile::GetRateHz() const 
{
    return buffer_->GetSampleRate();
}

unsigned AudioFile::GetNumChannels() const 
{
    return buffer_->GetNumChannels();
}

std::string AudioFile::GetName() const 
{
    if (name_.empty())
        return filename_;
    return name_;
}

unsigned AudioFile::FillBuffer(void* buff, unsigned max_bytes) 
{ 
    return buffer_->FillBuffer(buff, max_bytes); 
}

bool AudioFile::HasNextBuffer(std::uint64_t num_bytes_read) const 
{
    const auto num_frames = buffer_->GetNumFrames();
    const auto num_channels = buffer_->GetNumChannels();
    const auto num_bytes = num_frames * num_channels * sizeof(float);
    if (num_bytes == num_bytes_read)
        return false;
    return true;
}

void AudioFile::Reset() 
{
    buffer_->Open();
}

} // namespace
