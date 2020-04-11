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
#include "bitmap.h"
#include "color4f.h"
#include "device.h"
#include "image.h"
#include "program.h"
#include "shader.h"
#include "texture.h"
#include "types.h"
#include "text.h"

namespace gfx
{
    // Abstract interface for acquiring the actual texture data.
    class TextureSource
    {
    public:
        virtual ~TextureSource() = default;
        // Get the name for the texture. This is expected to be unique
        // and is used to map the source to a device texture resource.
        virtual std::string GetName() const = 0;
        // Generate or load the data as a bitmap.
        virtual std::shared_ptr<IBitmap> GetData() const = 0;
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
    private:
    };



    namespace detail {
        // Source texture data from an image file.
        class TextureFileSource : public TextureSource
        {
        public:
            TextureFileSource(const std::string& filename)
              : mFilename(filename)
            {}
            TextureFileSource() = default;
            virtual std::string GetName() const override
            { return mFilename; }
            virtual std::shared_ptr<IBitmap> GetData() const override
            {
                Image file(mFilename);
                if (file.GetDepthBits() == 8)
                    return std::make_shared<GrayscaleBitmap>(file.AsBitmap<Grayscale>());
                else if (file.GetDepthBits() == 24)
                    return std::make_shared<RgbBitmap>(file.AsBitmap<RGB>());
                else if (file.GetDepthBits() == 32)
                    return std::make_shared<RgbaBitmap>(file.AsBitmap<RGBA>());
                else throw std::runtime_error("unexpected image depth: " + std::to_string(file.GetDepthBits()));
                return nullptr;
            }
            virtual bool CanSerialize() const override
            { return true; }
            virtual nlohmann::json ToJson() const override
            {
                nlohmann::json json = {
                    {"filename", mFilename}
                };
                return json;
            }
            virtual bool FromJson(const nlohmann::json& object) override
            {
                return base::JsonReadSafe(object, "filename", &mFilename);
            }
        private:
            std::string mFilename;
        };

        // Source texture data from a bitmap
        template<typename T>
        class TextureBitmapSource : public TextureSource
        {
        public:
            TextureBitmapSource(const Bitmap<T>& bmp)
              : mBitmap(new Bitmap<T>(bmp))
            {}
            TextureBitmapSource(Bitmap<T>&& bmp)
              : mBitmap(new Bitmap<T>(std::move(bmp)))
            {}
            virtual std::string GetName() const override
            {
                size_t hash = mBitmap->GetHash();
                hash = base::hash_combine(hash, mBitmap->GetWidth());
                hash = base::hash_combine(hash, mBitmap->GetHeight());
                return std::to_string(hash);
            }
            virtual std::shared_ptr<IBitmap> GetData() const override
            { return mBitmap; }
        private:
            std::shared_ptr<Bitmap<T>> mBitmap;
        };

        // Rasterize text buffer and provide as a texture source.
        class TextureTextBufferSource : public TextureSource
        {
        public:
            TextureTextBufferSource(const TextBuffer& text)
              : mTextBuffer(text)
            {}
            virtual std::string GetName() const override
            {
                size_t hash = mTextBuffer.GetHash();
                hash = base::hash_combine(hash, mTextBuffer.GetWidth());
                hash = base::hash_combine(hash, mTextBuffer.GetHeight());
                return std::to_string(hash);
            }
            virtual std::shared_ptr<IBitmap> GetData() const override
            {
                return mTextBuffer.Rasterize();
            }
        private:
            const TextBuffer& mTextBuffer;
        };

        // this is super ugly but there needs to be some kind of abstraction
        // for loading materials from JSON and coming up with the correct
        // texture source object. This will get the job done by allowing
        // the client code to register a custom class type in the factory
        class TextureSourceFactory
        {
        public:
            static std::shared_ptr<TextureSource> Create(const std::string& class_)
            {
                return GetFactory().mFactories[class_]->Create();
            }
            template<typename T>
            void RegisterTextureSourceType()
            {
                GetFactory().mFactories[typeid(T).name()] = std::make_unique<TypedFactory<T>>();
            }

        private:
            static TextureSourceFactory& GetFactory()
            {
                static TextureSourceFactory almost_like_java;
                return almost_like_java;
            }
            TextureSourceFactory()
            {
                mFactories[typeid(TextureFileSource).name()] = std::make_unique<TypedFactory<TextureFileSource>>();
            }

        private:
            class AbstractFactory
            {
            public:
                virtual std::shared_ptr<TextureSource> Create() const = 0;
            private:
            };
            template<typename T>
            class TypedFactory :public AbstractFactory
            {
            public:
                virtual std::shared_ptr<TextureSource> Create() const override
                { return std::make_shared<T>(); }
            private:
            };
        private:
            std::map<std::string, std::unique_ptr<AbstractFactory>> mFactories;
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
            Sprite,
        };

        using MinTextureFilter = Texture::MinFilter;
        using MagTextureFilter = Texture::MagFilter;
        using TextureWrapping  = Texture::Wrapping;

        // constructor.
        Material(Type type) : mType(type)
        {}
        // allow "invalid" material to be constructed.
        Material() = default;

        // Create the shader for this material on the given device.
        // Returns the new shader object or nullptr if the shader
        // failed to compile.
        Shader* GetShader(Device& device) const
        {
            const std::string& name = GetShaderName();

            // The default constructed material cannot be used to render.
            ASSERT(!name.empty() &&
                "No material shader resource is set. Did you forget to call SetShader ?");

            Shader* shader = device.FindShader(name);
            if (shader == nullptr || !shader->IsValid())
            {
                if (shader == nullptr)
                    shader = device.MakeShader(name);
                if (!shader->CompileFile(name))
                    return nullptr;
            }
            return shader;
        }

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
        void Apply(const Environment& env, Device& device, Program& prog, RasterState& state) const
        {
            // set rasterizer state.
            if (mSurfaceType == SurfaceType::Opaque)
                state.blending = RasterState::Blending::None;
            else if (mSurfaceType == SurfaceType::Transparent)
                state.blending = RasterState::Blending::Transparent;
            else if (mSurfaceType == SurfaceType::Emissive)
                state.blending = RasterState::Blending::Additive;

            // currently we only bind two textures and set a blend
            // coefficient from the run time for the shader to do
            // blending between two animation frames.
            // Note different material instances map to the same program
            // object on the device. That means that if material m0 uses
            // textures t0 and t1 to render and material m1 *want's to use*
            // texture t2 and t3 to render but *forgets* to set them
            // then the rendering will use textures t0 and t1
            // or whichever textures happen to be bound to the texture units
            // since last rendering.
            if ((mType == Type::Texture || mType == Type::Sprite) && !mTextures.empty())
            {
                // if we're a sprite then we should probably animate if we have an FPS setting.
                const auto animating         = mType == Type::Sprite && mFps > 0.0f;
                const auto frame_interval    = animating ? 1.0f / mFps : 0.0f;
                const auto frame_fraction    = animating ? std::fmod(mRuntime, frame_interval) : 0.0f;
                const auto frame_blend_coeff = animating ? frame_fraction/frame_interval : 0.0f;
                const auto first_frame_index = animating ? (unsigned)(mRuntime/frame_interval) : 0u;

                const auto frame_count = mType == Type::Texture ? 1u : (unsigned)mTextures.size();
                const unsigned frame_index[2] = {
                    (first_frame_index + 0) % frame_count,
                    (first_frame_index + 1) % frame_count
                };

                for (unsigned i=0; i<std::min(frame_count, 2u); ++i)
                {
                    const auto& sampler = mTextures[frame_index[i]];
                    const auto& source  = sampler.source;
                    const auto& name    = source->GetName();
                    auto* texture = device.FindTexture(name);
                    if (!texture)
                    {
                        texture = device.MakeTexture(name);
                        auto bitmap = source->GetData();
                        const auto width  = bitmap->GetWidth();
                        const auto height = bitmap->GetHeight();
                        const auto format = Texture::DepthToFormat(bitmap->GetDepthBits());
                        texture->Upload(bitmap->GetDataPtr(), width, height, format);
                        texture->EnableGarbageCollection(sampler.enable_gc);
                    }
                    const auto w = (float)texture->GetWidth();
                    const auto h = (float)texture->GetHeight();
                    const bool normalized_box = sampler.box_is_normalized;
                    const auto& box = sampler.box_is_normalized
                        ? sampler.box : sampler.box.Normalize(FSize(w, h));

                    const float x = box.GetX();
                    const float y = 1.0f - box.GetY() - box.GetHeight();
                    const float sx = box.GetWidth();
                    const float sy = box.GetHeight();

                    const auto& kTexture = "kTexture" + std::to_string(i);
                    const auto& kTextureBox = "kTextureBox" + std::to_string(i);
                    const auto& kIsAlphaMask = "kIsAlphaMask" + std::to_string(i);
                    const auto alpha = texture->GetFormat() == Texture::Format::Grayscale
                        ? 1.0f : 0.0f;

                    prog.SetTexture(kTexture.c_str(), i, *texture);
                    prog.SetUniform(kTextureBox.c_str(), x, y, sx, sy);
                    prog.SetUniform(kIsAlphaMask.c_str(), alpha);
                    prog.SetUniform("kBlendCoeff", mBlendFrames ? frame_blend_coeff : 0.0f);

                    texture->SetFilter(mMinFilter);
                    texture->SetFilter(mMagFilter);
                    texture->SetWrapX(mWrapX);
                    texture->SetWrapY(mWrapY);
                }
            }
            prog.SetUniform("kBaseColor", mBaseColor);
            prog.SetUniform("kGamma", mGamma);
            prog.SetUniform("kRuntime", mRuntime);
            prog.SetUniform("kRenderPoints", env.render_points ? 1.0f : 0.0f);
            prog.SetUniform("kTextureScale", mTextureScaleX, mTextureScaleY);
        }
        // Material properties getters.

        std::string GetShaderName() const
        {
            // check if have a user defined specific shader or not.
            if (!mShader.empty())
                return mShader;

            if (mType == Type::Color)
                return "solid_color.glsl";
            else if (mType == Type::Texture)
                return "texture_map.glsl";
            else if (mType == Type::Sprite)
                return "texture_map.glsl";
            ASSERT(!"???");
            return "";
        }

        bool GetBlendFrames() const
        { return mBlendFrames; }
        float GetTextureScaleX() const
        { return mTextureScaleX; }
        float GetTextureScaleY() const
        { return mTextureScaleY; }
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
        // If the shader's type is set to Custom this should be
        // a complete path (either relative or absolute) to the
        // shader file in question. Otherwise the shader is looked
        // for under the engines shaders/es2/ folder.
        Material& SetShaderName(const std::string& shader_file)
        {
            mShader = shader_file;
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

        Material& AddTexture(const std::string& texture)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_shared<detail::TextureFileSource>(texture);
            return *this;
        }
        Material& AddTexture(const TextBuffer& text)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_shared<detail::TextureTextBufferSource>(text);
            mTextures.back().enable_gc = true;
            return *this;
        }
        template<typename T>
        Material& AddTexture(const Bitmap<T>& bitmap)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_shared<detail::TextureBitmapSource<T>>(bitmap);
            return *this;
        }

        template<typename T>
        Material& AddTexture(Bitmap<T>&& bitmap)
        {
            mTextures.emplace_back();
            mTextures.back().source = std::make_shared<detail::TextureBitmapSource<T>>(std::move(bitmap));
            return *this;
        }

        Material& AddTexture(std::shared_ptr<TextureSource> source)
        {
            mTextures.emplace_back();
            mTextures.back().source = source;
            return *this;
        }
        Material& SetTextureRect(std::size_t index, const FRect& rect)
        {
            ASSERT(index < mTextures.size());
            mTextures[index].box = rect;
            mTextures[index].box_is_normalized = true;
            return *this;
        }
        Material& SetTextureRect(std::size_t index, const URect& rect)
        {
            ASSERT(index < mTextures.size());
            mTextures[index].box = FRect(rect);
            mTextures[index].box_is_normalized = false;
            return *this;
        }
        Material& SetTextureGc(std::size_t index, bool on_off)
        {
            ASSERT(index < mTextures.size());
            mTextures[index].enable_gc = on_off;
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
            mTextureScaleX = x;
            return *this;
        }
        Material& SetTextureScaleY(float y)
        {
            mTextureScaleY = y;
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

        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            base::JsonWrite(json, "shader", mShader);
            base::JsonWrite(json, "type", mType);
            base::JsonWrite(json, "color", mBaseColor);
            base::JsonWrite(json, "surface", mSurfaceType);
            base::JsonWrite(json, "gamma", mGamma);
            base::JsonWrite(json, "fps", mFps);
            base::JsonWrite(json, "blending", mBlendFrames);
            base::JsonWrite(json, "texture_min_filter", mMinFilter);
            base::JsonWrite(json, "texture_mag_filter", mMagFilter);
            base::JsonWrite(json, "texture_wrap_x", mWrapX);
            base::JsonWrite(json, "texture_wrap_y", mWrapY);
            base::JsonWrite(json, "texture_scale_x", mTextureScaleX);
            base::JsonWrite(json, "texture_scale_y", mTextureScaleY);

            for (const auto& sampler : mTextures)
            {
                if (!sampler.source->CanSerialize())
                    continue;
                nlohmann::json js;
                base::JsonWrite(js, "box", sampler.box);
                base::JsonWrite(js, "box_is_normalized", sampler.box_is_normalized);
                base::JsonWrite(js, "class", typeid(*sampler.source).name());
                base::JsonWrite(js, "source", *sampler.source);
                base::JsonWrite(js, "enable_gc", sampler.enable_gc);
                json["samplers"].push_back(js);
            }
            return json;
        }
        static std::optional<Material> FromJson(const nlohmann::json& object)
        {
            Material mat;

            if (!base::JsonReadSafe(object, "shader", &mat.mShader) ||
                !base::JsonReadSafe(object, "color", &mat.mBaseColor) ||
                !base::JsonReadSafe(object, "type", &mat.mType) ||
                !base::JsonReadSafe(object, "surface", &mat.mSurfaceType) ||
                !base::JsonReadSafe(object, "gamma", &mat.mGamma) ||
                !base::JsonReadSafe(object, "fps", &mat.mFps) ||
                !base::JsonReadSafe(object, "blending", &mat.mBlendFrames) ||
                !base::JsonReadSafe(object, "texture_min_filter", &mat.mMinFilter) ||
                !base::JsonReadSafe(object, "texture_mag_filter", &mat.mMagFilter) ||
                !base::JsonReadSafe(object, "texture_wrap_x", &mat.mWrapX) ||
                !base::JsonReadSafe(object, "texture_wrap_y", &mat.mWrapY) ||
                !base::JsonReadSafe(object, "texture_scale_x", &mat.mTextureScaleX) ||
                !base::JsonReadSafe(object, "texture_scale_y", &mat.mTextureScaleY))
                return std::nullopt;

            if (!object.contains("samplers"))
                return mat;

            for (const auto& json_sampler : object["samplers"].items())
            {
                const auto& obj = json_sampler.value();
                const std::string& class_ = obj["class"];
                auto source = detail::TextureSourceFactory::Create(class_);
                if (!source->FromJson(obj["source"]))
                    return std::nullopt;
                TextureSampler s;
                if (!base::JsonReadSafe(obj, "box", &s.box) ||
                    !base::JsonReadSafe(obj, "box_is_normalized", &s.box_is_normalized) ||
                    !base::JsonReadSafe(obj, "enable_gc", &s.enable_gc))
                    return std::nullopt;

                s.source = std::move(source);
                mat.mTextures.push_back(std::move(s));
            }
            return mat;
        }
    private:
        std::string mShader;
        Color4f mBaseColor = gfx::Color::White;
        SurfaceType mSurfaceType = SurfaceType::Opaque;
        Type mType = Type::Color;
        float mGamma   = 1.0f;
        float mRuntime = 0.0f;
        float mFps     = 0.0f;
        bool  mBlendFrames = true;
        struct TextureSampler {
            FRect box = FRect(0.0f, 0.0f, 1.0f, 1.0f);
            bool box_is_normalized = true;
            bool enable_gc  = false;
            std::shared_ptr<TextureSource> source;
        };
        std::vector<TextureSampler> mTextures;
        MinTextureFilter mMinFilter = MinTextureFilter::Bilinear;
        MagTextureFilter mMagFilter = MagTextureFilter::Linear;
        TextureWrapping mWrapX = TextureWrapping::Repeat;
        TextureWrapping mWrapY = TextureWrapping::Repeat;
        float mTextureScaleX = 1.0f;
        float mTextureScaleY = 1.0f;
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
    inline Material SpriteMap(const std::string& texture, const std::vector<URect>& frames)
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
