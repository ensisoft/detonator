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
#include <mpg123.h>

#include "base/format.h"
#include "base/utility.h"
#include "base/logging.h"
#include "audio/mpg123.h"

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

Mpg123Decoder::Mpg123Decoder()
{
    // according to the latest docs (as of July 2021 this should no longer
    // be needed. However when using 1.26.4 (through Conan) mgp123_new will
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

bool Mpg123Decoder::Open(const std::string& file, SampleType format)
{
    ASSERT(!mIsOpen);

    int mpg123_format = 0;
    if (format == SampleType::Float32)
        mpg123_format = MPG123_ENC_FLOAT_32;
    else if (format == SampleType::Int32)
        mpg123_format = MPG123_ENC_SIGNED_32;
    else if (format == SampleType::Int16)
        mpg123_format = MPG123_ENC_SIGNED_16;
    else BUG("Unsupported sample format.");

    if (mpg123_open_fixed(mHandle, file.c_str(), MPG123_STEREO, mpg123_format) != MPG123_OK)
    {
        ERROR("Failed to open '%1' ('%2').", file, ::mpg123_strerror(mHandle));
        return false;
    }
    mIsOpen = true;

    int channel_count = 0;
    int encoding      = 0;
    long sample_rate  = 0;
    if (mpg123_getformat(mHandle, &sample_rate, &channel_count, &encoding) != MPG123_OK)
    {
        ERROR("Failed to get audio file '%1' format ('%2').", file, ::mpg123_strerror(mHandle));
        return false;
    }
    // this can fail if the library fails to probe the mp3 file for
    // headers related to the total number of PCM frames.
    const auto frames = ::mpg123_length(mHandle);
    if (frames == MPG123_ERR)
    {
        ERROR("Failed to get audio file '%1' PCM frame count ('%2').", file, ::mpg123_strerror(mHandle));
        return false;
    }
    mSampleRate = sample_rate;
    mFrameCount = frames;
    mOutFormat  = mpg123_format;
    DEBUG("Mpg123 stream on '%1' has %2 PCM frames in 2 channels @ %3 Hz.", file, mFrameCount, mSampleRate);
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