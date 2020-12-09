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
#include "gamelib/classlib.h"
#include "gamelib/renderer.h"
#include "gamelib/scene.h"
#include "gamelib/main/interface.h"

namespace {
    gfx::FPoint ToPoint(const glm::vec2& vec)
    { return gfx::FPoint(vec.x, vec.y); }
} // namespace

class TestCase
{
public:
    virtual ~TestCase() = default;
    virtual void Render(gfx::Painter& painter) = 0;
    virtual void Update(float dts) {}
    virtual void Start(game::ClassLibrary* loader) {}
    virtual void End() {}
private:
};

class SceneTest : public TestCase
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Transform transform;
        mRenderer.Draw(*mScene, painter, transform);

        // visualize a node bounding box.
        const auto* node = mScene->FindNodeByInstanceName("box1");
        const auto box = mScene->GetBoundingBox(node);
        gfx::DrawLine(painter, ToPoint(box.GetTopLeft()),  ToPoint(box.GetTopRight()), gfx::Color::HotPink);
        gfx::DrawLine(painter, ToPoint(box.GetTopRight()), ToPoint(box.GetBotRight()), gfx::Color::HotPink);
        gfx::DrawLine(painter, ToPoint(box.GetBotRight()), ToPoint(box.GetBotLeft()), gfx::Color::HotPink);
        gfx::DrawLine(painter, ToPoint(box.GetBotLeft()),  ToPoint(box.GetTopLeft()), gfx::Color::HotPink);
    }
    virtual void Update(float dt)
    {
        if (!mScene)
            return;

        mTime += dt;
        const auto angular_velocity = 0.4;
        const auto angle = mTime * angular_velocity;
        const float x = std::cos(angle);
        const float y = std::sin(angle);

        auto* ground = mScene->FindNodeByClassName("ground");
        ground->SetTranslation(glm::vec2(400, 400) + glm::vec2(x*100.0f, y*100.0f));
        auto* box0 = mScene->FindNodeByInstanceName("box0");
        box0->SetRotation(-angle);
        box0->SetTranslation(glm::vec2(100.0, 100.0f) + glm::vec2(x*50.0f, y*50.0f));
        auto* box1 = mScene->FindNodeByInstanceName("box1");
        box1->SetTranslation(glm::vec2(-100.0, -100.0f) + glm::vec2(-x*50.0f, -y*50.0f));
        box1->SetRotation(angle);
    }
    virtual void Start(game::ClassLibrary* loader)
    {
        // this is the "static" scene class object that
        // can be used to create scene instances.
        game::SceneClass scene;

        // create ground.
        {
            game::SceneNodeClass node;
            node.SetName("ground");
            node.SetSize(glm::vec2(200.0f, 100.0f));
            node.SetTranslation(glm::vec2(400, 400));
            node.SetRotation(0.2);
            game::DrawableItemClass draw;
            draw.SetDrawableId("rectangle");
            draw.SetMaterialId("uv_test");
            node.SetDrawable(draw);

            auto* child = scene.AddNode(node);
            auto& root  = scene.GetRenderTree();
            root.AppendChild(child);
            child->SetName("ground");
        }

        // create box0
        game::SceneNodeClass box;
        box.SetName("box");
        box.SetSize(glm::vec2(40.0f, 40.0f));
        game::DrawableItemClass draw;
        draw.SetDrawableId("rectangle");
        draw.SetMaterialId("uv_test");
        box.SetDrawable(draw);

        // create an instance of the scene.
        mScene = game::CreateSceneInstance(scene);

        // modify the instance with some new objects of type box.
        {
            auto* child = mScene->AddNode(game::SceneNode(box));
            auto& root  = mScene->GetRenderTree();
            root.GetChildNode(0).AppendChild(child);
            child->SetName("box0");
            child->SetTranslation(glm::vec2(100, 100));
        }
        // add another box object.
        {
            auto* child = mScene->AddNode(game::SceneNode(box));
            auto& root  = mScene->GetRenderTree();
            root.GetChildNode(0).AppendChild(child);
            child->SetName("box1");
            child->SetTranslation(glm::vec2(-100, -100));
        }
        mRenderer.SetLoader(loader);
    }
private:
    std::unique_ptr<game::Scene> mScene;
    game::Renderer mRenderer;
    float mTime = 0.0f;
};


class AnimationTest : public TestCase
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Transform transform;
        transform.Translate(200, 200);
        mRenderer.Draw(*mAnimations[0], painter, transform);

        transform.Reset();
        transform.MoveTo(500,200);
        mRenderer.Draw(*mAnimations[1], painter, transform);

        transform.Reset();
        transform.MoveTo(800, 200);
        mRenderer.Draw(*mAnimations[2], painter, transform);
    }

    virtual void Start(game::ClassLibrary* loader) override
    {
        mRenderer.SetLoader(loader);

        game::AnimationNodeClass node;
        node.SetDrawable("rectangle");
        node.SetMaterial("uv_test");
        node.SetSize(glm::vec2(200.0f, 200.0f));
        node.SetName("Root");

        game::AnimationTransformActuatorClass move(node.GetClassId());
        move.SetEndPosition(glm::vec2(0.0f, 200.0f));
        move.SetEndSize(glm::vec2(200.0f, 200.0f));
        move.SetDuration(0.25);
        move.SetStartTime(0.0f);

        game::AnimationTransformActuatorClass resize(node.GetClassId());
        resize.SetEndPosition(glm::vec2(0.0f, 200.0f));
        resize.SetEndSize(glm::vec2(500.0f, 500.0f));
        resize.SetDuration(0.25);
        resize.SetStartTime(0.25);

        game::AnimationTransformActuatorClass rotate(node.GetClassId());
        rotate.SetEndPosition(glm::vec2(0.0f, 200.0f));
        rotate.SetEndSize(glm::vec2(500.0f, 500.0f));
        rotate.SetEndRotation(math::Pi);
        rotate.SetDuration(0.25);
        rotate.SetStartTime(0.5);

        game::AnimationTransformActuatorClass back(node.GetClassId());
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
    game::Renderer mRenderer;
};

class BoundingBoxTest : public TestCase
{
public:
    // TestCase
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Transform transform;
        transform.Translate(500, 300);
        //mAnimation->Draw(painter, transform);

        mRenderer.Draw(*mAnimation, painter, transform);

        for (size_t i=0; i<mAnimation->GetNumNodes(); ++i)
        {
            const auto& node = mAnimation->GetNode(i);
            auto rect = mAnimation->GetBoundingRect(&node);
            rect.Translate(500.0f, 300.0f);
            gfx::DrawRectOutline(painter, rect, gfx::SolidColor(gfx::Color::Green), 1.0f);
        }

        auto bounds = mAnimation->GetBoundingRect();
        bounds.Translate(500.0f, 300.0f);
        gfx::DrawRectOutline(painter, bounds, gfx::SolidColor(gfx::Color::DarkYellow), 2.0f);

    }
    virtual void Start(game::ClassLibrary* loader) override
    {
        mRenderer.SetLoader(loader);

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
    game::Renderer mRenderer;
    float mTime = 0.0f;
};

class MyApp : public game::App, public game::ClassLibrary, public wdk::WindowListener
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
        mTestList.emplace_back(new SceneTest);
        mTestList[mTestIndex]->Start(this);
    }

    virtual void Init(gfx::Device::Context* context,
        unsigned surface_width, unsigned surface_height)
    {
        mDevice  = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2, context);
        mPainter = gfx::Painter::Create(mDevice);
        mPainter->SetSurfaceSize(surface_width, surface_height);
    }

    virtual void Draw() override
    {
        mDevice->BeginFrame();
        mDevice->ClearColor(gfx::Color4f(0.2, 0.3, 0.4, 1.0f));
        mPainter->SetViewport(0, 0, 1027, 768);
        mPainter->SetTopLeftView(1027.0f, 768.0f);
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

    // ClassLibrary
    virtual std::shared_ptr<const gfx::MaterialClass> FindMaterialClass(const std::string& name) const override
    {
        if (name == "uv_test")
            return std::make_shared<gfx::MaterialClass>(gfx::TextureMap("textures/uv_test_512.png"));
        else if (name == "checkerboard")
            return std::make_shared<gfx::MaterialClass>(gfx::TextureMap("textures/Checkerboard.png"));
        else if (name == "color")
            return std::make_shared<gfx::MaterialClass>(gfx::SolidColor(gfx::Color::HotPink));
        else if (name == "object")
            return std::make_shared<gfx::MaterialClass>(gfx::SolidColor(gfx::Color::HotPink));
        else if (name == "ground")
            return std::make_shared<gfx::MaterialClass>(gfx::SolidColor(gfx::Color::DarkGreen));
        ASSERT("No such material class.");
        return nullptr;
    }
    virtual std::shared_ptr<const gfx::DrawableClass> FindDrawableClass(const std::string& name) const override
    {
        if (name == "circle")
            return std::make_shared<gfx::CircleClass>();
        else if (name == "rectangle")
            return std::make_shared<gfx::RectangleClass>();
        ASSERT(!"No such drawable class.");
        return nullptr;
    }
    virtual std::shared_ptr<const game::AnimationClass> FindAnimationClassById(const std::string& id) const override
    { return nullptr; }
    virtual std::shared_ptr<const game::AnimationClass> FindAnimationClassByName(const std::string& name) const override
    { return nullptr; }
    virtual void LoadFromFile(const std::string& dir, const std::string& file) override
    {}

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

