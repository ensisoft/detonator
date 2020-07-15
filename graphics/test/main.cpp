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

#include "base/logging.h"
#include "graphics/device.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/text.h"
#include "graphics/drawing.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"
#include "graphics/resource.h"
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
    virtual void Start() {}
    virtual void End() {}
private:
};

class TransformTest : public GraphicsTest
{
public:
    TransformTest()
    {
        mRobot.reset(new BodyPart("Robot"));
        mRobot->SetPosition(100, 100);
        mRobot->SetScale(30.0f, 30.0f);

        mRobot->AddPart("Robot/Torso")
            .SetPosition(1.0f, 2.0f).SetSize(3, 5).SetColor(gfx::Color::DarkBlue);

        mRobot->AddPart("Robot/Head")
            .SetPosition(2.0f, 0.0f).SetSize(1.3f, 1.3f).SetColor(gfx::Color::Green);
        mRobot->AddPart("Robot/LeftArm")
            .SetPosition(1.0f, 2.0f).SetSize(1.0f, 2.0f).SetColor(gfx::Color::Gray)
            .SetVelocity(8).SetRotation(math::Pi * 0.8)
            .AddPart("Forearm")
            .SetPosition(0.0f, 2.0f).SetSize(1.0f, 2.0f).SetColor(gfx::Color::Yellow)
            .SetVelocity(10.0f);
        mRobot->AddPart("Robot/RightArm")
            .SetPosition(4.0f, 2.0f).SetSize(1.0f, 2.0f).SetColor(gfx::Color::Gray)
            .SetVelocity(-9).SetRotation(math::Pi * -0.9)
            .AddPart("Forearm")
            .SetPosition(0.0f, 2.0f).SetSize(1.0f, 2.0f).SetColor(gfx::Color::Yellow)
            .SetVelocity(-11.0f);
        mRobot->AddPart("Robot/LeftLeg")
            .SetPosition(1.0f, 7.0f).SetSize(1.0f, 2.0f).SetColor(gfx::Color::Red)
            .AddPart("Shin")
            .SetPosition(0.0f, 2.0f).SetSize(1.0f, 2.0f).SetColor(gfx::Color::Yellow);
        mRobot->AddPart("Robot/RightLeg")
            .SetPosition(3.0f, 7.0f).SetSize(1.0f, 2.0f).SetColor(gfx::Color::Red)
            .AddPart("Shin")
            .SetPosition(0.0f, 2.0f).SetSize(1.0f, 2.0f).SetColor(gfx::Color::Yellow);

    }

    virtual void Render(gfx::Painter& painter) override
    {
        const float velocity = 0.3;
        const float angle = mTime* velocity;

        gfx::Transform trans;
        mRobot->Render(painter, trans);

        gfx::Transform tr;
        tr.Translate(400, 400);

        // these two rectangles are in a parent-child relationship.
        // the child rectangle is transformed relative to the parent
        // which is transformed relative to the top level "view"
        // transform.
        // A call to push begins a new "scope" for a transformations
        // and subsequent operations combine into a single transformation
        // matrix.
        // the way you need to read these scopes is from the innermost
        // scope towards outermost.
        // individual operations happen in the order they're written.
        tr.Push();
            // in this scope first translate then rotate.
            // this transformation applies to rectangle A and B
            tr.Translate(-50.0f, -50.0f);
            tr.Rotate(math::Pi * 2.0f * angle);

            // begin transformation scope for rectangle A
            tr.Push();
                // scale only applies to this rectangle since the
                // transformation stack is popped below.
                // the scale could be removed and baked into rectangle.
                // having I for scale with Rectangle(100.0f, 100.0f)
                // yields the same result.
                tr.Scale(100.0f, 100.0f);
                painter.Draw(gfx::Rectangle(), tr, gfx::SolidColor(gfx::Color::Cyan));
            tr.Pop();

            // begin transformation scope for rectangle B.
            tr.Push();
                // first translate then rotate
                tr.Translate(30.0f, 30.0f);
                tr.Rotate(math::Pi * 2.0f * angle);
                tr.Push();
                    tr.Scale(20.0f, 20.0f);
                    painter.Draw(gfx::Rectangle(), tr, gfx::SolidColor(gfx::Color::Yellow));
                tr.Pop();
            tr.Pop();
        tr.Pop();
    }
    virtual void Update(float dt) override
    {
        mRobot->Update(dt);
        mTime += dt;
    }
private:
    class BodyPart
    {
    public:
        BodyPart(const std::string& name) : mName(name)
        {}
        void SetScale(float sx, float sy)
        {
            mSx = sx;
            mSy = sy;
        }
        BodyPart& SetPosition(float x, float y)
        {
            mX = x;
            mY = y;
            return *this;
        }
        BodyPart& SetSize(float width, float height)
        {
            mWidth  = width;
            mHeight = height;
            return *this;
        }
        BodyPart& SetColor(gfx::Color color)
        {
            mColor = color;
            return *this;
        }
        BodyPart& SetVelocity(float velo)
        {
            mVelocity = velo;
            return *this;
        }
        BodyPart& SetRotation(float value)
        {
            mRotation = value;
            return *this;
        }
        void Render(gfx::Painter& painter, gfx::Transform& trans) const
        {
            const float angle = std::sin(mTime * mVelocity);
            const float ROM = math::Pi * 0.3f;

            trans.Push();
            trans.Scale(mSx, mSy);
            trans.Rotate(mRotation + ROM * angle);
            trans.Translate(mX, mY);

            trans.Push();
                trans.Scale(mWidth, mHeight);
                painter.Draw(gfx::Rectangle(), trans, gfx::SolidColor(mColor));
            trans.Pop();

            for (const auto& bp : mBodyparts)
            {
                bp.Render(painter, trans);
            }
            trans.Pop();
        }
        void Update(float dt)
        {
            mTime += dt;
            for (auto& part : mBodyparts)
                part.Update(dt);
        }
        BodyPart& AddPart(const std::string& name)
        {
            mBodyparts.push_back(BodyPart(name));
            return mBodyparts.back();
        }

    private:
        const std::string mName;
        std::vector<BodyPart> mBodyparts;
        float mSx = 1.0f;
        float mSy = 1.0f;
        float mWidth = 0.0f;
        float mHeight = 0.0f;
        float mX = 0.0f;
        float mY = 0.0f;
        float mTime = 0.0f;
        float mVelocity = 0.0f;
        float mRotation = 0.0f;
        gfx::Color mColor;
    };
    std::unique_ptr<BodyPart> mRobot;

private:
    float mTime = 0.0f;
};

// render some different simple shapes.
class ShapesTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Material materials[4];
        materials[0] = gfx::SolidColor(gfx::Color::Red);
        materials[1] = gfx::TextureMap("textures/uv_test_512.png");
        materials[2] = gfx::SolidColor(gfx::Color::HotPink);
        materials[3] = gfx::TextureMap("textures/Checkerboard.png");

        // in order to validate the texture coordinates let's set
        // the filtering to nearest and clamp to edge on sampling
        materials[1].SetTextureMinFilter(gfx::Material::MinTextureFilter::Nearest);
        materials[1].SetTextureMagFilter(gfx::Material::MagTextureFilter::Nearest);
        materials[1].SetTextureWrapX(gfx::Material::TextureWrapping::Clamp);
        materials[1].SetTextureWrapY(gfx::Material::TextureWrapping::Clamp);

        {
            gfx::Transform transform;
            transform.Scale(100.0f, 100.0f);
            transform.Translate(10.0f, 10.0f);

            painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Wireframe), transform, materials[0]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Solid), transform, materials[1]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline), transform, materials[2]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Wireframe, 10.0f), transform, materials[1]);
            transform.Translate(120.0f, 0.0f);
            painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline, 10.0f), transform, materials[1]);

            transform.Translate(-450.0f, 150.0f);
            painter.Draw(gfx::Circle(gfx::Drawable::Style::Wireframe), transform, materials[0]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Circle(gfx::Drawable::Style::Solid), transform, materials[1]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Circle(gfx::Drawable::Style::Outline), transform, materials[2]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Circle(gfx::Drawable::Style::Wireframe), transform, materials[1]);
            transform.Translate(120.0f, 0.0f);
            painter.Draw(gfx::Circle(gfx::Drawable::Style::Outline, 10.0f), transform, materials[1]);

            transform.Translate(-450.0f, 150.0f);
            painter.Draw(gfx::Triangle(gfx::Drawable::Style::Wireframe), transform, materials[0]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Triangle(gfx::Drawable::Style::Solid), transform, materials[1]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Triangle(gfx::Drawable::Style::Outline), transform, materials[2]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Triangle(gfx::Drawable::Style::Wireframe, 10.0f), transform, materials[1]);
            transform.Translate(120.0f, 0.0f);
            painter.Draw(gfx::Triangle(gfx::Drawable::Style::Outline, 10.0f), transform, materials[1]);

            transform.Translate(-450.0f, 150.0f);
            painter.Draw(gfx::Arrow(gfx::Drawable::Style::Wireframe), transform, materials[0]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Arrow(gfx::Drawable::Style::Solid), transform, materials[1]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Arrow(gfx::Drawable::Style::Outline), transform, materials[2]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Arrow(gfx::Drawable::Style::Wireframe, 10.0f), transform, materials[1]);
            transform.Translate(120.0f, 0.0f);
            painter.Draw(gfx::Arrow(gfx::Drawable::Style::Outline, 10.0f), transform, materials[1]);

            transform.Translate(-450.0f, 150.0f);
            painter.Draw(gfx::Grid(5, 5), transform, materials[0]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Grid(5, 5), transform, materials[1]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Grid(5, 5), transform, materials[2]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::Grid(5, 5, 10.0f), transform, materials[1]);
        }
    }
    virtual void Update(float dts)
    {}
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
            p.boundary = ParticleEngine::BoundaryPolicy::Kill;
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
            p.rate_of_change_in_size_wrt_dist = -2.0f;
            p.rate_of_change_in_size_wrt_time = -2.0f;
            mFire.reset(new ParticleEngine(p));
        }

        {
            ParticleEngine::Params p;
            p.mode = ParticleEngine::SpawnPolicy::Continuous;
            p.boundary = ParticleEngine::BoundaryPolicy::Kill;
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
            p.rate_of_change_in_size_wrt_dist = -4.0f;
            p.rate_of_change_in_size_wrt_time = -8.0f;
            mSmoke.reset(new ParticleEngine(p));
        }

        {
            ParticleEngine::Params p;
            p.mode = ParticleEngine::SpawnPolicy::Continuous;
            p.boundary = ParticleEngine::BoundaryPolicy::Kill;
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
            p.rate_of_change_in_size_wrt_time = -2.0f;
            p.rate_of_change_in_size_wrt_dist = -2.0f;
            mBlood.reset(new ParticleEngine(p));
        }

        {
            ParticleEngine::Params p;
            p.mode = ParticleEngine::SpawnPolicy::Continuous;
            p.boundary = ParticleEngine::BoundaryPolicy::Kill;
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
                .SetBaseColor(gfx::Color4f(35, 35, 35, 20))
                .SetSurfaceType(gfx::Material::SurfaceType::Transparent));

        painter.Draw(*mFire, t,
            gfx::TextureMap("textures/BlackSmoke.png")
                .SetBaseColor(gfx::Color4f(0x71, 0x38, 0x0, 0xff))
                .SetSurfaceType(gfx::Material::SurfaceType::Emissive));

        t.Translate(500, 0);
        painter.Draw(*mBlood, t,
            gfx::TextureMap("textures/RoundParticle.png")
                .SetBaseColor(gfx::Color4f(234, 5, 3, 255))
                .SetSurfaceType(gfx::Material::SurfaceType::Transparent));

        t.Reset();
        t.Resize(2000, 200);
        t.MoveTo(-100, 100);
        painter.Draw(*mClouds, t,
            gfx::TextureMap("textures/WhiteCloud.png")
                .SetBaseColor(gfx::Color4f(224, 224, 224, 255))
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
                gfx::BitmapText(buff).SetBaseColor(gfx::Color::DarkGray));
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

    auto sampling = wdk::Config::Multisampling::None;

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
    std::vector<std::unique_ptr<GraphicsTest>> tests;
    tests.emplace_back(new TransformTest);
    tests.emplace_back(new RenderTextTest);
    tests.emplace_back(new RenderParticleTest);
    tests.emplace_back(new ShapesTest);
    tests[test_index]->Start();


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
    };

    // render in the window
    context->SetWindowSurface(window);

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
