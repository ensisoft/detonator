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

#include <iostream>
#include <algorithm>
#include <vector>

#include "base/assert.h"
#include "base/logging.h"
#include "base/utility.h"
#include "audio/loader.h"

namespace {
class FileStreamImpl : public audio::SourceStream
{
public:
    FileStreamImpl(std::ifstream&& stream) : mStream(std::move(stream))
    {
        mStream.seekg(0, mStream.end);
        mStreamSize = mStream.tellg();
        mStream.seekg(0, mStream.beg);
    }
    virtual std::int64_t Read(void* ptr, std::int64_t bytes) override
    {
        mStream.read((char*)ptr, bytes);
        return mStream.gcount();
    }
    virtual std::int64_t Tell() const override
    { return const_cast<std::ifstream&>(mStream).tellg(); }
    virtual std::int64_t Seek(std::int64_t offset, Whence whence) override
    {
        if (whence == Whence::FromCurrent)
            mStream.seekg(offset, mStream.cur);
        else if (whence == Whence::FromStart)
            mStream.seekg(offset, mStream.beg);
        else if (whence == Whence::FromEnd)
            mStream.seekg(offset, mStream.end);
        else BUG("Unknown whence in seek.");
        return mStream.tellg();
    }
    virtual std::int64_t GetSize() const override
    { return mStreamSize; }
private:
    std::ifstream mStream;
    std::int64_t mStreamSize = 0;
};
class FileBufferImpl : public audio::SourceBuffer
{
public:
    FileBufferImpl(std::vector<char>&& buffer) : mBuffer(std::move(buffer))
    {}
    virtual const void* GetData() const override
    { return mBuffer.empty() ? nullptr : &mBuffer[0]; }
    virtual size_t GetSize() const override
    { return mBuffer.size(); }
private:
    const std::vector<char> mBuffer;
};

} // namespace

namespace audio
{

SourceStreamHandle OpenFileStream(const std::string& file)
{
    auto stream = base::OpenBinaryInputStream(file);
    if (!stream.is_open())
    {
        ERROR("Failed to open audio file. [file='%1']", file);
        return nullptr;
    }
    DEBUG("Opened audio stream successfully. [file='%1']", file);
    return std::make_unique<FileStreamImpl>(std::move(stream));
}
SourceBufferHandle LoadFileBuffer(const std::string& file)
{
    auto stream = base::OpenBinaryInputStream(file);
    if (!stream.is_open())
    {
        ERROR("Failed to open audio file. [file='%1']", file);
        return nullptr;
    }

    std::vector<char> buffer;
    stream.seekg(0, std::ios::end);
    const auto size = (std::size_t)stream.tellg();
    stream.seekg(0, std::ios::beg);
    buffer.resize(size);
    stream.read(&buffer[0], size);
    if ((std::size_t)stream.gcount() != size)
    {
        ERROR("Failed to read audio file. [file='%1']", file);
        return nullptr;
    }
    DEBUG("Loaded audio file successfully. [file='%1', size=%2]", file, size);
    return std::make_shared<FileBufferImpl>(std::move(buffer));
}

} // namespace