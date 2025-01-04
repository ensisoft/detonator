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

#include <string>

#include "base/utility.h"
#include "data/fwd.h"
#include "graphics/types.h"
#include "graphics/texture_source.h"

namespace gfx
{
    class Device;
    class Texture;
    class TextureSource;

    // Interface for binding texture map(s) to texture sampler(s) in the material shader.
    class TextureMap
    {
    public:
        // Type of the texture map.
        enum class Type {
            // Texture2D is a static texture that always maps a single texture
            // to a single texture sampler.
            Texture2D,
            // Sprite texture map cycles over a series of textures over time
            // and chooses 2 textures closest to the current point in time
            // based on the sprite's fps setting.
            Sprite
        };
        // The current state to be used when binding textures to samplers.
        struct BindingState {
            bool dynamic_content = false;
            double current_time  = 0.0f;
            std::string group_tag;
        };
        // The result of binding textures.
        struct BoundState {
            // Which texture objects are currently being used. Can be 1 or 2.
            Texture* textures[2] = {nullptr, nullptr};
            // The texture rects for the textures.
            FRect rects[2];
            // If multiple textures are used when cycling through a series of
            // textures (i.e. a sprite) the blend coefficient defines the
            // current weight between the textures[0] and textures[1] based
            // on the current material time. This can be used to blend the
            // two closes frames together in order to create smoother animation.
            float blend_coefficient = 0.0;
            // The expected names of the texture samplers in the shader
            // as configured in the texture map.
            std::string sampler_names[2];
            // The expected names of the texture rect uniforms in the shader
            // as configured in the texture map.
            std::string rect_names[2];
        };
        struct SpriteSheet {
            unsigned cols = 0;
            unsigned rows = 0;
        };

        explicit TextureMap(std::string id = base::RandomString(10))
          : mName("Default")
          , mId(std::move(id))
        {}
        TextureMap(const TextureMap& other, bool copy);
        TextureMap(const TextureMap& other) : TextureMap(other, true) {}
        TextureMap(TextureMap&& other) noexcept;

        // Get the type of the texture map.
        inline Type GetType() const noexcept
        { return mType; }
        inline float GetSpriteFrameRate() const noexcept
        { return mFps; }
        inline bool IsSpriteLooping() const noexcept
        { return mLooping; }
        inline bool HasSpriteSheet() const noexcept
        { return mSpriteSheet.has_value(); }
        inline std::string GetId() const noexcept
        { return mId; }
        inline std::string GetName() const noexcept
        { return mName; }

        inline bool IsSpriteMap() const noexcept
        { return mType == Type::Sprite; }

        // Reset all texture objects. After this the texture map contains no textures.
        inline void ResetTextures() noexcept
        { mTextures.clear(); }
        inline void SetType(Type type) noexcept
        { mType = type; }
        inline void SetName(std::string name) noexcept
        { mName = std::move(name); }
        inline void SetNumTextures(size_t num)
        { mTextures.resize(num); }
        // Get the number of textures.
        inline size_t GetNumTextures() const noexcept
        { return mTextures.size(); }

        inline void SetSpriteFrameRate(float fps) noexcept
        { mFps = fps; }
        inline void SetSpriteLooping(bool looping) noexcept
        { mLooping = looping; }
        inline void SetSpriteSheet(const SpriteSheet& sheet) noexcept
        { mSpriteSheet = sheet; }
        inline void ResetSpriteSheet() noexcept
        { mSpriteSheet.reset(); }

        inline const SpriteSheet* GetSpriteSheet() const noexcept
        { return base::GetOpt(mSpriteSheet); }
        inline void SetSamplerName(std::string name, size_t index = 0) noexcept
        { mSamplerName[index] = std::move(name); }
        inline void SetRectUniformName(std::string name, size_t index = 0) noexcept
        { mRectUniformName[index] = std::move(name); }
        // Get the texture source object at the given index.
        inline const TextureSource* GetTextureSource(size_t index) const noexcept
        { return base::SafeIndex(mTextures, index).source.get(); }
        // Get the texture source object at the given index.
        inline TextureSource* GetTextureSource(size_t index) noexcept
        { return base::SafeIndex(mTextures, index).source.get(); }
        // Get the texture source rectangle at the given index.
        inline FRect GetTextureRect(size_t index) const noexcept
        { return base::SafeIndex(mTextures, index).rect; }
        // Set a new texture source rectangle for using a sub-rect of a texture.
        inline void SetTextureRect(size_t index, const FRect& rect) noexcept
        { base::SafeIndex(mTextures, index).rect = rect; }
        inline void ResetTextureSource(size_t index) noexcept
        { base::SafeIndex(mTextures, index).source.reset(); }
        // Set a new texture source object at the given index.
        inline void SetTextureSource(size_t index, std::unique_ptr<TextureSource> source) noexcept
        { base::SafeIndex(mTextures, index).source = std::move(source); }
        // Delete the texture source and the texture rect at the given index.
        inline void DeleteTexture(size_t index) noexcept
        { base::SafeErase(mTextures, index); }
        inline std::string GetSamplerName(size_t index) const noexcept
        { return mSamplerName[index]; }
        inline std::string GetRectUniformName(size_t index) const noexcept
        { return mRectUniformName[index]; }
        // Get the hash value based on the current state of the material map.
        std::size_t GetHash() const noexcept;
        // Select texture objects for sampling based on the current binding state.
        // If the texture objects don't yet exist on the device they're created.
        // The resulting BoundState expresses which textures should currently be
        // used and which are the sampler/uniform names that should be used when
        // binding the textures to the program's state before drawing.
        // Returns true if successful, otherwise false on error.
        bool BindTextures(const BindingState& state, Device& device, BoundState& result) const;
        // Serialize into JSON object.
        void IntoJson(data::Writer& data) const;
        // Load state from JSON object. Returns true if successful.
        bool FromJson(const data::Reader& data);
        bool FromLegacyJsonTexture2D(const data::Reader& data);

        // Find a specific texture source based on the texture source id.
        // Returns end index if no matching texture source was found.
        size_t FindTextureSourceIndexById(const std::string& id) const;
        // Find a texture source based on its name. Note that the names are not
        // necessarily unique. In such case it's unspecified which texture source
        // object is returned. Returns end index if no matching texture source was found.
        size_t FindTextureSourceIndexByName(const std::string& name) const;

        // down cast helpers.
        TextureMap* AsSpriteMap();
        TextureMap* AsTextureMap2D();
        const TextureMap* AsSpriteMap() const;
        const TextureMap* AsTextureMap2D() const;

        std::unique_ptr<TextureMap> Copy() const
        { return std::make_unique<TextureMap>(*this, true); }
        std::unique_ptr<TextureMap> Clone() const
        { return std::make_unique<TextureMap>(*this, false); }

        unsigned GetSpriteFrameCount() const;

        float GetSpriteCycleDuration() const;
        void SetSpriteFrameRateFromDuration(float duration);

        TextureMap& operator=(const TextureMap& other);
    private:
        std::string mName;
        std::string mId;

        Type mType = Type::Texture2D;
        float mFps = 0.0f;
        struct MyTexture {
            FRect rect = FRect(0.0f, 0.0f, 1.0f, 1.0f);
            std::unique_ptr<TextureSource> source;
        };
        std::vector<MyTexture> mTextures;
        std::string mSamplerName[2];
        std::string mRectUniformName[2];
        std::optional<SpriteSheet> mSpriteSheet;
        bool mLooping = true;
    };

    using TextureMap2D = TextureMap;
    using SpriteMap = TextureMap;
} // namespace