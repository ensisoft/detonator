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
#include <cmath>

#define GAMESTUDIO_GAMELIB_IMPLEMENTATION

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
#include "gamelib/main/interface.h"

class TestCase
{
public:
    virtual ~TestCase() = default;
    virtual void Render(gfx::Painter& painter) = 0;
    virtual void Update(float dts) {}
    virtual void Start(game::GfxFactory* factory) {}
    virtual void End() {}
private:
};

class AnimationTest : public TestCase
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Transform transform;
        transform.Translate(200, 200);
        mAnimations[0]->Draw(painter, transform);

        transform.Reset();
        transform.MoveTo(500,200);
        mAnimations[1]->Draw(painter, transform);

        transform.Reset();
        transform.MoveTo(800, 200);
        mAnimations[2]->Draw(painter, transform);
    }

    virtual void Start(game::GfxFactory* factory) override
    {
        game::AnimationNodeClass node;
        node.SetDrawable("rectangle");
        node.SetMaterial("uv_test");
        node.SetSize(glm::vec2(200.0f, 200.0f));
        node.SetName("Root");

        game::AnimationTransformActuatorClass move(node.GetId());
        move.SetEndPosition(glm::vec2(0.0f, 200.0f));
        move.SetEndSize(glm::vec2(200.0f, 200.0f));
        move.SetDuration(0.25);
        move.SetStartTime(0.0f);

        game::AnimationTransformActuatorClass resize(node.GetId());
        resize.SetEndPosition(glm::vec2(0.0f, 200.0f));
        resize.SetEndSize(glm::vec2(500.0f, 500.0f));
        resize.SetDuration(0.25);
        resize.SetStartTime(0.25);

        game::AnimationTransformActuatorClass rotate(node.GetId());
        rotate.SetEndPosition(glm::vec2(0.0f, 200.0f));
        rotate.SetEndSize(glm::vec2(500.0f, 500.0f));
        rotate.SetEndRotation(math::Pi);
        rotate.SetDuration(0.25);
        rotate.SetStartTime(0.5);

        game::AnimationTransformActuatorClass back(node.GetId());
        back.SetEndPosition(glm::vec2(0.0f, 0.0f));
        back.SetEndSize(glm::vec2(200.0f, 200.0f));
        back.SetEndRotation(0.0f);
        back.SetDuration(0.25);
        back.SetStartTime(0.75);

        auto track = std::make_shared<game::AnimationTrackClass>();
        track->AddActuator(move);
        track->AddActuator(resize);
        track->AddActuator(rotate);
        track->AddActuator(back);
        track->SetDuration(10.0f); // 10 seconds
        track->SetLooping(true);
        track->SetName("testing");

        auto animation = std::make_shared<game::AnimationClass>();
        auto* nptr = animation->AddNode(std::move(node));
        auto& root = animation->GetRenderTree();
        root.AppendChild(nptr);
        animation->Prepare(*factory);

        // create 2 instances of the same animation
        mAnimations[0] = std::make_unique<game::Animation>(animation);
        mAnimations[1] = std::make_unique<game::Animation>(animation);
        mAnimations[2] = std::make_unique<game::Animation>(animation);
        mAnimations[0]->Play(game::AnimationTrack(track));
        mAnimations[1]->Play(game::AnimationTrack(track));
        mAnimations[2]->Play(game::AnimationTrack(track));
    }
    virtual void Update(float dt) override
    {
        if (mAnimations[0])
            mAnimations[0]->Update(dt);
        if (mAnimations[1])
            mAnimations[1]->Update(dt);
        if (mAnimations[2])
            mAnimations[2]->Update(dt);
    }

private:
    std::unique_ptr<game::Animation> mAnimations[3];
};

class BoundingBoxTest : public TestCase
{
public:
    // TestCase
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Transform transform;
        transform.Translate(500, 300);
        mAnimation->Draw(painter, transform);

        for (size_t i=0; i<mAnimation->GetNumNodes(); ++i)
        {
            const auto& node = mAnimation->GetNode(i);
            auto rect = mAnimation->GetBoundingBox(&node);
            rect.Translate(500.0f, 300.0f);
            gfx::DrawRectOutline(painter, rect, gfx::SolidColor(gfx::Color::Green), 1.0f);
        }

        auto bounds = mAnimation->GetBoundingBox();
        bounds.Translate(500.0f, 300.0f);
        gfx::DrawRectOutline(painter, bounds, gfx::SolidColor(gfx::Color::DarkYellow), 2.0f);

    }
    virtual void Start(game::GfxFactory* factory) override
    {
        // create new animation class type.
        game::AnimationClass animation;
        {
            game::AnimationNodeClass node;
            node.SetDrawable("rectangle");
            node.SetMaterial("uv_test");
            node.SetSize(glm::vec2(200.0f, 200.0f));
            node.SetName("Root");
            auto* nptr = animation.AddNode(std::move(node));
            auto& root = animation.GetRenderTree();
            root.AppendChild(nptr);
        }

        {
            game::AnimationNodeClass node;
            node.SetDrawable("rectangle");
            node.SetMaterial("uv_test");
            node.SetSize(glm::vec2(100.0f, 100.0f));
            node.SetTranslation(glm::vec2(150.0f, 150.0f));
            node.SetName("Child 0");
            auto* nptr = animation.AddNode(std::move(node));
            auto& root = animation.GetRenderTree();
            root.GetChildNode(0).AppendChild(nptr);
        }

        {
            game::AnimationNodeClass node;
            node.SetDrawable("rectangle");
            node.SetMaterial("uv_test");
            node.SetSize(glm::vec2(50.0f, 50.0f));
            node.SetTranslation(glm::vec2(75.0f, 75.0f));
            node.SetName("Child 1");
            auto* nptr = animation.AddNode(std::move(node));
            auto& root = animation.GetRenderTree();
            root.GetChildNode(0).GetChildNode(0).AppendChild(nptr);
        }

        // using this as the gfx loader.
        animation.Prepare(*factory);
        // create an animation instance
        mAnimation = std::make_unique<game::Animation>(animation);
    }
    virtual void Update(float dts) override
    {
        mAnimation->Update(dts);

        const auto velocity = 1.245;
        mTime += dts * velocity;

        //auto& tree = mAnimation->GetRenderTree();
        auto* node = mAnimation->FindNodeByName("Child 0"); //tree.GetChildNode(0).GetChildNode(0);
        node->SetScale(std::sin(mTime) + 1.0f);

        for (size_t i=0; i<mAnimation->GetNumNodes(); ++i)
        {
            auto& node = mAnimation->GetNode(i);
            node.SetRotation(mTime);
        }
    }
    virtual void End() override
    {}


private:
    std::unique_ptr<game::Animation> mAnimation;
    float mTime = 0.0f;
};


class MyApp : public game::App, public game::GfxFactory, public wdk::WindowListener
{
public:
    virtual bool ParseArgs(int argc, const char* argv[]) override
    {
        bool debug = false;
        for (int i = 1; i < argc; ++i)
        {
            if (!std::strcmp("--debug", argv[i]))
                debug = true;
        }
        base::EnableDebugLog(debug);
        return true;
    }
    virtual bool GetNextRequest(Request* out) override
    {
        return mRequests.GetNext(out);
    }

    // Application implementation
    virtual void Start()
    {
        mTestList.emplace_back(new BoundingBoxTest);
        mTestList.emplace_back(new AnimationTest);
        mTestList[mTestIndex]->Start(this);
    }

    virtual void Init(gfx::Device::Context* context,
        unsigned surface_width, unsigned surface_height)
    {
        mDevice  = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2, context);
        mPainter = gfx::Painter::Create(mDevice);
    }

    virtual void Draw() override
    {
        mDevice->BeginFrame();
        mDevice->ClearColor(gfx::Color4f(0.2, 0.3, 0.4, 1.0f));
        mPainter->SetViewport(0, 0, 1027, 768);
        mTestList[mTestIndex]->Render(*mPainter);
        mDevice->EndFrame(true);

        mDevice->CleanGarbage(120);
    }

    virtual void Tick(double time) override
    {
        DEBUG("Tick!");
    }

    virtual void Update(double time, double dt) override
    {
        // jump animations forward by the some timestep
        for (auto& test : mTestList)
            test->Update(dt);
    }
    virtual void Shutdown() override
    {
        mTestList[mTestIndex]->End();
    }
    virtual bool IsRunning() const override
    { return mRunning; }
    virtual wdk::WindowListener* GetWindowListener() override
    { return this; }

    // WindowListener
    virtual void OnWantClose(const wdk::WindowEventWantClose&) override
    {
        mRunning = false;
    }
    virtual void OnKeydown(const wdk::WindowEventKeydown& key) override
    {
        const auto current_test_index = mTestIndex;
        if (key.symbol == wdk::Keysym::Escape)
            mRunning = false;
        else if (key.symbol == wdk::Keysym::ArrowLeft)
            mTestIndex = mTestIndex ? mTestIndex - 1 : mTestList.size() - 1;
        else if (key.symbol == wdk::Keysym::ArrowRight)
            mTestIndex = (mTestIndex + 1) % mTestList.size();
        else if (key.symbol == wdk::Keysym::KeyS && key.modifiers.test(wdk::Keymod::Control))
            TakeScreenshot();
        else if (key.symbol == wdk::Keysym::Space)
            mRequests.ToggleFullscreen();

        if (mTestIndex != current_test_index)
        {
            mTestList[current_test_index]->End();
            mTestList[mTestIndex]->Start(this);
        }
    }
    virtual void UpdateStats(const Stats& stats) override
    {
        DEBUG("fps: %1, wall_time: %2, game_time: %3, frames: %4",
            stats.current_fps, stats.total_wall_time, stats.total_game_time, stats.num_frames_rendered);
    }

    // GfxFactory
    virtual std::shared_ptr<const gfx::MaterialClass> GetMaterialClass(const std::string& name) const override
    {
        if (name == "uv_test")
            return std::make_shared<gfx::MaterialClass>(gfx::TextureMap("textures/uv_test_512.png"));
        else if (name == "checkerboard")
            return std::make_shared<gfx::MaterialClass>(gfx::TextureMap("textures/Checkerboard.png"));
        ASSERT("No such material class.");
        return nullptr;
    }
    virtual std::shared_ptr<const gfx::DrawableClass> GetDrawableClass(const std::string& name) const override
    {
        if (name == "circle")
            return std::make_shared<gfx::CircleClass>();
        else if (name == "rectangle")
            return std::make_shared<gfx::RectangleClass>();
        ASSERT(!"No such drawable class.");
        return nullptr;
    }

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
    void TakeScreenshot()
    {
        const auto& rgba = mDevice->ReadColorBuffer(1024, 768);
        gfx::WritePNG(rgba, "screenshot.png");
        INFO("Wrote screenshot");
    }
private:
    std::size_t mTestIndex = 0;
    std::vector<std::unique_ptr<TestCase>> mTestList;
    std::unique_ptr<gfx::Painter> mPainter;
    std::shared_ptr<gfx::Device> mDevice;
    bool mRunning = true;
    game::AppRequestQueue mRequests;
};

extern "C" {

GAMESTUDIO_EXPORT game::App* MakeApp()
{
    DEBUG("test app");
    return new MyApp;
}

} // extern "C"

