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

        virtual ~Material() = default;

        // Create the shader for this material on the given device.
        // Returns the new shader object or nullptr if the shader
        // failed to compile.
        virtual Shader* GetShader(Device& device) const = 0;

        struct Environment {
            bool render_points = false;
        };

        // Apply the material properties in the given program object.
        virtual void Apply(const Environment& env, Device& device, Program& prog) const = 0;

        // Get the material surface type.
        virtual SurfaceType GetSurfaceType() const = 0;
    protected:
    private:
    };

    // This material will fill the drawn shape with solid color value.
    class SolidColor : public Material
    {
    public:
        // Construct with default color value.
        // You can  use SetColor later to set the desired color.
        SolidColor() = default;
        // Construct with the given color value.
        SolidColor(const Color4f& color) : mColor(color)
        {}
        // Get/Create the corresponding shader object.
        virtual Shader* GetShader(Device& device) const override
        {
            Shader* s = device.FindShader("solid_color.glsl");
            if (s == nullptr || !s->IsValid())
            {
                if (s == nullptr)
                    s = device.MakeShader("solid_color.glsl");
                if (!s->CompileFile("solid_color.glsl"))
                    return nullptr;
            }
            return s;
        }
        // Apply the material state on the program object.
        virtual void Apply(const Environment& env, Device&, Program& prog) const override
        {
            prog.SetUniform("kColor",
                mColor.Red(), mColor.Green(), mColor.Blue(),
                mColor.Alpha());
        }
        // Get material surface type.
        virtual SurfaceType GetSurfaceType() const override
        {
            if (mTransparency)
                return SurfaceType::Transparent;
            return SurfaceType::Opaque;
        }

        // Set the fill color for the material.
        SolidColor& SetColor(const Color4f color)
        { 
            mColor = color; 
            return *this;
        }
        SolidColor& SetOpacity(float opacity)
        {
            mColor.SetAlpha(opacity);
            return *this;
        }

        // Enable/Disable transparency.
        SolidColor& EnableTransparency(bool on_off)
        { 
            mTransparency = on_off; 
            return *this;
        }
    private:
        Color4f mColor;
        bool mTransparency = false;
    };

    // This material will map the given texture onto the
    // the shape being drawn. 
    // The object being drawn must provide texture coordinates.
    class TextureMap : public Material
    {
    public:
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

        // Construct with the given texture name.
        // The texture name currently indicates a texture filename
        // relative to the current workig directory.
        TextureMap(const std::string& texture)
            : mSource(new FileSource(texture))
        {}

        template<typename T>
        TextureMap(const Bitmap<T>& bitmap) 
            : mSource(new BitmapSource<T>(bitmap))
        {}
        template<typename T>
        TextureMap(Bitmap<T>&& bitmap) 
            : mSource(new BitmapSource<T>(std::move(bitmap)))
        {}

        // Construct from a custom texture source.
        TextureMap(std::unique_ptr<TextureSource> source) 
            : mSource(std::move(source))
        {}

        // Get/Create the corresponding shader object.
        virtual Shader* GetShader(Device& device) const override
        {
            Shader* shader = device.FindShader("texture_map.glsl");
            if (shader == nullptr || !shader->IsValid())
            {
                if (!shader)
                    shader = device.MakeShader("texture_map.glsl");
                if (!shader->CompileFile("texture_map.glsl"))
                    return nullptr;
            }
            return shader;
        }
        // Apply the material state on program object.
        virtual void Apply(const Environment& env, Device& device, Program& prog) const override
        {
            const auto& name = mSource->GetName();

            Texture* texture = device.FindTexture(name);
            if (texture == nullptr)
            {
                texture = device.MakeTexture(name);
                auto bmp = mSource->GetData();
                const auto width  = bmp->GetWidth();
                const auto height = bmp->GetHeight();
                const auto format = Texture::DepthToFormat(bmp->GetDepthBits());
                texture->Upload(bmp->GetDataPtr(), width, height, format);
            }

            prog.SetTexture("kTexture", 0, *texture);
            prog.SetUniform("kRenderPoints", env.render_points ? 1.0f : 0.0f);
            prog.SetUniform("kBaseColor", mColor.Red(), mColor.Green(),
                mColor.Blue(), mColor.Alpha());
            prog.SetUniform("kGamma", mGamma);

            const float w = texture->GetWidth();
            const float h = texture->GetHeight();
            const float x = mNormalizedBox ? mBox.GetX() : mBox.GetX() / w;
            const float y = mNormalizedBox ? 1.0f - mBox.GetY() : 1.0f - ((mBox.GetY() + mBox.GetHeight()) / h);
            const float sx = mNormalizedBox ? mBox.GetWidth() : mBox.GetWidth() / w;
            const float sy = mNormalizedBox ? mBox.GetHeight() : mBox.GetHeight() / h;
            prog.SetUniform("kTextureBox", x, y, sx, sy);
        }
        // Get material surface type.
        virtual SurfaceType GetSurfaceType() const override
        { return mSurfaceType; }

        // Set the base color for color modulation.
        // Each channel sampled from the texture is multiplied
        // by the base color's corresponding channel value. 
        // This can then  used to modulate the output from
        // texture sampling per channel basis.
        // For example setting base color to (1.0f, 0.0f, 0.0f, 1.0f)
        // would result only R channel having value and the output
        // being in shades of red.
        TextureMap& SetBaseColor(const Color4f& color)
        {
            mColor = color;
            return *this;
        }

        // Set the surface type. This is most likely
        // either Transparent (if the texture contains an alpha channel)
        // or Opaque if no transparent blending is desired.
        TextureMap& SetSurfaceType(SurfaceType type)
        {
            mSurfaceType = type;
            return *this;
        }

        // Set the gamma (in)correction value.
        // Values below 1.0f will result in the rendered image
        // being "brighter" and above 1.0f will make it "darker".
        // The default is 1.0f
        TextureMap& SetGamma(float gamma)
        {
            mGamma = gamma;
            return *this;
        }

        // Set texture sampling subrectangle, i.e. the x,y offset
        // and the width and height of the texture subrect to use.
        // This will set the box in texture units i.e. pixels
        TextureMap& SetRect(const URect& rc)
        {
            mBox = FRect(rc);
            mNormalizedBox = false;
            return *this;
        }
        // Set texture sampling subrectangle, i.e. the x,y offset
        // and the width and height of the texture subrect to use.
        // This will expect the box to be in normalized units.
        TextureMap& SetRect(const FRect& rc)
        {
            mBox = rc;
            mNormalizedBox = true;
            return *this;
        }
        // Set the material alpha/opacity value. 
        // The expected range is between 0.0f and 1.0f
        // 0.0f being completely transparent and 1.0f
        // being completely opaque.
        TextureMap& SetOpacity(float alpha)
        {
            mColor.SetAlpha(alpha);
            return *this;
        }

    private:
        // Source texture data from an image file.
        class FileSource : public TextureSource
        {
        public: 
            FileSource(const std::string& filename)
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
        template<typename T>
        class BitmapSource : public TextureSource
        {
        public:
            BitmapSource(const Bitmap<T>& bmp) : mBitmap(new Bitmap<T>(bmp))
            {}
            BitmapSource(Bitmap<T>&& bmp) : mBitmap(new Bitmap<T>(std::move(bmp)))
            {}
            virtual std::string GetName() const override
            { 
                // generate a name for the bitmap based on the contents.
                if (!mName.empty())
                    return mName;
                
                const auto w = mBitmap->GetWidth();
                const auto h = mBitmap->GetHeight();
                std::size_t hash_value = 0;
                for (size_t y=0; y<h; ++y) 
                {
                    for (size_t x=0; x<w; ++x) 
                    {
                        const auto& pixel = mBitmap->GetPixel(y, x);
                        static_assert(sizeof(pixel) <= sizeof(std::uint32_t), 
                            "pixel doesn't fit in an 32bit integer");
                        std::uint32_t value = 0;
                        std::memcpy(&value, (const void*)&pixel, sizeof(pixel));
                        hash_value ^= std::hash<std::uint32_t>()(value);
                    }
                }
                mName = std::to_string(hash_value);
                return mName;
            }
            virtual std::shared_ptr<IBitmap> GetData() const 
            { return mBitmap; }
        private:
            std::string mName;
            std::shared_ptr<Bitmap<T>> mBitmap;
        };
            
    private:
        std::unique_ptr<TextureSource> mSource;        
        SurfaceType mSurfaceType = SurfaceType::Opaque;
        Color4f mColor = Color4f(1.0f, 1.0f, 1.0f, 1.0f);
        float mGamma   = 1.0f;
        FRect mBox = FRect(0.0f, 0.0f, 1.0f, 1.0f);
        bool  mNormalizedBox = true;
    };

    // SpriteSet is a material which renders a simple animation
    // by cycling through a set of textures at some fps.
    // The assumption is that cycling through the textures
    // renders a coherent animation.
    // In order to blend between the frames of the animation
    // the current time value needs to be set.
    class SpriteSet : public Material
    {
    public:
        // Create a new sprite with an initializer list of texture names.
        SpriteSet(const std::initializer_list<std::string>& textures)
          : mTextures(textures)
        {}
        // Create a new sprite with a vector of texture names.
        SpriteSet(const std::vector<std::string>& textures)
          : mTextures(textures)
        {}
        // Create an empty sprite.
        // You must then manually call AddTexture before using
        // the sprite to render something.
        SpriteSet()
        {}
        // Get/Create the corresponding shader object.
        virtual Shader* GetShader(Device& device) const override
        {
            Shader* shader = device.FindShader("sprite_set.glsl");
            if (shader == nullptr || !shader->IsValid())
            {
                if (shader == nullptr)
                    shader = device.MakeShader("sprite_set.glsl");
                if (!shader->CompileFile("sprite_set.glsl"))
                    return nullptr;
            }
            return shader;
        }
        // Apply the material state on program object.
        virtual void Apply(const Environment& env, Device& device, Program& prog) const override
        {
            assert(!mTextures.empty() &&
                "The sprite has no textures and cannot be used to draw.");

            const auto num_textures = (unsigned)mTextures.size();
            const auto frame_interval = 1.0f / mFps;
            const unsigned frame_index[2] = {
                (unsigned(mRuntime / frame_interval) + 0) % num_textures,
                (unsigned(mRuntime / frame_interval) + 1) % num_textures
            };
            for (unsigned i=0; i<2; ++i)
            {
                const auto index = frame_index[i];
                Texture* texture = device.FindTexture(mTextures[index]);
                if (!texture)
                {
                    texture = device.MakeTexture(mTextures[index]);
                    Image file(mTextures[index]);
                    const auto width  = file.GetWidth();
                    const auto height = file.GetHeight();
                    const auto format = Texture::DepthToFormat(file.GetDepthBits());
                    texture->Upload(file.GetData(), width, height, format);
                }
                const auto sampler = "kTexture" + std::to_string(i);
                prog.SetTexture(sampler.c_str(), i, *texture);
            }

            // blend factor between the two textures.
            const auto coeff = std::fmod(mRuntime, frame_interval) / frame_interval;
            prog.SetUniform("kBlendCoeff", coeff);
            prog.SetUniform("kBaseColor", mColor.Red(), mColor.Green(),
                mColor.Blue(), mColor.Alpha());
            prog.SetUniform("kRenderPoints", env.render_points ? 1.0f : 0.0f);
        }
        // Get material surface type.
        virtual SurfaceType GetSurfaceType() const override
        { return SurfaceType::Transparent; }

        // Set the desired frame rate per second.
        SpriteSet& SetFps(float fps)
        {
            mFps = fps;
            return *this;
        }
        // Set the current runtime. This is used to compute
        // the current texture to be rendered and the blend 
        // coefficient.
        SpriteSet& SetAppRuntime(float time)
        {
            mRuntime = time;
            return *this;
        }
        // Add a new texture name to the sprite.
        SpriteSet& AddTexture(const std::string& texture)
        {
            mTextures.push_back(texture);
            return *this;
        }
        // Set the base color for color modulation.
        // Each channel sampled from the texture is multiplied
        // by the base color's corresponding channel value. 
        // This can then  used to modulate the output from
        // texture sampling per channel basis.
        // For example setting base color to (1.0f, 0.0f, 0.0f, 1.0f)
        // would result only R channel having value and the output
        // being in shades of red.        
        SpriteSet& SetBaseColor(const Color4f& color)
        {
            mColor = color;
            return *this;
        }
    private:
        std::vector<std::string> mTextures;
        float mRuntime = 0;
        float mFps = 1.0f;
        Color4f mColor = Color::White;
    };


    // SpriteMap is a material which renders a simple animation
    // by cycling through different subregions of a single sprite
    // texture at some fps.
    // The assumption is that cycling through the regions
    // renders a coherent animation.
    class SpriteMap : public Material
    {
    public:
        // Frame identifies a sub-region in the given sprite map
        // texture that is used as a single frame in the animation.
        struct Frame {
            // x position (distance from the texture's left edge)
            // of the top left corner of the frame within the texture
            // in units of texels.
            unsigned x = 0;
            // y position (distance from the texture's top left corner)
            // of the top left corner of the frame within the texture
            // in units of texels.
            unsigned y = 0;
            // width of the texture sub-region used as the frame
            // in units of texels.
            unsigned w = 0;
            // height of the texture sub-region used as the frame
            // in units of texels.
            unsigned h = 0;
        };

        // Create a new sprite with texture name and a init list of frames.
        SpriteMap(const std::string& texture, const std::initializer_list<Frame>& frames)
          : mTexture(texture)
          , mFrames(frames)
        {}
        // Create a new sprite with texture name and a vector of frames.
        SpriteMap(const std::string& texture, const std::vector<Frame> frames)
          : mTexture(texture)
          , mFrames(frames)
        {}
        // Create an empty sprite map. You must then manually
        // call AddFrame and SetTexture before the sprite
        // can be used to draw.
        SpriteMap()
        {}
        // Get/Create the corresponding shader object.
        virtual Shader* GetShader(Device& device) const override
        {
            Shader* shader = device.FindShader("sprite_map.glsl");
            if (shader == nullptr || !shader->IsValid())
            {
                if (shader == nullptr)
                    shader = device.MakeShader("sprite_map.glsl");
                if (!shader->CompileFile("sprite_map.glsl"))
                    return nullptr;
            }
            return shader;
        }
        // Apply the material state on program object.
        virtual void Apply(const Environment& env, Device& device, Program& prog) const override
        {
            assert(!mTexture.empty() &&
                "The sprite has no texture and cannot be used to draw.");
            assert(!mFrames.empty() &&
                "The sprite has no frames and cannot be used to draw.");

            const auto num_frames = (unsigned)mFrames.size();
            const auto frame_interval = 1.0f / mFps;
            const unsigned frame_index[2] = {
                (unsigned(mRuntime / frame_interval) + 0) % num_frames,
                (unsigned(mRuntime / frame_interval) + 1) % num_frames
            };
            Texture* texture = device.FindTexture(mTexture);
            if (!texture)
            {
                texture = device.MakeTexture(mTexture);
                Image file(mTexture);
                const auto width  = file.GetWidth();
                const auto height = file.GetHeight();
                const auto format = Texture::DepthToFormat(file.GetDepthBits());
                texture->Upload(file.GetData(), width, height, format);
            }
            const float width  = texture->GetWidth();
            const float height = texture->GetHeight();

            for (unsigned i=0; i<2; ++i)
            {
                const auto index = frame_index[i];
                const auto& frame = mFrames[index];
                const auto& uniform_name = "kTextureFrame" + std::to_string(i);
                const auto x = frame.x / width;
                const auto y = 1.0 - ((frame.y + frame.h) / height);
                const auto w = frame.w / width;
                const auto h = frame.h / height;
                prog.SetUniform(uniform_name.c_str(), x, y, w, h);
            }

            const auto coeff = std::fmod(mRuntime, frame_interval) / frame_interval;
            prog.SetUniform("kBlendCoeff", coeff);
            prog.SetTexture("kTexture", 0, *texture);
            prog.SetUniform("kBaseColor", mColor.Red(), mColor.Green(),
                mColor.Blue(), mColor.Alpha());
        }
        // Get material surface type.
        virtual SurfaceType GetSurfaceType() const override
        { return SurfaceType::Transparent; }

        // Set the texture identifier/name.
        // Currently the meaning of texture name is a path
        // relative to the current working directory.
        SpriteMap& SetTexture(const std::string& texture)
        {
            mTexture = texture;
            return *this;
        }
        // Set the desired frame rate per second.
        SpriteMap& SetFps(float fps)
        {
            mFps = fps;
            return *this;
        }
        // Set the current runtime. This is used to compute
        // the current texture to be rendered and the blend 
        // coefficient.
        SpriteMap& SetAppRuntime(float time)
        {
            mRuntime = time;
            return *this;
        }
        SpriteMap& AddFrame(const Frame& frame)
        {
            mFrames.push_back(frame);
            return *this;
        }
        // Set the base color for color modulation.
        // Each channel sampled from the texture is multiplied
        // by the base color's corresponding channel value. 
        // This can then  used to modulate the output from
        // texture sampling per channel basis.
        // For example setting base color to (1.0f, 0.0f, 0.0f, 1.0f)
        // would result only R channel having value and the output
        // being in shades of red.
        SpriteMap& SetBaseColor(const Color4f& color)
        {
            mColor = color;
            return *this;
        }
    private:
        std::string mTexture;
        std::vector<Frame> mFrames;
        float mRuntime = 0.0f;
        float mFps = 1.0f;
        Color4f mColor = Color::White;
    };

    // This material will use the given text buffer object to
    // display text by rasterizing the text and creating a new
    // texture object of it. The resulting texture object is then
    // mapped onto the shape being drawn.
    // The drawable shape must provide texture coordinates.
    class BitmapText : public Material
    {
    public:
        // Construct with the reference to the texture buffer.
        BitmapText(const TextBuffer& text) : mText(text)
        {}
        // Get/Create the corresponding shader object.
        virtual Shader* GetShader(Device& device) const override
        {
            Shader* shader = device.FindShader("texture_alpha_mask.glsl");
            if (shader == nullptr || !shader->IsValid())
            {
                if (!shader)
                    shader = device.MakeShader("texture_alpha_mask.glsl");
                if (!shader->CompileFile("texture_alpha_mask.glsl"));
                    return nullptr;
            }
            return shader;
        }
        // Apply the material state on the program object.
        virtual void Apply(const Environment& env, Device& device, Program& prog) const override
        {
            const auto hash = mText.GetHash();
            const auto name = std::to_string(hash);

            Texture* texture = device.FindTexture(name);
            if (!texture) 
            {
                texture = device.MakeTexture(name);
                texture->EnableGarbageCollection(true);
                const auto& bmp = mText.Rasterize();
                texture->Upload(bmp->GetData(), bmp->GetWidth(), bmp->GetHeight(), 
                    Texture::Format::Grayscale);                                
            }

            // trigger reupload if the dimensions have changed.
            const auto width  = mText.GetWidth();
            const auto height = mText.GetHeight();
            const auto prev_width = texture->GetWidth();
            const auto prev_height = texture->GetHeight();
            if (prev_width != width || prev_height != height) 
            {
                const auto& bmp = mText.Rasterize();
                texture->Upload(bmp->GetData(), width, height, Texture::Format::Grayscale);
            }                
            const float secs = base::GetRuntimeSec();

            prog.SetTexture("kTexture", 0, *texture);
            prog.SetUniform("kRenderPoints", 0.0f);
            prog.SetUniform("kBaseColor", mColor.Red(), mColor.Green(), 
                mColor.Blue(), mColor.Alpha());
            prog.SetUniform("kGamma", mGamma);
            prog.SetUniform("kTimeSecs", secs);  
            prog.SetUniform("kBlinkPeriod", mBlinkText ? 1.5f :0.0f);          
        }
        // Get material surface type.
        virtual SurfaceType GetSurfaceType() const override
        { return SurfaceType::Transparent; }

        // Set the color for the text being rendered.
        // The rasterized text will contain AA alpha mask
        // which is used to modulate the output color values.
        // In other words pixels (fragments) that are not
        // covered by rasterized text will have 1 alpha from
        // the mask and be fully transparent and pixels that
        // are fully covered by the rasterized text will have 0 alpha
        // and be fully opaque. The rest of the pixels are somewhere
        // in between i.e partially transparent/opaque.
        BitmapText& SetBaseColor(const Color4f& color)
        {
            mColor = color;
            return *this;
        }
        // Set the gamma (in)correction value.
        // Values below 1.0f will result in the rendered image
        // being "brighter" and above 1.0f will make it "darker".
        // The default is 1.0f        
        BitmapText& SetGamma(float gamma)
        {
            mGamma = gamma;
            return *this;
        }

        BitmapText& SetBlinkText(bool blink)
        {
            mBlinkText = blink;
            return *this;
        }

    private:
        const TextBuffer& mText;
        Color4f mColor = Color::White;
        float mGamma   = 1.0f;
        bool mBlinkText = false;
    };


    // base class for material effects.
    class MaterialEffect : public Material
    {
    public:
        virtual Shader* GetShader(Device& device) const override
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

        virtual void Apply(const Environment& env, Device&, Program& prog) const override
        {
            prog.SetUniform("uRuntime", mRuntime);
            prog.SetUniform("kBaseColor", mBaseColor.Red(), mBaseColor.Green(),
                mBaseColor.Blue(), mBaseColor.Alpha());
        }
        virtual SurfaceType GetSurfaceType() const override
        { return SurfaceType::Transparent; }

        MaterialEffect& SetBaseColor(const Color4f& color)
        {
            mBaseColor = color;
            return *this;
        }

    protected:
        MaterialEffect(const std::string& shader, float runtime)
          : mShader(shader)
          , mRuntime(runtime)
        {}

    private:
        const std::string mShader;
        const float mRuntime = 0.0f;
        Color4f mBaseColor = Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    };

    class SlidingGlintEffect : public MaterialEffect
    {
    public:
        SlidingGlintEffect(float runtime)
          : MaterialEffect("sliding_glint_effect.glsl", runtime)
        {}
    private:
    };

    class ConcentricRingsEffect : public MaterialEffect
    {
    public:
        ConcentricRingsEffect(float runtime)
          : MaterialEffect("concentric_rings_effect.glsl", runtime)
        {}
    private:
    };

} // namespace
