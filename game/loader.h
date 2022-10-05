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

#pragma once

#include "config.h"

#include <memory>

namespace game
{
    // Interface for accessing tilemap (layer) data.
    class TilemapData
    {
    public:
        virtual ~TilemapData() = default;
        // Write data to the given offset in the buffer.
        // The data to be written should always be within the previously allocated
        // dimensions of the data buffer anything else is a BUG.
        virtual void Write(const void* ptr, size_t bytes, size_t offset) = 0;
        // Read data from the given offset in the buffer.
        // The read offset and number of bytes should always be within the
        // previously allocated dimensions of the data buffer. Anything else is a BUG.
        virtual void Read(void* ptr, size_t bytes, size_t offset) const = 0;
        // Append a new chunk of data to the buffer and reshape the buffer's dimensions.
        // The new chunk is expected to be allocated at the end of any previously
        // allocated data buffer thus forming a single contiguous memory range.
        // Returns an offset value into this new larger buffer in which the allocated
        // chunk begins.
        virtual size_t AppendChunk(size_t bytes) = 0;
        // Get the total size of the map buffer in bytes.
        virtual size_t GetByteCount() const = 0;
        // Resize the underlying memory buffer to a new size. The new size can be
        // bigger of smaller than any previous size.
        virtual void Resize(size_t bytes) = 0;
        // Clear a memory region/chunk with a value that is to be repeatedly copied over
        // the specified region starting at the given offset. value and value_size specify
        // the object and its size in terms of raw memory.
        // num_values is the number of items to be copied, i.e. how many times the value+value_size
        // is memcpy'd into the underlying buffer. Each write offset needs to be within
        // any previously allocated buffer size. Anything else is a BUG.
        virtual void ClearChunk(const void* value, size_t value_size, size_t offset, size_t num_values) = 0;
    private:
    };

    using TilemapDataHandle = std::shared_ptr<TilemapData>;

    class Loader
    {
    public:
        virtual ~Loader() = default;

        // Load the data for a tilemap layer based on the layer ID and the
        // associated data file URI. The read only flag indicates whether
        // the map is allowed to be modified by the running game itself.
        // Returns a handle to the tilemap data object or nullptr if the
        // loading fails.
        virtual TilemapDataHandle LoadTilemapData(const std::string& id, const std::string& uri, bool read_only) const = 0;
    private:

    };
} // namespace

