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

#include "graphics/shader_program.h"

namespace gfx
{
    class GenericShaderProgram : public ShaderProgram
    {
    public:
        GenericShaderProgram() = default;
        explicit GenericShaderProgram(std::string name, RenderPass renderPass) noexcept
          : mName(std::move(name))
        {}

        std::string GetName() const override
        { return mName; }

        RenderPass GetRenderPass() const override
        { return mRenderPass; }

        std::string GetShaderId(const Material& material, const Material::Environment& env) const override;
        std::string GetShaderId(const Drawable& drawable, const Drawable::Environment& env) const override;
        ShaderSource GetShader(const Material& material, const Material::Environment& env, const Device& device) const override;
        ShaderSource GetShader(const Drawable& drawable, const Drawable::Environment& env, const Device& device) const override;

    private:
        std::string mName;
        RenderPass mRenderPass = RenderPass::ColorPass;
    };
} // namespace
