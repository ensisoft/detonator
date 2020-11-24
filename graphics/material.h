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
#  include <base64/base64.h>
#include "warnpop.h"

#include <map>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <functional> // for hash
#include <optional>
#include <utility>
#include <type_traits>
#include <cmath>
#include <cassert>

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
            // Data comes from a bitmap buffer.
            BitmapBuffer,
            // Data comes from a bitmap generator algorithm.
            BitmapGenerator
        };

        virtual ~TextureSource() = default;
        // Get the type of the source of the texture data.
        virtual Source GetSourceType() const = 0;
        // Get texture source class/resource id.
        virtual std::string GetId() const = 0;
        // Get the human readable / and settable name.
        virtual std::string GetName() const = 0;
        // Get the texture hash that is used to map the content to a
        // GPU side object.
        virtual std::size_t GetHash() const = 0;
        // Set the texture source human readable name.
        virtual void SetName(const std::string& name) = 0;
        // Generate or load the data as a bitmap. If there's a content
        // error this function should return empty shared pointer.
        // The returned bitmap can be potentially shared but in an immutable
        // fashion.
        virtual std::shared_ptr<IBitmap> GetData() const = 0;
        // Clone this texture source.
        virtual std::unique_ptr<TextureSource> Clone() const = 0;
        // Serialize into JSON object.
        virtual nlohmann::json ToJson() const = 0;
        // Load state from JSON object. Returns true if successful
        // otherwise false.
        virtual bool FromJson(const nlohmann::json& object) = 0;
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
            TextureFileSource()
            { mId = base::RandomString(10); }
            TextureFileSource(const std::string& file) : TextureFileSource()
            { mFile = file; }
            virtual Source GetSourceType() const override
            { return Source::Filesystem; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::string GetName() const override
            { return mName; }
            virtual std::size_t GetHash() const override
            { return base::hash_combine(0, mFile); }
            virtual void SetName(const std::string& name)
            { mName = name; }
            virtual std::shared_ptr<IBitmap> GetData() const override;
            virtual std::unique_ptr<TextureSource> Clone() const override
            { return std::make_unique<TextureFileSource>(*this); }

            virtual nlohmann::json ToJson() const override
            {
                nlohmann::json json;
                base::JsonWrite(json, "id", mId);
                base::JsonWrite(json, "file", mFile);
                base::JsonWrite(json, "name", mName);
                return json;
            }
            virtual bool FromJson(const nlohmann::json& object) override
            {
                return base::JsonReadSafe(object, "id", &mId) &&
                       base::JsonReadSafe(object, "file", &mFile) &&
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
            void SetFileName(const std::string& file)
            { mFile = file; }
            const std::string& GetFilename() const
            { return mFile; }
        private:
            std::string mId;
            std::string mFile;
            std::string mName;
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
            virtual std::size_t GetHash() const override
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
            { return std::make_unique<TextureBitmapBufferSource>(*this); }
            virtual nlohmann::json ToJson() const override
            {
                const auto depth = mBitmap->GetDepthBits() / 8;
                const auto width = mBitmap->GetWidth();
                const auto height = mBitmap->GetHeight();
                const auto bytes = width * height * depth;
                nlohmann::json json;
                base::JsonWrite(json, "id", mId);
                base::JsonWrite(json, "name", mName);
                base::JsonWrite(json, "width", width);
                base::JsonWrite(json, "height", height);
                base::JsonWrite(json, "depth", depth);
                base::JsonWrite(json, "data",
                    base64::Encode((const unsigned char*)mBitmap->GetDataPtr(), bytes));
                return json;
            }
            virtual bool FromJson(const nlohmann::json& json) override
            {
                unsigned width = 0;
                unsigned height = 0;
                unsigned depth  = 0;
                std::string base64;
                if (!base::JsonReadSafe(json, "id", &mId) ||
                    !base::JsonReadSafe(json, "name", &mName) ||
                    !base::JsonReadSafe(json, "width", &width) ||
                    !base::JsonReadSafe(json, "height", &height) ||
                    !base::JsonReadSafe(json, "depth", &depth) ||
                    !base::JsonReadSafe(json, "data", &base64))
                    return false;
                const auto& data = base64::Decode(base64);
                if (depth == 1)
                    mBitmap = std::make_shared<GrayscaleBitmap>((const Grayscale*)&data[0], width, height);
                else if (depth == 3)
                    mBitmap = std::make_shared<RgbBitmap>((const RGB*)&data[0], width, height);
                else if (depth == 4)
                    mBitmap = std::make_shared<RgbaBitmap>((const RGBA*)&data[0], width, height);
                else return false;
                return true;
            }
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
            { return mGenerator->GetHash(); }
            virtual std::string GetName() const override
            { return mName; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual std::shared_ptr<IBitmap> GetData() const override
            { return mGenerator->Generate(); }
            virtual std::unique_ptr<TextureSource> Clone() const override
            { return std::make_unique<TextureBitmapGeneratorSource>(*this); }

            virtual nlohmann::json ToJson() const override
            {
                nlohmann::json json;
                base::JsonWrite(json, "id", mId);
                base::JsonWrite(json, "name", mName);
                base::JsonWrite(json, "function", mGenerator->GetFunction());
                base::JsonWrite(json, "generator", *mGenerator);
                return json;
            }
            virtual bool FromJson(const nlohmann::json& json) override
            {
                IBitmapGenerator::Function function;
                if (!base::JsonReadSafe(json, "id", &mId) ||
                    !base::JsonReadSafe(json, "name", &mName) ||
                    !base::JsonReadSafe(json, "function", &function))
                    return false;
                if (function == IBitmapGenerator::Function::Noise)
                    mGenerator = std::make_unique<NoiseBitmapGenerator>();
                if (!mGenerator->FromJson(json["generator"]))
                    return false;
                return true;
            }

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
            { return mTextBuffer.GetHash(); }
            virtual std::string GetName() const override
            { return mName; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual std::shared_ptr<IBitmap> GetData() const override;
            virtual std::unique_ptr<TextureSource> Clone() const override
            { return std::make_unique<TextureTextBufferSource>(*this); }

            virtual nlohmann::json ToJson() const override
            {
                nlohmann::json json;
                base::JsonWrite(json, "id", mId);
                base::JsonWrite(json, "name", mName);
                base::JsonWrite(json, "buffer", mTextBuffer);
                return json;
            }
            virtual bool FromJson(const nlohmann::json& json) override
            {
                if (!json.contains("buffer") || !json["buffer"].is_object())
                    return false;
                if (!base::JsonReadSafe(json, "name", &mName) ||
                    !base::JsonReadSafe(json, "id", &mId))
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
            std::string mId;
            std::string mName;
        };

    } // detail

    // MaterialClass holds the data for some particular type of material.
    // For example user might have defined material called "marble" with some
    // particular textures and parameters. One instance (a c++ object) of
    // MaterialClass would be used to represent this material type.
    // Material objects are then "instances" that represent an instance
    // of some material type.(I.e. they references a material class) and
    // contain per instance specific state.
    class MaterialClass
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
            // Material is using color(s) only.
            Gradient,
            // Material uses a static texture only.
            Texture,
            // Material uses a series of textures to create
            // a sprite animation.
            Sprite
        };

        // Action to take on per particle random value.
        enum class ParticleAction {
            // Do nothing
            None,
            // Rotate the texture coordinates
            Rotate
        };

        using MinTextureFilter = Texture::MinFilter;
        using MagTextureFilter = Texture::MagFilter;
        using TextureWrapping  = Texture::Wrapping;

        // constructor.
        MaterialClass(Type type) : mType(type)
        { mId = base::RandomString(10); }
        // allow "invalid" material to be constructed.
        MaterialClass()
        { mId = base::RandomString(10); }

        // Make a deep copy of the material class.
        MaterialClass(const MaterialClass& other);

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

        // state of a material instance of this type of material.
        struct MaterialInstance {
            float runtime = 0.0f;
            float alpha   = 1.0f;
        };

        // Apply the material properties to the given program object
        // and set the rasterizer state.
        void ApplyDynamicState(const Environment& env, const MaterialInstance& inst,
            Device& device, Program& prog, RasterState& state) const;
        // Apply the static state, i.e. the material state that doesn't change
        // during the material's lifetime and need to be only set once.
        void ApplyStaticState(Device& device, Program& prog) const;

        // MaterialClass properties getters.

        // get the class/resource id.
        std::string GetId() const
        { return mId; }
        void SetId(const std::string& id)
        { mId = id; }

        // Get the program ID for the material that is
        // Used to map the material to a device specific program object.
        std::string GetProgramId() const;

        // Get the ID/handle of the shader source file that
        // this material uses.
        std::string GetShaderFile() const;

        bool GetBlendFrames() const
        { return mBlendFrames; }
        bool IsStatic() const
        { return mStatic; }
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
        float GetBaseAlpha() const
        { return mBaseColor.Alpha(); }
        SurfaceType GetSurfaceType() const
        { return mSurfaceType; }
        Type GetType() const
        { return mType; }
        ParticleAction GetParticleAction() const
        { return mParticleAction; }

        // MaterialClass properties setters.

        // Set the material to use a specific shader.
        MaterialClass& SetShaderFile(const std::string& shader_file)
        {
            mShaderFile = shader_file;
            return *this;
        }

        // Set the functional material type that describes approximately
        // how the material should behave. The final output depends
        // on the actual shader being used.
        MaterialClass& SetType(Type type)
        {
            mType = type;
            return *this;
        }

        // Set whether to cut sharply between animation frames or
        // whether to smooth the transition by blending adjacent frames
        // together when in-between frames.
        MaterialClass& SetBlendFrames(bool on_off)
        {
            mBlendFrames = on_off;
            return *this;
        }

        // Set whether this material is static or not.
        // Static materials use their current state to map to a unique
        // shader program and apply the static material state only once
        // when the program is created. This can improve performance
        // since material state that doesn't change doesn't need to be
        // reset on every draw. However it'll create more program objects
        // i.e. consume more graphics memory. Additionally frequenct changes
        // of the material state will be expensive and cause recompilation
        // and rebuild of the corresponding shader program.
        MaterialClass& SetStatic(bool on_off)
        {
            mStatic = on_off;
            return *this;
        }

        // Set the gamma (in)correction value.
        // Values below 1.0f will result in the rendered image
        // being "brighter" and above 1.0f will make it "darker".
        // The default is 1.0f
        MaterialClass& SetGamma(float gamma)
        {
            mGamma = gamma;
            return *this;
        }
        MaterialClass& SetFps(float fps)
        {
            mFps = fps;
            return *this;
        }
        // Set the first material color.
        MaterialClass& SetBaseColor(const Color4f& color)
        {
            mBaseColor = color;
            return *this;
        }
        MaterialClass& SetBaseAlpha(float alpha)
        {
            mBaseColor.SetAlpha(alpha);
            return *this;
        }

        enum class ColorIndex {
            TopLeft, TopRight,
            BottomLeft, BottomRight
        };

        MaterialClass& SetColorMapColor(const Color4f& color, ColorIndex index)
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
            return *this;
        }
        Color4f GetColorMapColor(ColorIndex index) const
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

        // Set the surface type.
        MaterialClass& SetSurfaceType(SurfaceType type)
        {
            mSurfaceType = type;
            return *this;
        }

        MaterialClass& SetParticleAction(ParticleAction action)
        {
            mParticleAction = action;
            return *this;
        }

        MaterialClass& AddTexture(const std::string& file)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureFileSource>(file);
            return *this;
        }
        MaterialClass& AddTexture(const detail::TextureFileSource& texture)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureFileSource>(texture);
            return *this;
        }
        MaterialClass& AddTexture(detail::TextureFileSource&& texture)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureFileSource>(std::move(texture));
            return *this;
        }
        MaterialClass& AddTexture(const TextBuffer& text)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureTextBufferSource>(text);
            return *this;
        }
        MaterialClass& AddTexture(TextBuffer&& text)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureTextBufferSource>(std::move(text));
            return *this;
        }
        MaterialClass& AddTexture(const detail::TextureTextBufferSource& text)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureTextBufferSource>(text);
            return *this;
        }
        MaterialClass& AddTexture(detail::TextureTextBufferSource&& text)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureTextBufferSource>(std::move(text));
            return *this;
        }

        MaterialClass& AddTexture(const detail::TextureBitmapBufferSource& bitmap)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureBitmapBufferSource>(bitmap);
            return *this;
        }
        MaterialClass& AddTexture(detail::TextureBitmapBufferSource&& bitmap)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureBitmapBufferSource>(std::move(bitmap));
            return *this;
        }

        MaterialClass& AddTexture(const NoiseBitmapGenerator& generator)
        {
            auto gen = std::make_unique<NoiseBitmapGenerator>(generator);
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureBitmapGeneratorSource>(std::move(gen));
            return *this;
        }
        MaterialClass& AddTexture(NoiseBitmapGenerator&& generator)
        {
            auto gen = std::make_unique<NoiseBitmapGenerator>(std::move(generator));
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureBitmapGeneratorSource>(std::move(gen));
            return *this;
        }
        MaterialClass& AddTexture(std::unique_ptr<IBitmapGenerator> generator)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureBitmapGeneratorSource>(std::move(generator));
            return *this;
        }
        MaterialClass& AddTexture(const detail::TextureBitmapGeneratorSource& generator)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureBitmapGeneratorSource>(generator);
            return *this;
        }
        MaterialClass& AddTexture(detail::TextureBitmapGeneratorSource&& generator)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureBitmapGeneratorSource>(std::move(generator));
            return *this;
        }

        template<typename T>
        MaterialClass& AddTexture(const Bitmap<T>& bitmap)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureBitmapBufferSource>(bitmap);
            return *this;
        }

        template<typename T>
        MaterialClass& AddTexture(Bitmap<T>&& bitmap)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_unique<detail::TextureBitmapBufferSource>(std::move(bitmap));
            return *this;
        }

        MaterialClass& AddTexture(std::unique_ptr<TextureSource> source)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::move(source);
            return *this;
        }
        MaterialClass& AddTexture(std::unique_ptr<TextureSource> source, const FRect& rect)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::move(source);
            mTextures.back().box    = rect;
            return *this;
        }

        MaterialClass& SetTextureRect(std::size_t index, const FRect& rect)
        {
            ASSERT(index < mTextures.size());
            mTextures[index].box = rect;
            return *this;
        }
        MaterialClass& SetTextureGc(std::size_t index, bool on_off)
        {
            ASSERT(index < mTextures.size());
            mTextures[index].enable_gc = on_off;
            return *this;
        }
        MaterialClass& SetTextureSource(std::size_t index, std::unique_ptr<TextureSource> source)
        {
            ASSERT(index < mTextures.size());
            mTextures[index].source = std::move(source);
            return *this;
        }
        MaterialClass& SetTextureMinFilter(MinTextureFilter filter)
        {
            mMinFilter = filter;
            return *this;
        }
        MaterialClass& SetTextureMagFilter(MagTextureFilter filter)
        {
            mMagFilter = filter;
            return *this;
        }
        MaterialClass& SetTextureScaleX(float x)
        {
            mTextureScale.x = x;
            return *this;
        }
        MaterialClass& SetTextureScaleY(float y)
        {
            mTextureScale.y = y;
            return *this;
        }
        MaterialClass& SetTextureVelocityX(float x)
        {
            mTextureVelocity.x = x;
            return *this;
        }
        MaterialClass& SetTextureVelocityY(float y)
        {
            mTextureVelocity.y = y;
            return *this;
        }
        MaterialClass& SetTextureVelocityZ(float angle_radians)
        {
            mTextureVelocity.z = angle_radians;
            return *this;
        }
        MaterialClass& SetTextureWrapX(TextureWrapping wrap)
        {
            mWrapX = wrap;
            return *this;
        }
        MaterialClass& SetTextureWrapY(TextureWrapping wrap)
        {
            mWrapY = wrap;
            return *this;
        }
        size_t GetNumTextures() const
        { return mTextures.size(); }

        void DeleteTexture(size_t index)
        {
            ASSERT(index < mTextures.size());
            auto it = std::begin(mTextures);
            it += index;
            mTextures.erase(it);
        }
        const TextureSource* FindTextureSourceById(const std::string& id) const
        {
            for (const auto& tex : mTextures)
            {
                if (tex.source->GetId() == id)
                    return tex.source.get();
            }
            return nullptr;
        }
        const TextureSource* FindTextureSourceByName(const std::string& name) const
        {
            for (const auto& tex : mTextures)
            {
                if (tex.source->GetName() == name)
                    return tex.source.get();
            }
            return nullptr;
        }
        const TextureSource& GetTextureSource(size_t index) const
        {
            ASSERT(index < mTextures.size());
            return *mTextures[index].source;
        }
        TextureSource* FindTextureSourceById(const std::string& id)
        {
            for (const auto& tex : mTextures)
            {
                if (tex.source->GetId() == id)
                    return tex.source.get();
            }
            return nullptr;
        }
        TextureSource* FindTextureSourceByName(const std::string& name)
        {
            for (const auto& tex : mTextures)
            {
                if (tex.source->GetName() == name)
                    return tex.source.get();
            }
            return nullptr;
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
        bool GetTextureGc(size_t index) const
        {
            ASSERT(index < mTextures.size());
            return mTextures[index].enable_gc;
        }

        // Get the hash value of the material based on the current material
        // properties excluding the transient state i.e. runtime.
        size_t GetHash() const;

        nlohmann::json ToJson() const;

        static std::optional<MaterialClass> FromJson(const nlohmann::json& object);

        void BeginPacking(ResourcePacker* packer) const;
        void FinishPacking(const ResourcePacker* packer);

        // Deep copy of the material. Can be a bit expensive.
        MaterialClass& operator=(const MaterialClass& other);

    private:
        // material class id.
        std::string mId;
        // warning: remember to edit the copy constructor
        // and the assignment operator if members are added.
        std::string mShaderFile;
        Color4f mBaseColor = gfx::Color::White;
        SurfaceType mSurfaceType = SurfaceType::Opaque;
        Type mType = Type::Color;
        float mGamma   = 1.0f;
        float mFps     = 0.0f;
        bool  mBlendFrames = true;
        bool  mStatic = false;
        struct TextureSampler {
            FRect box = FRect(0.0f, 0.0f, 1.0f, 1.0f);
            bool enable_gc  = false;
            // todo: change to unique_ptr
            std::shared_ptr<TextureSource> source;
        };
        std::vector<TextureSampler> mTextures;
        MinTextureFilter mMinFilter = MinTextureFilter::Bilinear;
        MagTextureFilter mMagFilter = MagTextureFilter::Linear;
        TextureWrapping mWrapX = TextureWrapping::Clamp;
        TextureWrapping mWrapY = TextureWrapping::Clamp;
        glm::vec2 mTextureScale = glm::vec2(1.0f, 1.0f);
        glm::vec3 mTextureVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
        Color4f mColorMap[4] = {gfx::Color::White, gfx::Color::White,
            gfx::Color::White, gfx::Color::White};
        ParticleAction mParticleAction = ParticleAction::None;
    };

    // Material is an instance of some specific type of material.
    // The "type" (i.e. what kind of material it is) is defined by
    // The MaterialClass which is shared between instances of any
    // spefic material type.
    class Material
    {
    public:
        // Create new material instance based on the given material class.
        Material(const std::shared_ptr<const MaterialClass>& klass, float time = 0.0f)
            : mClass(klass)
            , mRuntime(time)
        {
            mAlpha = mClass->GetBaseAlpha();
        }
        Material(const MaterialClass& klass, float time = 0.0f)
        {
            mClass = std::make_shared<MaterialClass>(klass);
            mRuntime = time;
            mAlpha   = mClass->GetBaseAlpha();
        }
        // Get the material class object instance.
        const MaterialClass& GetClass() const
        { return *mClass; }
        // Shortcut operator for accessing the class object instance.
        const MaterialClass* operator->() const
        { return mClass.get(); }

        // Apply the material properties to the given program object
        // and set the rasterizer state.
        void ApplyDynamicState(const MaterialClass::Environment& env, Device& device, Program& prog,
            MaterialClass::RasterState& state) const
        {
            // currently the only dynamic (instance) state is the material
            // runtime. However if there's more such instance specific
            // data it'd be set here.
            MaterialClass::MaterialInstance instance;
            instance.runtime = mRuntime;
            instance.alpha   = mAlpha;
            mClass->ApplyDynamicState(env, instance, device, prog, state);
        }
        // Set the material's alpha value. will be clamped to [0.0f, 1.0f] range
        // and only works if the material supports transparent(alpha) blendign.
        void SetAlpha(float alpha)
        { mAlpha = math::clamp(0.0f, 1.0f, alpha); }
        void SetRuntime(float runtime)
        { mRuntime = runtime; }
        void Update(float dt)
        { mRuntime += dt; }
        float GetAlpha() const
        { return mAlpha; }
        float GetRuntime() const
        { return mRuntime; }
    private:
        // This is the "class" object for this material type.
        std::shared_ptr<const MaterialClass> mClass;
        // Current runtime for this material instance.
        float mRuntime = 0.0f;
        // Alpha value.
        float mAlpha = 1.0f;
    };


    // This material will fill the drawn shape with solid color value.
    inline MaterialClass SolidColor(const Color4f& color)
    {
        return MaterialClass(MaterialClass::Type::Color)
            .SetBaseColor(color);
    }

    // This material will map the given texture onto the drawn shape.
    // The object being drawn must provide texture coordinates.
    inline MaterialClass TextureMap(const std::string& texture)
    {
        return MaterialClass(MaterialClass::Type::Texture)
            .AddTexture(texture)
            .SetSurfaceType(MaterialClass::SurfaceType::Opaque);
    }

    // SpriteSet is a material which renders a simple animation
    // by cycling through a set of textures at some fps.
    // The assumption is that cycling through the textures
    // renders a coherent animation.
    // In order to blend between the frames of the animation
    // the current time value needs to be set.
    inline MaterialClass SpriteSet()
    {
        return MaterialClass(MaterialClass::Type::Sprite)
            .SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    }
    inline MaterialClass SpriteSet(const std::initializer_list<std::string>& textures)
    {
        auto material = SpriteSet();
        for (const auto& texture : textures)
            material.AddTexture(texture);
        return material;
    }
    inline MaterialClass SpriteSet(const std::vector<std::string>& textures)
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
    inline MaterialClass SpriteMap()
    {
        return MaterialClass(MaterialClass::Type::Sprite)
            .SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    }
    inline MaterialClass SpriteMap(const std::string& texture, const std::vector<FRect>& frames)
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
    inline MaterialClass BitmapText(const TextBuffer& text)
    {
        return MaterialClass(MaterialClass::Type::Texture)
            .AddTexture(text)
            .SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    }


    std::unique_ptr<Material> CreateMaterialInstance(const MaterialClass& klass);
    std::unique_ptr<Material> CreateMaterialInstance(const std::shared_ptr<const MaterialClass>& klass);

} // namespace
