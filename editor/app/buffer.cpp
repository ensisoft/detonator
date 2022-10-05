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

#define LOGTAG "workspace"

#include "config.h"

#include "warnpush.h"
  #include <QFile>
#include "warnpop.h"

#include <cstring>

#include "base/assert.h"
#include "editor/app/eventlog.h"
#include "editor/app/buffer.h"
#include "editor/app/types.h"

namespace app {
namespace detail {
bool LoadArrayBuffer(const QString& file, QByteArray* data)
{
    QFile io;
    io.setFileName(file);
    io.open(QIODevice::ReadOnly);
    if (!io.isOpen())
    {
        ERROR("File open error. [file='%1', error='%2']", file, io.error());
        return false;
    }
    *data = io.readAll();
    const auto size = data->size();
    DEBUG("File load done. [file='%1', size=%2]", file, Bytes{(quint64)size});
    return true;
}
} // namespace

void TilemapBuffer::Write(const void* ptr, size_t bytes, size_t offset)
{
    ASSERT(offset + bytes <= mBytes.size());
    auto* buff = mBytes.data();
    std::memcpy(&buff[offset], ptr, bytes);
}
void TilemapBuffer::Read(void* ptr, size_t bytes, size_t offset) const
{
    ASSERT(offset + bytes <= mBytes.size());
    const auto* buff = mBytes.data();
    std::memcpy(ptr, &buff[offset], bytes);
}

size_t TilemapBuffer::AppendChunk(size_t bytes)
{
    const auto offset = mBytes.size();
    mBytes.resize(offset + bytes);
    return offset;
}
size_t TilemapBuffer::GetByteCount() const
{
    return mBytes.size();
}
void TilemapBuffer::Resize(size_t bytes)
{
    mBytes.resize(bytes);
}

void TilemapBuffer::ClearChunk(const void* value, size_t value_size, size_t offset, size_t num_values)
{
    ASSERT(offset + value_size * num_values <= mBytes.size());

    for (size_t i=0; i<num_values; ++i)
    {
        const auto buffer_offset = offset + i * value_size;
        auto* buff = mBytes.data();
        std::memcpy(&buff[buffer_offset], value, value_size);
    }
}

// static
std::shared_ptr<TilemapBuffer> TilemapBuffer::LoadFromFile(const QString& file)
{
    QByteArray data;
    if (!detail::LoadArrayBuffer(file, &data))
        return nullptr;
    return std::make_shared<TilemapBuffer>(file, data);
}

void TilemapMemoryMap::Write(const void* ptr, size_t bytes, size_t offset)
{
    BUG("Trying to modify read-only tilemap layer data.");
}
void TilemapMemoryMap::Read(void* ptr, size_t bytes, size_t offset) const
{
    ASSERT(mMapAddr);
    ASSERT(offset + bytes <= mSize);
    std::memcpy(ptr, &mMapAddr[offset], bytes);
}
size_t TilemapMemoryMap::AppendChunk(size_t bytes)
{
    BUG("Trying to modify read-only tilemap layer data.");
}
size_t TilemapMemoryMap::GetByteCount() const
{
    return mSize;
}
void TilemapMemoryMap::Resize(size_t bytes)
{
    BUG("Trying to modify read-only tilemap layer data.");
}
void TilemapMemoryMap::ClearChunk(const void* value, size_t value_size, size_t offset, size_t num_values)
{
    BUG("Trying to modify read-only tilemap layer data.");
}

// static
std::shared_ptr<TilemapMemoryMap> TilemapMemoryMap::OpenFilemap(const QString& file)
{
    auto io = std::make_unique<QFile>();
    io->setFileName(file);
    io->open(QIODevice::ReadOnly);
    if (!io->isOpen())
    {
        ERROR("File open error. [file='%1', error='%2']", file, io->errorString());
        return nullptr;
    }
    uchar* addr = io->map(0, io->size());
    if (addr == nullptr)
    {
        ERROR("Failed to create memory mapping. [file='%1']", file, io->errorString());
        return nullptr;
    }
    auto ret = std::make_shared<TilemapMemoryMap>(addr, io->size(), std::move(io));
    return ret;
}

} // namespace
