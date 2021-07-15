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

SndFileDecoder::SndFileDecoder(std::unique_ptr<SndFileIODevice> device)
{
    SF_VIRTUAL_IO io = {};
    io.get_filelen = &Trampoline::GetLength;
    io.seek        = &Trampoline::Seek;
    io.read        = &Trampoline::Read;
    io.tell        = &Trampoline::Tell;
    SF_INFO info = {};
    mFile = sf_open_virtual(&io, SFM_READ, &info, (void*)device.get());
    if (!mFile)
        throw std::runtime_error("sf_open_virtual failed.");
    // When reading floating point wavs with the integer read functions
    // this flag needs to be set for proper conversion.
    // see Note2 @ http://www.mega-nerd.com/libsndfile/api.html#readf
    const auto cmd = SF_TRUE;
    sf_command(mFile, SFC_SET_SCALE_FLOAT_INT_READ, (void*)&cmd, sizeof(cmd));

    mSampleRate = info.samplerate;
    mChannels   = info.channels;
    mFrames     = info.frames;
    mDevice = std::move(device);
    DEBUG("SndFileDecoder stream has %1 PCM frames in %2 channel(s) @ %3 Hz.",
          mFrames, mChannels, mSampleRate);
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

SndFileInputStream::SndFileInputStream(const std::string& filename)
{
    if (!OpenFile(filename))
        throw std::runtime_error("Failed to open: " + filename);
}

SndFileInputStream::~SndFileInputStream()
{
    if (mStream.is_open())
        mStream.close();
}

bool SndFileInputStream::OpenFile(const std::string& filename)
{
    mStream = base::OpenBinaryInputStream(filename);
    if (mStream.is_open())
    {
        mStream.seekg(0, mStream.end);
        mStreamSize = mStream.tellg();
        mStream.seekg(0, mStream.beg);
        mFilename = filename;
        DEBUG("Opened audio file stream with %1 bytes from '%2'", mStreamSize, filename);
        return true;
    }
    ERROR("Failed to open '%1'", filename);
    return false;
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
        WARN("Stream read returned %1 bytes vs. %2 requested", mStream.gcount(), bytes_to_read);
    return mStream.gcount();
}
std::int64_t SndFileInputStream::Tell() const
{ return (sf_count_t)mStream.tellg(); }


std::int64_t SndFileBuffer::GetLength() const
{ return mBuffer.size(); }

std::int64_t SndFileBuffer::Seek(std::int64_t offset, int whence)
{
    if (whence == SEEK_CUR)
        mOffset += offset;
    else if (whence == SEEK_SET)
        mOffset = offset;
    else if (whence == SEEK_END)
        mOffset = mBuffer.size() - offset;
    else BUG("Unknown seek position");
    return mOffset;
}
std::int64_t SndFileBuffer::Read(void* ptr, std::int64_t count)
{
    const auto num_bytes = mBuffer.size();
    const auto num_avail = num_bytes - mOffset;
    const auto num_bytes_to_read = std::min((std::int64_t)num_avail, count);
    if (num_bytes_to_read == 0)
        return 0;

    std::memcpy(ptr, &mBuffer[mOffset], num_bytes_to_read);
    mOffset += num_bytes_to_read;
    return num_bytes_to_read;
}
std::int64_t SndFileBuffer::Tell() const
{ return mOffset; }

} // namespace