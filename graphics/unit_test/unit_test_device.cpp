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
#include "graphics/framebuffer.h"

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
        attrs.srgb_buffer = true;
        constexpr auto debug_context = false;
        mConfig   = std::make_unique<wdk::Config>(attrs);
        mContext  = std::make_unique<wdk::Context>(*mConfig, 2, 0, debug_context, wdk::Context::Type::OpenGL_ES);
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
    virtual Version GetVersion() const override
    {
        return Version::OpenGL_ES2;
    }

private:
    std::unique_ptr<wdk::Context> mContext;
    std::unique_ptr<wdk::Surface> mSurface;
    std::unique_ptr<wdk::Config>  mConfig;
};

gfx::Program* MakeTestProgram(gfx::Device& dev, const char* vssrc, const char* fssrc, const std::string name = "prog")
{
    auto* vs = dev.MakeShader(name + "/vert");
    auto* fs = dev.MakeShader(name + "/frag");
    TEST_REQUIRE(vs->CompileSource(vssrc));
    TEST_REQUIRE(fs->CompileSource(fssrc));
    std::vector<const gfx::Shader*> shaders;
    shaders.push_back(vs);
    shaders.push_back(fs);

    auto* prog = dev.MakeProgram(name);
    TEST_REQUIRE(prog->Build(shaders));
    return prog;
}


void unit_test_device()
{
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

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
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

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
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

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
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

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

void unit_test_framebuffer()
{
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

    {
        auto* fbo = dev->FindFramebuffer("test");
        TEST_REQUIRE(fbo == nullptr);
        fbo = dev->MakeFramebuffer("test");
        TEST_REQUIRE(fbo);
        dev->DeleteFramebuffers();
        fbo = dev->FindFramebuffer("test");
        TEST_REQUIRE(fbo == nullptr);
    }

    {
        auto* fbo = dev->MakeFramebuffer("fbo");
        gfx::Framebuffer::Config conf;
        conf.format = gfx::Framebuffer::Format::ColorRGBA8;
        conf.width  = 512;
        conf.height = 512;

        TEST_REQUIRE(fbo->Create(conf));
        dev->DeleteFramebuffers();
    }

    {
        auto* fbo = dev->MakeFramebuffer("fbo");
        gfx::Framebuffer::Config conf;
        conf.format = gfx::Framebuffer::Format::ColorRGBA8_Depth16;
        conf.width  = 512;
        conf.height = 512;
        TEST_REQUIRE(fbo->Create(conf));
        dev->DeleteFramebuffers();
    }

    {
        auto* fbo = dev->MakeFramebuffer("fbo");
        gfx::Framebuffer::Config conf;
        conf.format  = gfx::Framebuffer::Format::ColorRGBA8_Depth24_Stencil8;
        conf.width   = 512;
        conf.height  = 512;
        TEST_REQUIRE(fbo->Create(conf));
        dev->DeleteFramebuffers();
    }
}

void unit_test_render_fbo()
{
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

    gfx::Framebuffer::Config conf;
    conf.format = gfx::Framebuffer::Format::ColorRGBA8;
    conf.width  = 10;
    conf.height = 10;
    auto* fbo = dev->MakeFramebuffer("test");
    TEST_REQUIRE(fbo->Create(conf));

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

    // rendered a colored quad into the fbo then use the fbo
    // color buffer texture to sample in another program.
    auto* p0 = MakeTestProgram(*dev,
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})",
R"(#version 100
precision mediump float;
void main() {
  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
})", "p0");

    auto* p1 = MakeTestProgram(*dev,
R"(#version 100
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
  vTexCoord = aTexCoord;
})",
R"(#version 100
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D kTexture;
void main() {
  gl_FragColor = texture2D(kTexture, vTexCoord.xy);
})", "p1");

    gfx::Device::State state;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.bWriteColor  = true;
    state.viewport     = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    // do a couple of loops so that the texture gets bound for sampling
    // and for rendering.
    for (int i=0; i<2; ++i)
    {
        dev->BeginFrame();
        // clear the FBO to render and then render the green quad into it.
        dev->SetFramebuffer(fbo);
        dev->ClearColor(gfx::Color::Red);
        dev->Draw(*p0, *geom, state);

        // render using the second program and sample from the FBO texture.
        gfx::Texture* color = nullptr;
        fbo->Resolve(&color);
        color->SetFilter(gfx::Texture::MinFilter::Linear);
        color->SetFilter(gfx::Texture::MagFilter::Linear);

        p1->SetTexture("kTexture", 0, *color);

        dev->SetFramebuffer(nullptr);
        dev->ClearColor(gfx::Color::Blue);
        dev->Draw(*p1, *geom, state);

        dev->EndFrame();

        const auto& bmp = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(bmp.Compare(gfx::Color::Green));
    }
}

void unit_test_render_color_only()
{
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));
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
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(4, 4));

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
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(4, 4));

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
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

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
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

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
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

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
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

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
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

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
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

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
    // doesn't use it.
    prog->SetTexture("kTexture", 0, *texture);
    prog->SetTextureCount(1);

    dev->Draw(*prog, *geom, state);
    dev->EndFrame();
}

void unit_test_clean_textures()
{
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

    gfx::Device::State state;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.bWriteColor  = true;
    state.viewport     = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    const gfx::RGB pixels[2 * 3] = {
        gfx::Color::White, gfx::Color::White,
        gfx::Color::Red, gfx::Color::Red,
        gfx::Color::Blue, gfx::Color::Blue
    };
    const gfx::Vertex verts[] = {
        { {-1,  1}, {0, 1} },
        { {-1, -1}, {0, 0} },
        { { 1, -1}, {1, 0} },

        { {-1,  1}, {0, 1} },
        { { 1, -1}, {1, 0} },
        { { 1,  1}, {1, 1} }
    };
    auto* geom = dev->MakeGeometry("geom");
    geom->SetVertexBuffer(verts, 6);
    geom->AddDrawCmd(gfx::Geometry::DrawType::Triangles);

    // texture that is used is not cleaned
    {
        auto* texture = dev->MakeTexture("foo");
        texture->Upload(pixels, 2, 3, gfx::Texture::Format::RGB);
        TEST_REQUIRE(dev->FindTexture("foo"));

        const char* vs = R"(
#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
}
)";

        const char* fs = R"(
#version 100
precision mediump float;
uniform sampler2D kTexture;
void main() {
  gl_FragColor = texture2D(kTexture, vec2(0.5));
}
)";
        auto* prog = MakeTestProgram(*dev, vs, fs);

        for (int i=0; i<3; ++i)
        {
            dev->BeginFrame();

            prog->SetTexture("kTexture", 0, *texture);
            prog->SetTextureCount(1);

            dev->Draw(*prog, *geom, state);
            dev->EndFrame();
            dev->CleanGarbage(2, gfx::Device::GCFlags::Textures);
        }
        TEST_REQUIRE(dev->FindTexture("foo"));
    }

    // texture that is not used because the driver decided to optimize the
    // uniform away is not cleaned away.
    // if we let the texture be cleaned away then the material system will
    // end up trying to reload the texture all the time since it doesn't exist.
    // this would need some kind of propagation of the fact that the texture
    // doesn't actually contribute to the shader so that the material system
    // can then skip it. however, this is all because of this silly internal
    // optimization that leaks this from the driver. the easier fix for now
    // is just to let the texture stay there even if it's not used.
    {
        auto* texture = dev->MakeTexture("foo");
        texture->Upload(pixels, 2, 3, gfx::Texture::Format::RGB);
        TEST_REQUIRE(dev->FindTexture("foo"));

        const char* vs = R"(
#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
}
)";

        const char* fs = R"(
#version 100
precision mediump float;
uniform sampler2D kTexture;
void main() {
  gl_FragColor = vec4(1.0);
}
)";
        auto* prog = MakeTestProgram(*dev, vs, fs);

        for (int i=0; i<3; ++i)
        {
            dev->BeginFrame();

            prog->SetTexture("kTexture", 0, *texture);
            prog->SetTextureCount(1);

            dev->Draw(*prog, *geom, state);
            dev->EndFrame();
            dev->CleanGarbage(2, gfx::Device::GCFlags::Textures);
        }
        TEST_REQUIRE(dev->FindTexture("foo"));
    }


    dev->DeleteTextures();

    // texture that is not used gets cleaned away
    {
        auto* texture = dev->MakeTexture("foo");
        texture->Upload(pixels, 2, 3, gfx::Texture::Format::RGB);
        TEST_REQUIRE(dev->FindTexture("foo"));

        dev->BeginFrame();
        dev->EndFrame();
        dev->CleanGarbage(2, gfx::Device::GCFlags::Textures);
        TEST_REQUIRE(dev->FindTexture("foo"));

        dev->BeginFrame();
        dev->EndFrame();
        dev->CleanGarbage(2, gfx::Device::GCFlags::Textures);
        TEST_REQUIRE(dev->FindTexture("foo") == nullptr);
    }

}

void unit_test_render_dynamic()
{
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);

    auto* geom = dev->MakeGeometry("geom");
    const gfx::Vertex verts1[] = {
        { {-1,  1}, {0, 1} },
        { {-1,  0}, {0, 0} },
        { { 0,  0}, {1, 0} },

        { {-1,  1}, {0, 1} },
        { { 0,  0}, {1, 0} },
        { { 0,  1}, {1, 1} }
    };
    const gfx::Vertex verts2[] = {
        { {0,  1}, {0, 1} },
        { {0,  0}, {0, 0} },
        { {1,  0}, {1, 0} },

        { {0,  1}, {0, 1} },
        { {1,  0}, {1, 0} },
        { {1,  1}, {1, 1} }
    };

    geom->SetVertexBuffer(verts1, 6, gfx::Geometry::Usage::Dynamic);
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

    {
        gfx::RgbaBitmap expected;
        expected.Resize(10, 10);
        expected.Fill(gfx::Color::Red);
        expected.Fill(gfx::URect(0, 0, 5, 5), gfx::Color::White);

        // this has alpha in it.
        const auto& bmp = dev->ReadColorBuffer(10, 10);
        //gfx::WritePNG(bmp, "render-test-dynamic.png");
        TEST_REQUIRE(bmp == expected);
    }

    // change the geometry buffer.
    geom->ClearDraws();
    geom->SetVertexBuffer(verts2, 6, gfx::Geometry::Usage::Dynamic);
    geom->AddDrawCmd(gfx::Geometry::DrawType::Triangles);

    // draw frame
    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);
    dev->Draw(*prog, *geom, state);
    dev->EndFrame();

    {
        gfx::RgbaBitmap expected;
        expected.Resize(10, 10);
        expected.Fill(gfx::Color::Red);
        expected.Fill(gfx::URect(5, 0, 5, 5), gfx::Color::White);

        // this has alpha in it.
        const auto& bmp = dev->ReadColorBuffer(10, 10);
        //gfx::WritePNG(bmp, "render-test-dynamic.png");
        TEST_REQUIRE(bmp == expected);
    }
}

void unit_test_buffer_allocation()
{
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

    char junk_data[512] = {0};

    // static
    {
        auto* foo = dev->MakeGeometry("foo");
        foo->Upload(junk_data, sizeof(junk_data), gfx::Geometry::Usage::Static);

        gfx::Device::ResourceStats stats;

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_use == sizeof(junk_data));

        dev->BeginFrame();
        dev->EndFrame();

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_use == sizeof(junk_data));

        // should reuse the same buffer since the data is the same.
        foo->Upload(junk_data, sizeof(junk_data), gfx::Geometry::Usage::Static);

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_use == sizeof(junk_data));

        // should reuse the same buffer since the data is less
        foo->Upload(junk_data, sizeof(junk_data)/2, gfx::Geometry::Usage::Static);

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_use == sizeof(junk_data)); // no decrease here.

        auto* bar = dev->MakeGeometry("bar");
        bar->Upload(junk_data, sizeof(junk_data)/2, gfx::Geometry::Usage::Static);
        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_use == sizeof(junk_data) + sizeof(junk_data)/2);

    }
    dev->DeleteGeometries();

    // streaming. cleared after every frame, allocations remain.
    {
        auto* foo = dev->MakeGeometry("foo");
        foo->Upload(junk_data, sizeof(junk_data), gfx::Geometry::Usage::Stream);

        gfx::Device::ResourceStats stats;

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == sizeof(junk_data));
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_alloc > 0); // from static geometry testing above

        dev->BeginFrame();
        dev->EndFrame();

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc > 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_alloc > 0);
    }

    dev->DeleteGeometries();

    // dynamic
    {
        auto* foo = dev->MakeGeometry("foo");
        foo->Upload(junk_data, sizeof(junk_data), gfx::Geometry::Usage::Dynamic);

        gfx::Device::ResourceStats stats;

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc > 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc > 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == sizeof(junk_data));
        TEST_REQUIRE(stats.static_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_alloc > 0 );

        // should reuse the same buffer since the amount of data is the same.
        foo->Upload(junk_data, sizeof(junk_data), gfx::Geometry::Usage::Dynamic);

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc > 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == sizeof(junk_data));
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == sizeof(junk_data));
        TEST_REQUIRE(stats.static_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_alloc > 0);

        // should reuse the same buffer since the amount of data is less.
        foo->Upload(junk_data, sizeof(junk_data)-1, gfx::Geometry::Usage::Dynamic);

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc > 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == sizeof(junk_data));
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == sizeof(junk_data));
        TEST_REQUIRE(stats.static_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_alloc > 0);

        // grow dynamic buffer.
        char more_junk[1024] = {0};
        foo->Upload(more_junk, sizeof(more_junk), gfx::Geometry::Usage::Dynamic);

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc > 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == sizeof(more_junk) + sizeof(junk_data));
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == sizeof(more_junk));
        TEST_REQUIRE(stats.static_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_alloc > 0);

        // second geometry should be able to re-use the dynamic buffer that should
        // be unused.
        auto* bar = dev->MakeGeometry("bar");
        bar->Upload(junk_data, sizeof(junk_data), gfx::Geometry::Usage::Dynamic);

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc > 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == sizeof(more_junk) + sizeof(junk_data));
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == sizeof(more_junk) + sizeof(junk_data));
        TEST_REQUIRE(stats.static_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_alloc > 0);

    }
}

void unit_test_empty_draw_lost_uniform_bug()
{
    // if a uniform is set in the program and the program
    // is used to draw but the geometry is "empty" the
    // uniform doesn't get set to the program but the hash
    // value is kept. On the next draw if the same program
    // is used with the same uniform value the cached uniform
    // hash value will cause the uniform set to be skipped.
    // thus resulting in incorrect state!

    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

    auto* geom = dev->MakeGeometry("geom");
    // geometry doesn't have any actual vertex data!

    const char* fssrc =
            R"(#version 100
precision mediump float;
uniform vec4 kColor;
void main() {
  gl_FragColor = kColor;
})";

    const char* vssrc =
            R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";
    auto* prog = MakeTestProgram(*dev, vssrc, fssrc);

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);

    prog->SetUniform("kColor", gfx::Color::HotPink);

    gfx::Device::State state;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.bWriteColor  = true;
    state.viewport     = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    // this doesn't actually draw anything (and it cannot draw) because
    // there's no vertex data that has been put in the geometry.
    dev->Draw(*prog, *geom, state);
    dev->EndFrame();

    // now set the actual vertex geometry
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

    // draw
    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);

    // set color uniform again.
    prog->SetUniform("kColor", gfx::Color::HotPink);

    dev->Draw(*prog, *geom, state);
    dev->EndFrame();

    // this has alpha in it.
    const auto& bmp = dev->ReadColorBuffer(10, 10);
    TEST_REQUIRE(bmp.Compare(gfx::Color::HotPink));

}

void unit_test_max_texture_units_single_texture()
{
    // create enough textures to saturate all texture units.
    // then do enough draws to have all texture units become used.
    // then check that textures get evicted/rebound properly.
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

    gfx::Device::DeviceCaps caps = {};
    dev->GetDeviceCaps(&caps);

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

    const char* vssrc =
            R"(#version 100
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
  vTexCoord = aTexCoord;
})";
    const char* fssrc =
            R"(#version 100
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D kTexture;
void main() {
  gl_FragColor = texture2D(kTexture, vTexCoord.xy);
})";
    auto* program = MakeTestProgram(*dev, vssrc, fssrc);

    gfx::Bitmap<gfx::RGBA> bmp(10, 10);
    bmp.Fill(gfx::Color::Green);

    gfx::Device::State state;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;
    state.bWriteColor  = true;
    state.viewport     = gfx::IRect(0, 0, 10, 10);

    for (unsigned  i=0; i<caps.num_texture_units; ++i)
    {
        auto* texture = dev->MakeTexture("texture_" + std::to_string(i));
        texture->Upload(bmp.GetDataPtr(), 10, 10, gfx::Texture::Format::RGBA);
        dev->BeginFrame();
        program->SetTexture("kTexture", 0, *texture);
        dev->Draw(*program, *geom, state);
        dev->EndFrame();
        const auto& ret = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(gfx::Compare(bmp, ret));
    }

    // by now we should have all the texture units in use.
    // create yet another texture and use it to draw thereby
    // forcing some previous texture to be evicted.

    {
        gfx::Bitmap<gfx::RGBA> pink(10, 10);
        pink.Fill(gfx::Color::HotPink);
        auto* texture = dev->MakeTexture("pink");
        texture->SetFilter(gfx::Texture::MinFilter::Trilinear);
        texture->SetFilter(gfx::Texture::MagFilter::Linear);
        texture->Upload(pink.GetDataPtr(), 10, 10, gfx::Texture::Format::RGBA);

        {
            dev->BeginFrame();
            program->SetTexture("kTexture", 0, *texture);
            dev->Draw(*program, *geom, state);
            dev->EndFrame();
            const auto& ret = dev->ReadColorBuffer(10, 10);
            TEST_REQUIRE(gfx::Compare(pink, ret));
        }

        // change the filtering.
        texture->SetFilter(gfx::Texture::MinFilter::Linear);
        texture->SetFilter(gfx::Texture::MagFilter::Linear);

        {
            dev->BeginFrame();
            program->SetTexture("kTexture", 0, *texture);
            dev->Draw(*program, *geom, state);
            dev->EndFrame();
            const auto& ret = dev->ReadColorBuffer(10, 10);
            TEST_REQUIRE(gfx::Compare(pink, ret));
        }
    }
}

void unit_test_max_texture_units_many_textures()
{
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));

    gfx::Device::DeviceCaps caps = {};
    dev->GetDeviceCaps(&caps);

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

    // saturate texture units.
    {
        const char* vssrc =
                R"(#version 100
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
  vTexCoord = aTexCoord;
})";
        const char* fssrc =
                R"(#version 100
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D kTexture;
void main() {
  gl_FragColor = texture2D(kTexture, vTexCoord.xy);
})";
        auto* program = MakeTestProgram(*dev, vssrc, fssrc);

        gfx::Bitmap<gfx::RGBA> bmp(10, 10);
        bmp.Fill(gfx::Color::Green);

        gfx::Device::State state;
        state.blending     = gfx::Device::State::BlendOp::None;
        state.stencil_func = gfx::Device::State::StencilFunc::Disabled;
        state.bWriteColor  = true;
        state.viewport     = gfx::IRect(0, 0, 10, 10);

        for (unsigned  i=0; i<caps.num_texture_units; ++i)
        {
            auto* texture = dev->MakeTexture("texture_" + std::to_string(i));
            texture->Upload(bmp.GetDataPtr(), 10, 10, gfx::Texture::Format::RGBA);
            dev->BeginFrame();
            program->SetTexture("kTexture", 0, *texture);
            dev->Draw(*program, *geom, state);
            dev->EndFrame();
            const auto& ret = dev->ReadColorBuffer(10, 10);
            TEST_REQUIRE(gfx::Compare(bmp, ret));
        }
    }

    // do test, render with multiple textures.
    {
        const char* fssrc =
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
        const char* vssrc =
                R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";
        auto* program = MakeTestProgram(*dev, vssrc, fssrc);
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
        auto* tex0 = dev->MakeTexture("tex0");
        auto* tex1 = dev->MakeTexture("tex1");
        auto* tex2 = dev->MakeTexture("tex2");
        auto* tex3 = dev->MakeTexture("tex3");
        tex0->Upload(r.GetDataPtr(), 1, 1, gfx::Texture::Format::RGBA);
        tex1->Upload(g.GetDataPtr(), 1, 1, gfx::Texture::Format::RGBA);
        tex2->Upload(b.GetDataPtr(), 1, 1, gfx::Texture::Format::RGBA);
        tex3->Upload(a.GetDataPtr(), 1, 1, gfx::Texture::Format::RGBA);

        dev->BeginFrame();

        program->SetTexture("kTexture0", 0, *tex0);
        program->SetTexture("kTexture1", 1, *tex1);
        program->SetTexture("kTexture2", 2, *tex2);
        program->SetTexture("kTexture3", 3, *tex3);

        gfx::Device::State state;
        state.blending = gfx::Device::State::BlendOp::None;
        state.bWriteColor = true;
        state.viewport = gfx::IRect(0, 0, 10, 10);
        state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

        dev->Draw(*program, *geom, state);
        dev->EndFrame();

        const auto& ret = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(ret.Compare(gfx::Color::White));
    }

}


// When drawing multiple times within a single frame with either
// a single material or multiple materials that all map to the same
// underlying GL program object the set of uniforms that need to
// be set on the program object should only have the uniforms that
// have actually changed vs. what is the current program state on the GPU.
// when this test case is written there's a bug that if the same program
// is used to draw multiple times in a single frame the uniform vector
// keeps growing incorrectly.
void unit_test_repeated_uniform_bug()
{
    auto dev = gfx::Device::Create(std::make_shared<TestContext>(10, 10));
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

    const char* fssrc =
            R"(#version 100
precision mediump float;
uniform vec4 kColor;
void main() {
  gl_FragColor = kColor;
})";
    const char* vssrc =
            R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";

    auto* program = MakeTestProgram(*dev, vssrc, fssrc, "prog");

    gfx::Device::State state;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.bWriteColor  = true;
    state.viewport     = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    program->SetUniform("kColor", gfx::Color4f(gfx::Color::Red));
    TEST_REQUIRE(program->GetPendingUniformCount() == 1);

    dev->Draw(*program, *geom, state);
    TEST_REQUIRE(program->GetPendingUniformCount() == 0);

    dev->Draw(*program, *geom, state);
    TEST_REQUIRE(program->GetPendingUniformCount() == 0);

    program->SetUniform("kColor", gfx::Color4f(gfx::Color::Green));
    TEST_REQUIRE(program->GetPendingUniformCount() == 1);
    dev->Draw(*program, *geom, state);
    TEST_REQUIRE(program->GetPendingUniformCount() == 0);

}

int test_main(int argc, char* argv[])
{
    unit_test_device();
    unit_test_shader();
    unit_test_texture();
    unit_test_program();
    unit_test_framebuffer();

    unit_test_render_fbo();
    unit_test_render_color_only();
    unit_test_render_with_single_texture();
    unit_test_render_with_multiple_textures();
    unit_test_render_set_float_uniforms();
    unit_test_render_set_int_uniforms();
    unit_test_render_set_matrix2x2_uniform();
    unit_test_render_set_matrix3x3_uniform();
    unit_test_render_set_matrix4x4_uniform();
    unit_test_uniform_sampler_optimize_bug();
    unit_test_render_dynamic();
    unit_test_clean_textures();
    unit_test_buffer_allocation();
    unit_test_max_texture_units_single_texture();
    unit_test_max_texture_units_many_textures();
    // bugs
    unit_test_empty_draw_lost_uniform_bug();
    unit_test_repeated_uniform_bug();
    return 0;
}
