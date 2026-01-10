// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#include "graphics/drawable.h"

namespace test
{
    class TestDrawable : public gfx::Drawable
    {
    public:
        bool ApplyDynamicState(const Environment& env, gfx::Device& device, gfx::ProgramState& program, RasterState& state) const override
        {
            return true;
        }
        gfx::ShaderSource GetShader(const Environment& env, const gfx::Device& device) const override
        {
            return Drawable::CreateShader(env, device, Shader::Simple2D);
        }
        std::string GetShaderId(const Environment& env) const override
        {
            return Drawable::GetShaderId(env, Shader::Simple2D);
        }
        std::string GetShaderName(const Environment& env) const override
        {
            return Drawable::GetShaderName(env, Shader::Simple2D);
        }
        std::string GetGeometryId(const Environment& env) const override
        {
            return "test-geometry-id";
        }
        bool Construct(const Environment& env, gfx::Device& device, gfx::Geometry::CreateArgs& geometry) const override
        {
            return !fail_construct;
        }
        DrawPrimitive GetDrawPrimitive() const override
        {
            return DrawPrimitive::Triangles;
        }
        Type GetType() const override
        {
            return Type::Other;
        }
        SpatialMode GetSpatialMode() const override
        {
            return SpatialMode::Flat2D;
        }
        Usage GetGeometryUsage() const override
        {
            return usage;
        }
        bool fail_construct = false;
        size_t content_hash = 0;
        Usage usage = Usage::Static;
    };
} // namespace