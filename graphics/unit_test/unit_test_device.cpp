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
#include "device/device.h"
#include "graphics/algo.h"
#include "graphics/color4f.h"
#include "graphics/device.h"
#include "graphics/program.h"
#include "graphics/texture.h"
#include "graphics/shader.h"
#include "graphics/geometry.h"
#include "graphics/framebuffer.h"
#include "graphics/drawcmd.h"
#include "graphics/instance.h"

// We need this to create the rendering context.
#include "wdk/opengl/context.h"
#include "wdk/opengl/config.h"
#include "wdk/opengl/surface.h"

template<typename Pixel>
size_t CountPixels(const gfx::Bitmap<Pixel>& bmp, gfx::Color color)
{
    size_t ret = 0;
    for (unsigned y=0; y<bmp.GetHeight(); ++y) {
        for (unsigned x=0; x<bmp.GetWidth(); ++x)
            if (bmp.GetPixel(y, x) == color)
                ++ret;
    }
    return ret;
}

// setup context for headless rendering.
class TestContext : public dev::Context
{
public:
    static unsigned GL_ES_Version;

    TestContext(unsigned w, unsigned h)
    {
        wdk::Config::Attributes attrs;
        attrs.red_size         = 8;
        attrs.green_size       = 8;
        attrs.blue_size        = 8;
        attrs.alpha_size       = 8;
        attrs.stencil_size     = 8;
        attrs.surfaces.pbuffer = true;
        attrs.double_buffer    = false;
        attrs.srgb_buffer      = true;
        constexpr auto debug_context = true;
        mConfig   = std::make_unique<wdk::Config>(attrs);
        mContext  = std::make_unique<wdk::Context>(*mConfig, GL_ES_Version, 0, debug_context, wdk::Context::Type::OpenGL_ES);
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
        if (GL_ES_Version == 2)
            return Version::OpenGL_ES2;
        else if (GL_ES_Version == 3)
            return Version::OpenGL_ES3;
        else BUG("Missing OpenGL ES version");
    }

private:
    std::unique_ptr<wdk::Context> mContext;
    std::unique_ptr<wdk::Surface> mSurface;
    std::unique_ptr<wdk::Config>  mConfig;
};

unsigned TestContext::GL_ES_Version = 2;

gfx::ProgramPtr MakeTestProgram(gfx::Device& dev, const char* vssrc, const char* fssrc, const std::string name = "prog")
{
    gfx::Shader::CreateArgs vertex_shader_args;
    vertex_shader_args.name   = name + "/vertex";
    vertex_shader_args.source = vssrc;

    gfx::Shader::CreateArgs fragment_shader_args;
    fragment_shader_args.name = name + "/fragment";
    fragment_shader_args.source = fssrc;

    auto vs = dev.CreateShader(name + "/vert", vertex_shader_args);
    auto fs = dev.CreateShader(name + "/frag", fragment_shader_args);
    TEST_REQUIRE(vs->IsValid());
    TEST_REQUIRE(fs->IsValid());
    std::vector<gfx::ShaderPtr> shaders;
    shaders.push_back(vs);
    shaders.push_back(fs);

    gfx::Program::CreateArgs args;
    args.name = name;
    args.fragment_shader = fs;
    args.vertex_shader   = vs;
    auto prog = dev.CreateProgram(name, args);
    TEST_REQUIRE(prog->IsValid());
    return prog;
}

gfx::GeometryPtr MakeQuad(gfx::Device& dev)
{
    const gfx::Vertex2D verts[] = {
        { {-1,  1}, {0, 1} },
        { {-1, -1}, {0, 0} },
        { { 1, -1}, {1, 0} },

        { {-1,  1}, {0, 1} },
        { { 1, -1}, {1, 0} },
        { { 1,  1}, {1, 1} }
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    args.content_name = "quad";

    auto geom = dev.CreateGeometry("quad", std::move(args));
    return geom;
}

std::shared_ptr<gfx::Device> CreateDevice()
{
    return dev::CreateDevice(std::make_shared<TestContext>(10, 10))->GetSharedGraphicsDevice();
}

std::shared_ptr<gfx::Device> CreateDevice(unsigned render_width, unsigned render_height)
{
    return dev::CreateDevice(std::make_shared<TestContext>(render_width, render_height))->GetSharedGraphicsDevice();
}

void unit_test_shader()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    // junk
    {
        gfx::Shader::CreateArgs args;
        args.name = "test";
        args.source = "bla bla";

        auto shader = dev->CreateShader("foo", args);
        TEST_REQUIRE(shader->IsValid() == false);
    }

    // fragment shader
    {
        // missing frag gl_FragColor
        gfx::Shader::CreateArgs args;
        args.name   = "test";
        args.source =
R"(#version 100
precision mediump float;
void main()
{
})";

        auto shader = dev->CreateShader("foo", args);
        TEST_REQUIRE(shader->IsValid() == false);

        args.source =
R"(#version 100
precision mediump float;
void main() {
  gl_FragColor = vec4(1.0);
})";

        shader = dev->CreateShader("foo", args);
        TEST_REQUIRE(shader->IsValid() == true);
    }

    // vertex shader
    {

        // missing gl_Position
        gfx::Shader::CreateArgs args;
        args.name = "test";
        args.source =
R"(#version 100
attribute vec position;
void main() {}
)";
        auto shader = dev->CreateShader("foo", args);
        TEST_REQUIRE(shader->IsValid() == false);

        args.source =
R"(#version 100
void main() {
    gl_Position = vec4(1.0);
    }
)";
        shader = dev->CreateShader("foo", args);
        TEST_REQUIRE(shader->IsValid());
    }
}

void unit_test_texture()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    auto* texture = dev->MakeTexture("foo");
    TEST_REQUIRE(texture->GetWidth() == 0);
    TEST_REQUIRE(texture->GetHeight() == 0);
    TEST_REQUIRE(texture->GetMinFilter() == gfx::Texture::MinFilter::Default);
    TEST_REQUIRE(texture->GetMagFilter() == gfx::Texture::MagFilter::Default);
    TEST_REQUIRE(texture->GetWrapX() == gfx::Texture::Wrapping::Repeat);
    TEST_REQUIRE(texture->GetWrapY() == gfx::Texture::Wrapping::Repeat);
    // format is unspecified.

    const gfx::Pixel_RGB pixels[2 * 3] = {
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

void unit_test_render_fbo(gfx::Framebuffer::Format format, gfx::Framebuffer::MSAA msaa)
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    auto geom = MakeQuad(*dev);

    // rendered a colored quad into the fbo then use the fbo
    // color buffer texture to sample in another program.
    auto p0 = MakeTestProgram(*dev,
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

    auto p1 = MakeTestProgram(*dev,
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

    // let the FBO allocate the color target buffer.
    {

        gfx::Framebuffer::Config conf;
        conf.format = format;
        conf.width  = 10;
        conf.height = 10;
        conf.msaa   = msaa;
        auto* fbo = dev->MakeFramebuffer("test");
        fbo->SetConfig(conf);

        // do a couple of loops so that the texture gets bound for sampling
        // and for rendering.
        for (int i = 0; i < 2; ++i)
        {
            dev->BeginFrame();
            // clear the FBO to render and then render the green quad into it.
            dev->ClearColor(gfx::Color::Red, fbo);
            dev->Draw(*p0, *geom, state, fbo);

            // render using the second program and sample from the FBO texture.
            gfx::Texture* color = nullptr;
            fbo->Resolve(&color);
            color->SetFilter(gfx::Texture::MinFilter::Linear);
            color->SetFilter(gfx::Texture::MagFilter::Linear);

            p1->SetTexture("kTexture", 0, *color);

            dev->ClearColor(gfx::Color::Blue, nullptr);
            dev->Draw(*p1, *geom, state, nullptr);

            dev->EndFrame();

            const auto& bmp = dev->ReadColorBuffer(10, 10, nullptr);
            TEST_REQUIRE(bmp.Compare(gfx::Color::Green));
        }

        dev->DeleteFramebuffers();
    }

    // Configure the FBO to use a texture allocated by the caller.
    {

        auto* target = dev->MakeTexture("target");
        target->Upload(nullptr, 10, 10, gfx::Texture::Format::RGBA, false);
        target->SetName("FBO-color-target");

        gfx::Framebuffer::Config conf;
        conf.format = format;
        conf.width  = 10;
        conf.height = 10;
        conf.msaa   = msaa;
        auto* fbo = dev->MakeFramebuffer("test");
        fbo->SetConfig(conf);
        fbo->SetColorTarget(target);

        // do a couple of loops so that the texture gets bound for sampling
        // and for rendering.
        for (int i = 0; i < 2; ++i)
        {
            dev->BeginFrame();
            // clear the FBO to render and then render the green quad into it.
            dev->ClearColor(gfx::Color::Red, fbo);
            dev->Draw(*p0, *geom, state, fbo);

            // render using the second program and sample from the FBO texture.
            gfx::Texture* color = nullptr;
            fbo->Resolve(&color);
            color->SetFilter(gfx::Texture::MinFilter::Linear);
            color->SetFilter(gfx::Texture::MagFilter::Linear);

            p1->SetTexture("kTexture", 0, *color);

            dev->ClearColor(gfx::Color::Blue, nullptr);
            dev->Draw(*p1, *geom, state, nullptr);

            dev->EndFrame();

            const auto& bmp = dev->ReadColorBuffer(10, 10, nullptr);
            TEST_REQUIRE(bmp.Compare(gfx::Color::Green));
        }

    }
}

void unit_test_render_color_only()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    constexpr const char* fragment_src =
R"(#version 100
precision mediump float;
void main() {
  gl_FragColor = vec4(1.0);
})";

    constexpr const char* vertex_src =
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";

    auto prog = MakeTestProgram(*dev, vertex_src, fragment_src);

    gfx::Device::State state;
    state.bWriteColor  = true;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;
    state.viewport     = gfx::IRect(0, 0, 10, 10);

    // draw using vertex buffer only
    {

        constexpr const gfx::Vertex2D vertices[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },

            { {-1,  1}, {0, 1} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
        };

        gfx::Geometry::CreateArgs args;
        args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
        args.buffer.SetVertexBuffer(vertices, 6);
        args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
        auto geom = dev->CreateGeometry("geom", std::move(args));

        dev->BeginFrame();
          dev->ClearColor(gfx::Color::Red);
          dev->Draw(*prog, *geom, state);
        dev->EndFrame();

        // this has alpha in it.
        const auto& bmp = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(bmp.Compare(gfx::Color::White));
    }

    // draw using vertex and index buffer
    {

        constexpr const gfx::Vertex2D vertices[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
        };
        constexpr const gfx::Index16 indices[] = {
            0, 1, 2, // bottom triangle
            0, 2, 3  // top triangle
        };
        gfx::Geometry::CreateArgs args;
        args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
        args.buffer.SetVertexBuffer(vertices, 4);
        args.buffer.SetIndexBuffer(indices, 6);
        args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
        auto geom =  dev->CreateGeometry("geom", std::move(args));

        dev->BeginFrame();
            dev->ClearColor(gfx::Color::Red);
            dev->Draw(*prog, *geom, state);
        dev->EndFrame();

        // this has alpha in it.
        const auto& bmp = dev->ReadColorBuffer(10, 10);
        TEST_REQUIRE(bmp.Compare(gfx::Color::White));
    }
}

void unit_test_render_with_single_texture()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    gfx::Bitmap<gfx::Pixel_RGBA> data(4, 4);
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

    const gfx::Vertex2D verts[] = {
        { {-1,  1}, {0, 0} },
        { {-1, -1}, {0, 1} },
        { { 1, -1}, {1, 1} },

        { {-1,  1}, {0, 0} },
        { { 1, -1}, {1, 1} },
        { { 1,  1}, {1, 0} }
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    auto geom = dev->CreateGeometry("geom", std::move(args));

    constexpr const char* fssrc =
R"(#version 100
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D kTexture;
void main() {
  gl_FragColor = texture2D(kTexture, vTexCoord.xy);
})";

    constexpr const char* vssrc =
R"(#version 100
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
  vTexCoord = aTexCoord;
})";

    auto prog = MakeTestProgram(*dev, vssrc, fssrc, "prog");


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
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    // setup 4 textures and the output from fragment shader
    // is then the sum of all of these, i.e. white.
    gfx::Bitmap<gfx::Pixel_RGBA> r(1, 1);
    gfx::Bitmap<gfx::Pixel_RGBA> g(1, 1);
    gfx::Bitmap<gfx::Pixel_RGBA> b(1, 1);
    gfx::Bitmap<gfx::Pixel_RGBA> a(1, 1);
    r.SetPixel(0, 0, gfx::Color::Red);
    g.SetPixel(0, 0, gfx::Color::Green);
    b.SetPixel(0, 0, gfx::Color::Blue);
    a.SetPixel(0, 0, gfx::Pixel_RGBA(0, 0, 0, 0xff));

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::White);

    const gfx::Vertex2D verts[] = {
        { {-1,  1}, {0, 0} },
        { {-1, -1}, {0, 1} },
        { { 1, -1}, {1, 1} },

        { {-1,  1}, {0, 0} },
        { { 1, -1}, {1, 1} },
        { { 1,  1}, {1, 0} }
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    auto geom = dev->CreateGeometry("geom", std::move(args));

    constexpr const char* fssrc =
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

    constexpr const char* vssrc =
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";
    auto prog = MakeTestProgram(*dev, vssrc, fssrc, "prog");

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
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    const gfx::Vertex2D verts[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },

            { {-1,  1}, {0, 1} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    auto geom = dev->CreateGeometry("geom", std::move(args));

    constexpr const char* fssrc =
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

    constexpr const char* vssrc =
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";

    auto prog = MakeTestProgram(*dev, vssrc, fssrc, "prog");

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
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    const gfx::Vertex2D verts[] = {
            {{-1, 1},  {0, 1}},
            {{-1, -1}, {0, 0}},
            {{1,  -1}, {1, 0}},

            {{-1, 1},  {0, 1}},
            {{1,  -1}, {1, 0}},
            {{1,  1},  {1, 1}}
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    auto geom = dev->CreateGeometry("geom", std::move(args));

    constexpr const char* fssrc =
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

    constexpr const char* vssrc =
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";

    auto prog = MakeTestProgram(*dev, vssrc, fssrc, "prog");


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
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    const gfx::Vertex2D verts[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },

            { {-1,  1}, {0, 1} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    auto geom = dev->CreateGeometry("geom", std::move(args));

    constexpr const char* fssrc =
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

    constexpr const char* vssrc =
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";

    auto prog = MakeTestProgram(*dev, vssrc, fssrc, "prog");

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);
    gfx::Device::State state;
    state.blending = gfx::Device::State::BlendOp::None;
    state.bWriteColor = true;
    state.viewport = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    const glm::mat2 matrix = {
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
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    const gfx::Vertex2D verts[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },

            { {-1,  1}, {0, 1} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    auto geom = dev->CreateGeometry("geom", std::move(args));

    constexpr const char* fssrc =
R"(#version 100
precision mediump float;
uniform mat3 kMatrix;
void main() {
  float r = kMatrix[0][0] + kMatrix[0][1] + kMatrix[0][2];
  float g = kMatrix[1][0] + kMatrix[1][1] + kMatrix[1][2];
  float b = kMatrix[2][0] + kMatrix[2][1] + kMatrix[2][2];
  gl_FragColor = vec4(r, g, b, 1.0);
})";

    constexpr const char* vssrc =
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";

    auto prog = MakeTestProgram(*dev, vssrc, fssrc, "prog");

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);
    gfx::Device::State state;
    state.blending = gfx::Device::State::BlendOp::None;
    state.bWriteColor = true;
    state.viewport = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    const glm::mat3 matrix = {
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
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    const gfx::Vertex2D verts[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },

            { {-1,  1}, {0, 1} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    auto geom = dev->CreateGeometry("geom", std::move(args));

    constexpr const char* fssrc =
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

    constexpr const char* vssrc =
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";

    auto prog = MakeTestProgram(*dev, vssrc, fssrc, "prog");

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);
    gfx::Device::State state;
    state.blending = gfx::Device::State::BlendOp::None;
    state.bWriteColor = true;
    state.viewport = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    const glm::mat4 matrix = {
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
    TEST_CASE(test::Type::Feature)

    // shader code doesn't actually use the material, the sampler/uniform location
    // is thus -1 and no texture will be set.
    auto dev = CreateDevice();

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);

    auto* texture = dev->MakeTexture("foo");
    const gfx::Pixel_RGB pixels[2 * 3] = {
            gfx::Color::White, gfx::Color::White,
            gfx::Color::Red, gfx::Color::Red,
            gfx::Color::Blue, gfx::Color::Blue
    };
    texture->Upload(pixels, 2, 3, gfx::Texture::Format::RGB);

    const gfx::Vertex2D verts[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },

            { {-1,  1}, {0, 1} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    auto geom = dev->CreateGeometry("geom", std::move(args));

    // no mention of the texture sampler in the fragment shader!
    constexpr const char* fssrc =
R"(#version 100
precision mediump float;
void main() {
  gl_FragColor = vec4(1.0);
})";

    constexpr const char* vssrc =
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";
    auto prog = MakeTestProgram(*dev, vssrc, fssrc, "prog");

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
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    gfx::Device::State state;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.bWriteColor  = true;
    state.viewport     = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    const gfx::Pixel_RGB pixels[2 * 3] = {
        gfx::Color::White, gfx::Color::White,
        gfx::Color::Red, gfx::Color::Red,
        gfx::Color::Blue, gfx::Color::Blue
    };
    const gfx::Vertex2D verts[] = {
        { {-1,  1}, {0, 1} },
        { {-1, -1}, {0, 0} },
        { { 1, -1}, {1, 0} },

        { {-1,  1}, {0, 1} },
        { { 1, -1}, {1, 0} },
        { { 1,  1}, {1, 1} }
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    auto geom = dev->CreateGeometry("geom", std::move(args));

    // texture that is used is not cleaned
    {
        auto* texture = dev->MakeTexture("foo");
        texture->Upload(pixels, 2, 3, gfx::Texture::Format::RGB);
        TEST_REQUIRE(dev->FindTexture("foo"));

        constexpr const char* vs =
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
}
)";

        constexpr const char* fs =
R"(#version 100
precision mediump float;
uniform sampler2D kTexture;
void main() {
  gl_FragColor = texture2D(kTexture, vec2(0.5));
}
)";
        auto prog = MakeTestProgram(*dev, vs, fs);

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

        constexpr const char* vs =
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
}
)";

        constexpr const char* fs =
R"(#version 100
precision mediump float;
uniform sampler2D kTexture;
void main() {
  gl_FragColor = vec4(1.0);
}
)";
        auto prog = MakeTestProgram(*dev, vs, fs);

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

    // texture with GC disabled doesn't get cleaned away
    {
        auto* texture = dev->MakeTexture("foo");
        texture->Upload(pixels, 2, 3, gfx::Texture::Format::RGB);
        texture->SetFlag(gfx::Texture::Flags::GarbageCollect, false);
        TEST_REQUIRE(dev->FindTexture("foo"));

        dev->BeginFrame();
        dev->EndFrame();
        dev->CleanGarbage(2, gfx::Device::GCFlags::Textures);
        TEST_REQUIRE(dev->FindTexture("foo"));

        dev->BeginFrame();
        dev->EndFrame();
        dev->CleanGarbage(2, gfx::Device::GCFlags::Textures);
        TEST_REQUIRE(dev->FindTexture("foo"));
    }


    dev->DeleteTextures();
}


void unit_test_vbo_allocation()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    char junk_data[512] = {0};

    // static
    {
        {
            gfx::Geometry::CreateArgs args;
            args.buffer.SetVertexBuffer(junk_data, sizeof(junk_data));
            args.usage = gfx::GeometryBuffer::Usage::Static;
            auto foo = dev->CreateGeometry("foo", std::move(args));
        }

        gfx::Device::ResourceStats stats;

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_use == sizeof(junk_data));
        TEST_REQUIRE(stats.static_vbo_mem_alloc >= sizeof(junk_data));

        dev->BeginFrame();
        dev->EndFrame();

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_use == sizeof(junk_data));

        {
            gfx::Geometry::CreateArgs args;
            args.buffer.SetVertexBuffer(junk_data, sizeof(junk_data) / 2);
            args.usage = gfx::GeometryBuffer::Usage::Static;
            auto bar = dev->CreateGeometry("bar", std::move(args));
        }

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_use == sizeof(junk_data) + sizeof(junk_data)/2);

        dev->DeleteGeometries();
        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_use == 0);
    }

    dev->DeleteGeometries();

    // streaming. cleared after every frame, allocations remain.
    {
        {
            gfx::Geometry::CreateArgs args;
            args.buffer.SetVertexBuffer(junk_data, sizeof(junk_data));
            args.usage = gfx::GeometryBuffer::Usage::Stream;
            auto foo = dev->CreateGeometry("foo", std::move(args));
        }

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
        {
            gfx::Geometry::CreateArgs args;
            args.buffer.SetVertexBuffer(junk_data, sizeof(junk_data));
            args.usage = gfx::GeometryBuffer::Usage::Dynamic;
            auto foo = dev->CreateGeometry("foo", std::move(args));
        }

        gfx::Device::ResourceStats stats;

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc > 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc >= sizeof(junk_data));
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == sizeof(junk_data));
        TEST_REQUIRE(stats.static_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_alloc > 0 );

        {
            gfx::Geometry::CreateArgs args;
            args.buffer.SetVertexBuffer(junk_data, sizeof(junk_data));
            args.usage = gfx::GeometryBuffer::Usage::Dynamic;
            auto bar = dev->CreateGeometry("bar", std::move(args));
        }

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_vbo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_vbo_mem_alloc > 0);
        TEST_REQUIRE(stats.dynamic_vbo_mem_alloc == sizeof(junk_data) + sizeof(junk_data));
        TEST_REQUIRE(stats.dynamic_vbo_mem_use == sizeof(junk_data) + sizeof(junk_data));
        TEST_REQUIRE(stats.static_vbo_mem_use == 0);
        TEST_REQUIRE(stats.static_vbo_mem_alloc > 0);

    }
}

void unit_test_ibo_allocation()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    char junk_data[512] = {0};

    // static
    {
        {
            gfx::Geometry::CreateArgs args;
            args.buffer.UploadVertices(junk_data, sizeof(junk_data));
            args.buffer.UploadIndices(junk_data, sizeof(junk_data), gfx::Geometry::IndexType ::Index16);
            args.usage = gfx::GeometryBuffer::Usage::Static;
            auto foo = dev->CreateGeometry("foo", std::move(args));
        }


        gfx::Device::ResourceStats stats;

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_ibo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_ibo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_ibo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_ibo_mem_use == 0);
        TEST_REQUIRE(stats.static_ibo_mem_use == sizeof(junk_data));

        dev->BeginFrame();
        dev->EndFrame();

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_ibo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_ibo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_ibo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_ibo_mem_use == 0);
        TEST_REQUIRE(stats.static_ibo_mem_use == sizeof(junk_data));

        {
            gfx::Geometry::CreateArgs args;
            args.buffer.UploadVertices(junk_data, sizeof(junk_data));
            args.buffer.UploadIndices(junk_data, sizeof(junk_data), gfx::Geometry::IndexType ::Index16);
            args.usage = gfx::GeometryBuffer::Usage::Static;
            auto foo = dev->CreateGeometry("bar", std::move(args));
        }

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_ibo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_ibo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_ibo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_ibo_mem_use == 0);
        TEST_REQUIRE(stats.static_ibo_mem_use == sizeof(junk_data) + sizeof(junk_data));
    }
    dev->DeleteGeometries();

    // streaming. cleared after every frame, allocations remain.
    {
        {
            gfx::Geometry::CreateArgs args;
            args.buffer.UploadVertices(junk_data, sizeof(junk_data));
            args.buffer.UploadIndices(junk_data, sizeof(junk_data), gfx::Geometry::IndexType ::Index16);
            args.usage = gfx::GeometryBuffer::Usage::Stream;
            auto foo = dev->CreateGeometry("foo", std::move(args));
        }

        gfx::Device::ResourceStats stats;

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_ibo_mem_use == sizeof(junk_data));
        TEST_REQUIRE(stats.dynamic_ibo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_ibo_mem_use == 0);
        TEST_REQUIRE(stats.static_ibo_mem_use == 0);
        TEST_REQUIRE(stats.static_ibo_mem_alloc > 0); // from static geometry testing above

        dev->BeginFrame();
        dev->EndFrame();

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_ibo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_ibo_mem_alloc > 0);
        TEST_REQUIRE(stats.dynamic_ibo_mem_alloc == 0);
        TEST_REQUIRE(stats.dynamic_ibo_mem_use == 0);
        TEST_REQUIRE(stats.static_ibo_mem_use == 0);
        TEST_REQUIRE(stats.static_ibo_mem_alloc > 0);
    }

    dev->DeleteGeometries();

    // dynamic
    {

        {
            gfx::Geometry::CreateArgs args;
            args.buffer.UploadVertices(junk_data, sizeof(junk_data));
            args.buffer.UploadIndices(junk_data, sizeof(junk_data), gfx::Geometry::IndexType ::Index16);
            args.usage = gfx::GeometryBuffer::Usage::Dynamic;
            auto foo = dev->CreateGeometry("foo", std::move(args));
        }

        gfx::Device::ResourceStats stats;

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_ibo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_ibo_mem_alloc > 0);
        TEST_REQUIRE(stats.dynamic_ibo_mem_alloc > 0);
        TEST_REQUIRE(stats.dynamic_ibo_mem_use == sizeof(junk_data));
        TEST_REQUIRE(stats.static_ibo_mem_use == 0);
        TEST_REQUIRE(stats.static_ibo_mem_alloc > 0 );


        {
            gfx::Geometry::CreateArgs args;
            args.buffer.UploadVertices(junk_data, sizeof(junk_data));
            args.buffer.UploadIndices(junk_data, sizeof(junk_data), gfx::Geometry::IndexType ::Index16);
            args.usage = gfx::GeometryBuffer::Usage::Dynamic;
            auto foo = dev->CreateGeometry("bar", std::move(args));
        }

        dev->GetResourceStats(&stats);
        TEST_REQUIRE(stats.streaming_ibo_mem_use == 0);
        TEST_REQUIRE(stats.streaming_ibo_mem_alloc > 0);
        TEST_REQUIRE(stats.dynamic_ibo_mem_alloc == sizeof(junk_data) + sizeof(junk_data));
        TEST_REQUIRE(stats.dynamic_ibo_mem_use == sizeof(junk_data) + sizeof(junk_data));
        TEST_REQUIRE(stats.static_ibo_mem_use == 0);
        TEST_REQUIRE(stats.static_ibo_mem_alloc > 0);
    }
}

void unit_test_empty_draw_lost_uniform_bug()
{
    TEST_CASE(test::Type::Feature)

    // if a uniform is set in the program and the program
    // is used to draw but the geometry is "empty" the
    // uniform doesn't get set to the program but the hash
    // value is kept. On the next draw if the same program
    // is used with the same uniform value the cached uniform
    // hash value will cause the uniform set to be skipped.
    // thus resulting in incorrect state!

    auto dev = CreateDevice();

    gfx::Geometry::CreateArgs args;
    auto empty = dev->CreateGeometry("geom", args);
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
    auto prog = MakeTestProgram(*dev, vssrc, fssrc);

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);

    prog->SetUniform("kColor", gfx::Color::Green);

    gfx::Device::State state;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.bWriteColor  = true;
    state.viewport     = gfx::IRect(0, 0, 10, 10);
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;

    // this doesn't actually draw anything (and it cannot draw) because
    // there's no vertex data that has been put in the geometry.
    dev->Draw(*prog, *empty, state);
    dev->EndFrame();

    // now set the actual vertex geometry
    const gfx::Vertex2D verts[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },

            { {-1,  1}, {0, 1} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
    };
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    auto geom = dev->CreateGeometry("geom", std::move(args));

    // draw
    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);

    // set color uniform again.
    prog->SetUniform("kColor", gfx::Color::Green);

    dev->Draw(*prog, *geom, state);
    dev->EndFrame();

    // this has alpha in it.
    const auto& bmp = dev->ReadColorBuffer(10, 10);
    TEST_REQUIRE(bmp.Compare(gfx::Color::Green));

}

void unit_test_max_texture_units_single_texture()
{
    TEST_CASE(test::Type::Feature)

    // create enough textures to saturate all texture units.
    // then do enough draws to have all texture units become used.
    // then check that textures get evicted/rebound properly.
    auto dev = CreateDevice();

    gfx::Device::DeviceCaps caps = {};
    dev->GetDeviceCaps(&caps);


    const gfx::Vertex2D verts[] = {
        { {-1,  1}, {0, 0} },
        { {-1, -1}, {0, 1} },
        { { 1, -1}, {1, 1} },
        { {-1,  1}, {0, 0} },
        { { 1, -1}, {1, 1} },
        { { 1,  1}, {1, 0} }
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    auto geom = dev->CreateGeometry("geom", std::move(args));

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
    auto program = MakeTestProgram(*dev, vssrc, fssrc);

    gfx::Bitmap<gfx::Pixel_RGBA> bmp(10, 10);
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
        gfx::Bitmap<gfx::Pixel_RGBA> pink(10, 10);
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
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    gfx::Device::DeviceCaps caps = {};
    dev->GetDeviceCaps(&caps);


    const gfx::Vertex2D verts[] = {
        { {-1,  1}, {0, 0} },
        { {-1, -1}, {0, 1} },
        { { 1, -1}, {1, 1} },
        { {-1,  1}, {0, 0} },
        { { 1, -1}, {1, 1} },
        { { 1,  1}, {1, 0} }
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    auto geom = dev->CreateGeometry("geom", std::move(args));

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
        auto program = MakeTestProgram(*dev, vssrc, fssrc);

        gfx::Bitmap<gfx::Pixel_RGBA> bmp(10, 10);
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
        auto program = MakeTestProgram(*dev, vssrc, fssrc);
        // setup 4 textures and the output from fragment shader
        // is then the sum of all of these, i.e. white.
        gfx::Bitmap<gfx::Pixel_RGBA> r(1, 1);
        gfx::Bitmap<gfx::Pixel_RGBA> g(1, 1);
        gfx::Bitmap<gfx::Pixel_RGBA> b(1, 1);
        gfx::Bitmap<gfx::Pixel_RGBA> a(1, 1);
        r.SetPixel(0, 0, gfx::Color::Red);
        g.SetPixel(0, 0, gfx::Color::Green);
        b.SetPixel(0, 0, gfx::Color::Blue);
        a.SetPixel(0, 0, gfx::Pixel_RGBA(0, 0, 0, 0xff));
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

void unit_test_algo_texture_copy()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    auto* src = dev->MakeTexture("src");
    auto* dst = dev->MakeTexture("dst");

    gfx::Bitmap<gfx::Pixel_RGBA> bmp(10, 10);
    bmp.Fill(gfx::Color::Red);
    bmp.Fill(gfx::URect(0, 0, 10, 5), gfx::Color::Green);
    // flip the bitmap now temporarily to match the layout expected by OpenGL
    bmp.FlipHorizontally();
    src->Upload(bmp.GetDataPtr(), 10, 10, gfx::Texture::Format::RGBA, true);
    // flip back to our representation.
    bmp.FlipHorizontally();

    dst->Allocate(10, 10, gfx::Texture::Format::RGBA);

    gfx::algo::CopyTexture(src, dst, dev.get());

    gfx::Framebuffer::Config conf;
    conf.format = gfx::Framebuffer::Format::ColorRGBA8;
    conf.width  = 10;
    conf.height = 10;
    auto* fbo = dev->MakeFramebuffer("test");
    fbo->SetConfig(conf);
    fbo->SetColorTarget(dst);

    const auto& ret = dev->ReadColorBuffer(10, 10, fbo);
    TEST_REQUIRE(ret == bmp);
}

void unit_test_algo_texture_flip()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    auto* tex = dev->MakeTexture("texture");

    gfx::Bitmap<gfx::Pixel_RGBA> bmp(10, 10);
    bmp.Fill(gfx::Color::Red);
    bmp.Fill(gfx::URect(0, 0, 10, 5), gfx::Color::Green);
    // flip the bitmap now temporarily to match the layout expected by OpenGL
    bmp.FlipHorizontally();
    tex->Upload(bmp.GetDataPtr(), 10, 10, gfx::Texture::Format::RGBA, true);
    // flip back to our presentation.
    bmp.FlipHorizontally();

    gfx::algo::FlipTexture("texture", tex, dev.get(), gfx::algo::FlipDirection::Horizontal);

    bmp.FlipHorizontally();

    gfx::Framebuffer::Config conf;
    conf.format = gfx::Framebuffer::Format::ColorRGBA8;
    conf.width  = 10;
    conf.height = 10;
    auto* fbo = dev->MakeFramebuffer("test");
    fbo->SetConfig(conf);
    fbo->SetColorTarget(tex);

    const auto& ret = dev->ReadColorBuffer(10, 10, fbo);
    TEST_REQUIRE(ret == bmp);

    // todo: vertical flip test
}

void unit_test_algo_texture_read()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    auto* tex = dev->MakeTexture("texture");

    gfx::Bitmap<gfx::Pixel_RGBA> bmp(10, 10);
    bmp.Fill(gfx::Color::Red);
    bmp.Fill(gfx::URect(0, 0, 10, 5), gfx::Color::Green);
    // flip the bitmap now temporarily to match the layout expected by OpenGL
    bmp.FlipHorizontally();
    tex->Upload(bmp.GetDataPtr(), 10, 10, gfx::Texture::Format::RGBA, true);
    // flip back to our presentation.
    bmp.FlipHorizontally();

    const auto& ret = gfx::algo::ReadTexture(tex, dev.get());
    TEST_REQUIRE(ret);
    TEST_REQUIRE(ret->GetDepthBits() == 32);
    TEST_REQUIRE(ret->GetWidth() == 10);
    TEST_REQUIRE(ret->GetHeight() == 10);
    const auto* rgba_ret = dynamic_cast<const gfx::RgbaBitmap*>(ret.get());
    TEST_REQUIRE(*rgba_ret == bmp);
}

void unit_test_instanced_rendering()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice(200, 200);

    constexpr const char* fragment_src =
R"(#version 300 es
precision highp float;
out vec4 fragOutColor;
void main() {
  fragOutColor = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

    constexpr const char* vertex_src =
R"(#version 300 es
in vec2 aPosition;

// per instance attributes
in vec2 iaSize;
in vec2 iaPosition;

void main() {
  vec2 pos = aPosition * iaSize + iaPosition;
  gl_Position = vec4(pos.xy, 0.0, 1.0);
}
)";

    auto program = MakeTestProgram(*dev, vertex_src, fragment_src);

    gfx::Device::State state;
    state.bWriteColor  = true;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;
    state.viewport     = gfx::IRect(0, 0, 200, 200);

#pragma pack(push, 1)
    struct InstanceAttribute {
        gfx::Vec2 iaSize;
        gfx::Vec2 iaPosition;
    };
#pragma pack(pop)
    static const gfx::GeometryInstanceDataLayout layout(sizeof(InstanceAttribute), {
        {"iaSize",     0, 2, 0, offsetof(InstanceAttribute, iaSize)    },
        {"iaPosition", 0, 2, 0, offsetof(InstanceAttribute, iaPosition)}
    });

    // create reference (expected result)
    gfx::RgbaBitmap reference;
    reference.Resize(200, 200);
    reference.Fill(gfx::Color::DarkRed);

    gfx::URect rect;
    rect.Resize(20, 20);
    rect.Move(100, 100);
    rect.Translate(-50, -50);
    rect.Translate(-10, -10);
    reference.Fill(rect, gfx::Color::White);

    rect.Move(100, 100);
    rect.Translate(-50, 50);
    rect.Translate(-10, -10);
    reference.Fill(rect, gfx::Color::White);

    rect.Move(100, 100);
    rect.Translate(50, -50);
    rect.Translate(-10, -10);
    reference.Fill(rect, gfx::Color::White);

    rect.Move(100, 100);
    rect.Translate(50, 50);
    rect.Translate(-10, -10);
    reference.Fill(rect, gfx::Color::White);


    std::vector<InstanceAttribute> instances;
    instances.resize(4);
    instances[0].iaSize     = gfx::Vec2 {  0.1f,  0.1f };
    instances[0].iaPosition = gfx::Vec2 { -0.5f,  0.5f };
    instances[1].iaSize     = gfx::Vec2 {  0.1f,  0.1f };
    instances[1].iaPosition = gfx::Vec2 {  0.5f,  0.5f };
    instances[2].iaSize     = gfx::Vec2 {  0.1f,  0.1f };
    instances[2].iaPosition = gfx::Vec2 {  0.5f, -0.5f };
    instances[3].iaSize     = gfx::Vec2 {  0.1f,  0.1f };
    instances[3].iaPosition = gfx::Vec2 { -0.5f, -0.5f };

    gfx::GeometryInstance::CreateArgs instance_args;
    instance_args.usage = gfx::GeometryInstanceBuffer::Usage::Static;
    instance_args.buffer.SetVertexLayout(layout);
    instance_args.buffer.SetInstanceBuffer(instances);
    auto inst = dev->CreateGeometryInstance("inst", std::move(instance_args));

    // instanced draw arrays
    {
        constexpr const gfx::Vertex2D vertices[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },

            { {-1,  1}, {0, 1} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
        };

        gfx::Geometry::CreateArgs args;
        args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
        args.buffer.SetVertexBuffer(vertices, 6);
        args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
        auto geom = dev->CreateGeometry("geom", std::move(args));

        const gfx::GeometryDrawCommand draw(*geom, *inst);

        dev->BeginFrame();
           dev->ClearColor(gfx::Color::DarkRed);
           dev->Draw(*program, draw, state);
        dev->EndFrame();

        const auto& bmp = dev->ReadColorBuffer(200, 200);
        TEST_REQUIRE(bmp == reference);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::White) == 20*20*4);
    }

    // instanced draw elements
    {
        constexpr const gfx::Vertex2D vertices[] = {
            { {-1,  1}, {0, 1} },
            { {-1, -1}, {0, 0} },
            { { 1, -1}, {1, 0} },
            { { 1,  1}, {1, 1} }
        };
        constexpr const gfx::Index16 indices[] = {
            0, 1, 2, // bottom triangle
            0, 2, 3  // top triangle
        };

        gfx::Geometry::CreateArgs args;
        args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
        args.buffer.SetVertexBuffer(vertices, 4);
        args.buffer.SetIndexBuffer(indices, 6);
        args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
        auto geom = dev->CreateGeometry("geom", std::move(args));

        const gfx::GeometryDrawCommand draw(*geom, *inst);

        dev->BeginFrame();
        dev->ClearColor(gfx::Color::DarkRed);
        dev->Draw(*program, draw, state);
        dev->EndFrame();

        const auto& bmp = dev->ReadColorBuffer(200, 200);
        TEST_REQUIRE(bmp == reference);
        TEST_REQUIRE(CountPixels(bmp, gfx::Color::White) == 20*20*4);
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
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();
    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red);

    const gfx::Vertex2D verts[] = {
      { {-1,  1}, {0, 1} },
      { {-1, -1}, {0, 0} },
      { { 1, -1}, {1, 0} },

      { {-1,  1}, {0, 1} },
      { { 1, -1}, {1, 0} },
      { { 1,  1}, {1, 1} }
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(verts, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles);
    auto geom = dev->CreateGeometry("geom", std::move(args));

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

    auto program = MakeTestProgram(*dev, vssrc, fssrc, "prog");

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

// Texture object assigned to an FBO gets incorrectly deleted
// leading to the FBO becoming incomplete.
void unit_test_fbo_texture_delete_bug()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();
    auto* fbo = dev->MakeFramebuffer("fbo");

    auto* render_target_texture = dev->MakeTexture("render_texture");
    render_target_texture->Upload(nullptr, 10, 10, gfx::Texture::Format::RGBA);
    render_target_texture->SetName("render_target_texture");

    auto* dummy_texture = dev->MakeTexture("dummy_texture");
    dummy_texture->Upload(nullptr, 20, 20, gfx::Texture::Format::RGBA);
    dummy_texture->SetName("dummy_texture");
    dummy_texture->SetFlag(gfx::Texture::Flags::GarbageCollect, true);

    gfx::Framebuffer::Config conf;
    conf.format = gfx::Framebuffer::Format::ColorRGBA8;
    conf.width  = 10;
    conf.height = 10;
    fbo->SetConfig(conf);
    fbo->SetColorTarget(render_target_texture);

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::White, fbo);
    dev->EndFrame();
    dev->CleanGarbage(1, gfx::Device::GCFlags::Textures);

    TEST_REQUIRE(dev->FindTexture("dummy_texture") == nullptr);
    TEST_REQUIRE(dev->FindTexture("render_texture") != nullptr);

    // change the FBO's color buffer target texture to nullptr
    // which means the FBO will allocate its own texture.
    dev->BeginFrame();
    fbo->SetColorTarget(nullptr);

    dev->ClearColor(gfx::Color::White, fbo);
    dev->EndFrame();
    dev->CleanGarbage(1, gfx::Device::GCFlags::Textures);

    TEST_REQUIRE(dev->FindTexture("render_texture") == nullptr);

}

// Test that changing the FBO's color buffer without changing the
// device FBO works as expected. I.e. the FBO is set on the device
// with one color target, then the color target is changed in the FBO
// but the FBO itself remains the same on the device.
void unit_test_fbo_change_color_target_bug()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();
    auto* fbo = dev->MakeFramebuffer("fbo");

    auto* red = dev->MakeTexture("red");
    red->Upload(nullptr, 10, 10, gfx::Texture::Format::RGBA);
    red->SetName("red");

    auto* green = dev->MakeTexture("green");
    green->Upload(nullptr, 10, 10, gfx::Texture::Format::RGBA);
    green->SetName("green");

    gfx::Framebuffer::Config conf;
    conf.format = gfx::Framebuffer::Format::ColorRGBA8;
    conf.width  = 10;
    conf.height = 10;
    fbo->SetConfig(conf);

    dev->BeginFrame();

    fbo->SetColorTarget(red);

    dev->ClearColor(gfx::Color::Red, fbo);

    fbo->SetColorTarget(green);
    dev->ClearColor(gfx::Color::Green, fbo);

    dev->EndFrame();

    // red texture should now be red and green texture should now be green.
    // setup a program to sample from textures
    auto program = MakeTestProgram(*dev,
R"(#version 100
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
  gl_Position = vec4(aPosition.xy, 0.0, 1.0);
  vTexCoord = aTexCoord;
})",
R"(#version 100
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D kTexture;
void main() {
  gl_FragColor = texture2D(kTexture, vTexCoord.xy);
})", "p1");

    auto quad = MakeQuad(*dev);

    gfx::Device::State state;
    state.bWriteColor  = true;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;
    state.viewport     = gfx::IRect(0, 0, 10, 10);

    dev->BeginFrame();

    program->SetTextureCount(1);
    program->SetTexture("kTexture", 0, *red);
    dev->Draw(*program, *quad, state, nullptr);
    dev->EndFrame();

    const auto& result_red = dev->ReadColorBuffer(10, 10);
    TEST_REQUIRE(result_red.Compare(gfx::Color::Red));

    dev->BeginFrame();

    program->SetTextureCount(1);
    program->SetTexture("kTexture", 0, *green);
    dev->Draw(*program, *quad, state, nullptr);
    dev->EndFrame();

    const auto& result_green = dev->ReadColorBuffer(10, 10, nullptr);
    TEST_REQUIRE(result_green.Compare(gfx::Color::Green));
}

void unit_test_fbo_change_size()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();
    auto* fbo = dev->MakeFramebuffer("fbo");

    gfx::Framebuffer::Config conf;
    conf.format = gfx::Framebuffer::Format::ColorRGBA8;
    conf.width  = 10;
    conf.height = 10;
    fbo->SetConfig(conf);

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Red, fbo);
    dev->EndFrame();

    {
        gfx::Texture* color = nullptr;
        fbo->Resolve(&color);
        TEST_REQUIRE(color->GetWidth() == 10);
        TEST_REQUIRE(color->GetHeight() == 10);
    }

    conf.width = 20;
    conf.height = 30;
    fbo->SetConfig(conf);

    dev->BeginFrame();
    dev->ClearColor(gfx::Color::Blue, fbo);
    dev->EndFrame();

    {
        gfx::Texture* color = nullptr;
        fbo->Resolve(&color);
        TEST_REQUIRE(color->GetWidth() == 20);
        TEST_REQUIRE(color->GetHeight() == 30);
    }

}

void unit_test_index_draw_cmd_bug()
{
    TEST_CASE(test::Type::Feature)

    auto dev = CreateDevice();

    constexpr const char* fragment_src =
R"(#version 100
precision mediump float;
void main() {
  gl_FragColor = vec4(1.0);
})";

    constexpr const char* vertex_src =
R"(#version 100
attribute vec2 aPosition;
void main() {
  gl_Position = vec4(aPosition.xy, 1.0, 1.0);
})";

    auto prog = MakeTestProgram(*dev, vertex_src, fragment_src);

    gfx::Device::State state;
    state.bWriteColor  = true;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;
    state.viewport     = gfx::IRect(0, 0, 10, 10);

    constexpr const gfx::Vertex2D vertices[] = {
        { {-1,  1}, {0, 1} },
        { {-1, -1}, {0, 0} },
        { { 1, -1}, {1, 0} },
        { { 1,  1}, {1, 1} }
    };
    constexpr const gfx::Index16 indices[] = {
        0, 1, 2, // bottom triangle
        0, 2, 3  // top triangle
    };
    gfx::Geometry::CreateArgs args;
    args.buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    args.buffer.SetVertexBuffer(vertices, 4);
    args.buffer.SetIndexBuffer(indices, 6);
    args.buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 3, 3);
    auto geom = dev->CreateGeometry("geom", std::move(args));

    dev->BeginFrame();
       dev->ClearColor(gfx::Color::Red);
       dev->Draw(*prog, *geom, state);
    dev->EndFrame();

    // this has alpha in it.
    const auto& bmp = dev->ReadColorBuffer(10, 10);

    // the draw should draw the second (top) triangle
    // which means the right top corner should be white
    // and the bottom left corner should be clear color (red)
    TEST_REQUIRE(bmp.GetPixel(9, 0) == gfx::Color::Red);
    TEST_REQUIRE(bmp.GetPixel(0, 9) == gfx::Color::White);
}


EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    for (int i=1; i<argc; ++i)
    {
        if (!std::strcmp(argv[i], "--es3"))
            TestContext::GL_ES_Version = 3;
    }
    test::Print(test::Color::Info, "Testing with GL ES%d\n", TestContext::GL_ES_Version);

    unit_test_shader();
    unit_test_texture();

    unit_test_render_fbo(gfx::Framebuffer::Format::ColorRGBA8, gfx::Framebuffer::MSAA::Disabled);
    unit_test_render_fbo(gfx::Framebuffer::Format::ColorRGBA8_Depth16, gfx::Framebuffer::MSAA::Disabled);
    unit_test_render_fbo(gfx::Framebuffer::Format::ColorRGBA8_Depth24_Stencil8, gfx::Framebuffer::MSAA::Disabled);

    unit_test_render_fbo(gfx::Framebuffer::Format::ColorRGBA8, gfx::Framebuffer::MSAA::Enabled);
    unit_test_render_fbo(gfx::Framebuffer::Format::ColorRGBA8_Depth16, gfx::Framebuffer::MSAA::Enabled);
    unit_test_render_fbo(gfx::Framebuffer::Format::ColorRGBA8_Depth24_Stencil8, gfx::Framebuffer::MSAA::Enabled);
    unit_test_render_color_only();
    unit_test_render_with_single_texture();
    unit_test_render_with_multiple_textures();
    unit_test_render_set_float_uniforms();
    unit_test_render_set_int_uniforms();
    unit_test_render_set_matrix2x2_uniform();
    unit_test_render_set_matrix3x3_uniform();
    unit_test_render_set_matrix4x4_uniform();
    unit_test_uniform_sampler_optimize_bug();
    unit_test_clean_textures();
    unit_test_vbo_allocation();
    unit_test_ibo_allocation();
    unit_test_max_texture_units_single_texture();
    unit_test_max_texture_units_many_textures();
    unit_test_algo_texture_copy();
    unit_test_algo_texture_flip();
    unit_test_algo_texture_read();

    if (TestContext::GL_ES_Version == 3)
    {
        unit_test_instanced_rendering();
    }

    // bugs
    unit_test_empty_draw_lost_uniform_bug();
    unit_test_repeated_uniform_bug();
    unit_test_fbo_texture_delete_bug();
    unit_test_fbo_change_color_target_bug();
    unit_test_fbo_change_size();
    unit_test_index_draw_cmd_bug();
    return 0;
}
) // TEST_MAIN
