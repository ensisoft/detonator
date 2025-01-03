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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#include "warnpop.h"

#include <vector>

#include "graphics/drawable.h"

namespace gfx
{

    class LineBatch2D : public Drawable
    {
    public:
        struct Line {
            glm::vec2 start = {0.0f, 0.0};
            glm::vec2 end   = {0.0f, 0.0f};
        };
        LineBatch2D() = default;
        LineBatch2D(const glm::vec2& start, glm::vec2& end)
        {
            Line line;
            line.start = start;
            line.end   = end;
            mLines.push_back(line);
        }
        explicit LineBatch2D(std::vector<Line> lines) noexcept
          : mLines(std::move(lines))
        {}
        inline void AddLine(Line line)
        { mLines.push_back(line); }
        inline void AddLine(const glm::vec2& start, const glm::vec2& end)
        {
            Line line;
            line.start = start,
            line.end   = end;
            mLines.push_back(line);
        }
        inline void AddLine(float x0, float y0, float x1, float y1)
        {
            Line line;
            line.start = {x0, y0};
            line.end   = {x1, y1};
            mLines.push_back(line);
        }

        void ApplyDynamicState(const Environment& environment, ProgramState& program, RasterState& state) const override;
        ShaderSource GetShader(const Environment& environment, const Device& device) const override;
        std::string GetShaderId(const Environment& environment) const override;
        std::string GetShaderName(const Environment& environment) const override;
        std::string GetGeometryId(const Environment& environment) const override;
        bool Construct(const Environment& environment, Geometry::CreateArgs& create) const override;
        DrawPrimitive GetDrawPrimitive() const override
        { return DrawPrimitive::Lines; }
        Usage GetGeometryUsage() const override
        { return Usage::Stream; }
        Type GetType() const override
        { return Type::LineBatch2D; }
    private:
        std::vector<Line> mLines;
    };


    class LineBatch3D : public Drawable
    {
    public:
        struct Line {
            glm::vec3 start = {0.0f, 0.0f, 0.0f};
            glm::vec3 end   = {0.0f, 0.0f, 0.0f};
        };

        LineBatch3D() = default;
        LineBatch3D(const glm::vec3& start, const glm::vec3& end)
        {
            Line line;
            line.start = start;
            line.end   = end;
            mLines.push_back(line);
        }
        explicit LineBatch3D(std::vector<Line> lines) noexcept
                : mLines(std::move(lines))
        {}
        inline void AddLine(Line line)
        { mLines.push_back(std::move(line)); }
        inline void AddLine(glm::vec3 start, glm::vec3 end)
        { mLines.push_back({ start, end }); }

        void ApplyDynamicState(const Environment& environment, ProgramState& program, RasterState& state) const override;
        ShaderSource GetShader(const Environment& environment, const Device& device) const override;
        std::string GetShaderId(const Environment& environment) const override;
        std::string GetShaderName(const Environment& environment) const override;
        std::string GetGeometryId(const Environment& environment) const override;
        bool Construct(const Environment& environment, Geometry::CreateArgs& create) const override;
        DrawPrimitive GetDrawPrimitive() const override
        { return DrawPrimitive::Lines; }
        Usage GetGeometryUsage() const override
        { return Usage::Stream; }
        Type GetType() const override
        { return Type::LineBatch3D; }
    private:
        std::vector<Line> mLines;
    };


} // namespace