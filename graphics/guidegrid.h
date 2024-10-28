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

#include "graphics/drawable.h"

namespace gfx
{

    // render a series of intersecting horizontal and vertical lines
    // at some particular interval (gap distance)
    class Grid : public Drawable
    {
    public:
        // the num vertical and horizontal lines is the number of lines
        // *inside* the grid. I.e. not including the enclosing border lines
        Grid(unsigned num_vertical_lines, unsigned num_horizontal_lines, bool border_lines = true) noexcept
          : mNumVerticalLines(num_vertical_lines)
          , mNumHorizontalLines(num_horizontal_lines)
          , mBorderLines(border_lines)
        {}
        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const override;
        virtual ShaderSource GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;
        virtual std::string GetGeometryId(const Environment& env) const override;
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& geometry) const override;

        virtual DrawPrimitive GetDrawPrimitive() const override
        { return DrawPrimitive::Lines; }
        virtual Usage GetGeometryUsage() const override
        { return Usage::Static; }
    private:
        unsigned mNumVerticalLines = 1;
        unsigned mNumHorizontalLines = 1;
        bool mBorderLines = false;
    };


} // namespace