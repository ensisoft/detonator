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
#include <memory>
#include <vector>
#include <optional>
#include <cstddef>

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
            bool blend_frames = false;
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

        // Sprite sheet defines a sprite animation (a sprite cycle) where
        // each frame has the same size and the frames are arranged into
        // columns and rows. This is an alternative to having multiple
        // mages (textures where each animation frame is a separate image.
        // Note that a sprite sheet can contain multiple cycles so this
        // cols/rows mapping pertains always to the area inside the texture
        // source rectangle.
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
        Type GetType() const noexcept
        { return mType; }

        // Get the sprite FPS setting.
        float GetSpriteFrameRate() const noexcept
        { return mFps; }

        // Returns true if the sprite map is set to loop.
        bool IsSpriteLooping() const noexcept
        { return mLooping; }

        // Returns true if the texture map has a sprite sheet setting.
        bool HasSpriteSheet() const noexcept
        { return mSpriteSheet.has_value(); }

        // Returns true if the texture map is a sprite map.
        bool IsSpriteMap() const noexcept
        { return mType == Type::Sprite; }

        // Get the texture map ID.
        auto GetId() const noexcept
        { return mId; }

        // Get the texture map human-readable name.
        auto GetName() const noexcept
        { return mName; }

        // Reset and clear all texture settings.
        // After this the texture map contains no textures.
        void ResetTextures() noexcept
        { mTextures.clear(); }

        // Set the texture map type.
        void SetType(Type type) noexcept
        { mType = type; }

        // Set the texture map human-readable name.
        void SetName(std::string name) noexcept
        { mName = std::move(name); }

        // Set the number of textures. This allocates space in the
        // internal texture mapping array with the expectation that
        // some texture mapping is assigned to that index later on.
        void SetNumTextures(size_t num)
        { mTextures.resize(num); }

        // Get the number of allocated texture mappings.
        size_t GetNumTextures() const noexcept
        { return mTextures.size(); }

        // Set the sprite FPS that controls how fast (or slow)
        // the frames of the sprite cycle play out.
        void SetSpriteFrameRate(float fps) noexcept
        { mFps = fps; }

        // Set a flag controlling sprite cycle looping. When set to
        // true the sprite cycle loops forever and interpolates between
        // the last and the first frame when reaching the end.
        // When set to false the sprite cycle never loops and stops
        // at the end.
        void SetSpriteLooping(bool looping) noexcept
        { mLooping = looping; }

        // Set the sprite sheet setting.
        void SetSpriteSheet(const SpriteSheet& sheet) noexcept
        { mSpriteSheet = sheet; }

        // Reset and clear the current sprite sheet setting to nothing.
        void ResetSpriteSheet() noexcept
        { mSpriteSheet.reset(); }

        // Get the current sprite sheet setting if any. If there's no
        // sprite sheet setting returns a nullptr.
        const SpriteSheet* GetSpriteSheet() const noexcept
        { return base::GetOpt(mSpriteSheet); }

        // Set the expected texture sampler name at the designed sampler index.
        // The texture map has 2 designed sampler slots in order to support
        // interpolating between sprite animation frames. The names will be
        // used to identify the fragment shader texture samplers which will
        // be used to sample the textures provided by the texture map.
        void SetSamplerName(std::string name, size_t index = 0) noexcept
        { mSamplerName[index] = std::move(name); }

        // todo:
        void SetRectUniformName(std::string name, size_t index = 0) noexcept
        { mRectUniformName[index] = std::move(name); }

        // Get the texture source object at the given index which must be valid.
        // If the texture mapping slot has not texture source assigned to it
        // nullptr will be returned.
        const TextureSource* GetTextureSource(size_t index) const noexcept
        { return base::SafeIndex(mTextures, index).source.get(); }

        // Get the texture source object at the given index which must valid.
        // If the texture mapping slot has not texture source assigned to it
        // nullptr will be returned.
        TextureSource* GetTextureSource(size_t index) noexcept
        { return base::SafeIndex(mTextures, index).source.get(); }

        // Get the texture source rectangle at the given index.
        FRect GetTextureRect(size_t index) const noexcept
        { return base::SafeIndex(mTextures, index).rect; }

        // Set a new texture source rectangle for using a sub-rect of a texture.
        void SetTextureRect(size_t index, const FRect& rect) noexcept
        { base::SafeIndex(mTextures, index).rect = rect; }

        void ResetTextureSource(size_t index) noexcept
        { base::SafeIndex(mTextures, index).source.reset(); }

        // Set a new texture source object at the given index.
        void SetTextureSource(size_t index, std::unique_ptr<TextureSource> source) noexcept
        { base::SafeIndex(mTextures, index).source = std::move(source); }

        // Delete the texture mapping slot at the given index which must valid.
        void DeleteTexture(size_t index) noexcept
        { base::SafeErase(mTextures, index); }

        std::string GetSamplerName(size_t index) const noexcept
        { return mSamplerName[index]; }

        std::string GetRectUniformName(size_t index) const noexcept
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

        // Swap texture sources at the given indices one and two.
        // the indices must be valid indices.
        void SwapSources(size_t one, size_t two);

        // Move from texture mapping from one texture mapping slot
        // to another slot. Both slots (indices) must be valid.
        void ShuffleSource(size_t from_index, size_t to_index);

        // Get the number of frames in a sprite cycle irrespective whether
        // the sprite cycle uses separate images as frames or a sprite sheet
        // with row/col based grid cells as frames.
        // If the texture map is a not a sprite then returns zero. (no frames)
        unsigned GetSpriteFrameCount() const;

        // Compute the duration of the sprite cycle in seconds based on the
        // number of frames and the playback speed (FPS) setting.
        // If the texture map is not a sprite then duration will be 0.0f.
        float GetSpriteCycleDuration() const;

        // Set the new sprite FPS based on the target duration that needs to
        // be a positive value greater than 0.0f. If the texture map is not
        // a sprite then nothing is done.
        void SetSpriteFrameRateFromDuration(float duration);

        // Create an exact bitwise copy of this texture map including the ID.
        std::unique_ptr<TextureMap> Copy() const
        { return std::make_unique<TextureMap>(*this, true); }

        // Create a clone of this texture map, i.e with same data and settings
        // but with a different ID.
        std::unique_ptr<TextureMap> Clone() const
        { return std::make_unique<TextureMap>(*this, false); }

        TextureMap& operator=(const TextureMap& other);
    private:
        struct TextureMapping {
            FRect rect = FRect(0.0f, 0.0f, 1.0f, 1.0f);
            std::unique_ptr<TextureSource> source;
        };
        Type mType = Type::Texture2D;
        std::string mName;
        std::string mId;
        std::vector<TextureMapping> mTextures;
        std::string mSamplerName[2];
        std::string mRectUniformName[2];
        std::optional<SpriteSheet> mSpriteSheet;
        bool mLooping = true;
        float mFps = 0.0f;
    };

    using TextureMap2D = TextureMap;
    using SpriteMap = TextureMap;
} // namespace