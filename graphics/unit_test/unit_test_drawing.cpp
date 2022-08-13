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
#include "base/test_help.h"
#include "graphics/types.h"
#include "graphics/material.h"
#include "graphics/device.h"
#include "graphics/drawable.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/texture.h"
#include "graphics/geometry.h"

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
    virtual void Upload(const void* bytes, unsigned xres, unsigned yres, Format format, bool mips) override
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
    virtual void SetContentHash(size_t hash) override
    { mHash = hash; }
    virtual size_t GetContentHash() const override
    { return mHash; }
    virtual void SetTransient(bool on_off) override
    { mTransient = on_off; }
    virtual void SetName(const std::string&) override
    {}
    virtual void SetGroup(const std::string&) override
    {}
private:
    unsigned mWidth  = 0;
    unsigned mHeight = 0;
    Format mFormat  = Format::Grayscale;
    Wrapping mWrapX = Wrapping::Repeat;
    Wrapping mWrapY = Wrapping::Repeat;
    MinFilter mMinFilter = MinFilter::Default;
    MagFilter mMagFilter = MagFilter::Default;
    std::size_t mHash = 0;
    bool mTransient = false;
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
    virtual void SetTextureCount(unsigned count) override
    {
        mTextures.resize(count);
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
    size_t GetNumTextures() const
    { return mTextures.size(); }
    TextureBinding* FindTextureBinding(const std::string& sampler)
    {
        for (auto& binding : mTextures)
        {
            if (binding.sampler == sampler)
                return &binding;
        }
        return nullptr;
    }

private:
    std::unordered_map<std::string, std::any> mUniforms;
    std::vector<TextureBinding> mTextures;
};


class TestGeometry : public gfx::Geometry
{
public:
    struct Draw {
        gfx::Geometry::DrawType type;
        std::size_t offset = 0;
        std::size_t count  = 0;
    };
    std::vector<Draw> mDrawCmds;

    gfx::VertexLayout mLayout;

    virtual void ClearDraws() override
    {
        mDrawCmds.clear();
    }
    virtual void AddDrawCmd(DrawType type) override
    {
        Draw d;
        d.type = type;
        mDrawCmds.push_back(d);
    }
    virtual void AddDrawCmd(DrawType type, size_t offset, size_t count) override
    {
        Draw d;
        d.type = type;
        d.count = count;
        d.offset = offset;
        mDrawCmds.push_back(d);
    }
    virtual void SetVertexLayout(const gfx::VertexLayout& layout) override
    {
        mLayout = layout;
    }
    virtual void Upload(const void* data, size_t bytes, Usage usage) override
    {
        mUsage = usage;
        mData.resize(bytes);
        mBytes =bytes;
        mUploaded = true;
        std::memcpy(&mData[0], data, bytes);
    }
    // Set the hash value that identifies the data.
    virtual void SetDataHash(size_t hash) override
    { mHash = hash; }
    // Get the hash value that was used in the latest data upload.
    virtual size_t GetDataHash() const  override
    { return mHash; }

    bool mUploaded = false;
    gfx::Geometry::Usage mUsage;
    std::vector<char> mData;
    std::size_t mBytes = 0;
    std::size_t mHash = 0;
private:
};

class TestDevice : public gfx::Device
{
public:
    virtual void ClearColor(const gfx::Color4f& color) override
    {}
    virtual void ClearStencil(int value) override
    {}
    virtual void ClearDepth(float value) override
    {}
    virtual void ClearColorDepth(const gfx::Color4f& color, float depth) override
    {}
    virtual void ClearColorDepthStencil(const gfx::Color4f& color, float depth, int stencil) override
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
        auto it = mGeomIndexMap.find(name);
        if (it == mGeomIndexMap.end())
            return nullptr;
        return mGeoms[it->second].get();
    }
    virtual gfx::Geometry* MakeGeometry(const std::string& name) override
    {
        const size_t index = mGeoms.size();
        mGeoms.emplace_back(new TestGeometry);
        mGeomIndexMap[name] = index;
        return mGeoms.back().get();
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
    virtual gfx::Framebuffer* FindFramebuffer(const std::string& name) override
    {
        return nullptr;
    }
    virtual gfx::Framebuffer* MakeFramebuffer(const std::string& name) override
    {
        return nullptr;
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
    virtual void DeleteFramebuffers() override
    {}
    virtual void SetFramebuffer(const gfx::Framebuffer* fbo) override
    {

    }
    virtual void Draw(const gfx::Program& program, const gfx::Geometry& geometry, const State& state) override
    {}

    virtual void CleanGarbage(size_t, unsigned) override
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
    virtual void GetResourceStats(ResourceStats* stats) const override
    {

    }
    virtual void GetDeviceCaps(DeviceCaps* caps) const override
    {

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
    size_t GetNumTextures() const
    { return mTextures.size(); }

private:
    std::unordered_map<std::string, std::size_t> mTextureIndexMap;
    std::vector<std::unique_ptr<TestTexture>> mTextures;

    std::unordered_map<std::string, std::size_t> mGeomIndexMap;
    std::vector<std::unique_ptr<TestGeometry>> mGeoms;

    std::unordered_map<std::string, std::size_t> mShaderIndexMap;
    std::vector<std::unique_ptr<TestShader>> mShaders;
};


void unit_test_material_uniforms()
{
    // test dynamic program uniforms.
    {
        TestDevice device;
        TestProgram program;
        gfx::ColorClass test;

        test.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        test.SetBaseColor(gfx::Color::Green);
        test.SetStatic(false);
        test.SetGamma(2.0f);

        // check that the dynamic state is set as expected.
        // this should mean that both static uniforms  and dynamic
        // uniforms are set.
        gfx::MaterialClass::State env;
        env.render_points = false;
        env.material_time = 0.0f;
        test.ApplyDynamicState(env, device, program);

        gfx::Color4f base_color;
        TEST_REQUIRE(program.GetUniform("kBaseColor", &base_color));
        TEST_REQUIRE(base_color == gfx::Color::Green);
    }

    {
        TestDevice device;
        TestProgram program;
        gfx::GradientClass test;

        test.SetColor(gfx::Color::DarkBlue,    gfx::GradientClass::ColorIndex::BottomLeft);
        test.SetColor(gfx::Color::DarkGreen,   gfx::GradientClass::ColorIndex::TopLeft);
        test.SetColor(gfx::Color::DarkMagenta, gfx::GradientClass::ColorIndex::BottomRight);
        test.SetColor(gfx::Color::DarkGray,    gfx::GradientClass::ColorIndex::TopRight);
        test.SetStatic(false);
        test.SetGamma(2.0f);

        gfx::MaterialClass::State env;
        env.render_points = false;
        env.material_time = 0.0f;
        test.ApplyDynamicState(env, device, program);

        glm::vec1 gamma;
        gfx::Color4f gradients[4];
        TEST_REQUIRE(program.GetUniform("kGamma", &gamma));
        TEST_REQUIRE(program.GetUniform("kColor0", &gradients[0]));
        TEST_REQUIRE(program.GetUniform("kColor1", &gradients[1]));
        TEST_REQUIRE(program.GetUniform("kColor2", &gradients[2]));
        TEST_REQUIRE(program.GetUniform("kColor3", &gradients[3]));
        TEST_REQUIRE(gamma == glm::vec1(2.0f));
        TEST_REQUIRE(gradients[(int)gfx::GradientClass::ColorIndex::BottomLeft] == gfx::Color::DarkBlue);
        TEST_REQUIRE(gradients[(int)gfx::GradientClass::ColorIndex::TopLeft] == gfx::Color::DarkGreen);
        TEST_REQUIRE(gradients[(int)gfx::GradientClass::ColorIndex::BottomRight] == gfx::Color::DarkMagenta);
        TEST_REQUIRE(gradients[(int)gfx::GradientClass::ColorIndex::TopRight] == gfx::Color::DarkGray);

    }

    {
        gfx::RgbBitmap bitmap;
        bitmap.Resize(2, 2);

        TestDevice device;
        TestProgram program;
        gfx::TextureMap2DClass test;
        test.SetGamma(2.0f);
        test.SetTextureScaleX(2.0f);
        test.SetTextureScaleY(3.0f);
        test.SetTextureVelocityX(4.0f);
        test.SetTextureVelocityY(5.0f);
        test.SetTextureVelocityZ(-1.0f);
        test.SetStatic(false);
        test.SetTexture(gfx::CreateTextureFromBitmap(bitmap));

        gfx::MaterialClass::State env;
        env.render_points = false;
        env.material_time = 2.0f;
        test.ApplyDynamicState(env, device, program);

        glm::vec2 texture_scale;
        glm::vec2 texture_velocity_xy;
        glm::vec1 texture_velocity_z;
        glm::vec1 gamma;
        glm::vec1 particle_rotation_flag;
        glm::vec1 render_points_flag;
        glm::vec1 runtime;
        TEST_REQUIRE(program.GetUniform("kGamma", &gamma));
        TEST_REQUIRE(program.GetUniform("kTextureScale", &texture_scale));
        TEST_REQUIRE(program.GetUniform("kTextureVelocityXY", &texture_velocity_xy));
        TEST_REQUIRE(program.GetUniform("kTextureVelocityZ", &texture_velocity_z));
        TEST_REQUIRE(program.GetUniform("kApplyRandomParticleRotation", &particle_rotation_flag));
        TEST_REQUIRE(program.GetUniform("kRenderPoints", &render_points_flag));
        TEST_REQUIRE(program.GetUniform("kRuntime", &runtime));
        TEST_REQUIRE(texture_scale == glm::vec2(2.0f, 3.0f));
        TEST_REQUIRE(texture_velocity_xy == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(texture_velocity_z == glm::vec1(-1.0f));
        TEST_REQUIRE(gamma == glm::vec1(2.0f));
        TEST_REQUIRE(particle_rotation_flag == glm::vec1(0.0f));
        TEST_REQUIRE(render_points_flag == glm::vec1(0.0f));
        TEST_REQUIRE(runtime == glm::vec1(2.0f));

    }

    {
        gfx::RgbBitmap bitmap;
        bitmap.Resize(2, 2);

        TestDevice device;
        TestProgram program;
        gfx::SpriteClass test;
        test.SetGamma(2.0f);
        test.SetTextureScaleX(2.0f);
        test.SetTextureScaleY(3.0f);
        test.SetTextureVelocityX(4.0f);
        test.SetTextureVelocityY(5.0f);
        test.SetTextureVelocityZ(-1.0f);
        test.SetBaseColor(gfx::Color::Green);
        test.SetStatic(false);
        test.AddTexture(gfx::CreateTextureFromBitmap(bitmap));
        test.AddTexture(gfx::CreateTextureFromBitmap(bitmap));

        gfx::MaterialClass::State env;
        env.render_points = false;
        env.material_time = 2.0f;
        test.ApplyDynamicState(env, device, program);

        glm::vec2 texture_scale;
        glm::vec2 texture_velocity_xy;
        glm::vec1 texture_velocity_z;
        glm::vec1 gamma;
        glm::vec1 particle_rotation_flag;
        glm::vec1 render_points_flag;
        glm::vec1 runtime;
        gfx::Color4f base_color;
        TEST_REQUIRE(program.GetUniform("kGamma", &gamma));
        TEST_REQUIRE(program.GetUniform("kTextureScale", &texture_scale));
        TEST_REQUIRE(program.GetUniform("kTextureVelocityXY", &texture_velocity_xy));
        TEST_REQUIRE(program.GetUniform("kTextureVelocityZ", &texture_velocity_z));
        TEST_REQUIRE(program.GetUniform("kApplyRandomParticleRotation", &particle_rotation_flag));
        TEST_REQUIRE(program.GetUniform("kRenderPoints", &render_points_flag));
        TEST_REQUIRE(program.GetUniform("kRuntime", &runtime));
        TEST_REQUIRE(program.GetUniform("kBaseColor", &base_color));
        TEST_REQUIRE(texture_scale == glm::vec2(2.0f, 3.0f));
        TEST_REQUIRE(texture_velocity_xy == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(texture_velocity_z == glm::vec1(-1.0f));
        TEST_REQUIRE(gamma == glm::vec1(2.0f));
        TEST_REQUIRE(particle_rotation_flag == glm::vec1(0.0f));
        TEST_REQUIRE(render_points_flag == glm::vec1(0.0f));
        TEST_REQUIRE(runtime == glm::vec1(2.0f));
        TEST_REQUIRE(base_color == gfx::Color::Green);

    }

    // test static program uniforms.
    {
        TestDevice device;
        TestProgram program;
        gfx::ColorClass test;

        test.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        test.SetBaseColor(gfx::Color::Green);
        test.SetStatic(true);
        test.SetGamma(2.0f);

        test.ApplyStaticState(device, program);
        gfx::Color4f base_color;
        TEST_REQUIRE(program.GetUniform("kBaseColor", &base_color));
        TEST_REQUIRE(base_color == gfx::Color::Green);

        program.Clear();

        gfx::MaterialClass::State env;
        env.render_points = false;
        env.material_time = 0.0f;
        test.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(!program.HasUniform("kBaseColor"));
    }

    {
        TestDevice device;
        TestProgram program;
        gfx::GradientClass test;

        test.SetColor(gfx::Color::DarkBlue,    gfx::GradientClass::ColorIndex::BottomLeft);
        test.SetColor(gfx::Color::DarkGreen,   gfx::GradientClass::ColorIndex::TopLeft);
        test.SetColor(gfx::Color::DarkMagenta, gfx::GradientClass::ColorIndex::BottomRight);
        test.SetColor(gfx::Color::DarkGray,    gfx::GradientClass::ColorIndex::TopRight);
        test.SetStatic(true);
        test.SetGamma(2.0f);

        test.ApplyStaticState(device, program);
        glm::vec1 gamma;
        gfx::Color4f gradients[4];
        TEST_REQUIRE(program.GetUniform("kGamma", &gamma));
        TEST_REQUIRE(program.GetUniform("kColor0", &gradients[0]));
        TEST_REQUIRE(program.GetUniform("kColor1", &gradients[1]));
        TEST_REQUIRE(program.GetUniform("kColor2", &gradients[2]));
        TEST_REQUIRE(program.GetUniform("kColor3", &gradients[3]));
        TEST_REQUIRE(gamma == glm::vec1(2.0f));
        TEST_REQUIRE(gradients[(int)gfx::GradientClass::ColorIndex::BottomLeft] == gfx::Color::DarkBlue);
        TEST_REQUIRE(gradients[(int)gfx::GradientClass::ColorIndex::TopLeft] == gfx::Color::DarkGreen);
        TEST_REQUIRE(gradients[(int)gfx::GradientClass::ColorIndex::BottomRight] == gfx::Color::DarkMagenta);
        TEST_REQUIRE(gradients[(int)gfx::GradientClass::ColorIndex::TopRight] == gfx::Color::DarkGray);

        program.Clear();

        gfx::MaterialClass::State env;
        env.render_points = false;
        env.material_time = 0.0f;
        test.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(!program.HasUniform("kGamma"));
        TEST_REQUIRE(!program.HasUniform("kColor0"));
        TEST_REQUIRE(!program.HasUniform("kColor1"));
        TEST_REQUIRE(!program.HasUniform("kColor2"));
        TEST_REQUIRE(!program.HasUniform("kColor3"));
    }

    {
        gfx::RgbBitmap bitmap;
        bitmap.Resize(2, 2);

        TestDevice device;
        TestProgram program;
        gfx::TextureMap2DClass test;
        test.SetGamma(2.0f);
        test.SetTextureScaleX(2.0f);
        test.SetTextureScaleY(3.0f);
        test.SetTextureVelocityX(4.0f);
        test.SetTextureVelocityY(5.0f);
        test.SetTextureVelocityZ(-1.0f);
        test.SetStatic(true);
        test.SetTexture(gfx::CreateTextureFromBitmap(bitmap));

        test.ApplyStaticState(device, program);
        glm::vec2 texture_scale;
        glm::vec2 texture_velocity_xy;
        glm::vec1 texture_velocity_z;
        glm::vec1 gamma;
        glm::vec1 particle_rotation_flag;
        glm::vec1 render_points_flag;
        glm::vec1 runtime;
        TEST_REQUIRE(program.GetUniform("kGamma", &gamma));
        TEST_REQUIRE(program.GetUniform("kTextureScale", &texture_scale));
        TEST_REQUIRE(program.GetUniform("kTextureVelocityXY", &texture_velocity_xy));
        TEST_REQUIRE(program.GetUniform("kTextureVelocityZ", &texture_velocity_z));
        TEST_REQUIRE(texture_scale == glm::vec2(2.0f, 3.0f));
        TEST_REQUIRE(texture_velocity_xy == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(texture_velocity_z == glm::vec1(-1.0f));
        TEST_REQUIRE(gamma == glm::vec1(2.0f));

        program.Clear();

        gfx::MaterialClass::State env;
        env.render_points = false;
        env.material_time = 2.0f;
        test.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(!program.HasUniform("kGamma"));
        TEST_REQUIRE(!program.HasUniform("kTextureScale"));
        TEST_REQUIRE(!program.HasUniform("kTextureVelocityXY"));
        TEST_REQUIRE(!program.HasUniform("kTextureVelocityZ"));
    }

    {
        gfx::RgbBitmap bitmap;
        bitmap.Resize(2, 2);

        TestDevice device;
        TestProgram program;
        gfx::SpriteClass test;
        test.SetGamma(2.0f);
        test.SetTextureScaleX(2.0f);
        test.SetTextureScaleY(3.0f);
        test.SetTextureVelocityX(4.0f);
        test.SetTextureVelocityY(5.0f);
        test.SetTextureVelocityZ(-1.0f);
        test.SetStatic(true);
        test.SetBaseColor(gfx::Color::Red);
        test.AddTexture(gfx::CreateTextureFromBitmap(bitmap));
        test.AddTexture(gfx::CreateTextureFromBitmap(bitmap));

        test.ApplyStaticState(device, program);
        glm::vec2 texture_scale;
        glm::vec2 texture_velocity_xy;
        glm::vec1 texture_velocity_z;
        glm::vec1 gamma;
        gfx::Color4f base_color;
        TEST_REQUIRE(program.GetUniform("kGamma", &gamma));
        TEST_REQUIRE(program.GetUniform("kTextureScale", &texture_scale));
        TEST_REQUIRE(program.GetUniform("kTextureVelocityXY", &texture_velocity_xy));
        TEST_REQUIRE(program.GetUniform("kTextureVelocityZ", &texture_velocity_z));
        TEST_REQUIRE(program.GetUniform("kBaseColor", &base_color));
        TEST_REQUIRE(texture_scale == glm::vec2(2.0f, 3.0f));
        TEST_REQUIRE(texture_velocity_xy == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(texture_velocity_z == glm::vec1(-1.0f));
        TEST_REQUIRE(gamma == glm::vec1(2.0f));
        TEST_REQUIRE(base_color == gfx::Color::Red);

        program.Clear();

        gfx::MaterialClass::State env;
        env.render_points = false;
        env.material_time = 2.0f;
        test.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(!program.HasUniform("kGamma"));
        TEST_REQUIRE(!program.HasUniform("kTextureScale"));
        TEST_REQUIRE(!program.HasUniform("kTextureVelocityXY"));
        TEST_REQUIRE(!program.HasUniform("kTextureVelocityZ"));
        TEST_REQUIRE(!program.HasUniform("kBaseColor"));
    }

    // test that static programs generate different program ID
    // based on their static state even if the underlying shader
    // program has the same type.
    {
        gfx::ColorClass foo;
        foo.SetStatic(true);
        foo.SetBaseColor(gfx::Color::Red);

        auto bar = foo;
        TEST_REQUIRE(foo.GetProgramId() == bar.GetProgramId());

        bar.SetBaseColor(gfx::Color::Green);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
    }


    {
        gfx::GradientClass foo;
        foo.SetStatic(true);
        foo.SetColor(gfx::Color::DarkBlue,    gfx::GradientClass::ColorIndex::BottomLeft);
        foo.SetColor(gfx::Color::DarkGreen,   gfx::GradientClass::ColorIndex::TopLeft);
        foo.SetColor(gfx::Color::DarkMagenta, gfx::GradientClass::ColorIndex::BottomRight);
        foo.SetColor(gfx::Color::DarkGray,    gfx::GradientClass::ColorIndex::TopRight);

        auto bar = foo;
        TEST_REQUIRE(foo.GetProgramId() == bar.GetProgramId());

        foo.SetColor(gfx::Color::White, gfx::GradientClass::ColorIndex::BottomLeft);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
        bar = foo;
        foo.SetColor(gfx::Color::White,gfx::GradientClass::ColorIndex::TopLeft);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
        bar = foo;
        foo.SetColor(gfx::Color::White, gfx::GradientClass::ColorIndex::BottomRight);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
        bar = foo;
        foo.SetColor(gfx::Color::White, gfx::GradientClass::ColorIndex::TopRight);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
    }

    {
        gfx::TextureMap2DClass foo;
        foo.SetGamma(2.0f);
        foo.SetStatic(true);
        foo.SetTextureScaleX(2.0f);
        foo.SetTextureScaleY(3.0f);
        foo.SetTextureVelocityX(4.0f);
        foo.SetTextureVelocityY(5.0f);
        foo.SetTextureVelocityZ(-1.0f);

        auto bar = foo;
        TEST_REQUIRE(bar.GetProgramId() == foo.GetProgramId());
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

    {
        gfx::SpriteClass foo;
        foo.SetGamma(2.0f);
        foo.SetStatic(true);
        foo.SetTextureScaleX(2.0f);
        foo.SetTextureScaleY(3.0f);
        foo.SetTextureVelocityX(4.0f);
        foo.SetTextureVelocityY(5.0f);
        foo.SetTextureVelocityZ(-1.0f);
        foo.SetBaseColor(gfx::Color::Red);

        auto bar = foo;
        TEST_REQUIRE(bar.GetProgramId() == foo.GetProgramId());
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
        bar = foo;
        foo.SetBaseColor(gfx::Color::Blue);
        TEST_REQUIRE(foo.GetProgramId() != bar.GetProgramId());
    }
}

void unit_test_material_textures()
{
    // test setting basic texture properties.
    {
        TestDevice device;
        TestProgram program;

        gfx::TextureMap2DClass test;
        test.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Nearest);
        test.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Trilinear);
        test.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
        test.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);

        gfx::RgbBitmap bitmap;
        bitmap.Resize(100, 80);
        test.SetTexture(gfx::CreateTextureFromBitmap(bitmap));

        gfx::MaterialClass::State env;
        env.material_time = 1.0;
        test.ApplyDynamicState(env, device, program);

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

    // one texture, both texture bindings are using the same texture.
    {
        gfx::SpriteClass test;
        test.SetFps(1.0f); // 1 frame per second.

        gfx::RgbBitmap bitmap;
        bitmap.Resize(10, 10);
        test.AddTexture(gfx::CreateTextureFromBitmap(bitmap));

        TestDevice device;
        TestProgram program;
        {
            gfx::MaterialClass::State env;
            env.material_time = 0.0f;
            test.ApplyDynamicState(env, device, program);
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
            gfx::MaterialClass::State env;
            env.material_time = 1.5f;
            test.ApplyDynamicState(env, device, program);
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
        gfx::SpriteClass test;
        test.SetFps(1.0f); // 1 frame per second.

        gfx::RgbBitmap one;
        one.Resize(10, 10);
        one.Fill(gfx::Color::Red);
        gfx::RgbBitmap two;
        two.Resize(10, 10);
        two.Fill(gfx::Color::Green);

        test.AddTexture(gfx::CreateTextureFromBitmap(one));
        test.AddTexture(gfx::CreateTextureFromBitmap(two));

        TestDevice device;
        TestProgram program;
        {
            gfx::MaterialClass::State env;
            env.material_time = 0.0f;
            test.ApplyDynamicState(env, device, program);
            const auto& tex0 = device.GetTexture(0);
            const auto& tex1 = device.GetTexture(1);
            TEST_REQUIRE(program.GetTextureBinding(0).unit == 0);
            TEST_REQUIRE(program.GetTextureBinding(0).texture == &tex0);
            TEST_REQUIRE(program.GetTextureBinding(1).unit == 1);
            TEST_REQUIRE(program.GetTextureBinding(1).texture == &tex1);
        }
        program.Clear();
        {
            gfx::MaterialClass::State env;
            env.material_time = 1.0f;
            test.ApplyDynamicState(env, device, program);
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
        gfx::SpriteClass test;
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

        test.AddTexture(gfx::CreateTextureFromBitmap(one));
        test.AddTexture(gfx::CreateTextureFromBitmap(two));
        test.AddTexture(gfx::CreateTextureFromBitmap(three));

        TestDevice device;
        TestProgram program;
        {
            gfx::MaterialClass::State env;
            env.material_time = 0.0f;
            test.ApplyDynamicState(env, device, program);
            const auto& tex0 = device.GetTexture(0);
            const auto& tex1 = device.GetTexture(1);
            TEST_REQUIRE(program.GetTextureBinding(0).unit == 0);
            TEST_REQUIRE(program.GetTextureBinding(0).texture == &tex0);
            TEST_REQUIRE(program.GetTextureBinding(1).unit == 1);
            TEST_REQUIRE(program.GetTextureBinding(1).texture == &tex1);
        }
        program.Clear();
        {
            gfx::MaterialClass::State env;
            env.material_time = 1.0f;
            test.ApplyDynamicState(env, device, program);
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
            gfx::MaterialClass::State env;
            env.material_time = 2.5f;
            test.ApplyDynamicState(env, device, program);
            const auto& tex0 = device.GetTexture(0);
            const auto& tex1 = device.GetTexture(1);
            const auto& tex2 = device.GetTexture(2);
            TEST_REQUIRE(program.GetTextureBinding(0).unit == 0);
            TEST_REQUIRE(program.GetTextureBinding(0).texture == &tex2);
            TEST_REQUIRE(program.GetTextureBinding(1).unit == 1);
            TEST_REQUIRE(program.GetTextureBinding(1).texture == &tex0);
        }
    }

    // turn off looping.
    {
        gfx::SpriteClass test;
        test.SetFps(1.0f); // 1 frame per second.
        test.SetLooping(false);

        gfx::RgbBitmap one;
        one.Resize(10, 10);
        one.Fill(gfx::Color::Red);
        gfx::RgbBitmap two;
        two.Resize(10, 10);
        two.Fill(gfx::Color::Green);
        gfx::RgbBitmap three;
        two.Resize(10, 10);
        two.Fill(gfx::Color::Yellow);

        test.AddTexture(gfx::CreateTextureFromBitmap(one));
        test.AddTexture(gfx::CreateTextureFromBitmap(two));
        test.AddTexture(gfx::CreateTextureFromBitmap(three));

        TestDevice device;
        TestProgram program;
        {
            gfx::MaterialClass::State env;
            env.material_time = 0.0f;
            test.ApplyDynamicState(env, device, program);
            const auto& tex0 = device.GetTexture(0);
            const auto& tex1 = device.GetTexture(1);
            TEST_REQUIRE(program.GetTextureBinding(0).unit == 0);
            TEST_REQUIRE(program.GetTextureBinding(0).texture == &tex0);
            TEST_REQUIRE(program.GetTextureBinding(1).unit == 1);
            TEST_REQUIRE(program.GetTextureBinding(1).texture == &tex1);
        }

        program.Clear();
        {
            gfx::MaterialClass::State env;
            env.material_time = 1.0f;
            test.ApplyDynamicState(env, device, program);
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
            gfx::MaterialClass::State env;
            env.material_time = 2.5f;
            test.ApplyDynamicState(env, device, program);
            const auto& tex0 = device.GetTexture(0);
            const auto& tex1 = device.GetTexture(1);
            const auto& tex2 = device.GetTexture(2);
            TEST_REQUIRE(program.GetTextureBinding(0).unit == 0);
            TEST_REQUIRE(program.GetTextureBinding(0).texture == &tex2);
            TEST_REQUIRE(program.GetTextureBinding(1).unit == 1);
            TEST_REQUIRE(program.GetTextureBinding(1).texture == &tex2);
        }
    }
}

void unit_test_material_textures_bind_fail()
{
    TestDevice device;
    TestProgram program;

    // test setting basic texture properties.
    {
        gfx::TextureMap2DClass test;
        test.SetTexture(gfx::LoadTextureFromFile("no-such-file.png"));

        gfx::MaterialClass::State env;
        env.material_time = 1.0;
        // no crash
        test.ApplyDynamicState(env, device, program);
    }

    {
        gfx::SpriteClass test;
        test.AddTexture( gfx::LoadTextureFromFile("no-such-file.png"));

        gfx::MaterialClass::State env;
        env.material_time = 1.0;
        // no crash
        test.ApplyDynamicState(env, device, program);
    }

    {
        gfx::CustomMaterialClass test;
        gfx::SpriteMap sprite;
        sprite.AddTexture(gfx::LoadTextureFromFile("no-such-file.png"));

        test.SetTextureMap("huhu", sprite);

        gfx::MaterialClass::State env;
        env.material_time = 1.0;
        // no crash
        test.ApplyDynamicState(env, device, program);
    }
}

void unit_test_material_uniform_folding()
{

    // fold uniforms into consts in the GLSL when the material is
    // marked static.
    {
        TestDevice device;

        gfx::ColorClass klass;
        klass.SetGamma(0.8f);
        klass.SetBaseColor(gfx::Color::White);
        klass.SetStatic(true);
        klass.GetShader(device);

        const auto& shader = device.GetShader(0);
        const auto& source = shader.GetSource();
        TEST_REQUIRE(base::Contains(source, "uniform float kGamma;") == false);
        TEST_REQUIRE(base::Contains(source, "uniform vec4 kBaseColor;") == false);
        TEST_REQUIRE(base::Contains(source, "const float kGamma = 0.80;"));
        TEST_REQUIRE(base::Contains(source, "const vec4 kBaseColor = vec4(1.00,1.00,1.00,1.00);"));
    }

    {
        TestDevice device;

        gfx::GradientClass klass;
        klass.SetGamma(0.8);
        klass.SetColor(gfx::Color::Blue,  gfx::GradientClass::ColorIndex::BottomLeft);
        klass.SetColor(gfx::Color::Green, gfx::GradientClass::ColorIndex::TopLeft);
        klass.SetColor(gfx::Color::Red,   gfx::GradientClass::ColorIndex::BottomRight);
        klass.SetColor(gfx::Color::White, gfx::GradientClass::ColorIndex::TopRight);
        klass.SetStatic(true);
        klass.GetShader(device);

        const auto& shader = device.GetShader(0);
        const auto& source = shader.GetSource();
        TEST_REQUIRE(base::Contains(source, "uniform float kGamma;") == false);
        TEST_REQUIRE(base::Contains(source, "uniform vec4 kColor0;") == false);
        TEST_REQUIRE(base::Contains(source, "uniform vec4 kColor1;") == false);
        TEST_REQUIRE(base::Contains(source, "uniform vec4 kColor2;") == false);
        TEST_REQUIRE(base::Contains(source, "uniform vec4 kColor3;") == false);
        TEST_REQUIRE(base::Contains(source, "const float kGamma = 0.80;"));
        TEST_REQUIRE(base::Contains(source, "const vec4 kColor0 = vec4(0.00,1.00,0.00,1.00);"));
        TEST_REQUIRE(base::Contains(source, "const vec4 kColor1 = vec4(1.00,1.00,1.00,1.00);"));
        TEST_REQUIRE(base::Contains(source, "const vec4 kColor2 = vec4(0.00,0.00,1.00,1.00);"));
        TEST_REQUIRE(base::Contains(source, "const vec4 kColor3 = vec4(1.00,0.00,0.00,1.00);"));
    }

    {
        TestDevice device;

        gfx::TextureMap2DClass klass;
        klass.SetStatic(true);
        klass.SetGamma(0.8);
        klass.SetBaseColor(gfx::Color::White);
        klass.SetTextureVelocityX(4.0f);
        klass.SetTextureVelocityY(5.0f);
        klass.SetTextureVelocityZ(-1.0f);
        klass.SetTextureScaleX(2.0);
        klass.SetTextureScaleY(3.0);
        klass.GetShader(device);

        const auto& shader = device.GetShader(0);
        const auto& source = shader.GetSource();
        TEST_REQUIRE(base::Contains(source, "uniform float kGamma;") == false);
        TEST_REQUIRE(base::Contains(source, "uniform vec4 kBaseColor;") == false);
        TEST_REQUIRE(base::Contains(source, "uniform vec2 kTextureScale") == false);
        TEST_REQUIRE(base::Contains(source, "uniform vec2 kTextureVelocityXY") == false);
        TEST_REQUIRE(base::Contains(source, "uniform float kTextureVelocityZ") == false);
        TEST_REQUIRE(base::Contains(source, "const float kGamma = 0.80;") == true);
        TEST_REQUIRE(base::Contains(source, "const vec4 kBaseColor = vec4(1.00,1.00,1.00,1.00);"));
        TEST_REQUIRE(base::Contains(source, "const vec2 kTextureScale = vec2(2.00,3.00);"));
        TEST_REQUIRE(base::Contains(source, "const vec2 kTextureVelocityXY = vec2(4.00,5.00);"));
        TEST_REQUIRE(base::Contains(source, "const float kTextureVelocityZ = -1.00;"));
    }

    {
        TestDevice device;

        gfx::SpriteClass klass;
        klass.SetStatic(true);
        klass.SetGamma(0.8);
        klass.SetTextureVelocityX(4.0f);
        klass.SetTextureVelocityY(5.0f);
        klass.SetTextureVelocityZ(-1.0f);
        klass.SetTextureScaleX(2.0);
        klass.SetTextureScaleY(3.0);
        klass.GetShader(device);

        const auto& shader = device.GetShader(0);
        const auto& source = shader.GetSource();
        TEST_REQUIRE(base::Contains(source, "uniform float kGamma;") == false);
        TEST_REQUIRE(base::Contains(source, "uniform vec2 kTextureScale") == false);
        TEST_REQUIRE(base::Contains(source, "uniform vec2 kTextureVelocityXY") == false);
        TEST_REQUIRE(base::Contains(source, "uniform float kTextureVelocityZ") == false);
        TEST_REQUIRE(base::Contains(source, "const float kGamma = 0.80;") == true);
        TEST_REQUIRE(base::Contains(source, "const vec2 kTextureScale = vec2(2.00,3.00);"));
        TEST_REQUIRE(base::Contains(source, "const vec2 kTextureVelocityXY = vec2(4.00,5.00);"));
        TEST_REQUIRE(base::Contains(source, "const float kTextureVelocityZ = -1.00;"));

    }
}

void unit_test_custom_uniforms()
{
    gfx::CustomMaterialClass klass;
    klass.SetUniform("float", 56.0f);
    klass.SetUniform("int", 123);
    klass.SetUniform("vec2", glm::vec2(1.0f, 2.0f));
    klass.SetUniform("vec3", glm::vec3(1.0f, 2.0f, 3.0f));
    klass.SetUniform("vec4", glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
    klass.SetUniform("color", gfx::Color::DarkCyan);

    TestDevice device;
    TestProgram program;
    gfx::MaterialClass::State env;
    env.material_time = 0.0f;
    klass.ApplyDynamicState(env, device, program);

    glm::ivec1 ivec1_;
    glm::vec1 vec1_;
    glm::vec2 vec2_;
    glm::vec3 vec3_;
    glm::vec4 vec4_;
    gfx::Color4f color_;
    TEST_REQUIRE(program.GetUniform("float", &vec1_));
    TEST_REQUIRE(program.GetUniform("int", &ivec1_));
    TEST_REQUIRE(program.GetUniform("vec2", &vec2_));
    TEST_REQUIRE(program.GetUniform("vec3", &vec3_));
    TEST_REQUIRE(program.GetUniform("vec4", &vec4_));
    TEST_REQUIRE(program.GetUniform("color", &color_));
    TEST_REQUIRE(ivec1_.x   == 123);
    TEST_REQUIRE(vec1_.x == real::float32(56.0f));
    TEST_REQUIRE(vec2_  == glm::vec2(1.0f, 2.0f));
    TEST_REQUIRE(vec3_  == glm::vec3(1.0f, 2.0f, 3.0f));
    TEST_REQUIRE(vec4_  == glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
    TEST_REQUIRE(color_  == gfx::Color::DarkCyan);

}

void unit_test_custom_textures()
{
    gfx::CustomMaterialClass klass;
    {
        gfx::RgbBitmap bitmap;
        bitmap.Resize(10, 10);

        gfx::TextureMap2D texture;
        texture.SetTexture(gfx::CreateTextureFromBitmap(bitmap));
        texture.SetSamplerName("kFoobar");
        texture.SetRectUniformName("kFoobarRect");
        texture.SetTextureRect(gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));
        klass.SetTextureMap("texture", texture);
    }

    {
        gfx::RgbBitmap frame0, frame1;
        frame0.Resize(20, 20);
        frame1.Resize(30, 30);

        gfx::SpriteMap sprite;
        sprite.SetFps(10.0f);
        sprite.AddTexture(gfx::CreateTextureFromBitmap(frame0));
        sprite.AddTexture(gfx::CreateTextureFromBitmap(frame1));
        sprite.SetSamplerName("kTexture0", 0);
        sprite.SetSamplerName("kTexture1", 1);
        sprite.SetRectUniformName("kTextureRect0", 0);
        sprite.SetRectUniformName("kTextureRect1", 1);
        sprite.SetTextureRect(size_t(0), gfx::FRect(1.0f, 2.0f, 3.0f, 4.0f));
        sprite.SetTextureRect(size_t(1), gfx::FRect(4.0f, 3.0f, 2.0f, 1.0f));
        klass.SetTextureMap("sprite", sprite);
    }

    TestDevice device;
    TestProgram program;

    gfx::MaterialClass::State env;
    klass.ApplyDynamicState(env, device, program);
    // these textures should be bound to these samplers. check the textures based on their sizes.
    TEST_REQUIRE(program.FindTextureBinding("kFoobar")->texture->GetWidth() == 10);
    TEST_REQUIRE(program.FindTextureBinding("kFoobar")->texture->GetHeight() == 10);
    TEST_REQUIRE(program.FindTextureBinding("kTexture0")->texture->GetWidth()  == 20);
    TEST_REQUIRE(program.FindTextureBinding("kTexture0")->texture->GetHeight() == 20);
    TEST_REQUIRE(program.FindTextureBinding("kTexture1")->texture->GetWidth()  == 30);
    TEST_REQUIRE(program.FindTextureBinding("kTexture1")->texture->GetHeight() == 30);

    // check the texture rects.
    glm::vec4 kFoobarRect;
    TEST_REQUIRE(program.GetUniform("kFoobarRect", &kFoobarRect));
    TEST_REQUIRE(kFoobarRect == glm::vec4(0.5f, 0.6f, 0.7f, 0.8f));

    glm::vec4 kTextureRect0;
    glm::vec4 kTextureRect1;
    TEST_REQUIRE(program.GetUniform("kTextureRect0", &kTextureRect0));
    TEST_REQUIRE(program.GetUniform("kTextureRect1", &kTextureRect1));
    TEST_REQUIRE(kTextureRect0 == glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
    TEST_REQUIRE(kTextureRect1 == glm::vec4(4.0f, 3.0f, 2.0f, 1.0f));
}

void unit_test_static_poly()
{
    gfx::PolygonClass poly;

    // polygon is marked static, but we're in edit mode so the
    // contents should get re-uploaded as needed.
    poly.SetStatic(true);

    const gfx::Vertex verts[3] = {
       {{10.0f, 10.0f}, {0.5f, 1.0f}},
       {{-10.0f, -10.0f}, {0.0f, 0.0f}},
       {{10.0f, 10.0f}, {1.0f, 0.0f}}
    };
    gfx::PolygonClass::DrawCommand cmd;
    cmd.offset = 0;
    cmd.count  = 3;
    cmd.type   = gfx::PolygonClass::DrawType::TriangleFan;
    poly.AddVertices(verts, 3);
    poly.AddDrawCommand(cmd);

    TestDevice device;
    auto* geom = (TestGeometry*)poly.Upload(true, device);

    TEST_REQUIRE(geom);
    TEST_REQUIRE(geom->mUploaded == true);
    TEST_REQUIRE(geom->mBytes == sizeof(verts));
    TEST_REQUIRE(geom->mDrawCmds.size() == 1);
    TEST_REQUIRE(geom->mDrawCmds[0].offset == 0);
    TEST_REQUIRE(geom->mDrawCmds[0].count == 3);
    TEST_REQUIRE(geom->mDrawCmds[0].type == gfx::Geometry::DrawType::TriangleFan);

    geom->mUploaded = false;

    {
        auto* g = (TestGeometry*)poly.Upload(true, device);
        TEST_REQUIRE(g == geom);
    }
    TEST_REQUIRE(geom->mUploaded == false);
    TEST_REQUIRE(geom->mBytes == sizeof(verts));

    // change the content (simulate editing)
    poly.AddVertices(verts, 3);
    poly.AddDrawCommand(cmd);

    {
        auto* g = (TestGeometry*)poly.Upload(true, device);
        TEST_REQUIRE(g == geom);
    }
    TEST_REQUIRE(geom->mUploaded == true);
    TEST_REQUIRE(geom->mBytes == sizeof(verts) + sizeof(verts));

    geom->mUploaded = false;


    // change the content (simulate editing)
    poly.AddVertices(verts, 3);
    poly.AddDrawCommand(cmd);

    // upload when edit mode is false should not re-upload anything.
    {
        auto g = (TestGeometry*)poly.Upload(false, device);
        TEST_REQUIRE(g == geom);
    }
    TEST_REQUIRE(geom->mUploaded == false);
    TEST_REQUIRE(geom->mBytes == sizeof(verts) + sizeof(verts));
}


// multiple materials with textures should only load the
// same texture object once onto the device.
void unit_test_packed_texture_bug()
{
    gfx::RgbaBitmap bmp;
    bmp.Resize(10, 10);
    bmp.Fill(gfx::Color::HotPink);
    gfx::WritePNG(bmp, "test-texture.png");

    // several materials
    {
        gfx::TextureMap2DClass material0;
        material0.SetTexture(gfx::LoadTextureFromFile("test-texture.png"));
        gfx::TextureMap2DClass material1;
        material1.SetTexture(gfx::LoadTextureFromFile("test-texture.png"));

        TestDevice device;
        TestProgram program;
        gfx::MaterialClass::State env;
        env.material_time = 0.0f;
        env.editing_mode  = false;
        material0.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(device.GetNumTextures() == 1);

        material1.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(device.GetNumTextures() == 1);
    }

    // single material but with multiple texture maps
    {

    }
}

int test_main(int argc, char* argv[])
{
    unit_test_material_uniforms();
    unit_test_material_textures();
    unit_test_material_textures_bind_fail();
    unit_test_material_uniform_folding();
    unit_test_custom_uniforms();
    unit_test_custom_textures();
    unit_test_static_poly();

    unit_test_packed_texture_bug();
    return 0;
}