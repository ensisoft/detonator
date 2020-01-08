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

#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <functional> // for hash
#include <cmath>
#include <cassert>

#include "base/utility.h"
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
        private:
            const std::string mFilename;
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
                hash ^= std::hash<std::uint32_t>()(mBitmap->GetWidth());
                hash ^= std::hash<std::uint32_t>()(mBitmap->GetHeight());
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
                hash ^= std::hash<std::uint32_t>()(mTextBuffer.GetWidth());
                hash ^= std::hash<std::uint32_t>()(mTextBuffer.GetHeight());
                return std::to_string(hash);
            }
            virtual std::shared_ptr<IBitmap> GetData() const override
            {
                return mTextBuffer.Rasterize();
            }
        private:
            const TextBuffer& mTextBuffer;
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

        // constructor. 
        Material(const std::string& shader) : mShader(shader)
        {}

        // Create the shader for this material on the given device.
        // Returns the new shader object or nullptr if the shader
        // failed to compile.
        Shader* GetShader(Device& device) const
        {
            Shader* shader = device.FindShader(mShader);
            if (shader == nullptr || !shader->IsValid())
            {
                if (shader == nullptr)
                    shader = device.MakeShader(mShader);
                if (!shader->CompileFile(mShader))
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
            if (!mTextures.empty())
            {
                const auto frame_count = (unsigned)mTextures.size();
                const auto frame_interval = mFps > 0.0f ? 1.0f / mFps : 1.0f;
                const unsigned frame_index[2] = {
                    (unsigned(mRuntime / frame_interval) + 0) % frame_count,
                    (unsigned(mRuntime / frame_interval) + 1) % frame_count
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
                    const bool normalized_box = sampler.box_is_normalized;
                    const auto& box = sampler.box;
                    const float w = texture->GetWidth();
                    const float h = texture->GetHeight();
                    const float x = normalized_box ? box.GetX() : box.GetX() / w;
                    const float y = normalized_box ? 1.0f - box.GetY() : 1.0f - ((box.GetY() + box.GetHeight()) / h);
                    const float sx = normalized_box ? box.GetWidth() : box.GetWidth() / w;
                    const float sy = normalized_box ? box.GetHeight() : box.GetHeight() / h;                

                    const auto& kTexture = "kTexture" + std::to_string(i);
                    const auto& kTextureBox = "kTextureBox" + std::to_string(i);
                    const auto& kIsAlphaMask = "kIsAlphaMask" + std::to_string(i);
                    const auto alpha = texture->GetFormat() == Texture::Format::Grayscale 
                        ? 1.0 : 0.0f;

                    prog.SetTexture(kTexture.c_str(), i, *texture);
                    prog.SetUniform(kTextureBox.c_str(), x, y, sx, sy);
                    prog.SetUniform(kIsAlphaMask.c_str(), alpha);
                }
                if (frame_count >= 2)
                {
                    const auto coeff = std::fmod(mRuntime, frame_interval) / frame_interval;
                    prog.SetUniform("kBlendCoeff", coeff * mTextureBlendWeight);
                }
                else
                {
                    prog.SetUniform("kBlendCoeff", 0.0f);
                }
            }
            else
            {
                prog.SetUniform("kBlendCoeff", 0.0f);
            }
            prog.SetUniform("kColorA", mColorA);
            prog.SetUniform("kColorB", mColorB);
            prog.SetUniform("kGamma", mGamma);
            prog.SetUniform("kRuntime", mRuntime);
            prog.SetUniform("kRenderPoints", env.render_points ? 1.0f : 0.0f);
        }

        std::string GetName() const
        { return mShader; }

        Material& SetTextureBlendWeight(float weight)
        {
            mTextureBlendWeight = math::clamp(0.0f, 1.0f, weight);
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
        Material& SetColorA(const Color4f& color)
        {
            mColorA = color;
            return *this;
        }
        // Set the second material color.
        Material& SetColorB(const Color4f& color)
        {
            mColorB = color;
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
    private:
        const std::string mShader;
        Color4f mColorA = gfx::Color::White;
        Color4f mColorB = gfx::Color::White;
        SurfaceType mSurfaceType = SurfaceType::Opaque;
        float mGamma   = 1.0f;
        float mRuntime = 0.0f;
        float mFps     = 0.0f;
        float mTextureBlendWeight = 1.0f;
        struct TextureSampler {
            FRect box = FRect(0.0f, 0.0f, 1.0f, 1.0f);
            bool box_is_normalized = true;
            bool enable_gc  = false;
            std::shared_ptr<TextureSource> source;
        };
        std::vector<TextureSampler> mTextures;
    };

    // This material will fill the drawn shape with solid color value.
    inline Material SolidColor(const Color4f& color)
    { 
        return Material("solid_color.glsl").SetColorA(color);
    }
 

    // This material will map the given texture onto the drawn shape.
    // The object being drawn must provide texture coordinates.
    inline Material TextureMap(const std::string& texture)
    {
        return Material("texture_map.glsl")
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
        return Material("texture_map.glsl")
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
        return Material("texture_map.glsl")
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
        return Material("texture_map.glsl")
            .AddTexture(text)
            .SetSurfaceType(Material::SurfaceType::Transparent);
    }

    // todo: refactor away
    inline Material SlidingGlintEffect(float secs)
    {
        return Material("sliding_glint_effect.glsl")
            .SetSurfaceType(Material::SurfaceType::Transparent)
            .SetRuntime(secs);
    }
    // todo: refactor away
    inline Material ConcentricRingsEffect(float secs)
    {
        return Material("concentric_rings_effect.glsl")
            .SetSurfaceType(Material::SurfaceType::Transparent)
            .SetRuntime(secs);
    }

} // namespace
