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

namespace {
using namespace audio;
// trampoline functions to convert C style callbacks into
// c++ this calls.
struct Trampoline {
    static sf_count_t GetLength(void* user)
    { return static_cast<SndFileIODevice*>(user)->GetLength(); }
    static sf_count_t Seek(sf_count_t offset, int whence, void* user)
    { return static_cast<SndFileIODevice*>(user)->Seek(offset, whence); }
    static sf_count_t Read(void* ptr, sf_count_t count, void* user)
    { return static_cast<SndFileIODevice*>(user)->Read(ptr, count); }
    static sf_count_t Tell(void* user)
    { return static_cast<SndFileIODevice*>(user)->Tell(); }
};
} // namespace

namespace audio
{

SndFileDecoder::SndFileDecoder(std::unique_ptr<SndFileIODevice> io)
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

bool SndFileDecoder::Open(std::unique_ptr<SndFileIODevice> io)
{
    ASSERT(mDevice == nullptr);
    ASSERT(mFile   == nullptr);

    SF_VIRTUAL_IO virtual_io = {};
    virtual_io.get_filelen = &Trampoline::GetLength;
    virtual_io.seek        = &Trampoline::Seek;
    virtual_io.read        = &Trampoline::Read;
    virtual_io.tell        = &Trampoline::Tell;
    SF_INFO info = {};
    mFile = sf_open_virtual(&virtual_io, SFM_READ, &info, (void*)io.get());
    if (!mFile)
    {
        ERROR("SndFile decoder open failed. [name='%1']", io->GetName());
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
    mDevice     = std::move(io);
    DEBUG("SndFileDecoder successfully opened. [name='%1' frames=%2, channels=%3, rate=%4]",
          mDevice->GetName(), mFrames, mChannels, mSampleRate);
    return true;
}

SndFileInputStream::SndFileInputStream(const std::string& filename)
{
    if (!OpenFile(filename))
        throw std::runtime_error("Failed to open: " + filename);
}

bool SndFileInputStream::OpenFile(const std::string& filename)
{
    auto stream = base::OpenBinaryInputStream(filename);
    if (!stream.is_open()) ERROR_RETURN(false, "SndFileInputStream failed to open. [file='%1']", filename);

    UseStream(filename, std::move(stream));
    return true;
}

void SndFileInputStream::UseStream(const std::string& name, std::ifstream&& stream)
{
    ASSERT(stream.is_open());
    mFilename = name;
    mStream   = std::move(stream);

    mStream.seekg(0, mStream.end);
    mStreamSize = mStream.tellg();
    mStream.seekg(0, mStream.beg);
    DEBUG("SndFileInputStream using ifstream. [name='%1', bytes=%2]", name, mStreamSize);
}

std::int64_t SndFileInputStream::GetLength() const
{ return mStreamSize; }
std::int64_t SndFileInputStream::Seek(sf_count_t offset, int whence)
{
    if (whence == SEEK_CUR)
        mStream.seekg(offset, mStream.cur);
    else if (whence == SEEK_SET)
        mStream.seekg(offset, mStream.beg);
    else if (whence == SEEK_END)
        mStream.seekg(offset, mStream.end);
    else BUG("Unknown seek position.");
    return mStream.tellg();
}
std::int64_t SndFileInputStream::Read(void* ptr, sf_count_t count)
{
    const auto offset = mStream.tellg();
    const auto available = mStreamSize - offset;
    const auto bytes_to_read = std::min(available, count);
    if (bytes_to_read == 0)
        return 0;

    mStream.read((char*)ptr, bytes_to_read);
    if (mStream.gcount() != bytes_to_read)
        WARN("SndFileInputStream stream read failed. [request=%1, returned=%2]", bytes_to_read, mStream.gcount());
    return mStream.gcount();
}
std::int64_t SndFileInputStream::Tell() const
{ return (sf_count_t)mStream.tellg(); }

std::int64_t SndFileBuffer::GetLength() const
{ return mBuffer->GetSize(); }

std::int64_t SndFileBuffer::Seek(std::int64_t offset, int whence)
{
    if (whence == SEEK_CUR)
        mOffset += offset;
    else if (whence == SEEK_SET)
        mOffset = offset;
    else if (whence == SEEK_END)
        mOffset = mBuffer->GetSize() - offset;
    else BUG("Unknown seek position");
    return mOffset;
}
std::int64_t SndFileBuffer::Read(void* ptr, std::int64_t count)
{
    const auto num_bytes = mBuffer->GetSize();
    const auto num_avail = num_bytes - mOffset;
    const auto num_bytes_to_read = std::min((std::int64_t)num_avail, count);
    if (num_bytes_to_read == 0)
        return 0;
    const auto* src = static_cast<const std::uint8_t*>(mBuffer->GetData());
    std::memcpy(ptr, &src[mOffset], num_bytes_to_read);
    mOffset += num_bytes_to_read;
    return num_bytes_to_read;
}
std::int64_t SndFileBuffer::Tell() const
{ return mOffset; }

} // namespace