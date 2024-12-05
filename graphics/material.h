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

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <utility>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <cmath>

#include "base/utility.h"
#include "base/assert.h"
#include "base/hash.h"
#include "base/bitflag.h"
#include "data/fwd.h"
#include "graphics/texture.h"
#include "graphics/packer.h"
#include "graphics/bitmap.h"
#include "graphics/color4f.h"
#include "graphics/device.h"
#include "graphics/image.h"
#include "graphics/types.h"
#include "graphics/text.h"
#include "graphics/program.h"
#include "graphics/bitmap.h"
#include "graphics/bitmap_noise.h"
#include "graphics/texture_source.h"
#include "graphics/texture_map.h"

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
        virtual std::string GetClassId() const = 0;
        // Update material time by a delta value (in seconds).
        virtual void Update(float dt) = 0;
        // Set the material instance time to a specific time value.
        virtual void SetRuntime(double runtime) = 0;
        // Set material instance uniform.
        virtual void SetUniform(const std::string& name, const Uniform& value) = 0;
        // Set material instance uniform.
        virtual void SetUniform(const std::string& name, Uniform&& value) = 0;
        // Set all material uniforms at once.
        virtual void SetUniforms(const UniformMap& uniforms) = 0;
        // Clear away all material instance uniforms.
        virtual void ResetUniforms() = 0;
        // Get the current material time.
        virtual double GetRuntime() const = 0;
        // Get the material class instance if any. Warning, this may be null for
        // material objects that aren't based on any material clas!
        virtual const MaterialClass* GetClass() const  { return nullptr; }
    private:
    };

    // Material instance that represents an instance of some material class.
    class MaterialInstance : public Material
    {
    public:
        // Create new material instance based on the given material class.
        explicit MaterialInstance(std::shared_ptr<const MaterialClass> klass, double time = 0.0);
        explicit MaterialInstance(const MaterialClass& klass, double time = 0.0);

        // Apply the material properties to the given program object and set the rasterizer state.
        bool ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& raster) const override;
        void ApplyStaticState(const Environment& env, Device& device, ProgramState& program) const override;
        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment& env) const override;
        std::string GetShaderName(const Environment& env) const override;
        std::string GetClassId() const override;

        void Update(float dt) override
        { mRuntime += dt; }
        void SetRuntime(double runtime) override
        { mRuntime = runtime; }
        void SetUniform(const std::string& name, const Uniform& value) override
        { mUniforms[name] = value; }
        void SetUniform(const std::string& name, Uniform&& value) override
        { mUniforms[name] = std::move(value); }
        void ResetUniforms()  override
        { mUniforms.clear(); }
        void SetUniforms(const UniformMap& uniforms) override
        { mUniforms = uniforms; }
        double GetRuntime() const override
        { return mRuntime; }
        const MaterialClass* GetClass() const override
        { return mClass.get(); }

        // Shortcut operator for accessing the class object instance.
        const MaterialClass* operator->() const
        { return mClass.get(); }

        inline bool HasError() const noexcept
        { return mError; }
    private:
        // This is the "class" object for this material type.
        std::shared_ptr<const MaterialClass> mClass;
        // Current runtime for this material instance.
        double mRuntime = 0.0f;
        // material properties (uniforms) specific to this instance.
        UniformMap mUniforms;
        // I don't like this flag but when trying to make it easier for the
        // game developer to figure out what's wrong we need logging
        // but it'd be nice if the problem was logged only once instead
        // of spamming the log continuously.
        mutable bool mFirstRender = true;
        mutable bool mError = false;
    };


    // These functions are intended to be used when you just need to draw
    // something immediately and don't need to keep the material around.
    // It's unspecified whether any material classes are used or so on.
    // That means that the all the materials of some particular type *may*
    // share the material class which gets modified.

    MaterialInstance CreateMaterialFromColor(const Color4f& top_left,
                                             const Color4f& top_right,
                                             const Color4f& bottom_left,
                                             const Color4f& bottom_right);
    MaterialInstance CreateMaterialFromColor(const Color4f& color);
    MaterialInstance CreateMaterialFromImage(const std::string& uri);
    MaterialInstance CreateMaterialFromImages(const std::initializer_list<std::string>& uris);
    MaterialInstance CreateMaterialFromImages(const std::vector<std::string>& uris);
    MaterialInstance CreateMaterialFromSpriteAtlas(const std::string& uri, const std::vector<FRect>& frames);
    MaterialInstance CreateMaterialFromText(const TextBuffer& text);
    MaterialInstance CreateMaterialFromText(TextBuffer&& text);
    MaterialInstance CreateMaterialFromTexture(std::string gpu_id, Texture* texture = nullptr);

    std::unique_ptr<Material> CreateMaterialInstance(const MaterialClass& klass);
    std::unique_ptr<Material> CreateMaterialInstance(const std::shared_ptr<const MaterialClass>& klass);

} // namespace
