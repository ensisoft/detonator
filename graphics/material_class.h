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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#include "warnpop.h"

#include <string>
#include <variant>
#include <unordered_map>
#include <vector>
#include <optional>

#include "base/utility.h"
#include "graphics/color4f.h"
#include "graphics/texture.h"
#include "graphics/types.h"
#include "graphics/texture_map.h"
#include "graphics/shader_source.h"
#include "graphics/material_uniform.h"

namespace gfx
{
    class TexturePacker;
    class Device;
    class Program;
    class ProgramState;
    class TextBuffer;

    // Interface for classes of materials. Each material class implements
    // some type of shading algorithm expressed using the OpenGL (ES)
    // shading language and provides means for setting the properties
    // (uniforms) and the texture maps that are used by the shading algo.
    class MaterialClass
    {
    public:
        using MinTextureFilter = Texture::MinFilter;
        using MagTextureFilter = Texture::MagFilter;
        using TextureWrapping  = Texture::Wrapping;
        using ParticleEffect   = gfx::ParticleEffect;
        using ParticleRotation = gfx::ParticleRotation;
        using GradientType = gfx::GradientType;

        // The four supported colors are all identified by
        // a color index that maps to the 4 quadrants of the
        // texture coordinate space.
        enum class ColorIndex {
            BaseColor,
            GradientColor0, // top left
            GradientColor1, // top right
            GradientColor2, // bottom left
            GradientColor3, // bottom right
            AmbientColor,
            DiffuseColor,
            SpecularColor,
            ParticleStartColor,
            ParticleMidColor,
            ParticleEndColor
        };

        // Control the rasterizer blending operation and how the
        // output from this material's material shader is combined
        // with the existing color buffer fragments.
        enum class SurfaceType {
            // Surface is opaque and no blending is done.
            Opaque,
            // Surface is transparent and is blended with the destination
            // to create an interpolated mix of the colors.
            Transparent,
            // Surface gives off color (light)
            Emissive
        };

        // The type of the material.
        enum class Type {
            // Built-in color only material.
            Color,
            // Built-in gradient only material.
            Gradient,
            // Built-in material using a static texture.
            Texture,
            // Built-in material using a single sprite map
            // to display texture based animation.
            Sprite,
            // Built-in material using a single tile map
            // to display "tiles" based on a regular texture layout
            Tilemap,
            // Built-in 2D particle material
            Particle2D,
            // Material with basic light interaction
            BasicLight,
            // Custom material with user defined material
            // shader and an arbitrary number of uniforms
            // and texture maps.
            Custom
        };

        enum class Flags {
            // When set indicates that the material class is static and
            // Uniforms are folded into constants in the shader source or
            // set only at once when the program is created.
            Static,
            // When set indicates that the sprite animation frames are
            // blended together to create "tween" frames.
            BlendFrames,
            // When set, change the transparent blending equation
            // to expect alpha values to be premultiplied in the
            // RGB values.
            PremultipliedAlpha,
            // Enable bloom output
            Bloom,
            // Enable light on this material class.
            Lighting,
            // Enable fog on this material class.
            Fog,
            // Enable built-in SDF shape computation in the material shader.
            // Which shape and how is then controlled by the SDFShape and
            // SDFShapeFillMode uniforms.
            EnableSDF
        };

        enum class SDFShape : int {
            None,
            Circle,
            Rect, RoundRect,
            Parallelogram,
            HorizontalCapsule, VerticalCapsule
        };

        enum class SDFShapeFillMode {
            Solid,
            Outline
        };

        // The current material state to apply when applying the
        // material's *dynamic* state onto the program object.
        // The dynamic state is state that can change over time and
        // between material instances sharing the same underlying
        // material type. For example if there exists multiple material
        // instances of a single material type called they might still
        // have a different state.
        struct State {
            // true if running in an "editing mode", which means that even
            // content marked static might have changed and should be checked
            // in case it has been modified and should be re-uploaded.
            bool editing_mode = false;
            // what primitives will be drawn with this material.
            DrawPrimitive draw_primitive = DrawPrimitive::Triangles;
            // What is the drawable category and data interface we
            // can / need to expect.
            DrawCategory draw_category = DrawCategory::Basic;
            // The current material instance time.
            double material_time = 0.0f;
            // The uniform parameters set on the material instance (if any).
            // The instance uniforms will take precedence over the uniforms
            // set in the class whenever they're set.
            const UniformMap* uniforms = nullptr;
            // Current render pass the material is used in
            RenderPass render_pass = RenderPass::ColorPass;

            // see material flags
            std::uint32_t flags = 0;
            // This setting overrides the active texture map set in the class.
            // Used for things like
            // a) game wants to parametrize the material and change the appearance
            //    long term
            // b) a sprite cycle is being run and while the sprite cycle runs the
            //    sprite cycle texture map changes
            std::string active_texture_map_id;
        };

        explicit MaterialClass(Type type, std::string id = base::RandomString(10));
        MaterialClass(const MaterialClass& other, bool copy=true);
        MaterialClass(MaterialClass&& other) noexcept;
       ~MaterialClass();

        // Get the actual implementation type of the material.
        Type GetType() const noexcept
        { return mType; }
        // Get the surface type of the material.
        SurfaceType GetSurfaceType() const noexcept
        { return mSurfaceType; }

        bool IsBuiltIn() const noexcept
        { return mType != Type::Custom; }
        bool IsSprite() const noexcept
        { return mType == Type::Sprite; }

        // Set the surface type of the material.
        void SetSurfaceType(SurfaceType surface) noexcept
        { mSurfaceType = surface; }

        // Unsafe, but helpful for migration.
        void SetType(Type type) noexcept
        { mType = type; }

        // Set the human-readable material class name.
        void SetName(std::string name) noexcept
        { mName = std::move(name); }

        // Set a material flag to on or off.
        void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }
        void SetStatic(bool on_off) noexcept
        { mFlags.set(Flags::Static, on_off); }
        void SetBlendFrames(bool on_off) noexcept
        { mFlags.set(Flags::BlendFrames, on_off); }

        // Test material flag.
        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        bool PremultipliedAlpha() const noexcept
        { return TestFlag(Flags::PremultipliedAlpha); }
        bool IsStatic() const noexcept
        { return TestFlag(Flags::Static); }
        bool BlendFrames() const noexcept
        { return TestFlag(Flags::BlendFrames); }

        void SetTextureMinFilter(MinTextureFilter filter) noexcept
        { mTextureMinFilter = filter; }
        void SetTextureMagFilter(MagTextureFilter filter) noexcept
        { mTextureMagFilter = filter; }
        void SetTextureWrapX(TextureWrapping wrap) noexcept
        { mTextureWrapX = wrap; }
        void SetTextureWrapY(TextureWrapping wrap) noexcept
        { mTextureWrapY = wrap; }
        void SetActiveTextureMap(std::string id) noexcept
        { mActiveTextureMap = std::move(id); }

        void SetShaderUri(std::string uri) noexcept
        { mShaderUri = std::move(uri); }
        void SetShaderSrc(std::string src) noexcept
        { mShaderSrc = std::move(src); }
        bool HasShaderUri() const noexcept
        { return !mShaderUri.empty(); }
        bool HasShaderSrc() const noexcept
        { return !mShaderSrc.empty(); }
        void ClearShaderSrc() noexcept
        { mShaderSrc.clear(); }
        void ClearShaderUri() noexcept
        { mShaderUri.clear(); }

        std::string GetActiveTextureMap() const noexcept
        { return mActiveTextureMap; }
        // Get the material class id.
        std::string GetId() const noexcept
        { return mClassId; }
        // Get the human readable material class name.
        std::string GetName() const noexcept
        { return mName; }
        std::string GetShaderUri() const noexcept
        { return mShaderUri; }
        std::string GetShaderSrc() const noexcept
        { return mShaderSrc; }
        bool HasCustomShader() const noexcept
        { return !mShaderSrc.empty() || !mShaderUri.empty(); }

        MinTextureFilter GetTextureMinFilter() const noexcept
        { return mTextureMinFilter; }
        MagTextureFilter GetTextureMagFilter() const noexcept
        { return mTextureMagFilter; }
        TextureWrapping  GetTextureWrapX() const noexcept
        { return mTextureWrapX; }
        TextureWrapping GetTextureWrapY() const noexcept
        { return mTextureWrapY; }

        // statically typed uniform API

        template<typename kUniform>
        bool HasUniform() const noexcept
        {
            return base::Contains(mUniforms, kUniform::uniform_name);
        }

        template<typename kUniform>
        void DeleteUniform() noexcept
        {
            mUniforms.erase(kUniform::uniform_name);
        }

        template<typename kUniform>
        void SetUniform(const typename kUniform::T& value) noexcept
        {
            if (UniformEquals<kUniform>(value))
            {
                mUniforms.erase(kUniform::uniform_name);
                return;
            }
            using T = typename kUniform::T;
            if constexpr (std::is_enum<T>::value) {
                mUniforms[kUniform::uniform_name] = static_cast<int>(value);
            } else {
                mUniforms[kUniform::uniform_name] = value;
            }
        }

        template<typename kUniform>
        typename kUniform::T GetUniformValue() const noexcept
        {
            using T = typename kUniform::T;
            if (const auto* ptr = base::SafeFind(mUniforms, kUniform::uniform_name))
            {
                if constexpr (std::is_enum<T>::value) {
                    ASSERT(std::holds_alternative<int>(*ptr));
                    return static_cast<T>(std::get<int>(*ptr));
                } else {
                    ASSERT(std::holds_alternative<T>(*ptr));
                    return std::get<T>(*ptr);
                }
            }
            return kUniform::default_value;
        }

        // runtime uniform API.

        void SetUniform(const std::string& name, const Uniform& value)
        { mUniforms[name] = value; }

        bool HasUniform(const std::string& name) const noexcept
        { return base::SafeFind(mUniforms, name) != nullptr; }

        void DeleteUniform(const std::string& name) noexcept
        { mUniforms.erase(name); }

        template<typename T>
        T* FindUniformValue(const std::string& name) noexcept
        {
            if (auto* ptr = base::SafeFind(mUniforms, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        template<typename T>
        const T* FindUniformValue(const std::string& name) const noexcept
        {
            if (auto* ptr = base::SafeFind(mUniforms, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        template<typename T>
        T& GetUniformValue(const std::string& name, const T& init) noexcept
        {
            auto it = mUniforms.find(name);
            if (it == mUniforms.end())
                it = mUniforms.insert({name, init}).first;
            ASSERT(std::holds_alternative<T>(it->second));
            return std::get<T>(it->second);
        }
        template<typename T>
        const T& GetUniformValue(const std::string& name, const T& init) const noexcept
        {
            if (const auto* ptr = base::SafeFind(mUniforms, name)) {
                ASSERT(std::holds_alternative<T>(*ptr));
                return std::get<T>(*ptr);
            }
            return init;
        }
        template<typename T>
        bool CheckUniformType(const std::string& name) const noexcept
        {
            if (const auto* ptr = base::SafeFind(mUniforms, name)) {
                return std::holds_alternative<T>(*ptr);
            }
            return false;
        }
        template<typename T>
        bool HasUniform(const std::string& name) const noexcept
        {
            if (const auto* ptr = base::SafeFind(mUniforms, name))
                return std::get_if<T>(ptr) != nullptr;
            return false;
        }

        void DeleteUniforms() noexcept
        { mUniforms.clear(); }

        UniformMap GetUniforms() const
        { return mUniforms; }

        // Get the number of material maps in the material class.
        unsigned GetNumTextureMaps() const  noexcept
        { return mTextureMaps.size(); }

        // Get a texture map at the given index.
        const TextureMap* GetTextureMap(unsigned index) const noexcept
        { return base::SafeIndex(mTextureMaps, index).get(); }

        // Get a texture map at the given index.
        TextureMap* GetTextureMap(unsigned index) noexcept
        { return base::SafeIndex(mTextureMaps, index).get(); }

        // Delete the texture map at the given index.
        void DeleteTextureMap(unsigned index) noexcept
        { base::SafeErase(mTextureMaps, index); }

        void SetNumTextureMaps(unsigned count)
        { mTextureMaps.resize(count); }

        void SetTextureMap(unsigned index, std::unique_ptr<TextureMap> map) noexcept
        { base::SafeIndex(mTextureMaps, index) = std::move(map); }
        void SetTextureMap(unsigned index, TextureMap map) noexcept
        { base::SafeIndex(mTextureMaps, index) = std::make_unique<TextureMap>(std::move(map)); }

        std::string GetShaderName(const State& state) const noexcept;
        // Get the program ID for the material that is used to map the
        // material to a device specific program object.
        std::string GetShaderId(const State& state) const noexcept;
        // Get the material class hash value based on the current properties
        // of the class.
        std::size_t GetHash() const noexcept;

        ShaderSource GetShader(const State& state, const Device& device) const noexcept;
        // Apply the material properties onto the given program object based
        // on the material class and the material instance state.
        bool ApplyDynamicState(const State& state, Device& device, ProgramState& program) const noexcept;
        // Apply the static state, i.e. the material state that doesn't change
        // during the material's lifetime and need to be only set once.
        void ApplyStaticState(const State& state, Device& device, ProgramState& program) const noexcept;
        // Serialize the class into JSON.
        void IntoJson(data::Writer& data) const;
        // Load the class from JSON. Returns true on success.
        bool FromJson(const data::Reader& data, unsigned flags = 0);
        // Create an exact bitwise copy of this material class.
        std::unique_ptr<MaterialClass> Copy() const;
        // Create a similar clone of this material class but with unique id.
        std::unique_ptr<MaterialClass> Clone() const;
        // Begin the packing process by going over the associated resources
        // in the material and invoking the packer methods to pack
        // those resources.
        void BeginPacking(TexturePacker* packer) const;
        // Finish the packing process by retrieving the new updated resource
        // information the packer and updating the material's state.
        void FinishPacking(const TexturePacker* packer) ;

        // Helpers
        unsigned FindTextureMapIndexByName(const std::string& name) const;
        unsigned FindTextureMapIndexById(const std::string& id) const;
        unsigned FindTextureMapIndexBySampler(const std::string& name, unsigned sampler_index) const;

        TextureMap* FindTextureMapBySampler(const std::string& name, unsigned sampler_index);
        TextureMap* FindTextureMapByName(const std::string& name);
        TextureMap* FindTextureMapById(const std::string& id);
        const TextureMap* FindTextureMapBySampler(const std::string& name, unsigned sampler_index) const;
        const TextureMap* FindTextureMapByName(const std::string& name) const;
        const TextureMap* FindTextureMapById(const std::string& id) const;


        // Legacy / migration API
        void SetTexture(std::unique_ptr<TextureSource> source);
        void AddTexture(std::unique_ptr<TextureSource> source);
        void DeleteTextureSrc(const std::string& id) noexcept;
        void DeleteTextureMap(const std::string& id) noexcept;
        TextureSource* FindTextureSource(const std::string& id) noexcept;
        const TextureSource* FindTextureSource(const std::string& id) const noexcept;

        FRect FindTextureRect(const std::string& id) const noexcept;
        void SetTextureRect(const std::string& id, const gfx::FRect& rect) noexcept;
        void SetTextureRect(size_t map, size_t texture, const gfx::FRect& rect) noexcept;
        void SetTextureRect(const gfx::FRect& rect) noexcept;
        void SetTextureSource(size_t map, size_t texture, std::unique_ptr<TextureSource> source) noexcept;
        void SetTextureSource(std::unique_ptr<TextureSource> source) noexcept;

        enum LoadingFlags {
            EnableCaching = 0x1
        };

        static std::string GetColorUniformName(ColorIndex index);
        static std::unique_ptr<MaterialClass> ClassFromJson(const data::Reader& data, unsigned flags = 0u);

        MaterialClass& operator=(const MaterialClass& other);
    private:
        template<typename T>
        static bool SetUniform(const char* name, const UniformMap* uniforms, const T& backup, ProgramState& program);

        template<typename T>
        static void SetSDFUniform(const char* name, const UniformMap& uniforms, const T& backup, ProgramState& program);

        static bool SetUniform(const char* name, const UniformMap* uniforms, unsigned backup, ProgramState& program);

        template<typename T>
        bool ReadLegacyValue(const char* name, const char* uniform, const data::Reader& reader);

    private:
        TextureMap* SelectTextureMap(const State& state) const noexcept;
        ShaderSource GetShaderSource(const State& state, const Device& device) const;
        size_t GetShaderHash() const;

        bool ApplySpriteDynamicState(const State& state, Device& device, ProgramState& program) const noexcept;
        bool ApplyCustomDynamicState(const State& state, Device& device, ProgramState& program) const noexcept;
        bool ApplyTextureDynamicState(const State& state, Device& device, ProgramState& program) const noexcept;
        bool ApplyTilemapDynamicState(const State& state, Device& device, ProgramState& program) const noexcept;
        bool ApplyParticleDynamicState(const State& state, Device& device, ProgramState& program) const noexcept;
        bool ApplyBasicLightDynamicState(const State& state, Device& device, ProgramState& program) const noexcept;

    private:
        std::string mClassId;
        std::string mName;
        std::string mShaderUri;
        std::string mShaderSrc;
        std::string mActiveTextureMap;
        Type mType = Type::Color;
        SurfaceType mSurfaceType = SurfaceType::Opaque;
        MinTextureFilter mTextureMinFilter = MinTextureFilter::Default;
        MagTextureFilter mTextureMagFilter = MagTextureFilter::Default;
        TextureWrapping mTextureWrapX = TextureWrapping::Clamp;
        TextureWrapping mTextureWrapY = TextureWrapping::Clamp;
        base::bitflag<Flags> mFlags;
        std::unordered_map<std::string, Uniform> mUniforms;
        std::vector<std::unique_ptr<TextureMap>> mTextureMaps;
    private:
        struct ValueCache {
            size_t shader_hash = 0;
        };
        std::optional<ValueCache> mCache;
    };

    using ColorClass = MaterialClass;
    using GradientClass = MaterialClass;
    using SpriteClass = MaterialClass;
    using TextureMap2DClass = MaterialClass;
    using CustomMaterialClass = MaterialClass;

        // These functions are intended to use when you just need to create
    // a material quickly on the stack to draw something immediately and
    // don't need keep a material class around long term.
    //
    //             !! NO CLASS IDS ARE CREATED !!

    // Create gradient material based on 4 colors
    MaterialClass CreateMaterialClassFromColor(const Color4f& top_left,
                                               const Color4f& top_right,
                                               const Color4f& bottom_left,
                                               const Color4f& bottom_right);

    // Create material based on a simple color only.
    MaterialClass CreateMaterialClassFromColor(const Color4f& color);
    MaterialClass CreateMaterialClassFromColor(const Color color, float alpha);
    // Create a material based on a single image file.
    MaterialClass CreateMaterialClassFromSprite(const std::string& uri);
    // Create a material based on a single image file.
    MaterialClass CreateMaterialClassFromImage(const std::string& uri,
        MaterialClass::SurfaceType surface = MaterialClass::SurfaceType::Opaque);
    // Create a sprite from multiple images.
    MaterialClass CreateMaterialClassFromImages(const std::initializer_list<std::string>& uris);
    // Create a sprite from multiple images.
    MaterialClass CreateMaterialClassFromImages(const std::vector<std::string>& uris);
    // Create a sprite from a texture atlas where all the sprite frames
    // are packed inside the single texture.
    MaterialClass CreateMaterialClassFromSpriteAtlas(const std::string& texture, const std::vector<FRect>& frames);
    // Create a material class from a text buffer.
    MaterialClass CreateMaterialClassFromText(const TextBuffer& text);
    MaterialClass CreateMaterialClassFromText(TextBuffer&& text);

} // namespace
