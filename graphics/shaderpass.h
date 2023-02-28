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

namespace gfx
{
    class Program;

    // Shader passes are low level objects that are passed down to the
    // materials and drawables for the purpose of modifying the shader
    // behaviour in order to make it more suited/efficient/applicable
    // for the current rendering pass.
    class ShaderPass
    {
    public:
        // Rough shader pass type. In some cases the type can be
        // used to distinguish special cases in materials. For example
        // when doing a stencil pass the assumption is that only
        // stencil buffer is being updated which means complete color
        // computation can be skipped.
        enum class Type {
            Color,
            Stencil,
            Custom
        };
        // Inspect the current draw and its associated user object.
        // The void* user maps to void* user in the Painter's DrawShape.
        // If the function returns false the draw is skipped.
        virtual bool FilterDraw(void* user) const { return true; }
        // Modify the fragment shader source. The minimum that a shader pass
        // should do is to append the 'vec4 ShaderPass(vec4 color)' function
        // to the shader source.
        virtual std::string ModifyFragmentSource(Device& device, std::string source) const;
        // Modify the vertex shader source. TODO:
        virtual std::string ModifyVertexSource(Device& device, std::string source) const;
        // Get a hash value representing the state of this shader pass object.
        virtual std::size_t GetHash() const = 0;
        // Get the human-readable name of the shader pass for debugging/logging purposes.
        virtual std::string GetName() const = 0;
        // Get the rough shader pass type. See the Type enum for more details.
        virtual Type GetType() const = 0;
        // Apply any shader pass specific state on the GPU program object and
        // on the device state. When any object is being rendered this is the final
        // place to change any of the state required to draw. I.e. the state coming in
        // is the combination of the state from the drawable, material and painter and
        // this applies both to the program object and to the state object.
        virtual void ApplyDynamicState(Program& program, Device::State& state) const {}
    private:
    };

    namespace detail {
        class GenericShaderPass : public ShaderPass
        {
        public:
            virtual std::size_t GetHash() const override
            { return base::hash_combine(0, "generic-shader-pass"); }
            virtual std::string GetName() const override
            { return "GenericShaderPass"; }
            virtual Type GetType() const override
            { return Type::Color; }
        private:
        };

        class StencilShaderPass : public ShaderPass
        {
        public:
            virtual std::size_t GetHash() const override
            { return base::hash_combine(0, "stencil-shader-pass"); }
            virtual std::string GetName() const override
            { return "StencilShaderPass"; }
            virtual Type GetType() const override
            { return Type::Stencil; }
        private:
        };

    } // namespace

} // namespace
