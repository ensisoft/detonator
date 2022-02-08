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
class FileBufferImpl : public audio::SourceStream
{
public:
    FileBufferImpl(const std::string& name, std::vector<char>&& buffer)
      : mName(name)
      , mBuffer(std::move(buffer))
    {}
    virtual void Read(void* ptr, uint64_t offset, uint64_t bytes) const override
    {
        ASSERT(offset + bytes <= mBuffer.size());
        
        const auto size = mBuffer.size();
        bytes = std::min(size - offset, bytes);
        memcpy(ptr, &mBuffer[offset], bytes);
    }
    virtual std::uint64_t GetSize() const override
    { return mBuffer.size(); }
    virtual std::string GetName() const override
    { return mName; }
private:
    const std::string mName;
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
    return std::make_shared<FileBufferImpl>(file, std::move(buffer));
}

} // namespace