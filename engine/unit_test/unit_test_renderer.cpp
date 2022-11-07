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

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/test_help.h"

#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "game/entity.h"
#include "game/scene.h"
#include "engine/renderer.h"
#include "engine/classlib.h"

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

template<typename Pixel>
size_t CountPixels(const gfx::Bitmap<Pixel>& bmp, const gfx::URect& area, gfx::Color color)
{
    size_t ret = 0;
    for (unsigned row=0; row<area.GetHeight(); ++row) {
        for (unsigned col=0; col<area.GetWidth(); ++col) {
            const auto& pos = area.MapToGlobal(col, row);
            if (bmp.GetPixel(pos) == color)
                ++ret;
        }
    }
    return ret;
}

template<typename Pixel>
bool TestPixelCount(const gfx::Bitmap<Pixel>& bmp, const gfx::URect& area, gfx::Color color, float minimum)
{
    const double matching_pixels = CountPixels(bmp, area, color);
    const double  area_size = area.GetWidth() * area.GetHeight();
    if (matching_pixels / area_size >= minimum)
        return true;
    return false;
}

// setup context for headless rendering.
class TestContext : public gfx::Device::Context
{
public:
    TestContext(unsigned w, unsigned h)
    {
        wdk::Config::Attributes attrs;
        attrs.red_size         = 8;
        attrs.green_size       = 8;
        attrs.blue_size        = 8;
        attrs.alpha_size       = 8;
        attrs.stencil_size     = 8;
        attrs.depth_size       = 24;
        attrs.surfaces.pbuffer = true;
        attrs.double_buffer    = false;
        attrs.srgb_buffer      = true;
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
    { return Version::OpenGL_ES2; }
private:
    std::unique_ptr<wdk::Context> mContext;
    std::unique_ptr<wdk::Surface> mSurface;
    std::unique_ptr<wdk::Config>  mConfig;
};

class DummyClassLib : public engine::ClassLibrary
{
public:
    virtual ClassHandle<const audio::GraphClass> FindAudioGraphClassById(const std::string& id) const override
    { return nullptr; }
    virtual ClassHandle<const audio::GraphClass> FindAudioGraphClassByName(const std::string& name) const override
    { return nullptr; }
    virtual ClassHandle<const uik::Window> FindUIByName(const std::string& name) const override
    { return nullptr; }
    virtual ClassHandle<const uik::Window> FindUIById(const std::string& id) const override
    { return nullptr; }
    virtual ClassHandle<const gfx::MaterialClass> FindMaterialClassById(const std::string& id) const override
    {
        if (id == "pink")
            return std::make_shared<gfx::ColorClass>(gfx::CreateMaterialClassFromColor(gfx::Color::HotPink));
        else if (id == "red")
            return std::make_shared<gfx::ColorClass>(gfx::CreateMaterialClassFromColor(gfx::Color::Red));
        else if (id == "green")
            return std::make_shared<gfx::ColorClass>(gfx::CreateMaterialClassFromColor(gfx::Color::Green));
        else if (id == "red-green")
        {
            gfx::RgbBitmap bmp;
            bmp.Resize(2, 2);
            bmp.SetPixel(0, 0, gfx::Color::Red);
            bmp.SetPixel(1, 0, gfx::Color::Red);
            bmp.SetPixel(0, 1, gfx::Color::Green);
            bmp.SetPixel(1, 1, gfx::Color::Green);
            gfx::detail::TextureBitmapBufferSource src;
            src.SetName("bitmap");
            src.SetBitmap(std::move(bmp));

            gfx::TextureMap2DClass klass;
            klass.SetTexture(src.Copy());
            return std::make_shared<gfx::TextureMap2DClass>(klass);
        }
        else if (id == "red-green-sprite")
        {
            gfx::SpriteClass sprite;
            sprite.SetFps(1.0f);
            sprite.SetBlendFrames(false);

            gfx::RgbBitmap red;
            red.Resize(4, 4);
            red.Fill(gfx::Color::Red);
            gfx::detail::TextureBitmapBufferSource red_bitmap_src;
            red_bitmap_src.SetName("red");
            red_bitmap_src.SetBitmap(red);
            sprite.AddTexture(red_bitmap_src.Copy());

            gfx::RgbBitmap green;
            green.Resize(4, 4);
            green.Fill(gfx::Color::Green);
            gfx::detail::TextureBitmapBufferSource green_bitmap_src;
            green_bitmap_src.SetName("green");
            green_bitmap_src.SetBitmap(green);
            sprite.AddTexture(green_bitmap_src.Copy());
            return std::make_shared<gfx::SpriteClass>(sprite);
        }
        else if (id == "custom")
        {
constexpr auto* src = R"(
#version 100
precision highp float;
uniform vec4 kColor;
void main() {
  gl_FragColor = kColor;
}
)";
            gfx::CustomMaterialClass klass;
            klass.SetShaderSrc(src);
            klass.SetUniform("kColor", gfx::Color::HotPink);
            return std::make_shared<gfx::CustomMaterialClass>(klass);
        }
        TEST_REQUIRE(!"OOPS");
        return nullptr;
    }
    virtual ClassHandle<const gfx::DrawableClass> FindDrawableClassById(const std::string& id) const override
    {
        if (id == "rect")
            return std::make_shared<gfx::RectangleClass>();
        else if (id == "particles")
            return std::make_shared<gfx::KinematicsParticleEngineClass>();
        TEST_REQUIRE(!"OOPS");
        return nullptr;
    }
    virtual ClassHandle<const game::EntityClass> FindEntityClassByName(const std::string& name) const override
    { return nullptr; }
    virtual ClassHandle<const game::EntityClass> FindEntityClassById(const std::string& id) const override
    { return nullptr; }
    virtual ClassHandle<const game::SceneClass> FindSceneClassByName(const std::string& name) const override
    { return nullptr; }
    virtual ClassHandle<const game::SceneClass> FindSceneClassById(const std::string& id) const override
    { return nullptr; }
    virtual ClassHandle<const game::TilemapClass> FindTilemapClassById(const std::string& id) const override
    { return nullptr; }
private:
};


void unit_test_drawable_item()
{
    auto device = gfx::Device::Create(std::make_shared<TestContext>(256, 256));
    auto painter = gfx::Painter::Create(device);
    painter->SetEditingMode(false);
    painter->SetOrthographicProjection(256, 256);
    painter->SetViewport(0, 0, 256, 256);
    painter->SetSurfaceSize(256, 256);

    auto klass = std::make_shared<game::EntityClass>();
    klass->SetName("entity");

    game::DrawableItemClass drawable;
    drawable.SetDrawableId("rect");
    drawable.SetMaterialId("pink");

    game::EntityNodeClass node;
    node.SetName("foo");
    node.SetSize(glm::vec2(200.0f, 100.0f));
    node.SetTranslation(glm::vec2(100.0f, 50.0f));
    node.SetDrawable(drawable);
    klass->LinkChild(nullptr, klass->AddNode(node));

    auto entity = game::CreateEntityInstance(klass);

    DummyClassLib classloader;

    engine::Renderer renderer(&classloader);
    renderer.SetEditingMode(true);
    gfx::Transform transform;

    // test visibility flag
    {
        entity->GetNode(0).GetDrawable()->SetFlag(game::DrawableItem::Flags::VisibleInGame, true);

        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);

        auto bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(0, 0, 200, 100), gfx::Color::HotPink, 0.95));

        entity->GetNode(0).GetDrawable()->SetFlag(game::DrawableItem::Flags::VisibleInGame, false);
        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);

        bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(bmp.Compare(gfx::URect(0, 0, 200, 100), gfx::Color::Blue));
    }

    // change material at class level to a sprite so that
    // updating makes a visual difference.
    klass->GetNode(0).GetDrawable()->SetMaterialId("red-green");

    // test horizontal flip
    {
        entity->GetNode(0).GetDrawable()->SetFlag(game::DrawableItem::Flags::VisibleInGame, true);
        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);

        auto bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(0, 0, 100, 100), gfx::Color::Red, 0.95));
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(100, 0, 100, 100), gfx::Color::Green, 0.95));
        //TEST_REQUIRE(bmp.Compare(gfx::URect(0, 0, 100, 100), gfx::Color::Red));
        //TEST_REQUIRE(bmp.Compare(gfx::URect(100, 0, 100, 100), gfx::Color::Green));

        entity->GetNode(0).GetDrawable()->SetFlag(game::DrawableItem::Flags::FlipHorizontally, true);
        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);

        bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(0, 0, 100, 100), gfx::Color::Green, 0.95));
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(100, 0, 100, 100), gfx::Color::Red, 0.95));
    }

    // change material at class level to a sprite so that
    // updating makes a visual difference.
    klass->GetNode(0).GetDrawable()->SetMaterialId("red-green-sprite");

    // check material update
    {
        entity->GetNode(0).GetDrawable()->SetFlag(game::DrawableItem::Flags::UpdateMaterial, false);
        entity->GetNode(0).GetDrawable()->SetFlag(game::DrawableItem::Flags::VisibleInGame, true);
        entity->GetNode(0).GetDrawable()->SetFlag(game::DrawableItem::Flags::FlipHorizontally, false);

        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);
        auto bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(0, 0, 200, 100), gfx::Color::Red, 0.95));

        // this will update the material which would update its render color
        // but the drawable flag to update material is not set, so the material should *NOT* update.
        renderer.Update(*entity, 0.0f, 15.0f);

        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);
        bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(0, 0, 200, 100), gfx::Color::Red, 0.95));

        // enable the material update flag. the material should now change color.
        entity->GetNode(0).GetDrawable()->SetFlag(game::DrawableItem::Flags::UpdateMaterial, true);
        renderer.Update(*entity, 0.0f, 1.5f);

        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);
        bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(0, 0, 200, 100), gfx::Color::Green, 0.95));
    }

    // change material at class level to a sprite so that
    // updating makes a visual difference.
    klass->GetNode(0).GetDrawable()->SetMaterialId("custom");

    // check material parameter
    {
        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);
        auto bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(0, 0, 200, 100), gfx::Color::HotPink, 0.95));


        entity->GetNode(0).GetDrawable()->SetMaterialParam("kColor", gfx::Color::Green);
        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);
        bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(0, 0, 200, 100), gfx::Color::Green, 0.95));
    }

    // change drawable at class level to a sprite so that
    // updating makes a visual difference.
    klass->GetNode(0).GetDrawable()->SetDrawableId("particles");

    // drawable update
    {
        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
            device->EndFrame(true);
        }
        auto bmp0 = device->ReadColorBuffer(0, 0, 256, 256);

        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Update(*entity, 0.0f, 1.0f / 60.0f);
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);
        auto bmp1 = device->ReadColorBuffer(0, 0, 256, 256);

        entity->GetNode(0).GetDrawable()->SetFlag(game::DrawableItem::Flags::UpdateDrawable, false);
        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
             renderer.BeginFrame();
            {
                renderer.Update(*entity, 0.0f, 1.0f / 60.0f);
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);
        auto bmp2 = device->ReadColorBuffer(0, 0, 256, 256);

        TEST_REQUIRE(bmp0 != bmp1);
        TEST_REQUIRE(bmp1 == bmp2);

    }
}

void unit_test_text_item()
{
    // todo:
}

void unit_test_entity_layering()
{
    auto device = gfx::Device::Create(std::make_shared<TestContext>(256, 256));
    auto painter = gfx::Painter::Create(device);
    painter->SetEditingMode(false);
    painter->SetOrthographicProjection(256, 256);
    painter->SetViewport(0, 0, 256, 256);
    painter->SetSurfaceSize(256, 256);

    auto klass = std::make_shared<game::EntityClass>();
    klass->SetName("entity");

    game::DrawableItemClass red;
    red.SetDrawableId("rect");
    red.SetMaterialId("red");
    red.SetLayer(0);

    game::EntityNodeClass red_node;
    red_node.SetName("red");
    red_node.SetSize(glm::vec2(200.0f, 100.0f));
    red_node.SetTranslation(glm::vec2(100.0f, 50.0f));
    red_node.SetDrawable(red);
    klass->LinkChild(nullptr, klass->AddNode(red_node));

    game::DrawableItemClass green;
    green.SetDrawableId("rect");
    green.SetMaterialId("green");
    green.SetLayer(1);

    game::EntityNodeClass green_node;
    green_node.SetName("green");
    green_node.SetSize(glm::vec2(200.0f, 100.0f));
    green_node.SetTranslation(glm::vec2(100.0f, 50.0f));
    green_node.SetDrawable(green);
    klass->LinkChild(nullptr, klass->AddNode(green_node));

    auto entity = game::CreateEntityInstance(klass);

    DummyClassLib classloader;

    engine::Renderer renderer(&classloader);
    renderer.SetEditingMode(true);

    gfx::Transform transform;

    // green should be on top
    {
        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);

        auto bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(0, 0, 200, 100), gfx::Color::Green, 0.95));
    }

    klass->FindNodeByName("red")->GetDrawable()->SetLayer(2);

    // red should be on top.
    {
        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *painter, transform);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);

        auto bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(0, 0, 200, 100), gfx::Color::Red, 0.95));
    }
}

void unit_test_scene_layering()
{
    // todo:
}

void unit_test_entity_lifecycle()
{
    auto device = gfx::Device::Create(std::make_shared<TestContext>(256, 256));
    auto painter = gfx::Painter::Create(device);
    painter->SetEditingMode(false);
    painter->SetOrthographicProjection(256, 256);
    painter->SetViewport(0, 0, 256, 256);
    painter->SetSurfaceSize(256, 256);

    auto entity_klass = std::make_shared<game::EntityClass>();
    {
        game::DrawableItemClass red;
        red.SetDrawableId("rect");
        red.SetMaterialId("red");
        red.SetLayer(0);

        game::EntityNodeClass red_node;
        red_node.SetName("red");
        red_node.SetSize(glm::vec2(200.0f, 100.0f));
        red_node.SetTranslation(glm::vec2(100.0f, 50.0f));
        red_node.SetDrawable(red);

        entity_klass->LinkChild(nullptr, entity_klass->AddNode(red_node));
        entity_klass->SetName("entity");
    }

    auto scene_class = std::make_shared<game::SceneClass>();
    {
        game::SceneNodeClass node;
        node.SetEntity(entity_klass);
        node.SetName("1");
        scene_class->LinkChild(nullptr, scene_class->AddNode(node));
        scene_class->SetName("scene");
    }

    auto scene = game::CreateSceneInstance(scene_class);

    DummyClassLib classloader;
    engine::Renderer renderer(&classloader);

    const float dt = 1.0f/60.0f;

    engine::EntityInstanceDrawHook* hook = nullptr;

    renderer.CreateScene(*scene);
    TEST_REQUIRE(renderer.GetNumPaintNodes() == 1);

    // start frame looping
    {
        scene->BeginLoop();
        {
            scene->Update(dt, nullptr);

            renderer.UpdateScene(*scene);
            TEST_REQUIRE(renderer.GetNumPaintNodes() == 1);
            renderer.Update(0.0f, dt);

            // simulate game update here. entity gets killed.
            auto* ent = scene->FindEntityByInstanceName("1");
            ent->Die();

            renderer.BeginFrame();
            {
                renderer.Draw(*painter, hook);
            }
            renderer.EndFrame();
        }
        scene->EndLoop();
    }

    // entity 1 was killed, entity 2 gets spawned.
    {
        scene->BeginLoop();
        {
            scene->Update(dt, nullptr);

            renderer.UpdateScene(*scene);
            TEST_REQUIRE(renderer.GetNumPaintNodes() == 0);
            renderer.Update(0.0f, dt);

            // simulate game update here, entity gets spawned.
            game::EntityArgs args;
            args.klass = entity_klass;
            args.name = "2";
            args.id = "2";
            scene->SpawnEntity(args);

            renderer.BeginFrame();
            {
                renderer.Draw(*painter, hook);
            }
            renderer.EndFrame();
        }
        scene->EndLoop();
    }

    //
    {
        scene->BeginLoop();
        {
            scene->Update(dt, nullptr);

            renderer.UpdateScene(*scene);
            TEST_REQUIRE(renderer.GetNumPaintNodes() == 1);
            renderer.Update(0.0f, dt);

            renderer.BeginFrame();
            {
                renderer.Draw(*painter, hook);
            }
            renderer.EndFrame();
        }
    }
}

int test_main(int argc, char* argv[])
{
    unit_test_drawable_item();
    unit_test_text_item();
    unit_test_entity_layering();
    unit_test_scene_layering();
    unit_test_entity_lifecycle();
    return 0;
}
