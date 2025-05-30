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

#include <memory>

#include "graphics/material.h"

namespace gfx
{
    class TextBuffer;

    // Material instance that represents an instance of some material class.
    class MaterialInstance : public Material
    {
    public:
        // Create new material instance based on the given material class.
        explicit MaterialInstance(std::shared_ptr<const MaterialClass> klass, double time = 0.0);
        explicit MaterialInstance(const MaterialClass& klass, double time = 0.0);
        explicit MaterialInstance(MaterialClass&& klass, double time = 0.0) noexcept;

        void SetFlag(Flags flag, bool on_off) noexcept override
        {
            if (on_off)
                mFlags |= static_cast<uint32_t>(flag);
            else mFlags &= ~static_cast<uint32_t>(flag);
        }

        bool TestFlag(Flags flag) const noexcept override
        {
            return (mFlags & static_cast<uint32_t>(flag)) != 0;
        }

        // Apply the material properties to the given program object and set the rasterizer state.
        bool ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& raster) const override;
        void ApplyStaticState(const Environment& env, Device& device, ProgramState& program) const override;
        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment& env) const override;
        std::string GetShaderName(const Environment& env) const override;
        std::string GetClassId() const override;
        bool Execute(const Environment& env, const Command& cmd) override;
        void Update(float dt) override;
        void SetRuntime(double runtime) override;
        bool GetValue(const std::string& key, RuntimeValue* value) const override;

        void SetUniform(const std::string& name, Uniform value) override
        { mUniforms[name] = std::move(value); }
        void ResetUniforms()  override
        { mUniforms.clear(); }
        void SetUniforms(UniformMap uniforms) override
        { mUniforms = std::move(uniforms); }
        double GetRuntime() const override
        { return mRuntime; }
        const MaterialClass* GetClass() const override
        { return mClass.get(); }

        std::unique_ptr<Material> Clone() const override;

        // Shortcut operator for accessing the class object instance.
        const MaterialClass* operator->() const
        { return mClass.get(); }

        inline bool HasError() const noexcept
        { return mError; }
    private:
        void InitFlags() noexcept;
    private:
        // This is the "class" object for this material type.
        std::shared_ptr<const MaterialClass> mClass;
        std::uint32_t mFlags = 0;
        // Current runtime for this material instance.
        double mRuntime = 0.0f;
        // material properties (uniforms) specific to this instance.
        UniformMap mUniforms;

        struct SpriteCycleRun {
            float delay = 0.0f;
            bool started = false;
            double runtime = 0.0;
            // ID of the texture map that runs this sprite animation cycle
            std::string id;
            std::string name;
        };
        std::optional<SpriteCycleRun> mSpriteCycle;

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
    std::unique_ptr<Material> CreateMaterialInstance(MaterialClass&& klass);
    std::unique_ptr<Material> CreateMaterialInstance(const std::shared_ptr<const MaterialClass>& klass);

} // namespace