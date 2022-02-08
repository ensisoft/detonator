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

#include <stdexcept>
#include <cstring>

#include <sndfile.h>

#include "base/utility.h"
#include "base/logging.h"
#include "audio/sndfile.h"
#include "audio/loader.h"

namespace audio
{
SndFileDecoder::SndFileDecoder(std::shared_ptr<const SourceStream> io)
{
    if (!Open(std::move(io)))
        throw std::runtime_error("SndFileDecoder open failed.");
}

SndFileDecoder::~SndFileDecoder()
{
    if (mFile)
        ::sf_close(mFile);
}

size_t SndFileDecoder::ReadFrames(float* ptr, size_t frames)
{ return sf_readf_float(mFile, ptr, frames); }
size_t SndFileDecoder::ReadFrames(short* ptr, size_t frames)
{ return sf_readf_short(mFile, ptr, frames); }
size_t SndFileDecoder::ReadFrames(int* ptr, size_t frames)
{ return sf_readf_int(mFile, ptr, frames); }

void SndFileDecoder::Reset()
{ sf_seek(mFile, 0, SEEK_SET); }

bool SndFileDecoder::Open(std::shared_ptr<const SourceStream> source)
{
    ASSERT(mSource == nullptr);
    ASSERT(mFile   == nullptr);
    struct Trampoline {
        static sf_count_t GetLength(void* user)
        { return static_cast<SndFileDecoder*>(user)->GetLength(); }
        static sf_count_t Seek(sf_count_t offset, int whence, void* user)
        { return static_cast<SndFileDecoder*>(user)->Seek(offset, whence); }
        static sf_count_t Read(void* ptr, sf_count_t count, void* user)
        { return static_cast<SndFileDecoder*>(user)->Read(ptr, count); }
        static sf_count_t Tell(void* user)
        { return static_cast<SndFileDecoder*>(user)->Tell(); }
    };
    mSource = source;

    SF_VIRTUAL_IO virtual_io = {};
    virtual_io.get_filelen = &Trampoline::GetLength;
    virtual_io.seek        = &Trampoline::Seek;
    virtual_io.read        = &Trampoline::Read;
    virtual_io.tell        = &Trampoline::Tell;
    SF_INFO info = {};
    mFile = sf_open_virtual(&virtual_io, SFM_READ, &info, (void*)this);
    if (!mFile)
    {
        ERROR("SndFile decoder open failed. [name='%1']", mSource->GetName());
        return false;
    }
    // When reading floating point wavs with the integer read functions
    // this flag needs to be set for proper conversion.
    // see Note2 @ http://www.mega-nerd.com/libsndfile/api.html#readf
    const auto cmd = SF_TRUE;
    sf_command(mFile, SFC_SET_SCALE_FLOAT_INT_READ, (void*)&cmd, sizeof(cmd));

    mSampleRate = info.samplerate;
    mChannels   = info.channels;
    mFrames     = info.frames;
    DEBUG("SndFileDecoder is open. [name='%1' frames=%2, channels=%3, rate=%4]",
          mSource->GetName(), mFrames, mChannels, mSampleRate);
    return true;
}

std::int64_t SndFileDecoder::GetLength() const
{ return mSource->GetSize(); }

std::int64_t SndFileDecoder::Seek(std::int64_t offset, int whence)
{
    if (whence == SEEK_CUR)
        mOffset += offset;
    else if (whence == SEEK_SET)
        mOffset = offset;
    else if (whence == SEEK_END)
        mOffset = mSource->GetSize() + offset;
    else BUG("Unknown seek position");
    mOffset = math::clamp(int64_t(0), int64_t(mSource->GetSize()), mOffset);
    return mOffset;
}
std::int64_t SndFileDecoder::Read(void* ptr, std::int64_t count)
{
    const auto num_bytes = mSource->GetSize();
    const auto num_avail = mOffset >= num_bytes ? 0 : num_bytes - mOffset;
    const auto num_bytes_to_read = std::min((std::int64_t)num_avail, count);
    if (num_bytes_to_read == 0)
        return 0;
    mSource->Read(ptr, mOffset, num_bytes_to_read);
    mOffset += num_bytes_to_read;
    return num_bytes_to_read;
}
std::int64_t SndFileDecoder::Tell() const
{ return mOffset; }

} // namespace
