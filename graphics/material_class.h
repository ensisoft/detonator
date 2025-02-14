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

#include "base/utility.h"
#include "graphics/color4f.h"
#include "graphics/texture.h"
#include "graphics/types.h"
#include "graphics/texture_map.h"
#include "graphics/shader_source.h"

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
            EnableBloom
        };

        // Action to take on per particle random value. This can
        // be used when applying the material onto particle
        // system and to add some variation to the rendered output
        // in order to make the result visually more appealing.
        enum class ParticleEffect : int {
            None   =  0,
            Rotate =  1
        };

        enum class ParticleRotation : int {
            None,
            BaseRotation,
            RandomRotation,
            ParticleDirection,
            ParticleDirectionAndBase
        };

        enum class GradientType : int {
            Bilinear, Radial, Conical
        };

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
            RenderPass renderpass = RenderPass::ColorPass;

            // see material flags
            std::uint32_t flags = 0;
        };

        explicit MaterialClass(Type type, std::string id = base::RandomString(10));
        MaterialClass(const MaterialClass& other, bool copy=true);
        MaterialClass(MaterialClass&& other) noexcept;
       ~MaterialClass();

        inline bool IsBuiltIn() const noexcept
        { return mType != Type::Custom; }

        // Set the surface type of the material.
        inline void SetSurfaceType(SurfaceType surface) noexcept
        { mSurfaceType = surface; }

        // Unsafe, but helpful for migration.
        inline void SetType(Type type) noexcept
        { mType = type; }

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
        inline void SetParticleRotation(ParticleRotation rotation) noexcept
        { SetUniform("kParticleRotation", static_cast<int>(rotation)); }
        inline void SetParticleStartColor(const Color4f& color) noexcept
        { SetUniform("kParticleStartColor", color); }
        inline void SetParticleEndColor(const Color4f& color) noexcept
        { SetUniform("kParticleEndColor", color); }
        inline void SetParticleMidColor(const Color4f& color) noexcept
        { SetUniform("kParticleMidColor", color); }
        inline void SetParticleBaseRotation(float rotation_angle) noexcept
        { SetUniform("kParticleBaseRotation", rotation_angle); }

        inline void SetAmbientColor(const Color4f& color) noexcept
        { SetUniform("kAmbientColor", color); }
        inline void SetDiffuseColor(const Color4f& color) noexcept
        { SetUniform("kDiffuseColor", color); }
        inline void SetSpecularColor(const Color4f& color) noexcept
        { SetUniform("kSpecularColor", color); }
        inline void SetSpecularExponent(float exponent) noexcept
        { SetUniform("kSpecularExponent", exponent); }

        inline void SetGradientType(GradientType gradient) noexcept
        { SetUniform("kGradientType", static_cast<int>(gradient)); }

        inline void SetParticleEffect(ParticleEffect action) noexcept
        { action == ParticleEffect::None ? DeleteUniform("kParticleEffect")
                                         : SetUniform("kParticleEffect", static_cast<int>(action)); }
        inline void SetAlphaCutoff(float cutoff) noexcept
        { SetUniform("kAlphaCutoff", cutoff); }
        inline void SetGradientWeight(glm::vec2 weight) noexcept
        { SetUniform("kGradientWeight", weight); }
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

        inline GradientType GetGradientType() const noexcept
        { return static_cast<GradientType>(GetUniformValue("kGradientType", static_cast<int>(GradientType::Bilinear))); }

        inline Color4f GetParticleStartColor() const noexcept
        { return GetUniformValue<Color4f>("kParticleStartColor", Color::White); }
        inline Color4f GetParticleEndColor() const noexcept
        { return GetUniformValue<Color4f>("kParticleEndColor", Color::White); }
        inline Color4f GetParticleMidColor() const noexcept
        { return GetUniformValue<Color4f>("kParticleMidColor", Color::White); }

        inline ParticleRotation GetParticleRotation() const noexcept
        { return static_cast<ParticleRotation>(GetUniformValue("kParticleRotation", static_cast<int>(ParticleRotation::None))); }
        inline float GetParticleBaseRotation() const noexcept
        { return GetUniformValue<float>("kParticleBaseRotation", 0.0f); }

        inline float GetAlphaCutoff() const noexcept
        { return GetUniformValue<float>("kAlphaCutoff", -1.0f); }
        inline Color4f GetColor(ColorIndex index) const noexcept
        { return GetUniformValue<Color4f>(GetColorUniformName(index), Color::White); }
        inline Color4f GetBaseColor() const noexcept
        { return GetColor(ColorIndex::BaseColor); }
        inline glm::vec2 GetGradientWeight() const noexcept
        { return GetUniformValue<glm::vec2>("kGradientWeight", {0.5f, 0.5f}); }
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
        inline ParticleEffect GetParticleEffect() const noexcept
        { return static_cast<ParticleEffect>(GetUniformValue("kParticleEffect", 0)); }

        inline Color4f GetAmbientColor() const noexcept
        { return GetUniformValue<Color4f>("kAmbientColor", gfx::Color::Gray); }
        inline Color4f GetDiffuseColor() const noexcept
        { return GetUniformValue<Color4f>("kDiffuseColor", gfx::Color::Gray); }
        inline Color4f GetSpecularColor() const noexcept
        { return GetUniformValue<Color4f>("kSpecularColor", gfx::Color::White); }
        inline float GetSpecularExponent() const noexcept
        { return GetUniformValue<float>("kSpecularExponent", 4.0f); }


        inline MinTextureFilter GetTextureMinFilter() const noexcept
        { return mTextureMinFilter; }
        inline MagTextureFilter GetTextureMagFilter() const noexcept
        { return mTextureMagFilter; }
        inline TextureWrapping  GetTextureWrapX() const noexcept
        { return mTextureWrapX; }
        inline TextureWrapping GetTextureWrapY() const noexcept
        { return mTextureWrapY; }

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

        static std::string GetColorUniformName(ColorIndex index);
        static std::unique_ptr<MaterialClass> ClassFromJson(const data::Reader& data);

        MaterialClass& operator=(const MaterialClass& other);
    private:
        template<typename T>
        static bool SetUniform(const char* name, const UniformMap* uniforms, const T& backup, ProgramState& program);

        static bool SetUniform(const char* name, const UniformMap* uniforms, unsigned backup, ProgramState& program);

        template<typename T>
        bool ReadLegacyValue(const char* name, const char* uniform, const data::Reader& reader);

    private:
        TextureMap* SelectTextureMap(const State& state) const noexcept;
        ShaderSource GetShaderSource(const State& state, const Device& device) const;

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


} // namespace
