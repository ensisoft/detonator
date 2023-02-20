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

    class ShaderPass
    {
    public:
        enum class Type {
            Color,
            Stencil
        };
        virtual std::string ModifyFragmentSource(Device& device, std::string source) const;
        virtual std::string ModifyVertexSource(Device& device, std::string source) const;
        virtual std::size_t GetHash() const = 0;
        virtual std::string GetName() const = 0;
        virtual Type GetType() const = 0;
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
