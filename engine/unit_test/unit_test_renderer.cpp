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

#include <vector>
#include <fstream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/test_help.h"
#include "device/device.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "graphics/utility.h"
#include "game/entity.h"
#include "game/scene.h"
#include "game/util.h"
#include "game/tilemap.h"
#include "game/loader.h"
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

class TestMapData : public game::TilemapData
{
public:
    virtual void Write(const void* ptr, size_t bytes, size_t offset) override
    {
        TEST_REQUIRE(offset + bytes <= mBytes.size());

        std::memcpy(&mBytes[offset], ptr, bytes);
    }
    virtual void Read(void* ptr, size_t bytes, size_t offset) const override
    {
        TEST_REQUIRE(offset + bytes <= mBytes.size());

        std::memcpy(ptr, &mBytes[offset], bytes);
    }
    virtual size_t AppendChunk(size_t bytes) override
    {
        const auto offset = mBytes.size();
        mBytes.resize(offset + bytes);
        return offset;
    }
    virtual void Resize(size_t bytes) override
    {
        mBytes.resize(bytes);
    }
    virtual void ClearChunk(const void* value, size_t value_size, size_t offset, size_t num_values) override
    {
        TEST_REQUIRE(offset + value_size * num_values <= mBytes.size());

        for (size_t i=0; i<num_values; ++i)
        {
            const auto buffer_offset = offset + i * value_size;
            TEST_REQUIRE(buffer_offset + value_size <= mBytes.size());
            std::memcpy(&mBytes[buffer_offset], value, value_size);
        }
    }
    virtual size_t GetByteCount() const override
    {
        return mBytes.size();
    }

    void Dump(const std::string& file) const
    {
        std::ofstream out;
        out.open(file, std::ios::out | std::ios::binary);
        TEST_REQUIRE(out.is_open());
        out.write((const char*)(&mBytes[0]), mBytes.size());
    }
private:
    std::vector<unsigned char> mBytes;
    unsigned mNumRows = 0;
    unsigned mNumCols = 0;
    size_t mValSize = 0;
};


// setup context for headless rendering.
class TestContext : public dev::Context
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
    virtual ClassHandle<const gfx::MaterialClass> FindMaterialClassByName(const std::string& name) const override
    { return nullptr; }
    virtual ClassHandle<const gfx::MaterialClass> FindMaterialClassById(const std::string& id) const override
    {
        if (id == "pink")
            return std::make_shared<gfx::ColorClass>(gfx::CreateMaterialClassFromColor(gfx::Color::HotPink));
        else if (id == "red")
            return std::make_shared<gfx::ColorClass>(gfx::CreateMaterialClassFromColor(gfx::Color::Red));
        else if (id == "green")
            return std::make_shared<gfx::ColorClass>(gfx::CreateMaterialClassFromColor(gfx::Color::Green));
        else if (id == "blue")
            return std::make_shared<gfx::ColorClass>(gfx::CreateMaterialClassFromColor(gfx::Color::Blue));
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

            gfx::TextureMap2DClass klass(gfx::MaterialClass::Type::Texture);
            klass.SetTexture(src.Copy());
            return std::make_shared<gfx::TextureMap2DClass>(klass);
        }
        else if (id == "red-green-sprite")
        {
            gfx::SpriteClass sprite(gfx::MaterialClass::Type::Sprite);

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

            sprite.GetTextureMap(0)->SetFps(1.0f);
            sprite.SetBlendFrames(false);
            return std::make_shared<gfx::SpriteClass>(sprite);
        }
        else if (id == "custom")
        {
constexpr auto* src = R"(
uniform vec4 kColor;
void FragmentShaderMain() {
  fs_out.color = kColor;
}
)";
            gfx::CustomMaterialClass klass(gfx::MaterialClass::Type::Custom);
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
        else if (id == "circle")
            return std::make_shared<gfx::CircleClass>();
        else if (id == "particles")
            return std::make_shared<gfx::ParticleEngineClass>();
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

std::shared_ptr<gfx::Device> CreateDevice(unsigned width=256, unsigned height=256)
{
    return dev::CreateDevice(std::make_shared<TestContext>(width, height))->GetSharedGraphicsDevice();
}

void unit_test_drawable_item()
{
    TEST_CASE(test::Type::Feature)

    auto device = CreateDevice();

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

    engine::Renderer::Surface surface;
    surface.size     = gfx::USize(256, 256);
    surface.viewport = gfx::IRect(0, 0, 256, 256);
    renderer.SetSurface(surface);

    engine::Renderer::Camera camera;
    camera.viewport = gfx::FRect(0.0f, 0.0f, 256.0f, 256.0f);
    renderer.SetCamera(camera);

    // test visibility flag
    {
        entity->GetNode(0).GetDrawable()->SetFlag(game::DrawableItem::Flags::VisibleInGame, true);

        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *device);
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
                renderer.Draw(*entity, *device);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);

        bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(bmp.Compare(gfx::URect(0, 0, 200, 100), gfx::Color::Blue));
    }

    // change material so that updating makes a visual difference.
    entity->GetNode(0).GetDrawable()->SetMaterialId("red-green");

    // test horizontal flip
    {
        entity->GetNode(0).GetDrawable()->SetFlag(game::DrawableItem::Flags::VisibleInGame, true);
        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *device);
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
                renderer.Draw(*entity, *device);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);

        bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(0, 0, 100, 100), gfx::Color::Green, 0.95));
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(100, 0, 100, 100), gfx::Color::Red, 0.95));
    }

    // change material so that updating makes a visual difference.
    entity->GetNode(0).GetDrawable()->SetMaterialId("red-green-sprite");

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
                renderer.Draw(*entity, *device);
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
                renderer.Draw(*entity, *device);
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
                renderer.Draw(*entity, *device);
            }
            renderer.EndFrame();
        }
        device->EndFrame(true);
        bmp = device->ReadColorBuffer(0, 0, 256, 256);
        TEST_REQUIRE(TestPixelCount(bmp, gfx::URect(0, 0, 200, 100), gfx::Color::Green, 0.95));
    }

    // change material so that updating makes a visual difference.
    entity->GetNode(0).GetDrawable()->SetMaterialId("custom");

    // check material parameter
    {
        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *device);
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
                renderer.Draw(*entity, *device);
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
                renderer.Draw(*entity, *device);
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
                renderer.Draw(*entity, *device);
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
                renderer.Draw(*entity, *device);
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
    TEST_CASE(test::Type::Feature)

    auto device = CreateDevice();

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

    engine::Renderer::Surface surface;
    surface.size     = gfx::USize(256, 256);
    surface.viewport = gfx::IRect(0, 0, 256, 256);
    renderer.SetSurface(surface);

    engine::Renderer::Camera camera;
    camera.viewport = gfx::FRect(0.0f, 0.0f, 256.0f, 256.0f);
    renderer.SetCamera(camera);

    // green should be on top
    {
        device->BeginFrame();
        {
            device->ClearColor(gfx::Color::Blue);
            renderer.BeginFrame();
            {
                renderer.Draw(*entity, *device);
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
                renderer.Draw(*entity, *device);
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
    TEST_CASE(test::Type::Feature)

    auto device = CreateDevice();

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
        game::EntityPlacement node;
        node.SetEntity(entity_klass);
        node.SetName("1");
        scene_class->LinkChild(nullptr, scene_class->PlaceEntity(node));
        scene_class->SetName("scene");
    }

    auto scene = game::CreateSceneInstance(scene_class);

    DummyClassLib classloader;
    engine::Renderer renderer(&classloader);

    const float dt = 1.0f/60.0f;

    renderer.CreateRenderStateFromScene(*scene);
    TEST_REQUIRE(renderer.GetNumPaintNodes() == 1);

    engine::Renderer::Surface surface;
    surface.size     = gfx::USize(256, 256);
    surface.viewport = gfx::IRect(0, 0, 256, 256);
    renderer.SetSurface(surface);

    engine::Renderer::Camera camera;
    camera.viewport = gfx::FRect(0.0f, 0.0f, 256.0f, 256.0f);
    renderer.SetCamera(camera);

    // start frame looping
    {
        scene->BeginLoop();
        {
            scene->Update(dt, nullptr);

            renderer.UpdateRenderStateFromScene(*scene);
            TEST_REQUIRE(renderer.GetNumPaintNodes() == 1);
            renderer.Update(0.0f, dt);

            // simulate game update here. entity gets killed.
            auto* ent = scene->FindEntityByInstanceName("1");
            ent->Die();

            renderer.BeginFrame();
            {
                renderer.Draw(*device);
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

            renderer.UpdateRenderStateFromScene(*scene);
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
                renderer.Draw(*device);
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

            renderer.UpdateRenderStateFromScene(*scene);
            TEST_REQUIRE(renderer.GetNumPaintNodes() == 1);
            renderer.Update(0.0f, dt);

            renderer.BeginFrame();
            {
                renderer.Draw(*device);
            }
            renderer.EndFrame();
        }
    }
}

// test the precision of the entity (node) transformations.
void unit_test_transform_precision()
{
    TEST_CASE(test::Type::Feature)

    auto device = CreateDevice(1024, 1024);

    game::EntityClass entity;
    // first node has a transformation without rotation.
    {
        game::DrawableItemClass drawable;
        drawable.SetMaterialId("red");
        drawable.SetDrawableId("circle");
        drawable.SetLayer(0);

        game::EntityNodeClass node;
        node.SetName("first");
        node.SetSize(glm::vec2(200.0f, 200.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f); // radians
        node.SetDrawable(drawable);
        entity.LinkChild(nullptr, entity.AddNode(std::move(node)));
    }
    // second node has a transformation with rotation.
    // the transformation relative to the parent should be such that
    // this node completely covers the first node.
    {
        game::DrawableItemClass drawable;
        drawable.SetMaterialId("green");
        drawable.SetDrawableId("circle");
        drawable.SetLayer(1);

        game::EntityNodeClass node;
        node.SetName("second");
        node.SetSize(glm::vec2(200.0f, 200.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(math::Pi); // Pi radians, i.e. 180 degrees.
        node.SetDrawable(drawable);
        entity.LinkChild(nullptr, entity.AddNode(std::move(node)));
    }

    DummyClassLib classlib;

    engine::Renderer renderer(&classlib);
    renderer.SetEditingMode(false);

    engine::Renderer::Surface surface;
    surface.size     = gfx::USize(1024, 1024);
    surface.viewport = gfx::IRect(0, 0, 1024, 1024);
    renderer.SetSurface(surface);

    engine::Renderer::Camera camera;
    camera.viewport = gfx::FRect(0.0f, 0.0f, 1024.0f, 1024.0f);
    camera.position = glm::vec2{-512.0f, -512.0f};
    renderer.SetCamera(camera);

    {
        class Hook : public engine::EntityClassDrawHook {
        public:
            virtual bool InspectPacket(const game::EntityNodeClass* node, engine::DrawPacket& packet) override
            {
                mPackets.push_back(packet);
                return true;
            }
            void Test()
            {
                TEST_REQUIRE(mPackets.size() == 2);
                const auto& rect0 = game::ComputeBoundingRect(mPackets[0].transform);
                const auto& rect1 = game::ComputeBoundingRect(mPackets[1].transform);
                TEST_REQUIRE(rect0 == rect1);
            }
        private:
            std::vector<engine::DrawPacket> mPackets;
        } hook;

        device->BeginFrame();
        device->ClearColor(gfx::Color::Black);

        renderer.BeginFrame();
            renderer.Draw(entity, *device, &hook);
        renderer.EndFrame();

        hook.Test();

        device->EndFrame(true);
    }
}

void unit_test_axis_aligned_map()
{
    TEST_CASE(test::Type::Feature)

    // Set things up, create a 2x2 tiles map with 2 layers
    auto map = std::make_shared<game::TilemapClass>();
    map->SetTileWidth(50.0f);
    map->SetTileHeight(50.0f);
    map->SetTileDepth(50.0f);
    map->SetMapWidth(2);
    map->SetMapHeight(2);
    map->SetPerspective(game::TilemapClass::Perspective::AxisAligned);

    auto layer0 = std::make_shared<game::TilemapLayerClass>();
    layer0->SetName("layer0");
    layer0->SetDepth(0);
    layer0->SetType(game::TilemapLayerClass::Type::Render);
    layer0->SetDefaultTilePaletteMaterialIndex(layer0->GetMaxPaletteIndex());
    layer0->SetReadOnly(false);
    layer0->SetPaletteMaterialId("red", 0);
    layer0->SetPaletteMaterialId("green", 1);
    layer0->SetPaletteMaterialId("blue", 2);
    layer0->SetPaletteMaterialId("pink", 3);
    map->AddLayer(layer0);

    auto layer1 = std::make_shared<game::TilemapLayerClass>();
    layer1->SetName("layer1");
    layer1->SetDepth(0);
    layer1->SetType(game::TilemapLayerClass::Type::Render);
    layer1->SetDefaultTilePaletteMaterialIndex(layer1->GetMaxPaletteIndex());
    layer1->SetReadOnly(false);
    layer1->SetPaletteMaterialId("red", 0);
    layer1->SetPaletteMaterialId("green", 1);
    layer1->SetPaletteMaterialId("blue", 2);
    layer1->SetPaletteMaterialId("pink", 3);
    map->AddLayer(layer1);

    auto data0 = std::make_shared<TestMapData>();
    auto data1 = std::make_shared<TestMapData>();
    // initialize the layer data structures on the data object.
    layer0->Initialize(map->GetMapWidth(), map->GetMapHeight(), *data0);
    layer1->Initialize(map->GetMapWidth(), map->GetMapHeight(), *data1);

    // setup layer0 tile data
    {

        auto layer = game::CreateTilemapLayer(layer0, map->GetMapWidth(), map->GetMapHeight());

        layer->Load(data0, 1024);

        auto* ptr = game::TilemapLayerCast<game::TilemapLayer_Render>(layer);
        ptr->SetTile({0}, 0, 0);
        ptr->SetTile({1}, 0, 1);
        ptr->SetTile({2}, 1, 0);
        ptr->SetTile({3}, 1, 1);

        layer->FlushCache();
        layer->Save();
    }
    // setup layer1 tile data
    {
        auto layer = game::CreateTilemapLayer(layer1, map->GetMapWidth(), map->GetMapHeight());
        layer->Load(data1, 1024);

        auto* ptr = game::TilemapLayerCast<game::TilemapLayer_Render>(layer);
        ptr->SetTile({3}, 0, 0);
        ptr->SetTile({2}, 0, 1);
        ptr->SetTile({1}, 1, 0);
        ptr->SetTile({0}, 1, 1);

        layer->FlushCache();
        layer->Save();
    }

    auto map_instance = game::CreateTilemap(map);
    // calling load on each layer instead of calling MapLoad because we don't
    // have the loader implemented for tilemap data.
    map_instance->GetLayer(0).Load(data0, 1024);
    map_instance->GetLayer(1).Load(data1, 1024);
    TEST_REQUIRE(map_instance->GetNumLayers() == 2);
    TEST_REQUIRE(map_instance->GetLayer(0).IsLoaded());
    TEST_REQUIRE(map_instance->GetLayer(1).IsLoaded());
    TEST_REQUIRE(map_instance->GetLayer(0).IsVisible());
    TEST_REQUIRE(map_instance->GetLayer(1).IsVisible());

    auto device = CreateDevice();
    auto painter = gfx::Painter::Create(device);

    painter->SetEditingMode(false);
    painter->SetViewport(0, 0, 256, 256);
    painter->SetSurfaceSize(256, 256);

    DummyClassLib library;

    engine::Renderer renderer;
    renderer.SetClassLibrary(&library);
    renderer.SetEditingMode(false);

    engine::Renderer::Surface surface;
    surface.size = painter->GetSurfaceSize();
    surface.viewport = painter->GetViewport();
    renderer.SetSurface(surface);

    engine::Renderer::Camera camera;
    camera.position   = glm::vec2{0.0f, 0.0f};
    camera.scale      = glm::vec2{1.0f, 1.0f};
    camera.rotation   = 0.0f;
    camera.viewport   = game::FRect(-128, -128, 256, 256);
    renderer.SetCamera(camera);

    class MapHook : public engine::TileBatchDrawHook {
    public:
        virtual void BeginDrawBatch(const engine::DrawPacket& packet, gfx::Painter&) override
        {
            batches.push_back(packet);
        }
        void Clear()
        {
            batches.clear();
        }
        std::vector<engine::DrawPacket> batches;
    } hook;

    renderer.Draw(*map_instance, *device, &hook, true, false);

    {
        const auto& bmp = device->ReadColorBuffer(0, 0, 256, 256);
        gfx::WritePNG(bmp, "map_render.png");
    }

    // verify the render order.
    {
        // current rendering order should be
        // row < col < height < layer

        // but right now we assume the layer takes care of the height,
        // so we end up with
        //
        // row=0, col=0, layer=0
        // row=0, col=0, layer=1,
        // row=0, col=1, layer=0,
        // row=0; col=1, layer=1,
        // ...
        // row=n, col=m; layer=0,
        // row=n, col=m, layer=1

        const auto& batches = hook.batches;
        TEST_REQUIRE(batches.size() == 2 * 2 * 2);

        struct Expected {
            uint16_t row, col, layer;
        } expected[] = {
            {0, 0, 0},
            {0, 0, 1},
            {0, 1, 0},
            {0, 1, 1},
            {1, 0, 0},
            {1, 0, 1},
            {1, 1, 0},
            {1, 1, 1}
        };
        for (size_t i=0; i<batches.size(); ++i)
        {
            TEST_REQUIRE(batches[i].map_row   == expected[i].row);
            TEST_REQUIRE(batches[i].map_col   == expected[i].col);
            TEST_REQUIRE(batches[i].map_layer == expected[i].layer);
        }
    }

    // verify the render and tile size.
    {
        hook.Clear();

        map->SetTileRenderWidthScale(1.0);
        map->SetTileRenderHeightScale(1.0f);
        renderer.Draw(*map_instance, *device, &hook, true, false);
        for (size_t i=0; i<hook.batches.size(); ++i)
        {
            const auto* tiles = dynamic_cast<const gfx::TileBatch*>(hook.batches[i].drawable.get());
            TEST_REQUIRE(tiles->GetTileRenderSize() == glm::vec2(50.0f, 50.0f));
        }

        hook.Clear();

        // change the render scale
        map->SetTileRenderWidthScale(2.0);
        map->SetTileRenderHeightScale(1.0f);
        renderer.Draw(*map_instance, *device, &hook, true, false);

        for (size_t i=0; i<hook.batches.size(); ++i)
        {
            const auto* tiles = dynamic_cast<const gfx::TileBatch*>(hook.batches[i].drawable.get());
            TEST_REQUIRE(tiles->GetTileRenderSize() == glm::vec2(100.0f, 50.0f));
        }

        hook.Clear();

        map->SetTileRenderWidthScale(1.0f);
        map->SetTileRenderHeightScale(2.0f);
        renderer.Draw(*map_instance, *device, &hook, true, false);
        for (size_t i=0; i<hook.batches.size(); ++i)
        {
            const auto* tiles = dynamic_cast<const gfx::TileBatch*>(hook.batches[i].drawable.get());
            TEST_REQUIRE(tiles->GetTileRenderSize() == glm::vec2(50.0f, 100.0f));
        }
    }
}

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    unit_test_drawable_item();
    unit_test_text_item();
    unit_test_entity_layering();
    unit_test_scene_layering();
    unit_test_entity_lifecycle();
    unit_test_transform_precision();

    unit_test_axis_aligned_map();
    return 0;
}
) // TEST_MAIN