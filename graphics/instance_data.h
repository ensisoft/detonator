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

#include <cstdint>
#include <string>
#include <memory>

#include "graphics/instance_buffer.h"

namespace gfx
{
    // Per geometry instance vertex data on the GPU.
    class InstanceData
    {
    public:
        using Usage = InstanceBuffer::Usage;
        struct CreateArgs {
            InstanceBuffer buffer;
            // The expected usage of the geometry instance data.
            Usage usage = Usage::Stream;
            // Set the (human-readable) name of the instance geometry.
            // This has debug significance only.
            std::string content_name;
            // Set the hash value based on the contents of the buffer.
            std::size_t content_hash = 0;
        };
        virtual ~InstanceData() = default;

        virtual std::size_t GetContentHash() const = 0;
        virtual std::string GetContentName() const = 0;
        virtual void SetContentHash(std::size_t hash) = 0;
        virtual void SetContentName(std::string name) = 0;
    private:
    };

    using InstanceDataPtr = std::shared_ptr<const InstanceData>;

} // namespace
