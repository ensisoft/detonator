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

        // The four supported colors are all identified by
        // a color index that maps to the 4 quadrants of the
        // texture coordinate space.
        enum class ColorIndex {
            BaseColor,
            TopLeft,
            TopRight,
            BottomLeft,
            BottomRight
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
            PremultipliedAlpha
        };

        // Action to take on per particle random value. This can
        // be used when applying the material onto particle
        // system and to add some variation to the rendered output
        // in order to make the result visually more appealing.
        enum class ParticleAction : int {
            None   =  0,
            Custom =  1, // try to set this value so that future expansion
                         // works without breaking stuff. 255 or so could
                         // also work but magic_enum breaks on a value
                         // discontinuity
            Rotate =  2,
        };


        // Material/Shader uniform/params
        using Uniform = std::variant<float, int,
                std::string,
                gfx::Color4f,
                glm::vec2, glm::vec3, glm::vec4>;
        using UniformMap = std::unordered_map<std::string, Uniform>;

        // The current material state to apply when applying the
        // material's *dynamic* state onto the program object.
        // The dynamic state is state that can change over time and
        // between material instances sharing the same underlying
        // material type. For example if there exists multiple material
        // instances of a single material type called they might still
        // have a different state.
        struct State {
            // First render flag to indicate whether this is the first time
            // this material instance renders.
            bool first_render = false;
            // true if running in an "editing mode", which means that even
            // content marked static might have changed and should be checked
            // in case it has been modified and should be re-uploaded.
            bool editing_mode = false;
            // what primitives will be drawn with this material.
            DrawPrimitive draw_primitive = DrawPrimitive::Triangles;
            // The current material instance time.
            double material_time = 0.0f;
            // The uniform parameters set on the material instance (if any).
            // The instance uniforms will take precedence over the uniforms
            // set in the class whenever they're set.
            const UniformMap* uniforms = nullptr;
            // Current render pass the material is used in
            RenderPass renderpass = RenderPass::ColorPass;
        };

        explicit MaterialClass(Type type, std::string id = base::RandomString(10));
        MaterialClass(const MaterialClass& other, bool copy=true);

        // Set the surface type of the material.
        inline void SetSurfaceType(SurfaceType surface) noexcept
        { mSurfaceType = surface; }
        inline void SetParticleAction(ParticleAction action) noexcept
        { mParticleAction = action; }
        inline void SetActiveTextureMap(std::string id) noexcept
        { mActiveTextureMap = id; }
        // Set the human-readable material class name.
        inline void SetName(std::string name) noexcept
        { mName = std::move(name); }
        inline void SetStatic(bool on_off) noexcept
        { mFlags.set(Flags::Static, on_off); }
        inline void SetBlendFrames(bool on_off) noexcept
        { mFlags.set(Flags::BlendFrames, on_off); }
        inline void SetTextureMinFilter(MinTextureFilter filter) noexcept
        { mTextureMinFilter = filter; }
        inline void SetTextureMagFilter(MagTextureFilter filter) noexcept
        { mTextureMagFilter = filter; }
        inline void SetTextureWrapX(TextureWrapping wrap) noexcept
        { mTextureWrapX = wrap; }
        inline void SetTextureWrapY(TextureWrapping wrap) noexcept
        { mTextureWrapY = wrap; }
        // Set a material flag to on or off.
        inline void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }
        inline void SetShaderUri(std::string uri) noexcept
        { mShaderUri = std::move(uri); }
        inline void SetShaderSrc(std::string src) noexcept
        { mShaderSrc = std::move(src); }
        inline bool HasShaderUri() const noexcept
        { return !mShaderUri.empty(); }
        inline bool HasShaderSrc() const noexcept
        { return !mShaderSrc.empty(); }
        inline void ClearShaderSrc() noexcept
        { mShaderSrc.clear(); }
        inline void ClearShaderUri() noexcept
        { mShaderUri.clear(); }
        inline std::string GetActiveTextureMap() const noexcept
        { return mActiveTextureMap; }
        // Get the material class id.
        inline std::string GetId() const noexcept
        { return mClassId; }
        // Get the human readable material class name.
        inline std::string GetName() const noexcept
        { return mName; }
        inline std::string GetShaderUri() const noexcept
        { return mShaderUri; }
        inline std::string GetShaderSrc() const noexcept
        { return mShaderSrc; }
        // Get the actual implementation type of the material.
        inline Type GetType() const noexcept
        { return mType; }
        // Get the surface type of the material.
        inline SurfaceType GetSurfaceType() const noexcept
        { return mSurfaceType; }
        // Test a material flag. Returns true if the flag is set, otherwise false.
        inline bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        inline bool PremultipliedAlpha() const noexcept
        { return TestFlag(Flags::PremultipliedAlpha); }
        inline bool IsStatic() const noexcept
        { return TestFlag(Flags::Static); }
        inline bool BlendFrames() const noexcept
        { return TestFlag(Flags::BlendFrames); }
        inline bool HasCustomShader() const noexcept
        { return !mShaderSrc.empty() || !mShaderUri.empty(); }

        // Material uniform API for setting/getting "Known" uniforms.
        inline void SetAlphaCutoff(float cutoff) noexcept
        { SetUniform("kAlphaCutoff", cutoff); }
        inline void SetColorWeight(glm::vec2 weight) noexcept
        { SetUniform("kWeight", weight); }
        inline void SetBaseColor(const Color4f& color) noexcept
        { SetColor(color, ColorIndex::BaseColor); }
        inline void SetColor(const Color4f& color, ColorIndex index) noexcept
        { SetUniform(GetColorUniformName(index), color); }
        inline void SetTextureScaleX(float scale) noexcept
        { GetUniformValue<glm::vec2>("kTextureScale", {1.0f, 1.0f}).x = scale; }
        inline void SetTextureScaleY(float scale) noexcept
        { GetUniformValue<glm::vec2>("kTextureScale", {1.0f, 1.0f}).y = scale; }
        inline void SetTextureScale(const glm::vec2& scale) noexcept
        { GetUniformValue<glm::vec2>("kTextureScale", {1.0f, 1.0f}) = scale; }
        inline void SetTextureVelocityX(float x) noexcept
        { GetUniformValue<glm::vec3>("kTextureVelocity", {0.0f, 0.0f, 0.0f}).x = x; }
        inline void SetTextureVelocityY(float y) noexcept
        { GetUniformValue<glm::vec3>("kTextureVelocity", {0.0f, 0.0f, 0.0f}).y = y; }
        inline void SetTextureVelocityZ(float angle_radians) noexcept
        { GetUniformValue<glm::vec3>("kTextureVelocity", {0.0f, 0.0f, 0.0f}).z = angle_radians; }
        inline void SetTextureRotation(float angle_radians) noexcept
        { GetUniformValue<float>("kTextureRotation", 0.0f) = angle_radians; }
        inline void SetTextureVelocity(const glm::vec2& linear, float angular) noexcept
        { GetUniformValue<glm::vec3>("kTextureVelocity", {0.0f, 0.0f, 0.0f}) = glm::vec3(linear, angular); }
        inline void SetTileSize(const glm::vec2& size) noexcept
        { GetUniformValue<glm::vec2>("kTileSize", {0.0f, 0.0f}) = size; }
        inline void SetTileOffset(const glm::vec2& offset) noexcept
        { GetUniformValue<glm::vec2>("kTileOffset", {0.0, 0.0f}) = offset; }
        inline void SetTilePadding(const glm::vec2& padding) noexcept
        { GetUniformValue<glm::vec2>("kTilePadding", {0.0f, 0.0f}) = padding; }

        inline float GetAlphaCutoff() const noexcept
        { return GetUniformValue<float>("kAlphaCutoff", -1.0f); }
        inline Color4f GetColor(ColorIndex index) const noexcept
        { return GetUniformValue<Color4f>(GetColorUniformName(index), Color::White); }
        inline Color4f GetBaseColor() const noexcept
        { return GetColor(ColorIndex::BaseColor); }
        inline glm::vec2 GetColorWeight() const noexcept
        { return GetUniformValue<glm::vec2>("kWeight", {0.5f, 0.5f}); }
        inline float GetTextureScaleX() const noexcept
        { return GetTextureScale().x; }
        inline float GetTextureScaleY() const noexcept
        { return GetTextureScale().y; }
        inline glm::vec2 GetTextureScale() const noexcept
        { return GetUniformValue<glm::vec2>("kTextureScale", {1.0f, 1.0f}); }
        inline float GetTextureVelocityX() const noexcept
        { return GetTextureVelocity().x; }
        inline float GetTextureVelocityY() const noexcept
        { return GetTextureVelocity().y; }
        inline float GetTextureVelocityZ() const noexcept
        { return GetTextureVelocity().z; }
        inline glm::vec3 GetTextureVelocity() const noexcept
        { return GetUniformValue<glm::vec3>("kTextureVelocity", {0.0f, 0.0f, 0.0f}); }
        inline float GetTextureRotation() const noexcept
        { return GetUniformValue<float>("kTextureRotation", 0.0f); }
        inline glm::vec2 GetTileSize() const noexcept
        { return GetUniformValue<glm::vec2>("kTileSize", {0.0f, 0.0f}); }
        inline glm::vec2 GetTileOffset() const noexcept
        { return GetUniformValue<glm::vec2>("kTileOffset", {0.0f, 0.0f}); }
        inline glm::vec2 GetTilePadding() const noexcept
        { return GetUniformValue<glm::vec2>("kTilePadding", {0.0f, 0.0f}); }

        inline MinTextureFilter GetTextureMinFilter() const noexcept
        { return mTextureMinFilter; }
        inline MagTextureFilter GetTextureMagFilter() const noexcept
        { return mTextureMagFilter; }
        inline TextureWrapping  GetTextureWrapX() const noexcept
        { return mTextureWrapX; }
        inline TextureWrapping GetTextureWrapY() const noexcept
        { return mTextureWrapY; }
        inline ParticleAction GetParticleAction() const noexcept
        { return mParticleAction; }

        inline void SetUniform(const std::string& name, const Uniform& value) noexcept
        { mUniforms[name] = value; }
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
        inline bool CheckUniformType(const std::string& name) const noexcept
        {
            if (const auto* ptr = base::SafeFind(mUniforms, name)) {
                return std::holds_alternative<T>(*ptr);
            }
            return false;
        }

        inline void DeleteUniforms() noexcept
        { mUniforms.clear(); }
        inline void DeleteUniform(const std::string& name) noexcept
        { mUniforms.erase(name); }
        inline bool HasUniform(const std::string& name) const noexcept
        { return base::SafeFind(mUniforms, name) != nullptr; }
        template<typename T>
        bool HasUniform(const std::string& name) const noexcept
        {
            if (const auto* ptr = base::SafeFind(mUniforms, name))
                return std::get_if<T>(ptr) != nullptr;
            return false;
        }
        UniformMap GetUniforms() const
        { return mUniforms; }

        // Get the number of material maps in the material class.
        inline unsigned GetNumTextureMaps() const  noexcept
        { return mTextureMaps.size(); }
        // Get a texture map at the given index.
        const TextureMap* GetTextureMap(unsigned index) const noexcept
        { return base::SafeIndex(mTextureMaps, index).get(); }
        // Get a texture map at the given index.
        inline TextureMap* GetTextureMap(unsigned index) noexcept
        { return base::SafeIndex(mTextureMaps, index).get(); }
        // Delete the texture map at the given index.
        inline void DeleteTextureMap(unsigned index) noexcept
        { base::SafeErase(mTextureMaps, index); }
        inline void SetNumTextureMaps(unsigned count)
        { mTextureMaps.resize(count); }
        inline void SetTextureMap(unsigned index, std::unique_ptr<TextureMap> map) noexcept
        { base::SafeIndex(mTextureMaps, index) = std::move(map); }
        inline void SetTextureMap(unsigned index, TextureMap map) noexcept
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
        bool FromJson(const data::Reader& data);
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

        TextureMap* FindTextureMapByName(const std::string& name);
        TextureMap* FindTextureMapById(const std::string& id);
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

        inline void SetTextureRect(size_t map, size_t texture, const gfx::FRect& rect) noexcept
        { base::SafeIndex(mTextureMaps, map)->SetTextureRect(texture, rect); }
        inline void SetTextureRect(const gfx::FRect& rect)noexcept
        { SetTextureRect(0, 0, rect); }
        inline void SetTextureSource(size_t map, size_t texture, std::unique_ptr<TextureSource> source) noexcept
        { base::SafeIndex(mTextureMaps, map)->SetTextureSource(texture, std::move(source)); }
        inline void SetTextureSource(std::unique_ptr<TextureSource> source) noexcept
        { SetTextureSource(0, 0, std::move(source)); }

        static ShaderSource CreateShaderStub(Type type);

        static std::string GetColorUniformName(ColorIndex index);

        static std::unique_ptr<MaterialClass> ClassFromJson(const data::Reader& data);

        MaterialClass& operator=(const MaterialClass& other);
    private:
        template<typename T>
        static bool SetUniform(const char* name, const UniformMap* uniforms, const T& backup, ProgramState& program);

        template<typename T>
        bool ReadLegacyValue(const char* name, const char* uniform, const data::Reader& reader);

    private:
        TextureMap* SelectTextureMap(const State& state) const noexcept;
        ShaderSource GetShaderSource(const State& state, const Device& device) const;
        ShaderSource GetColorShaderSource(const State& state, const Device& device) const;
        ShaderSource GetGradientShaderSource(const State& state, const Device& device) const;
        ShaderSource GetSpriteShaderSource(const State& state, const Device& device) const;
        ShaderSource GetTextureShaderSource(const State& state, const Device& device) const;
        ShaderSource GetTilemapShaderSource(const State& state, const Device& device) const;
        static ShaderSource GetColorShaderSource();
        static ShaderSource GetGradientShaderSource();
        static ShaderSource GetSpriteShaderSource();
        static ShaderSource GetTextureShaderSource();
        static ShaderSource GetTilemapShaderSource();

        bool ApplySpriteDynamicState(const State& state, Device& device, ProgramState& program) const noexcept;
        bool ApplyCustomDynamicState(const State& state, Device& device, ProgramState& program) const noexcept;
        bool ApplyTextureDynamicState(const State& state, Device& device, ProgramState& program) const noexcept;
        bool ApplyTilemapDynamicState(const State& state, Device& device, ProgramState& program) const noexcept;

    private:
        std::string mClassId;
        std::string mName;
        std::string mShaderUri;
        std::string mShaderSrc;
        std::string mActiveTextureMap;
        Type mType = Type::Color;
        ParticleAction mParticleAction = ParticleAction::None;
        SurfaceType mSurfaceType = SurfaceType::Opaque;
        MinTextureFilter mTextureMinFilter = MinTextureFilter::Default;
        MagTextureFilter mTextureMagFilter = MagTextureFilter::Default;
        TextureWrapping mTextureWrapX = TextureWrapping::Clamp;
        TextureWrapping mTextureWrapY = TextureWrapping::Clamp;
        base::bitflag<Flags> mFlags;
        std::unordered_map<std::string, Uniform> mUniforms;
        std::vector<std::unique_ptr<TextureMap>> mTextureMaps;
    };

    using ColorClass = MaterialClass;
    using GradientClass = MaterialClass;
    using SpriteClass = MaterialClass;
    using TextureMap2DClass = MaterialClass;
    using CustomMaterialClass = MaterialClass;

    // Material instance. Each instance can contain and alter the
    // instance specific state even between instances that refer
    // to the same underlying material type.
    class Material
    {
    public:
        using Uniform    = MaterialClass::Uniform;
        using UniformMap = MaterialClass::UniformMap;
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
        MaterialInstance(const std::shared_ptr<const MaterialClass>& klass, double time = 0.0)
            : mClass(klass)
            , mRuntime(time)
        {}
        MaterialInstance(const MaterialClass& klass, double time = 0.0)
           : mClass(klass.Copy())
           , mRuntime(time)
        {}

        // Apply the material properties to the given program object and set the rasterizer state.
        virtual bool ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& raster) const override;
        virtual void ApplyStaticState(const Environment& env, Device& device, ProgramState& program) const override;
        virtual ShaderSource GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;

        virtual std::string GetClassId() const override
        { return mClass->GetId(); }
        virtual void Update(float dt) override
        { mRuntime += dt; }
        virtual void SetRuntime(double runtime) override
        { mRuntime = runtime; }
        virtual void SetUniform(const std::string& name, const Uniform& value) override
        { mUniforms[name] = value; }
        virtual void SetUniform(const std::string& name, Uniform&& value) override
        { mUniforms[name] = std::move(value); }
        virtual void ResetUniforms()  override
        { mUniforms.clear(); }
        virtual void SetUniforms(const UniformMap& uniforms) override
        { mUniforms = uniforms; }
        virtual double GetRuntime() const override
        { return mRuntime; }
        virtual const MaterialClass* GetClass() const override
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


    // These functions are intended to use when you just need to create
    // a material quickly on the stack to draw something immediately and
    // don't need keep a material class around long term.
    //
    //             !! NO CLASS IDS ARE CREATED !!

    // Create gradient material based on 4 colors
    GradientClass CreateMaterialClassFromColor(const Color4f& top_left,
                                               const Color4f& top_right,
                                               const Color4f& bottom_left,
                                               const Color4f& bottom_right);

    // Create material based on a simple color only.
    ColorClass CreateMaterialClassFromColor(const Color4f& color);
    // Create a material based on a single image file.
    TextureMap2DClass CreateMaterialClassFromImage(const std::string& uri);
    // Create a sprite from multiple images.
    SpriteClass CreateMaterialClassFromImages(const std::initializer_list<std::string>& uris);
    // Create a sprite from multiple images.
    SpriteClass CreateMaterialClassFromImages(const std::vector<std::string>& uris);
    // Create a sprite from a texture atlas where all the sprite frames
    // are packed inside the single texture.
    SpriteClass CreateMaterialClassFromSpriteAtlas(const std::string& texture, const std::vector<FRect>& frames);
    // Create a material class from a text buffer.
    TextureMap2DClass CreateMaterialClassFromText(const TextBuffer& text);
    TextureMap2DClass CreateMaterialClassFromText(TextBuffer&& text);

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
