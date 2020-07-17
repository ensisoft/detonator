// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <neargye/magic_enum.hpp>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <map>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <functional> // for hash
#include <cmath>
#include <cassert>
#include <optional>

#include "base/utility.h"
#include "base/assert.h"
#include "graphics/texture.h"
#include "graphics/resource.h"
#include "graphics/bitmap.h"
#include "graphics/color4f.h"
#include "graphics/device.h"
#include "graphics/image.h"
#include "graphics/types.h"
#include "graphics/text.h"

namespace gfx
{
    class Device;
    class Shader;

    // Abstract interface for acquiring the actual texture data.
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
            // Data comes as an arbitrary bitmap that the user can manipulate
            // as per pixel by pixel basis.
            Bitmap
        };

        virtual ~TextureSource() = default;
        // Get the type of the source of the texture data.
        virtual Source GetSourceType() const = 0;
        // Get the name for the texture for the device.
        // This is expected to be unique and is used to map the source to a
        // device texture resource. Not human readable.
        virtual std::string GetId() const = 0;
        // Get the human readable / and settable name.
        virtual std::string GetName() const = 0;
        // Set the texture source human readable name.
        virtual void SetName(const std::string& name) = 0;
        // Generate or load the data as a bitmap. If there's a content
        // error this function should return empty shared pointer.
        // The returned bitmap can be potentially shared but in an immutable
        // fashion.
        virtual std::shared_ptr<IBitmap> GetData() const = 0;
        // Clone this texture source.
        virtual std::unique_ptr<TextureSource> Clone() const = 0;
        // Returns whether texture source can serialize into JSON or not.
        virtual bool CanSerialize() const
        { return false; }
        // Serialize into JSON object.
        virtual nlohmann::json ToJson() const
        { return nlohmann::json{}; }
        // Load state from JSON object. Returns true if succesful
        // otherwise false.
        virtual bool FromJson(const nlohmann::json& object)
        { return false; }
        // Begin packing the texture source into the packer.
        virtual void BeginPacking(ResourcePacker* packer) const
        {}
        // Finish packing the texture source into the packer.
        // Update the state with the details from the packer.
        virtual void FinishPacking(const ResourcePacker* packer)
        {}
    private:
    };

    namespace detail {
        // Source texture data from an image file.
        class TextureFileSource : public TextureSource
        {
        public:
            TextureFileSource(const std::string& file,
                              const std::string& name)
              : mFile(file)
              , mName(name)
            {}
            TextureFileSource() = default;
            virtual Source GetSourceType() const override
            { return Source::Filesystem; }
            virtual std::string GetId() const override
            { return mFile; }
            virtual std::string GetName() const override
            { return mName; }
            virtual void SetName(const std::string& name)
            { mName = name; }

            virtual std::shared_ptr<IBitmap> GetData() const override;

            virtual std::unique_ptr<TextureSource> Clone() const override
            { return std::make_unique<TextureFileSource>(*this); }

            virtual bool CanSerialize() const override
            { return true; }
            virtual nlohmann::json ToJson() const override
            {
                nlohmann::json json = {
                    {"file", mFile},
                    {"name", mName}
                };
                return json;
            }
            virtual bool FromJson(const nlohmann::json& object) override
            {
                return base::JsonReadSafe(object, "file", &mFile) &&
                       base::JsonReadSafe(object, "name", &mName);
            }
            virtual void BeginPacking(ResourcePacker* packer) const override
            {
                packer->PackTexture(this, mFile);
            }
            virtual void FinishPacking(const ResourcePacker* packer) override
            {
                mFile = packer->GetPackedTextureId(this);
            }

            const std::string& GetFilename() const
            { return mFile; }
        private:
            std::string mFile;
            std::string mName;
        private:
        };

        // Source texture data from a bitmap
        class TextureBitmapSource : public TextureSource
        {
        public:
            template<typename T>
            TextureBitmapSource(const Bitmap<T>& bmp)
              : mBitmap(new Bitmap<T>(bmp))
            {}
            template<typename T>
            TextureBitmapSource(Bitmap<T>&& bmp)
              : mBitmap(new Bitmap<T>(std::move(bmp)))
            {}
            TextureBitmapSource() = default;
            TextureBitmapSource(const TextureBitmapSource& other)
            {
                mName = other.mName;
                if (other.mBitmap)
                    mBitmap = other.mBitmap->Clone();
            }

            virtual Source GetSourceType() const override
            { return Source::Bitmap; }
            virtual std::string GetId() const override
            {
                size_t hash = mBitmap->GetHash();
                hash = base::hash_combine(hash, mBitmap->GetWidth());
                hash = base::hash_combine(hash, mBitmap->GetHeight());
                return std::to_string(hash);
            }
            virtual std::string GetName() const override
            { return mName; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual std::shared_ptr<IBitmap> GetData() const override
            { return mBitmap; }
            virtual std::unique_ptr<TextureSource> Clone() const override
            { return std::make_unique<TextureBitmapSource>(*this); }
        private:
            std::shared_ptr<IBitmap> mBitmap;
            std::string mName;
        };

        // Rasterize text buffer and provide as a texture source.
        class TextureTextBufferSource : public TextureSource
        {
        public:
            TextureTextBufferSource(const TextBuffer& text)
              : mTextBuffer(text)
            {}
            TextureTextBufferSource(const TextBuffer& text, const std::string& name)
              : mTextBuffer(text)
              , mName(name)
            {}
            TextureTextBufferSource(TextBuffer&& text)
              : mTextBuffer(std::move(text))
            {}
            TextureTextBufferSource(TextBuffer&& text, std::string&& name)
              : mTextBuffer(std::move(text))
              , mName(std::move(name))
            {}
            TextureTextBufferSource() = default;

            virtual Source GetSourceType() const override
            { return Source::TextBuffer; }
            virtual std::string GetId() const override
            {
                size_t hash = mTextBuffer.GetHash();
                hash = base::hash_combine(hash, mTextBuffer.GetWidth());
                hash = base::hash_combine(hash, mTextBuffer.GetHeight());
                return std::to_string(hash);
            }
            virtual std::string GetName() const override
            { return mName; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual std::shared_ptr<IBitmap> GetData() const override;

            virtual std::unique_ptr<TextureSource> Clone() const override
            { return std::make_unique<TextureTextBufferSource>(*this); }

            virtual bool CanSerialize() const override
            { return true; }
            virtual nlohmann::json ToJson() const override
            {
                nlohmann::json json;
                json["name"] = mName;
                json["buffer"] = mTextBuffer.ToJson();
                return json;
            }
            virtual bool FromJson(const nlohmann::json& json) override
            {
                if (!json.contains("buffer") || !json["buffer"].is_object())
                    return false;
                if (!base::JsonReadSafe(json, "name", &mName))
                    return false;
                auto ret = TextBuffer::FromJson(json["buffer"]);
                if (!ret.has_value())
                    return false;
                mTextBuffer = std::move(ret.value());
                return true;
            }
            virtual void BeginPacking(ResourcePacker* packer) const override
            {
                const size_t num_texts = mTextBuffer.GetNumTexts();
                for (size_t i=0; i<num_texts; ++i)
                {
                    const auto& text = mTextBuffer.GetText(i);
                    packer->PackFont(&text, text.font);
                }
            }
            virtual void FinishPacking(const ResourcePacker* packer) override
            {
                const size_t num_texts = mTextBuffer.GetNumTexts();
                for (size_t i=0; i<num_texts; ++i)
                {
                    auto& text = mTextBuffer.GetText(i);
                    text.font  = packer->GetPackedFontId(&text);
                }
            }

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
            std::string mName;
        };

    } // detail

    // Material objects hold the material parameters per material instance
    // and apply the parameters to the program object and rasterizer state.
    class Material
    {
    public:
        enum class SurfaceType {
            // Surface is opaque and no blending is done.
            Opaque,
            // Surface is transparent and is blended with the destination
            // to create a interpolated mix of the colors.
            Transparent,
            // Surface gives off color (light)
            Emissive
        };

        // The functional type of the material. The functional
        // type groups similar materials into categories with common
        // set of properties. Then the each specific shader will provide
        // the actual implementation using those properties.
        enum class Type {
            // Material is using color(s) only.
            Color,
            // Material uses a static texture only.
            Texture,
            // Material uses a series of textures to create
            // a sprite animation.
            Sprite
        };

        using MinTextureFilter = Texture::MinFilter;
        using MagTextureFilter = Texture::MagFilter;
        using TextureWrapping  = Texture::Wrapping;

        // constructor.
        Material(Type type) : mType(type)
        {}
        // allow "invalid" material to be constructed.
        Material() = default;

        // Make a deep copy of the material.
        Material(const Material& other);

        // Create the shader for this material on the given device.
        // Returns the new shader object or nullptr if the shader
        // failed to compile.
        Shader* GetShader(Device& device) const;

        // Environmental parameters such as lights etc.
        struct Environment {
            bool render_points = false;
        };

        // Rasterizer state that the material can manipulate.
        struct RasterState {
            using Blending = Device::State::BlendOp;
            Blending blending = Blending::None;
        };

        // Apply the material properties to the given program object
        // and set the rasterizer state.
        void Apply(const Environment& env, Device& device, Program& prog, RasterState& state) const;

        // Material properties getters.

        std::string GetShaderName() const
        {
            const std::size_t hash = std::hash<std::string>()(GetShaderFile());
            return std::to_string(hash);
        }

        std::string GetShaderFile() const
        {
            // check if have a user defined specific shader or not.
            if (!mShaderFile.empty())
                return mShaderFile;

            if (mType == Type::Color)
                return "shaders/es2/solid_color.glsl";
            else if (mType == Type::Texture)
                return "shaders/es2/texture_map.glsl";
            else if (mType == Type::Sprite)
                return "shaders/es2/texture_map.glsl";
            ASSERT(!"???");
            return "";
        }

        bool GetBlendFrames() const
        { return mBlendFrames; }
        float GetTextureScaleX() const
        { return mTextureScale.x; }
        float GetTextureScaleY() const
        { return mTextureScale.y; }
        float GetTextureVelocityX() const
        { return mTextureVelocity.x; }
        float GetTextureVelocityY() const
        { return mTextureVelocity.y; }
        float GetGamma() const
        { return mGamma; }
        float GetFps() const
        { return mFps; }
        MinTextureFilter GetMinTextureFilter() const
        { return mMinFilter; }
        MagTextureFilter GetMagTextureFilter() const
        { return mMagFilter; }
        TextureWrapping GetTextureWrapX() const
        { return mWrapX; }
        TextureWrapping GetTextureWrapY() const
        { return mWrapY; }
        const Color4f& GetBaseColor() const
        { return mBaseColor; }
        SurfaceType GetSurfaceType() const
        { return mSurfaceType; }
        Type GetType() const
        { return mType; }

        // Material properties setters.

        // Set the material to use a specific shader.
        Material& SetShaderFile(const std::string& shader_file)
        {
            mShaderFile = shader_file;
            return *this;
        }

        // Set the functional material type that describes approximately
        // how the material should behave. The final output depends
        // on the actual shader being used.
        Material& SetType(Type type)
        {
            mType = type;
            return *this;
        }

        // Set whether to cut sharply between animation frames or
        // whether to smooth the transition by blending adjacent frames
        // together when in-between frames.
        Material& SetBlendFrames(bool on_off)
        {
            mBlendFrames = on_off;
            return *this;
        }

        // Set the gamma (in)correction value.
        // Values below 1.0f will result in the rendered image
        // being "brighter" and above 1.0f will make it "darker".
        // The default is 1.0f
        Material& SetGamma(float gamma)
        {
            mGamma = gamma;
            return *this;
        }
        Material& SetFps(float fps)
        {
            mFps = fps;
            return *this;
        }
        Material& SetRuntime(float time)
        {
            mRuntime = time;
            return *this;
        }

        // Set the first material color.
        Material& SetBaseColor(const Color4f& color)
        {
            mBaseColor = color;
            return *this;
        }
        // Set the surface type.
        Material& SetSurfaceType(SurfaceType type)
        {
            mSurfaceType = type;
            return *this;
        }

        Material& AddTexture(const std::string& file, const std::string& name = "")
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureFileSource>(file, name);
            return *this;
        }
        Material& AddTexture(const TextBuffer& text)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureTextBufferSource>(text);
            mTextures.back().enable_gc = true;
            return *this;
        }
        Material& AddTexture(TextBuffer&& text)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureTextBufferSource>(std::move(text));
            mTextures.back().enable_gc = true;
            return *this;
        }
        template<typename T>
        Material& AddTexture(const Bitmap<T>& bitmap)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureBitmapSource>(bitmap);
            return *this;
        }

        template<typename T>
        Material& AddTexture(Bitmap<T>&& bitmap)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureBitmapSource>(std::move(bitmap));
            return *this;
        }

        Material& AddTexture(std::unique_ptr<TextureSource> source)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::move(source);
            return *this;
        }
        Material& AddTexture(std::unique_ptr<TextureSource> source, const FRect& rect)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::move(source);
            mTextures.back().box    = rect;
            return *this;
        }

        Material& SetTextureRect(std::size_t index, const FRect& rect)
        {
            ASSERT(index < mTextures.size());
            mTextures[index].box = rect;
            return *this;
        }
        Material& SetTextureGc(std::size_t index, bool on_off)
        {
            ASSERT(index < mTextures.size());
            mTextures[index].enable_gc = on_off;
            return *this;
        }
        Material& SetTextureSource(std::size_t index, std::unique_ptr<TextureSource> source)
        {
            ASSERT(index < mTextures.size());
            mTextures[index].source = std::move(source);
            return *this;
        }
        Material& SetTextureMinFilter(MinTextureFilter filter)
        {
            mMinFilter = filter;
            return *this;
        }
        Material& SetTextureMagFilter(MagTextureFilter filter)
        {
            mMagFilter = filter;
            return *this;
        }
        Material& SetTextureScaleX(float x)
        {
            mTextureScale.x = x;
            return *this;
        }
        Material& SetTextureScaleY(float y)
        {
            mTextureScale.y = y;
            return *this;
        }
        Material& SetTextureVelocityX(float x)
        {
            mTextureVelocity.x = x;
            return *this;
        }
        Material& SetTextureVelocityY(float y)
        {
            mTextureVelocity.y = y;
            return *this;
        }
        Material& SetTextureWrapX(TextureWrapping wrap)
        {
            mWrapX = wrap;
            return *this;
        }
        Material& SetTextureWrapY(TextureWrapping wrap)
        {
            mWrapY = wrap;
            return *this;
        }
        size_t GetNumTextures()
        { return mTextures.size(); }

        void DeleteTexture(size_t index)
        {
            ASSERT(index < mTextures.size());
            auto it = std::begin(mTextures);
            it += index;
            mTextures.erase(it);
        }
        const TextureSource& GetTextureSource(size_t index) const
        {
            ASSERT(index < mTextures.size());
            return *mTextures[index].source;
        }
        TextureSource& GetTextureSource(size_t index)
        {
            ASSERT(index < mTextures.size());
            return *mTextures[index].source;
        }
        FRect GetTextureRect(size_t index) const
        {
            ASSERT(index < mTextures.size());
            return mTextures[index].box;
        }

        nlohmann::json ToJson() const;

        static std::optional<Material> FromJson(const nlohmann::json& object);

        void BeginPacking(ResourcePacker* packer) const
        {
            packer->PackShader(this, GetShaderFile());
            for (const auto& sampler : mTextures)
            {
                sampler.source->BeginPacking(packer);
            }
            for (const auto& sampler : mTextures)
            {
                const ResourcePacker::ObjectHandle handle = sampler.source.get();
                packer->SetTextureBox(handle, sampler.box);
            }
        }
        void FinishPacking(const ResourcePacker* packer)
        {
            mShaderFile = packer->GetPackedShaderId(this);
            for (auto& sampler : mTextures)
            {
                sampler.source->FinishPacking(packer);
            }
            for (auto& sampler : mTextures)
            {
                const ResourcePacker::ObjectHandle handle = sampler.source.get();
                sampler.box = packer->GetPackedTextureBox(handle);
            }
        }

        // Deep copy of the material. Can be a bit expensive.
        Material& operator=(const Material& other);

    private:
        // warning: remember to edit the copy constructor
        // and the assignment operator if members are added.
        std::string mShaderFile;
        Color4f mBaseColor = gfx::Color::White;
        SurfaceType mSurfaceType = SurfaceType::Opaque;
        Type mType = Type::Color;
        float mGamma   = 1.0f;
        float mRuntime = 0.0f;
        float mFps     = 0.0f;
        bool  mBlendFrames = true;
        struct TextureSampler {
            FRect box = FRect(0.0f, 0.0f, 1.0f, 1.0f);
            bool enable_gc  = false;
            // todo: change to unique_ptr
            std::shared_ptr<TextureSource> source;
        };
        std::vector<TextureSampler> mTextures;
        MinTextureFilter mMinFilter = MinTextureFilter::Bilinear;
        MagTextureFilter mMagFilter = MagTextureFilter::Linear;
        TextureWrapping mWrapX = TextureWrapping::Repeat;
        TextureWrapping mWrapY = TextureWrapping::Repeat;
        glm::vec2 mTextureScale = glm::vec2(1.0f, 1.0f);
        glm::vec2 mTextureVelocity = glm::vec2(0.0f, 0.0f);
    };

    // This material will fill the drawn shape with solid color value.
    inline Material SolidColor(const Color4f& color)
    {
        return Material(Material::Type::Color)
            .SetBaseColor(color);
    }

    // This material will map the given texture onto the drawn shape.
    // The object being drawn must provide texture coordinates.
    inline Material TextureMap(const std::string& texture)
    {
        return Material(Material::Type::Texture)
            .AddTexture(texture)
            .SetSurfaceType(Material::SurfaceType::Opaque);
    }

    // SpriteSet is a material which renders a simple animation
    // by cycling through a set of textures at some fps.
    // The assumption is that cycling through the textures
    // renders a coherent animation.
    // In order to blend between the frames of the animation
    // the current time value needs to be set.
    inline Material SpriteSet()
    {
        return Material(Material::Type::Sprite)
            .SetSurfaceType(Material::SurfaceType::Transparent);
    }
    inline Material SpriteSet(const std::initializer_list<std::string>& textures)
    {
        auto material = SpriteSet();
        for (const auto& texture : textures)
            material.AddTexture(texture);
        return material;
    }
    inline Material SpriteSet(const std::vector<std::string>& textures)
    {
        auto material = SpriteSet();
        for (const auto& texture : textures)
            material.AddTexture(texture);
        return material;
    }

    // SpriteMap is a material which renders a simple animation
    // by cycling through different subregions of a single sprite
    // texture at some fps.
    // The assumption is that cycling through the regions
    // renders a coherent animation.
    inline Material SpriteMap()
    {
        return Material(Material::Type::Sprite)
            .SetSurfaceType(Material::SurfaceType::Transparent);
    }
    inline Material SpriteMap(const std::string& texture, const std::vector<FRect>& frames)
    {
        auto material = SpriteMap();
        for (size_t i=0; i<frames.size(); ++i)
        {
            material.AddTexture(texture);
            material.SetTextureRect(i, frames[i]);
        }
        return material;
    }

    // This material will use the given text buffer object to
    // display text by rasterizing the text and creating a new
    // texture object of it. The resulting texture object is then
    // mapped onto the shape being drawn.
    // The drawable shape must provide texture coordinates.
    inline Material BitmapText(const TextBuffer& text)
    {
        return Material(Material::Type::Texture)
            .AddTexture(text)
            .SetSurfaceType(Material::SurfaceType::Transparent);
    }

} // namespace
