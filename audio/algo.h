// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#pragma once

#include "config.h"

#include "audio/buffer.h"
#include "audio/format.h"

namespace audio
{
    template<unsigned ChannelCount>
    void AdjustFrameGain(Frame<float, ChannelCount>* frame, float gain)
    {
        // float's can exceed the -1.0f - 1.0f range.
        for (unsigned i=0; i<ChannelCount; ++i)
        {
            frame->channels[i] *= gain;
        }
    }

    template<typename Type, unsigned ChannelCount>
    void AdjustFrameGain(Frame<Type, ChannelCount>* frame, float gain)
    {
        static_assert(std::is_integral<Type>::value);
        // for integer formats clamp the values in order to avoid
        // undefined overflow / wrapping over.
        for (unsigned i=0; i<ChannelCount; ++i)
        {
            const std::int64_t sample = frame->channels[i] * gain;
            const std::int64_t min = SampleBits<Type>::Bits * -1;
            const std::int64_t max = SampleBits<Type>::Bits;
            frame->channels[i] = math::clamp(min, max, sample);
        }
    }

    template<unsigned ChannelCount>
    void MixFrames(Frame<float, ChannelCount>** srcs, unsigned src_count, float src_gain,
                   Frame<float, ChannelCount>* out)
    {
        float channel_values[ChannelCount] = {};

        // profiling with valgrind+callgrind shows that the
        // loops perform better when performed over all srcs
        // frames and then over the channels of each frame
        // as opposed to over channel followed by over srcs.
        for (unsigned j=0; j<src_count; ++j)
        {
            for (unsigned i=0; i<ChannelCount; ++i)
            {
                channel_values[i] += (src_gain * srcs[j]->channels[i]);
            }
        }
        for (unsigned i=0; i<ChannelCount; ++i)
        {
            out->channels[i] = channel_values[i];
        }

        /*
        for (unsigned i=0; i<ChannelCount; ++i)
        {
            float channel_value = 0.0f;

            for (unsigned j=0; j<src_count; ++j)
            {
                channel_value += (src_gain * srcs[j]->channels[i]);
            }
            out->channels[i] = channel_value;
        }
         */
    }

    template<typename Type, unsigned ChannelCount>
    void MixFrames(Frame<Type, ChannelCount>** srcs, unsigned count, float src_gain,
                   Frame<Type, ChannelCount>* out)
    {
        static_assert(std::is_integral<Type>::value);

        for (unsigned i=0; i<ChannelCount; ++i)
        {
            std::int64_t channel_value = 0;
            for (unsigned j=0; j<count; ++j)
            {
                // todo: this could still wrap around.
                channel_value += (src_gain * srcs[j]->channels[i]);
            }
            constexpr std::int64_t min = SampleBits<Type>::Bits * -1;
            constexpr std::int64_t max = SampleBits<Type>::Bits;
            out->channels[i] = math::clamp(min, max, channel_value);
        }
    }

    template<typename Type, unsigned ChannelCount>
    float FadeBuffer(BufferHandle buffer, float current_time, float start_time, float duration, bool fade_in)
    {
        using AudioFrame = Frame<Type, ChannelCount>;

        const auto frame_size  = sizeof(AudioFrame);
        const auto format      = buffer->GetFormat();
        const auto buffer_size = buffer->GetByteSize();
        const auto num_frames  = buffer_size / frame_size;
        ASSERT((buffer_size % frame_size) == 0);

        const auto sample_rate = format.sample_rate;
        const auto sample_duration = 1000.0f / sample_rate;

        auto* ptr = static_cast<AudioFrame*>(buffer->GetPtr());
        for (unsigned i=0; i<num_frames; ++i)
        {
            const auto effect_time = current_time - start_time;
            const auto effect_time_norm = math::clamp(0.0f, 1.0f, effect_time / duration);
            const auto effect_value = fade_in ? effect_time_norm : 1.0f - effect_time_norm;
            const auto sample_gain = std::pow(effect_value, 2.2);
            AdjustFrameGain(&ptr[i], sample_gain);
            current_time += sample_duration;
        }
        return current_time;
    }

    template<typename Type, unsigned ChannelCount>
    BufferHandle MixBuffers(std::vector<BufferHandle>& src_buffers, float src_gain)
    {
        using AudioFrame = Frame<Type, ChannelCount>;

        std::vector<AudioFrame*> src_ptrs;

        BufferHandle out_buffer;
        // find the maximum buffer for computing how many frames must be processed.
        // also the biggest buffer can be used for the output (mixing in place)
        unsigned max_buffer_size = 0;
        for (const auto& buffer : src_buffers)
        {
            auto* ptr = static_cast<AudioFrame*>(buffer->GetPtr());
            src_ptrs.push_back(ptr);
            if (buffer->GetByteSize() > max_buffer_size)
            {
                max_buffer_size = buffer->GetByteSize();
                out_buffer = buffer;
            }
        }
        for (const auto& buffer : src_buffers)
        {
            if (buffer == out_buffer)
                continue;
            Buffer::CopyInfoTags(*buffer, *out_buffer);
        }

        const auto frame_size  = sizeof(AudioFrame);
        const auto max_num_frames = max_buffer_size / frame_size;

        auto* out = static_cast<AudioFrame*>(out_buffer->GetPtr());

        for (unsigned frame=0; frame<max_num_frames; ++frame, ++out)
        {
            MixFrames(&src_ptrs[0], src_ptrs.size(), src_gain, out);

            ASSERT(src_buffers.size() == src_ptrs.size());
            for (size_t i=0; i<src_buffers.size();)
            {
                const auto& buffer = src_buffers[i];
                const auto buffer_size = buffer->GetByteSize();
                const auto buffer_frames = buffer_size / frame_size;
                if (buffer_frames == frame+1)
                {
                    const auto end = src_buffers.size() - 1;
                    std::swap(src_buffers[i], src_buffers[end]);
                    std::swap(src_ptrs[i], src_ptrs[end]);
                    src_buffers.pop_back();
                    src_ptrs.pop_back();
                }
                else
                {
                    ++src_ptrs[i++];
                }
            }
        }
        return out_buffer;
    }

    // for testing currently
    static BufferHandle MixBuffers(std::vector<BufferHandle>& src_buffers, float src_gain)
    {
        for (size_t i=1; i<src_buffers.size(); ++i)
        {
            ASSERT(src_buffers[0]->GetFormat() == src_buffers[i]->GetFormat());
        }
        BufferHandle  ret;

        const auto& format = src_buffers[0]->GetFormat();
        if (format.sample_type == SampleType::Int32)
            ret = format.channel_count == 1 ? MixBuffers<int, 1>(src_buffers, src_gain)
                                            : MixBuffers<int, 2>(src_buffers, src_gain);
        else if (format.sample_type == SampleType::Float32)
            ret = format.channel_count == 1 ? MixBuffers<float, 1>(src_buffers,src_gain)
                                            : MixBuffers<float, 2>(src_buffers, src_gain);
        else if (format.sample_type == SampleType::Int16)
            ret = format.channel_count == 1 ? MixBuffers<short, 1>(src_buffers, src_gain)
                                            : MixBuffers<short, 2>(src_buffers, src_gain);
        return ret;
    }

} // namespace
