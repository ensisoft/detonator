// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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

#include "graphics/types.h"
#include "graphics/device.h"

namespace gfx
{
    class Device;
    class ShaderSource;
    class TextureSource;
    class MaterialClass;

    // Material instance. Each instance can contain and alter the
    // instance specific state even between instances that refer
    // to the same underlying material type.
    class Material
    {
    public:
        using Uniform    = gfx::Uniform;
        using UniformMap = gfx::UniformMap;
        using RenderPass = gfx::RenderPass;

        struct Environment {
            // true if running in an "editing mode", which means that even
            // content marked static might have changed and should be checked
            // in case it has been modified and should be re-uploaded.
            bool editing_mode  = false;
            DrawPrimitive draw_primitive = DrawPrimitive::Triangles;
            DrawCategory draw_category = DrawCategory::Basic;
            RenderPass renderpass = RenderPass::ColorPass;
        };
        struct RasterState {
            using Blending = Device::State::BlendOp;
            Blending blending = Blending::None;
            bool premultiplied_alpha = false;
        };

        virtual ~Material() = default;
        // Apply the dynamic material properties to the given program object
        // and set the rasterizer state. Dynamic properties are the properties
        // that can change between one material instance to another even when
        // the underlying material type is the same. For example two instances
        // of material "marble" have the same underlying static type "marble"
        // but both instances have their own instance state as well.
        // Returns true if the state is applied correctly and drawing can proceed.
        virtual bool ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& raster) const = 0;
        // Apply the static state, i.e. the material state that doesn't change
        // during the material's lifetime and need to be only set once.
        virtual void ApplyStaticState(const Environment& env, Device& device, ProgramState& program) const = 0;
        // Get the device specific shader source applicable for this material, its state
        // and the given environment in which it should execute.
        virtual ShaderSource GetShader(const Environment& env, const Device& device) const = 0;
        // Get the shader ID applicable for this material, its state and the given
        // environment in which it should execute.
        virtual std::string GetShaderId(const Environment& env) const = 0;
        // Get the human readable debug name that should be associated with the
        // shader object generated from this drawable.
        virtual std::string GetShaderName(const Environment& env) const = 0;
        // Get the material class id (if any).
        virtual std::string GetClassId() const { return {}; }
        // Update material time by a delta value (in seconds).
        virtual void Update(float dt) {}
        // Set the material instance time to a specific time value.
        virtual void SetRuntime(double runtime) {}
        // Set material instance uniform.
        virtual void SetUniform(const std::string& name, Uniform value) {}
        // Set all material uniforms at once.
        virtual void SetUniforms(UniformMap uniforms) {}
        // Clear away all material instance uniforms.
        virtual void ResetUniforms() {}
        // Get the current material time.
        virtual double GetRuntime() const { return 0.0; }
        // Get the material class instance if any. Warning, this may be null for
        // material objects that aren't based on any material clas!
        virtual const MaterialClass* GetClass() const  { return nullptr; }
    private:
    };


} // namespace
