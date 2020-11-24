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

#include <iostream>
#include <unordered_map>

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

class TestShader : public gfx::Shader
{
public:
    virtual bool CompileSource(const std::string& source) override
    {
        return true;
    }
    virtual bool CompileFile(const std::string& file) override
    {
        return true;
    }
    virtual bool IsValid() const override
    { return true; }
private:
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
    {}
    virtual void SetUniform(const char* name, int x, int y) override
    {}
    virtual void SetUniform(const char* name, float x) override
    {}
    virtual void SetUniform(const char* name, float x, float y) override
    {}
    virtual void SetUniform(const char* name, float x, float y, float z) override
    {}
    virtual void SetUniform(const char* name, float x, float y, float z, float w) override
    {}
    virtual void SetUniform(const char* name, const gfx::Color4f& color) override
    {}
    virtual void SetUniform(const char* name, const Matrix2x2& matrix) override
    {}
    virtual void SetUniform(const char* name, const Matrix3x3& matrix) override
    {}
    virtual void SetUniform(const char* name, const Matrix4x4& matrix) override
    {}
    virtual void SetTexture(const char* sampler, unsigned unit, const gfx::Texture& texture) override
    {}
private:
};

class TestTexture : public gfx::Texture
{
public:
    virtual void SetFilter(MinFilter filter) override
    {}
    virtual void SetFilter(MagFilter filter) override
    {}
    virtual MinFilter GetMinFilter() const override
    { return MinFilter::Default; }
    virtual MagFilter GetMagFilter() const override
    { return MagFilter::Default; }
    virtual void SetWrapX(Wrapping w) override
    {}
    virtual void SetWrapY(Wrapping w) override
    {}
    virtual Wrapping GetWrapX() const override
    { return Wrapping::Repeat; }
    virtual Wrapping GetWrapY() const override
    { return Wrapping::Repeat; }
    virtual void Upload(const void* bytes, unsigned xres, unsigned yres, Format format) override
    {}
    virtual unsigned GetWidth() const override
    { return 0; }
    virtual unsigned GetHeight() const override
    { return 0; }
    virtual Format GetFormat() const override
    { return Format::RGBA; }
    virtual void EnableGarbageCollection(bool gc) override
    {}
private:
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
    virtual void Update(const gfx::Vertex* vertices, std::size_t count) override
    {}
    virtual void Update(const std::vector<gfx::Vertex>& vertices) override
    {}
    virtual void Update(std::vector<gfx::Vertex>&& vertices) override
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
        return nullptr;
    }
    virtual gfx::Shader* MakeShader(const std::string& name) override
    {
        return nullptr;
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
        return nullptr;
    }
    virtual gfx::Texture* MakeTexture(const std::string& name) override
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
};

int test_main(int argc, char* argv[])
{
    TestDevice device;
    return 0;
}