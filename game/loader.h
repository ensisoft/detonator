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
    class TilemapData
    {
    public:
        virtual ~TilemapData() = default;
        virtual void Write(const void* ptr, size_t bytes, size_t offset) = 0;
        virtual void Read(void* ptr, size_t bytes, size_t offset) const = 0;
        virtual size_t AppendChunk(size_t bytes) = 0;
        virtual size_t GetByteCount() const = 0;
        virtual void Resize(size_t bytes) = 0;
        virtual void ClearChunk(const void* value, size_t value_size, size_t offset, size_t num_values) = 0;
    private:
    };

    using TilemapDataHandle = std::shared_ptr<TilemapData>;

    class Loader
    {
    public:
        virtual ~Loader() = default;

        virtual TilemapDataHandle LoadTilemapData(const std::string& id, bool read_only) const = 0;

    private:

    };
} // namespace

