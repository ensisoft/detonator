// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
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

#include "config.h"

#include "warnpush.h"
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <iostream>
#include <unordered_map>
#include <string>
#include <any>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "graphics/types.h"
#include "graphics/material.h"
#include "graphics/device.h"
#include "graphics/drawable.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/texture.h"
#include "graphics/geometry.h"

bool operator==(const gfx::Color4f& lhs, const gfx::Color4f& rhs)
{
    return real::equals(lhs.Red(), rhs.Red()) &&
           real::equals(lhs.Green(), rhs.Green()) &&
           real::equals(lhs.Blue(), rhs.Blue()) &&
           real::equals(lhs.Alpha(), rhs.Alpha());
}

class TestShader : public gfx::Shader
{
public:
    virtual bool CompileSource(const std::string& source) override
    {
        mSource = source;
        return true;
    }
    virtual bool CompileFile(const std::string& file) override
    {
        mFilename = file;
        return true;
    }
    virtual bool IsValid() const override
    { return true; }

    std::string GetFilename() const
    { return mFilename; }

    std::string GetSource() const
    { return mSource; }
private:
    std::string mFilename;
    std::string mSource;
};

class TestTexture : public gfx::Texture
{
public:
    virtual void SetFilter(MinFilter filter) override
    { mMinFilter = filter; }
    virtual void SetFilter(MagFilter filter) override
    { mMagFilter = filter; }
    virtual MinFilter GetMinFilter() const override
    { return mMinFilter; }
    virtual MagFilter GetMagFilter() const override
    { return mMagFilter; }
    virtual void SetWrapX(Wrapping w) override
    { mWrapX = w; }
    virtual void SetWrapY(Wrapping w) override
    { mWrapY = w; }
    virtual Wrapping GetWrapX() const override
    { return mWrapX; }
    virtual Wrapping GetWrapY() const override
    { return mWrapY; }
    virtual void Upload(const void* bytes, unsigned xres, unsigned yres, Format format) override
    {
        mWidth  = xres;
        mHeight = yres;
        mFormat = format;
    }
    virtual unsigned GetWidth() const override
    { return mWidth; }
    virtual unsigned GetHeight() const override
    { return mHeight; }
    virtual Format GetFormat() const override
    { return mFormat; }
    virtual void EnableGarbageCollection(bool gc) override
    {}
private:
    unsigned mWidth  = 0;
    unsigned mHeight = 0;
    Format mFormat  = Format::Grayscale;
    Wrapping mWrapX = Wrapping::Repeat;
    Wrapping mWrapY = Wrapping::Repeat;
    MinFilter mMinFilter = MinFilter::Default;
    MagFilter mMagFilter = MagFilter::Default;
};

class TestProgram : public gfx::Program
{
public:
    virtual bool Build(const std::vector<const gfx::Shader*>& shaders) override
    {
        return true;
    }
    virtual bool IsValid() const override
    { return true; }

    virtual void SetUniform(const char* name, int x) override
    {
        mUniforms[name] = glm::ivec1(x);
    }
    virtual void SetUniform(const char* name, int x, int y) override
    {
        mUniforms[name] = glm::ivec2(x, y);
    }
    virtual void SetUniform(const char* name, float x) override
    {
        mUniforms[name] = glm::vec1(x);
    }
    virtual void SetUniform(const char* name, float x, float y) override
    {
        mUniforms[name] = glm::vec2(x, y);
    }
    virtual void SetUniform(const char* name, float x, float y, float z) override
    {
        mUniforms[name] = glm::vec3(x, y, z);
    }
    virtual void SetUniform(const char* name, float x, float y, float z, float w) override
    {
        mUniforms[name] = glm::vec4(x, y, z, w);
    }
    virtual void SetUniform(const char* name, const gfx::Color4f& color) override
    {
        mUniforms[name] = color;
    }
    virtual void SetUniform(const char* name, const Matrix2x2& matrix) override
    {}
    virtual void SetUniform(const char* name, const Matrix3x3& matrix) override
    {}
    virtual void SetUniform(const char* name, const Matrix4x4& matrix) override
    {}

    struct TextureBinding {
        const TestTexture* texture = nullptr;
        unsigned unit = 0;
        std::string sampler;
    };

    virtual void SetTexture(const char* sampler, unsigned unit, const gfx::Texture& texture) override
    {
        TextureBinding binding;
        binding.sampler = sampler;
        binding.unit    = unit;
        binding.texture = static_cast<const TestTexture*>(&texture);
        mTextures.push_back(binding);
    }

    template<typename T>
    bool GetUniform(const char* name, T* out) const
    {
        auto it = mUniforms.find(name);
        if (it == mUniforms.end())
            return false;
        *out = std::any_cast<T>(it->second);
        return true;
    }
    bool HasUniform(const char* name) const
    {
        auto it = mUniforms.find(name);
        if (it != mUniforms.end())
            return true;
        return false;
    }
    bool HasUniforms() const
    { return !mUniforms.empty(); }

    void Clear()
    {
        mUniforms.clear();
        mTextures.clear();
    }

    const TextureBinding& GetTextureBinding(size_t index) const
    {
        TEST_REQUIRE(index <mTextures.size());
        return mTextures[index];
    }
    const TestTexture& GetTexture(size_t index) const
    {
        TEST_REQUIRE(index < mTextures.size());
        return *mTextures[index].texture;
    }

private:
    std::unordered_map<std::string, std::any> mUniforms;
    std::vector<TextureBinding> mTextures;
};


class TestGeometry : public gfx::Geometry
{
public:
    virtual void ClearDraws() override
    {}
    virtual void AddDrawCmd(DrawType type) override
    {}
    virtual void AddDrawCmd(DrawType type, size_t offset, size_t count) override
    {}
    virtual void SetLineWidth(float width) override
    {}
    virtual void SetVertexBuffer(std::unique_ptr<gfx::VertexBuffer> buffer) override
    {}
    virtual void SetVertexLayout(const gfx::VertexLayout& layout) override
    {}
private:
};

class TestDevice : public gfx::Device
{
public:
    virtual void ClearColor(const gfx::Color4f& color) override
    {}
    virtual void ClearStencil(int value) override
    {}

    virtual void SetDefaultTextureFilter(MinFilter filter) override
    {}
    virtual void SetDefaultTextureFilter(MagFilter filter) override
    {}

    // resource creation APIs
    virtual gfx::Shader* FindShader(const std::string& name) override
    {
        auto it = mShaderIndexMap.find(name);
        if (it == mShaderIndexMap.end())
            return nullptr;
        return mShaders[it->second].get();
    }
    virtual gfx::Shader* MakeShader(const std::string& name) override
    {
        const size_t index = mShaders.size();
        mShaders.emplace_back(new TestShader);
        mShaderIndexMap[name] = index;
        return mShaders.back().get();
    }
    virtual gfx::Program* FindProgram(const std::string& name) override
    {
        return nullptr;
    }
    virtual gfx::Program* MakeProgram(const std::string& name) override
    {
        return nullptr;
    }
    virtual gfx::Geometry* FindGeometry(const std::string& name) override
    {
        return nullptr;
    }
    virtual gfx::Geometry* MakeGeometry(const std::string& name) override
    {
        return nullptr;
    }
    virtual gfx::Texture* FindTexture(const std::string& name) override
    {
        auto it = mTextureIndexMap.find(name);
        if (it == mTextureIndexMap.end())
            return nullptr;
        return mTextures[it->second].get();
    }
    virtual gfx::Texture* MakeTexture(const std::string& name) override
    {
        const size_t index = mTextures.size();
        mTextures.emplace_back(new TestTexture);
        mTextureIndexMap[name] = index;
        return mTextures.back().get();
    }
    // Resource deletion APIs
    virtual void DeleteShaders() override
    {}
    virtual void DeletePrograms() override
    {}
    virtual void DeleteGeometries() override
    {}
    virtual void DeleteTextures() override
    {}

    virtual void Draw(const gfx::Program& program, const gfx::Geometry& geometry, const State& state) override
    {}

    virtual Type GetDeviceType() const override
    { return Type::OpenGL_ES2; }
    virtual void CleanGarbage(size_t) override
    {}

    virtual void BeginFrame() override
    {}
    virtual void EndFrame(bool display) override
    {}
    virtual gfx::Bitmap<gfx::RGBA> ReadColorBuffer(unsigned width, unsigned height) const override
    {
        gfx::Bitmap<gfx::RGBA> bitmap;
        bitmap.Resize(width, height);
        bitmap.Fill(gfx::Color::DarkGreen);
        return bitmap;
    }
    virtual gfx::Bitmap<gfx::RGBA> ReadColorBuffer(unsigned x, unsigned y,
                                                   unsigned width, unsigned height) const override
    {
        gfx::Bitmap<gfx::RGBA> bitmap;
        bitmap.Resize(width, height);
        bitmap.Fill(gfx::Color::DarkGreen);
        return bitmap;
    }

    const TestTexture& GetTexture(size_t index) const
    {
        TEST_REQUIRE(index < mTextures.size());
        return *mTextures[index].get();
    }

    const TestShader& GetShader(size_t index) const
    {
        TEST_REQUIRE(index < mShaders.size());
        return *mShaders[index].get();
    }

    void Clear()
    {
        mTextureIndexMap.clear();
        mTextures.clear();
        mShaderIndexMap.clear();
        mShaders.clear();
    }

private:
    std::unordered_map<std::string, std::size_t> mTextureIndexMap;
    std::vector<std::unique_ptr<TestTexture>> mTextures;

    std::unordered_map<std::string, std::size_t> mShaderIndexMap;
    std::vector<std::unique_ptr<TestShader>> mShaders;
};


void unit_test_material_uniforms()
{
    // test dynamic program uniforms.
    {
        TestDevice device;
        TestProgram program;
        gfx::MaterialClass test;

        test.SetType(gfx::MaterialClass::Type::Sprite);
        test.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        test.SetBaseColor(gfx::Color::Green);
        test.SetColorMapColor(gfx::Color::DarkBlue, gfx::MaterialClass::ColorIndex::BottomLeft);
        test.SetColorMapColor(gfx::Color::DarkGreen,gfx::MaterialClass::ColorIndex::TopLeft);
        test.SetColorMapColor(gfx::Color::DarkMagenta, gfx::MaterialClass::ColorIndex::BottomRight);
        test.SetColorMapColor(gfx::Color::DarkGray, gfx::MaterialClass::ColorIndex::TopRight);
        test.SetGamma(2.0f);
        test.SetTextureScaleX(2.0f);
        test.SetTextureScaleY(3.0f);
        test.SetTextureVelocityX(4.0f);
        test.SetTextureVelocityY(5.0f);
        test.SetTextureVelocityZ(-1.0f);
        test.SetStatic(false);
        test.ApplyStaticState(device, program);
        TEST_REQUIRE(program.HasUniforms() == false);

        // check that the dynamic state is set as expected.
        // this should mean that both static uniforms  and dynamic
        // uniforms are set.
        gfx::MaterialClass::Environment env;
        env.render_points = false;
        gfx::MaterialClass::InstanceState instance;
        instance.base_color = gfx::Color::HotPink;
        instance.runtime    = 2.0f;
        gfx::MaterialClass::RasterState raster;
        test.ApplyDynamicState(env, instance, device, program, raster);

        glm::vec1 particle_rotation_flag;
        glm::vec1 render_points_flag;
        glm::vec2 texture_alpha_mask_flags;
        glm::vec1 runtime;
        gfx::Color4f base_color;
        gfx::Color4f gradients[4];
        glm::vec2 texture_scale;
        glm::vec2 texture_velocity_xy;
        glm::vec1 texture_velocity_z;
        glm::vec1 gamma;
        TEST_REQUIRE(program.GetUniform("kBaseColor", &base_color));
        TEST_REQUIRE(program.GetUniform("kGamma", &gamma));
        TEST_REQUIRE(program.GetUniform("kTextureScale", &texture_scale));
        TEST_REQUIRE(program.GetUniform("kTextureVelocityXY", &texture_velocity_xy));
        TEST_REQUIRE(program.GetUniform("kTextureVelocityZ", &texture_velocity_z));
        TEST_REQUIRE(program.GetUniform("kColor0", &gradients[0]));
        TEST_REQUIRE(program.GetUniform("kColor1", &gradients[1]));
        TEST_REQUIRE(program.GetUniform("kColor2", &gradients[2]));
        TEST_REQUIRE(program.GetUniform("kColor3", &gradients[3]));
        TEST_REQUIRE(program.GetUniform("kApplyRandomParticleRotation", &particle_rotation_flag));
        TEST_REQUIRE(program.GetUniform("kRenderPoints", &render_points_flag));
        TEST_REQUIRE(program.GetUniform("kIsAlphaMask", &texture_alpha_mask_flags));
        TEST_REQUIRE(program.GetUniform("kRuntime", &runtime));
        TEST_REQUIRE(base_color == gfx::Color::HotPink); // instance variable!
        TEST_REQUIRE(gradients[(int)gfx::MaterialClass::ColorIndex::BottomLeft] == gfx::Color::DarkBlue);
        TEST_REQUIRE(gradients[(int)gfx::MaterialClass::ColorIndex::TopLeft] == gfx::Color::DarkGreen);
        TEST_REQUIRE(gradients[(int)gfx::MaterialClass::ColorIndex::BottomRight] == gfx::Color::DarkMagenta);
        TEST_REQUIRE(gradients[(int)gfx::MaterialClass::ColorIndex::TopRight] == gfx::Color::DarkGray);
        TEST_REQUIRE(texture_scale == glm::vec2(2.0f, 3.0f));
        TEST_REQUIRE(texture_velocity_xy == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(texture_velocity_z == glm::vec1(-1.0f));
        TEST_REQUIRE(gamma == glm::vec1(2.0f));
        TEST_REQUIRE(particle_rotation_flag == glm::vec1(0.0f));
        TEST_REQUIRE(render_points_flag == glm::vec1(0.0f));
        TEST_REQUIRE(texture_alpha_mask_flags == glm::vec2(0.0f, 0.0f));
        TEST_REQUIRE(runtime == glm::vec1(2.0f));
    }

    // test static program uniforms.
    {
        TestDevice device;
        TestProgram program;
        gfx::MaterialClass test;

        test.SetType(gfx::MaterialClass::Type::Sprite);
        test.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        test.SetBaseColor(gfx::Color::Green);
        test.SetColorMapColor(gfx::Color::DarkBlue, gfx::MaterialClass::ColorIndex::BottomLeft);
        test.SetColorMapColor(gfx::Color::DarkGreen,gfx::MaterialClass::ColorIndex::TopLeft);
        test.SetColorMapColor(gfx::Color::DarkMagenta, gfx::MaterialClass::ColorIndex::BottomRight);
        test.SetColorMapColor(gfx::Color::DarkGray, gfx::MaterialClass::ColorIndex::TopRight);
        test.SetGamma(2.0f);
        test.SetTextureScaleX(2.0f);
        test.SetTextureScaleY(3.0f);
        test.SetTextureVelocityX(4.0f);
        test.SetTextureVelocityY(5.0f);
        test.SetTextureVelocityZ(-1.0f);
        test.SetStatic(true);
        test.ApplyStaticState(device, program);

        // check that the static state is applied as expected.
        gfx::Color4f base_color;
        gfx::Color4f gradients[4];
        glm::vec2 texture_scale;
        glm::vec2 texture_velocity_xy;
        glm::vec1 texture_velocity_z;
        glm::vec1 gamma;
        TEST_REQUIRE(program.GetUniform("kBaseColor", &base_color));
        TEST_REQUIRE(program.GetUniform("kGamma", &gamma));
        TEST_REQUIRE(program.GetUniform("kTextureScale", &texture_scale));
        TEST_REQUIRE(program.GetUniform("kTextureVelocityXY", &texture_velocity_xy));
        TEST_REQUIRE(program.GetUniform("kTextureVelocityZ", &texture_velocity_z));
        TEST_REQUIRE(program.GetUniform("kColor0", &gradients[0]));
        TEST_REQUIRE(program.GetUniform("kColor1", &gradients[1]));
        TEST_REQUIRE(program.GetUniform("kColor2", &gradients[2]));
        TEST_REQUIRE(program.GetUniform("kColor3", &gradients[3]));
        TEST_REQUIRE(base_color == gfx::Color::Green);
        TEST_REQUIRE(gradients[(int)gfx::MaterialClass::ColorIndex::BottomLeft] == gfx::Color::DarkBlue);
        TEST_REQUIRE(gradients[(int)gfx::MaterialClass::ColorIndex::TopLeft] == gfx::Color::DarkGreen);
        TEST_REQUIRE(gradients[(int)gfx::MaterialClass::ColorIndex::BottomRight] == gfx::Color::DarkMagenta);
        TEST_REQUIRE(gradients[(int)gfx::MaterialClass::ColorIndex::TopRight] == gfx::Color::DarkGray);
        TEST_REQUIRE(texture_scale == glm::vec2(2.0f, 3.0f));
        TEST_REQUIRE(texture_velocity_xy == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(texture_velocity_z == glm::vec1(-1.0f));
        TEST_REQUIRE(gamma == glm::vec1(2.0f));
        program.Clear();

        // check that the dynamic state is set as expected.
        // this should mean that the static uniforms are *not* set
        // but only dynamic ones are.
        gfx::MaterialClass::Environment env;
        env.render_points = false;
        gfx::MaterialClass::InstanceState instance;
        instance.base_color = gfx::Color::HotPink;
        instance.runtime    = 2.0f;
        gfx::MaterialClass::RasterState raster;
        test.ApplyDynamicState(env, instance, device, program, raster);

        glm::vec1 particle_rotation_flag;
        glm::vec1 render_points_flag;
        glm::vec2 texture_alpha_mask_flags;
        glm::vec1 runtime;
        TEST_REQUIRE(program.GetUniform("kApplyRandomParticleRotation", &particle_rotation_flag));
        TEST_REQUIRE(program.GetUniform("kRenderPoints", &render_points_flag));
        TEST_REQUIRE(program.GetUniform("kIsAlphaMask", &texture_alpha_mask_flags));
        TEST_REQUIRE(program.GetUniform("kRuntime", &runtime));
        TEST_REQUIRE(particle_rotation_flag == glm::vec1(0.0f));
        TEST_REQUIRE(render_points_flag == glm::vec1(0.0f));
        TEST_REQUIRE(texture_alpha_mask_flags == glm::vec2(0.0f, 0.0f));
        TEST_REQUIRE(runtime == glm::vec1(2.0f));
        // these should not be set.
        TEST_REQUIRE(!program.HasUniform("kBaseColor"));
        TEST_REQUIRE(!program.HasUniform("kGamma"));
        TEST_REQUIRE(!program.HasUniform("kTextureScale"));
        TEST_REQUIRE(!program.HasUniform("kTextureVelocityXY"));
        TEST_REQUIRE(!program.HasUniform("kTextureVelocityZ"));
        TEST_REQUIRE(!program.HasUniform("kColor0"));
        TEST_REQUIRE(!program.HasUniform("kColor1"));
        TEST_REQUIRE(!program.HasUniform("kColor2"));
        TEST_REQUIRE(!program.HasUniform("kColor3"));
    }

    // test that static programs generate different program ID
    // based on their static state even if the underlying shader
    // program has the same type.
    {
        gfx::MaterialClass foo;
        foo.SetType(gfx::MaterialClass::Type::Sprite);
        foo.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        foo.SetStatic(true);
        foo.SetBaseColor(gfx::Color::Red);
        foo.SetColorMapColor(gfx::Color::DarkBlue, gfx::MaterialClass::ColorIndex::BottomLeft);
        foo.SetColorMapColor(gfx::Color::DarkGreen,gfx::MaterialClass::ColorIndex::TopLeft);
        foo.SetColorMapColor(gfx::Color::DarkMagenta, gfx::MaterialClass::ColorIndex::BottomRight);
        foo.SetColorMapColor(gfx::Color::DarkGray, gfx::MaterialClass::ColorIndex::TopRight);
        foo.SetGamma(2.0f);
        foo.SetTextureScaleX(2.0f);
        foo.SetTextureScaleY(3.0f);
        foo.SetTextureVelocityX(4.0f);
        foo.SetTextureVelocityY(5.0f);
        foo.SetTextureVelocityZ(-1.0f);

        gfx::MaterialClass bar = foo;
        TEST_REQUIRE(foo.GetProgramId() == bar.GetProgramId());

        bar.SetBaseColor(gfx::Color::Green);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
        bar = foo;
        foo.SetColorMapColor(gfx::Color::White, gfx::MaterialClass::ColorIndex::BottomLeft);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
        bar = foo;
        foo.SetColorMapColor(gfx::Color::White,gfx::MaterialClass::ColorIndex::TopLeft);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
        bar = foo;
        foo.SetColorMapColor(gfx::Color::White, gfx::MaterialClass::ColorIndex::BottomRight);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
        bar = foo;
        foo.SetColorMapColor(gfx::Color::White, gfx::MaterialClass::ColorIndex::TopRight);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
        bar = foo;
        foo.SetGamma(2.5f);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
        bar = foo;
        foo.SetTextureScaleX(2.2f);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
        bar = foo;
        foo.SetTextureScaleY(2.0f);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
        bar = foo;
        foo.SetTextureVelocityX(4.1f);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
        bar = foo;
        foo.SetTextureVelocityY(-5.0f);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
        bar = foo;
        foo.SetTextureVelocityZ(1.0f);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
    }
}

void unit_test_material_textures()
{
    // test setting basic texture properties.
    {
        TestDevice device;
        TestProgram program;

        gfx::MaterialClass test;
        test.SetType(gfx::MaterialClass::Type::Texture);
        test.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Nearest);
        test.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Trilinear);
        test.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
        test.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);

        gfx::RgbBitmap bitmap;
        bitmap.Resize(100, 80);
        test.AddTexture(bitmap);

        gfx::MaterialClass::Environment env;
        gfx::MaterialClass::RasterState raster;
        gfx::MaterialClass::InstanceState instance;
        instance.runtime = 1.0f;
        instance.base_color = gfx::Color::White;
        test.ApplyDynamicState(env, instance, device, program, raster);

        const auto& texture = device.GetTexture(0);
        TEST_REQUIRE(texture.GetHeight() == 80);
        TEST_REQUIRE(texture.GetWidth() == 100);
        TEST_REQUIRE(texture.GetFormat() == gfx::Texture::Format::RGB);
        TEST_REQUIRE(texture.GetMinFilter() == gfx::Texture::MinFilter::Trilinear);
        TEST_REQUIRE(texture.GetMagFilter() == gfx::Texture::MagFilter::Nearest);
        TEST_REQUIRE(texture.GetWrapX() == gfx::Texture::Wrapping::Clamp);
        TEST_REQUIRE(texture.GetWrapY() == gfx::Texture::Wrapping::Clamp);
        TEST_REQUIRE(program.GetTextureBinding(0).unit == 0);
        TEST_REQUIRE(program.GetTextureBinding(0).texture == &texture);
    }

    // test cycling through sprite textures.
    // one texture, both texture bindings are using the
    // same texture.
    {
        gfx::MaterialClass test;
        test.SetType(gfx::MaterialClass::Type::Sprite);
        test.SetFps(1.0f); // 1 frame per second.

        gfx::RgbBitmap bitmap;
        bitmap.Resize(10, 10);
        test.AddTexture(bitmap);

        TestDevice device;
        TestProgram program;
        {
            gfx::MaterialClass::Environment env;
            gfx::MaterialClass::RasterState raster;
            gfx::MaterialClass::InstanceState instance;
            instance.runtime = 0.0f;
            instance.base_color = gfx::Color::White;
            test.ApplyDynamicState(env, instance, device, program, raster);
            const auto &texture = device.GetTexture(0);
            TEST_REQUIRE(program.GetTextureBinding(0).unit == 0);
            TEST_REQUIRE(program.GetTextureBinding(0).texture == &texture);
            TEST_REQUIRE(program.GetTextureBinding(1).unit == 1);
            TEST_REQUIRE(program.GetTextureBinding(1).texture == &texture);
            glm::vec1 blend_factor;
            TEST_REQUIRE(program.GetUniform("kBlendCoeff", &blend_factor));
            TEST_REQUIRE(blend_factor == glm::vec1(0.0f));
        }
        program.Clear();
        {
            gfx::MaterialClass::Environment env;
            gfx::MaterialClass::RasterState raster;
            gfx::MaterialClass::InstanceState instance;
            instance.runtime = 1.5f;
            instance.base_color = gfx::Color::White;
            test.ApplyDynamicState(env, instance, device, program, raster);
            const auto &texture = device.GetTexture(0);
            TEST_REQUIRE(program.GetTextureBinding(0).unit == 0);
            TEST_REQUIRE(program.GetTextureBinding(0).texture == &texture);
            TEST_REQUIRE(program.GetTextureBinding(1).unit == 1);
            TEST_REQUIRE(program.GetTextureBinding(1).texture == &texture);
            glm::vec1 blend_factor;
            TEST_REQUIRE(program.GetUniform("kBlendCoeff", &blend_factor));
            TEST_REQUIRE(blend_factor == glm::vec1(0.5f));
        }
    }

    // 2 textures.
    {
        gfx::MaterialClass test;
        test.SetType(gfx::MaterialClass::Type::Sprite);
        test.SetFps(1.0f); // 1 frame per second.

        gfx::RgbBitmap one;
        one.Resize(10, 10);
        one.Fill(gfx::Color::Red);
        gfx::RgbBitmap two;
        two.Resize(10, 10);
        two.Fill(gfx::Color::Green);

        test.AddTexture(one);
        test.AddTexture(two);

        TestDevice device;
        TestProgram program;
        {
            gfx::MaterialClass::Environment env;
            gfx::MaterialClass::RasterState raster;
            gfx::MaterialClass::InstanceState instance;
            instance.runtime = 0.0f;
            instance.base_color = gfx::Color::White;
            test.ApplyDynamicState(env, instance, device, program, raster);
            const auto& tex0 = device.GetTexture(0);
            const auto& tex1 = device.GetTexture(1);
            TEST_REQUIRE(program.GetTextureBinding(0).unit == 0);
            TEST_REQUIRE(program.GetTextureBinding(0).texture == &tex0);
            TEST_REQUIRE(program.GetTextureBinding(1).unit == 1);
            TEST_REQUIRE(program.GetTextureBinding(1).texture == &tex1);
        }
        program.Clear();
        {
            gfx::MaterialClass::Environment env;
            gfx::MaterialClass::RasterState raster;
            gfx::MaterialClass::InstanceState instance;
            instance.runtime = 1.0f;
            instance.base_color = gfx::Color::White;
            test.ApplyDynamicState(env, instance, device, program, raster);
            const auto& tex0 = device.GetTexture(0);
            const auto& tex1 = device.GetTexture(1);
            TEST_REQUIRE(program.GetTextureBinding(0).unit == 0);
            TEST_REQUIRE(program.GetTextureBinding(0).texture == &tex1);
            TEST_REQUIRE(program.GetTextureBinding(1).unit == 1);
            TEST_REQUIRE(program.GetTextureBinding(1).texture == &tex0);
        }
    }

    // 3 textures.
    {
        gfx::MaterialClass test;
        test.SetType(gfx::MaterialClass::Type::Sprite);
        test.SetFps(1.0f); // 1 frame per second.

        gfx::RgbBitmap one;
        one.Resize(10, 10);
        one.Fill(gfx::Color::Red);
        gfx::RgbBitmap two;
        two.Resize(10, 10);
        two.Fill(gfx::Color::Green);
        gfx::RgbBitmap three;
        two.Resize(10, 10);
        two.Fill(gfx::Color::Yellow);

        test.AddTexture(one);
        test.AddTexture(two);
        test.AddTexture(three);

        TestDevice device;
        TestProgram program;
        {
            gfx::MaterialClass::Environment env;
            gfx::MaterialClass::RasterState raster;
            gfx::MaterialClass::InstanceState instance;
            instance.runtime = 0.0f;
            instance.base_color = gfx::Color::White;
            test.ApplyDynamicState(env, instance, device, program, raster);
            const auto& tex0 = device.GetTexture(0);
            const auto& tex1 = device.GetTexture(1);
            TEST_REQUIRE(program.GetTextureBinding(0).unit == 0);
            TEST_REQUIRE(program.GetTextureBinding(0).texture == &tex0);
            TEST_REQUIRE(program.GetTextureBinding(1).unit == 1);
            TEST_REQUIRE(program.GetTextureBinding(1).texture == &tex1);
        }
        program.Clear();
        {
            gfx::MaterialClass::Environment env;
            gfx::MaterialClass::RasterState raster;
            gfx::MaterialClass::InstanceState instance;
            instance.runtime = 1.0f;
            instance.base_color = gfx::Color::White;
            test.ApplyDynamicState(env, instance, device, program, raster);
            const auto& tex0 = device.GetTexture(0);
            const auto& tex1 = device.GetTexture(1);
            const auto& tex2 = device.GetTexture(2);
            TEST_REQUIRE(program.GetTextureBinding(0).unit == 0);
            TEST_REQUIRE(program.GetTextureBinding(0).texture == &tex1);
            TEST_REQUIRE(program.GetTextureBinding(1).unit == 1);
            TEST_REQUIRE(program.GetTextureBinding(1).texture == &tex2);
        }
        program.Clear();
        {
            gfx::MaterialClass::Environment env;
            gfx::MaterialClass::RasterState raster;
            gfx::MaterialClass::InstanceState instance;
            instance.runtime = 2.5f;
            instance.base_color = gfx::Color::White;
            test.ApplyDynamicState(env, instance, device, program, raster);
            const auto& tex0 = device.GetTexture(0);
            const auto& tex1 = device.GetTexture(1);
            const auto& tex2 = device.GetTexture(2);
            TEST_REQUIRE(program.GetTextureBinding(0).unit == 0);
            TEST_REQUIRE(program.GetTextureBinding(0).texture == &tex2);
            TEST_REQUIRE(program.GetTextureBinding(1).unit == 1);
            TEST_REQUIRE(program.GetTextureBinding(1).texture == &tex0);
        }
    }
}

void unit_test_material_uniform_folding()
{
    // fold uniforms into consts in the GLSL when the material is
    // marked static.

    // dummy shader source which we write to the disk now
    // in order to simplify the deployment of the unit test.
    const std::string& original =
            R"(#version 100
precision highp float;
uniform sampler2D kTexture0;
uniform sampler2D kTexture1;
uniform float kGamma;
uniform float kRuntime;
uniform vec2 kTextureScale;
uniform vec2 kTextureVelocityXY;
uniform float kTextureVelocityZ;
uniform vec4 kBaseColor;
uniform vec4 kColor0;
uniform vec4 kColor1;
uniform vec4 kColor2;
uniform vec4 kColor3;

const float MyConst1 = 1.24;
const vec2 MyConst2 = vec2(1.0, 2.0);

#define FOO 12.3

varying vec2 vTexCoord;
varying float vAlpha;

void main() {
   vec4 tex0 = texture2D(kTexture0, vTexCoord);
   vec4 tex1 = texture2D(kTexture1, vTexCoord);
   gl_FragColor = pow(vec4(1.0), vec4(kGamma));
}
)";

    const std::string& expected =
            R"(#version 100
precision highp float;
uniform sampler2D kTexture0;
uniform sampler2D kTexture1;
const float kGamma = 0.80;
uniform float kRuntime;
const vec2 kTextureScale = vec2(2.00,3.00);
const vec2 kTextureVelocityXY = vec2(4.00,5.00);
const float kTextureVelocityZ = -1.00;
const vec4 kBaseColor = vec4(1.00,1.00,1.00,1.00);
const vec4 kColor0 = vec4(0.00,1.00,0.00,1.00);
const vec4 kColor1 = vec4(1.00,1.00,1.00,1.00);
const vec4 kColor2 = vec4(0.00,0.00,1.00,1.00);
const vec4 kColor3 = vec4(1.00,0.00,0.00,1.00);

const float MyConst1 = 1.24;
const vec2 MyConst2 = vec2(1.0, 2.0);

#define FOO 12.3

varying vec2 vTexCoord;
varying float vAlpha;

void main() {
   vec4 tex0 = texture2D(kTexture0, vTexCoord);
   vec4 tex1 = texture2D(kTexture1, vTexCoord);
   gl_FragColor = pow(vec4(1.0), vec4(kGamma));
}
)";

    base::OverwriteTextFile("test_shader.glsl", original);

    gfx::MaterialClass klass;
    klass.SetShaderFile("test_shader.glsl");
    klass.SetType(gfx::MaterialClass::Type::Texture);
    klass.SetBaseColor(gfx::Color::White);
    klass.SetColorMapColor(gfx::Color::Blue, gfx::MaterialClass::ColorIndex::BottomLeft);
    klass.SetColorMapColor(gfx::Color::Green,gfx::MaterialClass::ColorIndex::TopLeft);
    klass.SetColorMapColor(gfx::Color::Red, gfx::MaterialClass::ColorIndex::BottomRight);
    klass.SetColorMapColor(gfx::Color::White, gfx::MaterialClass::ColorIndex::TopRight);
    klass.SetGamma(0.8f);
    klass.SetTextureVelocityX(4.0f);
    klass.SetTextureVelocityY(5.0f);
    klass.SetTextureVelocityZ(-1.0f);
    klass.SetTextureScaleX(2.0);
    klass.SetTextureScaleY(3.0);

    TestDevice device;
    {
        klass.SetStatic(true);
        klass.GetShader(device);
        const auto& shader = device.GetShader(0);
        const auto& source = shader.GetSource();
        TEST_REQUIRE(source == expected);
    }

    {
        klass.SetStatic(false);
        klass.GetShader(device);
        const auto& shader = device.GetShader(1);
        const auto& source = shader.GetSource();
        TEST_REQUIRE(source == original);
    }
}

int test_main(int argc, char* argv[])
{
    unit_test_material_uniforms();
    unit_test_material_textures();
    unit_test_material_uniform_folding();
    return 0;
}