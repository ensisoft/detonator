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

#include <memory>
#include <vector>
#include <cstring>
#include <chrono>
#include <cmath>
#include <iostream>

#if defined(LINUX_OS)
#  include <fenv.h>
#endif

#include "base/logging.h"
#include "base/format.h"
#include "graphics/image.h"
#include "graphics/device.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/text.h"
#include "graphics/drawing.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"
#include "graphics/resource.h"
#include "gamelib/animation.h"
#include "gamelib/gfxfactory.h"
#include "wdk/opengl/config.h"
#include "wdk/opengl/context.h"
#include "wdk/opengl/surface.h"
#include "wdk/window.h"
#include "wdk/events.h"
#include "wdk/system.h"

class TestCase
{
public:
    virtual ~TestCase() = default;
    virtual void Render(gfx::Painter& painter) = 0;
    virtual void Update(float dts) {}
    virtual void Start() {}
    virtual void End() {}
private:
};

class AnimationTest : public TestCase,
                      public game::GfxFactory
{
public:
    // TestCase
    virtual void Render(gfx::Painter& painter) override
    {

        gfx::Transform transform;
        transform.Translate(500, 300);
        mAnimation.Draw(painter, transform);

        for (size_t i=0; i<mAnimation.GetNumNodes(); ++i)
        {
            const auto& node = mAnimation.GetNode(i);
            auto rect = mAnimation.GetBoundingBox(&node);
            rect.Translate(500.0f, 300.0f);
            gfx::DrawRectOutline(painter, rect, gfx::SolidColor(gfx::Color::Green), 1.0f);
        }

        auto bounds = mAnimation.GetBoundingBox();
        bounds.Translate(500.0f, 300.0f);
        gfx::DrawRectOutline(painter, bounds, gfx::SolidColor(gfx::Color::DarkYellow), 2.0f);

    }
    virtual void Start() override
    {
        {
            game::AnimationNode node;
            node.SetDrawable("rectangle", nullptr);
            node.SetMaterial("uv_test", nullptr);
            node.SetSize(glm::vec2(200.0f, 200.0f));
            node.SetName("Root");
            auto* nptr = mAnimation.AddNode(std::move(node));
            auto& root = mAnimation.GetRenderTree();
            root.AppendChild(nptr);
        }

        {
            game::AnimationNode node;
            node.SetDrawable("rectangle", nullptr);
            node.SetMaterial("uv_test", nullptr);
            node.SetSize(glm::vec2(100.0f, 100.0f));
            node.SetTranslation(glm::vec2(150.0f, 150.0f));
            node.SetName("Child 0");
            auto* nptr = mAnimation.AddNode(std::move(node));
            auto& root = mAnimation.GetRenderTree();
            root.GetChildNode(0).AppendChild(nptr);
        }

        {
            game::AnimationNode node;
            node.SetDrawable("rectangle", nullptr);
            node.SetMaterial("uv_test", nullptr);
            node.SetSize(glm::vec2(50.0f, 50.0f));
            node.SetTranslation(glm::vec2(75.0f, 75.0f));
            node.SetName("Child 1");
            auto* nptr = mAnimation.AddNode(std::move(node));
            auto& root = mAnimation.GetRenderTree();
            root.GetChildNode(0).GetChildNode(0).AppendChild(nptr);
        }

        // using this as the gfx loader.
        mAnimation.Prepare(*this);
    }
    virtual void Update(float dts) override
    {
        mAnimation.Update(dts);

        const auto velocity = 1.245;
        mTime += dts * velocity;

        for (size_t i=0; i<mAnimation.GetNumNodes(); ++i)
        {
            auto& node = mAnimation.GetNode(i);
            node.SetRotation(mTime);
        }
    }
    virtual void End() override
    {}

    // GfxFactory
    virtual std::shared_ptr<gfx::Material> MakeMaterial(const std::string& name) const override
    {
        if (name == "uv_test")
            return std::make_shared<gfx::Material>(gfx::TextureMap("textures/uv_test_512.png"));
        else if (name == "checkerboard")
            return std::make_shared<gfx::Material>(gfx::TextureMap("textures/Checkerboard.png"));
        ASSERT(!"No such material.");
        return nullptr;
    }
    virtual std::shared_ptr<gfx::Drawable> MakeDrawable(const std::string& name) const override
    {
        if (name == "circle")
            return std::make_shared<gfx::Circle>();
        else if (name == "rectangle")
            return std::make_shared<gfx::Rectangle>();
        ASSERT(!"No such drawable.");
        return nullptr;
    }

private:
    game::Animation mAnimation;
    float mTime = 0.0f;
};

int main(int argc, char* argv[])
{

#if defined(LINUX_OS)
    // SIGFPE on floating point exception
    feenableexcept(FE_INVALID  |
                   FE_DIVBYZERO |
                   FE_OVERFLOW|
                   FE_UNDERFLOW
                   );
    DEBUG("Enabled floating point exceptions");
#endif

    base::OStreamLogger logger(std::cout);
    base::SetGlobalLog(&logger);
    DEBUG("It's alive!");
    INFO("Copyright (c) 2010-2020 Sami Vaisanen");
    INFO("http://www.ensisoft.com");
    INFO("http://github.com/ensisoft/gamestudio");

    auto sampling = wdk::Config::Multisampling::None;
    bool testing  = false;
    bool issue_gold = false;
    std::string casename;

    for (int i=1; i<argc; ++i)
    {
        if (!std::strcmp(argv[i], "--debug-log"))
            base::EnableDebugLog(true);
        else if (!std::strcmp(argv[i], "--msaa4"))
            sampling = wdk::Config::Multisampling::MSAA4;
        else if (!std::strcmp(argv[i], "--msaa8"))
            sampling = wdk::Config::Multisampling::MSAA8;
        else if (!std::strcmp(argv[i], "--msaa16"))
            sampling = wdk::Config::Multisampling::MSAA16;
        else if (!std::strcmp(argv[i], "--test"))
            testing = true;
        else if (!std::strcmp(argv[i], "--case"))
            casename = argv[++i];
        else if (!std::strcmp(argv[i], "--issue-gold"))
            issue_gold = true;
    }

    // context integration glue code that puts together
    // wdk::Context and gfx::Device
    class WindowContext : public gfx::Device::Context
    {
    public:
        WindowContext(wdk::Config::Multisampling sampling)
        {
            wdk::Config::Attributes attrs;
            attrs.red_size  = 8;
            attrs.green_size = 8;
            attrs.blue_size = 8;
            attrs.alpha_size = 8;
            attrs.stencil_size = 8;
            attrs.surfaces.window = true;
            attrs.double_buffer = true;
            attrs.sampling = sampling; //wdk::Config::Multisampling::MSAA4;
            attrs.srgb_buffer = true;

            mConfig   = std::make_unique<wdk::Config>(attrs);
            mContext  = std::make_unique<wdk::Context>(*mConfig, 2, 0,  false, //debug
                wdk::Context::Type::OpenGL_ES);
            mVisualID = mConfig->GetVisualID();
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
        wdk::uint_t GetVisualID() const
        { return mVisualID; }

        void SetWindowSurface(wdk::Window& window)
        {
            mSurface = std::make_unique<wdk::Surface>(*mConfig, window);
            mContext->MakeCurrent(mSurface.get());
            mConfig.release();
        }
        void Dispose()
        {
            mContext->MakeCurrent(nullptr);
            mSurface->Dispose();
            mSurface.reset();
            mConfig.reset();
        }
    private:
        std::unique_ptr<wdk::Context> mContext;
        std::unique_ptr<wdk::Surface> mSurface;
        std::unique_ptr<wdk::Config>  mConfig;
        wdk::uint_t mVisualID = 0;
    };

    auto context = std::make_shared<WindowContext>(sampling);
    auto device  = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2, context);
    auto painter = gfx::Painter::Create(device);

    std::size_t test_index = 0;
    std::vector<std::unique_ptr<TestCase>> tests;
    tests.emplace_back(new AnimationTest);

    bool stop_for_input = false;

    wdk::Window window;
    window.Create("Demo", 1024, 768, context->GetVisualID());
    window.on_keydown = [&](const wdk::WindowEventKeydown& key) {
        const auto current_test_index = test_index;
        if (key.symbol == wdk::Keysym::Escape)
            window.Destroy();
        else if (key.symbol == wdk::Keysym::ArrowLeft)
            test_index = test_index ? test_index - 1 : tests.size() - 1;
        else if (key.symbol == wdk::Keysym::ArrowRight)
            test_index = (test_index + 1) % tests.size();
        else if (key.symbol == wdk::Keysym::KeyS && key.modifiers.test(wdk::Keymod::Control))
        {
            static unsigned screenshot_number = 0;
            const auto& rgba = device->ReadColorBuffer(window.GetSurfaceWidth(),
                window.GetSurfaceHeight());

            gfx::Bitmap<gfx::RGB> tmp;

            const auto& name = "demo_" + std::to_string(screenshot_number) + ".png";
            gfx::WritePNG(gfx::Bitmap<gfx::RGB>(rgba), name);
            INFO("Wrote screen capture '%1'", name);
            screenshot_number++;
        }
        if (test_index != current_test_index)
        {
            tests[current_test_index]->End();
            tests[test_index]->Start();
        }
        stop_for_input = false;
    };

    // render in the window
    context->SetWindowSurface(window);

    tests[test_index]->Start();

    using clock = std::chrono::high_resolution_clock;

    auto stamp = clock::now();
    float frames  = 0;
    float seconds = 0.0f;

    while (window.DoesExist())
    {
        // measure how much time has elapsed since last iteration
        const auto now  = clock::now();
        const auto gone = now - stamp;
        // if sync to vblank is off then we it's possible that we might be
        // rendering too fast for milliseconds, let's use microsecond
        // precision for now. otherwise we'd need to accumulate time worth of
        // several iterations of the loop in order to have an actual time step
        // for updating the animations.
        const auto secs = std::chrono::duration_cast<std::chrono::microseconds>(gone).count() / (1000.0 * 1000.0);
        stamp = now;

        // jump animations forward by the *previous* timestep
        for (auto& test : tests)
            test->Update(secs);

        device->BeginFrame();
        device->ClearColor(gfx::Color4f(0.2f, 0.3f, 0.4f, 1.0f));
        painter->SetViewport(0, 0, window.GetSurfaceWidth(), window.GetSurfaceHeight());

        // render the current test
        tests[test_index]->Render(*painter);

        device->EndFrame(true /*display*/);
        device->CleanGarbage(120);

        // process incoming (window) events
        wdk::native_event_t event;
        while (wdk::PeekEvent(event))
        {
            window.ProcessEvent(event);
        }

        ++frames;
        seconds += secs;
        if (seconds > 1.0f)
        {
            const auto fps = frames / seconds;
            INFO("FPS: %1", fps);
            frames = 0.0f;
            seconds = 0.0f;
        }
    }

    context->Dispose();
    return 0;
}
