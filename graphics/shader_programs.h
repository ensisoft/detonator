// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include "graphics/generic_shader_program.h"

namespace gfx
{
    class FlatShadedColorProgram : public GenericShaderProgram
    {
    public:
        FlatShadedColorProgram(std::string renderer_name = "") noexcept
          : GenericShaderProgram("FlatShadedColorProgram", std::move(renderer_name))
        {}
    private:
    };

    class StencilShaderProgram : public GenericShaderProgram
    {
    public:
        StencilShaderProgram(std::string renderer_name = "") noexcept
           : GenericShaderProgram("StencilShaderProgram", std::move(renderer_name))
        {}
    private:
    };

    class BasicLightProgram : public GenericShaderProgram
    {
    public:
        BasicLightProgram(std::string renderer_name = "") noexcept
          : GenericShaderProgram("BasicLightProgram", std::move(renderer_name))
        {
            EnableFeature(ShadingFeatures::BasicLight, true);
        }
    private:
    };

} // namespace