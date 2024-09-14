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

namespace data
{
    class Reader;
    class Writer;
    class IODevice;

    class Chunk
    {
    public:
        virtual ~Chunk() = default;

        virtual const Reader* GetReader() const noexcept = 0;
        virtual Writer* GetWriter() noexcept = 0;

        virtual void OverwriteChunk(const char* name, std::unique_ptr<Chunk> chunk) = 0;
        virtual void OverwriteChunk(const char* name, std::unique_ptr<Chunk> chunk, unsigned index) = 0;

        // Dump and write the contents of this chunk to the IO device.
        virtual bool Dump(IODevice& device) const = 0;
    private:

    };
} // namespace