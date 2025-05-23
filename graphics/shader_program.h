// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include <string>
#include <cstdint>
#include <optional>

#include "base/hash.h"
#include "graphics/device.h"
#include "graphics/types.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/types.h"

namespace gfx
{
    class ShaderSource;

    // Shader program provides the GPU shader sources for generating
    // device specific GPU shader programs based on materials and drawables.
    // Both materials and drawables provide some part of the shader functionality
    // to create input for the rest of the program.
    class ShaderProgram
    {
    public:
        using RenderPass = gfx::RenderPass;

        virtual ~ShaderProgram() = default;

        virtual RenderPass GetRenderPass() const { return RenderPass::ColorPass; }

        // Inspect the current draw and its associated user object.
        // The void* user maps to void* user in the Painter's DrawCommand.
        // If the function returns false the draw is skipped.
        virtual bool FilterDraw(void* user) const { return true; }
        // Get the material object fragment shader device ID.
        // The default implementation will simply call the material in order to generate the ID.
        virtual std::string GetShaderId(const Material& material, const Material::Environment& env) const = 0;
        // Get the drawable object vertex shader device ID.
        // The default implementation will simply call the drawable in order to generate the ID.
        virtual std::string GetShaderId(const Drawable& drawable, const Drawable::Environment& env) const = 0;
        // Get the device specific material (fragment) shader source.
        virtual ShaderSource GetShader(const Material& material, const Material::Environment& env, const Device& device) const = 0;
        // Get the device specific drawable (vertex) shader source.
        virtual ShaderSource GetShader(const Drawable& drawable, const Drawable::Environment& env, const Device& device) const = 0;

        // Get the human-readable name of the shader pass for debugging/logging purposes.
        virtual std::string GetName() const = 0;
        // Apply any shader program state on the GPU program object and on the device state.
        // When any object is being rendered this is the final place to change any of the state
        // required to draw. I.e. the state coming in is the combination of the state from the
        // drawable, material and painter. This applies both to the program and the state object.
        // This is called for every painter draw command.
        virtual void ApplyDynamicState(const Device& device, ProgramState& program, Device::RasterState& state, void* user) const {}

        // Apply any shader program state dynamically once on the GPU program when the
        // program is used for the first time for the current draw command list.
        // In other words if you're using the same shader program to draw multiple objects
        // you can use this function to apply any state on the *GPU* program once while
        // drawing all the current painter draw commands in the draw cmd list.
        virtual void ApplyDynamicState(const Device& device, ProgramState& program) const {}

        // todo:
        virtual void ApplyStaticState(const Device& device, ProgramState& program) const {}
    private:
    };

} // namespace
