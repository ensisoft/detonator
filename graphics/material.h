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
        // Enum to specify what is the underlying data source for
        // for the texture data.
        enum class Source {
            // Data comes from a file (such as a .png or a .jpg) in the filesystem.
            Filesystem,
            // Data comes as an in memory bitmap rasterized by the TextBuffer
            // based on the text/font content/parameters.
            TextBuffer,
            // Data comes from a bitmap buffer.
            BitmapBuffer,
            // Data comes from a bitmap generator algorithm.
            BitmapGenerator
        };
        enum class ColorSpace {
            Linear, sRGB
        };

        virtual ~TextureSource() = default;
        // Get the color space of the texture source's texture content
        virtual ColorSpace GetColorSpace() const
        { return ColorSpace::Linear; }
        // Get the type of the source of the texture data.
        virtual Source GetSourceType() const = 0;
        // Get texture source class/resource id.
        virtual std::string GetId() const = 0;
        // Get the identifier to be used for the GPU side resource.
        virtual std::string GetGpuId() const
        { return GetId(); }
        // Get the human-readable / and settable name.
        virtual std::string GetName() const = 0;
        // Get the texture source hash value based on the properties
        // of the texture source object itself *and* its content.
        virtual std::size_t GetHash() const = 0;
        // Get the hash value based on the content of the texture source.
        // This hash value only covers the bits of the *content* i.e.
        // the content of the IBitmap to be generated.
        virtual std::size_t GetContentHash() const = 0;
        // Set the texture source human-readable name.
        virtual void SetName(const std::string& name) = 0;
        // Generate or load the data as a bitmap. If there's a content
        // error this function should return empty shared pointer.
        // The returned bitmap can be potentially immutably shared.
        virtual std::shared_ptr<IBitmap> GetData() const = 0;
        // Create a similar clone of this texture source but
        // with unique id.
        virtual std::unique_ptr<TextureSource> Clone() const = 0;
        // Create an exact bitwise copy of this texture source object.
        virtual std::unique_ptr<TextureSource> Copy() const = 0;
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
    private:
    };

    namespace detail {
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
            {
                mId = base::RandomString(10);
                mFlags.set(Flags::AllowResizing, true);
                mFlags.set(Flags::AllowPacking, true);
            }
            TextureFileSource(const std::string& file) : TextureFileSource()
            { mFile = file; }
            virtual ColorSpace GetColorSpace() const override
            { return mColorSpace; }
            virtual Source GetSourceType() const override
            { return Source::Filesystem; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::string GetGpuId() const override
            {
                // using the mFile URI is *not* enough to uniquely
                // identify this texture object on the GPU because it's
                // possible that the *same* texture object (same underlying file)
                // is used with *different* flags in another material.
                // in other words, "foo.png with premultiplied alpha" must be
                // a different GPU texture object than "foo.png with straight alpha".
                size_t hash = 0;
                hash = base::hash_combine(hash, mFile);
                hash = base::hash_combine(hash, mColorSpace);
                hash = base::hash_combine(hash, mFlags.test(Flags::PremulAlpha));
                return std::to_string(hash);
            }
            virtual std::string GetName() const override
            { return mName; }
            virtual std::size_t GetHash() const override
            {
                auto hash = GetContentHash();
                hash = base::hash_combine(hash, mId);
                hash = base::hash_combine(hash, mName);
                hash = base::hash_combine(hash, mFlags);
                hash = base::hash_combine(hash, mColorSpace);
                return hash;
            }
            virtual std::size_t GetContentHash() const override
            { return base::hash_combine(0, mFile); }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual std::shared_ptr<IBitmap> GetData() const override;
            virtual std::unique_ptr<TextureSource> Clone() const override
            {
                auto ret = std::make_unique<TextureFileSource>(*this);
                ret->mId = base::RandomString(10);
                return ret;
            }
            virtual std::unique_ptr<TextureSource> Copy() const override
            { return std::make_unique<TextureFileSource>(*this); }

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
        private:
            std::string mId;
            std::string mFile;
            std::string mName;
            base::bitflag<Flags> mFlags;
            ColorSpace mColorSpace = ColorSpace::Linear; // default to liaear now for compatibility
        private:
        };

        // Source texture data from a bitmap
        class TextureBitmapBufferSource : public TextureSource
        {
        public:
            TextureBitmapBufferSource()
            { mId = base::RandomString(10); }
            TextureBitmapBufferSource(std::unique_ptr<IBitmap>&& bitmap) : TextureBitmapBufferSource()
            { mBitmap = std::move(bitmap); }
            template<typename T>
            TextureBitmapBufferSource(const Bitmap<T>& bmp) : TextureBitmapBufferSource()
            {
                mBitmap.reset(new Bitmap<T>(bmp));
            }
            template<typename T>
            TextureBitmapBufferSource(Bitmap<T>&& bmp) : TextureBitmapBufferSource()
            {
                mBitmap.reset(new Bitmap<T>(std::move(bmp)));
            }

            TextureBitmapBufferSource(const TextureBitmapBufferSource& other)
            {
                mId     = other.mId;
                mName   = other.mName;
                mBitmap = other.mBitmap->Clone();
            }

            virtual Source GetSourceType() const override
            { return Source::BitmapBuffer; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::size_t GetHash()  const override
            {
                auto hash = GetContentHash();
                hash = base::hash_combine(hash, mId);
                hash = base::hash_combine(hash, mName);
                return hash;
            }
            virtual std::size_t GetContentHash() const override
            {
                size_t hash = mBitmap->GetHash();
                hash = base::hash_combine(hash, mBitmap->GetWidth());
                hash = base::hash_combine(hash, mBitmap->GetHeight());
                return hash;
            }
            virtual std::string GetName() const override
            { return mName; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual std::shared_ptr<IBitmap> GetData() const override
            { return mBitmap; }
            virtual std::unique_ptr<TextureSource> Clone() const override
            {
                auto ret = std::make_unique<TextureBitmapBufferSource>(*this);
                ret->mId = base::RandomString(10);
                return ret;
            }
            virtual std::unique_ptr<TextureSource> Copy() const override
            { return std::make_unique<TextureBitmapBufferSource>(*this); }

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
        private:
            std::string mId;
            std::string mName;
            std::shared_ptr<IBitmap> mBitmap;
        };

        // Source texture data from a bitmap
        class TextureBitmapGeneratorSource : public TextureSource
        {
        public:
            TextureBitmapGeneratorSource()
            { mId = base::RandomString(10); }
            TextureBitmapGeneratorSource(std::unique_ptr<IBitmapGenerator>&& generator)
                : TextureBitmapGeneratorSource()
            {
                mGenerator = std::move(generator);
            }
            TextureBitmapGeneratorSource(const TextureBitmapGeneratorSource& other)
            {
                mGenerator = other.mGenerator->Clone();
                mId = other.mId;
                mName = other.mName;
            }

            virtual Source GetSourceType() const override
            { return Source::BitmapGenerator; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::size_t GetHash() const override
            {
                auto hash = GetContentHash();
                hash = base::hash_combine(hash, mId);
                hash = base::hash_combine(hash, mName);
                return hash;
            }
            virtual std::size_t GetContentHash() const override
            { return mGenerator->GetHash(); }
            virtual std::string GetName() const override
            { return mName; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual std::shared_ptr<IBitmap> GetData() const override
            { return mGenerator->Generate(); }
            virtual std::unique_ptr<TextureSource> Clone() const override
            {
                auto ret = std::make_unique<TextureBitmapGeneratorSource>(*this);
                ret->mId = base::RandomString(10);
                return ret;
            }
            virtual std::unique_ptr<TextureSource> Copy() const override
            { return std::make_unique<TextureBitmapGeneratorSource>(*this); }

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
        private:
            std::string mId;
            std::string mName;
            std::unique_ptr<IBitmapGenerator> mGenerator;
        };

        // Rasterize text buffer and provide as a texture source.
        class TextureTextBufferSource : public TextureSource
        {
        public:
            TextureTextBufferSource()
            { mId = base::RandomString(10); }

            TextureTextBufferSource(const TextBuffer& text) : TextureTextBufferSource()
            {
                mTextBuffer = text;
            }
            TextureTextBufferSource(TextBuffer&& text) : TextureTextBufferSource()
            {
                mTextBuffer = std::move(text);
            }

            virtual Source GetSourceType() const override
            { return Source::TextBuffer; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::size_t GetHash() const override
            {
                auto hash = GetContentHash();
                hash = base::hash_combine(hash, mId);
                hash = base::hash_combine(hash, mName);
                return hash;
            }
            virtual std::size_t GetContentHash() const override
            { return mTextBuffer.GetHash(); }
            virtual std::string GetName() const override
            { return mName; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual std::shared_ptr<IBitmap> GetData() const override;
            virtual std::unique_ptr<TextureSource> Clone() const override
            {
                auto ret = std::make_unique<TextureTextBufferSource>(*this);
                ret->mId = base::RandomString(10);
                return ret;
            }
            virtual std::unique_ptr<TextureSource> Copy() const override
            { return std::make_unique<TextureTextBufferSource>(*this); }

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
        private:
            TextBuffer mTextBuffer;
            std::string mId;
            std::string mName;
        };

    } // detail

    inline std::unique_ptr<detail::TextureFileSource> LoadTextureFromFile(const std::string& uri)
    { return std::make_unique<detail::TextureFileSource>(uri); }

    template<typename T> inline
    std::unique_ptr<detail::TextureBitmapBufferSource> CreateTextureFromBitmap(const Bitmap<T>& bitmap)
    { return std::make_unique<detail::TextureBitmapBufferSource>(bitmap); }
    template<typename T> inline
    std::unique_ptr<TextureSource> CreateTextureFromBitmap(Bitmap<T>&& bitmap)
    { return std::make_unique<detail::TextureBitmapBufferSource>(std::forward<T>(bitmap)); }

    inline std::unique_ptr<detail::TextureTextBufferSource> CreateTextureFromText(const TextBuffer& text)
    { return std::make_unique<detail::TextureTextBufferSource>(text); }
    inline std::unique_ptr<detail::TextureTextBufferSource> CreateTextureFromText(TextBuffer&& text)
    { return std::make_unique<detail::TextureTextBufferSource>(std::move(text)); }

    inline std::unique_ptr<detail::TextureBitmapGeneratorSource> GenerateTexture(std::unique_ptr<IBitmapGenerator> generator)
    { return std::make_unique<detail::TextureBitmapGeneratorSource>(std::move(generator)); }
    template<typename T> inline
    std::unique_ptr<detail::TextureBitmapGeneratorSource> GenerateTexture(T&& generator)
    { return GenerateTexture(std::make_unique<std::remove_reference_t<T>>(std::forward<T>(generator))); }

    inline std::unique_ptr<detail::TextureBitmapGeneratorSource> GenerateNoiseTexture(const NoiseBitmapGenerator& generator)
    {
        auto gen = std::make_unique<NoiseBitmapGenerator>(generator);
        return std::make_unique<detail::TextureBitmapGeneratorSource>(std::move(gen));
    }
    inline std::unique_ptr<detail::TextureBitmapGeneratorSource> GenerateNoiseTexture(NoiseBitmapGenerator&& generator)
    {
        auto gen = std::make_unique<NoiseBitmapGenerator>(std::move(generator));
        return std::make_unique<detail::TextureBitmapGeneratorSource>(std::move(gen));
    }


    class SpriteMap;
    class TextureMap2D;

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
            // and chooses 2 textures closes to the current point in time
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

        virtual ~TextureMap() = default;
        // Get the type of the texture map.
        virtual Type GetType() const = 0;
        // Get the hash value based on the current state of the material map.
        virtual std::size_t GetHash() const = 0;
        // Create a similar clone of this texture map but with a new unique id.
        virtual std::unique_ptr<TextureMap> Clone() const = 0;
        // Create an exact bit-wise copy of this texture map.
        virtual std::unique_ptr<TextureMap> Copy() const = 0;
        // Select texture objects for sampling based on the current binding state.
        // If the texture objects don't yet exist on the device they're created.
        // The resulting BoundState expresses which textures should currently be
        // used and which are the sampler/uniform names that should be used when
        // binding the textures to the program's state before drawing.
        // Returns true if successful, otherwise false on error.
        virtual bool BindTextures(const BindingState& state, Device& device, BoundState& result) const = 0;
        // Serialize into JSON object.
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load state from JSON object. Returns true if successful.
        virtual bool FromJson(const data::Reader& data) = 0;
        // Find a specific texture source based on the texture source id.
        // Returns nullptr if no matching texture source was found.
        virtual TextureSource* FindTextureSourceById(const std::string& id) = 0;
        // Find a texture source based on its name. Note that the names are not
        // necessarily unique. In such case it's unspecified which texture source
        // object is returned. Returns nullptr if no matching texture source was found.
        virtual TextureSource* FindTextureSourceByName(const std::string& name) = 0;
        // Find a specific texture source based on the texture source id.
        // Returns nullptr if no matching texture source was found.
        virtual const TextureSource* FindTextureSourceById(const std::string& id) const = 0;
        // Find a texture source based on its name. Note that the names are not
        // necessarily unique. In such case it's unspecified which texture source
        // object is returned. Returns nullptr if no matching texture source was found.
        virtual const TextureSource* FindTextureSourceByName(const std::string& name) const = 0;
        // Find the texture rectangle set for the texture source. Returns false if no such
        // source was found in this texture map and rect remains unmodified.
        virtual bool FindTextureRect(const TextureSource* source, FRect* rect) const = 0;
        // Set the texture rect for the given texture source. Returns false if no such
        // source was found in this texture map.
        virtual bool SetTextureRect(const TextureSource* source, const FRect& rect) = 0;
        // Delete the ive texture source. Returns true if source was found and deleted.
        virtual bool DeleteTexture(const TextureSource* source) = 0;
        // Reset all texture objects. After this the texture map contains no textures.
        virtual void ResetTextures() = 0;

        // down cast helpers.
        SpriteMap* AsSpriteMap();
        TextureMap2D* AsTextureMap2D();
        const SpriteMap* AsSpriteMap() const;
        const TextureMap2D* AsTextureMap2D() const;
    private:
    };

    // Implements texture mapping by cycling through a series of textures
    // based on the current time, the number of textures and the speed at
    // which the textures are cycled through.
    class SpriteMap : public TextureMap
    {
    public:
        SpriteMap() = default;
        SpriteMap(const SpriteMap& other, bool copy);
        SpriteMap(const SpriteMap& other) : SpriteMap(other, true) {}

        void AddTexture(std::unique_ptr<TextureSource> source)
        {
            mSprites.emplace_back();
            mSprites.back().source = std::move(source);
        }
        void AddTexture(std::unique_ptr<TextureSource> source, const FRect& rect)
        {
            mSprites.emplace_back();
            mSprites.back().source = std::move(source);
            mSprites.back().rect = rect;
        }
        void DeleteTexture(size_t index)
        { base::SafeErase(mSprites, index); }
        void DeleteTextureById(const std::string& id);
        const TextureSource* GetTextureSource(size_t index) const
        { return base::SafeIndex(mSprites, index).source.get(); }
        TextureSource* GetTextureSource(size_t index)
        { return base::SafeIndex(mSprites, index).source.get(); }
        void SetSamplerName(const std::string& name, size_t index)
        { mSamplerName[index] = name; }
        void SetRectUniformName(const std::string& name, size_t index)
        { mRectUniformName[index] = name; }
        void SetTextureRect(std::size_t index, const FRect& rect)
        { base::SafeIndex(mSprites, index).rect = rect; }
        void SetTextureSource(std::size_t index, std::unique_ptr<TextureSource> source)
        { base::SafeIndex(mSprites, index).source = std::move(source); }
        void SetFps(float fps)
        { mFps = fps; }
        void SetLooping(bool looping)
        { mLooping = looping; }
        std::string GetSamplerName(size_t index) const
        { return mSamplerName[index]; }
        std::string GetRectUniformName(size_t index) const
        { return mRectUniformName[index]; }
        float GetFps() const
        { return mFps; }
        FRect GetTextureRect(size_t index) const
        { return base::SafeIndex(mSprites, index).rect; }
        size_t GetNumTextures() const
        { return mSprites.size(); }
        bool IsLooping() const
        { return mLooping; }
        // TextureMap implementation.
        virtual Type GetType() const override
        { return Type::Sprite; }
        virtual std::size_t GetHash() const override;
        virtual std::unique_ptr<TextureMap> Copy() const override
        { return std::make_unique<SpriteMap>(*this, true); }
        virtual std::unique_ptr<TextureMap> Clone() const override
        { return std::make_unique<SpriteMap>(*this, false); }
        virtual bool BindTextures(const BindingState& state, Device& device, BoundState& result) const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
        virtual TextureSource* FindTextureSourceById(const std::string& id) override;
        virtual TextureSource* FindTextureSourceByName(const std::string& name) override;
        virtual const TextureSource* FindTextureSourceById(const std::string& id) const override;
        virtual const TextureSource* FindTextureSourceByName(const std::string& name) const override;
        virtual bool FindTextureRect(const TextureSource* source, FRect* rect) const override;
        virtual bool SetTextureRect(const TextureSource* source, const FRect& rect) override;
        virtual bool DeleteTexture(const TextureSource* source) override;
        virtual void ResetTextures() override
        { mSprites.clear(); }
        SpriteMap& operator=(const SpriteMap& other);
    private:
        float mFps = 0.0f;
        struct Sprite {
            FRect rect = FRect(0.0f, 0.0f, 1.0f, 1.0f);
            std::unique_ptr<TextureSource> source;
        };
        std::vector<Sprite> mSprites;
        std::string mSamplerName[2];
        std::string mRectUniformName[2];
        bool mLooping = true;
    };

    // Implements texture mapping by always mapping a single 2D texture object
    // onto to the same texture sampler on every single bind.
    class TextureMap2D : public TextureMap
    {
    public:
        TextureMap2D() = default;
        TextureMap2D(const TextureMap2D& other, bool copy);
        TextureMap2D(const TextureMap2D& other) : TextureMap2D(other, true){}
        void SetTexture(std::unique_ptr<TextureSource> source)
        { mSource = std::move(source); }
        void SetTexture(std::unique_ptr<TextureSource> source, const FRect& rect)
        {
            mSource = std::move(source);
            mRect   = rect;
        }
        void SetTextureRect(const FRect& rect)
        { mRect = rect; }
        void ResetTexture()
        {
            mSource.reset();
            mRect = FRect(0.0f, 0.0f, 1.0f, 1.0f);
        }
        void SetSamplerName(const std::string& name)
        { mSamplerName = name; }
        void SetRectUniformName(const std::string& name)
        { mRectUniformName = name; }
        std::string GetSamplerName() const
        { return mSamplerName; }
        std::string GetRectUniformName() const
        { return mRectUniformName; }
        gfx::FRect GetTextureRect() const
        { return mRect; }
        TextureSource* GetTextureSource()
        { return mSource.get(); }
        const TextureSource* GetTextureSource() const
        { return mSource.get(); }
        // TextureMap implementation.
        virtual Type GetType() const override
        { return Type::Texture2D; }
        virtual std::unique_ptr<TextureMap> Copy() const override
        { return std::make_unique<TextureMap2D>(*this, true); }
        virtual std::unique_ptr<TextureMap> Clone() const override
        { return std::make_unique<TextureMap2D>(*this, false); }
        virtual std::size_t GetHash() const override;
        virtual bool BindTextures(const BindingState& state, Device& device, BoundState& result) const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
        virtual TextureSource* FindTextureSourceById(const std::string& id) override;
        virtual TextureSource* FindTextureSourceByName(const std::string& name) override;
        virtual const TextureSource* FindTextureSourceById(const std::string& id) const override;
        virtual const TextureSource* FindTextureSourceByName(const std::string& name) const override;
        virtual bool FindTextureRect(const TextureSource* source, FRect* rect) const override;
        virtual bool SetTextureRect(const TextureSource* source, const FRect& rect) override;
        virtual bool DeleteTexture(const TextureSource* source) override;
        virtual void ResetTextures() override { ResetTexture(); }
        TextureMap2D& operator=(const TextureMap2D& other);
    private:
        std::unique_ptr<TextureSource> mSource;
        gfx::FRect mRect {0.0f, 0.0f, 1.0f, 1.0f};
        std::string mSamplerName;
        std::string mRectUniformName;
    };


    class BuiltInMaterialClass;
    class ColorClass;
    class GradientClass;
    class SpriteClass;
    class TextureMap2DClass;
    class CustomMaterialClass;

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
            // When set, change the transparent blending equation
            // to expect alpha values to be premultiplied in the
            // RGB values.
            PremultipliedAlpha
        };

        // Material/Shader uniform.
        using Uniform = std::variant<float, int,
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

        virtual ~MaterialClass() = default;
        // Get the material class id.
        virtual std::string GetId() const = 0;
        // Get the human readable material class name.
        virtual std::string GetName() const = 0;
        // Get the program ID for the material that is used to map the
        // material to a device specific program object.
        virtual std::string GetProgramId(const State& state) const = 0;
        // Get the material class hash value based on the current properties
        // of the class.
        virtual std::size_t GetHash() const = 0;
        // Get the actual implementation type of the material.
        virtual Type GetType() const = 0;
        // Get the surface type of the material.
        virtual SurfaceType GetSurfaceType() const = 0;
        // Set the surface type of the material.
        virtual void SetSurfaceType(SurfaceType surface) = 0;
        // Set the material class id. Used when creating specific materials with
        // fixed static ids. Todo: refactor away and use constructor.
        virtual void SetId(const std::string& id) = 0;
        // Set the human-readable material class name.
        virtual void SetName(const std::string& name) = 0;
        // Create the shader for this material on the given device.
        // Returns the new shader object or nullptr if the shader fails to compile.
        virtual Shader* GetShader(const State& state, Device& device) const = 0;
        // Apply the material properties onto the given program object based
        // on the material class and the material instance state.
        virtual void ApplyDynamicState(const State& state, Device& device, Program& program) const = 0;
        // Apply the static state, i.e. the material state that doesn't change
        // during the material's lifetime and need to be only set once.
        virtual void ApplyStaticState(const State& state, Device& device, Program& program) const = 0;
        // Serialize the class into JSON.
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load the class from JSON. Returns true on success.
        virtual bool FromJson(const data::Reader& data) = 0;
        // Create an exact bitwise copy of this material class.
        virtual std::unique_ptr<MaterialClass> Copy() const = 0;
        // Create a similar clone of this material class but with unique id.
        virtual std::unique_ptr<MaterialClass> Clone() const = 0;
        // Begin the packing process by going over the associated resources
        // in the material and invoking the packer methods to pack
        // those resources.
        virtual void BeginPacking(TexturePacker* packer) const = 0;
        // Finish the packing process by retrieving the new updated resource
        // information the packer and updating the material's state.
        virtual void FinishPacking(const TexturePacker* packer) = 0;
        // Test a material flag. Returns true if the flag is set, otherwise false.
        virtual bool TestFlag(Flags flag) const = 0;
        // Set a material flag to on or off.
        virtual void SetFlag(Flags flag, bool on_off) = 0;
        // Helpers
        bool PremultipliedAlpha() const
        { return TestFlag(Flags::PremultipliedAlpha); }

        inline BuiltInMaterialClass* AsBuiltIn()
        { return MaterialCast<BuiltInMaterialClass>(this); }
        inline SpriteClass* AsSprite()
        { return MaterialCast<SpriteClass>(this); }
        inline TextureMap2DClass* AsTexture()
        { return MaterialCast<TextureMap2DClass>(this); }
        inline ColorClass* AsColor()
        { return MaterialCast<ColorClass>(this); }
        inline GradientClass* AsGradient()
        { return MaterialCast<GradientClass>(this); }
        inline CustomMaterialClass* AsCustom()
        { return MaterialCast<CustomMaterialClass>(this); }
        inline const BuiltInMaterialClass* AsBuiltIn() const
        { return MaterialCast<const BuiltInMaterialClass>(this); }
        inline const SpriteClass* AsSprite() const
        { return MaterialCast<const SpriteClass>(this); }
        inline const TextureMap2DClass* AsTexture() const
        { return MaterialCast<const TextureMap2DClass>(this); }
        inline const ColorClass* AsColor() const
        { return MaterialCast<const ColorClass>(this); }
        inline const GradientClass* AsGradient() const
        { return MaterialCast<const GradientClass>(this); }
        inline const CustomMaterialClass* AsCustom() const
        { return MaterialCast<const CustomMaterialClass>(this); }

        static std::unique_ptr<MaterialClass> ClassFromJson(const data::Reader& data);
    protected:
        MaterialClass() = default;
        MaterialClass& operator=(const MaterialClass&) = default;

        template<typename T, typename F>
        T* MaterialCast(F* self) const
        { return dynamic_cast<T*>(self); }

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
    private:
    };

    // Base class to support function shared between various
    // built-in material classes.
    class BuiltInMaterialClass : public MaterialClass
    {
    public:
        using MinTextureFilter = Texture::MinFilter;
        using MagTextureFilter = Texture::MagFilter;
        using TextureWrapping  = Texture::Wrapping;
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
        BuiltInMaterialClass()
        { mClassId = base::RandomString(10); }
        void SetGamma(float gamma)
        { mGamma = gamma; }
        void SetStatic(bool on_off)
        { mStatic = on_off; }
        bool IsStatic() const
        { return mStatic; }
        float GetGamma() const
        { return mGamma; }
        virtual std::string GetId() const override
        { return mClassId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual void SetId(const std::string& id) override
        { mClassId = id; }
        virtual void SetName(const std::string& name) override
        { mName = name; }
        virtual void SetSurfaceType(SurfaceType surface) override
        { mSurfaceType = surface; }
        virtual SurfaceType GetSurfaceType() const override
        { return mSurfaceType; }
        virtual void BeginPacking(TexturePacker* packer) const override
        { /* empty */ }
        virtual void FinishPacking(const TexturePacker* packer) override
        { /*empty */ }
        virtual bool TestFlag(Flags flag) const override
        { return mFlags.test(flag); }
        virtual void SetFlag(Flags flag, bool on_off) override
        { mFlags.set(flag, on_off); }
    protected:
        std::string mClassId;
        std::string mName;
        SurfaceType mSurfaceType = SurfaceType::Opaque;
        float mGamma  = 1.0f;
        bool mStatic = false;
        base::bitflag<Flags> mFlags;
    };

    // Shade surfaces using a simple solid color shader only.
    class ColorClass : public BuiltInMaterialClass
    {
    public:
        ColorClass() = default;
        ColorClass(const Color4f& color)
          : mColor(color)
        {}
        void SetBaseColor(const Color4f& color)
        { mColor = color; }
        void SetBaseAlpha(float alpha)
        { mColor.SetAlpha(alpha); }
        float GetBaseAlpha() const
        { return mColor.Alpha(); }
        const Color4f& GetBaseColor() const
        { return mColor; }
        virtual Type GetType() const override { return Type::Color; }
        virtual Shader* GetShader(const State& state, Device& device) const override;
        virtual std::size_t GetHash() const override;
        virtual std::string GetProgramId(const State& state) const override;
        virtual std::unique_ptr<MaterialClass> Copy() const override
        { return std::make_unique<ColorClass>(*this); }
        virtual std::unique_ptr<MaterialClass> Clone() const override
        {
            auto ret = std::make_unique<ColorClass>(*this);
            ret->mClassId = base::RandomString(10);
            return ret;
        }
        virtual void ApplyDynamicState(const State& state, Device& device, Program& program) const override;
        virtual void ApplyStaticState(const State& state, Device& device, Program& program) const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
    private:
        Color4f mColor = gfx::Color::White;
    };

    // Shade surfaces using a color gradient that smoothly
    // interpolates between multiple colors.
    class GradientClass : public BuiltInMaterialClass
    {
    public:
        // The four supported colors are all identified by
        // a color index that maps to the 4 quadrants of the
        // texture coordinate space.
        enum class ColorIndex {
            TopLeft,    TopRight,
            BottomLeft, BottomRight
        };
        void SetColor(const Color4f& color, ColorIndex index)
        {
            if (index == ColorIndex::TopLeft)
                mColorMap[0] = color;
            else if (index == ColorIndex::TopRight)
                mColorMap[1] = color;
            else if (index == ColorIndex::BottomLeft)
                mColorMap[2] = color;
            else if (index == ColorIndex::BottomRight)
                mColorMap[3] = color;
            else BUG("incorrect color index");
        }
        Color4f GetColor(ColorIndex index) const
        {
            if (index == ColorIndex::TopLeft)
                return mColorMap[0];
            else if (index == ColorIndex::TopRight)
                return mColorMap[1];
            else if (index == ColorIndex::BottomLeft)
                return mColorMap[2];
            else if (index == ColorIndex::BottomRight)
                return mColorMap[3];
            else BUG("incorrect color index");
            return Color4f();
        }
        glm::vec2 GetOffset() const
        { return mOffset; }
        void SetOffset(const glm::vec2& offset)
        { mOffset = offset; }
        virtual Type GetType() const override { return Type::Gradient; }
        virtual Shader* GetShader(const State& state, Device& device) const override;
        virtual std::size_t GetHash() const override;
        virtual std::string GetProgramId(const State&) const override;
        virtual std::unique_ptr<MaterialClass> Copy() const override
        { return std::make_unique<GradientClass>(*this); }
        virtual std::unique_ptr<MaterialClass> Clone() const override
        {
            auto ret = std::make_unique<GradientClass>(*this);
            ret->mClassId = base::RandomString(10);
            return ret;
        }
        virtual void ApplyDynamicState(const State& state, Device& device, Program& program) const override;
        virtual void ApplyStaticState(const State& state, Device& device, Program& program) const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
    private:
        Color4f mColorMap[4] = {gfx::Color::White, gfx::Color::White,
                                gfx::Color::White, gfx::Color::White};
        glm::vec2 mOffset = {0.5f, 0.5f};
    };

    // Shade surfaces using a sprite animation, i.e. by cycling through
    // a series of textures which are sampled and blended together
    // in order to create a smoothed animation.
    class SpriteClass : public BuiltInMaterialClass
    {
    public:
        SpriteClass() = default;
        SpriteClass(const SpriteClass& other, bool copy);
        SpriteClass(const SpriteClass& other) : SpriteClass(other, true)
        {}
        void ResetTextures()
        { mSprite.ResetTextures(); }
        void AddTexture(std::unique_ptr<TextureSource> source)
        { mSprite.AddTexture(std::move(source)); }
        void AddTexture(std::unique_ptr<TextureSource> source, const FRect& rect)
        { mSprite.AddTexture(std::move(source), rect); }
        void DeleteTexture(size_t index)
        { mSprite.DeleteTexture(index); }
        void DeleteTextureById(const std::string& id)
        { mSprite.DeleteTextureById(id); }
        const TextureSource* GetTextureSource(size_t index) const
        { return mSprite.GetTextureSource(index); }
        const TextureSource * FindTextureSourceById(const std::string& id) const
        { return mSprite.FindTextureSourceById(id); }
        const TextureSource* FindTextureSourceByName(const std::string& name) const
        { return mSprite.FindTextureSourceByName(name); }
        TextureSource* GetTextureSource(size_t index)
        { return mSprite.GetTextureSource(index); }
        TextureSource* FindTextureSourceById(const std::string& id)
        { return mSprite.FindTextureSourceById(id); }
        TextureSource* FindTextureSourceByName(const std::string& name)
        { return mSprite.FindTextureSourceByName(name); }

        void SetBaseColor(const Color4f& color)
        { mBaseColor = color; }
        void SetTextureRect(std::size_t index, const FRect& rect)
        { mSprite.SetTextureRect(index, rect); }
        void SetTextureSource(std::size_t index, std::unique_ptr<TextureSource> source)
        {  mSprite.SetTextureSource(index, std::move(source)); }
        void SetTextureMinFilter(MinTextureFilter filter)
        { mMinFilter = filter; }
        void SetTextureMagFilter(MagTextureFilter filter)
        { mMagFilter = filter; }
        void SetTextureWrapX(TextureWrapping wrap)
        { mWrapX = wrap; }
        void SetTextureWrapY(TextureWrapping wrap)
        { mWrapY = wrap; }
        void SetTextureScaleX(float scale)
        { mTextureScale.x = scale; }
        void SetTextureScaleY(float scale)
        { mTextureScale.y = scale; }
        void SetTextureScale(const glm::vec2& scale)
        { mTextureScale = scale; }
        void SetTextureVelocityX(float x)
        { mTextureVelocity.x = x; }
        void SetTextureVelocityY(float y)
        { mTextureVelocity.y = y; }
        void SetTextureVelocityZ(float angle_radians)
        { mTextureVelocity.z = angle_radians; }
        void SetTextureRotation(float angle_radians)
        { mTextureRotation = angle_radians; }
        void SetTextureVelocity(const glm::vec2 linear, float radial)
        { mTextureVelocity = glm::vec3(linear, radial); }
        void SetFps(float fps)
        { mSprite.SetFps(fps); }
        void SetBlendFrames(float on_off)
        { mBlendFrames = on_off; }
        void SetLooping(bool on_off)
        { mSprite.SetLooping(on_off); }
        void SetParticleAction(ParticleAction action)
        { mParticleAction = action; }
        gfx::Color4f GetBaseColor() const
        { return mBaseColor; }
        gfx::FRect GetTextureRect(size_t index) const
        { return mSprite.GetTextureRect(index); }
        size_t GetNumTextures() const
        { return mSprite.GetNumTextures(); }
        bool GetBlendFrames() const
        { return mBlendFrames; }
        bool IsLooping() const
        { return mSprite.IsLooping(); }
        float GetFps() const
        { return mSprite.GetFps(); }
        float GetTextureScaleX() const
        { return mTextureScale.x; }
        float GetTextureScaleY() const
        { return mTextureScale.y; }
        float GetTextureVelocityX() const
        { return mTextureVelocity.x; }
        float GetTextureVelocityY() const
        { return mTextureVelocity.y; }
        float GetTextureVelocityZ() const
        { return mTextureVelocity.z; }
        float GetTextureRotation() const
        { return mTextureRotation; }
        MinTextureFilter GetTextureMinFilter() const
        { return mMinFilter; }
        MagTextureFilter GetTextureMagFilter() const
        { return mMagFilter; }
        TextureWrapping  GetTextureWrapX() const
        { return mWrapX; }
        TextureWrapping GetTextureWrapY() const
        { return mWrapY; }
        ParticleAction GetParticleAction() const
        { return mParticleAction; }

        virtual Type GetType() const override { return Type::Sprite; }
        virtual Shader* GetShader(const State& state, Device& device) const override;
        virtual std::size_t GetHash() const override;
        virtual std::string GetProgramId(const State&) const override;
        virtual std::unique_ptr<MaterialClass> Copy() const override;
        virtual std::unique_ptr<MaterialClass> Clone() const override;
        virtual void ApplyDynamicState(const State& state, Device& device, Program& program) const override;
        virtual void ApplyStaticState(const State& state, Device& device, Program& program) const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
        virtual void BeginPacking(TexturePacker* packer) const override;
        virtual void FinishPacking(const TexturePacker* packer) override;
        SpriteClass& operator=(const SpriteClass&);
    private:
        bool mBlendFrames = true;
        gfx::Color4f mBaseColor = gfx::Color::White;
        glm::vec2 mTextureScale = {1.0f, 1.0f};
        glm::vec3 mTextureVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
        float mTextureRotation = 0.0f;
        MinTextureFilter mMinFilter = MinTextureFilter::Default;
        MagTextureFilter mMagFilter = MagTextureFilter::Default;
        TextureWrapping mWrapX = TextureWrapping::Clamp;
        TextureWrapping mWrapY = TextureWrapping::Clamp;
        ParticleAction mParticleAction = ParticleAction::None;
        SpriteMap mSprite;
    };

    // Shade surfaces by sampling a single static texture
    // and optionally modulate the output by a base color.
    // Supports also alpha masks (i.e. 8bit textures with
    // alpha channel only), so thus can be used with e.g.
    // noise and text textures.
    class TextureMap2DClass : public BuiltInMaterialClass
    {
    public:
        TextureMap2DClass() = default;
        TextureMap2DClass(const TextureMap2DClass& other, bool copy);
        TextureMap2DClass(const TextureMap2DClass& other) : TextureMap2DClass(other, true)
        {}
        void SetTexture(std::unique_ptr<TextureSource> source)
        { mTexture.SetTexture(std::move(source)); }
        void SetTexture(std::unique_ptr<TextureSource> source, const FRect& rect)
        { mTexture.SetTexture(std::move(source), rect); }
        void SetTextureRect(const FRect& rect)
        { mTexture.SetTextureRect(rect); }
        void SetTextureMinFilter(MinTextureFilter filter)
        { mMinFilter = filter; }
        void SetTextureMagFilter(MagTextureFilter filter)
        { mMagFilter = filter; }
        void SetTextureWrapX(TextureWrapping wrap)
        { mWrapX = wrap; }
        void SetTextureWrapY(TextureWrapping wrap)
        { mWrapY = wrap; }
        void SetTextureScaleX(float scale)
        { mTextureScale.x = scale; }
        void SetTextureScaleY(float scale)
        { mTextureScale.y = scale; }
        void SetTextureScale(const glm::vec2& scale)
        { mTextureScale = scale; }
        void SetTextureVelocityX(float x)
        { mTextureVelocity.x = x; }
        void SetTextureVelocityY(float y)
        { mTextureVelocity.y = y; }
        void SetTextureVelocityZ(float angle_radians)
        { mTextureVelocity.z = angle_radians; }
        void SetTextureRotation(float angle_radians)
        { mTextureRotation = angle_radians; }
        void SetTextureVelocity(const glm::vec2 linear, float radial)
        { mTextureVelocity = glm::vec3(linear, radial); }
        void SetBaseColor(const Color4f& color)
        { mBaseColor = color; }
        void SetParticleAction(ParticleAction action)
        { mParticleAction = action; }
        void ResetTexture()
        { mTexture.ResetTexture(); }
        gfx::FRect GetTextureRect() const
        { return mTexture.GetTextureRect(); }
        TextureSource* GetTextureSource()
        { return mTexture.GetTextureSource(); }
        const TextureSource* GetTextureSource() const
        { return mTexture.GetTextureSource(); }
        float GetTextureScaleX() const
        { return mTextureScale.x; }
        float GetTextureScaleY() const
        { return mTextureScale.y; }
        float GetTextureVelocityX() const
        { return mTextureVelocity.x; }
        float GetTextureVelocityY() const
        { return mTextureVelocity.y; }
        float GetTextureVelocityZ() const
        { return mTextureVelocity.z; }
        float GetTextureRotation() const
        { return mTextureRotation; }
        MinTextureFilter GetTextureMinFilter() const
        { return mMinFilter; }
        MagTextureFilter GetTextureMagFilter() const
        { return mMagFilter; }
        TextureWrapping  GetTextureWrapX() const
        { return mWrapX; }
        TextureWrapping GetTextureWrapY() const
        { return mWrapY; }
        Color4f GetBaseColor() const
        { return mBaseColor; }
        ParticleAction GetParticleAction() const
        { return mParticleAction; }
        virtual Type GetType() const override { return Type::Texture; }
        virtual Shader* GetShader(const State& state, Device& device) const override;
        virtual std::size_t GetHash() const override;
        virtual std::string GetProgramId(const State&) const override;
        virtual std::unique_ptr<MaterialClass> Copy() const override;
        virtual std::unique_ptr<MaterialClass> Clone() const override;
        virtual void ApplyDynamicState(const State& state, Device& device, Program& program) const override;
        virtual void ApplyStaticState(const State&, Device& device, Program& program) const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
        virtual void BeginPacking(TexturePacker* packer) const override;
        virtual void FinishPacking(const TexturePacker* packer) override;
        TextureMap2DClass& operator=(const TextureMap2DClass&);
    private:
        gfx::Color4f mBaseColor;
        glm::vec2 mTextureScale = {1.0f, 1.0f};
        glm::vec3 mTextureVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
        float mTextureRotation = 0.0f;
        MinTextureFilter mMinFilter = MinTextureFilter::Default;
        MagTextureFilter mMagFilter = MagTextureFilter::Default;
        TextureWrapping mWrapX = TextureWrapping::Clamp;
        TextureWrapping mWrapY = TextureWrapping::Clamp;
        ParticleAction mParticleAction = ParticleAction::None;
        TextureMap2D mTexture;
    };

    // Shade surfaces using an arbitrary user defined shading
    // algorithm. The material can be configured with the
    // shader URI that defines the shader GLSL file and with
    // an arbitrary set of uniforms and texture maps.
    class CustomMaterialClass : public MaterialClass
    {
    public:
        CustomMaterialClass()
        { mClassId = base::RandomString(10); }
        CustomMaterialClass(const CustomMaterialClass& other, bool copy);
        CustomMaterialClass(const CustomMaterialClass& other) : CustomMaterialClass(other, true) {}

        void SetTextureMinFilter(MinTextureFilter filter)
        { mMinFilter = filter; }
        void SetTextureMagFilter(MagTextureFilter filter)
        { mMagFilter = filter; }
        void SetTextureWrapX(TextureWrapping wrap)
        { mWrapX = wrap; }
        void SetTextureWrapY(TextureWrapping wrap)
        { mWrapY = wrap; }
        void SetShaderUri(const std::string& uri)
        { mShaderUri = uri; }
        void SetShaderSrc(const std::string& src)
        { mShaderSrc = src; }
        void SetUniform(const std::string& name, const Uniform& value)
        { mUniforms[name] = value; }
        void SetUniform(const std::string& name, Uniform&& value)
        { mUniforms[name] = std::move(value); }
        Uniform* FindUniform(const std::string& name)
        { return base::SafeFind(mUniforms, name); }
        const Uniform* FindUniform(const std::string& name) const
        { return base::SafeFind(mUniforms, name); }
        template<typename T>
        T* GetUniformValue(const std::string& name)
        {
            if (auto* ptr = base::SafeFind(mUniforms, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        template<typename T>
        const T* GetUniformValue(const std::string& name) const
        {
            if (auto* ptr = base::SafeFind(mUniforms, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        void DeleteUniforms()
        { mUniforms.clear(); }
        void DeleteUniform(const std::string& name)
        { mUniforms.erase(name); }
        bool HasUniform(const std::string& name) const
        { return base::SafeFind(mUniforms, name) != nullptr; }
        template<typename T>
        bool HasUniform(const std::string& name) const
        {
            if (const auto* ptr = base::SafeFind(mUniforms, name))
                return std::get_if<T>(ptr) != nullptr;
            return false;
        }
        UniformMap GetUniforms() const
        { return mUniforms; }
        std::string GetShaderUri() const
        { return mShaderUri; }
        std::string GetShaderSrc() const
        { return mShaderSrc; }
        MinTextureFilter GetTextureMinFilter() const
        { return mMinFilter; }
        MagTextureFilter GetTextureMagFilter() const
        { return mMagFilter; }
        TextureWrapping  GetTextureWrapX() const
        { return mWrapX; }
        TextureWrapping GetTextureWrapY() const
        { return mWrapY; }
        void SetTextureMap(const std::string& name, std::unique_ptr<TextureMap> map)
        { mTextureMaps[name] = std::move(map); }
        void SetTextureMap(const std::string& name, const TextureMap& map)
        { mTextureMaps[name] = map.Copy(); }
        void DeleteTextureMaps()
        { mTextureMaps.clear(); }
        void DeleteTextureMap(const std::string& name)
        { mTextureMaps.erase(name); }
        bool HasTextureMap(const std::string& name) const
        { return base::SafeFind(mTextureMaps, name) != nullptr; }
        TextureMap* FindTextureMap(const std::string& name)
        { return base::SafeFind(mTextureMaps, name); }
        const TextureMap* FindTextureMap(const std::string& name) const
        { return base::SafeFind(mTextureMaps, name); }
        // helper functions to access the texture sources directly.
        TextureSource* FindTextureSourceById(const std::string& id);
        TextureSource* FindTextureSourceByName(const std::string& name);
        const TextureSource* FindTextureSourceById(const std::string& id) const;
        const TextureSource* FindTextureSourceByName(const std::string& name) const;
        gfx::FRect FindTextureSourceRect(const TextureSource* source) const;
        void SetTextureSourceRect(const TextureSource* source, const FRect& rect);
        void DeleteTextureSource(const TextureSource* source);

        std::unordered_set<std::string> GetTextureMapNames() const;
        virtual void SetSurfaceType(SurfaceType surface) override
        { mSurfaceType = surface; }
        virtual void SetId(const std::string& id) override
        { mClassId = id; }
        virtual void SetName(const std::string& name) override
        { mName = name; }
        virtual Type GetType() const override { return Type::Custom; }
        virtual SurfaceType GetSurfaceType() const override { return mSurfaceType; }
        virtual Shader* GetShader(const State& state, Device& device) const override;
        virtual std::size_t GetHash() const override;
        virtual std::string GetId() const override { return mClassId; }
        virtual std::string GetName() const override { return mName; }
        virtual std::string GetProgramId(const State&) const override;
        virtual std::unique_ptr<MaterialClass> Copy() const override;
        virtual std::unique_ptr<MaterialClass> Clone() const override;
        virtual void ApplyDynamicState(const State& state, Device& device, Program& program) const override;
        virtual void ApplyStaticState(const State&, Device& device, Program& program) const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
        virtual void BeginPacking(TexturePacker* packer) const override;
        virtual void FinishPacking(const TexturePacker* packer) override;
        virtual bool TestFlag(Flags flag) const override
        { return mFlags.test(flag); }
        virtual void SetFlag(Flags flag, bool on_off) override
        { mFlags.set(flag, on_off); }
        CustomMaterialClass& operator=(const CustomMaterialClass& other);
    private:
        std::string mClassId;
        std::string mName;
        std::string mShaderUri;
        std::string mShaderSrc;
        std::unordered_map<std::string, Uniform> mUniforms;
        SurfaceType mSurfaceType = SurfaceType::Opaque;
        MinTextureFilter mMinFilter = MinTextureFilter::Default;
        MagTextureFilter mMagFilter = MagTextureFilter::Default;
        TextureWrapping mWrapX = TextureWrapping::Clamp;
        TextureWrapping mWrapY = TextureWrapping::Clamp;
        std::unordered_map<std::string, std::unique_ptr<TextureMap>> mTextureMaps;
        base::bitflag<Flags> mFlags;
    };

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
            const ShaderPass* render_pass = nullptr;
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
        // and set the rasterizer state.
        // Dynamic properties are the properties that can change between
        // one material instance to another even when the underlying
        // type/material class is the same.
        virtual void ApplyDynamicState(const Environment& env, Device& device, Program& program, RasterState& raster) const = 0;
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
        virtual void SetRuntime(float runtime) = 0;
        // Set material instance uniform.
        virtual void SetUniform(const std::string& name, const Uniform& value) = 0;
        // Set material instance uniform.
        virtual void SetUniform(const std::string& name, Uniform&& value) = 0;
        // Set all material uniforms at once.
        virtual void SetUniforms(const UniformMap& uniforms) = 0;
        // Clear away all material instance uniforms.
        virtual void ResetUniforms() = 0;
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
        {
            mClass   = klass.Copy();
            mRuntime = time;
        }

        // Apply the material properties to the given program object and set the rasterizer state.
        virtual void ApplyDynamicState(const Environment& env, Device& device, Program& program, RasterState& raster) const override;
        virtual Shader* GetShader(const Environment& env, Device& device) const override;
        virtual void ApplyStaticState(const Environment& env, Device& device, Program& program) const override;
        virtual std::string GetProgramId(const Environment& env) const override;

        virtual std::string GetClassId() const override
        { return mClass->GetId(); }
        virtual void Update(float dt) override
        { mRuntime += dt; }
        virtual void SetRuntime(float runtime) override
        { mRuntime = runtime; }
        virtual void SetUniform(const std::string& name, const Uniform& value) override
        { mUniforms[name] = value; }
        virtual void SetUniform(const std::string& name, Uniform&& value) override
        { mUniforms[name] = std::move(value); }
        virtual void ResetUniforms()  override
        { mUniforms.clear(); }
        virtual void SetUniforms(const UniformMap& uniforms) override
        { mUniforms = uniforms; }

        double GetRuntime() const
        { return mRuntime; }

        // Get the material class object instance.
        const MaterialClass& GetClass() const
        { return *mClass; }
        // Shortcut operator for accessing the class object instance.
        const MaterialClass* operator->() const
        { return mClass.get(); }
    private:
        // This is the "class" object for this material type.
        std::shared_ptr<const MaterialClass> mClass;
        // Current runtime for this material instance.
        double mRuntime = 0.0f;
        // material properties (uniforms) specific to this instance.
        UniformMap mUniforms;
    };

    // material specialized for rendering text using
    // a pre-rasterized bitmap of text. Creates transient
    // texture objects for the text.
    class TextMaterial : public gfx::Material
    {
    public:
        TextMaterial(const TextBuffer& text);
        TextMaterial(TextBuffer&& text);
        virtual void ApplyDynamicState(const Environment& env, Device& device, Program& program, RasterState& raster) const override;
        virtual void ApplyStaticState(const Environment& env, Device& device, Program& program) const override;
        virtual Shader* GetShader(const Environment& env, Device& device) const override;
        virtual std::string GetProgramId(const Environment&) const override;
        virtual std::string GetClassId() const override;
        virtual void Update(float dt) override;
        virtual void SetRuntime(float runtime) override;
        virtual void SetUniform(const std::string& name, const Uniform& value) override;
        virtual void SetUniform(const std::string& name, Uniform&& value) override;
        virtual void ResetUniforms()  override;
        virtual void SetUniforms(const UniformMap& uniforms) override;
        void SetColor(const Color4f& color)
        { mColor = color; }
        // Set point sampling to true in order to use a fast filtering
        // when sampling from the texture. This should be for maximum perf
        // ideally when the geometry to be drawn matches closely with the
        // rasterized text texture/buffer. So when the texture maps onto
        // a rectangle and there's now transformation that would change the
        // rasterized dimensions (in pixels) of the rectangle from the
        // dimensions of the rasterized text texture.
        // The default is true.
        void SetPointSampling(bool on_off)
        { mPointSampling = on_off; }
    private:
        TextBuffer mText;
        Color4f mColor = Color::White;
        bool mPointSampling = true;
    };
    // Create gradient material based on 4 colors
    GradientClass CreateMaterialClassFromColor(const Color4f& top_left,
                                               const Color4f& top_right,
                                               const Color4f& bottom_left,
                                               const Color4f& bottom_right);

    // Create material based on a simple color only.
    ColorClass CreateMaterialClassFromColor(const Color4f& color);
    // Create a material based on a 2D texture map.
    TextureMap2DClass CreateMaterialClassFromTexture(const std::string& uri);
    // Create a sprite from multiple textures.
    SpriteClass CreateMaterialClassFromSprite(const std::initializer_list<std::string>& textures);
    // Create a sprite from multiple textures.
    SpriteClass CreateMaterialClassFromSprite(const std::vector<std::string>& textures);
    // Create a sprite from a texture atlas where all the sprite frames
    // are packed inside the single texture.
    SpriteClass CreateMaterialClassFromSpriteAtlas(const std::string& texture, const std::vector<FRect>& frames);
    // Create a material class from a text buffer.
    TextureMap2DClass CreateMaterialClassFromText(const TextBuffer& text);

    inline MaterialClassInst CreateMaterialFromColor(const Color4f& top_left,
                                                     const Color4f& top_right,
                                                     const Color4f& bottom_left,
                                                     const Color4f& bottom_right)
    { return MaterialClassInst(CreateMaterialClassFromColor(top_left, top_right, bottom_left, bottom_right)); }
    inline MaterialClassInst CreateMaterialFromColor(const Color4f& color)
    { return MaterialClassInst(CreateMaterialClassFromColor(color)); }
    inline MaterialClassInst CreateMaterialFromTexture(const std::string& uri)
    { return MaterialClassInst(CreateMaterialClassFromTexture(uri)); }
    inline MaterialClassInst CreateMaterialFromSprite(const std::initializer_list<std::string>& textures)
    { return MaterialClassInst(CreateMaterialClassFromSprite(textures)); }
    inline MaterialClassInst CreateMaterialFromSprite(const std::vector<std::string>& textures)
    { return MaterialClassInst(CreateMaterialClassFromSprite(textures)); }
    inline MaterialClassInst CreateMaterialFromSpriteAtlas(const std::string& texture, const std::vector<FRect>& frames)
    { return MaterialClassInst(CreateMaterialClassFromSpriteAtlas(texture, frames)); }
    inline MaterialClassInst CreateMaterialFromText(const TextBuffer& text)
    { return MaterialClassInst(CreateMaterialClassFromText(text)); }

    std::unique_ptr<Material> CreateMaterialInstance(const MaterialClass& klass);
    std::unique_ptr<Material> CreateMaterialInstance(const std::shared_ptr<const MaterialClass>& klass);
    std::unique_ptr<TextMaterial> CreateMaterialInstance(const TextBuffer& text);
    std::unique_ptr<TextMaterial> CreateMaterialInstance(TextBuffer&& text);

} // namespace
