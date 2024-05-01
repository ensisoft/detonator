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

#include <cstddef>

#include "base/assert.h"
#include "base/utility.h"
#include "data/fwd.h"
#include "graphics/geometry.h"

namespace gfx
{
    class GeometryDrawCommand
    {
    public:
        using DrawCommand = Geometry::DrawCommand;

        GeometryDrawCommand(const Geometry& geometry) noexcept
          : mGeometry(&geometry)
          , mCmdStart(0)
          , mCmdCount(geometry.GetNumDrawCmds())
        {}

        GeometryDrawCommand(const Geometry& geometry, size_t cmd_start, size_t cmd_count)
          : mGeometry(&geometry)
          , mCmdStart(cmd_start)
          , mCmdCount(ResolveCount(geometry, cmd_count))
        {}
        inline size_t GetNumDrawCmds() const noexcept
        { return mCmdCount; }
        inline DrawCommand GetDrawCmd(size_t index) const noexcept
        { return mGeometry->GetDrawCmd(mCmdStart + index); }
        inline const Geometry* GetGeometry() const noexcept
        { return mGeometry;}

        static size_t ResolveCount(const Geometry& geometry, size_t count) noexcept
        {
            if (count == std::numeric_limits<size_t>::max())
                return geometry.GetNumDrawCmds();
            return count;
        }

    private:
        const Geometry* mGeometry = nullptr;
        const size_t mCmdStart = 0;
        const size_t mCmdCount = 0;
    };

    class CommandStream
    {
    public:
        using DrawCommand = Geometry::DrawCommand;

        explicit CommandStream(const std::vector<DrawCommand>& commands) noexcept
          : mCommands(commands.data())
          , mCount(commands.size())
        {}
        CommandStream(const DrawCommand* commands, size_t count) noexcept
          : mCommands(commands)
          , mCount(count)
        {}

        inline size_t GetCount() const noexcept
        { return mCount; }
        inline DrawCommand GetCommand(size_t index) const noexcept
        {
            ASSERT(index < mCount);
            return mCommands[index];
        }

        void IntoJson(data::Writer& writer) const;

    private:
        const DrawCommand* mCommands = nullptr;
        const size_t mCount = 0;
    };

    class CommandBuffer
    {
    public:
        using DrawCommand = Geometry::DrawCommand;

        explicit CommandBuffer(std::vector<DrawCommand>* commands) noexcept
          : mBuffer(commands)
        {}
        CommandBuffer() noexcept
          : mBuffer(&mStorage)
        {}

        inline size_t GetCount() const noexcept
        { return mBuffer->size(); }
        inline DrawCommand GetCommand(size_t index) const noexcept
        { return base::SafeIndex(*mBuffer, index); }
        inline const auto& GetCommandBuffer() const & noexcept
        { return *mBuffer; }
        inline auto&& GetCommandBuffer() && noexcept
        { return std::move(*mBuffer); }
        inline void PushBack(const DrawCommand& cmd)
        { mBuffer->push_back(cmd); }

        bool FromJson(const data::Reader& reader);

    private:
        std::vector<DrawCommand> mStorage;
        std::vector<DrawCommand>* mBuffer = nullptr;
    };

} // namespace
