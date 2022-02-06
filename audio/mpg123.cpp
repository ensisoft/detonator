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
#include <algorithm>
#include <mpg123.h>

#include "base/format.h"
#include "base/utility.h"
#include "base/logging.h"
#include "base/math.h"
#include "audio/mpg123.h"
#include "audio/loader.h"

namespace {
using namespace audio;
struct IOTrampoline {
    // ssize_t is NOT STANDARD. mpg123.h provides a definition
    // for MSVC. One could argue about the sanity of such a choice
    // of a return type however....
    static ssize_t Read(void* user, void* buffer, size_t bytes)
    { return static_cast<Mpg123IODevice*>(user)->Read(buffer, bytes); }
    static off_t Seek(void* user, off_t offset, int whence)
    { return static_cast<Mpg123IODevice*>(user)->Seek(offset, whence); }
};
} // namespace

namespace audio
{
struct Mpg123Decoder::Library {
    Library()
    {
        if (::mpg123_init())
            throw std::runtime_error("mpg123_init failed.");
    }
   ~Library() {
        ::mpg123_exit();
    }
};

long Mpg123FileInputStream::Read(void* buffer,  size_t bytes)
{
    const auto size = mStream->GetSize();
    const auto offset = mStream->Tell();
    const auto available = size - offset;
    const auto bytes_to_read = std::min((size_t)available, bytes);
    if (bytes_to_read == 0)
        return 0;

    const auto ret = mStream->Read((char*)buffer, bytes_to_read);
    if (ret != bytes_to_read)
        WARN("Mpg123FileInputStream stream read failed. [requested=%1, returned=%2]", bytes_to_read, ret);
    return ret;
}

off_t Mpg123FileInputStream::Seek(off_t offset, int whence)
{
    if (whence == SEEK_CUR)
        mStream->Seek(offset, SourceStream::Whence::FromCurrent);
    else if (whence == SEEK_SET)
        mStream->Seek(offset, SourceStream::Whence::FromStart);
    else if (whence == SEEK_END)
        mStream->Seek(offset, SourceStream::Whence::FromEnd);
    else BUG("Unknown seek position.");
    return mStream->Tell();
}

long Mpg123Buffer::Read(void* buffer, size_t bytes)
{
    const auto num_bytes = mBuffer->GetSize();
    const auto num_avail = mOffset >= num_bytes ? 0 : num_bytes - mOffset;
    const auto num_bytes_to_read = std::min(size_t(num_avail), bytes);
    if (num_bytes_to_read == 0)
        return 0;
    const auto* src = static_cast<const std::uint8_t*>(mBuffer->GetData());
    std::memcpy(buffer, &src[mOffset], num_bytes_to_read);
    mOffset += num_bytes_to_read;
    return num_bytes_to_read;
}

off_t Mpg123Buffer::Seek(off_t offset, int whence)
{
    if (whence == SEEK_CUR)
        mOffset += offset;
    else if (whence == SEEK_SET)
        mOffset = offset;
    else if (whence == SEEK_END)
        mOffset = mBuffer->GetSize() + offset;
    else BUG("Unknown seek position");
    mOffset = math::clamp(off_t(0), off_t(mBuffer->GetSize()), mOffset);
    return mOffset;
}

Mpg123Decoder::Mpg123Decoder()
{
    // according to the latest docs (as of July 2021 this should no longer
    // be needed. However, when using 1.26.4 (through Conan) mgp123_new will
    // actually fail with an error saying that "you didn't initialize the library!"
    static std::weak_ptr<Library> global_library;
    mLibrary = global_library.lock();
    if (!mLibrary)
    {
        mLibrary = std::make_shared<Library>();
        global_library = mLibrary;
    }

    int error = 0;
    mHandle = ::mpg123_new(NULL, &error);
    if (mHandle == NULL)
        throw std::runtime_error(base::FormatString("mpg123_new failed %1 ('%2')" , error,
                                     mpg123_plain_strerror(error)));
    mpg123_replace_reader_handle(mHandle,
        &IOTrampoline::Read, &IOTrampoline::Seek,nullptr);
}

Mpg123Decoder::Mpg123Decoder(std::unique_ptr<Mpg123IODevice> io, SampleType format) : Mpg123Decoder()
{
    if (!Open(std::move(io), format))
        throw std::runtime_error("Mpg123Decoder open failed.");
}

Mpg123Decoder::~Mpg123Decoder()
{
    if (mIsOpen)
        ::mpg123_close(mHandle);
    ::mpg123_delete(mHandle);
}

unsigned Mpg123Decoder::GetSampleRate() const
{ return mSampleRate; }
unsigned Mpg123Decoder::GetNumChannels() const
{ return 2; }
unsigned Mpg123Decoder::GetNumFrames() const
{ return mFrameCount; }

size_t Mpg123Decoder::ReadFrames(float* ptr, size_t frames)
{
    ASSERT(mOutFormat == MPG123_ENC_FLOAT_32 &&
            "Mismatch PCM audio data read format.");
    return ReadFrames<float>(ptr, frames);
}
size_t Mpg123Decoder::ReadFrames(short* ptr, size_t frames)
{
    ASSERT(mOutFormat == MPG123_ENC_SIGNED_16 &&
           "Mismatch PCM audio data read format.");
    return ReadFrames<short>(ptr, frames);
}
size_t Mpg123Decoder::ReadFrames(int* ptr, size_t frames)
{
    ASSERT(mOutFormat == MPG123_ENC_SIGNED_32 &&
           "Mismatch PCM audio data read format.");
    return ReadFrames<int>(ptr, frames);
}

void Mpg123Decoder::Reset()
{
    if (mpg123_seek_frame(mHandle, 0, SEEK_SET) != MPG123_OK)
        throw std::runtime_error("mpg123_seek_frame failed");
}

bool Mpg123Decoder::Open(std::unique_ptr<Mpg123IODevice> io, SampleType format)
{
    ASSERT(!mIsOpen);

    int mpg123_data_format = 0;
    if (format == SampleType::Float32)
        mpg123_data_format = MPG123_ENC_FLOAT_32;
    else if (format == SampleType::Int32)
        mpg123_data_format = MPG123_ENC_SIGNED_32;
    else if (format == SampleType::Int16)
        mpg123_data_format = MPG123_ENC_SIGNED_16;
    else BUG("Unsupported sample format.");

    // perform the same actions that mpg123_open_fixed does.
    // we need to do this manually since we're now using the
    // abstract IO handle system.
    // the functionality to be replicated can be found in
    // mpg123/src/libmpg123.c

    if ((mpg123_param(mHandle, MPG123_ADD_FLAGS, MPG123_NO_FRANKENSTEIN, 0.0f) != MPG123_OK) ||
        (mpg123_format_none(mHandle) != MPG123_OK) ||
        (mpg123_format2(mHandle, 0, MPG123_STEREO, mpg123_data_format) != MPG123_OK))
    {
        ERROR("Mpg123Decoder set format failed. [name='%1', error='%2']", io->GetName(), mpg123_strerror(mHandle));
        return false;
    }

    if (mpg123_open_handle(mHandle, (void*)io.get()) != MPG123_OK)
    {
        ERROR("Mpg123Decoder open handle failed. [name='%1', error='%2']", io->GetName(), mpg123_strerror(mHandle));
        return false;
    }
    mIsOpen = true;

    int channel_count = 0;
    int encoding      = 0;
    long sample_rate  = 0;
    if ((mpg123_getformat(mHandle, &sample_rate, &channel_count, &encoding) != MPG123_OK) ||
        (mpg123_format_none(mHandle) != MPG123_OK) ||
        (mpg123_format(mHandle, sample_rate, channel_count, encoding) != MPG123_OK))
    {
        ERROR("Mpg123Decoder set format failed. [name='%1', error='%2']", io->GetName(), mpg123_strerror(mHandle));
        return false;
    }

    // this can fail if the library fails to probe the mp3 file for
    // headers related to the total number of PCM frames.
    auto frames = mpg123_length(mHandle);
    if (frames == MPG123_ERR)
    {
        // make a full parsing scan over the stream. this is only needed really
        // if the stream has no header frame with meta information about the stream
        // such as the frame count.
        if (mpg123_scan(mHandle) != MPG123_OK)
        {
            ERROR("Mpg123Decoder stream scan failed. [name='%1', error='%2']", io->GetName(), mpg123_strerror(mHandle));
            return false;
        }
    }
    frames = mpg123_length(mHandle);
    if (frames == MPG123_ERR)
    {
        ERROR("Mpg123Decoder get stream length failed. [name='%1', error='%2']", io->GetName(), mpg123_strerror(mHandle));
        return false;
    }

    mSampleRate = sample_rate;
    mFrameCount = frames;
    mOutFormat  = mpg123_data_format;
    mDevice     = std::move(io);
    DEBUG("Mpg123Decoder is open. [name='%1', frames=%2, channels=%3, rate=%4]", mDevice->GetName(), mFrameCount, 2, mSampleRate);
    return true;
}

template<typename T>
size_t Mpg123Decoder::ReadFrames(T* ptr, size_t frames)
{
    const auto frame_size = sizeof(T) * 2; // we're always in stereo!
    const auto bytes_max  = frames * frame_size;
    size_t bytes_read = 0;
    // todo: deal with MPG123_NEED_MORE stuff ??
    if (mpg123_read(mHandle, ptr, bytes_max, &bytes_read) != MPG123_OK)
        throw std::runtime_error("mpg123_read failed.");
    ASSERT((bytes_read % frame_size) == 0);
    return bytes_read / frame_size;
}

} // namespace
