// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include <memory>
#include <vector>
#include <cstring>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "base/logging.h"
#include "base/format.h"
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
#include "graphics/framebuffer.h"
#include "graphics/texture.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/renderpass.h"
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
    virtual std::string GetName() const = 0;
    virtual bool IsFeatureTest() const
    { return true; }
private:
};

class ScissorTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        const auto cycle = 2.0f;
        const auto time = fmodf(mTime, cycle);
        const bool clip = time >= (cycle * 0.5);
        if (clip)
            painter.SetScissor(10, 10, 300, 300);

        gfx::FillRect(painter, gfx::FRect(0, 0, 1024, 768),
                      gfx::CreateMaterialFromImage("textures/uv_test_512.png"));
        painter.ClearScissor();
    }
    virtual std::string GetName() const override
    { return "ScissorTest"; }
    virtual void Update(float dt) override
    {
        mTime += dt;
    }
private:
    float mTime = 0.0f;
};

class ViewportTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        const auto cycle = 2.0f;
        const auto time = fmodf(mTime, cycle);
        const bool clip = time >= (cycle * 0.5);
        if (clip)
            painter.SetViewport(10, 10, 300, 300);

        static auto klass = gfx::CreateMaterialFromImage("textures/uv_test_512.png");
        gfx::FillRect(painter, gfx::FRect(0, 0, 1024, 768), gfx::MaterialClassInst(klass));
    }
    virtual std::string GetName() const override
    { return "ViewportTest"; }
    virtual void Update(float dt) override
    {
        mTime += dt;
    }
private:
    float mTime = 0.0f;
};

// Render nothing test.
class NullTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter&) override
    {}
    virtual std::string GetName() const override
    { return "NullTest"; }
    virtual bool IsFeatureTest() const override
    { return false; }
private:
};

// replication of https://www.vsynctester.com
// alternate between red and cyan, if you see any
// red or cyan then vsync is failing.
class VSyncTest : public GraphicsTest
{
public:
    VSyncTest()
    {
        mColors.push_back(gfx::Color::Red);
        mColors.push_back(gfx::Color::Cyan);
    }
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::DrawTextRect(painter,
          "VSYNC TEST",
          "fonts/AtariFontFullVersion.ttf", 96,
          gfx::FRect(0, 0, 1024, 768),
          mColors[mColorIndex],
          gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter, 0, 1.4f);
        mColorIndex = (mColorIndex + 1) % mColors.size();
    }
    virtual std::string GetName() const override
    { return "VSyncTest"; }
    virtual bool IsFeatureTest() const override
    { return false; }
private:
    std::vector<gfx::Color4f> mColors;
    std::size_t mColorIndex = 0;
};

class MegaParticleTest : public GraphicsTest
{
public:
    MegaParticleTest()
    {
        gfx::KinematicsParticleEngineClass::Params p;
        p.mode = gfx::KinematicsParticleEngineClass::SpawnPolicy::Once;
        p.boundary = gfx::KinematicsParticleEngineClass::BoundaryPolicy::Reflect;
        p.num_particles = 100000;
        p.min_lifetime = std::numeric_limits<float>::max();
        p.max_lifetime = std::numeric_limits<float>::max();
        p.max_xpos = 1.0f;
        p.max_ypos = 1.0f;
        p.init_rect_xpos = 0.0f;
        p.init_rect_ypos = 0.0f;
        p.init_rect_width = 1.0f;
        p.init_rect_height = 1.0f;
        p.direction_sector_start_angle = 0.0f;
        p.direction_sector_size = math::Pi * 2.0;
        p.min_velocity = 0.2;
        p.max_velocity = 0.5;
        p.min_point_size = 2;
        p.max_point_size = 2;
        gfx::KinematicsParticleEngineClass klass(p);
        mEngine.reset(new gfx::KinematicsParticleEngine(p));
    }
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Transform model;
        model.Resize(1024, 768);
        painter.Draw(*mEngine, model, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
    }
    virtual void Update(float dt) override
    {
        if (!mStarted)
            return;

        gfx::Transform transform;
        transform.Resize(1024, 768);
        const auto& model = transform.GetAsMatrix();

        gfx::DrawableClass::Environment e;
        e.model_matrix = &model;
        mEngine->Update(e, dt);
    }
    virtual void Start() override
    {
        gfx::Transform transform;
        transform.Resize(1024, 768);
        const auto& view  = glm::mat4(1.0f);
        const auto& model = transform.GetAsMatrix();

        gfx::DrawableClass::Environment e;
        e.model_matrix = &model;
        mEngine->Restart(e);

        mStarted = true;
    }
    virtual void End() override
    {
        mStarted = false;
    }
    virtual std::string GetName() const override
    { return "MegaParticleTest"; }
    virtual bool IsFeatureTest() const override
    { return false; }
private:
    std::unique_ptr<gfx::KinematicsParticleEngine> mEngine;
    bool mStarted = false;
};

class JankTest : public GraphicsTest
{
public:
    JankTest()
    {
        gfx::KinematicsParticleEngineClass::Params p;
        p.mode = gfx::KinematicsParticleEngineClass::SpawnPolicy::Once;
        p.boundary = gfx::KinematicsParticleEngineClass::BoundaryPolicy::Reflect;
        p.num_particles = 1000;
        p.min_lifetime = std::numeric_limits<float>::max();
        p.max_lifetime = std::numeric_limits<float>::max();
        p.max_xpos = 1.0f;
        p.max_ypos = 1.0f;
        p.init_rect_xpos = 0.0f;
        p.init_rect_ypos = 0.0f;
        p.init_rect_width = 0.0f;
        p.init_rect_height = 1.0f;
        p.direction_sector_start_angle = 0.0f;
        p.direction_sector_size = 0.0f;
        p.min_velocity = 0.2;
        p.max_velocity = 0.2;
        p.min_point_size = 40;
        p.max_point_size = 40;
        {
            gfx::KinematicsParticleEngineClass klass(p);
            mEngine[0].reset(new gfx::KinematicsParticleEngine(klass));
        }

        {
            p.init_rect_xpos = 0.5f;
            gfx::KinematicsParticleEngineClass klass(p);
            mEngine[1].reset(new gfx::KinematicsParticleEngine(klass));
        }
    }
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Transform model;
        model.Resize(1024, 768);
        painter.Draw(*mEngine[0], model, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
        painter.Draw(*mEngine[1], model, gfx::CreateMaterialFromColor(gfx::Color::Green));
    }
    virtual void Update(float dt) override
    {
        gfx::Transform transform;
        transform.Resize(1024, 768);
        const auto& model = transform.GetAsMatrix();

        gfx::DrawableClass::Environment e;
        e.model_matrix = &model;

        mEngine[0]->Update(e, dt);
        mEngine[1]->Update(e, dt);
    }
    virtual void Start() override
    {
        gfx::Transform transform;
        transform.Resize(1024, 768);
        const auto& model = transform.GetAsMatrix();

        gfx::DrawableClass::Environment e;
        e.model_matrix = &model;
        mEngine[0]->Restart(e);
        mEngine[1]->Restart(e);
    }

    virtual std::string GetName() const override
    { return "JankTest"; }
    virtual bool IsFeatureTest() const override
    { return false; }
private:
    std::unique_ptr<gfx::KinematicsParticleEngine> mEngine[2];
};

class PolygonTest : public GraphicsTest
{
public:
    PolygonTest()
    {
        // allow editing
        mPoly.SetDynamic(true);
    }

    virtual void Render(gfx::Painter& painter) override
    {
        mPoly.Clear();
        AddPacman(mPoly, 0.4f, -0.5f, 0.3f);
        AddCircleShape(mPoly, 0.60f, -0.5f, 0.05f);
        AddCircleShape(mPoly, 0.75f, -0.5f, 0.05f);
        AddCircleShape(mPoly, 0.90f, -0.5f, 0.05f);

        // pacman body + food dots
        gfx::Transform transform;
        transform.Resize(500, 500);
        transform.MoveTo(200, 200);

        gfx::GradientClass material;
        material.SetColor(gfx::Color::Yellow, gfx::GradientClass::ColorIndex::TopLeft);
        material.SetColor(gfx::Color::Yellow, gfx::GradientClass::ColorIndex::TopRight);
        material.SetColor(gfx::Color::Black,  gfx::GradientClass::ColorIndex::BottomLeft);
        material.SetColor(gfx::Color::Yellow, gfx::GradientClass::ColorIndex::BottomRight);
        material.SetGamma(2.2f);
        painter.Draw(gfx::Polygon(mPoly), transform, gfx::MaterialClassInst(material));

        // eye
        transform.Resize(40, 40);
        transform.MoveTo(430, 350);
        painter.Draw(gfx::Circle(), transform, gfx::CreateMaterialFromColor(gfx::Color::Black));

        // chomp text when mouth is nearly closed
        const auto mouth = (std::sin(mTime) + 1.0f) / 2.0f * 15;
        if (mouth <= 5)
        {
            gfx::DrawTextRect(painter, "Chomp!",
                "fonts/AtariFontFullVersion.ttf", 30,
                gfx::FRect(500, 200, 200, 50),
                gfx::Color::DarkYellow, gfx::TextAlign::AlignBottom, 0, 1.4f);
        }
    }
    virtual std::string GetName() const override
    { return "PolygonTest"; }

    virtual void Update(float dts) override
    {
        const float velocity = 5.23;
        mTime += dts * velocity;
    }
private:
    void AddPacman(gfx::PolygonClass& poly,  float x, float y, float r)
    {
        gfx::PolygonClass::Vertex center = {
            {x, y}, {x, -y}
        };
        std::vector<gfx::PolygonClass::Vertex> verts;
        verts.push_back(center);

        const auto slices = 200;
        const auto angle = (math::Pi * 2) / slices;
        const auto mouth = (std::sin(mTime) + 1.0f) / 2.0f * 15;
        for (int i=mouth; i<=slices-mouth; ++i)
        {
            const float a = i * angle;
            gfx::PolygonClass::Vertex v;
            v.aPosition.x = x + std::cos(a) * r;
            v.aPosition.y = y + std::sin(a) * r;
            v.aTexCoord.x = v.aPosition.x;
            v.aTexCoord.y = v.aPosition.y * -1.0f;
            verts.push_back(v);
        }
        gfx::PolygonClass::DrawCommand cmd;
        cmd.type   = gfx::PolygonClass::DrawType::TriangleFan;
        cmd.offset = poly.GetNumVertices();
        cmd.count  = verts.size();
        poly.AddVertices(std::move(verts));
        poly.AddDrawCommand(cmd);
    }

    void AddCircleShape(gfx::PolygonClass& poly,  float x, float y, float r)
    {
        gfx::PolygonClass::Vertex center = {
            {x, y}, {x, -y}
        };
        std::vector<gfx::PolygonClass::Vertex> verts;
        verts.push_back(center);

        const auto slices = 200;
        const auto angle = (math::Pi * 2) / slices;
        for (int i=0; i<=slices; ++i)
        {
            const float a = i * angle;
            gfx::PolygonClass::Vertex v;
            v.aPosition.x = x + std::cos(a) * r;
            v.aPosition.y = y + std::sin(a) * r;
            v.aTexCoord.x = v.aPosition.x;
            v.aTexCoord.y = v.aPosition.y * -1.0f;
            verts.push_back(v);
        }
        gfx::PolygonClass::DrawCommand cmd;
        cmd.type   = gfx::PolygonClass::DrawType::TriangleFan;
        cmd.offset = poly.GetNumVertices();
        cmd.count  = verts.size();
        poly.AddVertices(std::move(verts));
        poly.AddDrawCommand(cmd);
    }
private:
    float mTime = 0.0f;
    gfx::PolygonClass mPoly;
};

class TileBatchTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        const auto tile_size = 50.0f;

        {
            gfx::TileBatch tiles;
            tiles.SetTileWidth(tile_size);
            tiles.SetTileHeight(tile_size);
            for (unsigned row=0; row<10; ++row)
            {
                for (unsigned col=0; col<10; ++col)
                {
                    gfx::TileBatch::Tile tile;
                    tile.pos.y = row;
                    tile.pos.x = col;
                    tiles.AddTile(std::move(tile));
                }
            }
            gfx::Transform trans;
            trans.MoveTo(100, 100);
            trans.Resize(1.0f, 1.0f);
            painter.Draw(tiles, trans, gfx::CreateMaterialFromColor(gfx::Color::DarkGray));
        }

        {

            gfx::Transform trans;
            trans.MoveTo(100, 100);
            trans.Resize(tile_size * 10, tile_size * 10);
            painter.Draw(gfx::Grid(10, 10, true), trans, gfx::CreateMaterialFromColor(gfx::Color::Green));
        }
    }
    virtual std::string GetName() const override
    { return "TileBatchTest"; }
private:
};

class StencilTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        // draw a gradient in the background
        {
            gfx::GradientClass material;
            material.SetColor(gfx::Color::Red,   gfx::GradientClass::ColorIndex::TopLeft);
            material.SetColor(gfx::Color::Green, gfx::GradientClass::ColorIndex::BottomLeft);
            material.SetColor(gfx::Color::Blue,  gfx::GradientClass::ColorIndex::BottomRight);
            material.SetColor(gfx::Color::Black, gfx::GradientClass::ColorIndex::TopRight);
            material.SetGamma(2.2f);
            gfx::Transform transform;
            transform.Resize(1024, 768);
            painter.Draw(gfx::Rectangle(), transform, gfx::MaterialClassInst(material));
        }

        {
            gfx::TextureMap2DClass material;
            material.SetTexture(gfx::LoadTextureFromFile("textures/Checkerboard.png"));
            gfx::Transform mask;
            mask.Resize(400, 400);
            mask.Translate(200 + std::cos(mTime) * 200, 200 + std::sin(mTime) * 200);

            // Clear stencil to all 1s and then write to 0 when fragment is written.
            gfx::MaterialClassInst material_instance(material);

            const gfx::StencilMaskPass stencil(1, 0, painter);
            stencil.Draw(gfx::Circle(), mask, material_instance);

            // write fragments only where stencil has value 1
            const gfx::StencilTestColorWritePass cover(1, painter);

            gfx::Transform shape;
            shape.Resize(1024, 768);
            cover.Draw(gfx::Rectangle(), shape, material_instance);
        }
    }
    virtual void Update(float dts) override
    {
        const float velocity = 1.23;
        mTime += dts * velocity;
    }
    virtual std::string GetName() const override
    { return "StencilTest"; }
private:
    float mTime = 0.0f;

};

class TextureBlurTest : public GraphicsTest
{
public:
    TextureBlurTest()
    {
        gfx::TextureMap2DClass material;
        material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);

        {
            auto source = gfx::LoadTextureFromFile("textures/bird/bird-1024x1024.png");
            source->SetName("bird-1024x1024.png (blur)");
            source->SetEffect(gfx::TextureSource::Effect::Blur, true);
            material.SetTexture(std::move(source));
            material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            mBlur1024x1024 = gfx::CreateMaterialInstance(material);
        }
        {
            auto source = gfx::LoadTextureFromFile("textures/bird/bird-1024x1024.png");
            source->SetName("bird-1024x1024.png (none)");
            material.SetTexture(std::move(source));
            material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            mClear1024x1024 = gfx::CreateMaterialInstance(material);
        }

        {
            auto source = gfx::LoadTextureFromFile("textures/bird/bird-512x512.png");
            source->SetName("bird-512x512.png (blur)");
            source->SetEffect(gfx::TextureSource::Effect::Blur, true);
            material.SetTexture(std::move(source));
            material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            mBlur512x512 = gfx::CreateMaterialInstance(material);
        }
        {
            auto source = gfx::LoadTextureFromFile("textures/bird/bird-512x512.png");
            source->SetName("bird-512x512.png (none)");
            material.SetTexture(std::move(source));
            material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            mClear512x512 = gfx::CreateMaterialInstance(material);
        }

        {
            auto source = gfx::LoadTextureFromFile("textures/bird/bird-256x256.png");
            source->SetName("bird-256x256.png (blur)");
            source->SetEffect(gfx::TextureSource::Effect::Blur, true);
            material.SetTexture(std::move(source));
            material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            mBlur256x256 = gfx::CreateMaterialInstance(material);
        }
        {
            auto source = gfx::LoadTextureFromFile("textures/bird/bird-256x256.png");
            source->SetName("bird-256x256.png (none)");
            material.SetTexture(std::move(source));
            material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            mClear256x256 = gfx::CreateMaterialInstance(material);
        }
    }

    virtual void Render(gfx::Painter& painter) override
    {
        gfx::FillRect(painter, gfx::FRect(100, 100, 256, 256), *mBlur1024x1024);
        gfx::FillRect(painter, gfx::FRect(100, 400, 256, 256), *mClear1024x1024);

        gfx::FillRect(painter, gfx::FRect(400, 100, 256, 256), *mBlur512x512);
        gfx::FillRect(painter, gfx::FRect(400, 400, 256, 256), *mClear512x512);

        gfx::FillRect(painter, gfx::FRect(700, 100, 256, 256), *mBlur256x256);
        gfx::FillRect(painter, gfx::FRect(700, 400, 256, 256), *mClear256x256);
    }
    virtual std::string GetName() const override
    { return "TextureBlurTest"; }
private:
    std::unique_ptr<gfx::Material> mBlur1024x1024;
    std::unique_ptr<gfx::Material> mBlur512x512;
    std::unique_ptr<gfx::Material> mBlur256x256;

    std::unique_ptr<gfx::Material> mClear1024x1024;
    std::unique_ptr<gfx::Material> mClear512x512;
    std::unique_ptr<gfx::Material> mClear256x256;
};

class GradientTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::GradientClass material;
        material.SetColor(gfx::Color::Red,   gfx::GradientClass::ColorIndex::TopLeft);
        material.SetColor(gfx::Color::Green, gfx::GradientClass::ColorIndex::BottomLeft);
        material.SetColor(gfx::Color::Blue,  gfx::GradientClass::ColorIndex::BottomRight);
        material.SetColor(gfx::Color::Black, gfx::GradientClass::ColorIndex::TopRight);
        material.SetGamma(2.2f);
        gfx::FillRect(painter, gfx::FRect(0, 0, 400, 400), gfx::MaterialClassInst(material));

        // *perceptually* linear gradient ramp
        material.SetColor(gfx::Color::Black,   gfx::GradientClass::ColorIndex::TopLeft);
        material.SetColor(gfx::Color::Black, gfx::GradientClass::ColorIndex::BottomLeft);
        material.SetColor(gfx::Color::White,  gfx::GradientClass::ColorIndex::BottomRight);
        material.SetColor(gfx::Color::White, gfx::GradientClass::ColorIndex::TopRight);
        gfx::FillRect(painter, gfx::FRect(500, 20, 400, 100), gfx::MaterialClassInst(material));

        material.SetOffset(glm::vec2(0.75, 0.0f));
        gfx::FillRect(painter, gfx::FRect(500, 140, 400, 100), gfx::MaterialClassInst(material));

        material.SetOffset(glm::vec2(0.25, 0.0f));
        gfx::FillRect(painter, gfx::FRect(500, 260, 400, 100), gfx::MaterialClassInst(material));
    }
    virtual std::string GetName() const override
    { return "GradientTest"; }
private:
};

class TextureTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        // whole texture (box = 1.0f)
        {
            gfx::TextureMap2DClass material;
            material.SetTexture(gfx::LoadTextureFromFile("textures/uv_test_512.png"));
            material.SetTextureRect( gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            gfx::FillRect(painter, gfx::FRect(0, 0, 128, 128), gfx::MaterialClassInst(material));

            material.SetTextureScaleX(2.0);
            material.SetTextureScaleY(2.0);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(150, 0, 128, 128), gfx::MaterialClassInst(material));

            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(300, 0, 128, 128), gfx::MaterialClassInst(material));

            material.SetTextureScaleX(-2.0);
            material.SetTextureScaleY(-2.0);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(450, 0, 128, 128), gfx::MaterialClassInst(material));
        }

        // texture box > 1.0
        // todo: maybe just limit the box to 0.0, 1.0 range and dismiss this case ?
        {
            // clamp
            gfx::TextureMap2DClass material;
            material.SetTexture(gfx::LoadTextureFromFile("textures/uv_test_512.png"));
            material.SetTextureRect(gfx::FRect(0.0, 0.0, 2.0, 1.0));
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(0, 150, 128, 128), gfx::MaterialClassInst(material));

            material.SetTextureRect(gfx::FRect(0.0, 0.0, 2.0, 2.0));
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(150, 150, 128, 128), gfx::MaterialClassInst(material));

            material.SetTextureRect(gfx::FRect(0.0, 0.0, 2.0, 2.0));
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(300, 150, 128, 128), gfx::MaterialClassInst(material));

        }

        // texture box < 1.0
        {
            // basic case. sampling within the box.
            gfx::TextureMap2DClass material;
            material.SetTexture(gfx::LoadTextureFromFile("textures/uv_test_512.png"));
            material.SetTextureRect(gfx::FRect(0.5, 0.5, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(0, 300, 128, 128), gfx::MaterialClassInst(material));

            // clamping with texture boxing.
            material.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Linear);
            material.SetTextureRect(gfx::FRect(0.0, 0.0, 0.5, 0.5));
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureScaleX(2.0);
            material.SetTextureScaleY(2.0);
            gfx::FillRect(painter, gfx::FRect(150, 300, 128, 128), gfx::MaterialClassInst(material));

            // should be 4 squares each brick color (the top left quadrant of the source texture)
            material.SetTextureRect(gfx::FRect(0.0, 0.0, 0.5, 0.5));
            material.SetTextureScaleX(2.0);
            material.SetTextureScaleY(2.0);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(300, 300, 128, 128), gfx::MaterialClassInst(material));

            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            material.SetTextureScaleX(1.0f);
            material.SetTextureScaleY(1.0f);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(450, 300, 128, 128), gfx::MaterialClassInst(material));

            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            material.SetTextureScaleX(2.0f);
            material.SetTextureScaleY(2.0f);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(600, 300, 128, 128), gfx::MaterialClassInst(material));

            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            material.SetTextureScaleX(2.0f);
            material.SetTextureScaleY(2.0f);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(750, 300, 128, 128), gfx::MaterialClassInst(material));
        }

        // texture velocity + rotation
        {
            gfx::TextureMap2DClass material;
            material.SetTexture(gfx::LoadTextureFromFile("textures/uv_test_512.png"));
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureVelocityX(0.2);
            gfx::FillRect(painter, gfx::FRect(0, 450, 128, 128), gfx::MaterialClassInst(material, mTime));

            material.SetTextureVelocityX(0.0);
            material.SetTextureVelocityY(0.2);
            gfx::FillRect(painter, gfx::FRect(150, 450, 128, 128), gfx::MaterialClassInst(material, mTime));

            material.SetTextureVelocityX(0.25);
            material.SetTextureVelocityY(0.2);
            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(300, 450, 128, 128), gfx::MaterialClassInst(material, mTime));

            material.SetTextureVelocityX(0.0f);
            material.SetTextureVelocityY(0.0f);
            material.SetTextureVelocityZ(3.134);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            gfx::FillRect(painter, gfx::FRect(450, 450, 128, 128), gfx::MaterialClassInst(material, mTime));

            material.SetTextureVelocityX(0.0f);
            material.SetTextureVelocityY(0.0f);
            material.SetTextureVelocityZ(-3.134);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            gfx::FillRect(painter, gfx::FRect(600, 450, 128, 128), gfx::MaterialClassInst(material, mTime));

            material.SetTextureVelocityX(0.0f);
            material.SetTextureVelocityY(0.0f);
            material.SetTextureVelocityZ(0.0f);
            material.SetTextureRotation(0.25f * math::Pi);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            gfx::FillRect(painter, gfx::FRect(750, 450, 128, 128), gfx::MaterialClassInst(material, mTime));
        }
    }
    virtual void Update(float dt) override
    {
        mTime += dt;
    }

    virtual std::string GetName() const override
    { return "TextureTest"; }
private:
    float mTime = 0.0f;
};

class SpriteTest : public GraphicsTest
{
public:
    SpriteTest()
    {
        mMaterial = std::make_shared<gfx::SpriteClass>();
        mMaterial->SetSurfaceType(gfx::MaterialClass::SurfaceType::Opaque);
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-1.png")).SetName("frame-1.png");
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-2.png")).SetName("frame-2.png");
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-3.png")).SetName("frame-3.png");
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-4.png")).SetName("frame-4.png");
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-5.png")).SetName("frame-5.png");
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-6.png")).SetName("frame-6.png");
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-7.png")).SetName("frame-7.png");
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-8.png")).SetName("frame-8.png");
        mMaterial->SetBlendFrames(false);
        mMaterial->SetFps(10.0f);
    }

    virtual void Render(gfx::Painter& painter) override
    {
        // instance
        gfx::MaterialClassInst material(mMaterial);
        material.SetRuntime(mTime);

        // whole texture (box = 1.0f)
        {
            SetRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));

            mMaterial->SetTextureScaleX(1.0);
            mMaterial->SetTextureScaleY(1.0);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(0, 0, 128, 128), material);

            mMaterial->SetTextureScaleX(2.0);
            mMaterial->SetTextureScaleY(2.0);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(150, 0, 128, 128), material);

            mMaterial->SetTextureScaleX(2.0);
            mMaterial->SetTextureScaleY(2.0);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(300, 0, 128, 128), material);

            mMaterial->SetTextureScaleX(-2.0);
            mMaterial->SetTextureScaleY(-2.0);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(450, 0, 128, 128), material);
        }

        // texture box > 1.0
        // todo: maybe just limit the box to 0.0, 1.0 range and dismiss this case ?
        {

            SetRect(gfx::FRect(0.0f, 0.0f, 2.0f, 1.0f));
            mMaterial->SetTextureScaleX(1.0);
            mMaterial->SetTextureScaleY(1.0);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(0, 150, 128, 128), material);

            SetRect(gfx::FRect(0.0, 0.0, 2.0, 2.0));
            mMaterial->SetTextureScaleX(1.0);
            mMaterial->SetTextureScaleY(1.0);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(150, 150, 128, 128), material);

            SetRect(gfx::FRect(0.0, 0.0, 2.0, 2.0));
            mMaterial->SetTextureScaleX(1.0);
            mMaterial->SetTextureScaleY(1.0);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(300, 150, 128, 128), material);
        }

        // texture box < 1.0
        {
            // basic case. sampling within the box.
            SetRect(gfx::FRect(0.5f, 0.5f, 0.5f, 0.5f));
            mMaterial->SetTextureScaleX(1.0);
            mMaterial->SetTextureScaleY(1.0);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(0, 300, 128, 128), material);

            // clamping with texture boxing.
            SetRect(gfx::FRect(0.0, 0.0, 0.5, 0.5));
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureScaleX(2.0);
            mMaterial->SetTextureScaleY(2.0);
            gfx::FillRect(painter, gfx::FRect(150, 300, 128, 128),material);

            // should be 4 squares each brick color (the top left quadrant of the source texture)
            SetRect(gfx::FRect(0.0, 0.0, 0.5, 0.5));
            mMaterial->SetTextureScaleX(2.0);
            mMaterial->SetTextureScaleY(2.0);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(300, 300, 128, 128), material);

            SetRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            mMaterial->SetTextureScaleX(1.0f);
            mMaterial->SetTextureScaleY(1.0f);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(450, 300, 128, 128), material);

            SetRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            mMaterial->SetTextureScaleX(2.0f);
            mMaterial->SetTextureScaleY(2.0f);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(600, 300, 128, 128), material);

            SetRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            mMaterial->SetTextureScaleX(2.0f);
            mMaterial->SetTextureScaleY(2.0f);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(750, 300, 128, 128), material);
        }

        // texture velocity + rotation
        {
            mMaterial->SetTextureScaleX(1.0f);
            mMaterial->SetTextureScaleY(1.0f);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            SetRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));

            mMaterial->SetTextureVelocityX(0.2);
            mMaterial->SetTextureVelocityY(0.0f);
            gfx::FillRect(painter, gfx::FRect(0, 450, 128, 128), material);

            mMaterial->SetTextureVelocityX(0.0);
            mMaterial->SetTextureVelocityY(0.2);
            gfx::FillRect(painter, gfx::FRect(150, 450, 128, 128), material);

            SetRect(gfx::FRect(0.25f, 0.25f, 0.5f, 0.5f));
            mMaterial->SetTextureVelocityX(0.25);
            mMaterial->SetTextureVelocityY(0.2);
            gfx::FillRect(painter, gfx::FRect(300, 450, 128, 128), material);

            SetRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            mMaterial->SetTextureVelocityX(0.0f);
            mMaterial->SetTextureVelocityY(0.0f);
            mMaterial->SetTextureVelocityZ(3.134);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(450, 450, 128, 128), material);

            SetRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            mMaterial->SetTextureVelocityX(0.0f);
            mMaterial->SetTextureVelocityY(0.0f);
            mMaterial->SetTextureVelocityZ(-3.134);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(600, 450, 128, 128), material);

            SetRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            mMaterial->SetTextureVelocityX(0.0f);
            mMaterial->SetTextureVelocityY(0.0f);
            mMaterial->SetTextureVelocityZ(0.0f);
            mMaterial->SetTextureRotation(0.25 * math::Pi);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(750, 450, 128, 128), material);
        }

        mMaterial->SetTextureVelocityX(0.0f);
        mMaterial->SetTextureVelocityY(0.0f);
        mMaterial->SetTextureVelocityZ(0.0f);
        mMaterial->SetTextureScaleX(1.0f);
        mMaterial->SetTextureScaleY(1.0f);
        mMaterial->SetTextureRotation(0.0f);
        SetRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));

    }

    virtual void Update(float dt) override
    { mTime += dt; }
    virtual std::string GetName() const override
    { return "SpriteTest"; }
private:
    void SetRect(const gfx::FRect& rect)
    {
        for (size_t i=0; i<mMaterial->GetNumTextures(); ++i)
            mMaterial->SetTextureRect(i, rect);
    }

private:
    std::shared_ptr<gfx::SpriteClass> mMaterial;
    float mTime = 0.0f;
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
        // which is transformed relative to the top level "view" transform.
        // A call to push begins a new "scope" for a transformations
        // and subsequent operations combine into a single transformation matrix.
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
                painter.Draw(gfx::Rectangle(), tr, gfx::CreateMaterialFromColor(gfx::Color::Cyan));
            tr.Pop();

            // begin transformation scope for rectangle B.
            tr.Push();
                // first translate then rotate
                tr.Translate(30.0f, 30.0f);
                tr.Rotate(math::Pi * 2.0f * angle);
                tr.Push();
                    tr.Scale(20.0f, 20.0f);
                    painter.Draw(gfx::Rectangle(), tr, gfx::CreateMaterialFromColor(gfx::Color::Yellow));
                tr.Pop();
            tr.Pop();
        tr.Pop();
    }
    virtual void Update(float dt) override
    {
        mRobot->Update(dt);
        mTime += dt;
    }
    virtual std::string GetName() const override
    { return "TransformTest"; }
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
                painter.Draw(gfx::Rectangle(), trans, gfx::CreateMaterialFromColor(mColor));
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

template<typename Shape>
class ShapeTest : public GraphicsTest
{
public:
    ShapeTest(const std::string& name) : mName(name)
    {}
    virtual void Render(gfx::Painter& painter) override
    {
        auto klass = gfx::CreateMaterialClassFromImage("textures/uv_test_512.png");
        // in order to validate the texture coordinates let's set
        // the filtering to nearest and clamp to edge on sampling
        klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Nearest);
        klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Nearest);
        klass.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
        klass.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
        gfx::MaterialClassInst material(klass);

        gfx::Transform transform;
        transform.Scale(200.0f, 200.0f);

        transform.Translate(10.0f, 10.0f);
        painter.Draw(Shape(gfx::Drawable::Style::Wireframe), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::Drawable::Style::Outline), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::Drawable::Style::Solid), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::Drawable::Style::Outline, 10.0f), transform, material);

        transform.MoveTo(10.0f, 250.0f);
        transform.Resize(200.0f, 100.0f);
        painter.Draw(Shape(gfx::Drawable::Style::Wireframe), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::Drawable::Style::Outline), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::Drawable::Style::Solid), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::Drawable::Style::Wireframe, 2.0f), transform, material);

        transform.MoveTo(60.0f, 400.0f);
        transform.Resize(100.0f, 200.0f);
        painter.Draw(Shape(gfx::Drawable::Style::Wireframe), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::Drawable::Style::Outline), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::Drawable::Style::Solid), transform, material);
    }
    virtual void Update(float dt) override
    {}
    virtual std::string GetName() const override
    { return mName; }
private:
    const std::string mName;
};

class RenderParticleTest : public GraphicsTest
{
public:
    using ParticleEngine = gfx::KinematicsParticleEngine;

    RenderParticleTest()
    {
        {
            gfx::KinematicsParticleEngineClass::Params p;
            p.mode = gfx::KinematicsParticleEngineClass::SpawnPolicy::Continuous;
            p.boundary = gfx::KinematicsParticleEngineClass::BoundaryPolicy::Kill;
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
            mFire.reset(new gfx::KinematicsParticleEngine(p));
        }

        {
            gfx::KinematicsParticleEngineClass::Params p;
            p.mode = gfx::KinematicsParticleEngineClass::SpawnPolicy::Continuous;
            p.boundary = gfx::KinematicsParticleEngineClass::BoundaryPolicy::Kill;
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
            mSmoke.reset(new gfx::KinematicsParticleEngine(p));
        }

        {
            gfx::KinematicsParticleEngineClass::Params p;
            p.mode = gfx::KinematicsParticleEngineClass::SpawnPolicy::Continuous;
            p.boundary = gfx::KinematicsParticleEngineClass::BoundaryPolicy::Kill;
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
            mBlood.reset(new gfx::KinematicsParticleEngine(p));
        }

        {
            gfx::KinematicsParticleEngineClass::Params p;
            p.mode = gfx::KinematicsParticleEngineClass::SpawnPolicy::Continuous;
            p.boundary = gfx::KinematicsParticleEngineClass::BoundaryPolicy::Kill;
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
            mClouds.reset(new gfx::KinematicsParticleEngine(p));
        }

    }

    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Transform model;
        model.Resize(300, 300);
        model.Translate(-150, -150);
        model.Rotate(math::Pi);
        model.Translate(150 + 100, 150 + 300);

        gfx::TextureMap2DClass material;
        material.SetTexture(gfx::LoadTextureFromFile("textures/BlackSmoke.png"));
        material.SetBaseColor(gfx::Color4f(35, 35, 35, 20));
        material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        painter.Draw(*mSmoke, model, gfx::MaterialClassInst(material));

        material.SetBaseColor(gfx::Color4f(0x71, 0x38, 0x0, 0xff));
        material.SetTexture(gfx::LoadTextureFromFile("textures/BlackSmoke.png"));
        material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Emissive);
        painter.Draw(*mFire, model, gfx::MaterialClassInst(material));

        material.SetBaseColor(gfx::Color4f(234, 5, 3, 255));
        material.SetTexture(gfx::LoadTextureFromFile("textures/RoundParticle.png"));
        material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        model.Translate(500, 0);
        painter.Draw(*mBlood, model, gfx::MaterialClassInst(material));

        material.SetBaseColor(gfx::Color4f(224, 224, 224, 255));
        material.SetTexture(gfx::LoadTextureFromFile("textures/WhiteCloud.png"));
        material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);

        model.Reset();
        model.Resize(2000, 200);
        model.MoveTo(-100, 100);
        painter.Draw(*mClouds, model, gfx::MaterialClassInst(material));
    }
    virtual void Update(float dt) override
    {
        // todo: setup the model matrices properly. keep in mind that
        // the model matrix needs to change per each particle engine
        gfx::DrawableClass::Environment e;
        e.model_matrix = nullptr;

        mFire->Update(e, dt);
        mSmoke->Update(e, dt);
        mBlood->Update(e, dt);
        mClouds->Update(e, dt);
    }
    virtual void Start() override
    {
        // todo: setup the model matrices properly. keep in mind that
        // the model matrix needs to change per each particle engine
        gfx::DrawableClass::Environment e;
        e.model_matrix = nullptr;

        mFire->Restart(e);
        mSmoke->Restart(e);
        mBlood->Restart(e);
        mClouds->Restart(e);
    }

    virtual std::string GetName() const override
    { return "ParticleTest"; }
private:
    std::unique_ptr<ParticleEngine> mFire;
    std::unique_ptr<ParticleEngine> mSmoke;
    std::unique_ptr<ParticleEngine> mBlood;
    std::unique_ptr<ParticleEngine> mClouds;
    float mTime = 0.0f;
};

class TextRectScaleTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        const auto width  = 600.0f + 600 * std::sin(mAngle);
        const auto height = 600.0f + 600 * std::sin(mAngle);

        gfx::FRect rect;
        rect.Resize(width, height);
        rect.Move(1024.0f/2, 768.0f/2);
        rect.Translate(-width/2, -height/2);

        gfx::FillRect(painter, rect, gfx::Color::DarkGray);
        gfx::DrawTextRect(painter,
            "Lorem ipsum dolor sit amet, consectetur adipiscing\n"
            "elit, sed do eiusmod tempor incididunt ut labore et\n"
            "dolore magna aliqua. Ut enim ad minim veniam, quis\n"
            "nostrud exercitation ullamco laboris nisi ut aliquip\n"
            "ex ea commodo consequat.",
            "fonts/Cousine-Regular.ttf", 20,
            rect,
            gfx::Color::Black);
    }
    virtual std::string GetName() const override
    { return "TextRectScaleTest"; }

    virtual void Update(float dt) override
    {
        // full circle in 2s
        const auto angular_velo = math::Pi;
        mAngle += (angular_velo * dt);
    }
    virtual bool IsFeatureTest() const override
    { return false; }
private:
    float mAngle = 0.0f;
};

class TextAlignTest : public GraphicsTest
{
public:
    TextAlignTest(const std::string& name, const std::string& font, const gfx::Color4f& color, unsigned size)
      : mName(name)
      , mFont(font)
      , mColor(color)
      , mFontSize(size)
    {}

    virtual void Render(gfx::Painter& painter) override
    {
        const auto cycle = 2.0f;
        const auto reminder = fmodf(mTime, cycle);
        const bool show_box = reminder >= 1.0f;

        // top row
        gfx::DrawTextRect(painter,
            "Left,top\naligned\ntext",
            mFont, mFontSize,
            gfx::FRect(50, 50, 200, 200),
            mColor,
            gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignTop, 0, 1.4f);
        if (show_box)
            gfx::DrawRectOutline(painter, gfx::FRect(50, 50, 200, 200), gfx::Color::HotPink, 1.0f);

        gfx::DrawTextRect(painter,
            "Center,top\naligned\ntext",
            mFont, mFontSize,
            gfx::FRect(300, 50, 200, 200),
            mColor,
            gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignTop, 0, 1.4f);
        if (show_box)
            gfx::DrawRectOutline(painter, gfx::FRect(300, 50, 200, 200), gfx::Color::HotPink, 1.0f);

        gfx::DrawTextRect(painter,
            "Right,top\naligned\ntext",
            mFont, mFontSize,
            gfx::FRect(550, 50, 200, 200),
            mColor,
            gfx::TextAlign::AlignRight | gfx::TextAlign::AlignTop, 0, 1.4f);
        if (show_box)
            gfx::DrawRectOutline(painter, gfx::FRect(550, 50, 200, 200), gfx::Color::HotPink, 1.0f);

        // middle row
        gfx::DrawTextRect(painter,
            "Left,center\naligned\ntext",
            mFont, mFontSize,
            gfx::FRect(50, 300, 200, 200),
            mColor,
            gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter, 0, 1.4f);
        if (show_box)
            gfx::DrawRectOutline(painter, gfx::FRect(50, 300, 200, 200), gfx::Color::HotPink, 1.0f);

        gfx::DrawTextRect(painter,
            "Center,center\naligned\ntext",
            mFont, mFontSize,
            gfx::FRect(300, 300, 200, 200),
            mColor,
            gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter, 0, 1.4f);
        if (show_box)
            gfx::DrawRectOutline(painter, gfx::FRect(300, 300, 200, 200), gfx::Color::HotPink, 1.0f);

        gfx::DrawTextRect(painter,
            "Right,center\naligned\ntext",
            mFont, mFontSize,
            gfx::FRect(550, 300, 200, 200),
            mColor,
            gfx::TextAlign::AlignRight | gfx::TextAlign::AlignVCenter, 0, 1.4f);
        if (show_box)
            gfx::DrawRectOutline(painter, gfx::FRect(550, 300, 200, 200), gfx::Color::HotPink, 1.0f);

        // bottom row
        gfx::DrawTextRect(painter,
            "Left,bottom\naligned\ntext",
            mFont, mFontSize,
            gfx::FRect(50, 550, 200, 200),
            mColor,
            gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignBottom, 0, 1.4f);
        if (show_box)
            gfx::DrawRectOutline(painter, gfx::FRect(50, 550, 200, 200), gfx::Color::HotPink, 1.0f);

        gfx::DrawTextRect(painter,
            "Center,bottom\naligned\ntext",
            mFont, mFontSize,
            gfx::FRect(300, 550, 200, 200),
            mColor,
            gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignBottom, 0, 1.4f);
        if (show_box)
            gfx::DrawRectOutline(painter, gfx::FRect(300, 550, 200, 200), gfx::Color::HotPink, 1.0f);

        gfx::DrawTextRect(painter,
            "Right,bottom\naligned\ntext",
             mFont, mFontSize,
            gfx::FRect(550, 550, 200, 200),
            mColor,
            gfx::TextAlign::AlignRight | gfx::TextAlign::AlignBottom, 0, 1.4f);
        if (show_box)
            gfx::DrawRectOutline(painter, gfx::FRect(550, 550, 200, 200), gfx::Color::HotPink, 1.0f);

        gfx::DrawTextRect(painter,
            "clipclipclipclipclip\n"
            "clipclipclipclipclip\n"
            "clipclipclipclipclip\n"
            "clipclipclipclipclip\n"
            "clipclipclipclipclip\n"
            "clipclipclipclipclip\n"
            "clipclipclipclipclip\n"
            "clipclipclipclipclip\n"
            "clipclipclipclipclip\n"
            "clipclipclipclipclip\n"
            "clipclipclipclipclip\n"
            "clipclipclipclipclip\n"
            "clipclipclipclipclip\n",
            mFont, mFontSize,
            gfx::FRect(800, 50, 173, 173),
            mColor,
            gfx::TextAlign::AlignRight | gfx::TextAlign::AlignBottom);
        if (show_box)
            gfx::DrawRectOutline(painter, gfx::FRect(800, 50, 173, 173), gfx::Color::HotPink, 1.0f);
    }
    virtual std::string GetName() const override
    { return mName;  }

    virtual void Update(float dt) override
    {
        mTime += dt;
    }
private:
    const std::string mName;
    const std::string mFont;
    const gfx::Color4f mColor;
    unsigned mFontSize = 0;
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
            gfx::FRect(0, 0, 1024, 150),
            gfx::Color::DarkGray,
            gfx::TextAlign::AlignVCenter | gfx::TextAlign::AlignHCenter,
            0, 1.4f);

        gfx::DrawTextRect(painter,
            "Cousine-Regular.ttf, 20px\n"
            "Hello World!\n"
            "1234567890\n"
            "!£$/[]}?,._-<>\n",
            "fonts/Cousine-Regular.ttf", 20,
            gfx::FRect(0, 150, 1024, 100),
            gfx::Color::DarkGray,
            gfx::TextAlign::AlignVCenter | gfx::TextAlign::AlignHCenter);

        gfx::DrawTextRect(painter,
            "Cousine-Bold.ttf, 16px\n"
            "Hello World!\n"
            "1234567890\n"
            "!£$/[]}?,._-<>\n",
            "fonts/Cousine-Bold.ttf", 16,
            gfx::FRect(0, 250, 1024, 100),
            gfx::Color::DarkGray);

        gfx::DrawTextRect(painter,
            "Cousine-Italic.ttf, 16px\n"
            "Hello World!\n"
            "1234567890\n"
            "!£$/[]}?,._-<>\n",
            "fonts/Cousine-Italic.ttf", 16,
            gfx::FRect(0, 350, 1024, 100),
            gfx::Color::DarkGray);

        gfx::DrawTextRect(painter,
            "Hello world!\n"
            "123456789\n"
            "#+![]",
            "fonts/nuskool_krome_64x64.json", 32,
             gfx::FRect(0, 450, 1024, 100),
             gfx::Color::White);

        gfx::DrawTextRect(painter,
            "Underlined text",
            "fonts/AtariFontFullVersion.ttf", 18,
            gfx::FRect(0, 50, 300, 100),
            gfx::Color::DarkGray,
            gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter,
            gfx::TextProp::Underline, 1.4f);

        gfx::DrawTextRect(painter,
            "Blinking text",
            "fonts/AtariFontFullVersion.ttf", 18,
            gfx::FRect(0, 100, 300, 100),
            gfx::Color::DarkGray,
            gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter,
            gfx::TextProp::Blinking, 1.4f);

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
            buff.SetText("Hello World!", "fonts/AtariFontFullVersion.ttf", 20);

            gfx::TextMaterial material(std::move(buff));
            material.SetColor(gfx::Color::DarkGray);
            painter.Draw(gfx::Rectangle(), transform, material);
        }

        // modulate text color based on time
        {
            const float r = (std::sin(angle + 0.1*circle) + 1.0) * 0.5;
            const float g = (std::cos(angle + 0.2*circle) + 1.0) * 0.5;
            const float b = (std::sin(angle + 0.3*circle) + 1.0) * 0.5;

            gfx::DrawTextRect(painter,
                "Very colorful text",
                "fonts/AtariFontFullVersion.ttf", 20,
                gfx::FRect(0, 600, 1024, 100),
                gfx::Color4f(r, g, b, 1.0f));
        }
    }
    virtual void Update(float dts) override
    {
        mTime += dts;
    }
    virtual std::string GetName() const override
    { return "TextTest"; }
private:
    float mTime = 0.0f;
};

class FillShapeTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::FRect rect(10, 10, 100, 140);
        gfx::FillShape(painter, rect,  gfx::Rectangle(), gfx::Color::DarkGreen);
        rect.Translate(150, 0);
        gfx::FillShape(painter, rect, gfx::RoundRectangle(), gfx::Color::DarkGreen);
        rect.Translate(150, 0);
        gfx::FillShape(painter, rect, gfx::Trapezoid(), gfx::Color::DarkGreen);
        rect.Translate(150, 0);
        gfx::FillShape(painter, rect, gfx::Parallelogram(), gfx::Color::DarkGreen);
        rect.Translate(150, 0);
        gfx::FillShape(painter, rect, gfx::RightTriangle(), gfx::Color::DarkGreen);
        rect.Translate(150, 0);
        gfx::FillShape(painter, rect, gfx::IsoscelesTriangle(), gfx::Color::DarkGreen);
        rect.Move(10, 200);
        gfx::FillShape(painter, rect, gfx::Capsule(), gfx::Color::DarkGreen);
        rect.Translate(150, 0);
        gfx::FillShape(painter, rect, gfx::Circle(), gfx::Color::DarkGreen);
    }
    virtual std::string GetName() const override
    { return "FillShapeTest"; }
private:
};

class DrawShapeOutlineTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::FRect rect(10, 10, 100, 140);
        gfx::DrawShapeOutline(painter, rect,  gfx::Rectangle(), gfx::Color::DarkGreen);
        rect.Translate(150, 0);
        gfx::DrawShapeOutline(painter, rect, gfx::RoundRectangle(), gfx::Color::DarkGreen);
        rect.Translate(150, 0);
        gfx::DrawShapeOutline(painter, rect, gfx::Trapezoid(), gfx::Color::DarkGreen);
        rect.Translate(150, 0);
        gfx::DrawShapeOutline(painter, rect, gfx::Parallelogram(), gfx::Color::DarkGreen);
        rect.Translate(150, 0);
        gfx::DrawShapeOutline(painter, rect, gfx::RightTriangle(), gfx::Color::DarkGreen);
        rect.Translate(150, 0);
        gfx::DrawShapeOutline(painter, rect, gfx::IsoscelesTriangle(), gfx::Color::DarkGreen);
        rect.Move(10, 200);
        gfx::DrawShapeOutline(painter, rect, gfx::Capsule(), gfx::Color::DarkGreen);
        rect.Translate(150, 0);
        gfx::DrawShapeOutline(painter, rect, gfx::Circle(), gfx::Color::DarkGreen);

        rect.Move(10, 400);
        gfx::DrawShapeOutline(painter, rect,  gfx::Rectangle(), gfx::Color::DarkGreen, 3.0f);
        rect.Translate(150, 0);
        gfx::DrawShapeOutline(painter, rect, gfx::RoundRectangle(), gfx::Color::DarkGreen, 3.0f);
        rect.Translate(150, 0);
        gfx::DrawShapeOutline(painter, rect, gfx::Trapezoid(), gfx::Color::DarkGreen, 3.0f);
        rect.Translate(150, 0);
        gfx::DrawShapeOutline(painter, rect, gfx::Parallelogram(), gfx::Color::DarkGreen, 3.0f);
        rect.Translate(150, 0);
        gfx::DrawShapeOutline(painter, rect, gfx::RightTriangle(), gfx::Color::DarkGreen, 3.0f);
        rect.Translate(150, 0);
        gfx::DrawShapeOutline(painter, rect, gfx::IsoscelesTriangle(), gfx::Color::DarkGreen, 3.0f);
        rect.Move(10, 600);
        gfx::DrawShapeOutline(painter, rect, gfx::Capsule(), gfx::Color::DarkGreen, 3.0f);
        rect.Translate(150, 0);
        gfx::DrawShapeOutline(painter, rect, gfx::Circle(), gfx::Color::DarkGreen);
    }
    virtual std::string GetName() const override
    { return "DrawShapeOutline"; }
private:
};

class sRGBColorGradientTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        static gfx::CustomMaterialClass material;
        material.SetShaderSrc(
                R"(
#version 100
precision mediump float;
uniform float kGamma;
varying vec2 vTexCoord;
void main() {
  float block = 1.0 / 32.0;
  float index = floor(vTexCoord.x / block);
  float value = index * block;
  gl_FragColor = vec4(pow(vec3(value), vec3(kGamma)), 1.0);
}
        )");
        material.SetUniform("kGamma", 1.0f);

        auto inst = gfx::MaterialClassInst(material);

        // This test might give some clues how incorrect the rendering is
        // in regard to sRGB color space (and gamma in-correction).
        //
        // The shader always writes linear values. The OpenGL implementation
        // and the current frame-buffer setting will then be used to determine
        // the meaning of these values, i.e. whether they should be interpreted
        // to mean perceptually linear values (in sRGB color space) or physically
        // linear values (in "raw" RGB color space that drive the monitor's brightness).
        //
        // Whether sRGB is actually enabled or not depends on:
        //  - whether we're writing to the default frame buffer or GL provided FBO
        //  - what is the current rendering context, ES2, ES3, GLx
        //  - what flags are enabled, GL_FRAMEBUFFER_SRGB (for big Gl)
        //
        // The test should be interpreted something like this:
        //
        // - if the physically linear gamma ramp looks *perceptually* linear
        //   it means that the color buffer is written in perceptually linear
        //   values using sRGB encoding. Thus, when the buffer is being displayed
        //   and gamma decoding takes place (in the display system) RGB values
        //   lose some brightness which is compensated by the visual sensory system
        //   and thus become "perceptually linear". For example RGB value
        //   v = (0.5, 0.5, 0.5) when gamma decoded becomes approx (0.22, 0.22, 0.22),
        //   which in human visual sensory system appears half way bright between
        //   0.0f and 1.0f giving us visually/perceptually linear ramp.
        //
        //   corollary => the explicitly g=2.2 encoded ramp should be too dark.
        //
        // - if the physically linear gamma ramp looks too bright and washed out
        //   so that only the left edge is black and the brightness quickly becomes
        //   very bright and white towards the right edge it means that the color
        //   buffer is likely assumed to be in raw linear space driving the display
        //   values directly. So when it gets displayed the actual value output by
        //   the display reflects the value written by the shader. This means that the
        //   0.5f "half way" bright value will then appear too bright to the human
        //   perception and shades of darkness are lost.
        //
        //   corollary => the explicitly g=2.2 encoded ramp should look fine.
        //
        // The big problem here is that ES 2 does not have natively any clue about sRGB.
        //
        // The extension EGL_KHR_gl_colorspace is an EGL extension for asking for
        // sRGB enabled/encoded window system render targets. However, the extensions
        // explicitly says that:
        //
        //   "Only OpenGL and OpenGL ES contexts which support sRGB
        //   rendering must respect requests for EGL_GL_COLORSPACE_SRGB_KHR, and
        //   only to sRGB formats supported by the context (normally just SRGB8)
        //   Older versions not supporting sRGB rendering will ignore this
        //   surface attribute."
        //
        // So this means that with ES2 context even when using EGL with EGL_KHR_gl_colorspace
        // there's no guarantee of any sRGB correct output.
        //
        // The GL_EXT_sRGB extension applies to ES1.0 or greater and brings support
        // for sRGB encoded textures (GL_SRGB_EXT and GL_SRGB_EXT_ALPHA) and render
        // buffers basically says the same thing as the EGL_KHR_gl_colorspace
        // regarding the default frame buffer:
        //
        // "For the default framebuffer, color encoding is determined by the implementation."
        //

        // linear gradient ramp.
        // each block grows by a constant amount in intensity.
        inst.SetUniform("kGamma", 1.0f);
        gfx::FillRect(painter, gfx::FRect(20, 20, 800, 300), inst);

        // explicitly gamma decoded gradient ramp. the steps are adjusted by a gamma value
        // that approximates the inverse of the human perception. I.e. small increase
        // in input creates a large visual change. the gamma adjustment takes a large
        // output adjustment and maps it to a small change in input.
        inst.SetUniform("kGamma", 2.2f);
        gfx::FillRect(painter, gfx::FRect(20, 420, 800, 300), inst);

    }
    virtual std::string GetName() const override
    { return "sRGBColorGradientTest"; }
    virtual bool IsFeatureTest() const override
    { return false; }
private:
};

class sRGBTextureTest : public GraphicsTest
{
public:
    virtual void Start() override
    {
        // inspect the image
        {
            gfx::Image img;
            img.Load("textures/black-gray-white.png");
            ASSERT(img.GetDepthBits() == 24);
            ASSERT(img.GetWidth() == 2);
            ASSERT(img.GetHeight() == 2);

            auto view = img.GetReadView();
            gfx::RGB values[4];
            // okay so the image was created with THE GIMP and should
            // contain 4 pixels. One fully black (0.0f, 0.0f, 0.0f),
            // one gray at (0.5f, 0.5f, 0.5f) and finally one white
            // pixel @ (1.0f, 1.0f, 1.0f). The values should be in
            // sRGB encoded perceptually linear space.
            view->ReadPixel(0, 0, &values[0]);
            view->ReadPixel(0, 1, &values[1]);
            view->ReadPixel(1, 0, &values[2]);
            view->ReadPixel(1, 1, &values[3]);
            // print the sucker out
            std::printf("\n[0x%x 0x%x 0x%x][0x%x 0x%x 0x%x]"
                        "\n[0x%x 0x%x 0x%x][0x%x 0x%x 0x%x]",
                        values[0].r, values[0].g, values[0].b,
                        values[1].r, values[1].g, values[1].b,
                        values[2].r, values[2].g, values[2].b,
                        values[3].r, values[3].g, values[3].b);
            const auto srgb   = 0.5f;
            const auto linear = gfx::sRGB_decode(srgb);
            std::printf("sRGB to linear %f = %f\n", srgb, linear);
        }

        {
            gfx::TextureMap2DClass material;
            auto source = std::make_unique<gfx::detail::TextureFileSource>();
            source->SetFileName("textures/black-gray-white.png");
            source->SetColorSpace(gfx::TextureSource::ColorSpace::sRGB);
            material.SetTexture(std::move(source));
            material.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            mMaterialSRGB = gfx::CreateMaterialInstance(std::move(material));
        }

        {
            gfx::TextureMap2DClass material;
            auto source = std::make_unique<gfx::detail::TextureFileSource>();
            source->SetFileName("textures/black-gray-white.png");
            source->SetColorSpace(gfx::TextureSource::ColorSpace::Linear);
            material.SetTexture(std::move(source));
            material.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            mMaterialLinear = gfx::CreateMaterialInstance(std::move(material));
        }
    }

    virtual void Render(gfx::Painter& painter) override
    {
        gfx::FillRect(painter, gfx::FRect(20.0f, 20.0f, 400.0f, 400.0f), *mMaterialSRGB);
        gfx::FillRect(painter, gfx::FRect(520.0f, 20.0f, 400.0f, 400.0f), *mMaterialLinear);
    }
    virtual std::string GetName() const override
    { return "sRGBTextureTest"; }
    virtual bool IsFeatureTest() const override
    { return false; }
private:
    std::unique_ptr<gfx::Material> mMaterialSRGB;
    std::unique_ptr<gfx::Material> mMaterialLinear;
};

class PremultiplyAlphaTest : public GraphicsTest
{
public:
    virtual void Start() override
    {
        const char* src = R"(
#version 100
precision highp float;
uniform sampler2D kTexture;
varying vec2 vTexCoord;
void main() {
    vec4 foo = texture2D(kTexture, vTexCoord);
    gl_FragColor = vec4(foo.rgb, foo.a);
})";
        {
            gfx::TextureMap2D map;
            map.SetTexture(gfx::LoadTextureFromFile("textures/alpha-cutout.png"));
            map.SetSamplerName("kTexture");
            gfx::CustomMaterialClass material;
            material.SetShaderSrc(src);
            material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            material.SetTextureMap("kTexture", std::move(map));
            material.SetTextureMagFilter(gfx::TextureMap2DClass::MagTextureFilter::Linear);
            mMaterialStraightAlpha = gfx::CreateMaterialInstance(material);
        }
        {
            gfx::TextureMap2D map;
            auto tex = gfx::LoadTextureFromFile("textures/alpha-cutout.png");
            tex->SetFlag(gfx::detail::TextureFileSource::Flags::PremulAlpha, true);
            map.SetTexture(std::move(tex));
            map.SetSamplerName("kTexture");
            gfx::CustomMaterialClass material;
            material.SetShaderSrc(src);
            material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            material.SetTextureMap("kTexture", std::move(map));
            material.SetTextureMagFilter(gfx::TextureMap2DClass::MagTextureFilter::Linear);
            material.SetFlag(gfx::MaterialClass::Flags::PremultipliedAlpha, true);
            mMaterialPremultAlpha = gfx::CreateMaterialInstance(material);
        }
    }
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::FillRect(painter, gfx::FRect(10.0f, 10.0, 512.0f, 512.0f), *mMaterialStraightAlpha);
        gfx::FillRect(painter, gfx::FRect(500.0f, 10.0, 512.0f, 512.0f), *mMaterialPremultAlpha);
    }
    virtual std::string GetName() const override
    { return "PremultiplyAlphaTest"; }
    virtual bool IsFeatureTest() const override
    { return true; }
private:
    std::unique_ptr<gfx::Material> mMaterialStraightAlpha;
    std::unique_ptr<gfx::Material> mMaterialPremultAlpha;
};


int main(int argc, char* argv[])
{
    base::OStreamLogger logger(std::cout);
    logger.EnableTerminalColors(true);
    base::SetGlobalLog(&logger);
    DEBUG("It's alive!");
    INFO("Copyright (c) 2020-2021 Sami Vaisanen");
    INFO("http://www.ensisoft.com");
    INFO("http://github.com/ensisoft/gamestudio");

    auto sampling = wdk::Config::Multisampling::None;
    bool testing  = false;
    bool issue_gold = false;
    bool fullscreen = false;
    bool user_interaction = true;
    bool srgb = true;
    bool debug_context = false;
    int swap_interval = 0;
    int test_result   = EXIT_SUCCESS;
    std::string casename;

    for (int i=1; i<argc; ++i)
    {
        if (!std::strcmp(argv[i], "--debug-log"))
            base::EnableDebugLog(true);
        else if (!std::strcmp(argv[i], "--debug"))
            debug_context = true;
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
        else if (!std::strcmp(argv[i], "--vsync"))
            swap_interval = 1;
        else if (!std::strcmp(argv[i], "--fullscreen"))
            fullscreen = true;
        else if (!std::strcmp(argv[i], "--no-user"))
            user_interaction = false;
        else if (!std::strcmp(argv[i], "--no-srgb"))
            srgb = false;
    }

    // context integration glue code that puts together
    // wdk::Context and gfx::Device
    class WindowContext : public dev::Context
    {
    public:
        WindowContext(wdk::Config::Multisampling sampling, bool srgb, bool debug)
        {
            wdk::Config::Attributes attrs;
            attrs.red_size        = 8;
            attrs.green_size      = 8;
            attrs.blue_size       = 8;
            attrs.alpha_size      = 8;
            attrs.stencil_size    = 8;
            attrs.depth_size      = 24;
            attrs.surfaces.window = true;
            attrs.double_buffer   = true;
            attrs.sampling        = sampling;
            attrs.srgb_buffer     = srgb;

            mConfig   = std::make_unique<wdk::Config>(attrs);
            mContext  = std::make_unique<wdk::Context>(*mConfig, 2, 0,  debug,
                wdk::Context::Type::OpenGL_ES);
            mVisualID = mConfig->GetVisualID();
            mDebug = debug;
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
        virtual bool IsDebug() const override
        {
            return mDebug;
        }
        wdk::uint_t GetVisualID() const
        { return mVisualID; }

        void SetWindowSurface(wdk::Window& window)
        {
            mSurface = std::make_unique<wdk::Surface>(*mConfig, window);
            mContext->MakeCurrent(mSurface.get());
            mConfig.reset();
        }
        void SetSwapInterval(int swap_interval)
        {
            mContext->SetSwapInterval(swap_interval);
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
        bool mDebug = false;
    };

    auto context = std::make_shared<WindowContext>(sampling, srgb, debug_context);
    auto dev_device  = dev::CreateDevice(context);
    auto gfx_device = dev_device->GetSharedGraphicsDevice();
    auto painter = gfx::Painter::Create(gfx_device);
    painter->SetEditingMode(false);

    std::size_t test_index = 0;
    std::vector<std::unique_ptr<GraphicsTest>> tests;
    tests.emplace_back(new FillShapeTest);
    tests.emplace_back(new DrawShapeOutlineTest);
    tests.emplace_back(new TransformTest);
    tests.emplace_back(new RenderTextTest);
    tests.emplace_back(new TextAlignTest("TextAlignTest", "fonts/AtariFontFullVersion.ttf", gfx::Color::DarkGray, 14));
    tests.emplace_back(new TextAlignTest("TextAlignTest2", "fonts/nuskool_krome_64x64.json", gfx::Color::White, 20));
    tests.emplace_back(new TextRectScaleTest);
    tests.emplace_back(new RenderParticleTest);
    tests.emplace_back(new ShapeTest<gfx::Arrow>("ArrowShapeTest"));
    tests.emplace_back(new ShapeTest<gfx::Capsule>("CapsuleShapeTest"));
    tests.emplace_back(new ShapeTest<gfx::Circle>("CircleShapeTest"));
    tests.emplace_back(new ShapeTest<gfx::IsoscelesTriangle>("IsoscelesTriangleShapeTest"));
    tests.emplace_back(new ShapeTest<gfx::Parallelogram>("ParallelogramShapeTest"));
    tests.emplace_back(new ShapeTest<gfx::Rectangle>("RectangleShapeTest"));
    tests.emplace_back(new ShapeTest<gfx::RoundRectangle>("RoundRectShapeTest"));
    tests.emplace_back(new ShapeTest<gfx::RightTriangle>("RightTriangleShapeTest"));
    tests.emplace_back(new ShapeTest<gfx::Trapezoid>("TrapezoidShapeTest"));
    tests.emplace_back(new ShapeTest<gfx::SemiCircle>("SemiCircleShapeTest"));
    tests.emplace_back(new ShapeTest<gfx::Sector>("SectorTest"));
    tests.emplace_back(new TextureTest);
    tests.emplace_back(new TextureBlurTest);
    tests.emplace_back(new GradientTest);
    tests.emplace_back(new SpriteTest);
    tests.emplace_back(new StencilTest);
    tests.emplace_back(new PolygonTest);
    tests.emplace_back(new TileBatchTest);
    tests.emplace_back(new JankTest);
    tests.emplace_back(new MegaParticleTest);
    tests.emplace_back(new VSyncTest);
    tests.emplace_back(new NullTest);
    tests.emplace_back(new ScissorTest);
    tests.emplace_back(new ViewportTest);
    tests.emplace_back(new sRGBColorGradientTest);
    tests.emplace_back(new sRGBTextureTest);
    tests.emplace_back(new PremultiplyAlphaTest);

    bool stop_for_input = false;

    wdk::Window window;
    unsigned surface_width  = 1024;
    unsigned surface_height = 768;
    window.Create("Demo", 1024, 768, context->GetVisualID());
    window.SetFullscreen(fullscreen);
    window.OnResize = [&](const wdk::WindowEventResize& resize) {
        surface_width = resize.width;
        surface_height = resize.height;
    };
    window.OnKeyDown = [&](const wdk::WindowEventKeyDown& key) {
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
            const auto& rgba = gfx_device->ReadColorBuffer(window.GetSurfaceWidth(),
                window.GetSurfaceHeight());

            const auto& name = "demo_" + std::to_string(screenshot_number) + ".png";
            gfx::WritePNG(rgba, name);
            INFO("Wrote screen capture '%1'", name);
            screenshot_number++;
        }
        if (test_index != current_test_index)
        {
            tests[current_test_index]->End();
            tests[test_index]->Start();
            window.SetTitle(tests[test_index]->GetName());
        }
        stop_for_input = false;
    };

    // render in the window
    context->SetWindowSurface(window);
    context->SetSwapInterval(swap_interval);

    if (testing)
    {
        const float dt = 1.0f/60.0f;

        for (auto& test : tests)
        {
            if (!test->IsFeatureTest())
                continue;

            if (!casename.empty())
            {
                if (casename != test->GetName())
                    continue;
            }
            INFO("Running test case: '%1'", test->GetName());
            test->Start();

            for (int i=0; i<3; ++i)
            {
                // update test in small time steps trying to avoid
                // any simulation from becoming unstable.
                for (int step=0; step<534; ++step)
                    test->Update(dt);

                gfx_device->BeginFrame();
                gfx_device->ClearColor(gfx::Color::Black);
                painter->SetViewport(0, 0, surface_width, surface_height);
                painter->SetSurfaceSize(surface_width, surface_height);
                painter->SetOrthographicProjection(surface_width , surface_height);
                // render the test.
                test->Render(*painter);

                const gfx::Bitmap<gfx::RGBA>& result = gfx_device->ReadColorBuffer(window.GetSurfaceWidth(),
                    window.GetSurfaceHeight());

                const auto& resultfile = base::FormatString("Result_%1_%2_%3_.png", test->GetName(), i, sampling);
                const auto& goldfile   = base::FormatString("Gold_%1_%2_%3_.png", test->GetName(), i, sampling);
                const auto& deltafile  = base::FormatString("Delta_%1_%2_%3_.png", test->GetName(), i, sampling);
                if (!base::FileExists(goldfile) || issue_gold)
                {
                    gfx_device->EndFrame(true /*display*/);
                    gfx_device->CleanGarbage(120, gfx::Device::GCFlags::Textures);
                    // the result is the new gold image. should be eye-balled and verified.
                    gfx::WritePNG(result, goldfile);
                    INFO("Wrote new gold file. '%1'", goldfile);
                    continue;
                }

                stop_for_input = true;

                gfx::Bitmap<gfx::RGBA>::MSE mse;
                mse.SetErrorTreshold(5.0);

                // load gold image
                gfx::Image img(goldfile);
                const gfx::Bitmap<gfx::RGBA>& gold = img.AsBitmap<gfx::RGBA>();
                if (!gfx::Compare(gold, result, mse))
                {
                    ERROR("'%1' vs '%2' FAILED.", goldfile, resultfile);
                    if (gold.GetWidth() != result.GetWidth() || gold.GetHeight() != result.GetHeight())
                    {
                        ERROR("Image dimensions mismatch: Gold = %1x%1 vs. Result = %2x%3",
                            gold.GetWidth(), gold.GetHeight(),
                            result.GetWidth(), result.GetHeight());
                    }
                    else
                    {
                        // generate difference visualization file.
                        gfx::Bitmap<gfx::RGBA> diff;
                        diff.Resize(gold.GetWidth(), gold.GetHeight());
                        diff.Fill(gfx::Color::White);
                        for (size_t y=0; y<gold.GetHeight(); ++y)
                        {
                            for (size_t x=0; x<gold.GetWidth(); ++x)
                            {
                                const auto& src = gold.GetPixel(y, x);
                                const auto& ret = result.GetPixel(y, x);
                                if (src != ret)
                                {
                                    diff.SetPixel(y, x, gfx::Color::Black);
                                }
                            }
                        }
                        gfx::WritePNG(diff, deltafile);
                    }
                    gfx::WritePNG(result, resultfile);
                    test_result = EXIT_FAILURE;
                }
                else
                {
                    INFO("'%1' vs '%2' OK.", goldfile, resultfile);
                    stop_for_input = false;
                }

                gfx_device->EndFrame(true /* display */);
                gfx_device->CleanGarbage(120, gfx::Device::GCFlags::Textures);

                if (stop_for_input && user_interaction)
                {
                    while (stop_for_input)
                    {
                        wdk::native_event_t event;
                        wdk::WaitEvent(event);
                        window.ProcessEvent(event);
                    }
                }
                else
                {
                    wdk::native_event_t event;
                    while (wdk::PeekEvent(event))
                        window.ProcessEvent(event);
                }
                stop_for_input = false;
            }
            test->End();
        }
    }
    else
    {
        if (!casename.empty())
        {
            for (test_index=0; test_index<tests.size(); ++test_index)
            {
                if (tests[test_index]->GetName() == casename)
                    break;
            }
            if (test_index == tests.size())
                test_index = 0;
        }

        tests[test_index]->Start();
        window.SetTitle(tests[test_index]->GetName());

        using clock = std::chrono::high_resolution_clock;

        auto stamp = clock::now();
        float frames  = 0;
        float seconds = 0.0f;
        std::vector<double> frame_times;

        while (window.DoesExist())
        {
            // measure how much time has elapsed since last iteration
            const auto now  = clock::now();
            const auto gone = now - stamp;
            // if sync to vblank is off then it's possible that we might be
            // rendering too fast for milliseconds, let's use microsecond
            // precision for now. otherwise, we'd need to accumulate time worth of
            // several iterations of the loop in order to have an actual time step
            // for updating the animations.
            const auto secs = std::chrono::duration_cast<std::chrono::microseconds>(gone).count() / (1000.0 * 1000.0);
            stamp = now;

            frame_times.push_back(secs);

            // jump animations forward by the *previous* timestep
            for (auto& test : tests)
                test->Update(secs);

            gfx_device->BeginFrame();
            gfx_device->ClearColor(gfx::Color4f(0.2f, 0.3f, 0.4f, 1.0f));
            painter->SetViewport(0, 0, surface_width, surface_height);
            painter->SetSurfaceSize(surface_width, surface_height);
            painter->SetOrthographicProjection(surface_width , surface_height);
            // render the current test
            tests[test_index]->Render(*painter);

            gfx_device->EndFrame(true /*display*/);
            gfx_device->CleanGarbage(120, gfx::Device::GCFlags::Textures);

            // process incoming (window) events
            wdk::native_event_t event;
            while (wdk::PeekEvent(event))
            {
                window.ProcessEvent(event);
            }

            ++frames;
            seconds += secs;
            if (seconds > 2.0f)
            {
                const auto fps = frames / seconds;
                const auto [min, max] = std::minmax_element(frame_times.begin(), frame_times.end());
                //INFO("Time: %1s, frames: %2, FPS: %3 min: %4, max: %5", seconds, frames, fps, *min, *max);
                frame_times.clear();
                frames = 0.0f;
                seconds = 0.0f;
            }
        }
    }

    context->Dispose();
    return test_result;
}
