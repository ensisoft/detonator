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
#  include <boost/circular_buffer.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>
#include <cstring>
#include <cmath>

#define GAMESTUDIO_GAMELIB_IMPLEMENTATION

#include "engine/main/interface.h"
#include "base/logging.h"
#include "base/format.h"
#include "audio/graph.h"
#include "audio/loader.h"
#include "device/device.h"
#include "graphics/image.h"
#include "graphics/device.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/text.h"
#include "graphics/drawing.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"
#include "graphics/resource.h"
#include "graphics/utility.h"
#include "uikit/window.h"
#include "uikit/widget.h"
#include "uikit/state.h"
#include "game/entity.h"
#include "game/scene.h"
#include "engine/audio.h"
#include "engine/classlib.h"
#include "engine/renderer.h"
#include "engine/physics.h"
#include "engine/ui.h"

#include "test-shared.h"

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
    virtual void Start(engine::ClassLibrary* loader) {}
    virtual void End() {}
    virtual void Tick() {}
    virtual void OnKeydown(const wdk::WindowEventKeyDown& key) {}
    virtual void OnMousePress(const wdk::WindowEventMousePress& mickey) {}
    virtual void OnMouseRelease(const wdk::WindowEventMouseRelease& mickey) {}
    virtual void OnMouseMove(const wdk::WindowEventMouseMove& mickey) {}
    virtual void SetSurfaceSize(unsigned width, unsigned height)  {}
    virtual std::string GetName() const = 0;
private:
};

class AudioMusicTest : public TestCase,
                       public audio::Loader
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::FRect rect(20, 20, 700, 800);
        gfx::DrawTextRect(painter,
            base::FormatString(
               "Key 1 - music/SkyFire (Title Screen).ogg\n"
               "Key 2 - music/440Hz_44100Hz_16bit_05sec.mp3\n\n"
               "Effect Gain %1 (Press +/- to adjust)\n", mMusicGain),
            "assets/fonts/orbitron-medium.otf", 18, rect,
            gfx::Color::HotPink,
            gfx::TextAlign::AlignLeft | gfx::AlignTop);
    }
    virtual void Update(float dt) override
    {
        std::vector<engine::AudioEvent> events;
        mEngine->Update(&events);
        for (auto& event : events)
        {
            DEBUG("AudioEvent (%1) on track '%2'", event.type, event.track);
        }
    }
    virtual void Start(engine::ClassLibrary* loader) override
    {
        mEngine = std::make_unique<engine::AudioEngine>("TestApp");
        mEngine->SetLoader(this);
        mEngine->Start();
        mEngine->SetMusicGain(mMusicGain);
    }
    virtual void End() override
    { mEngine.reset(); }

    virtual void OnKeydown(const wdk::WindowEventKeyDown& key) override
    {
        std::string track;
        if (key.symbol == wdk::Keysym::Key1)
            track = "assets/music/SkyFire (Title Screen).ogg";
        else if (key.symbol == wdk::Keysym::Key2)
            track = "assets/music/440Hz_44100Hz_16bit_05sec.mp3";
        else if (key.symbol == wdk::Keysym::Plus)
            mMusicGain = math::clamp(0.0f, 1.0f, mMusicGain + 0.05f);
        else if (key.symbol == wdk::Keysym::Minus)
            mMusicGain = math::clamp(0.0f, 1.0f, mMusicGain - 0.05f);
        mEngine->SetMusicGain(mMusicGain);

        if (track.empty()) return;
        const auto& name = base::ToString(key.symbol);
        mEngine->PrepareMusicGraph(BuildMusicGraph(name, track));
        mEngine->SetMusicEffect(name, 2.0f*1000u, engine::AudioEngine::Effect::FadeIn);
        mEngine->ResumeMusic(name);
    }
    virtual std::string GetName() const override
    { return "AudioMusicTest"; }

    virtual audio::SourceStreamHandle OpenAudioStream(const std::string& uri,
        AudioIOStrategy strategy, bool enable_file_caching) const override
    { return audio::OpenFileStream(uri, strategy, enable_file_caching); }
private:
    engine::AudioEngine::GraphHandle BuildMusicGraph(const std::string& name, const std::string& audio_file)
    {
        auto graph = std::make_shared<audio::GraphClass>(name);

        audio::GraphClass::Element file;
        file.id   = base::RandomString(10);
        file.name = "file";
        file.type = "FileSource";
        file.args["file"]  = audio_file;
        file.args["type"]  = audio::SampleType::Float32;
        file.args["loops"] = 1u;
        graph->AddElement(file);

        audio::GraphClass::Element stereo;
        stereo.id = base::RandomString(10);
        stereo.name = "stereo";
        stereo.type = "StereoMaker";
        stereo.args["channel"] = audio::StereoMaker::Channel::Both;
        graph->AddElement(stereo);

        audio::GraphClass::Element resampler;
        resampler.id = base::RandomString(10);
        resampler.name = "resampler";
        resampler.type = "Resampler";
        resampler.args["sample_rate"] = 44100u;
        graph->AddElement(resampler);

        audio::GraphClass::Link file_to_stereo;
        file_to_stereo.id = base::RandomString(10);
        file_to_stereo.src_element = file.id;
        file_to_stereo.dst_element = stereo.id;
        file_to_stereo.src_port = "out";
        file_to_stereo.dst_port = "in";
        graph->AddLink(file_to_stereo);

        audio::GraphClass::Link stereo_to_resampler;
        stereo_to_resampler.id = base::RandomString(10);
        stereo_to_resampler.src_element = stereo.id;
        stereo_to_resampler.dst_element = resampler.id;
        stereo_to_resampler.src_port    = "out";
        stereo_to_resampler.dst_port    = "in";
        graph->AddLink(stereo_to_resampler);

        graph->SetGraphOutputElementId(resampler.id);
        graph->SetGraphOutputElementPort("out");
        return graph;
    }
private:
    std::unique_ptr<engine::AudioEngine> mEngine;
    float mMusicGain = 0.5f;
};

class AudioEffectTest : public TestCase,
                        public audio::Loader
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::FRect rect(20, 20, 700, 800);
        gfx::DrawTextRect(painter,
            base::FormatString(
                "Key 1 - sounds/sound 21.ogg\n"
                "Key 2 - sounds/qubodup-cfork-ccby3-jump.ogg\n"
                "Key 3 - sounds/completetask_0.mp3\n"
                "Key 4 - sounds/Laser_05.mp3\n\n"
                "Effect gain %1 (Press +/- to adjust)\n"
                "Effect delay %2 (Press Up/Down arrow to adjust)\n", mEffectGain, mDelay),
            "assets/fonts/orbitron-medium.otf", 18, rect,
            gfx::Color::HotPink,
            gfx::TextAlign::AlignLeft | gfx::AlignTop);
    }
    virtual void Update(float dt) override
    {
        mEngine->Update(nullptr);
    }
    virtual void Start(engine::ClassLibrary* loader) override
    {
        mEngine = std::make_unique<engine::AudioEngine>("TestApp");
        mEngine->SetLoader(this);
        mEngine->Start();
        mEngine->SetSoundEffectGain(mEffectGain);
        // when running WASM code we're single threaded and small (~20ms)
        // audio buffers are likely going to cause stutter 
        mEngine->SetBufferSize(40);
    }
    virtual void End() override
    { mEngine.reset(); }

    virtual void OnKeydown(const wdk::WindowEventKeyDown& key) override
    {
        const auto millisec = unsigned(mDelay * 1000u);
        if (key.symbol == wdk::Keysym::Key1)
            mEngine->PlaySoundEffect(BuildEffectGraph("21", "assets/sounds/sound 21.ogg"), millisec);
        else if (key.symbol == wdk::Keysym::Key2)
            mEngine->PlaySoundEffect(BuildEffectGraph("jump", "assets/sounds/qubodup-cfork-ccby3-jump.ogg"), millisec);
        else if (key.symbol == wdk::Keysym::Key3)
            mEngine->PlaySoundEffect(BuildEffectGraph("tada", "assets/sounds/completetask_0.mp3"), millisec);
        else if (key.symbol == wdk::Keysym::Key4)
            mEngine->PlaySoundEffect(BuildEffectGraph("laser", "assets/sounds/Laser_05.mp3"), millisec);
        else if (key.symbol == wdk::Keysym::Plus)
            mEffectGain = math::clamp(0.0f, 1.0f, mEffectGain + 0.05f);
        else if (key.symbol == wdk::Keysym::Minus)
            mEffectGain = math::clamp(0.0f, 1.0f, mEffectGain - 0.05f);
        else if (key.symbol == wdk::Keysym::ArrowUp)
            mDelay = math::clamp(0.0f, 10.0f, mDelay + 0.5f);
        else if (key.symbol == wdk::Keysym::ArrowDown)
            mDelay = math::clamp(0.0f, 10.0f, mDelay - 0.5f);
        mEngine->SetSoundEffectGain(mEffectGain);
    }
    virtual std::string GetName() const override
    { return "AudioEffectTest"; }
    virtual audio::SourceStreamHandle OpenAudioStream(const std::string& uri,
        AudioIOStrategy strategy, bool enable_file_caching) const override
    { return audio::OpenFileStream(uri); }
private:
    engine::AudioEngine::GraphHandle BuildEffectGraph(const std::string& name, const std::string& audio_file)
    {
        auto graph = std::make_shared<audio::GraphClass>(name);

        audio::GraphClass::Element file;
        file.id   = base::RandomString(10);
        file.name = "file";
        file.type = "FileSource";
        file.args["file"]  = audio_file;
        file.args["type"]  = audio::SampleType::Float32;
        file.args["loops"] = 1u;
        graph->AddElement(file);

        audio::GraphClass::Element stereo;
        stereo.id = base::RandomString(10);
        stereo.name = "stereo";
        stereo.type = "StereoMaker";
        stereo.args["channel"] = audio::StereoMaker::Channel::Both;
        graph->AddElement(stereo);

        audio::GraphClass::Element resampler;
        resampler.id = base::RandomString(10);
        resampler.name = "resampler";
        resampler.type = "Resampler";
        resampler.args["sample_rate"] = 44100u;
        graph->AddElement(resampler);

        audio::GraphClass::Link file_to_stereo;
        file_to_stereo.id = base::RandomString(10);
        file_to_stereo.src_element = file.id;
        file_to_stereo.dst_element = stereo.id;
        file_to_stereo.src_port = "out";
        file_to_stereo.dst_port = "in";
        graph->AddLink(file_to_stereo);

        audio::GraphClass::Link stereo_to_resampler;
        stereo_to_resampler.id = base::RandomString(10);
        stereo_to_resampler.src_element = stereo.id;
        stereo_to_resampler.dst_element = resampler.id;
        stereo_to_resampler.src_port    = "out";
        stereo_to_resampler.dst_port    = "in";
        graph->AddLink(stereo_to_resampler);

        graph->SetGraphOutputElementId(resampler.id);
        graph->SetGraphOutputElementPort("out");
        return graph;
    }
private:
    std::unique_ptr<engine::Loader> mLoader;
    std::unique_ptr<engine::AudioEngine> mEngine;
    float mEffectGain = 0.5f;
    float mDelay      = 0.0f;
};

class PhysicsTest : public TestCase
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Transform transform;
        mRenderer.Draw(*mScene, painter, transform);
        mPhysics.DebugDrawObjects(painter, transform);
    }
    virtual void Update(float dt) override
    {
        if (mPhysics.HaveWorld())
        {
            mPhysics.Step();
            mPhysics.UpdateScene(*mScene);
        }
    }
    virtual void Start(engine::ClassLibrary* loader) override
    {
        auto klass = std::make_shared<game::SceneClass>();
        // create ground.
        {
            game::SceneNodeClass ground;
            ground.SetTranslation(glm::vec2(400.0f, 500.0f));
            ground.SetRotation(0.1f);
            ground.SetEntity(loader->FindEntityClassByName("ground"));
            klass->LinkChild(nullptr, klass->AddNode(std::move(ground)));
        }
        {
            game::SceneNodeClass ground;
            ground.SetTranslation(glm::vec2(400.0f, 500.0f));
            ground.SetRotation(-0.4f);
            ground.SetTranslation(glm::vec2(500.0f, 650.0f));
            ground.SetEntity(loader->FindEntityClassByName("ground"));
            klass->LinkChild(nullptr, klass->AddNode(std::move(ground)));
        }

        // create some boxes.
        {
            for (int i=0; i<3; ++i)
            {
                game::SceneNodeClass node;
                const auto x = 400.0f + (i & 1) * 25.0f;
                const auto y = 300 + i * 50.0f;
                node.SetTranslation(glm::vec2(x, y));
                node.SetEntity(loader->FindEntityClassByName("box"));
                klass->LinkChild(nullptr, klass->AddNode(std::move(node)));
            }
        }

        // create a few circle shapes.
        {
            for (int i=0; i<3; ++i)
            {
                game::SceneNodeClass node;
                const auto x = 300.0f + (i & 1) * 25.0f;
                const auto y = 300 + i * 50.0f;
                node.SetTranslation(glm::vec2(x, y));
                node.SetEntity(loader->FindEntityClassByName("circle"));
                klass->LinkChild(nullptr, klass->AddNode(std::move(node)));
            }
        }

        mScene = game::CreateSceneInstance(klass);
        mRenderer.SetClassLibrary(loader);
        mPhysics.SetClassLibrary(loader);
        mPhysics.SetGravity(glm::vec2(0.0f, 100.0f));
        mPhysics.SetScale(glm::vec2(10.0f, 10.0f));
        mPhysics.DeleteAll();
        mPhysics.CreateWorld(*mScene);
    }
    virtual std::string GetName() const override
    { return "PhysicsTest"; }
private:
    std::unique_ptr<game::Scene>  mScene;
    engine::Renderer mRenderer;
    engine::PhysicsEngine mPhysics;
};


class ViewportTest : public TestCase
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        // visualize the logical viewport.
        painter.SetViewport(0, 0, mSurfaceWidth, mSurfaceHeight);

        // map the logical viewport to some area in the rendering surface
        // so that the rendering area (the device viewport) has the same
        // aspect ratio as the logical viewport.
        const float width       = mViewport.GetWidth();
        const float height      = mViewport.GetHeight();
        const float surf_width  = (float)mSurfaceWidth;
        const float surf_height = (float)mSurfaceHeight;
        const float scale = std::min(surf_width / width, surf_height / height);
        const float device_viewport_width = width * scale;
        const float device_viewport_height = height * scale;
        const float device_viewport_x = (surf_width - device_viewport_width) / 2;
        const float device_viewport_y = (surf_height - device_viewport_height) / 2;
        gfx::DrawRectOutline(painter, gfx::FRect(device_viewport_x, device_viewport_y,
                                                 device_viewport_width, device_viewport_height),
                             gfx::Color::Green, 1.0f);
        // set the actual viewport for proper clipping.
        painter.SetViewport(device_viewport_x, device_viewport_y, device_viewport_width, device_viewport_height);
        // set the logical game view
        painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(mViewport));

        //gfx::FRect test(0.0f, 0.0f, 100.0f, 100.0f);
        //gfx::FillRect(painter, test, gfx::Color::Yellow);

        gfx::Transform transform;
        mRenderer.Draw(*mScene, painter, transform);
    }
    virtual void Update(float dt) override
    {
        if (mScene)
            mScene->Update(dt);
    }
    virtual void Start(engine::ClassLibrary* loader) override
    {
        auto klass = std::make_shared<game::SceneClass>();
        {
            game::SceneNodeClass robot;
            robot.SetEntityId("robot");
            robot.SetTranslation(glm::vec2(100.0f, 100.0f));
            robot.SetScale(glm::vec2(0.8, 0.8));
            robot.SetName("robot 1");
            robot.SetEntity(loader->FindEntityClassByName("robot"));
            klass->LinkChild(nullptr, klass->AddNode(robot));
        }

        {
            game::SceneNodeClass robot;
            robot.SetEntityId("robot");
            robot.SetTranslation(glm::vec2(300.0f, 100.0f));
            robot.SetScale(glm::vec2(1.0, 1.0));
            robot.SetName("robot 2");
            robot.SetEntity(loader->FindEntityClassByName("robot"));
            klass->LinkChild(nullptr, klass->AddNode(robot));
        }

        // landmark box at 0,0
        {
            game::SceneNodeClass box;
            box.SetEntityId("unit_box");
            box.SetTranslation(glm::vec2(50.0f, 50.0f));
            box.SetScale(glm::vec2(100.0f, 100.0f));
            box.SetName("unit_box");
            box.SetEntity(loader->FindEntityClassByName("unit_box"));
            klass->LinkChild(nullptr, klass->AddNode(box));
        }

        mScene = game::CreateSceneInstance(klass);
        mScene->FindEntityByInstanceName("robot 1")->PlayAnimationByName("idle");
        mScene->FindEntityByInstanceName("robot 2")->PlayAnimationByName("idle");
        mRenderer.SetClassLibrary(loader);
        mViewport = gfx::FRect(0.0f, 0.0f, 200.0f, 200.0f);
    }
    virtual void SetSurfaceSize(unsigned width, unsigned height) override
    {
        mSurfaceWidth  = width;
        mSurfaceHeight = height;
    }
    virtual void OnKeydown(const wdk::WindowEventKeyDown& key) override
    {
        if (key.symbol == wdk::Keysym::Key1)
            mViewport.Grow(0.0f, -10.0f);
        else if (key.symbol == wdk::Keysym::Key2)
            mViewport.Grow(0.0f, 10.0f);
        else if (key.symbol == wdk::Keysym::Key3)
            mViewport.Grow(-10.0f, 0.0f);
        else if (key.symbol == wdk::Keysym::Key4)
            mViewport.Grow(10.0f, 0.0f);
        else if (key.symbol == wdk::Keysym::KeyA)
            mViewport.Translate(-10.0f, 0.0f);
        else if (key.symbol == wdk::Keysym::KeyD)
            mViewport.Translate(10.0f, 0.0f);
        else if (key.symbol == wdk::Keysym::KeyW)
            mViewport.Translate(0.0f, -10.0f);
        else if (key.symbol == wdk::Keysym::KeyS)
            mViewport.Translate(0.0f, 10.0f);

        DEBUG("viewport: %1", mViewport);
    }
    virtual std::string GetName() const override
    { return "ViewportTest"; }
private:
    std::unique_ptr<game::Scene> mScene;
    engine::Renderer mRenderer;
    gfx::FRect  mViewport;
    unsigned mSurfaceWidth = 0;
    unsigned mSurfaceHeight = 0;
};

class SceneTest : public TestCase
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Transform transform;
        transform.Translate(300.0f, 400.0f);
        mRenderer.Draw(*mScene, painter, transform);

        for (size_t i=0; i<mScene->GetNumEntities(); ++i)
        {
            const auto& entity = mScene->GetEntity(i);
            if (mDrawEntityBoundingRects)
            {
                auto rect = mScene->FindEntityBoundingRect(&entity);
                rect.Translate(300.0f, 400.0f);
                gfx::DrawRectOutline(painter, rect, gfx::CreateMaterialFromColor(gfx::Color::Yellow), 1.0f);
            }

            for (size_t i=0; i<entity.GetNumNodes(); ++i)
            {
                const auto& node = entity.GetNode(i);
                if (mDrawEntityNodeBoundingRects)
                {
                    auto rect = mScene->FindEntityNodeBoundingRect(&entity, &node);
                    rect.Translate(300.0f, 400.0f);
                    gfx::DrawRectOutline(painter, rect, gfx::CreateMaterialFromColor(gfx::Color::Yellow), 1.0f);
                }
                if (mDrawEntityNodeBoundingBoxes)
                {
                    auto box = mScene->FindEntityNodeBoundingBox(&entity, &node);
                    box.Transform(transform.GetAsMatrix());
                    gfx::DrawLine(painter, ToPoint(box.GetTopLeft()), ToPoint(box.GetTopRight()), gfx::Color::HotPink);
                    gfx::DrawLine(painter, ToPoint(box.GetTopRight()), ToPoint(box.GetBotRight()), gfx::Color::HotPink);
                    gfx::DrawLine(painter, ToPoint(box.GetBotRight()), ToPoint(box.GetBotLeft()), gfx::Color::HotPink);
                    gfx::DrawLine(painter, ToPoint(box.GetBotLeft()), ToPoint(box.GetTopLeft()), gfx::Color::HotPink);
                }
            }
        }
    }
    virtual void Update(float dt) override
    {
        if (mScene)
        {
            mScene->Update(dt);
        }
    }
    virtual void Start(engine::ClassLibrary* loader) override
    {
        auto klass = std::make_shared<game::SceneClass>();

        {
            game::SceneNodeClass robot;
            robot.SetEntityId("robot");
            robot.SetTranslation(glm::vec2(100.0f, 100.0f));
            robot.SetScale(glm::vec2(0.8, 0.8));
            robot.SetName("robot 1");
            robot.SetEntity(loader->FindEntityClassByName("robot"));
            klass->LinkChild(nullptr, klass->AddNode(robot));
        }

        {
            game::SceneNodeClass robot;
            robot.SetEntityId("robot");
            robot.SetTranslation(glm::vec2(300.0f, 100.0f));
            robot.SetScale(glm::vec2(1.0, 1.0));
            robot.SetName("robot 2");
            robot.SetEntity(loader->FindEntityClassByName("robot"));
            klass->LinkChild(nullptr, klass->AddNode(robot));
        }

        mScene = game::CreateSceneInstance(klass);
        mScene->FindEntityByInstanceName("robot 1")->PlayAnimationByName("idle");
        mScene->FindEntityByInstanceName("robot 2")->PlayAnimationByName("idle");
        mRenderer.SetClassLibrary(loader);
    }
    virtual void OnKeydown(const wdk::WindowEventKeyDown& key) override
    {
        if (key.symbol == wdk::Keysym::Key1)
            mDrawEntityBoundingRects = !mDrawEntityBoundingRects;
        else if (key.symbol == wdk::Keysym::Key2)
            mDrawEntityNodeBoundingRects = !mDrawEntityNodeBoundingRects;
        else if (key.symbol == wdk::Keysym::Key3)
            mDrawEntityNodeBoundingBoxes = !mDrawEntityNodeBoundingBoxes;
    }

    virtual std::string GetName() const override
    { return "SceneTest"; }
private:
    std::unique_ptr<game::Scene> mScene;
    engine::Renderer mRenderer;
    bool mDrawEntityBoundingRects = true;
    bool mDrawEntityNodeBoundingRects = true;
    bool mDrawEntityNodeBoundingBoxes = true;

};

class EntityTest : public TestCase
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Transform transform;
        transform.MoveTo(400, 400);
        mRenderer.Draw(*mEntity, painter, transform);

        for (size_t i=0; i<mEntity->GetNumNodes(); ++i)
        {
            const auto& node = mEntity->GetNode(i);
            if (mDrawBoundingBoxes)
            {
                auto box = mEntity->FindNodeBoundingBox(&node);
                box.Transform(transform.GetAsMatrix());
                gfx::DrawLine(painter, ToPoint(box.GetTopLeft()), ToPoint(box.GetTopRight()), gfx::Color::HotPink);
                gfx::DrawLine(painter, ToPoint(box.GetTopRight()), ToPoint(box.GetBotRight()), gfx::Color::HotPink);
                gfx::DrawLine(painter, ToPoint(box.GetBotRight()), ToPoint(box.GetBotLeft()), gfx::Color::HotPink);
                gfx::DrawLine(painter, ToPoint(box.GetBotLeft()), ToPoint(box.GetTopLeft()), gfx::Color::HotPink);
            }
            if (mDrawBoundingRects)
            {
                auto rect = mEntity->FindNodeBoundingRect(&node);
                rect.Translate(400, 400);
                gfx::DrawRectOutline(painter, rect, gfx::CreateMaterialFromColor(gfx::Color::Yellow), 1.0f);
            }
        }
    }
    virtual void Update(float dt) override
    {
        if (!mEntity)
            return;

        mTime += dt;
        mEntity->Update(dt);
    }
    virtual void Start(engine::ClassLibrary* loader) override
    {
        auto klass = loader->FindEntityClassByName("robot");
        mEntity = game::CreateEntityInstance(klass);
        mEntity->PlayAnimationByName("idle");
        mRenderer.SetClassLibrary(loader);
    }
    virtual void OnKeydown(const wdk::WindowEventKeyDown& key) override
    {
        if (key.symbol == wdk::Keysym::Key1)
            mDrawBoundingBoxes = !mDrawBoundingBoxes;
        else if (key.symbol == wdk::Keysym::Key2)
            mDrawBoundingRects = !mDrawBoundingRects;
    }
    virtual std::string GetName() const override
    { return "EntityTest"; }
private:
    std::unique_ptr<game::Entity> mEntity;
    engine::Renderer mRenderer;
    float mTime = 0.0f;
    bool mDrawBoundingBoxes = true;
    bool mDrawBoundingRects = true;
};


class UITest : public TestCase
{
public:
    UITest() : mMessageQueue(20)
    {}

    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Transform view;
        view.Translate(mOffsetX, mOffsetY);
        painter.SetViewMatrix(view.GetAsMatrix());

        mPainter.SetPainter(&painter);
        mWindow.Paint(mState, mPainter, mTime);

        painter.ResetViewMatrix();

        gfx::FRect rect(10, 30, 500, 20);
        for (const auto& print : mMessageQueue)
        {
            gfx::FillRect(painter, rect, gfx::Color4f(gfx::Color::Black, 0.4f));
            gfx::DrawTextRect(painter, print, "assets/fonts/orbitron-medium.otf", 14, rect,
                gfx::Color::HotPink, gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
            rect.Translate(0, 20);
        }
    }
    virtual void Update(float dt) override
    {
        mPainter.Update(mTime, dt);
        const auto& actions = mWindow.PollAction(mState, mTime, dt);
        for (const auto& action : actions)
        {
            mMessageQueue.push_back(base::FormatString("Event: %1, widget: '%2'", action.type, action.name));
        }
        mTime += dt;
    }
    virtual void Start(engine::ClassLibrary* loader) override
    {
        mWindow.ClearWidgets();
        mStyle.SetClassLibrary(loader);
        mPainter.SetStyle(&mStyle);

        // generic properties that apply across all widget types.
        mStyle.SetMaterial("widget/background", engine::detail::UIColor(gfx::Color::Black));
        mStyle.SetMaterial("widget/border", engine::detail::UIColor(gfx::Color::LightGray));
        mStyle.SetProperty("widget/shape", "RoundRect");
        // static text properties
        mStyle.SetProperty("widget/text-font", "assets/fonts/orbitron-medium.otf");
        mStyle.SetProperty("widget/text-size", 16);
        mStyle.SetProperty("widget/text-color", gfx::Color::White);
        // button properties.
        mStyle.SetMaterial("widget/button-background", engine::detail::UIColor(gfx::Color::Black));
        mStyle.SetMaterial("widget/button-border", engine::detail::UIColor(gfx::Color::Gray));
        mStyle.SetMaterial("widget/button-icon", engine::detail::UIColor(gfx::Color::Gold));
        mStyle.SetMaterial("widget/pressed/button-background", engine::detail::UIColor(gfx::Color::Gray));
        mStyle.SetMaterial("widget/pressed/button-border", engine::detail::UIColor(gfx::Color::Silver));
        // editable text properties.
        mStyle.SetProperty("widget/edit-text-font", "assets/fonts/orbitron-medium.otf");
        mStyle.SetProperty("widget/edit-text-size", 16);
        mStyle.SetProperty("widget/edit-text-color", gfx::Color::Black);
        // text edit box properties.
        mStyle.SetMaterial("widget/text-edit-background", engine::detail::UIColor(gfx::Color::White));
        // slider properties.
        mStyle.SetMaterial("slider/slider-background", engine::detail::UIColor(gfx::Color::White));
        mStyle.SetMaterial("slider/slider-knob", engine::detail::UIColor(gfx::Color::Black));
        mStyle.SetMaterial("slider/pressed/slider-knob", engine::detail::UIColor(gfx::Color::Gray));
        // progress bar properties
        mStyle.SetMaterial("progress-bar/progress-bar-fill", engine::detail::UIColor(gfx::Color::DarkGray));

        // some assorted properties
        mStyle.SetProperty("label/mouse-over/text-color", gfx::Color::DarkGreen);
        mStyle.SetMaterial("checkbox/background", engine::detail::UINullMaterial());
        mStyle.SetMaterial("checkbox/border", engine::detail::UINullMaterial());
        mStyle.SetMaterial("checkbox/check-border", engine::detail::UIColor(gfx::Color::White));
        mStyle.SetMaterial("checkbox/check-mark", engine::detail::UIColor(gfx::Color::Silver));
        mStyle.SetMaterial("label/background", engine::detail::UINullMaterial());
        mStyle.SetMaterial("label/border", engine::detail::UINullMaterial());
        mStyle.SetMaterial("form/background", engine::detail::UIColor(gfx::Color::DarkGray));

        uik::Widget* form =  nullptr;
        {
            uik::Form form;
            form.SetName("form");
            form.SetSize(500.0f, 500.0f);
            mWindow.AddWidget(form);
            mWindow.LinkChild(nullptr, mWindow.FindWidgetByName("form"));
        }
        form = mWindow.FindWidgetByName("form");

        // add some widgets
        {
            uik::CheckBox chk;
            chk.SetName("Check");
            chk.SetText("Check");
            chk.SetCheckLocation(uik::CheckBox::Check::Right);
            chk.SetPosition(30.0f, 30.0f);
            auto* child = mWindow.AddWidget(chk);
            mWindow.LinkChild(form, child);
        }
        {
            uik::PushButton ok;
            ok.SetName("ok");
            ok.SetText("OK");
            ok.SetPosition(150.0f, 30.0f);
            auto* child = mWindow.AddWidget(ok);
            mWindow.LinkChild(form, child);
        }
        {
            uik::PushButton play;
            play.SetName("play");
            play.SetText("Play!");
            play.SetPosition(300.0f, 30.0f);
            auto* child = mWindow.AddWidget(play);
            mWindow.LinkChild(form, child);
        }

        {
            uik::Label lbl;
            lbl.SetName("label");
            lbl.SetText("Hello world");
            lbl.SetPosition(30.0f, 80.0f);
            auto* child = mWindow.AddWidget(lbl);
            mWindow.LinkChild(form, child);
        }

        {
            uik::SpinBox spin;
            spin.SetName("spin");
            spin.SetPosition(200.0f, 80.0f);
            auto* child = mWindow.AddWidget(spin);
            mWindow.LinkChild(form, child);
        }

        {
            uik::Slider slider;
            slider.SetName("slider");
            slider.SetPosition(30.0f, 150.0f);
            slider.SetSize(250.f, 30.0f);
            auto* child = mWindow.AddWidget(slider);
            mWindow.LinkChild(form, child);
        }

        {
            uik::CheckBox chk;
            chk.SetName("Check");
            chk.SetText("Check");
            chk.SetCheckLocation(uik::CheckBox::Check::Left);
            chk.SetPosition(300.0f, 150.0f);
            auto* child = mWindow.AddWidget(chk);
            mWindow.LinkChild(form, child);
        }

        {
            uik::ProgressBar prg;
            prg.SetName("progress");
            prg.SetValue(0.5f);
            prg.SetPosition(30.0f, 200.0f);
            prg.SetText("Done %1%");
            auto* child = mWindow.AddWidget(prg);
            mWindow.LinkChild(form, child);
        }

        {
            uik::ProgressBar prg;
            prg.SetName("progress2");
            prg.SetPosition(30.0f, 250.0f);
            prg.SetText("Wait...");
            auto* child = mWindow.AddWidget(prg);
            mWindow.LinkChild(form, child);
        }

        mState.Clear();
        mTime = 0.0;
    }
    virtual void Tick() override
    {
        if (!mMessageQueue.empty())
            mMessageQueue.pop_front();
    }
    virtual void OnMousePress(const wdk::WindowEventMousePress& mickey) override
    {
        uik::Window::MouseEvent event;
        event.window_mouse_pos = uik::FPoint(mickey.window_x-mOffsetX, mickey.window_y-mOffsetY);
        event.native_mouse_pos = uik::FPoint(mickey.window_x, mickey.window_y);
        event.button = MapMouseButton(mickey.btn);
        event.time   = mTime;
        const auto& actions = mWindow.MousePress(event, mState);
        for (const auto& action : actions)
        {
            mMessageQueue.push_back(base::FormatString("Event: %1, widget: '%2'", action.type, action.name));
        }
    }
    virtual void OnMouseRelease(const wdk::WindowEventMouseRelease& mickey) override
    {
        uik::Window::MouseEvent event;
        event.window_mouse_pos = uik::FPoint(mickey.window_x-mOffsetX, mickey.window_y-mOffsetY);
        event.native_mouse_pos = uik::FPoint(mickey.window_x, mickey.window_y);
        event.button = MapMouseButton(mickey.btn);
        event.time   = mTime;
        const auto& actions = mWindow.MouseRelease(event, mState);
        for (const auto& action : actions)
        {
            mMessageQueue.push_back(base::FormatString("Event: %1, widget: '%2'", action.type, action.name));
        }
    }
    virtual void OnMouseMove(const wdk::WindowEventMouseMove& mickey) override
    {
        uik::Window::MouseEvent event;
        event.window_mouse_pos = uik::FPoint(mickey.window_x-mOffsetX, mickey.window_y-mOffsetY);
        event.native_mouse_pos = uik::FPoint(mickey.window_x, mickey.window_y);
        event.button = MapMouseButton(mickey.btn);
        event.time   = mTime;
        const auto& actions = mWindow.MouseMove(event, mState);
        for (const auto& action : actions)
        {
            mMessageQueue.push_back(base::FormatString("Event: %1, widget: '%2'", action.type, action.name));
            if (action.name == "slider")
            {
                const float value = std::get<float>(action.value);
                auto* widget = mWindow.FindWidgetByName("progress");
                auto* prog = dynamic_cast<uik::ProgressBar*>(widget);
                prog->SetValue(value);
            }
        }
    }
    virtual std::string GetName() const override
    { return "UITest"; }
private:
    uik::MouseButton MapMouseButton(const wdk::MouseButton btn) const
    {
        if (btn == wdk::MouseButton::Left)
            return uik::MouseButton::Left;
        else if (btn == wdk::MouseButton::Right)
            return uik::MouseButton::Right;
        else if (btn == wdk::MouseButton::Wheel)
            return uik::MouseButton::Wheel;
        else if (btn == wdk::MouseButton::WheelScrollUp)
            return uik::MouseButton::WheelUp;
        else if (btn == wdk::MouseButton::WheelScrollDown)
            return uik::MouseButton::WheelDown;
        return uik::MouseButton::None;
    }
private:
    float mOffsetX = 250.0f;
    float mOffsetY = 180.0f;
    uik::Window mWindow;
    uik::State mState;
    engine::UIStyle mStyle;
    engine::UIPainter mPainter;
    double mTime = 0.0;
    boost::circular_buffer<std::string> mMessageQueue;
};


class MyApp : public engine::Engine, public wdk::WindowListener
{
public:
    virtual bool ParseArgs(int argc, const char* argv[]) override
    {
        bool debug = false;
        for (int i = 1; i < argc; ++i)
        {
            if (!std::strcmp("--debug-log", argv[i]))
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
    virtual void Start() override
    {
#if !defined(__EMSCRIPTEN__)
        mTestList.emplace_back(new ViewportTest);
#endif
        mTestList.emplace_back(new AudioEffectTest);
        mTestList.emplace_back(new AudioMusicTest);
        mTestList.emplace_back(new PhysicsTest);
        mTestList.emplace_back(new EntityTest);
        mTestList.emplace_back(new SceneTest);
        mTestList.emplace_back(new UITest);
        mTestList[mTestIndex]->Start(&mClassLib);
        INFO("Test case: '%1'", mTestList[mTestIndex]->GetName());
    }

    virtual void Init(const InitParams& init) override
    {
        mDevice  = dev::CreateDevice(init.context)->GetSharedGraphicsDevice();
        mPainter = gfx::Painter::Create(mDevice);
        mPainter->SetSurfaceSize(init.surface_width, init.surface_height);
        mPainter->SetEditingMode(false);
        mSurfaceWidth  = init.surface_width;
        mSurfaceHeight = init.surface_height;
    }

    virtual void Draw() override
    {
        mDevice->BeginFrame();
        mDevice->ClearColor(gfx::Color4f(0.2, 0.3, 0.4, 1.0f));
        mPainter->SetViewport(0, 0, mSurfaceWidth, mSurfaceHeight);
        mPainter->SetProjectionMatrix(gfx::MakeOrthographicProjection(mSurfaceWidth , mSurfaceHeight));
        mTestList[mTestIndex]->SetSurfaceSize(mSurfaceWidth, mSurfaceHeight);
        mTestList[mTestIndex]->Render(*mPainter);
        mDevice->EndFrame(true);

        mDevice->CleanGarbage(120, gfx::Device::GCFlags::Textures);
    }

    virtual void Update(float dt) override
    {
        const auto time_step = 1.0/60.0;
        const auto tick_step = 1.0/1.0;
        mTimeAccum += dt;

        while (mTimeAccum >= time_step)
        {
            mTestList[mTestIndex]->Update(time_step);

            mTimeAccum -= time_step;
            mGameTime  += time_step;
            mTickAccum += time_step;
            auto tick_time = mGameTime;
            while (mTickAccum >= tick_step)
            {
                mTestList[mTestIndex]->Tick();
                tick_time += tick_step;
                mTickAccum -= tick_step;
            }
        }

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
    virtual void OnKeyDown(const wdk::WindowEventKeyDown& key) override
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
            mRequests.ToggleFullScreen();

        if (mTestIndex != current_test_index)
        {
            mTestList[current_test_index]->End();
            mTestList[mTestIndex]->Start(&mClassLib);
            INFO("Test case: '%1'", mTestList[mTestIndex]->GetName());
        }
        mTestList[mTestIndex]->OnKeydown(key);
    }
    virtual void OnMousePress(const wdk::WindowEventMousePress& mickey) override
    {
        mTestList[mTestIndex]->OnMousePress(mickey);
    }
    virtual void OnMouseRelease(const wdk::WindowEventMouseRelease& mickey) override
    {
        mTestList[mTestIndex]->OnMouseRelease(mickey);
    }
    virtual void OnMouseMove(const wdk::WindowEventMouseMove& mickey) override
    {
        mTestList[mTestIndex]->OnMouseMove(mickey);
    }

    virtual void SetHostStats(const HostStats& stats) override
    {
        DEBUG("fps: %1, wall_time: %2, game_time: %3, frames: %4",
            stats.current_fps, stats.total_wall_time, mGameTime, stats.num_frames_rendered);
    }
    virtual void OnRenderingSurfaceResized(unsigned width, unsigned height) override
    {
        DEBUG("Rendering surface resized to %1x%2", width, height);
        mSurfaceWidth  = width;
        mSurfaceHeight = height;
        mPainter->SetSurfaceSize(width, height);
    }
private:
    void TakeScreenshot()
    {
#if !defined(__EMSCRIPTEN__)
        const auto& rgba = mDevice->ReadColorBuffer(1024, 768);
        gfx::WritePNG(rgba, "screenshot.png");
        INFO("Wrote screenshot");
#endif
    }
private:
    TestClassLib mClassLib;
    std::size_t mTestIndex = 0;
    std::vector<std::unique_ptr<TestCase>> mTestList;
    std::unique_ptr<gfx::Painter> mPainter;
    std::shared_ptr<gfx::Device> mDevice;
    bool mRunning = true;
    engine::AppRequestQueue mRequests;
    unsigned mSurfaceWidth  = 0;
    unsigned mSurfaceHeight = 0;
    double mGameTime = 0.0;
    double mTimeAccum = 0.0;
    double mTickAccum = 0.0;
};

extern "C" {

GAMESTUDIO_EXPORT engine::Engine* Gamestudio_CreateEngine()
{
    DEBUG("test engine");
    return new MyApp;
}

} // extern "C"

