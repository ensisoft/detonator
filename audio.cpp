// Copyright (c) 2014 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "config.h"

#ifdef ENABLE_AUDIO

#include "warnpush.h"
#  include <QResource>
#  include <QFile>
#include "warnpop.h"
#include <algorithm>
#include <stdexcept>
#include <sndfile.h>
#include "audio.h"

namespace {

class io_buffer
{
public:
    using u8 = std::uint8_t;

    io_buffer(const void* ptr, std::size_t len) : ptr_((const std::uint8_t*)ptr), len_(len)
    {
        // set up a virtual IO device for libsound and then read all the frames
        // into a conversion buffer so we have the raw data.
        SF_VIRTUAL_IO io = {};
        io.get_filelen = ioGetLength;
        io.seek        = ioSeek;
        io.read        = ioRead;
        io.tell        = ioTell;

        SF_INFO sfinfo  = {};
        SNDFILE* sffile = sf_open_virtual(&io, SFM_READ, &sfinfo, (void*)this);
        if (!sffile)
            throw std::runtime_error("sf_open_virtual failed");

        sample_rate_  = sfinfo.samplerate;
        num_channels_ = sfinfo.channels;
        num_frames_   = sfinfo.frames;

        // reserve space for the PCM data
        buffer_.resize(num_frames_ * num_channels_ * sizeof(short));

        // read and convert the whole audio clip into 16bit NE
        if (sf_readf_short(sffile, (short*)&buffer_[0], num_frames_) !=  num_frames_)
        {
            sf_close(sffile);
            throw std::runtime_error("sf_readf_short failed");
        }
        sf_close(sffile);
    }

    std::vector<u8> get_buffer() &&
    { return std::move(buffer_); }

    unsigned rate() const
    { return sample_rate_; }

    unsigned channels() const
    { return num_channels_; }

    unsigned frames() const
    { return num_frames_; }

private:
    static sf_count_t ioGetLength(void* user)
    {
        const auto* this_ = static_cast<io_buffer*>(user);
        return this_->len_;
    }

    static sf_count_t ioSeek(sf_count_t offset, int whence, void* user)
    {
        auto* this_ = static_cast<io_buffer*>(user);
        switch (whence)
        {
            case SEEK_CUR:
                this_->position_ += offset;
                break;
            case SEEK_SET:
                this_->position_ = offset;
                break;
            case SEEK_END:
                this_->position_ = this_->len_ - offset;
                break;
        }
        return this_->position_;
    }

    static sf_count_t ioRead(void* ptr, sf_count_t count, void* user)
    {
        auto* this_ = static_cast<io_buffer*>(user);

        const auto num_bytes = this_->len_;
        const auto num_avail = num_bytes - this_->position_;
        const auto num_readb = std::min((sf_count_t)num_avail, count);

        std::memcpy(ptr, &this_->ptr_[this_->position_], num_readb);
        this_->position_ += num_readb;
        return num_readb;
    }

    static sf_count_t ioTell(void* user)
    {
        const auto* this_ = static_cast<io_buffer*>(user);
        return this_->position_;
    }
private:
    const std::uint8_t* ptr_ = nullptr;
    const std::size_t len_   = 0;
private:
    sf_count_t position_ = 0;
private:
    unsigned sample_rate_  = 0;
    unsigned num_channels_ = 0;
    unsigned num_frames_   = 0;
private:
    std::vector<u8> buffer_;
};



} // namespace

namespace invaders
{

AudioSample::AudioSample(const u8* ptr, std::size_t len, const std::string& name)
{
    io_buffer buff(ptr, len);

    buffer_ = std::move(buff).get_buffer();
    sample_rate_  = buff.rate();
    num_channels_ = buff.channels();
    num_frames_   = buff.frames();
    name_ = name;
}

AudioSample::AudioSample(const QString& path, const std::string& name)
{
    QResource resource(path);
    if (resource.isValid())
    {
        io_buffer buff(resource.data(), resource.size());
        sample_rate_  = buff.rate();
        num_channels_ = buff.channels();
        num_frames_   = buff.frames();
        buffer_       = std::move(buff).get_buffer();
    }
    else
    {
        QFile io(path);
        if (!io.open(QIODevice::ReadOnly))
            throw std::runtime_error("open audio file failed");
        QByteArray data = io.readAll();

        io_buffer buff(data.constData(), data.size());
        sample_rate_  = buff.rate();
        num_channels_ = buff.channels();
        num_frames_   = buff.frames();
        buffer_       = std::move(buff).get_buffer();
    }
    name_ = name;
}

} // invaders

#endif // ENABLE_AUDIO