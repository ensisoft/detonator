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
#include "base/threadpool.h"
#include "audio/decoder.h"
#include "audio/sndfile.h"
#include "audio/mpg123.h"
#include "audio/loader.h"
#include "audio/elements/file_source.h"

namespace audio
{

// raw PCM data blob
struct FileSource::PCMBuffer {
    bool complete = false;
    unsigned rate        = 0;
    unsigned channels    = 0;
    unsigned frame_count = 0;
    std::vector<char> pcm;
    audio::SampleType type = audio::SampleType::Float32;
};

// A pass through decoder that does no actual decoding
// but just reads a PCM buffer
class FileSource::PCMDecoder : public audio::Decoder
{
public:
    explicit PCMDecoder(const std::shared_ptr<const PCMBuffer>& pcm)
      : mBuffer(pcm)
    {}
    unsigned GetSampleRate() const override
    { return mBuffer->rate; }
    unsigned GetNumChannels() const override
    { return mBuffer->channels; }
    unsigned GetNumFrames() const override
    { return mBuffer->frame_count; }
    size_t ReadFrames(float* ptr, size_t frames) override
    {
        ASSERT(mBuffer->type == audio::SampleType::Float32);
        return ReadFrames<float>(ptr, frames);
    }
    size_t ReadFrames(short* ptr, size_t frames) override
    {
        ASSERT(mBuffer->type == audio::SampleType::Int16);
        return ReadFrames<short>(ptr, frames);
    }
    size_t ReadFrames(int* ptr, size_t frames) override
    {
        ASSERT(mBuffer->type == audio::SampleType::Int32);
        return ReadFrames<int>(ptr, frames);
    }
    void Reset() override
    {
        mFrame = 0;
    }
private:
    template<typename T>
    size_t ReadFrames(T* ptr, size_t frames)
    {
        const auto frame_size = sizeof(T) * mBuffer->channels;
        const auto byte_count = frame_size * frames;
        const auto byte_offset = frame_size * mFrame;
        ASSERT(byte_offset + byte_count <= mBuffer->pcm.size());
        std::memcpy(ptr, &mBuffer->pcm[byte_offset], byte_count);
        mFrame += frames;
        return frames;
    }
private:
    std::shared_ptr<const PCMBuffer> mBuffer;
    std::uint64_t mFrame = 0;
};

// static
FileSource::PCMCache FileSource::pcm_cache;
// static
FileSource::FileInfoCache FileSource::file_info_cache;

FileSource::FileSource(std::string name, std::string file, SampleType type, unsigned loops)
  : mName(std::move(name))
  , mId(base::RandomString(10))
  , mFile(std::move(file))
  , mPort("out")
  , mLoopCount(loops)
{
    mFormat.sample_type = type;
}

FileSource::FileSource(std::string name, std::string id, std::string file, SampleType type, unsigned loops)
  : mName(std::move(name))
  , mId(std::move(id))
  , mFile(std::move(file))
  , mPort("out")
  , mLoopCount(loops)
{
    mFormat.sample_type = type;
}

FileSource::FileSource(FileSource&& other) noexcept
  : mName(other.mName)
  , mId(other.mId)
  , mFile(other.mFile)
  , mDecoder(std::move(other.mDecoder))
  , mPort(std::move(other.mPort))
  , mFormat(other.mFormat)
  , mFramesRead(other.mFramesRead)
{}

FileSource::~FileSource() = default;

void FileSource::PortPing(size_t ping_counter)
{
    std::vector<PortControlMessage> messages;
    mPort.TransferMessages(&messages);

    for (const auto& msg : messages)
    {
        if (msg.message == "Shutdown")
        {
            DEBUG("Audio file source shutting down on control message. [name='%1']", mName);
            Shutdown();
        }
        else
        {
            DEBUG("Audio file source received control message. [name='%1, msg=%2]", mName, msg.message);
        }
    }
}


bool FileSource::Prepare(const Loader& loader, const PrepareParams& params)
{
    std::shared_ptr<PCMBuffer> cached_pcm_buffer;
    std::unique_ptr<Decoder> decoder;

    const bool enable_pcm_caching = params.enable_pcm_caching && mEnablePcmCaching;
    if (enable_pcm_caching)
    {
        auto it = pcm_cache.find(mId);
        if (it != pcm_cache.end())
            cached_pcm_buffer = it->second;
    }

    // if we have a file info available (discovered through preload)
    // then we don't need the actual codec in order to prepare
    // the FileSource. Rather we can use the cached information
    // (assuming it's correct) and then defer the decoder open to
    // background task in the thread pool that is waited on when
    // the first call to process audio data is done.
    const auto* info = base::SafeFind(file_info_cache, mFile);

    // if there already exists a complete PCM blob for the
    // contents of this (as identified by ID) FileSource
    // element's audio file then we can use that data directly
    // and not perform any duplicate mp3/ogg/flac decoding.
    if (cached_pcm_buffer && cached_pcm_buffer->complete)
    {
        decoder = std::make_unique<PCMDecoder>(cached_pcm_buffer);
        DEBUG("Using a cached PCM audio buffer. [elem=%1, file='%2', id=%3]", mName, mFile, mId);
    }
    else
    {
        auto source = loader.OpenAudioStream(mFile, mIOStrategy, mEnableFileCaching);
        if (!source)
            return false;

        const auto& upper = base::ToUpperUtf8(mFile);
        if (base::EndsWith(upper, ".MP3"))
        {
            class OpenDecoderTask : public base::ThreadTask {
            public:
                OpenDecoderTask(std::unique_ptr<Mpg123Decoder> decoder,
                                std::shared_ptr<const SourceStream> stream,
                                SampleType type)
                  : mDecoder(std::move(decoder))
                  , mStream(std::move(stream))
                  , mSampleType(type)
                {}
                void DoTask() override
                {
                    if (!mDecoder->Open(mStream, mSampleType))
                    {
                        mFlags.set(Flags::Error, true);
                    }
                }
                void GetValue(const char* key, void* ptr) override
                {
                    ASSERT(!std::strcmp(key, "decoder"));
                    using DecoderPtr = std::unique_ptr<Decoder>;
                    auto* foo = static_cast<DecoderPtr*>(ptr);
                    *foo = std::move(mDecoder);
                }
            private:
                std::unique_ptr<Mpg123Decoder> mDecoder;
                std::shared_ptr<const SourceStream> mStream;
                SampleType mSampleType;
            };

            auto mpg123_decoder = std::make_unique<Mpg123Decoder>();
            auto* pool = base::GetGlobalThreadPool();
            if (info && pool && pool->HasThread(base::ThreadPool::AudioThreadID))
            {
                auto* pool = base::GetGlobalThreadPool();
                auto task = std::make_unique<OpenDecoderTask>(std::move(mpg123_decoder), std::move(source), mFormat.sample_type);
                mOpenDecoderTask = pool->SubmitTask(std::move(task), base::ThreadPool::AudioThreadID);
                DEBUG("Submitted new audio decoder open task. [file='%1']", mFile);
            }
            else
            {
                if (!mpg123_decoder->Open(source, mFormat.sample_type))
                    return false;
                decoder = std::move(mpg123_decoder);
            }
        }
        else if (base::EndsWith(upper, ".OGG") ||
                 base::EndsWith(upper, ".WAV") ||
                 base::EndsWith(upper, ".FLAC"))
        {
            class OpenDecoderTask : public base::ThreadTask {
            public:
                OpenDecoderTask(std::unique_ptr<SndFileDecoder> decoder,
                                std::shared_ptr<const SourceStream> stream)
                  : mDecoder(std::move(decoder))
                  , mStream(std::move(stream))
                {}
                void DoTask() override
                {
                    if (!mDecoder->Open(mStream))
                    {
                        mFlags.set(Flags::Error, true);
                    }
                }
                void GetValue(const char* key, void* ptr) override
                {
                    ASSERT(!std::strcmp(key, "decoder"));
                    using DecoderPtr = std::unique_ptr<Decoder>;
                    auto* foo = static_cast<DecoderPtr*>(ptr);
                    *foo = std::move(mDecoder);
                }
            private:
                std::unique_ptr<SndFileDecoder> mDecoder;
                std::shared_ptr<const SourceStream> mStream;
            };

            auto snd_file_decoder = std::make_unique<SndFileDecoder>();
            auto* pool = base::GetGlobalThreadPool();
            if (info && pool && pool->HasThread(base::ThreadPool::AudioThreadID))
            {
                auto* pool = base::GetGlobalThreadPool();
                auto task = std::make_unique<OpenDecoderTask>(std::move(snd_file_decoder), std::move(source));
                mOpenDecoderTask = pool->SubmitTask(std::move(task), base::ThreadPool::AudioThreadID);
                DEBUG("Submitted new audio decoder open task. [file='%1']", mFile);
            }
            else
            {
                if (!snd_file_decoder->Open(source))
                    return false;
                decoder = std::move(snd_file_decoder);
            }
        }
        else
        {
            ERROR("Audio file source file format is unsupported. [elem=%1, file='%2']", mName, mFile);
            return false;
        }
        ASSERT(mOpenDecoderTask.IsValid() || decoder);

        if (!cached_pcm_buffer && enable_pcm_caching)
        {
            cached_pcm_buffer = std::make_shared<PCMBuffer>();
            cached_pcm_buffer->complete    = false;
            cached_pcm_buffer->channels    = info ? info->channels : decoder->GetNumChannels();
            cached_pcm_buffer->rate        = info ? info->sample_rate : decoder->GetSampleRate();
            cached_pcm_buffer->frame_count = info ? info->frames : decoder->GetNumFrames();
            cached_pcm_buffer->type        = mFormat.sample_type;
            pcm_cache[mId] = cached_pcm_buffer;
            mPCMBuffer = cached_pcm_buffer;
        }
    }
    Format format;
    format.channel_count = info ? info->channels : decoder->GetNumChannels();
    format.sample_rate   = info ? info->sample_rate : decoder->GetSampleRate();
    format.sample_type   = mFormat.sample_type;

    DEBUG("Audio file source prepared successfully. [elem=%1, file='%2', format=%3]", mName, mFile,format);
    if (info == nullptr)
    {
        FileInfo fileinfo;
        fileinfo.sample_rate = decoder->GetSampleRate();
        fileinfo.channels    = decoder->GetNumChannels();
        fileinfo.frames      = decoder->GetNumFrames();
        file_info_cache[mFile] = fileinfo;
        DEBUG("Saved audio file source file info. [file='%1']", mFile);
    }

    mDecoder = std::move(decoder);
    mPort.SetFormat(format);
    mFormat = format;
    return true;
}

void FileSource::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("FileSource");

    if (mOpenDecoderTask.IsValid())
    {
        // wait here? maybe not... generate silence ?
        if (!mOpenDecoderTask.IsComplete())
            return;

        auto task = mOpenDecoderTask.GetTask();
        if (task->Failed())
        {
            ERROR("Failed to open decoder on audio stream. [elem=%1, file='%2']", mName, mFile);
            return;
        }
        task->GetValue("decoder", &mDecoder);
        ASSERT(mDecoder);

        mOpenDecoderTask.Clear();
        DEBUG("Audio decoder open task is complete.");
    }

    const auto frame_size = GetFrameSizeInBytes(mFormat);
    const auto frames_per_ms = mFormat.sample_rate / 1000;
    const auto frames_wanted = (unsigned)(frames_per_ms * milliseconds);
    const auto frames_available = mDecoder->GetNumFrames();
    const auto frames = std::min(frames_available - mFramesRead, frames_wanted);

    auto buffer = allocator.Allocate(frame_size * frames);
    buffer->SetFormat(mFormat);
    buffer->SetByteSize(frame_size * frames);
    void* buff = buffer->GetPtr();

    size_t ret = 0;
    TRACE_BLOCK("Decode",
        if (mFormat.sample_type == SampleType::Float32)
            ret = mDecoder->ReadFrames((float*)buff, frames);
        else if (mFormat.sample_type == SampleType::Int32)
            ret = mDecoder->ReadFrames((int*)buff, frames);
        else if (mFormat.sample_type == SampleType::Int16)
            ret = mDecoder->ReadFrames((short*)buff, frames);
        else BUG("Missing sampletype");
    );

    if (mPCMBuffer && !mPCMBuffer->complete)
    {
        const auto old_size = mPCMBuffer->pcm.size();
        const auto new_size = old_size + ret * frame_size;
        mPCMBuffer->pcm.resize(new_size);
        std::memcpy(&mPCMBuffer->pcm[old_size], buff, ret * frame_size);
    }

    if (ret != frames)
    {
        WARN("Unexpected number of audio frames decoded. [elem=%1, expected=%2, decoded=%3]", mName, frames, ret);
    }

    mFramesRead += ret;
    if (mFramesRead == frames_available)
    {
        if (mPCMBuffer)
        {
            const auto size = mPCMBuffer->pcm.size();
            mPCMBuffer->complete = true;
            DEBUG("Audio PCM buffer is complete. [elem=%1, file='%2', id=%3, bytes=%4]", mName, mFile, mId, size);
        }

        if (++mPlayCount != mLoopCount)
        {
            if (mPCMBuffer)
            {
                mDecoder = std::make_unique<PCMDecoder>(mPCMBuffer);
            }
            mDecoder->Reset();
            mFramesRead = 0;
            DEBUG("Audio file source was reset for looped playback. [elem=%1, file='%2', count=%3]", mName, mFile, mPlayCount+1);
        }
        else
        {
            DEBUG("Audio file source is done. [elem=%1, file='%2']", mName, mFile);
        }
        mPCMBuffer.reset();
    }

    mPort.PushBuffer(buffer);
}

void FileSource::Shutdown()
{
    mDecoder.reset();
    mOpenDecoderTask.Clear();
}

bool FileSource::IsSourceDone() const
{
    if (mOpenDecoderTask.IsValid())
        return false;
    if (!mDecoder)
        return true;

    return mFramesRead == mDecoder->GetNumFrames();
}

// static
bool FileSource::ProbeFile(const std::string& file, FileInfo* info)
{
    auto stream = audio::OpenFileStream(file);
    if (!stream)
        return false;

    std::unique_ptr<Decoder> decoder;
    const auto& upper = base::ToUpperUtf8(file);
    if (base::EndsWith(upper, ".MP3"))
    {
        auto dec = std::make_unique<Mpg123Decoder>();
        if (!dec->Open(stream, SampleType::Float32))
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
    else return false;
    info->sample_rate = decoder->GetSampleRate();
    info->channels    = decoder->GetNumChannels();
    info->frames      = decoder->GetNumFrames();
    info->seconds     = (float)info->frames / (float)info->sample_rate;
    info->bytes       = stream->GetSize();
    return true;
}
// static
void FileSource::ClearCache()
{
    pcm_cache.clear();
    file_info_cache.clear();
}



} // namespace