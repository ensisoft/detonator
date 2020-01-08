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

#include "base/logging.h"
#include "graphics/device.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/text.h"
#include "graphics/drawing.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"
#include "wdk/opengl/config.h"
#include "wdk/opengl/context.h"
#include "wdk/opengl/surface.h"
#include "wdk/window.h"
#include "wdk/events.h"
#include "wdk/system.h"

class GraphicsTest 
{
public:
    virtual ~GraphicsTest() = default;
    virtual void Render(gfx::Painter& painter) = 0;
    virtual void Update(float dts) {}
private:
};

class RenderParticleTest : public GraphicsTest
{
public:
    using ParticleEngine = gfx::KinematicsParticleEngine;

    RenderParticleTest()
    {
        {
            ParticleEngine::Params p;
            p.mode = ParticleEngine::SpawnPolicy::Continuous;
            p.num_particles = 300;
            p.min_lifetime = 1.0f;
            p.max_lifetime = 2.0f;
            p.max_xpos = 1.0f;
            p.max_ypos = 1.0f;
            p.init_rect_xpos = 0.0f;
            p.init_rect_ypos = 0.0f;
            p.init_rect_width = 1.0f;
            p.init_rect_height = 0.0f;
            p.direction_sector_start_angle = math::Pi * +.5;
            p.direction_sector_size = 40.0f/180.0f * math::Pi;
            p.min_velocity = 0.2;
            p.max_velocity = 0.4;
            p.min_lifetime = 1.0f;
            p.max_lifetime = 2.0f;
            p.min_point_size = 20;
            p.max_point_size = 60;
            mFire.reset(new ParticleEngine(p));
            mFire->SetBoundaryPolicy(ParticleEngine::BoundaryPolicy::Kill);
            mFire->SetGrowthWithRespectToDistance(-2.0f);
            mFire->SetGrowthWithRespectToTime(-2.0f);
        }

        {
            ParticleEngine::Params p;
            p.mode = ParticleEngine::SpawnPolicy::Continuous;
            p.num_particles = 300;
            p.min_lifetime = 1.0f;
            p.max_lifetime = 2.0f;
            p.max_xpos = 1.0f;
            p.max_ypos = 1.0f;
            p.init_rect_xpos = 0.1f;
            p.init_rect_ypos = 0.0f;
            p.init_rect_width = .8f;
            p.init_rect_height = 0.0f;
            p.direction_sector_start_angle = math::Pi * +.5;
            p.direction_sector_size =  40.0f/180.0f * math::Pi;
            p.min_velocity = 0.2;
            p.max_velocity = 0.25;
            p.min_lifetime = 10.0f;
            p.max_lifetime = 25.0f;
            p.min_point_size = 20;
            p.max_point_size = 60;
            mSmoke.reset(new ParticleEngine(p));
            mSmoke->SetBoundaryPolicy(ParticleEngine::BoundaryPolicy::Kill);
            mSmoke->SetGrowthWithRespectToDistance(-4.0f);
            mSmoke->SetGrowthWithRespectToTime(-8.0f);
        }        

        {
            ParticleEngine::Params p;
            p.mode = ParticleEngine::SpawnPolicy::Continuous;
            p.num_particles = 1000;
            p.min_lifetime = 1.0f;
            p.max_lifetime = 2.0f;
            p.max_xpos = 1.0f;
            p.max_ypos = 1.0f;
            p.init_rect_xpos = 0.45f;
            p.init_rect_ypos = 0.45f;
            p.init_rect_width = 0.1f;
            p.init_rect_height = 0.1f;
            p.min_velocity = 0.2;
            p.max_velocity = 0.4;
            p.min_lifetime = 1.0f;
            p.max_lifetime = 2.0f;
            p.min_point_size = 20;
            p.max_point_size = 40;
            mBlood.reset(new ParticleEngine(p));
            mBlood->SetBoundaryPolicy(ParticleEngine::BoundaryPolicy::Kill);
            mBlood->SetGrowthWithRespectToDistance(-2.0f);
            mBlood->SetGrowthWithRespectToTime(-2.0f);
        }

        {
            ParticleEngine::Params p;
            p.mode = ParticleEngine::SpawnPolicy::Continuous;
            p.num_particles = 0.45;
            p.min_lifetime = 20.0f;
            p.max_lifetime = 20.0f;
            p.max_xpos = 1.0f;
            p.max_ypos = 1.0f;
            p.init_rect_xpos = 0;
            p.init_rect_ypos = 0;
            p.init_rect_width = 0;
            p.init_rect_height = 1.0f;
            p.direction_sector_start_angle = 0;
            p.direction_sector_size = 0;
            p.min_velocity = 0.01;
            p.max_velocity = 0.02;
            p.min_lifetime = 20.0f;
            p.max_lifetime = 30.0f;
            p.min_point_size = 100;
            p.max_point_size = 150;
            mClouds.reset(new ParticleEngine(p));
            mClouds->SetBoundaryPolicy(ParticleEngine::BoundaryPolicy::Kill);
        }        

    }

    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Transform t;
        t.Resize(300, 300);
        t.Translate(-150, -150);
        t.Rotate(math::Pi);
        t.Translate(150 + 100, 150 + 300);

        painter.Draw(*mSmoke, t, 
            gfx::TextureMap("textures/BlackSmoke.png")
                .SetColorA(gfx::Color4f(35, 35, 35, 20))
                .SetSurfaceType(gfx::Material::SurfaceType::Transparent));                

        painter.Draw(*mFire, t, 
            gfx::TextureMap("textures/BlackSmoke.png")
                .SetColorA(gfx::Color4f(0x71, 0x38, 0x0, 0xff))
                .SetSurfaceType(gfx::Material::SurfaceType::Emissive));

        t.Translate(500, 0);
        painter.Draw(*mBlood, t, 
            gfx::TextureMap("textures/RoundParticle.png")
                .SetColorA(gfx::Color4f(234, 5, 3, 255))
                .SetSurfaceType(gfx::Material::SurfaceType::Transparent));                
        
        t.Reset();
        t.Resize(2000, 200);
        t.MoveTo(-100, 100);
        painter.Draw(*mClouds, t, 
            gfx::TextureMap("textures/WhiteCloud.png")
                .SetColorA(gfx::Color4f(224, 224, 224, 255))
                .SetSurfaceType(gfx::Material::SurfaceType::Transparent));

    }
    virtual void Update(float dts) override
    {
        mFire->Update(dts);
        mSmoke->Update(dts);
        mBlood->Update(dts);
        mClouds->Update(dts);
    }
private:
    std::unique_ptr<ParticleEngine> mFire;
    std::unique_ptr<ParticleEngine> mSmoke;
    std::unique_ptr<ParticleEngine> mBlood;
    std::unique_ptr<ParticleEngine> mClouds;
    float mTime = 0.0f;
};


// Render test text with some different fonts and text styling
// properties.
class RenderTextTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::DrawTextRect(painter, 
            "AtariFontFullVersion.ttf, 20px\n"
            "Hello World!\n"
            "1234567890\n"
            "!£$/[]}?,._-<>\n",
            "fonts/AtariFontFullVersion.ttf", 20,
            gfx::FRect(0, 0, 1024, 100), 
            gfx::Color::DarkGray);
            
        gfx::DrawTextRect(painter, 
            "Cousine-Regular.ttf, 20px\n"
            "Hello World!\n"
            "1234567890\n"
            "!£$/[]}?,._-<>\n",
            "fonts/Cousine-Regular.ttf", 20,            
            gfx::FRect(0, 100, 1024, 100),
            gfx::Color::DarkGray);

        gfx::DrawTextRect(painter, 
            "Cousine-Bold.ttf, 16px\n"
            "Hello World!\n"
            "1234567890\n"
            "!£$/[]}?,._-<>\n",
            "fonts/Cousine-Bold.ttf", 16,            
            gfx::FRect(0, 200, 1024, 100),
            gfx::Color::DarkGray);            

        gfx::DrawTextRect(painter, 
            "Cousine-Italic.ttf, 16px\n"
            "Hello World!\n"
            "1234567890\n"
            "!£$/[]}?,._-<>\n",
            "fonts/Cousine-Italic.ttf", 16,            
            gfx::FRect(0, 300, 1024, 100),
            gfx::Color::DarkGray);                        

        gfx::DrawTextRect(painter, 
            "Underlined text", 
            "fonts/AtariFontFullVersion.ttf", 18,
            gfx::FRect(0, 0, 300, 100), 
            gfx::Color::DarkGray, 
            gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter, 
            gfx::TextProp::Underline);

        gfx::DrawTextRect(painter, 
            "Blinking text", 
            "fonts/AtariFontFullVersion.ttf", 18,
            gfx::FRect(0, 100, 300, 100), 
            gfx::Color::DarkGray, 
            gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter, 
            gfx::TextProp::Blinking);            

        
        const auto circle = 2.0 * math::Pi;
        const auto angle  = circle * mTime * 0.3;        

        // use the more complicated API with a transform object        
        {
            gfx::Transform transform;
            transform.Resize(300, 200);
            transform.MoveTo(-150, -100);
            transform.Rotate(angle);
            transform.Translate(150, 300);

            gfx::TextBuffer buff(300, 200);
            buff.AddText("Hello World!", "fonts/AtariFontFullVersion.ttf", 20);            
            painter.Draw(gfx::Rectangle(), transform, 
                gfx::BitmapText(buff).SetColorA(gfx::Color::DarkGray));
        }

        // modulate text color based on time
        {
            const float r = (std::sin(angle + 0.1*circle) + 1.0) * 0.5;
            const float g = (std::cos(angle + 0.2*circle) + 1.0) * 0.5;
            const float b = (std::sin(angle + 0.3*circle) + 1.0) * 0.5;

            gfx::DrawTextRect(painter, 
                "Very colorful text",
                "fonts/AtariFontFullVersion.ttf", 20, 
                gfx::FRect(0, 500, 1024, 100),
                gfx::Color4f(r, g, b, 1.0f));
        }
    }
    virtual void Update(float dts) override
    {
        mTime += dts;
    }
private:
    float mTime = 0.0f;
};


int main(int argc, char* argv[])
{
    base::CursesLogger logger;
    base::SetGlobalLog(&logger);
    DEBUG("It's alive!");
    INFO("Copyright (c) 2010-2019 Sami Vaisanen");
    INFO("http://www.ensisoft.com");
    INFO("http://github.com/ensisoft/pinyin-invaders");

    for (int i=1; i<argc; ++i)
    {
        if (!std::strcmp(argv[i], "--debug-log"))
            base::EnableDebugLog(true);
    }

    // context integration glue code that puts together
    // wdk::Context and gfx::Device
    class WindowContext : public gfx::Device::Context 
    {
    public: 
        WindowContext() 
        {
            wdk::Config::Attributes attrs;
            attrs.red_size  = 8;
            attrs.green_size = 8;            
            attrs.blue_size = 8;
            attrs.alpha_size = 8;
            attrs.stencil_size = 8;
            attrs.surfaces.window = true;
            attrs.double_buffer = true;
            attrs.sampling = wdk::Config::Multisampling::MSAA4;
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

    auto context = std::make_shared<WindowContext>();

    std::size_t test_index = 0;
    std::vector<std::unique_ptr<GraphicsTest>> tests;
    tests.emplace_back(new RenderTextTest);
    tests.emplace_back(new RenderParticleTest);


    wdk::Window window;
    window.on_keydown = [&](const wdk::WindowEventKeydown& key) {
        if (key.symbol == wdk::Keysym::Escape)
            window.Destroy();
        else if (key.symbol == wdk::Keysym::ArrowLeft)
            test_index = test_index ? test_index - 1 : tests.size() - 1;
        else if (key.symbol == wdk::Keysym::ArrowRight)
            test_index = (test_index + 1) % tests.size();
    };
    

    window.Create("Demo", 1024, 768, context->GetVisualID());

    context->SetWindowSurface(window);

    auto device  = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2, context);
    auto painter = gfx::Painter::Create(device);

    using clock = std::chrono::high_resolution_clock;

    auto start = clock::now();

    while (window.DoesExist())
    {
        const auto end  = clock::now();
        const auto gone = end - start;
        const auto secs = std::chrono::duration_cast<std::chrono::milliseconds>(gone).count() / 1000.0;

        for (auto& test : tests) 
            test->Update(secs);

        device->BeginFrame();
        device->ClearColor(gfx::Color4f(0.2f, 0.3f, 0.4f, 1.0f));
        painter->SetViewport(0, 0, window.GetSurfaceWidth(), window.GetSurfaceHeight());

        // render the current test
        tests[test_index]->Render(*painter);
        
        device->EndFrame(true /*display*/);
        device->CleanGarbage(30);

        // process incoming (window) events
        wdk::native_event_t event;
        while (wdk::PeekEvent(event))
        {
            window.ProcessEvent(event);
        }

        start = end;
    }

    context->Dispose();
    return 0;
}
