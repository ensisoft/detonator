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

namespace gfx
{
    class Device;
    class Shader;
    class ShaderPass;

    // Interface for acquiring texture data. Possible implementations
    // might load the data from a file or generate it on the fly.
    class TextureSource
    {
    public:
        // Enum to specify what is the underlying data source for the texture data.
        enum class Source {
            // Data comes from a file (such as a .png or a .jpg) in the filesystem.
            Filesystem,
            // Data comes as an in memory bitmap rasterized by the TextBuffer
            // based on the text/font content/parameters.
            TextBuffer,
            // Data comes from a bitmap buffer.
            BitmapBuffer,
            // Data comes from a bitmap generator algorithm.
            BitmapGenerator,
            // Data is already a texture on the device
            Texture
        };
        enum class ColorSpace {
            Linear, sRGB
        };

        enum class Effect {
            Blur,
            Edges,
        };

        virtual ~TextureSource() = default;
        // Get the color space of the texture source's texture content
        virtual ColorSpace GetColorSpace() const
        { return ColorSpace::Linear; }
        // Get the texture effect if any on the texture.
        virtual base::bitflag<Effect> GetEffects() const
        { return base::bitflag<Effect>(0); }
        // Get the type of the source of the texture data.
        virtual Source GetSourceType() const = 0;
        // Get texture source class/resource id.
        virtual std::string GetId() const = 0;
        // Get the human-readable / and settable name.
        virtual std::string GetName() const = 0;
        // Get the texture source hash value based on the properties
        // of the texture source object itself *and* its content.
        virtual std::size_t GetHash() const = 0;
        // Set the texture source human-readable name.
        virtual void SetName(const std::string& name) = 0;
        // Set a texture effect on/off on the texture.
        virtual void SetEffect(Effect effect, bool on_off) {}
        // Generate or load the data as a bitmap. If there's a content
        // error this function should return empty shared pointer.
        // The returned bitmap can be potentially immutably shared.
        virtual std::shared_ptr<IBitmap> GetData() const = 0;

        struct Environment {
            bool dynamic_content = false;
        };
        // Create a texture out of the texture source on the device.
        // Returns a texture object on success of nullptr on error.
        virtual Texture* Upload(const Environment& env, Device& device) const = 0;

        // Serialize into JSON object.
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load state from JSON object. Returns true if successful
        // otherwise false.
        virtual bool FromJson(const data::Reader& data) = 0;
        // Begin packing the texture source into the packer.
        virtual void BeginPacking(TexturePacker* packer) const {}
        // Finish packing the texture source into the packer.
        // Update the state with the details from the packer.
        virtual void FinishPacking(const TexturePacker* packer) {}

        // Create a similar clone of this texture source but with a new unique ID.
        inline std::unique_ptr<TextureSource> Clone()
        {
            return this->MakeCopy(base::RandomString(10));
        }
        // Create an exact bitwise copy of this texture source object.
        inline std::unique_ptr<TextureSource> Copy()
        {
            return this->MakeCopy(GetId());
        }
        inline bool TestEffect(Effect effect) const
        { return GetEffects().test(effect); }

    protected:
        virtual std::unique_ptr<TextureSource> MakeCopy(std::string copy_id) const = 0;
    private:
    };

    namespace detail {
        class TextureTextureSource : public TextureSource
        {
        public:
            TextureTextureSource()
              : mId(base::RandomString(10))
            {}
            //  Persistence only works with a known GPU texture ID.
            TextureTextureSource(std::string gpu_id, gfx::Texture* texture =  nullptr, std::string id = base::RandomString(10))
               : mId(std::move(id))
               , mGpuId(std::move(gpu_id))
               , mTexture(texture)
            {}
            virtual ColorSpace GetColorSpace() const override
            { return ColorSpace::Linear; }
            virtual Source GetSourceType() const override
            { return Source::Texture; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::string GetName() const override
            { return mName; }
            virtual std::size_t GetHash() const override
            {
                size_t hash = 0;
                hash = base::hash_combine(hash, mId);
                hash = base::hash_combine(hash, mName);
                hash = base::hash_combine(hash, mGpuId);
                return hash;
            }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual std::shared_ptr<IBitmap> GetData() const override
            { return nullptr; }
            virtual Texture* Upload(const Environment& env, Device& device) const override;
            virtual void IntoJson(data::Writer& data) const override;
            virtual bool FromJson(const data::Reader& data) override;

            inline void SetTexture(gfx::Texture* texture) noexcept
            { mTexture = texture; }
        protected:
            virtual std::unique_ptr<TextureSource> MakeCopy(std::string copy_id) const override
            {
                auto ret = std::make_unique<TextureTextureSource>(*this);
                ret->mId = std::move(copy_id);
                return ret;
            }
        private:
            std::string mId;
            std::string mName;
            std::string mGpuId;
            mutable gfx::Texture* mTexture = nullptr;
        };

        // Source texture data from an image file.
        class TextureFileSource : public TextureSource
        {
        public:
            enum class Flags {
                AllowPacking,
                AllowResizing,
                PremulAlpha
            };
            TextureFileSource()
              : mId(base::RandomString(10))
            {
                mFlags.set(Flags::AllowResizing, true);
                mFlags.set(Flags::AllowPacking, true);
            }
            TextureFileSource(std::string file, std::string id = base::RandomString(10))
              : mId(std::move(id))
              , mFile(std::move(file))
            {
                mFlags.set(Flags::AllowResizing, true);
                mFlags.set(Flags::AllowPacking, true);
            }
            virtual ColorSpace GetColorSpace() const override
            { return mColorSpace; }
            virtual Source GetSourceType() const override
            { return Source::Filesystem; }
            virtual base::bitflag<Effect> GetEffects() const override
            { return mEffects; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::string GetName() const override
            { return mName; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual void SetEffect(Effect effect, bool on_off) override
            { mEffects.set(effect, on_off); }

            virtual std::size_t GetHash() const override;

            virtual std::shared_ptr<IBitmap> GetData() const override;

            virtual Texture* Upload(const Environment& env, Device& device) const override;

            virtual void IntoJson(data::Writer& data) const override;
            virtual bool FromJson(const data::Reader& data) override;

            virtual void BeginPacking(TexturePacker* packer) const override
            {
                packer->PackTexture(this, mFile);
                packer->SetTextureFlag(this, TexturePacker::TextureFlags::AllowedToPack,
                                       TestFlag(Flags::AllowPacking));
                packer->SetTextureFlag(this, TexturePacker::TextureFlags::AllowedToResize,
                                       TestFlag(Flags::AllowResizing));
            }
            virtual void FinishPacking(const TexturePacker* packer) override
            {
                mFile = packer->GetPackedTextureId(this);
            }
            void SetFileName(const std::string& file)
            { mFile = file; }
            const std::string& GetFilename() const
            { return mFile; }
            bool TestFlag(Flags flag) const
            { return mFlags.test(flag); }
            void SetFlag(Flags flag, bool on_off)
            { mFlags.set(flag, on_off); }
            void SetColorSpace(ColorSpace space)
            { mColorSpace = space; }
        protected:
            virtual std::unique_ptr<TextureSource> MakeCopy(std::string id) const override
            {
                auto ret = std::make_unique<TextureFileSource>(*this);
                ret->mId = std::move(id);
                return ret;
            }
        private:
            std::string mId;
            std::string mFile;
            std::string mName;
            base::bitflag<Flags> mFlags;
            base::bitflag<Effect> mEffects;
            ColorSpace mColorSpace = ColorSpace::Linear; // default to linear now for compatibility
        private:
        };

        // Source texture data from a bitmap
        class TextureBitmapBufferSource : public TextureSource
        {
        public:
            TextureBitmapBufferSource()
              : mId(base::RandomString(10))
            {}

            TextureBitmapBufferSource(std::unique_ptr<IBitmap>&& bitmap, std::string id = base::RandomString(10))
              : mId(std::move(id))
              , mBitmap(std::move(bitmap))
            {}

            template<typename T>
            TextureBitmapBufferSource(const Bitmap<T>& bmp, std::string id = base::RandomString(10))
              : mId(std::move(id))
              , mBitmap(new Bitmap<T>(bmp))
            {}

            template<typename T>
            TextureBitmapBufferSource(Bitmap<T>&& bmp, std::string id = base::RandomString(10))
              : mId(std::move(id))
              , mBitmap(new Bitmap<T>(std::move(bmp)))
            {}

            TextureBitmapBufferSource(const TextureBitmapBufferSource& other)
              : mId(other.mId)
              , mName(other.mName)
              , mBitmap(other.mBitmap->Clone())
            {}

            virtual base::bitflag<Effect> GetEffects() const override
            { return mEffects; }
            virtual Source GetSourceType() const override
            { return Source::BitmapBuffer; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::size_t GetHash()  const override
            {
                size_t hash = mBitmap->GetHash();
                hash = base::hash_combine(hash, mBitmap->GetWidth());
                hash = base::hash_combine(hash, mBitmap->GetHeight());
                hash = base::hash_combine(hash, mId);
                hash = base::hash_combine(hash, mName);
                hash = base::hash_combine(hash, mEffects);
                return hash;
            }
            virtual std::string GetName() const override
            { return mName; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual std::shared_ptr<IBitmap> GetData() const override
            { return mBitmap; }
            virtual void SetEffect(Effect effect, bool on_off) override
            { mEffects.set(effect, on_off); }

            virtual Texture* Upload(const Environment& env, Device& device) const override;

            virtual void IntoJson(data::Writer& data) const override;
            virtual bool FromJson(const data::Reader& data) override;

            void SetBitmap(std::unique_ptr<IBitmap> bitmap)
            { mBitmap = std::move(bitmap); }
            template<typename T>
            void SetBitmap(const Bitmap<T>& bitmap)
            { mBitmap = std::make_unique<gfx::Bitmap<T>>(bitmap); }
            template<typename T>
            void SetBitmap(Bitmap<T>&& bitmap)
            { mBitmap = std::make_unique<gfx::Bitmap<T>>(std::move(bitmap)); }
            const IBitmap& GetBitmap() const
            { return *mBitmap; }

            template<typename Pixel>
            const Bitmap<Pixel>* GetBitmap() const
            {
                // todo: maybe use some kind of format type.
                const auto bytes = mBitmap->GetDepthBits() / 8;
                if (sizeof(Pixel) == bytes)
                    return static_cast<Bitmap<Pixel>*>(mBitmap.get());
                return nullptr;
            }
            template<typename Pixel> inline
            bool GetBitmap(const Bitmap<Pixel>** out) const
            {
                const auto* ret = GetBitmap<Pixel>();
                if (ret) {
                    *out = ret;
                    return true;
                }
                return false;
            }
        protected:
            virtual std::unique_ptr<TextureSource> MakeCopy(std::string id) const override
            {
                auto ret = std::make_unique<TextureBitmapBufferSource>(*this);
                ret->mId = id;
                return ret;
            }
        private:
            std::string mId;
            std::string mName;
            std::shared_ptr<IBitmap> mBitmap;
            base::bitflag<Effect> mEffects;
        };

        // Source texture data from a bitmap
        class TextureBitmapGeneratorSource : public TextureSource
        {
        public:
            TextureBitmapGeneratorSource()
              : mId(base::RandomString(10))
            {}

            TextureBitmapGeneratorSource(std::unique_ptr<IBitmapGenerator>&& generator, std::string id = base::RandomString(10))
              : mId(std::move(id))
              , mGenerator(std::move(generator))
            {}

            TextureBitmapGeneratorSource(const TextureBitmapGeneratorSource& other)
              : mId(other.mId)
              , mName(other.mName)
              , mGenerator(other.mGenerator->Clone())
            {}

            virtual base::bitflag<Effect> GetEffects() const override
            { return mEffects; }
            virtual Source GetSourceType() const override
            { return Source::BitmapGenerator; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::size_t GetHash() const override
            {
                auto hash = mGenerator->GetHash();
                hash = base::hash_combine(hash, mId);
                hash = base::hash_combine(hash, mName);
                hash = base::hash_combine(hash, mEffects);
                return hash;
            }
            virtual std::string GetName() const override
            { return mName; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual void SetEffect(Effect effect, bool on_off) override
            { mEffects.set(effect, on_off); }
            virtual std::shared_ptr<IBitmap> GetData() const override
            { return mGenerator->Generate(); }

            virtual Texture* Upload(const Environment& env, Device& device) const override;

            virtual void IntoJson(data::Writer& data) const override;
            virtual bool FromJson(const data::Reader& data) override;

            IBitmapGenerator& GetGenerator()
            { return *mGenerator; }
            const IBitmapGenerator& GetGenerator() const
            { return *mGenerator; }

            void SetGenerator(std::unique_ptr<IBitmapGenerator> generator)
            { mGenerator = std::move(generator); }

            template<typename T>
            void SetGenerator(T&& generator)
            {
                // generator is a "universal reference"
                // Meyer's Item. 24
                mGenerator = std::make_unique<std::remove_reference_t<T>>(std::forward<T>(generator));
            }
        protected:
            virtual std::unique_ptr<TextureSource> MakeCopy(std::string id) const override
            {
                auto ret = std::make_unique<TextureBitmapGeneratorSource>(*this);
                ret->mId = std::move(id);
                return ret;
            }
        private:
            std::string mId;
            std::string mName;
            std::unique_ptr<IBitmapGenerator> mGenerator;
            base::bitflag<Effect> mEffects;
        };

        // Rasterize text buffer and provide as a texture source.
        class TextureTextBufferSource : public TextureSource
        {
        public:
            TextureTextBufferSource()
              : mId(base::RandomString(10))
            {}

            explicit TextureTextBufferSource(const TextBuffer& text, std::string id = base::RandomString(10))
              : mId(std::move(id))
              , mTextBuffer(text)
            {}

            explicit TextureTextBufferSource(TextBuffer&& text, std::string id = base::RandomString(10)) noexcept
              : mId(std::move(id))
              , mTextBuffer(std::move(text))
            {}

            virtual base::bitflag<Effect> GetEffects() const override
            {return mEffects; }
            virtual Source GetSourceType() const override
            { return Source::TextBuffer; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::size_t GetHash() const override
            {
                auto hash = mTextBuffer.GetHash();
                hash = base::hash_combine(hash, mId);
                hash = base::hash_combine(hash, mName);
                hash = base::hash_combine(hash, mEffects);
                return hash;
            }
            virtual std::string GetName() const override
            { return mName; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual void SetEffect(Effect effect, bool on_off) override
            { mEffects.set(effect, on_off); }
            virtual std::shared_ptr<IBitmap> GetData() const override;

            virtual Texture* Upload(const Environment&env, Device& device) const override;

            virtual void IntoJson(data::Writer& data) const override;
            virtual bool FromJson(const data::Reader& data) override;

            gfx::TextBuffer& GetTextBuffer()
            { return mTextBuffer; }
            const gfx::TextBuffer& GetTextBuffer() const
            { return mTextBuffer; }

            void SetTextBuffer(const TextBuffer& text)
            { mTextBuffer = text; }
            void SetTextBuffer(TextBuffer&& text)
            { mTextBuffer = std::move(text); }
        protected:
            virtual std::unique_ptr<TextureSource> MakeCopy(std::string id) const override
            {
                auto ret = std::make_unique<TextureTextBufferSource>(*this);
                ret->mId = std::move(id);
                return ret;
            }

        private:
            std::string mId;
            std::string mName;
            TextBuffer mTextBuffer;
            base::bitflag<Effect> mEffects;
        };

    } // detail

    inline auto  UseExistingTexture(std::string gpu_id, gfx::Texture* texture = nullptr, std::string id = base::RandomString(10))
    { return std::make_unique<detail::TextureTextureSource>(std::move(gpu_id), texture, std::move(id)); }

    inline auto LoadTextureFromFile(std::string uri, std::string id = base::RandomString(10))
    { return std::make_unique<detail::TextureFileSource>(std::move(uri), std::move(id)); }

    template<typename T>
    inline auto CreateTextureFromBitmap(const Bitmap<T>& bitmap, std::string id = base::RandomString(10))
    { return std::make_unique<detail::TextureBitmapBufferSource>(bitmap, id); }

    template<typename T>
    inline auto CreateTextureFromBitmap(Bitmap<T>&& bitmap, std::string id = base::RandomString(10))
    { return std::make_unique<detail::TextureBitmapBufferSource>(std::forward<T>(bitmap), std::move(id)); }

    inline auto CreateTextureFromText(const TextBuffer& text, std::string id = base::RandomString(10))
    { return std::make_unique<detail::TextureTextBufferSource>(text, std::move(id)); }
    inline auto CreateTextureFromText(TextBuffer&& text, std::string id = base::RandomString(10))
    { return std::make_unique<detail::TextureTextBufferSource>(std::move(text), std::move(id)); }

    inline auto GenerateTexture(std::unique_ptr<IBitmapGenerator> generator, std::string id = base::RandomString(10))
    {  return std::make_unique<detail::TextureBitmapGeneratorSource>(std::move(generator), std::move(id)); }

    template<typename T>
    inline auto GenerateTexture(T&& generator, std::string id = base::RandomString(10))
    {
        auto gen = std::make_unique<std::remove_reference_t<T>>(std::forward<T>(generator));
        return GenerateTexture(std::move(gen), std::move(id));
    }

    inline auto GenerateNoiseTexture(const NoiseBitmapGenerator& generator, std::string id = base::RandomString(10))
    {
        auto gen = std::make_unique<NoiseBitmapGenerator>(generator);
        return std::make_unique<detail::TextureBitmapGeneratorSource>(std::move(gen), std::move(id));
    }
    inline auto GenerateNoiseTexture(NoiseBitmapGenerator&& generator, std::string id = base::RandomString(10))
    {
        auto gen = std::make_unique<NoiseBitmapGenerator>(std::move(generator));
        return std::make_unique<detail::TextureBitmapGeneratorSource>(std::move(gen), std::move(id));
    }

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

        TextureMap(std::string id = base::RandomString(10))
          : mName("Default")
          , mId(std::move(id))
        {}
        TextureMap(const TextureMap& other, bool copy);
        TextureMap(const TextureMap& other) : TextureMap(other, true) {}

        // Get the type of the texture map.
        inline Type GetType() const noexcept
        { return mType; }
        inline float GetFps() const noexcept
        { return mFps; }
        inline bool IsLooping() const noexcept
        { return mLooping; }
        inline bool HasSpriteSheet() const noexcept
        { return mSpriteSheet.has_value(); }
        inline std::string GetId() const noexcept
        { return mId; }
        inline std::string GetName() const noexcept
        { return mName; }

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
        inline void SetFps(float fps) noexcept
        { mFps = fps; }
        inline void SetLooping(bool looping) noexcept
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

        unsigned GetSpriteSpriteFrameCount() const;

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
        enum class ParticleAction {
            // Do nothing
            None,
            // Rotate the texture coordinates
            Rotate
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
            // True when rendering points, i.e. the material is being applied
            // on anything that rasterizes as GL_POINTS. When this is true
            // the shader must use the built-in gl_PointCoord variable instead
            // of the texture coordinates coming from the vertex shader.
            bool render_points = false;
            // The current material instance time.
            double material_time = 0.0f;
            // The uniform parameters set on the material instance (if any).
            // The instance uniforms will take precedence over the uniforms
            // set in the class whenever they're set.
            const UniformMap* uniforms = nullptr;
            // the current render pass in which the material is used.
            const ShaderPass* shader_pass = nullptr;
        };


        MaterialClass(Type type, std::string id = base::RandomString(10));
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

        // Material uniform API for setting/getting "Known" uniforms.
        inline void SetAlphaCutoff(float cutoff) noexcept
        { SetUniform("kAlphaCutoff", cutoff); }
        inline void SetGamma(float gamma) noexcept
        { SetUniform("kGamma", gamma); }
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

        inline float GetAlphaCutoff() const noexcept
        { return GetUniformValue<float>("kAlphaCutoff", 0.0f); }
        inline float GetGamma() const noexcept
        { return GetUniformValue<float>("kGamma", 1.0f); }
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
        inline void SetUniform(const std::string& name, Uniform&& value) noexcept
        { mUniforms[name] = std::move(value); }
        inline Uniform* FindUniform(const std::string& name) noexcept
        { return base::SafeFind(mUniforms, name); }
        inline const Uniform* FindUniform(const std::string& name) const noexcept
        { return base::SafeFind(mUniforms, name); }
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

        // Get the program ID for the material that is used to map the
        // material to a device specific program object.
        std::string GetProgramId(const State& state) const noexcept;
        // Get the material class hash value based on the current properties
        // of the class.
        std::size_t GetHash() const noexcept;
        // Create the shader for this material on the given device.
        // Returns the new shader object or nullptr if the shader fails to compile.
        Shader* GetShader(const State& state, Device& device) const noexcept;
        // Apply the material properties onto the given program object based
        // on the material class and the material instance state.
        bool ApplyDynamicState(const State& state, Device& device, Program& program) const noexcept;
        // Apply the static state, i.e. the material state that doesn't change
        // during the material's lifetime and need to be only set once.
        void ApplyStaticState(const State& state, Device& device, Program& program) const noexcept;
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

        static std::string GetColorUniformName(ColorIndex index);

        static std::unique_ptr<MaterialClass> ClassFromJson(const data::Reader& data);

        MaterialClass& operator=(const MaterialClass& other);
    private:
        template<typename T>
        static bool SetUniform(const char* name, const UniformMap* uniforms, const T& backup, Program& program)
        {
            if (uniforms)
            {
                auto it = uniforms->find(name);
                if (it == uniforms->end()) {
                    program.SetUniform(name, backup);
                    return true;
                }
                const auto& value = it->second;
                if (const auto* ptr = std::get_if<T>(&value)) {
                    program.SetUniform(name, *ptr);
                    return true;
                }
            }
            program.SetUniform(name, backup);
            return false;
        }
        template<typename T>
        bool ReadLegacyValue(const char* name, const char* uniform, const data::Reader& reader);

    private:
        TextureMap* SelectTextureMap(const State& state) const noexcept;
        std::string GetShaderSource(const State& state, Device& device) const;
        std::string GetColorShaderSource(const State& state, Device& device) const;
        std::string GetGradientShaderSource(const State& state, Device& device) const;
        std::string GetSpriteShaderSource(const State& state, Device& device) const;
        std::string GetTextureShaderSource(const State& state, Device& device) const;
        bool ApplySpriteDynamicState(const State& state, Device& device, Program& program) const noexcept;
        bool ApplyCustomDynamicState(const State& state, Device& device, Program& program) const noexcept;
        bool ApplyTextureDynamicState(const State& state, Device& device, Program& program) const noexcept;

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
        virtual ~Material() = default;

        struct Environment {
            const ShaderPass* shader_pass = nullptr;
            // true if running in an "editing mode", which means that even
            // content marked static might have changed and should be checked
            // in case it has been modified and should be re-uploaded.
            bool editing_mode  = false;
            bool render_points = false;
        };
        struct RasterState {
            using Blending = Device::State::BlendOp;
            Blending blending = Blending::None;
            bool premultiplied_alpha = false;
        };
        // Apply the dynamic material properties to the given program object
        // and set the rasterizer state. Dynamic properties are the properties
        // that can change between one material instance to another even when
        // the underlying material type is the same. For example two instances
        // of material "marble" have the same underlying static type "marble"
        // but both instances have their own instance state as well.
        // Returns true if the state is applied correctly and drawing can proceed.
        virtual bool ApplyDynamicState(const Environment& env, Device& device, Program& program, RasterState& raster) const = 0;
        // Apply the static state, i.e. the material state that doesn't change
        // during the material's lifetime and need to be only set once.
        virtual void ApplyStaticState(const Environment& env, Device& device, Program& program) const = 0;
        // Create the shader for this material on the given device.
        // Returns the new shader object or nullptr if the shader failed to compile.
        virtual Shader* GetShader(const Environment& env, Device& device) const = 0;
        // Get the program ID for the material that is used to map the
        // material to a device specific program object.
        virtual std::string GetProgramId(const Environment& env) const = 0;
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
    class MaterialClassInst : public Material
    {
    public:
        // Create new material instance based on the given material class.
        MaterialClassInst(const std::shared_ptr<const MaterialClass>& klass, double time = 0.0)
            : mClass(klass)
            , mRuntime(time)
        {}
        MaterialClassInst(const MaterialClass& klass, double time = 0.0)
           : mClass(klass.Copy())
           , mRuntime(time)
        {}

        // Apply the material properties to the given program object and set the rasterizer state.
        virtual bool ApplyDynamicState(const Environment& env, Device& device, Program& program, RasterState& raster) const override;
        virtual Shader* GetShader(const Environment& env, Device& device) const override;
        virtual void ApplyStaticState(const Environment& env, Device& device, Program& program) const override;
        virtual std::string GetProgramId(const Environment& env) const override;

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

    // material specialized for rendering text using
    // a pre-rasterized bitmap of text. Creates transient
    // texture objects for the text.
    class TextMaterial : public gfx::Material
    {
    public:
        TextMaterial(const TextBuffer& text);
        TextMaterial(TextBuffer&& text);
        virtual bool ApplyDynamicState(const Environment& env, Device& device, Program& program, RasterState& raster) const override;
        virtual void ApplyStaticState(const Environment& env, Device& device, Program& program) const override;
        virtual Shader* GetShader(const Environment& env, Device& device) const override;
        virtual std::string GetProgramId(const Environment&) const override;
        virtual std::string GetClassId() const override;
        virtual void Update(float dt) override;
        virtual void SetRuntime(double runtime) override;
        virtual void SetUniform(const std::string& name, const Uniform& value) override;
        virtual void SetUniform(const std::string& name, Uniform&& value) override;
        virtual void ResetUniforms()  override;
        virtual void SetUniforms(const UniformMap& uniforms) override;
        virtual double GetRuntime() const override
        { return 0.0f; }

        void ComputeTextMetrics(unsigned* width, unsigned* height) const;

        inline void SetColor(const Color4f& color) noexcept
        { mColor = color; }
        // Set point sampling to true in order to use a fast filtering
        // when sampling from the texture. This should be for maximum perf
        // ideally when the geometry to be drawn matches closely with the
        // rasterized text texture/buffer. So when the texture maps onto
        // a rectangle and there's now transformation that would change the
        // rasterized dimensions (in pixels) of the rectangle from the
        // dimensions of the rasterized text texture.
        // The default is true.
        inline void SetPointSampling(bool on_off) noexcept
        { mPointSampling = on_off; }
    private:
        TextBuffer mText;
        Color4f mColor = Color::White;
        bool mPointSampling = true;
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

    MaterialClassInst CreateMaterialFromColor(const Color4f& top_left,
                                              const Color4f& top_right,
                                              const Color4f& bottom_left,
                                              const Color4f& bottom_right);
    MaterialClassInst CreateMaterialFromColor(const Color4f& color);
    MaterialClassInst CreateMaterialFromImage(const std::string& uri);
    MaterialClassInst CreateMaterialFromImages(const std::initializer_list<std::string>& uris);
    MaterialClassInst CreateMaterialFromImages(const std::vector<std::string>& uris);
    MaterialClassInst CreateMaterialFromSpriteAtlas(const std::string& uri, const std::vector<FRect>& frames);
    MaterialClassInst CreateMaterialFromText(const TextBuffer& text);
    MaterialClassInst CreateMaterialFromText(TextBuffer&& text);
    MaterialClassInst CreateMaterialFromTexture(std::string gpu_id, Texture* texture = nullptr);

    TextMaterial CreateMaterialFromText(const std::string& text,
                                        const std::string& font,
                                        const gfx::Color4f& color,
                                        unsigned font_size_px,
                                        unsigned raster_width,
                                        unsigned raster_height,
                                        unsigned text_align,
                                        unsigned text_prop,
                                        float line_height);

    std::unique_ptr<Material> CreateMaterialInstance(const MaterialClass& klass);
    std::unique_ptr<Material> CreateMaterialInstance(const std::shared_ptr<const MaterialClass>& klass);
    std::unique_ptr<TextMaterial> CreateMaterialInstance(const TextBuffer& text);
    std::unique_ptr<TextMaterial> CreateMaterialInstance(TextBuffer&& text);


} // namespace
