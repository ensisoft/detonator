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

#include "base/test_minimal.h"
#include "graphics/color4f.h"
#include "graphics/device.h"
#include "graphics/program.h"
#include "graphics/texture.h"
#include "graphics/shader.h"
#include "graphics/geometry.h"

// We need this to create the rendering context.
#include "wdk/opengl/context.h"
#include "wdk/opengl/config.h"
#include "wdk/opengl/surface.h"

// setup context for headless rendering.
class TestContext : public gfx::Device::Context
{
public:
    TestContext(unsigned w, unsigned h)
    {
        wdk::Config::Attributes attrs;
        attrs.red_size  = 8;
        attrs.green_size = 8;
        attrs.blue_size = 8;
        attrs.alpha_size = 8;
        attrs.stencil_size = 8;
        attrs.surfaces.pbuffer = true;
        attrs.double_buffer = false;
        attrs.sampling = wdk::Config::Multisampling::MSAA4;
        attrs.srgb_buffer = true;
        mConfig   = std::make_unique<wdk::Config>(attrs);
        mContext  = std::make_unique<wdk::Context>(*mConfig, 2, 0,  false, //debug
            wdk::Context::Type::OpenGL_ES);
        mSurface  = std::make_unique<wdk::Surface>(*mConfig, w, h);
        mContext->MakeCurrent(mSurface.get());
    }
   ~TestContext()
    {
        mContext->MakeCurrent(nullptr);
        mSurface->Dispose();
        mSurface.reset();
        mConfig.reset();
    }
    virtual void Display() override
    {
        mContext->SwapBuffers();
    }
    virtual void* Resolve(const char* name) override
    {
        return mContext->Resolve(name);
    }
    virtual void MakeCurrent() override
    {
        mContext->MakeCurrent(mSurface.get());
    }

private:
    std::unique_ptr<wdk::Context> mContext;
    std::unique_ptr<wdk::Surface> mSurface;
    std::unique_ptr<wdk::Config>  mConfig;
};



void unit_test_device()
{
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
        std::make_shared<TestContext>(10, 10));

    // test clear color.
    const gfx::Color colors[] = {
        gfx::Color::Red,
        gfx::Color::White
    };
    for (auto c : colors)
    {
        dev->BeginFrame();
        dev->ClearColor(c);
        dev->EndFrame();

        // this has alpha in it.
        auto bmp = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(bmp.Compare(c));
    }

    // resources
    {
        TEST_REQUIRE(dev->FindShader("foo") == nullptr);
        TEST_REQUIRE(dev->FindProgram("foo") == nullptr);
        TEST_REQUIRE(dev->FindGeometry("foo") == nullptr);
        TEST_REQUIRE(dev->FindTexture("foo") == nullptr);

        TEST_REQUIRE(dev->MakeShader("foo") != nullptr);
        TEST_REQUIRE(dev->MakeProgram("foo") != nullptr);
        TEST_REQUIRE(dev->MakeGeometry("foo") != nullptr);
        TEST_REQUIRE(dev->MakeTexture("foo") != nullptr);

        // identity
        TEST_REQUIRE(dev->FindShader("foo") == dev->FindShader("foo"));
        TEST_REQUIRE(dev->FindProgram("foo") == dev->FindProgram("foo"));
        TEST_REQUIRE(dev->FindGeometry("foo") == dev->FindGeometry("foo"));
        TEST_REQUIRE(dev->FindTexture("foo") == dev->FindTexture("foo"));

        dev->DeleteShaders();
        dev->DeletePrograms();
        dev->DeleteGeometries();
        dev->DeleteTextures();
        TEST_REQUIRE(dev->FindShader("foo") == nullptr);
        TEST_REQUIRE(dev->FindProgram("foo") == nullptr);
        TEST_REQUIRE(dev->FindGeometry("foo") == nullptr);
        TEST_REQUIRE(dev->FindTexture("foo") == nullptr);
    }
}

void unit_test_shader()
{
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
        std::make_shared<TestContext>(10, 10));

    // junk
    {
        auto* shader = dev->MakeShader("foo");
        TEST_REQUIRE(shader->IsValid() == false);
        TEST_REQUIRE(shader->CompileSource("bla alsgas") == false);
    }

    // fragment shader
    {
        auto* shader = dev->MakeShader("foo");

        // missing frag gl_FragColor
        const std::string& wrong =
R"(#version 100
precision mediump float;
void main()
{
})";
        TEST_REQUIRE(shader->CompileSource(wrong) == false);

        const std::string& valid =
R"(#version 100
precision mediump float;
void main() {
  gl_FragColor = vec4(1.0);
})";
        TEST_REQUIRE(shader->CompileSource(valid) == true);
    }

    // vertex shader
    {
        auto* shader = dev->MakeShader("foo");

        // missing gl_Position
        const std::string& wrong =
R"(#version 100
attribute vec position;
void main() {}
)";
        TEST_REQUIRE(shader->CompileSource(wrong) == false);

        const std::string& valid =
R"(#version 100
void main() {
    gl_Position = vec4(1.0);
    }
)";
        TEST_REQUIRE(shader->CompileSource(valid) == true);
    }
}

void unit_test_texture()
{
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
        std::make_shared<TestContext>(10, 10));

    auto* texture = dev->MakeTexture("foo");
    TEST_REQUIRE(texture->GetWidth() == 0);
    TEST_REQUIRE(texture->GetHeight() == 0);
    TEST_REQUIRE(texture->GetMinFilter() == gfx::Texture::MinFilter::Default);
    TEST_REQUIRE(texture->GetMagFilter() == gfx::Texture::MagFilter::Default);
    TEST_REQUIRE(texture->GetWrapX() == gfx::Texture::Wrapping::Repeat);
    TEST_REQUIRE(texture->GetWrapY() == gfx::Texture::Wrapping::Repeat);
    // format is unspecified.

    const gfx::RGB pixels[2*3] = {
        gfx::Color::White, gfx::Color::White,
        gfx::Color::Red, gfx::Color::Red,
        gfx::Color::Blue, gfx::Color::Blue
    };
    texture->Upload(pixels, 2, 3, gfx::Texture::Format::RGB);
    texture->SetFilter(gfx::Texture::MinFilter::Linear);
    texture->SetFilter(gfx::Texture::MagFilter::Nearest);
    texture->SetWrapX(gfx::Texture::Wrapping::Clamp);
    texture->SetWrapY(gfx::Texture::Wrapping::Clamp);

    TEST_REQUIRE(texture->GetWidth() == 2);
    TEST_REQUIRE(texture->GetHeight() == 3);
    TEST_REQUIRE(texture->GetFormat() == gfx::Texture::Format::RGB);
    TEST_REQUIRE(texture->GetMinFilter() == gfx::Texture::MinFilter::Linear);
    TEST_REQUIRE(texture->GetMagFilter() == gfx::Texture::MagFilter::Nearest);
    TEST_REQUIRE(texture->GetWrapX() == gfx::Texture::Wrapping::Clamp);
    TEST_REQUIRE(texture->GetWrapY() == gfx::Texture::Wrapping::Clamp);

}

void unit_test_program()
{
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
        std::make_shared<TestContext>(10, 10));

    auto* prog = dev->MakeProgram("foo");

    TEST_REQUIRE(prog->IsValid() == false);

    // missing fragment shader
    {
        auto* vert = dev->MakeShader("vert");
        const std::string& valid =
R"(#version 100
void main() {
    gl_Position = vec4(1.0);
    }
)";
        TEST_REQUIRE(vert->CompileSource(valid) == true);
        std::vector<const gfx::Shader*> shaders;
        shaders.push_back(vert);
        TEST_REQUIRE(prog->Build(shaders) == false);
    }

    // missing vertex shader
    {
        auto* frag = dev->MakeShader("frag");
        const std::string& valid =
R"(#version 100
precision mediump float;
void main() {
  gl_FragColor = vec4(1.0);
})";
        TEST_REQUIRE(frag->CompileSource(valid) == true);
        std::vector<const gfx::Shader*> shaders;
        shaders.push_back(frag);
        TEST_REQUIRE(prog->Build(shaders) == false);
    }

    // complete program with vertex and fragment shaders
    {
        auto* vert = dev->MakeShader("vert");
        const std::string& validvs =
R"(#version 100
void main() {
    gl_Position = vec4(1.0);
    }
)";
        TEST_REQUIRE(vert->CompileSource(validvs) == true);

        auto* frag = dev->MakeShader("frag");
        const std::string& validfs =
R"(#version 100
precision mediump float;
void main() {
  gl_FragColor = vec4(1.0);
})";
        TEST_REQUIRE(frag->CompileSource(validfs) == true);

        std::vector<const gfx::Shader*> shaders;
        shaders.push_back(vert);
        shaders.push_back(frag);
        TEST_REQUIRE(prog->Build(shaders) == true);
    }
}

void unit_test_render_color_only()
{
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
        std::make_shared<TestContext>(10, 10));
    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);

    auto* geom = dev->MakeGeometry("geom");
    const gfx::Vertex verts[] = {
        { {-1,  1}, {0, 1} },
        { {-1, -1}, {0, 0} },
        { { 1, -1}, {1, 0} },

        { {-1,  1}, {0, 1} },
        { { 1, -1}, {1, 0} },
        { { 1,  1}, {1, 1} }
    };
    geom->SetVertexBuffer(verts, 6);
    geom->AddDrawCmd(gfx::Geometry::DrawType::Triangles);

    const std::string& fssrc =
R"(#version 100
precision mediump float;
void main() {
  gl_FragColor = vec4(1.0);
})";

    const std::string& vssrc =
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";
    auto* vs = dev->MakeShader("vert");
    auto* fs = dev->MakeShader("frag");
    TEST_REQUIRE(vs->CompileSource(vssrc));
    TEST_REQUIRE(fs->CompileSource(fssrc));
    std::vector<const gfx::Shader*> shaders;
    shaders.push_back(vs);
    shaders.push_back(fs);

    auto* prog = dev->MakeProgram("prog");
    TEST_REQUIRE(prog->Build(shaders));

    gfx::Device::State state;
    state.blending = gfx::Device::State::BlendOp::None;
    state.bWriteColor = true;
    state.viewport = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    dev->Draw(*prog, *geom, state);
    dev->EndFrame();

    // this has alpha in it.
    const auto& bmp = dev->ReadColorBuffer(10, 10);
    TEST_REQUIRE(bmp.Compare(gfx::Color::White));
}

void unit_test_render_with_single_texture()
{
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
        std::make_shared<TestContext>(4, 4));

    gfx::Bitmap<gfx::RGBA> data(4, 4);
    data.SetPixel(0, 0, gfx::Color::Red);
    data.SetPixel(1, 0, gfx::Color::Red);
    data.SetPixel(0, 1, gfx::Color::Red);
    data.SetPixel(1, 1, gfx::Color::Red);
    data.SetPixel(2, 0, gfx::Color::Blue);
    data.SetPixel(3, 0, gfx::Color::Blue);
    data.SetPixel(2, 1, gfx::Color::Blue);
    data.SetPixel(3, 1, gfx::Color::Blue);
    data.SetPixel(2, 2, gfx::Color::Green);
    data.SetPixel(3, 2, gfx::Color::Green);
    data.SetPixel(2, 3, gfx::Color::Green);
    data.SetPixel(3, 3, gfx::Color::Green);
    data.SetPixel(0, 2, gfx::Color::Yellow);
    data.SetPixel(1, 2, gfx::Color::Yellow);
    data.SetPixel(0, 3, gfx::Color::Yellow);
    data.SetPixel(1, 3, gfx::Color::Yellow);

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::White);

    auto* geom = dev->MakeGeometry("geom");
    const gfx::Vertex verts[] = {
        { {-1,  1}, {0, 0} },
        { {-1, -1}, {0, 1} },
        { { 1, -1}, {1, 1} },

        { {-1,  1}, {0, 0} },
        { { 1, -1}, {1, 1} },
        { { 1,  1}, {1, 0} }
    };
    geom->SetVertexBuffer(verts, 6);
    geom->AddDrawCmd(gfx::Geometry::DrawType::Triangles);

    const std::string& fssrc =
R"(#version 100
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D kTexture;
void main() {
  gl_FragColor = texture2D(kTexture, vTexCoord.xy);
})";

    const std::string& vssrc =
R"(#version 100
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
  vTexCoord = aTexCoord;
})";
    auto* vs = dev->MakeShader("vert");
    auto* fs = dev->MakeShader("frag");
    TEST_REQUIRE(vs->CompileSource(vssrc));
    TEST_REQUIRE(fs->CompileSource(fssrc));
    std::vector<const gfx::Shader*> shaders;
    shaders.push_back(vs);
    shaders.push_back(fs);

    auto* prog = dev->MakeProgram("prog");
    TEST_REQUIRE(prog->Build(shaders));

    auto* texture = dev->MakeTexture("tex");
    texture->Upload(data.GetDataPtr(), 4, 4, gfx::Texture::Format::RGBA);

    prog->SetTexture("kTexture", 0, *texture);

    gfx::Device::State state;
    state.blending = gfx::Device::State::BlendOp::None;
    state.bWriteColor = true;
    state.viewport = gfx::IRect(0, 0, 4, 4);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    dev->Draw(*prog, *geom, state);
    dev->EndFrame();

    const auto& bmp = dev->ReadColorBuffer(4, 4);
    WritePNG(bmp, "foo.png");
    TEST_REQUIRE(gfx::Compare(bmp, data));
}

void unit_test_render_with_multiple_textures()
{
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
        std::make_shared<TestContext>(4, 4));

    // setup 4 textures and the output from fragment shader
    // is then the sum of all of these, i.e. white.
    gfx::Bitmap<gfx::RGBA> r(1, 1);
    gfx::Bitmap<gfx::RGBA> g(1, 1);
    gfx::Bitmap<gfx::RGBA> b(1, 1);
    gfx::Bitmap<gfx::RGBA> a(1, 1);
    r.SetPixel(0, 0, gfx::Color::Red);
    g.SetPixel(0, 0, gfx::Color::Green);
    b.SetPixel(0, 0, gfx::Color::Blue);
    a.SetPixel(0, 0, gfx::RGBA(0, 0, 0, 0xff));

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::White);

    auto* geom = dev->MakeGeometry("geom");
    const gfx::Vertex verts[] = {
        { {-1,  1}, {0, 0} },
        { {-1, -1}, {0, 1} },
        { { 1, -1}, {1, 1} },

        { {-1,  1}, {0, 0} },
        { { 1, -1}, {1, 1} },
        { { 1,  1}, {1, 0} }
    };
    geom->SetVertexBuffer(verts, 6);
    geom->AddDrawCmd(gfx::Geometry::DrawType::Triangles);

    const std::string& fssrc =
R"(#version 100
precision mediump float;
uniform sampler2D kTexture0;
uniform sampler2D kTexture1;
uniform sampler2D kTexture2;
uniform sampler2D kTexture3;
void main() {
    gl_FragColor =
        texture2D(kTexture0, vec2(0.0)) +
        texture2D(kTexture1, vec2(0.0)) +
        texture2D(kTexture2, vec2(0.0)) +
        texture2D(kTexture3, vec2(0.0));
})";

    const std::string& vssrc =
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";
    auto* vs = dev->MakeShader("vert");
    auto* fs = dev->MakeShader("frag");
    TEST_REQUIRE(vs->CompileSource(vssrc));
    TEST_REQUIRE(fs->CompileSource(fssrc));
    std::vector<const gfx::Shader*> shaders;
    shaders.push_back(vs);
    shaders.push_back(fs);

    auto* prog = dev->MakeProgram("prog");
    TEST_REQUIRE(prog->Build(shaders));

    auto* tex0 = dev->MakeTexture("tex0");
    auto* tex1 = dev->MakeTexture("tex1");
    auto* tex2 = dev->MakeTexture("tex2");
    auto* tex3 = dev->MakeTexture("tex3");
    tex0->Upload(r.GetDataPtr(), 1, 1, gfx::Texture::Format::RGBA);
    tex1->Upload(g.GetDataPtr(), 1, 1, gfx::Texture::Format::RGBA);
    tex2->Upload(b.GetDataPtr(), 1, 1, gfx::Texture::Format::RGBA);
    tex3->Upload(a.GetDataPtr(), 1, 1, gfx::Texture::Format::RGBA);

    prog->SetTexture("kTexture0", 0, *tex0);
    prog->SetTexture("kTexture1", 1, *tex1);
    prog->SetTexture("kTexture2", 2, *tex2);
    prog->SetTexture("kTexture3", 3, *tex3);

    gfx::Device::State state;
    state.blending = gfx::Device::State::BlendOp::None;
    state.bWriteColor = true;
    state.viewport = gfx::IRect(0, 0, 4, 4);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    dev->Draw(*prog, *geom, state);
    dev->EndFrame();

    const auto& bmp = dev->ReadColorBuffer(4, 4);
    WritePNG(bmp, "foo.png");
    TEST_REQUIRE(bmp.Compare(gfx::Color::White));
}

void unit_test_render_set_float_uniforms()
{
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
                                   std::make_shared<TestContext>(10, 10));


    auto* geom = dev->MakeGeometry("geom");
    const gfx::Vertex verts[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },

            { {-1,  1}, {0, 1} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
    };
    geom->SetVertexBuffer(verts, 6);
    geom->AddDrawCmd(gfx::Geometry::DrawType::Triangles);

    const std::string& fssrc =
            R"(#version 100
precision mediump float;
uniform float kFloat;
uniform vec2  kVec2;
uniform vec3  kVec3;
uniform vec4  kVec4;
void main() {
  float value = kFloat +
    (kVec2.x + kVec2.y) +
    (kVec3.x + kVec3.y + kVec3.z) +
    (kVec4.x + kVec4.y + kVec4.z + kVec4.w);
  gl_FragColor = vec4(value, value, value, value);
})";

    const std::string& vssrc =
            R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";
    auto* vs = dev->MakeShader("vert");
    auto* fs = dev->MakeShader("frag");
    TEST_REQUIRE(vs->CompileSource(vssrc));
    TEST_REQUIRE(fs->CompileSource(fssrc));
    std::vector<const gfx::Shader*> shaders;
    shaders.push_back(vs);
    shaders.push_back(fs);

    auto* prog = dev->MakeProgram("prog");
    TEST_REQUIRE(prog->Build(shaders));

    gfx::Device::State state;
    state.blending = gfx::Device::State::BlendOp::None;
    state.bWriteColor = true;
    state.viewport = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);
    prog->SetUniform("kFloat", 0.2f); // 0.2f
    prog->SetUniform("kVec2", 0.1f, 0.1f); // 0.2f total
    prog->SetUniform("kVec3", 0.05f, 0.05f, 0.1f); // 0.2f total
    prog->SetUniform("kVec4", 0.1f, 0.1f, 0.1f, 0.1f); // 0.4 total
    dev->Draw(*prog, *geom, state);
    dev->EndFrame();
    {
        const auto &bmp = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(bmp.Compare(gfx::Color::White));
    }

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Black);
    prog->SetUniform("kFloat", 1.0f);
    prog->SetUniform("kVec2", 0.0f, 0.0f);
    prog->SetUniform("kVec3", 0.0f, 0.0f, 0.0f);
    prog->SetUniform("kVec4", 0.0f, 0.0f, 0.0f, 0.0f);
    dev->Draw(*prog, *geom, state);
    dev->EndFrame();
    {
        const auto &bmp = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(bmp.Compare(gfx::Color::White));
    }

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Black);
    prog->SetUniform("kFloat", 0.0f);
    prog->SetUniform("kVec2", 0.5f, 0.5f);
    prog->SetUniform("kVec3", 0.0f, 0.0f, 0.0f);
    prog->SetUniform("kVec4", 0.0f, 0.0f, 0.0f, 0.0f);
    dev->Draw(*prog, *geom, state);
    dev->EndFrame();
    {
        const auto &bmp = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(bmp.Compare(gfx::Color::White));
    }

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Black);
    prog->SetUniform("kFloat", 0.0f);
    prog->SetUniform("kVec2", 0.0f, 0.0f);
    prog->SetUniform("kVec3", 0.5f, 0.3f, 0.2f);
    prog->SetUniform("kVec4", 0.0f, 0.0f, 0.0f, 0.0f);
    dev->Draw(*prog, *geom, state);
    dev->EndFrame();
    {
        const auto &bmp = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(bmp.Compare(gfx::Color::White));
    }

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Black);
    prog->SetUniform("kFloat", 0.0f);
    prog->SetUniform("kVec2", 0.0f, 0.0f);
    prog->SetUniform("kVec3", 0.0f, 0.0f, 0.0f);
    prog->SetUniform("kVec4", 0.25f, 0.25f, 0.25f, 0.25f);
    dev->Draw(*prog, *geom, state);
    dev->EndFrame();
    {
        const auto &bmp = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(bmp.Compare(gfx::Color::White));
    }
}

void unit_test_render_set_int_uniforms()
{
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
                                   std::make_shared<TestContext>(10, 10));

    auto *geom = dev->MakeGeometry("geom");
    const gfx::Vertex verts[] = {
            {{-1, 1},  {0, 1}},
            {{-1, -1}, {0, 0}},
            {{1,  -1}, {1, 0}},

            {{-1, 1},  {0, 1}},
            {{1,  -1}, {1, 0}},
            {{1,  1},  {1, 1}}
    };
    geom->SetVertexBuffer(verts, 6);
    geom->AddDrawCmd(gfx::Geometry::DrawType::Triangles);

    const std::string& fssrc =
            R"(#version 100
precision mediump float;
uniform int kValue;
uniform ivec2 kVec2;
void main() {
  gl_FragColor = vec4(0.0);
  int sum = kValue + kVec2.x + kVec2.y;
  if (sum == 1)
    gl_FragColor = vec4(1.0);
})";

    const std::string& vssrc =
            R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";
    auto *vs = dev->MakeShader("vert");
    auto *fs = dev->MakeShader("frag");
    TEST_REQUIRE(vs->CompileSource(vssrc));
    TEST_REQUIRE(fs->CompileSource(fssrc));
    std::vector<const gfx::Shader *> shaders;
    shaders.push_back(vs);
    shaders.push_back(fs);
    auto *prog = dev->MakeProgram("prog");
    TEST_REQUIRE(prog->Build(shaders));

    gfx::Device::State state;
    state.blending = gfx::Device::State::BlendOp::None;
    state.bWriteColor = true;
    state.viewport = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);
    prog->SetUniform("kValue", 1);
    prog->SetUniform("kVec2", 0, 0);
    dev->Draw(*prog, *geom, state);
    dev->EndFrame();
    {
        const auto &bmp = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(bmp.Compare(gfx::Color::White));
    }

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);
    prog->SetUniform("kValue", 0);
    prog->SetUniform("kVec2", 1, 0);
    dev->Draw(*prog, *geom, state);
    dev->EndFrame();
    {
        const auto &bmp = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(bmp.Compare(gfx::Color::White));
    }

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);
    prog->SetUniform("kValue", 0);
    prog->SetUniform("kVec2", 0, 1);
    dev->Draw(*prog, *geom, state);
    dev->EndFrame();
    {
        const auto &bmp = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(bmp.Compare(gfx::Color::White));
    }

}
void unit_test_render_set_matrix2x2_uniform()
{
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
                                   std::make_shared<TestContext>(10, 10));
    auto* geom = dev->MakeGeometry("geom");
    const gfx::Vertex verts[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },

            { {-1,  1}, {0, 1} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
    };
    geom->SetVertexBuffer(verts, 6);
    geom->AddDrawCmd(gfx::Geometry::DrawType::Triangles);

    const std::string& fssrc =
            R"(#version 100
precision mediump float;
uniform mat2 kMatrix;
void main() {
  gl_FragColor = vec4(
    kMatrix[0][0] +
    kMatrix[1][0] +
    kMatrix[0][1] +
    kMatrix[1][1]);
})";

    const std::string& vssrc =
            R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";
    auto* vs = dev->MakeShader("vert");
    auto* fs = dev->MakeShader("frag");
    TEST_REQUIRE(vs->CompileSource(vssrc));
    TEST_REQUIRE(fs->CompileSource(fssrc));
    std::vector<const gfx::Shader*> shaders;
    shaders.push_back(vs);
    shaders.push_back(fs);
    auto* prog = dev->MakeProgram("prog");
    TEST_REQUIRE(prog->Build(shaders));

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);
    gfx::Device::State state;
    state.blending = gfx::Device::State::BlendOp::None;
    state.bWriteColor = true;
    state.viewport = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    const gfx::Program::Matrix2x2 matrix = {
        {0.25f, 0.25f},
        {0.25f, 0.25f}
    };
    prog->SetUniform("kMatrix", matrix);
    dev->Draw(*prog, *geom, state);
    dev->EndFrame();

    // this has alpha in it.
    const auto& bmp = dev->ReadColorBuffer(10, 10);
    TEST_REQUIRE(bmp.Compare(gfx::Color::White));
}

void unit_test_render_set_matrix3x3_uniform()
{
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
                                   std::make_shared<TestContext>(10, 10));
    auto* geom = dev->MakeGeometry("geom");
    const gfx::Vertex verts[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },

            { {-1,  1}, {0, 1} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
    };
    geom->SetVertexBuffer(verts, 6);
    geom->AddDrawCmd(gfx::Geometry::DrawType::Triangles);

    const std::string& fssrc =
            R"(#version 100
precision mediump float;
uniform mat3 kMatrix;
void main() {
  float r = kMatrix[0][0] + kMatrix[0][1] + kMatrix[0][2];
  float g = kMatrix[1][0] + kMatrix[1][1] + kMatrix[1][2];
  float b = kMatrix[2][0] + kMatrix[2][1] + kMatrix[2][2];
  gl_FragColor = vec4(r, g, b, 1.0);
})";

    const std::string& vssrc =
            R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";
    auto* vs = dev->MakeShader("vert");
    auto* fs = dev->MakeShader("frag");
    TEST_REQUIRE(vs->CompileSource(vssrc));
    TEST_REQUIRE(fs->CompileSource(fssrc));
    std::vector<const gfx::Shader*> shaders;
    shaders.push_back(vs);
    shaders.push_back(fs);
    auto* prog = dev->MakeProgram("prog");
    TEST_REQUIRE(prog->Build(shaders));

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);
    gfx::Device::State state;
    state.blending = gfx::Device::State::BlendOp::None;
    state.bWriteColor = true;
    state.viewport = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    const gfx::Program::Matrix3x3 matrix = {
            {0.25f, 0.25f, 0.50f},
            {0.25f, 0.50f, 0.25f},
            {0.50f, 0.25f, 0.25f}
    };
    prog->SetUniform("kMatrix", matrix);
    dev->Draw(*prog, *geom, state);
    dev->EndFrame();

    // this has alpha in it.
    const auto& bmp = dev->ReadColorBuffer(10, 10);
    TEST_REQUIRE(bmp.Compare(gfx::Color::White));
}

void unit_test_render_set_matrix4x4_uniform()
{
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
                                   std::make_shared<TestContext>(10, 10));
    auto* geom = dev->MakeGeometry("geom");
    const gfx::Vertex verts[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },

            { {-1,  1}, {0, 1} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
    };
    geom->SetVertexBuffer(verts, 6);
    geom->AddDrawCmd(gfx::Geometry::DrawType::Triangles);

    const std::string& fssrc =
            R"(#version 100
precision mediump float;
uniform mat4 kMatrix;
void main() {
  float r = kMatrix[0][0] + kMatrix[0][1] + kMatrix[0][2] + kMatrix[0][3];
  float g = kMatrix[1][0] + kMatrix[1][1] + kMatrix[1][2] + kMatrix[1][3];
  float b = kMatrix[2][0] + kMatrix[2][1] + kMatrix[2][2] + kMatrix[2][3];
  float a = kMatrix[3][0] + kMatrix[3][1] + kMatrix[3][2] + kMatrix[3][3];
  gl_FragColor = vec4(r, g, b, a);
})";

    const std::string& vssrc =
            R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";
    auto* vs = dev->MakeShader("vert");
    auto* fs = dev->MakeShader("frag");
    TEST_REQUIRE(vs->CompileSource(vssrc));
    TEST_REQUIRE(fs->CompileSource(fssrc));
    std::vector<const gfx::Shader*> shaders;
    shaders.push_back(vs);
    shaders.push_back(fs);
    auto* prog = dev->MakeProgram("prog");
    TEST_REQUIRE(prog->Build(shaders));

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);
    gfx::Device::State state;
    state.blending = gfx::Device::State::BlendOp::None;
    state.bWriteColor = true;
    state.viewport = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    const gfx::Program::Matrix4x4 matrix = {
            {0.25f, 0.25, 0.25f, 0.25f},
            {0.25f, 0.25, 0.25f, 0.25f},
            {0.25f, 0.25, 0.25f, 0.25f},
            {0.25f, 0.25, 0.25f, 0.25f}
    };
    prog->SetUniform("kMatrix", matrix);
    dev->Draw(*prog, *geom, state);
    dev->EndFrame();

    // this has alpha in it.
    const auto& bmp = dev->ReadColorBuffer(10, 10);
    TEST_REQUIRE(bmp.Compare(gfx::Color::White));
}

void unit_test_uniform_sampler_optimize_bug()
{
    // shader code doesn't actually use the material, the sampler/uniform location
    // is thus -1 and no texture will be set.
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2, std::make_shared<TestContext>(10, 10));
    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);

    auto* texture = dev->MakeTexture("foo");
    const gfx::RGB pixels[2*3] = {
            gfx::Color::White, gfx::Color::White,
            gfx::Color::Red, gfx::Color::Red,
            gfx::Color::Blue, gfx::Color::Blue
    };
    texture->Upload(pixels, 2, 3, gfx::Texture::Format::RGB);

    auto* geom = dev->MakeGeometry("geom");
    const gfx::Vertex verts[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },

            { {-1,  1}, {0, 1} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
    };
    geom->SetVertexBuffer(verts, 6);
    geom->AddDrawCmd(gfx::Geometry::DrawType::Triangles);

    // no mention of the texture sampler in the fragment shader!
    const std::string& fssrc =
            R"(#version 100
precision mediump float;
void main() {
  gl_FragColor = vec4(1.0);
})";

    const std::string& vssrc =
            R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";
    auto* vs = dev->MakeShader("vert");
    auto* fs = dev->MakeShader("frag");
    TEST_REQUIRE(vs->CompileSource(vssrc));
    TEST_REQUIRE(fs->CompileSource(fssrc));
    std::vector<const gfx::Shader*> shaders;
    shaders.push_back(vs);
    shaders.push_back(fs);
    auto* prog = dev->MakeProgram("prog");
    TEST_REQUIRE(prog->Build(shaders));

    gfx::Device::State state;
    state.blending = gfx::Device::State::BlendOp::None;
    state.bWriteColor = true;
    state.viewport = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    // set the texture that isn't actually set  since the shader
    // doesnt' use it.
    prog->SetTexture("kTexture", 0, *texture);
    prog->SetTextureCount(1);

    dev->Draw(*prog, *geom, state);
    dev->EndFrame();
    dev->CleanGarbage(120);
}

void unit_test_clean_garbage()
{
    auto dev = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
                                   std::make_shared<TestContext>(10, 10));

    {
        const gfx::RGB pixels[2 * 3] = {
                gfx::Color::White, gfx::Color::White,
                gfx::Color::Red, gfx::Color::Red,
                gfx::Color::Blue, gfx::Color::Blue
        };
        auto* texture = dev->MakeTexture("foo");
        texture->Upload(pixels, 2, 3, gfx::Texture::Format::RGB);
        texture->EnableGarbageCollection(true);
        TEST_REQUIRE(dev->FindTexture("foo"));
    }

    dev->BeginFrame();
    dev->EndFrame();
    dev->CleanGarbage(2);

    TEST_REQUIRE(dev->FindTexture("foo"));

    dev->BeginFrame();
    dev->EndFrame();
    dev->CleanGarbage(2);
    TEST_REQUIRE(dev->FindTexture("foo") == nullptr);

}

int test_main(int argc, char* argv[])
{
    unit_test_device();
    unit_test_shader();
    unit_test_texture();
    unit_test_program();

    unit_test_render_color_only();
    unit_test_render_with_single_texture();
    unit_test_render_with_multiple_textures();
    unit_test_render_set_float_uniforms();
    unit_test_render_set_int_uniforms();
    unit_test_render_set_matrix2x2_uniform();
    unit_test_render_set_matrix3x3_uniform();
    unit_test_render_set_matrix4x4_uniform();
    unit_test_uniform_sampler_optimize_bug();

    unit_test_clean_garbage();
    return 0;
}
