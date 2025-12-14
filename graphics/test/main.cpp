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
#include <filesystem>

#include "base/logging.h"
#include "base/format.h"
#include "data/json.h"
#include "data/io.h"
#include "device/device.h"
#include "graphics/device_algo.h"
#include "graphics/image.h"
#include "graphics/device.h"
#include "graphics/material.h"
#include "graphics/material_class.h"
#include "graphics/material_instance.h"
#include "graphics/painter.h"
#include "graphics/drawing.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"
#include "graphics/resource.h"
#include "graphics/framebuffer.h"
#include "graphics/texture.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/renderpass.h"
#include "graphics/shader_source.h"
#include "graphics/shader_programs.h"
#include "graphics/simple_shape.h"
#include "graphics/polygon_mesh.h"
#include "graphics/wavefront_mesh.h"
#include "graphics/particle_engine.h"
#include "graphics/tilebatch.h"
#include "graphics/linebatch.h"
#include "graphics/guidegrid.h"
#include "graphics/debug_drawable.h"
#include "graphics/effect_drawable.h"
#include "graphics/utility.h"
#include "graphics/tool/polygon.h"
#include "graphics/texture_texture_source.h"
#include "graphics/texture_file_source.h"
#include "graphics/text_buffer.h"
#include "graphics/text_material.h"
#include "wdk/opengl/config.h"
#include "wdk/opengl/context.h"
#include "wdk/opengl/surface.h"
#include "wdk/window.h"
#include "wdk/events.h"
#include "wdk/system.h"

#include "base/snafu.h"

#undef far

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
    virtual void KeyDown(const wdk::WindowEventKeyDown& key)
    {}
private:
};

class ScissorTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
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
    std::string GetName() const override
    { return "ScissorTest"; }
    void Update(float dt) override
    {
        mTime += dt;
    }
private:
    float mTime = 0.0f;
};

class ViewportTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        const auto cycle = 2.0f;
        const auto time = fmodf(mTime, cycle);
        const bool clip = time >= (cycle * 0.5);
        if (clip)
            painter.SetViewport(10, 10, 300, 300);

        static auto klass = gfx::CreateMaterialFromImage("textures/uv_test_512.png");
        gfx::FillRect(painter, gfx::FRect(0, 0, 1024, 768), gfx::MaterialInstance(klass));
    }
    std::string GetName() const override
    { return "ViewportTest"; }
    void Update(float dt) override
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
    void Render(gfx::Painter&) override
    {}
    std::string GetName() const override
    { return "NullTest"; }
    bool IsFeatureTest() const override
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
        mColors.emplace_back(gfx::Color::Red);
        mColors.emplace_back(gfx::Color::Cyan);
    }
    void Render(gfx::Painter& painter) override
    {
        gfx::DrawTextRect(painter,
          "VSYNC TEST",
          "fonts/AtariFontFullVersion.ttf", 96,
          gfx::FRect(0, 0, 1024, 768),
          mColors[mColorIndex],
          gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter, 0, 1.4f);
        mColorIndex = (mColorIndex + 1) % mColors.size();
    }
    std::string GetName() const override
    { return "VSyncTest"; }
    bool IsFeatureTest() const override
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
        gfx::ParticleEngineClass::Params p;
        p.mode = gfx::ParticleEngineClass::SpawnPolicy::Once;
        p.boundary = gfx::ParticleEngineClass::BoundaryPolicy::Reflect;
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
        gfx::ParticleEngineClass klass(p);
        mEngine = std::make_unique<gfx::ParticleEngineInstance>(p);
    }
    void Render(gfx::Painter& painter) override
    {
        gfx::Transform model;
        model.Resize(1024, 768);
        painter.Draw(*mEngine, model, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
    }
    void Update(float dt) override
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
    void Start() override
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
    void End() override
    {
        mStarted = false;
    }
    std::string GetName() const override
    { return "MegaParticleTest"; }
    bool IsFeatureTest() const override
    { return false; }
private:
    std::unique_ptr<gfx::ParticleEngineInstance> mEngine;
    bool mStarted = false;
};

class JankTest : public GraphicsTest
{
public:
    JankTest()
    {
        gfx::ParticleEngineClass::Params p;
        p.mode = gfx::ParticleEngineClass::SpawnPolicy::Once;
        p.boundary = gfx::ParticleEngineClass::BoundaryPolicy::Reflect;
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
            gfx::ParticleEngineClass klass(p);
            mEngine[0] = std::make_unique<gfx::ParticleEngineInstance>(klass);
        }

        {
            p.init_rect_xpos = 0.5f;
            gfx::ParticleEngineClass klass(p);
            mEngine[1] = std::make_unique<gfx::ParticleEngineInstance>(klass);
        }
    }
    void Render(gfx::Painter& painter) override
    {
        gfx::Transform model;
        model.Resize(1024, 768);
        painter.Draw(*mEngine[0], model, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
        painter.Draw(*mEngine[1], model, gfx::CreateMaterialFromColor(gfx::Color::Green));
    }
    void Update(float dt) override
    {
        gfx::Transform transform;
        transform.Resize(1024, 768);
        const auto& model = transform.GetAsMatrix();

        gfx::DrawableClass::Environment e;
        e.model_matrix = &model;

        mEngine[0]->Update(e, dt);
        mEngine[1]->Update(e, dt);
    }
    void Start() override
    {
        gfx::Transform transform;
        transform.Resize(1024, 768);
        const auto& model = transform.GetAsMatrix();

        gfx::DrawableClass::Environment e;
        e.model_matrix = &model;
        mEngine[0]->Restart(e);
        mEngine[1]->Restart(e);
    }

    std::string GetName() const override
    { return "JankTest"; }
    bool IsFeatureTest() const override
    { return false; }
private:
    std::unique_ptr<gfx::ParticleEngineInstance> mEngine[2];
};

class PacmanPolygon
{
public:
    static void Build(gfx::PolygonMeshClass* polygon, float time)
    {
        gfx::tool::PolygonBuilder2D builder;
        builder.SetStatic(false);
        AddPacman(builder, 0.4f, -0.5f, 0.3f, time);
        AddCircleShape(builder, 0.60f, -0.5f, 0.05f);
        AddCircleShape(builder, 0.75f, -0.5f, 0.05f);
        AddCircleShape(builder, 0.90f, -0.5f, 0.05f);

        builder.BuildPoly(*polygon);
    }
private:
    static void AddPacman(gfx::tool::PolygonBuilder2D& poly, float x, float y, float r, float time)
    {
        gfx::Vertex2D center = {
                {x, y}, {x, -y}
        };
        std::vector<gfx::Vertex2D> verts;
        verts.push_back(center);

        const auto slices = 200;
        const auto angle = (math::Pi * 2) / slices;
        const auto mouth = (std::sin(time) + 1.0f) / 2.0f * 15;
        for (int i=mouth; i<=slices-mouth; ++i)
        {
            const float a = i * angle;
            gfx::Vertex2D v;
            v.aPosition.x = x + std::cos(a) * r;
            v.aPosition.y = y + std::sin(a) * r;
            v.aTexCoord.x = v.aPosition.x;
            v.aTexCoord.y = v.aPosition.y * -1.0f;
            verts.push_back(v);
        }
        gfx::Geometry::DrawCommand cmd;
        cmd.type   = gfx::Geometry::DrawType::TriangleFan;
        cmd.offset = poly.GetVertexCount();
        cmd.count  = verts.size();
        poly.AddVertices(std::move(verts));
        poly.AddDrawCommand(cmd);
    }
    static void AddCircleShape(gfx::tool::PolygonBuilder2D& poly, float x, float y, float r)
    {
        gfx::Vertex2D center = {
                {x, y}, {x, -y}
        };
        std::vector<gfx::Vertex2D> verts;
        verts.push_back(center);

        const auto slices = 200;
        const auto angle = (math::Pi * 2) / slices;
        for (int i=0; i<=slices; ++i)
        {
            const float a = i * angle;
            gfx::Vertex2D v;
            v.aPosition.x = x + std::cos(a) * r;
            v.aPosition.y = y + std::sin(a) * r;
            v.aTexCoord.x = v.aPosition.x;
            v.aTexCoord.y = v.aPosition.y * -1.0f;
            verts.push_back(v);
        }
        gfx::Geometry::DrawCommand cmd;
        cmd.type   = gfx::Geometry::DrawType::TriangleFan;
        cmd.offset = poly.GetVertexCount();
        cmd.count  = verts.size();
        poly.AddVertices(std::move(verts));
        poly.AddDrawCommand(cmd);
    }
private:
};

class PolygonTest : public GraphicsTest
{
public:
    PolygonTest()
    {
        // allow editing
        mPoly.SetDynamic(true);
    }

    void Render(gfx::Painter& painter) override
    {
        PacmanPolygon pacman;
        PacmanPolygon::Build(&mPoly, mTime);

        // pacman body + food dots
        gfx::Transform transform;
        transform.Resize(500, 500);
        transform.MoveTo(200, 200);

        gfx::GradientClass material(gfx::MaterialClass::Type::Gradient);
        material.SetColor(gfx::Color::Yellow, gfx::GradientClass::ColorIndex::GradientColor0);
        material.SetColor(gfx::Color::Yellow, gfx::GradientClass::ColorIndex::GradientColor1);
        material.SetColor(gfx::Color::Black,  gfx::GradientClass::ColorIndex::GradientColor2);
        material.SetColor(gfx::Color::Yellow, gfx::GradientClass::ColorIndex::GradientColor3);
        material.SetGradientGamma(2.2f);
        painter.Draw(gfx::PolygonMeshInstance(mPoly), transform, gfx::MaterialInstance(material));

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
    std::string GetName() const override
    { return "PolygonTest"; }

    void Update(float dts) override
    {
        const float velocity = 5.23;
        mTime += dts * velocity;
    }
private:
    float mTime = 0.0f;
    gfx::PolygonMeshClass mPoly;

};

class TileOverdrawTest : public GraphicsTest
{
public:
    void Start() override
    {
        gfx::TextureMap2D map;
        map.SetNumTextures(1);
        map.SetTextureSource(0, gfx::LoadTextureFromFile("textures/tilemap_128x128.png"));
        map.SetSamplerName("kTexture");
        map.SetName("Tilemap");

        gfx::MaterialClass klass(gfx::MaterialClass::Type::Tilemap);
        klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        klass.SetNumTextureMaps(1);
        klass.SetTextureMap(0, map);
        klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Linear);
        klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Trilinear);
        klass.SetTileSize(glm::vec2(128.0f, 128.0f));
        klass.SetTilePadding(glm::vec2(2.0f, 2.0f));
        klass.SetTileOffset(glm::vec2(0.0f, 0.0f));
        mTileset = gfx::CreateMaterialInstance(klass);
    }
    void Render(gfx::Painter& painter) override
    {
        const float tile_indices[9] = {
            3.0f, 14.0f, 13.0f,
            0.0f, 12.0f, 10.0f,
            1.0f, 9.0f, 4.0f
        };
        for (unsigned row=0; row<3; ++row)
        {
            for (unsigned col=0; col<3; ++col)
            {
                gfx::FRect rect;
                rect.Resize(128.0f, 128.0f);
                rect.Move((1024.0f - 3.0f*128.0f) * 0.5f,
                          (768.0f - 3.0f*128.0f) * 0.5f);
                rect.Translate(col*128.0f, row*128.0f);

                mTileset->SetUniform("kTileIndex", tile_indices[row*3 + col]);
                gfx::FillRect(painter, rect, *mTileset);
            }
        }
    }
    std::string GetName() const override
    {
        return "TileOverdrawTest";
    }

private:
    std::unique_ptr<gfx::Material> mTileset;
};


class TileBatchTest : public GraphicsTest
{
public:
    void Start() override
    {
        {
            gfx::TextureMap2D map;
            map.SetNumTextures(1);
            map.SetTextureSource(0, gfx::LoadTextureFromFile("textures/tilemap_64x64.png"));
            map.SetSamplerName("kTexture");
            map.SetName("Tilemap");

            gfx::MaterialClass klass(gfx::MaterialClass::Type::Tilemap);
            klass.SetNumTextureMaps(1);
            klass.SetTextureMap(0, map);
            klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Nearest);
            klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Nearest);

            klass.SetTileSize(glm::vec2(64.0f, 64.0f));
            klass.SetTileOffset(glm::vec2(0.0f, 0.0f));
            mTileset64x46 = gfx::CreateMaterialInstance(klass);
        }

        {
            gfx::TextureMap2D map;
            map.SetNumTextures(1);
            map.SetTextureSource(0, gfx::LoadTextureFromFile("textures/tilemap_64x64_offset_32x32.png"));
            map.SetSamplerName("kTexture");
            map.SetName("Tilemap");

            gfx::MaterialClass klass(gfx::MaterialClass::Type::Tilemap);
            klass.SetNumTextureMaps(1);
            klass.SetTextureMap(0, map);
            klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Nearest);
            klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Nearest);

            klass.SetTileSize(glm::vec2(64.0f, 64.0f));
            klass.SetTileOffset(glm::vec2(32.0f, 32.0f));
            mTileset64x46Offset32x32 = gfx::CreateMaterialInstance(klass);
        }

        {
            const auto tile_width = 64.0f;
            const auto tile_height = 64.0f;
            const auto texture_width = tile_width * 4.0f + 32.0f;
            const auto texture_height = tile_height * 2.0f + 32.0f;

            gfx::FRect texture(0.0f, 0.0f, 512.0f, 256.0f);
            gfx::FRect rect(0.0f, 0.0f, texture_width, texture_height);
            rect.Move(512.0f, 256.0f);
            rect.Translate(-texture_width, -texture_height);
            rect = MapToLocalNormalize(texture, rect);

            gfx::TextureMap2D map;
            map.SetNumTextures(1);
            map.SetTextureSource(0, gfx::LoadTextureFromFile("textures/tilemap_64x64_offset_32x32_atlas_512x256.png"));
            map.SetSamplerName("kTexture");
            map.SetName("Tilemap");
            map.SetTextureRect(0, rect);

            gfx::MaterialClass klass(gfx::MaterialClass::Type::Tilemap);
            klass.SetNumTextureMaps(1);
            klass.SetTextureMap(0, map);
            klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Nearest);
            klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Nearest);

            klass.SetTileSize(glm::vec2(64.0f, 64.0f));
            klass.SetTileOffset(glm::vec2(32.0f, 32.0f));
            mTileset64x46Offset32x32Atlas512x256 = gfx::CreateMaterialInstance(klass);
        }

        {
            const auto tile_width = 64.0f;
            const auto tile_height = 64.0f;
            const auto tile_padding = 2.0f;
            const auto texture_width = (tile_width + 2.0*tile_padding) * 4.0f;
            const auto texture_height = tile_height + 2.0*tile_padding;

            gfx::TextureMap2D map;
            map.SetNumTextures(1);
            map.SetTextureSource(0, gfx::LoadTextureFromFile("textures/tilemap_64x64_padding_2x2.png"));
            map.SetSamplerName("kTexture");
            map.SetName("Tilemap");

            gfx::MaterialClass klass(gfx::MaterialClass::Type::Tilemap);
            klass.SetNumTextureMaps(1);
            klass.SetTextureMap(0, map);
            klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Nearest);
            klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Nearest);

            klass.SetTileSize(glm::vec2(64.0f, 64.0f));
            klass.SetTilePadding(glm::vec2(2.0f, 2.0f));
            mTileSet64x64Padding2x2 = gfx::CreateMaterialInstance(klass);
        }


    }

    void Render(gfx::Painter& painter) override
    {
        const auto tile_size = 50.0f;

        // these should all render the same.
        // the difference is in the input texture and how
        // the tiles are laid out in the texture.

        {
            gfx::TileBatch tiles;
            tiles.SetTileWorldWidth(64.0f);
            tiles.SetTileWorldHeight(64.0f);
            tiles.SetTileRenderWidth(64.0f);
            tiles.SetTileRenderHeight(64.0f);
            for (unsigned row=0; row<2; ++row)
            {
                for (unsigned col=0; col<4; ++col)
                {
                    gfx::TileBatch::Tile tile;
                    tile.pos.y  = static_cast<float>(row);
                    tile.pos.x  = static_cast<float>(col);
                    tile.data.x = static_cast<float>(row * 4 + col); // tile index.
                    tiles.AddTile(tile);
                }
            }
            gfx::Transform transform;
            transform.MoveTo(20.0f, 20.0f);
            transform.Resize(1.0f, 1.0f);
            painter.Draw(tiles, transform, *mTileset64x46);
        }

        {
            gfx::TileBatch tiles;
            tiles.SetTileWorldWidth(64.0f);
            tiles.SetTileWorldHeight(64.0f);
            tiles.SetTileRenderWidth(64.0f);
            tiles.SetTileRenderHeight(64.0f);
            for (unsigned row=0; row<2; ++row)
            {
                for (unsigned col=0; col<4; ++col)
                {
                    gfx::TileBatch::Tile tile;
                    tile.pos.y  = static_cast<float>(row);
                    tile.pos.x  = static_cast<float>(col);
                    tile.data.x = static_cast<float>(row * 4 + col); // tile index.
                    tiles.AddTile(tile);
                }
            }
            gfx::Transform transform;
            transform.MoveTo(20.0f, 200.0f);
            transform.Resize(1.0f, 1.0f);
            painter.Draw(tiles, transform, *mTileset64x46Offset32x32);
        }

        {
            gfx::TileBatch tiles;
            tiles.SetTileWorldWidth(64.0f);
            tiles.SetTileWorldHeight(64.0f);
            tiles.SetTileRenderWidth(64.0f);
            tiles.SetTileRenderHeight(64.0f);
            for (unsigned row=0; row<2; ++row)
            {
                for (unsigned col=0; col<4; ++col)
                {
                    gfx::TileBatch::Tile tile;
                    tile.pos.y  = static_cast<float>(row);
                    tile.pos.x  = static_cast<float>(col);
                    tile.data.x = static_cast<float>(row * 4 + col); // tile index.
                    tiles.AddTile(tile);
                }
            }
            gfx::Transform transform;
            transform.MoveTo(20.0f, 400.0f);
            transform.Resize(1.0f, 1.0f);
            painter.Draw(tiles, transform, *mTileset64x46Offset32x32Atlas512x256);
        }

        {
            gfx::TileBatch tiles;
            tiles.SetTileWorldWidth(64.0f);
            tiles.SetTileWorldHeight(64.0f);
            tiles.SetTileRenderWidth(64.0f);
            tiles.SetTileRenderHeight(64.0f);
            for (unsigned row=0; row<2; ++row)
            {
                for (unsigned col=0; col<4; ++col)
                {
                    gfx::TileBatch::Tile tile;
                    tile.pos.y  = static_cast<float>(row);
                    tile.pos.x  = static_cast<float>(col);
                    tile.data.x = static_cast<float>((row * 4 + col + row) % 4); // tile index.
                    tiles.AddTile(tile);
                }
            }
            gfx::Transform transform;
            transform.MoveTo(300.0f, 20.0f);
            transform.Resize(1.0f, 1.0f);
            painter.Draw(tiles, transform, *mTileSet64x64Padding2x2);
        }

    }
    std::string GetName() const override
    { return "TileBatchTest"; }
private:
    std::unique_ptr<gfx::Material> mTileset64x46;
    std::unique_ptr<gfx::Material> mTileset64x46Offset32x32;
    std::unique_ptr<gfx::Material> mTileset64x46Offset32x32Atlas512x256;
    std::unique_ptr<gfx::Material> mTileSet64x64Padding2x2;
};

class StencilCoverTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        // use the stencil to cover (block) areas from getting changed
        // by the color buffer write.

        // Clear stencil buffer to 1 (write color everywhere)
        painter.ClearStencil(gfx::StencilClearValue(1));

        // write the mask shape to the stencil buffer by writing
        // zeroes to the stencil buffer to the fragments that are NOT to be modified
        gfx::Painter::RenderPassState mask_cover_state;
        mask_cover_state.render_pass = gfx::RenderPass::StencilPass;
        mask_cover_state.cds.bWriteColor   = false;
        mask_cover_state.cds.stencil_ref   = 0;
        mask_cover_state.cds.stencil_mask  = 0xff;
        mask_cover_state.cds.stencil_func  = gfx::Painter::StencilFunc::PassAlways;
        mask_cover_state.cds.stencil_dpass = gfx::Painter::StencilOp::WriteRef;
        mask_cover_state.cds.stencil_dfail = gfx::Painter::StencilOp::WriteRef;

        // Set the state so that we modify fragments only when the stencil value is 1.
        gfx::Painter::RenderPassState mask_draw_color_state;
        mask_draw_color_state.render_pass = gfx::RenderPass::ColorPass;
        mask_draw_color_state.cds.bWriteColor   = true;
        mask_draw_color_state.cds.stencil_ref   = 1;
        mask_draw_color_state.cds.stencil_mask  = 0xff;
        mask_draw_color_state.cds.stencil_func  = gfx::Painter::StencilFunc::RefIsEqual;
        mask_draw_color_state.cds.stencil_dpass = gfx::Painter::StencilOp::DontModify;
        mask_draw_color_state.cds.stencil_dfail = gfx::Painter::StencilOp::DontModify;

        gfx::Transform mask_transform;
        mask_transform.Resize(200.0f, 200.0f);
        mask_transform.MoveTo(300.0f, 300.0f);

        gfx::Transform shape_transform;
        shape_transform.Resize(200.0f, 200.0f);
        shape_transform.MoveTo(300.0f, 300.0f);

        gfx::Painter::DrawCommandState cmd_state;

        gfx::StencilShaderProgram stencil_program;
        gfx::GenericShaderProgram color_program;

        painter.Draw(gfx::Circle(),gfx::CreateMaterialFromColor(gfx::Color::White), mask_transform,
            stencil_program, mask_cover_state, cmd_state);

        painter.Draw(gfx::Rectangle(), gfx::CreateMaterialFromColor(gfx::Color::HotPink), shape_transform,
             color_program, mask_draw_color_state, cmd_state);
    }
    std::string GetName() const override
    { return "StencilCoverTest"; }
private:
};

class StencilExposeTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        // use the stencil to cover (block) areas from getting changed
        // by the color buffer write.

        // Clear stencil buffer to 0. (Write color nowhere)
        painter.ClearStencil(gfx::StencilClearValue(0));

        // write the mask shape to the stencil buffer by writing
        // ones to the stencil buffer to the fragments that are to be *modified*
        gfx::Painter::RenderPassState mask_expose_state;
        mask_expose_state.render_pass = gfx::RenderPass::StencilPass;
        mask_expose_state.cds.bWriteColor   = false;
        mask_expose_state.cds.stencil_ref   = 1;
        mask_expose_state.cds.stencil_mask  = 0xff;
        mask_expose_state.cds.stencil_func  = gfx::Painter::StencilFunc::PassAlways;
        mask_expose_state.cds.stencil_dpass = gfx::Painter::StencilOp::WriteRef;
        mask_expose_state.cds.stencil_dfail = gfx::Painter::StencilOp::WriteRef;

        // Set the state so that we modify fragments only when the stencil value is 1.
        gfx::Painter::RenderPassState mask_draw_color_state;
        mask_draw_color_state.render_pass = gfx::RenderPass::ColorPass;
        mask_draw_color_state.cds.bWriteColor   = true;
        mask_draw_color_state.cds.stencil_ref   = 1;
        mask_draw_color_state.cds.stencil_mask  = 0xff;
        mask_draw_color_state.cds.stencil_func  = gfx::Painter::StencilFunc::RefIsEqual;
        mask_draw_color_state.cds.stencil_dpass = gfx::Painter::StencilOp::DontModify;
        mask_draw_color_state.cds.stencil_dfail = gfx::Painter::StencilOp::DontModify;

        gfx::Transform mask_transform;
        mask_transform.Resize(200.0f, 200.0f);
        mask_transform.MoveTo(300.0f, 300.0f);

        gfx::Transform shape_transform;
        shape_transform.Resize(200.0f, 200.0f);
        shape_transform.MoveTo(300.0f, 300.0f);

        gfx::Painter::DrawCommandState cmd_state;

        gfx::StencilShaderProgram stencil_program;
        gfx::GenericShaderProgram color_program;

        painter.Draw(gfx::Circle(),gfx::CreateMaterialFromColor(gfx::Color::White), mask_transform,
            stencil_program, mask_expose_state, cmd_state);

        painter.Draw(gfx::Rectangle(), gfx::CreateMaterialFromColor(gfx::Color::HotPink), shape_transform,
             color_program, mask_draw_color_state, cmd_state);
    }
    std::string GetName() const override
    { return "StencilExposeTest"; }
private:
};

class TextureBlurTest : public GraphicsTest
{
public:
    TextureBlurTest()
    {
        gfx::TextureMap2DClass material(gfx::MaterialClass::Type::Texture);
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

    void Render(gfx::Painter& painter) override
    {
        gfx::FillRect(painter, gfx::FRect(100, 100, 256, 256), *mBlur1024x1024);
        gfx::FillRect(painter, gfx::FRect(100, 400, 256, 256), *mClear1024x1024);

        gfx::FillRect(painter, gfx::FRect(400, 100, 256, 256), *mBlur512x512);
        gfx::FillRect(painter, gfx::FRect(400, 400, 256, 256), *mClear512x512);

        gfx::FillRect(painter, gfx::FRect(700, 100, 256, 256), *mBlur256x256);
        gfx::FillRect(painter, gfx::FRect(700, 400, 256, 256), *mClear256x256);
    }
    std::string GetName() const override
    { return "TextureBlurTest"; }
private:
    std::unique_ptr<gfx::Material> mBlur1024x1024;
    std::unique_ptr<gfx::Material> mBlur512x512;
    std::unique_ptr<gfx::Material> mBlur256x256;

    std::unique_ptr<gfx::Material> mClear1024x1024;
    std::unique_ptr<gfx::Material> mClear512x512;
    std::unique_ptr<gfx::Material> mClear256x256;
};

class TextureEdgeTest : public GraphicsTest
{
public:
    TextureEdgeTest()
    {
        gfx::TextureMap2DClass material(gfx::MaterialClass::Type::Texture);
        material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);

        {
            auto source = gfx::LoadTextureFromFile("textures/bird/bird-512x512.png");
            source->SetName("bird-512x512.png (edge)");
            source->SetEffect(gfx::TextureSource::Effect::Edges, true);
            material.SetTexture(std::move(source));
            mEdge512x512 = gfx::CreateMaterialInstance(material);
        }

        {
            auto source = gfx::LoadTextureFromFile("textures/bird/bird-512x512.png");
            source->SetName("bird-512x512.png (none)");
            material.SetTexture(std::move(source));
            material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            mClear512x512 = gfx::CreateMaterialInstance(material);
        }
    }

    void Render(gfx::Painter& painter) override
    {
        gfx::FillRect(painter, gfx::FRect(400, 100, 256, 256), *mEdge512x512);
        gfx::FillRect(painter, gfx::FRect(400, 400, 256, 256), *mClear512x512);
    }
    std::string GetName() const override
    { return "TextureEdgeTest"; }
private:
    std::unique_ptr<gfx::Material> mEdge512x512;
    std::unique_ptr<gfx::Material> mClear512x512;
};

class TextureColorExtractTest : public GraphicsTest
{
public:
    TextureColorExtractTest()
    {
        gfx::TextureMap2DClass material(gfx::MaterialClass::Type::Texture);
        material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Opaque);
        {
            auto source = gfx::LoadTextureFromFile("textures/rgbw_512x512.png");
            source->SetName("rgbw_512x512.png");
            material.SetTexture(std::move(source));
            mSource = gfx::CreateMaterialInstance(material);
        }
    }
    void Render(gfx::Painter& painter) override
    {
        gfx::FillRect(painter, gfx::FRect(100.0f, 200.0f, 256.0f, 256.0f), *mSource);

        auto* device = painter.GetDevice();

        if (!mColorR)
        {
            mColorR = gfx::CreateMaterialInstance(MakeMaterial("R", gfx::Color::Red, 0.75f, device));
        }
        if (!mColorG)
        {
            mColorG = gfx::CreateMaterialInstance(MakeMaterial("G", gfx::Color::Green, 0.75f, device));
        }
        if (!mColorB)
        {
            mColorB = gfx::CreateMaterialInstance(MakeMaterial("B", gfx::Color::Blue, 0.75f, device));
        }

        gfx::FillRect(painter, gfx::FRect(500.0f, 200.0f, 128.0f, 128.0f), *mColorR);
        gfx::FillRect(painter, gfx::FRect(650.0f, 200.0f, 128.0f, 128.0f), *mColorG);
        gfx::FillRect(painter, gfx::FRect(800.0f, 200.0f, 128.0f, 128.0f), *mColorB);

    }
    std::string GetName() const override
    {
        return "TextureColorExtractTest";
    }

private:
    gfx::MaterialClass MakeMaterial(const std::string& id, const gfx::Color4f& color_value, float threshold, gfx::Device* device) const
    {
        const auto& klass = mSource->GetClass();
        const auto& map   = klass->GetTextureMap(0);
        const auto* src   = map->GetTextureSource(0);

        gfx::TextureSource::Environment env;
        env.dynamic_content = false;
        // get the source texture handle
        gfx::Texture* src_texture = src->Upload(env, *device);
        gfx::Texture* dst_texture = device->MakeTexture(src_texture->GetId() + "/" + id);
        dst_texture->SetName(src_texture->GetName() + "/" + id);
        dst_texture->Allocate(src_texture->GetWidth(),
                              src_texture->GetHeight(),
                              gfx::Texture::Format::sRGBA);
        // disallow garbage collection since we're using the handle
        // in the material.
        dst_texture->SetGarbageCollection(false);

        gfx::algo::ExtractColor(src_texture, dst_texture, device, color_value, threshold);

        gfx::TextureMap2DClass material(gfx::MaterialClass::Type::Texture);
        material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Opaque);
        material.AddTexture(gfx::UseExistingTexture(dst_texture, ""));
        return material;
    }

private:
    std::unique_ptr<gfx::Material> mSource;
    std::unique_ptr<gfx::Material> mColorR;
    std::unique_ptr<gfx::Material> mColorG;
    std::unique_ptr<gfx::Material> mColorB;

};


class GradientTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        gfx::GradientClass material(gfx::MaterialClass::Type::Gradient);
        material.SetColor(gfx::Color::Red,   gfx::GradientClass::ColorIndex::GradientColor0);
        material.SetColor(gfx::Color::Green, gfx::GradientClass::ColorIndex::GradientColor2);
        material.SetColor(gfx::Color::Blue,  gfx::GradientClass::ColorIndex::GradientColor3);
        material.SetColor(gfx::Color::Black, gfx::GradientClass::ColorIndex::GradientColor1);
        material.SetGradientGamma(2.2f);
        gfx::FillRect(painter, gfx::FRect(0, 0, 400, 400), gfx::MaterialInstance(material));

        // *perceptually* linear gradient ramp
        material.SetColor(gfx::Color::Black,   gfx::GradientClass::ColorIndex::GradientColor0);
        material.SetColor(gfx::Color::Black,   gfx::GradientClass::ColorIndex::GradientColor2);
        material.SetColor(gfx::Color::White,   gfx::GradientClass::ColorIndex::GradientColor3);
        material.SetColor(gfx::Color::White,   gfx::GradientClass::ColorIndex::GradientColor1);
        gfx::FillRect(painter, gfx::FRect(500, 20, 400, 100), gfx::MaterialInstance(material));

        material.SetGradientWeight(glm::vec2(0.75, 0.0f));
        gfx::FillRect(painter, gfx::FRect(500, 140, 400, 100), gfx::MaterialInstance(material));

        material.SetGradientWeight(glm::vec2(0.25, 0.0f));
        gfx::FillRect(painter, gfx::FRect(500, 260, 400, 100), gfx::MaterialInstance(material));
    }
    std::string GetName() const override
    { return "GradientTest"; }
private:
};

class TextureTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        // whole texture (box = 1.0f)
        {
            gfx::TextureMap2DClass material(gfx::MaterialClass::Type::Texture);
            material.SetTexture(gfx::LoadTextureFromFile("textures/uv_test_512.png"));
            material.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            gfx::FillRect(painter, gfx::FRect(0, 0, 128, 128), gfx::MaterialInstance(material));

            material.SetTextureScaleX(2.0);
            material.SetTextureScaleY(2.0);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(150, 0, 128, 128), gfx::MaterialInstance(material));

            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(300, 0, 128, 128), gfx::MaterialInstance(material));

            material.SetTextureScaleX(-2.0);
            material.SetTextureScaleY(-2.0);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(450, 0, 128, 128), gfx::MaterialInstance(material));
        }

        // texture box > 1.0
        // todo: maybe just limit the box to 0.0, 1.0 range and dismiss this case ?
        {
            // clamp
            gfx::TextureMap2DClass material(gfx::MaterialClass::Type::Texture);
            material.SetTexture(gfx::LoadTextureFromFile("textures/uv_test_512.png"));
            material.SetTextureRect(0, 0, gfx::FRect(0.0, 0.0, 2.0, 1.0));
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(0, 150, 128, 128), gfx::MaterialInstance(material));

            material.SetTextureRect(gfx::FRect(0.0, 0.0, 2.0, 2.0));
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(150, 150, 128, 128), gfx::MaterialInstance(material));

            material.SetTextureRect(gfx::FRect(0.0, 0.0, 2.0, 2.0));
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(300, 150, 128, 128), gfx::MaterialInstance(material));

        }

        // texture box < 1.0
        {
            // basic case. sampling within the box.
            gfx::TextureMap2DClass material(gfx::MaterialClass::Type::Texture);
            material.SetTexture(gfx::LoadTextureFromFile("textures/uv_test_512.png"));
            material.SetTextureRect(gfx::FRect(0.5, 0.5, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(0, 300, 128, 128), gfx::MaterialInstance(material));

            // clamping with texture boxing.
            material.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Linear);
            material.SetTextureRect(gfx::FRect(0.0, 0.0, 0.5, 0.5));
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureScaleX(2.0);
            material.SetTextureScaleY(2.0);
            gfx::FillRect(painter, gfx::FRect(150, 300, 128, 128), gfx::MaterialInstance(material));

            // should be 4 squares each brick color (the top left quadrant of the source texture)
            material.SetTextureRect(gfx::FRect(0.0, 0.0, 0.5, 0.5));
            material.SetTextureScaleX(2.0);
            material.SetTextureScaleY(2.0);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(300, 300, 128, 128), gfx::MaterialInstance(material));

            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            material.SetTextureScaleX(1.0f);
            material.SetTextureScaleY(1.0f);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(450, 300, 128, 128), gfx::MaterialInstance(material));

            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            material.SetTextureScaleX(2.0f);
            material.SetTextureScaleY(2.0f);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(600, 300, 128, 128), gfx::MaterialInstance(material));

            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            material.SetTextureScaleX(2.0f);
            material.SetTextureScaleY(2.0f);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(750, 300, 128, 128), gfx::MaterialInstance(material));
        }

        // texture velocity + rotation
        {
            gfx::TextureMap2DClass material(gfx::MaterialClass::Type::Texture);
            material.SetTexture(gfx::LoadTextureFromFile("textures/uv_test_512.png"));
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            material.SetTextureVelocityX(0.2);
            gfx::FillRect(painter, gfx::FRect(0, 450, 128, 128), gfx::MaterialInstance(material, mTime));

            material.SetTextureVelocityX(0.0);
            material.SetTextureVelocityY(0.2);
            gfx::FillRect(painter, gfx::FRect(150, 450, 128, 128), gfx::MaterialInstance(material, mTime));

            material.SetTextureVelocityX(0.25);
            material.SetTextureVelocityY(0.2);
            material.SetTextureRect(gfx::FRect(0.25, 0.25, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(300, 450, 128, 128), gfx::MaterialInstance(material, mTime));

            material.SetTextureVelocityX(0.0f);
            material.SetTextureVelocityY(0.0f);
            material.SetTextureVelocityZ(3.134);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            gfx::FillRect(painter, gfx::FRect(450, 450, 128, 128), gfx::MaterialInstance(material, mTime));

            material.SetTextureVelocityX(0.0f);
            material.SetTextureVelocityY(0.0f);
            material.SetTextureVelocityZ(-3.134);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            gfx::FillRect(painter, gfx::FRect(600, 450, 128, 128), gfx::MaterialInstance(material, mTime));

            material.SetTextureVelocityX(0.0f);
            material.SetTextureVelocityY(0.0f);
            material.SetTextureVelocityZ(0.0f);
            material.SetTextureRotation(0.25f * math::Pi);
            material.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            material.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            gfx::FillRect(painter, gfx::FRect(750, 450, 128, 128), gfx::MaterialInstance(material, mTime));
        }
    }
    void Update(float dt) override
    {
        mTime += dt;
    }

    std::string GetName() const override
    { return "TextureTest"; }
private:
    float mTime = 0.0f;
};

class SpriteTest : public GraphicsTest
{
public:
    SpriteTest()
    {
        mMaterial = std::make_shared<gfx::SpriteClass>(gfx::MaterialClass::Type::Sprite);
        mMaterial->SetSurfaceType(gfx::MaterialClass::SurfaceType::Opaque);
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-1.png"));
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-2.png"));
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-3.png"));
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-4.png"));
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-5.png"));
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-6.png"));
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-7.png"));
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/bird/frame-8.png"));
        mMaterial->SetBlendFrames(false);
        mMaterial->GetTextureMap(0)->SetSpriteFrameRate(10.0f);
    }

    void Render(gfx::Painter& painter) override
    {
        // instance
        gfx::MaterialInstance material(mMaterial);
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

    void Update(float dt) override
    { mTime += dt; }
    std::string GetName() const override
    { return "SpriteTest"; }
private:
    void SetRect(const gfx::FRect& rect)
    {
        auto* map = mMaterial->GetTextureMap(0);
        for (size_t i=0; i<map->GetNumTextures(); ++i)
            map->SetTextureRect(i, rect);
    }

private:
    std::shared_ptr<gfx::SpriteClass> mMaterial;
    float mTime = 0.0f;
};

class SpriteSheetTest : public GraphicsTest
{
public:
    SpriteSheetTest()
    {
        mMaterial = std::make_shared<gfx::SpriteClass>(gfx::MaterialClass::Type::Sprite);
        mMaterial->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        mMaterial->SetBlendFrames(false);
        mMaterial->AddTexture(gfx::LoadTextureFromFile("textures/IdleSheet.png"));
        // the sheet has 32x32 pixel frames.
        // we're taking the idle animation from the middle of the sheet
        // with the character facing south.
        const float tile_size_px = 32;
        const float tile_height_px = 32;
        const float img_width_px = 256;
        const float img_height_px = 256;
        gfx::FRect rect;
        rect.Translate(0.0f, 4*tile_height_px / img_height_px);
        rect.SetWidth(1.0f);
        rect.SetHeight(tile_height_px / img_height_px);

        gfx::TextureMap::SpriteSheet sheet;
        sheet.rows = 1;
        sheet.cols = 8;

        auto* map = mMaterial->GetTextureMap(0);
        map->SetTextureRect(0, rect);
        map->SetSpriteSheet(sheet);
        map->SetSpriteLooping(true);
        map->SetSpriteFrameRate(15.0f);
    }

    void Render(gfx::Painter& painter) override
    {
        gfx::MaterialInstance material(mMaterial);
        material.SetRuntime(mTime);

        // whole texture
        {
            mMaterial->SetTextureScaleX(1.0);
            mMaterial->SetTextureScaleY(1.0);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(128, 128, 128, 128), material);

            mMaterial->SetTextureScaleX(2.0);
            mMaterial->SetTextureScaleY(2.0);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(128+150, 128, 128, 128), material);

            mMaterial->SetTextureScaleX(2.0);
            mMaterial->SetTextureScaleY(2.0);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(128+300, 128, 128, 128), material);

            mMaterial->SetTextureScaleX(-2.0);
            mMaterial->SetTextureScaleY(-2.0);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(128+450, 128, 128, 128), material);
        }

        // texture velocity + rotation
        {
            mMaterial->SetTextureScaleX(1.0f);
            mMaterial->SetTextureScaleY(1.0f);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
            mMaterial->SetTextureVelocityX(0.2);
            mMaterial->SetTextureVelocityY(0.0f);
            gfx::FillRect(painter, gfx::FRect(128, 350, 128, 128), material);

            mMaterial->SetTextureVelocityX(0.0);
            mMaterial->SetTextureVelocityY(0.2);
            gfx::FillRect(painter, gfx::FRect(128+150, 350, 128, 128), material);

            mMaterial->SetTextureVelocityX(0.0f);
            mMaterial->SetTextureVelocityY(0.0f);
            mMaterial->SetTextureVelocityZ(3.134);
            mMaterial->SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
            mMaterial->SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(128+450, 350, 128, 128), material);
        }

        mMaterial->SetTextureVelocityX(0.0f);
        mMaterial->SetTextureVelocityY(0.0f);
        mMaterial->SetTextureVelocityZ(0.0f);
        mMaterial->SetTextureScaleX(1.0f);
        mMaterial->SetTextureScaleY(1.0f);
        mMaterial->SetTextureRotation(0.0f);
    }

    void Update(float dt) override
    { mTime += dt; }
    std::string GetName() const override
    { return "SpriteSheetTest"; }
private:
    std::shared_ptr<gfx::SpriteClass> mMaterial;
    float mTime = 0.0f;
};

class TransformTest : public GraphicsTest
{
public:
    TransformTest()
    {
        mRobot = std::make_unique<BodyPart>("Robot");
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

    void Render(gfx::Painter& painter) override
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
            tr.RotateAroundZ(math::Pi * 2.0f * angle);

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
                tr.RotateAroundZ(math::Pi * 2.0f * angle);
                tr.Push();
                    tr.Scale(20.0f, 20.0f);
                    painter.Draw(gfx::Rectangle(), tr, gfx::CreateMaterialFromColor(gfx::Color::Yellow));
                tr.Pop();
            tr.Pop();
        tr.Pop();
    }
    void Update(float dt) override
    {
        mRobot->Update(dt);
        mTime += dt;
    }
    std::string GetName() const override
    { return "TransformTest"; }
private:
    class BodyPart
    {
    public:
        explicit BodyPart(std::string name) noexcept
          : mName(std::move(name))
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
            trans.RotateAroundZ(mRotation + ROM * angle);
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
            mBodyparts.emplace_back(BodyPart(name));
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
        gfx::Color mColor = gfx::Color::White;
    };
    std::unique_ptr<BodyPart> mRobot;

private:
    float mTime = 0.0f;
};

template<typename Shape>
class ShapeTest : public GraphicsTest
{
public:
    explicit ShapeTest(std::string name) noexcept
       : mName(std::move(name))
    {}
    void Render(gfx::Painter& painter) override
    {
        auto klass = gfx::CreateMaterialClassFromImage("textures/uv_test_512.png");
        // in order to validate the texture coordinates let's set
        // the filtering to nearest and clamp to edge on sampling
        klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Nearest);
        klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Nearest);
        klass.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);
        klass.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
        gfx::MaterialInstance material(klass);

        gfx::Transform transform;
        transform.Scale(200.0f, 200.0f);

        transform.Translate(10.0f, 10.0f);
        painter.Draw(gfx::Wireframe<Shape>(gfx::SimpleShapeStyle::Solid), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::SimpleShapeStyle::Outline), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::SimpleShapeStyle::Solid), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::SimpleShapeStyle::Outline), transform, material, 10.0f);

        transform.MoveTo(10.0f, 250.0f);
        transform.Resize(200.0f, 100.0f);
        painter.Draw(gfx::Wireframe<Shape>(gfx::SimpleShapeStyle::Solid), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::SimpleShapeStyle::Outline), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::SimpleShapeStyle::Solid), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(gfx::Wireframe<Shape>(gfx::SimpleShapeStyle::Solid), transform, material, 2.0);

        transform.MoveTo(60.0f, 400.0f);
        transform.Resize(100.0f, 200.0f);
        painter.Draw(gfx::Wireframe<Shape>(gfx::SimpleShapeStyle::Solid), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::SimpleShapeStyle::Outline), transform, material);
        transform.Translate(250.0f, 0.0f);
        painter.Draw(Shape(gfx::SimpleShapeStyle::Solid), transform, material);
    }
    void Update(float dt) override
    {}
    std::string GetName() const override
    { return mName; }
private:
    const std::string mName;
};

class RenderParticleTest : public GraphicsTest
{
public:
    using ParticleEngine = gfx::ParticleEngineInstance;

    RenderParticleTest()
    {
        {
            gfx::ParticleEngineClass::Params p;
            p.mode = gfx::ParticleEngineClass::SpawnPolicy::Continuous;
            p.boundary = gfx::ParticleEngineClass::BoundaryPolicy::Kill;
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
            mFire = std::make_unique<gfx::ParticleEngineInstance>(p);
        }

        {
            gfx::ParticleEngineClass::Params p;
            p.mode = gfx::ParticleEngineClass::SpawnPolicy::Continuous;
            p.boundary = gfx::ParticleEngineClass::BoundaryPolicy::Kill;
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
            mSmoke = std::make_unique<gfx::ParticleEngineInstance>(p);
        }

        {
            gfx::ParticleEngineClass::Params p;
            p.mode = gfx::ParticleEngineClass::SpawnPolicy::Continuous;
            p.boundary = gfx::ParticleEngineClass::BoundaryPolicy::Kill;
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
            mBlood = std::make_unique<gfx::ParticleEngineInstance>(p);
        }

        {
            gfx::ParticleEngineClass::Params p;
            p.mode = gfx::ParticleEngineClass::SpawnPolicy::Continuous;
            p.boundary = gfx::ParticleEngineClass::BoundaryPolicy::Kill;
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
            mClouds = std::make_unique<gfx::ParticleEngineInstance>(p);
        }

    }

    void Render(gfx::Painter& painter) override
    {
        gfx::Transform model;
        model.Resize(300, 300);
        model.Translate(-150, -150);
        model.RotateAroundZ(math::Pi);
        model.Translate(150 + 100, 150 + 300);

        gfx::TextureMap2DClass material(gfx::MaterialClass::Type::Texture);
        material.SetTexture(gfx::LoadTextureFromFile("textures/BlackSmoke.png"));
        material.SetBaseColor(gfx::Color4f(35, 35, 35, 20));
        material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        painter.Draw(*mSmoke, model, gfx::MaterialInstance(material));

        material.SetBaseColor(gfx::Color4f(0x71, 0x38, 0x0, 0xff));
        material.SetTexture(gfx::LoadTextureFromFile("textures/BlackSmoke.png"));
        material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Emissive);
        painter.Draw(*mFire, model, gfx::MaterialInstance(material));

        material.SetBaseColor(gfx::Color4f(234, 5, 3, 255));
        material.SetTexture(gfx::LoadTextureFromFile("textures/RoundParticle.png"));
        material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        model.Translate(500, 0);
        painter.Draw(*mBlood, model, gfx::MaterialInstance(material));

        material.SetBaseColor(gfx::Color4f(224, 224, 224, 255));
        material.SetTexture(gfx::LoadTextureFromFile("textures/WhiteCloud.png"));
        material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);

        model.Reset();
        model.Resize(2000, 200);
        model.MoveTo(-100, 100);
        painter.Draw(*mClouds, model, gfx::MaterialInstance(material));
    }
    void Update(float dt) override
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
    void Start() override
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

    std::string GetName() const override
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
    void Render(gfx::Painter& painter) override
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
    std::string GetName() const override
    { return "TextRectScaleTest"; }

    void Update(float dt) override
    {
        // full circle in 2s
        const auto angular_velo = math::Pi;
        mAngle += (angular_velo * dt);
    }
    bool IsFeatureTest() const override
    { return false; }
private:
    float mAngle = 0.0f;
};

class TextAlignTest : public GraphicsTest
{
public:
    TextAlignTest(std::string name, std::string font, const gfx::Color4f& color, unsigned size)
      : mName(std::move(name))
      , mFont(std::move(font))
      , mColor(color)
      , mFontSize(size)
    {}

    void Render(gfx::Painter& painter) override
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
    std::string GetName() const override
    { return mName;  }

    void Update(float dt) override
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
    void Render(gfx::Painter& painter) override
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
            transform.RotateAroundZ(angle);
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
    void Update(float dts) override
    {
        mTime += dts;
    }
    std::string GetName() const override
    { return "TextTest"; }
private:
    float mTime = 0.0f;
};

class FillShapeTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
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
    std::string GetName() const override
    { return "FillShapeTest"; }
private:
};

class DrawShapeOutlineTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
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
    std::string GetName() const override
    { return "DrawShapeOutline"; }
private:
};

class sRGBWindowTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        gfx::CustomMaterialClass srgb_out(gfx::MaterialClass::Type::Custom);
        gfx::CustomMaterialClass linear_out(gfx::MaterialClass::Type::Custom);

        // If we choose a reference value of #808080FF in GIMP this is approximately
        // half way gray. But the value that the GIMP shows in the color picker is
        // sRGB encoded, so the actual *linear* value is approx 0.18.
        // If we then use the actual liner value in the shader and write it to the
        // (supposedly) sRGB encoded color buffer then output should be such that
        //
        // IF the system does sRGB encoding
        // => the shader that writes sRGB values should produce gray that is
        //    too bright when compared to the GIMPs reference.
        // => the shader that writes linear values should produce gray that
        //    matches the reference gray. In other words reading back the value
        //    should be 0.5 (or approx #808080 through a screenshot)
        //
        // IF the system doesn't do sRGB encoding
        // => the shader that writes sRGB values should produce gray that
        //    matches the reference gray.
        // => the shader that writes linear values should produce gray that
        //    is too dark.

        // compute matching linear value for 0.5 and pass it to the shader
        const float color = gfx::sRGB_decode(0.5f);

        class TestProgram : public gfx::ShaderProgram {
        public:
            std::string GetName() const override
            { return "TestProgram"; }

            std::string GetShaderId(const gfx::Material& material, const gfx::Material::Environment& env) const override
            {
                return material.GetShaderId(env);
            }
            std::string GetShaderId(const gfx::Drawable& drawable, const gfx::Drawable::Environment& env) const override
            {
                return drawable.GetShaderId(env);
            }


            gfx::ShaderSource GetShader(const gfx::Drawable& drawable, const gfx::Drawable::Environment& env, const gfx::Device& device) const override
            {
                gfx::ShaderSource source;
                source.SetType(gfx::ShaderSource::Type::Vertex);
                source.LoadRawSource(R"(
#version 300 es

struct VS_OUT {
    // vertex position in clip space (after projection transformation)
    vec4 clip_position;
    // vertx position in eye coordinates (after camera/view transformation)
    vec4 view_position;
    // view space surface normal vector
    vec3 view_normal;
    // view space surface tagent vector
    vec3 view_tangent;
    // view space surface bi-tangent vector
    vec3 view_bitangent;
    // point size for GL_POINTS rasterization.
    float point_size;

    bool need_tbn;
    bool have_tbn;
} vs_out;

void VertexShaderMain();

void main() {
    vs_out.have_tbn = false;
    vs_out.need_tbn = false;
    VertexShaderMain();

    gl_PointSize = vs_out.point_size;
    gl_Position  = vs_out.clip_position;
}
)");
                source.AddPreprocessorDefinition("DRAWABLE_FLAGS_FLIP_UV_VERTICALLY", static_cast<unsigned>(gfx::DrawableFlags::Flip_UV_Vertically));
                source.AddPreprocessorDefinition("DRAWABLE_FLAGS_FLIP_UV_HORIZONTALLY", static_cast<unsigned>(gfx::DrawableFlags::Flip_UV_Horizontally));
                source.Merge(drawable.GetShader(env, device));
                return source;
            }

            gfx::ShaderSource GetShader(const gfx::Material& material, const gfx::Material::Environment& env, const gfx::Device& device) const override
            {
                auto source = material.GetShader(env, device);
                source.SetType(gfx::ShaderSource::Type::Fragment);
                source.SetPrecision(gfx::ShaderSource::Precision::High);
                source.SetVersion(gfx::ShaderSource::Version::GLSL_100);
                return source;
            }
        private:
        } program;

        srgb_out.SetShaderSrc(R"(
uniform float kColor;

float sRGB_encode(float value)
{
   return value <= 0.0031308
       ? value * 12.92
       : pow(value, 1.0/2.4) * 1.055 - 0.055;
}
vec4 sRGB_encode(vec4 color)
{
   vec4 ret;
   ret.r = sRGB_encode(color.r);
   ret.g = sRGB_encode(color.g);
   ret.b = sRGB_encode(color.b);
   ret.a = color.a; // alpha is always linear
   return ret;
}
void main() {
  gl_FragColor = sRGB_encode(vec4(kColor, kColor, kColor, 1.0));
})");

        linear_out.SetShaderSrc(R"(
uniform float kColor;

void main() {
  gl_FragColor = vec4(kColor, kColor, kColor, 1.0);
})");
        srgb_out.SetUniform("kColor", color);
        linear_out.SetUniform("kColor", color);

        gfx::Transform model_to_world;
        model_to_world.Resize(256.0f, 256.0f);
        model_to_world.Translate(20.0f, 20.0f);

        gfx::Painter::DrawState state;
        state.write_color  = true;
        state.stencil_func = gfx::Painter::StencilFunc::Disabled;
        state.depth_test   = gfx::Painter::DepthTest ::Disabled;

        painter.Draw(gfx::Rectangle(), model_to_world, gfx::MaterialInstance(srgb_out), state, program, gfx::Painter::LegacyDrawState());
        model_to_world.Translate(256.0f, 0.0f);
        model_to_world.Translate(20.0f, 0.0f);
        painter.Draw(gfx::Rectangle(), model_to_world, gfx::MaterialInstance(linear_out), state, program, gfx::Painter::LegacyDrawState());
    }
    std::string GetName() const override
    { return "sRGBWindowTest"; }
    bool IsFeatureTest() const override
    { return false; }
private:
};

class sRGBTextureSampleTest : public GraphicsTest
{
public:
    void Start() override
    {
        // inspect the image
        {
            gfx::Image img;
            img.Load("textures/black-gray-white.png");
            ASSERT(img.GetDepthBits() == 24);
            ASSERT(img.GetWidth() == 2);
            ASSERT(img.GetHeight() == 2);

            auto view = img.GetReadView();
            gfx::Pixel_RGB values[4];
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
            gfx::MaterialClass material(gfx::MaterialClass::Type::Color);
            material.SetShaderSrc(R"(
float sRGB_decode(float value)
{
    return value <= 0.04045
        ? value / 12.92
        : pow((value + 0.055) / 1.055, 2.4);
}
vec4 sRGB_decode(vec4 color)
{
   vec4 ret;
   ret.r = sRGB_decode(color.r);
   ret.g = sRGB_decode(color.g);
   ret.b = sRGB_decode(color.b);
   ret.a = color.a; // alpha is always linear
   return ret;
}

void CustomFragmentShaderMain()
{
    float color;

    vec2 coords = GetTextureCoords();
    float x = coords.x;
    float y = coords.y;
    if (x < 0.5)
    {
      if (y < 0.5)
        color = 0.0;
      else color = 127.0;
    }
    else
    {
      if (y < 0.5)
       color = 64.0;
      else color = 255.0;
    }
    color = color / 255.0;

    fs_out.color = sRGB_decode(vec4(color, color, color, 1.0));
}
)");
            mMaterialReference = gfx::CreateMaterialInstance(material);

        }

        // have a reference image created in TEH GIMP! that has 4 pixels
        // 0x00, 0x40, 0x80 and 0xff
        // remember that these values are sRGB encoded values! Using a shader
        // we can reproduce the same colors by using floating point approximations
        // of the same sRBG values and then converting them to linear.
        //
        // We can then proceed to rasterize two rectangles with the reference texture.
        // One with sRGB flag set and the other with sRGB flag NOT set (i.e. linear).
        // These rectangles can then be overlaid by a a rectangle that is shaded
        // with pixels  generated by the shader above that reproduces the values
        // expected to be in the texture.
        //
        // If the overlay with the sRGB texture is *seamless* it means that sRGB
        // texture is working.
        //
        // NOTE that this doesn't mean that the *actual* colors overall are correct
        // since the representation of the render buffer can still be broken
        // separately. !

        {
            gfx::TextureMap2DClass material(gfx::MaterialClass::Type::Texture);
            auto source = std::make_unique<gfx::TextureFileSource>();
            source->SetFileName("textures/black-gray-white.png");
            source->SetColorSpace(gfx::TextureSource::ColorSpace::sRGB);
            material.SetTexture(std::move(source));
            material.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            mMaterialSRGB = gfx::CreateMaterialInstance(std::move(material));
        }

        {
            gfx::TextureMap2DClass material(gfx::MaterialClass::Type::Texture);
            auto source = std::make_unique<gfx::TextureFileSource>();
            source->SetFileName("textures/black-gray-white.png");
            source->SetColorSpace(gfx::TextureSource::ColorSpace::Linear);
            material.SetTexture(std::move(source));
            material.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            mMaterialLinear = gfx::CreateMaterialInstance(std::move(material));
        }
    }

    void Render(gfx::Painter& painter) override
    {
        gfx::FillRect(painter, gfx::FRect(20.0f, 20.0f, 400.0f, 400.0f), *mMaterialSRGB);
        gfx::FillRect(painter, gfx::FRect(20.0f + 72.0f, 20.0f + 72.0f, 256.0f, 256.0f), *mMaterialReference);

        gfx::FillRect(painter, gfx::FRect(520.0f, 20.0f, 400.0f, 400.0f), *mMaterialLinear);
        gfx::FillRect(painter, gfx::FRect(520.0f + 72.0f, 20.0f + 72.0f, 256.0f, 256.0f), *mMaterialReference);
    }
    std::string GetName() const override
    { return "sRGBTextureSampleTest"; }
    bool IsFeatureTest() const override
    { return false; }
private:
    std::unique_ptr<gfx::Material> mMaterialSRGB;
    std::unique_ptr<gfx::Material> mMaterialLinear;
    std::unique_ptr<gfx::Material> mMaterialReference;
};

class PremultiplyAlphaTest : public GraphicsTest
{
public:
    void Start() override
    {
        const char* src = R"(
#version 300 es

uniform sampler2D kTexture;

in vec2 vTexCoord;

void FragmentShaderMain() {
    vec4 foo = texture2D(kTexture, vTexCoord);
    fs_out.color = vec4(foo.rgb, foo.a);
})";
        {
            gfx::TextureMap2D map;
            map.SetNumTextures(1);
            map.SetTextureSource(0, gfx::LoadTextureFromFile("textures/alpha-cutout.png"));
            map.SetSamplerName("kTexture");
            map.SetName("kTexture");

            gfx::CustomMaterialClass material(gfx::MaterialClass::Type::Custom);
            material.SetShaderSrc(src);
            material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            material.SetNumTextureMaps(1);
            material.SetTextureMap(0, map);
            material.SetTextureMagFilter(gfx::TextureMap2DClass::MagTextureFilter::Linear);
            mMaterialStraightAlpha = gfx::CreateMaterialInstance(material);
        }
        {
            gfx::TextureMap2D map;
            auto tex = gfx::LoadTextureFromFile("textures/alpha-cutout.png");
            tex->SetFlag(gfx::TextureFileSource::Flags::PremulAlpha, true);
            map.SetNumTextures(1);
            map.SetTextureSource(0, std::move(tex));
            map.SetSamplerName("kTexture");
            map.SetName("kTexture");

            gfx::CustomMaterialClass material(gfx::MaterialClass::Type::Custom);
            material.SetShaderSrc(src);
            material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            material.SetTextureMagFilter(gfx::TextureMap2DClass::MagTextureFilter::Linear);
            material.SetFlag(gfx::MaterialClass::Flags::PremultipliedAlpha, true);
            material.SetNumTextureMaps(1);
            material.SetTextureMap(0, std::move(map));
            mMaterialPremultAlpha = gfx::CreateMaterialInstance(material);
        }
    }
    void Render(gfx::Painter& painter) override
    {
        gfx::FillRect(painter, gfx::FRect(10.0f, 10.0, 512.0f, 512.0f), *mMaterialStraightAlpha);
        gfx::FillRect(painter, gfx::FRect(500.0f, 10.0, 512.0f, 512.0f), *mMaterialPremultAlpha);
    }
    std::string GetName() const override
    { return "PremultiplyAlphaTest"; }
    bool IsFeatureTest() const override
    { return true; }
private:
    std::unique_ptr<gfx::Material> mMaterialStraightAlpha;
    std::unique_ptr<gfx::Material> mMaterialPremultAlpha;
};

class PrecisionTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        // test transformation precision by rendering overlapping
        // objects after doing different transformations. the green
        // rectangle rendered after the red rectangle should completely
        // cover the red rectangle.
        {
            gfx::Transform t;
            t.Resize(200.0f, 200.0f);
            t.Translate(300.0f, 300.0f);
            painter.Draw(gfx::Rectangle(), t, gfx::CreateMaterialFromColor(gfx::Color::Red));
        }

        {
            gfx::Transform t;
            t.Resize(200.0f, 200.0f);
            t.Translate(-100.0f, -100.0f);
            t.RotateAroundZ(math::Pi);
            t.Translate(100.0f, 100.0f);
            t.Translate(300.0f, 300.0f);
            painter.Draw(gfx::Rectangle(), t, gfx::CreateMaterialFromColor(gfx::Color::Green));
        }

    }
    std::string GetName() const override
    { return "PrecisionTest"; }
    bool IsFeatureTest() const override
    { return true; }
private:
};

class Draw3DTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        constexpr auto const aspect = 1024.0 / 768.0f;

        gfx::Painter p(painter);
        p.ResetViewMatrix();
        p.SetProjectionMatrix(gfx::MakePerspectiveProjection(gfx::FDegrees { 45.0f }, aspect, 1.0f, 100.0f));

        gfx::FlatShadedColorProgram program;

        gfx::Painter::DrawState state;
        state.depth_test   = gfx::Painter::DepthTest::LessOrEQual;
        state.stencil_func = gfx::Painter::StencilFunc::Disabled;
        state.write_color  = true;

        const auto t = base::GetTime();

        {
            gfx::Transform transform;

            transform.Resize(2.0f, -2.0f, 2.0f);
            transform.RotateAroundY(std::sin(t));
            transform.RotateAroundX(std::cos(t));
            transform.MoveTo(2.5f, 0.0f, -10.0f);
            transform.Push();
               transform.Translate(-0.5f, -0.5f, 0.0f);

            p.Draw(gfx::Rectangle(), transform, gfx::CreateMaterialFromImage("textures/uv_test_512.png"),
                   state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::None));

            /*
            gfx::TextBuffer::Text text;
            text.font = "fonts/nuskool_krome_64x64.json";
            text.fontsize = 32;
            text.text = "hello\n";
            text.lineheight = 1.0f;
            gfx::TextBuffer buffer(100, 100);
            buffer.SetText(std::move(text));
            p.Draw(gfx::Rectangle(), transform, gfx::CreateMaterialFromText(std::move(buffer)),
                   state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::None));
                   */
        }

        // cube reference
        {
            gfx::Transform transform;
            transform.Resize(2.0f, 2.0f, 2.0f);
            transform.RotateAroundY(std::sin(t));
            transform.RotateAroundX(std::cos(t));
            transform.MoveTo(-2.5f, 0.0f, -10.0f);
            p.Draw(gfx::Cube(), transform, gfx::CreateMaterialFromImage("textures/uv_test_512.png"),
                   state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));
        }
    }
    std::string GetName() const override
    { return "Draw3DTest"; }
    bool IsFeatureTest() const override
    { return false; }
};


class Shape3DTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        constexpr auto const aspect = 1024.0f / 768.0f;
        constexpr auto Fov = 45.0f;
        constexpr auto Far = 10000.0f;
        const auto half_width = 1024.0f*0.5f;
        const auto half_height = 768.0f*0.5f;
        const auto ortho = glm::ortho(-half_width, half_width, -half_height, half_height, -1000.0f, 1000.0f);
        const auto near = half_height / std::tan(glm::radians(Fov*0.5f));
        const auto projection = ortho *
                glm::translate(glm::mat4(1.0f), glm::vec3 { 0.0f, 0.0f, -1000.0f} ) *
                glm::inverse(ortho) *
                glm::perspective(glm::radians(Fov), aspect, near, Far) *
                glm::translate(glm::mat4(1.0f), glm::vec3 { 0.0f, 0.0f, -near });

        gfx::Painter p(painter);
        p.ResetViewMatrix();
        p.SetProjectionMatrix(projection);

        gfx::FlatShadedColorProgram program;

        gfx::Painter::DrawState state;
        state.depth_test   = gfx::Painter::DepthTest::LessOrEQual;
        state.stencil_func = gfx::Painter::StencilFunc::Disabled;
        state.write_color  = true;

        const auto t = mTime;

        gfx::Transform transform;
        transform.Resize(100.0f, 100.0f, 100.0f);
        transform.RotateAroundY(std::sin(t));
        transform.RotateAroundX(std::cos(t));
        transform.Translate(-half_width, half_height);

        transform.Translate(100.0f, -100.0f);
        p.Draw(gfx::Pyramid(), transform, gfx::CreateMaterialFromImage("textures/uv_test_512.png"),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

        transform.Translate(200.0f, 0.0f);
        p.Draw(gfx::Cube(), transform, gfx::CreateMaterialFromImage("textures/uv_test_512.png"),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

        transform.Translate(200.0f, 0.0f);
        p.Draw(gfx::Cylinder(), transform, gfx::CreateMaterialFromImage("textures/uv_test_512.png"),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::None));

        transform.Translate(200.0f, 0.0f);
        p.Draw(gfx::Cone(), transform, gfx::CreateMaterialFromImage("textures/uv_test_512.png"),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

        transform.Translate(200.0f, 0.0f);
        p.Draw(gfx::Sphere(), transform, gfx::CreateMaterialFromImage("textures/uv_test_512.png"),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

        // wireframes

        transform.MoveTo(-half_width, half_height);
        transform.Translate(100.0f, -300.0f);
        p.Draw(gfx::Wireframe<gfx::Pyramid>(), transform, gfx::CreateMaterialFromImage("textures/uv_test_512.png"),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

        transform.Translate(200.0f, 0.0f);
        p.Draw(gfx::Wireframe<gfx::Cube>(), transform, gfx::CreateMaterialFromImage("textures/uv_test_512.png"),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

        transform.Translate(200.0f, 0.0f);
        p.Draw(gfx::Wireframe<gfx::Cylinder>(), transform, gfx::CreateMaterialFromImage("textures/uv_test_512.png"),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::None));

        transform.Translate(200.0f, 0.0f);
        p.Draw(gfx::Wireframe<gfx::Cone>(), transform, gfx::CreateMaterialFromImage("textures/uv_test_512.png"),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

        transform.Translate(200.0f, 0.0f);
        p.Draw(gfx::Wireframe<gfx::Sphere>(),transform, gfx::CreateMaterialFromImage("textures/uv_test_512.png"),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

        // normals, tangents and bitangents

        transform.MoveTo(-half_width, half_height);
        transform.Translate(100.0f, -500.0f);
        p.Draw(gfx::Pyramid(), transform, gfx::CreateMaterialFromColor(gfx::Color::DarkGray),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));
        p.Draw(gfx::NormalMesh<gfx::Pyramid>(mFlags), transform, gfx::CreateMaterialFromColor(gfx::Color::HotPink),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

        transform.Translate(200.0f, 0.0f);
        p.Draw(gfx::Cube(), transform, gfx::CreateMaterialFromColor(gfx::Color::DarkGray),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));
        p.Draw(gfx::NormalMesh<gfx::Cube>(mFlags), transform, gfx::CreateMaterialFromColor(gfx::Color::HotPink),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

        transform.Translate(200.0f, 0.0f);
        p.Draw(gfx::Cylinder(), transform, gfx::CreateMaterialFromColor(gfx::Color::DarkGray),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));
        p.Draw(gfx::NormalMesh<gfx::Cylinder>(mFlags), transform, gfx::CreateMaterialFromColor(gfx::Color::HotPink),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

        transform.Translate(200.0f, 0.0f);
        p.Draw(gfx::Cone(), transform, gfx::CreateMaterialFromColor(gfx::Color::DarkGray),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));
        p.Draw(gfx::NormalMesh<gfx::Cone>(mFlags), transform, gfx::CreateMaterialFromColor(gfx::Color::HotPink),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

        transform.Translate(200.0f, 0.0f);
        p.Draw(gfx::Sphere(), transform, gfx::CreateMaterialFromColor(gfx::Color::DarkGray),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

        // reduce the level of detail (the number of slices) in the sphere when visualizing
        // the normals to make it visually less crowded.
        p.Draw(gfx::NormalMesh<gfx::Sphere>(mFlags, gfx::Sphere::Style::Solid, 15), transform, gfx::CreateMaterialFromColor(gfx::Color::HotPink),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

    }
    std::string GetName() const override
    { return "Shape3DTest"; }
    bool IsFeatureTest() const override
    { return false; }

    void KeyDown(const wdk::WindowEventKeyDown& key) override
    {
        if (key.symbol == wdk::Keysym::Key1)
            mFlags.flip(gfx::DebugDrawableBase::Flags::Normals);
        else if (key.symbol == wdk::Keysym::Key2)
            mFlags.flip(gfx::DebugDrawableBase::Flags::Tangents);
        else if (key.symbol == wdk::Keysym::Key3)
            mFlags.flip(gfx::DebugDrawableBase::Flags::Bitangents);
        else if (key.symbol == wdk::Keysym::Space)
            mPaused = !mPaused;

    }
    void Update(float dt) override
    {
        if (!mPaused)
            mTime += dt;
    }
    void Start() override
    {
        mTime = 0.0f;
        mFlags.clear();
        mFlags.set(gfx::DebugDrawableBase::Flags::Normals);
    }
private:
    gfx::DebugDrawableBase::FlagBits mFlags;
    bool mPaused = false;
    float mTime = 0.0f;
};

class FramebufferTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        auto* device = painter.GetDevice();

        {
            auto* fbo = device->FindFramebuffer("fbo_msaa_disabled");
            if (fbo == nullptr)
            {
                gfx::Framebuffer::Config conf;
                conf.format = gfx::Framebuffer::Format::ColorRGBA8;
                conf.width = 512;
                conf.height = 512;
                conf.msaa = gfx::Framebuffer::MSAA::Disabled;
                fbo = device->MakeFramebuffer("fbo_msaa_disabled");
                fbo->SetConfig(conf);
            }
            gfx::Painter p(painter);
            p.SetSurfaceSize(512, 512);
            p.SetViewport(0, 0, 512, 512);
            p.SetProjectionMatrix(gfx::MakeOrthographicProjection(512, 512));
            p.ClearScissor();
            p.ResetViewMatrix();

            gfx::Transform transform;
            transform.Resize(400.0f, 400.0f);
            transform.Translate(-200.0f, -200.0f);
            transform.RotateAroundZ(mTime);
            transform.Translate(200.0f, 200.0f);
            transform.Translate(56.0f, 56.0f);

            p.SetFramebuffer(fbo);
            p.ClearColor(gfx::Color::Transparent);
            p.Draw(gfx::IsoscelesTriangle(), transform, gfx::CreateMaterialFromColor(gfx::Color::Green));

            gfx::Texture* result = nullptr;
            fbo->Resolve(&result);
            ASSERT(result);

            {
                gfx::Transform transform;
                transform.Resize(512.0f, 512.0f);
                transform.MoveTo(0.0f, 20.0f);
                gfx::MaterialClass klass(gfx::MaterialClass::Type::Texture);
                klass.SetTexture(gfx::UseExistingTexture(result, ""));
                klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
                painter.Draw(gfx::Rectangle(), transform, gfx::MaterialInstance(klass));
            }

            {
                auto* fbo = device->FindFramebuffer("fbo_msaa_enabled");
                if (fbo == nullptr)
                {
                    gfx::Framebuffer::Config conf;
                    conf.format = gfx::Framebuffer::Format::ColorRGBA8;
                    conf.width = 512;
                    conf.height = 512;
                    conf.msaa = gfx::Framebuffer::MSAA::Enabled;
                    fbo = device->MakeFramebuffer("fbo_msaa_enabled");
                    fbo->SetConfig(conf);
                }
                gfx::Painter p(painter);
                p.SetSurfaceSize(512, 512);
                p.SetViewport(0, 0, 512, 512);
                p.SetProjectionMatrix(gfx::MakeOrthographicProjection(512, 512));
                p.ClearScissor();
                p.ResetViewMatrix();

                gfx::Transform transform;
                transform.Resize(400.0f, 400.0f);
                transform.Translate(-200.0f, -200.0f);
                transform.RotateAroundZ(mTime);
                transform.Translate(200.0f, 200.0f);
                transform.Translate(56.0f, 56.0f);

                p.SetFramebuffer(fbo);
                p.ClearColor(gfx::Color::Transparent);
                p.Draw(gfx::IsoscelesTriangle(), transform, gfx::CreateMaterialFromColor(gfx::Color::Green));

                gfx::Texture* result = nullptr;
                fbo->Resolve(&result);
                ASSERT(result);

                {
                    gfx::Transform transform;
                    transform.Resize(512.0f, 512.0f);
                    transform.MoveTo(512.0f, 20.0f);
                    gfx::MaterialClass klass(gfx::MaterialClass::Type::Texture);
                    klass.SetTexture(gfx::UseExistingTexture(result, ""));
                    klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
                    painter.Draw(gfx::Rectangle(), transform, gfx::MaterialInstance(klass));
                }
            }
        }
    }
    std::string GetName() const override
    { return "FramebufferTest"; }
    bool IsFeatureTest() const override
    { return true;}
    void KeyDown(const wdk::WindowEventKeyDown& key) override
    {
        if (key.symbol == wdk::Keysym::Space)
        {
            mAccumulateTime = !mAccumulateTime;
        }
    }
    void Update(float dt) override
    {
        if (mAccumulateTime)
            mTime += dt;
    }
private:
    bool mAccumulateTime = true;
    double mTime = 0.0;
};


class DepthLayerTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        const auto surface_size = painter.GetSurfaceSize();
        const auto surface_width = surface_size.GetWidth();
        const auto surface_height = surface_size.GetHeight();

        const auto near_plane_distance = -10.0f;
        const auto far_plane_distance = 100.0f;

        // this projection matrix maps world space coordinates so that when,
        // surface_width  = 100
        // surface_height = 100
        //
        // world x =   0.0 => clip -1.0f
        // world x = 100.0 => clip  1.0f
        // world y =   0.0 => clip  1.0f
        // world y   100.0 => clip -1.0f
        //
        // this essentially flips the coordinates on Y axis.
        // Z (depth) remains the same thus when we're looking
        // along the negative z axis (into depth, positive Z coming
        // out the screen and pointing towards *you*) moving
        // more into negative z direction moves away from
        // the viewer adding more depth
        //

        painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(0.0f, surface_width,
                                                                    0.0f, surface_height,
                                                                    near_plane_distance, far_plane_distance));

        gfx::Painter::DrawState state;
        state.depth_test = gfx::Painter::DepthTest::LessOrEQual;

        gfx::FlatShadedColorProgram program;

        // layer 0
        gfx::Transform trans;
        trans.Resize(300.0f, 300.0f);
        trans.Translate(300.0f, 300.0f, 0.0f);
        painter.Draw(gfx::Rectangle(), trans,
                     gfx::CreateMaterialFromColor(gfx::Color::DarkGreen),
                     state, program);

        // layer 1, this should paint behind layer 0
        trans.Translate(75.0f, 75.0f, -10.0f);
        painter.Draw(gfx::Rectangle(), trans,
                     gfx::CreateMaterialFromColor(gfx::Color::DarkRed),
                     state, program);

        // layer 2, this should paint between 0 and 1
        trans.MoveTo(300.0f, 300.0f, 0.0f);
        trans.Translate(30.0f, -50.0f, -5.0f);
        painter.Draw(gfx::Rectangle(), trans,
                     gfx::CreateMaterialFromColor(gfx::Color::DarkBlue),
                     state, program);

        // layer 3, this should paint on top of layer 0
        trans.MoveTo(300.0f, 300.0f, 0.0f);
        trans.Translate(100.0f, 100.0f, 5.0f);
        trans.Resize(100.0f, 100.0f);
        painter.Draw(gfx::Rectangle(), trans,
                     gfx::CreateMaterialFromColor(gfx::Color::DarkYellow),
                     state, program);

    }

    std::string GetName() const override
    { return "DepthLayerTest"; }
    bool IsFeatureTest() const override
    { return false; }
private:

};

class Simple2DInstanceTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        gfx::Rectangle drawable;
        gfx::MaterialInstance material(gfx::CreateMaterialClassFromColor(gfx::Color::DarkGreen));

        gfx::Drawable::InstancedDraw instanced;
        instanced.gpu_id = "simple-2d-instance-test";
        instanced.usage = gfx::BufferUsage::Static;
        instanced.content_name = "simple-2d-instance-test";
        instanced.content_hash = 0; // irrelevant since we use static data and are not in editing mode.

        for (unsigned i=0; i<10; ++i)
        {
            for (unsigned j=0; j<10; ++j)
            {
                const auto xpos = 20.0f + j * 100.0f;
                const auto ypos = 20.0f + i * 45.0f;
                gfx::Drawable::DrawInstance inst;
                inst.model_to_world = MakeTransform(glm::vec2{xpos, ypos}, glm::vec2{80.0f, 38.0f});
                instanced.instances.push_back(inst);
            }
        }

        gfx::Painter::DrawCommand cmd;
        cmd.drawable = &drawable;
        cmd.material = &material;
        cmd.instanced_draw = instanced;

        gfx::Painter::DrawCommandList draw_list;
        draw_list.push_back(cmd);

        gfx::FlatShadedColorProgram program;

        gfx::Painter::RenderPassState render_pass_state;
        render_pass_state.render_pass = gfx::RenderPass::ColorPass;
        render_pass_state.cds.depth_test   = gfx::Painter::DepthTest::Disabled;
        render_pass_state.cds.stencil_func = gfx::Painter::StencilFunc::Disabled;
        render_pass_state.cds.bWriteColor  = true;

        painter.Draw(draw_list, program, render_pass_state);
    }
    std::string GetName() const override
    {
        return "Simple2DInstanceTest";
    }
    static glm::mat4 MakeTransform(const glm::vec2& pos, const glm::vec2& size)
    {
        gfx::Transform transform;
        transform.Resize(size);
        transform.MoveTo(pos);
        return transform.GetAsMatrix();
    }
private:
};

class Simple3DInstanceTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        constexpr auto const aspect = 1024.0f / 768.0f;
        constexpr auto Fov = 45.0f;
        constexpr auto Far = 10000.0f;
        const auto half_width = 1024.0f*0.5f;
        const auto half_height = 768.0f*0.5f;
        const auto ortho = glm::ortho(-half_width, half_height, -half_height, half_height, -1000.0f, 1000.0f);
        const auto near = half_height / std::tan(glm::radians(Fov*0.5f));
        const auto projection = ortho *
                                  glm::translate(glm::mat4(1.0f), glm::vec3 { 0.0f, 0.0f, -1000.0f} ) *
                                    glm::inverse(ortho) *
                                      glm::perspective(glm::radians(Fov), aspect, near, Far) *
                                        glm::translate(glm::mat4(1.0f), glm::vec3 { 0.0f, 0.0f, -near });

        gfx::Painter p(painter);
        p.ResetViewMatrix();
        p.SetProjectionMatrix(projection);

        gfx::Cube drawable;
        gfx::MaterialInstance material(gfx::CreateMaterialClassFromImage("textures/uv_test_512.png"));

        gfx::Drawable::InstancedDraw instanced;
        instanced.gpu_id = "simple-3d-instance-test";
        instanced.usage = gfx::BufferUsage::Stream;
        instanced.content_name = "simple-3d-instance-test";
        instanced.content_hash = 0; // irrelevant since we're doing stream (i.e. every render updates the VBO)

        gfx::Painter::DrawState state;
        state.depth_test   = gfx::Painter::DepthTest::LessOrEQual;
        state.stencil_func = gfx::Painter::StencilFunc::Disabled;
        state.culling      = gfx::Painter::Culling::Back;
        state.write_color  = true;

        gfx::FlatShadedColorProgram program;

        const auto t = mTime;

        for (unsigned i=0; i<10; ++i)
        {
            for (unsigned j=0; j<10; ++j)
            {
                const auto xpos =  100.0f + j * 80.0f;
                const auto ypos = -50.0f - i * 80.0f;

                gfx::Transform transform;
                transform.Resize(40.0f, 40.0f, 40.0f);
                transform.RotateAroundY(std::sin(t));
                transform.RotateAroundX(std::cos(t));
                transform.Translate(-half_width, half_height);
                transform.Translate(xpos, ypos);

                //p.Draw(drawable, transform, material, state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));

                gfx::Drawable::DrawInstance inst;
                inst.model_to_world = transform;
                instanced.instances.push_back(inst);
            }
        }

        gfx::Painter::DrawCommand cmd;
        cmd.drawable = &drawable;
        cmd.material = &material;
        cmd.state.culling  = gfx::Painter::Culling::Back;
        cmd.instanced_draw = instanced;

        gfx::Painter::DrawCommandList draw_list;
        draw_list.push_back(cmd);

        gfx::Painter::RenderPassState render_pass_state;
        render_pass_state.render_pass = gfx::RenderPass::ColorPass;
        render_pass_state.cds.depth_test   = gfx::Painter::DepthTest::Disabled;
        render_pass_state.cds.stencil_func = gfx::Painter::StencilFunc::Disabled;
        render_pass_state.cds.bWriteColor  = true;

        p.Draw(draw_list, program, render_pass_state);
    }
    void Update(float dt) override
    {
        mTime += dt;
    }

    std::string GetName() const override
    {
        return "Simple3DInstanceTest";
    }
private:
    double mTime = 0.0;
};

class PolygonInstanceTest : public GraphicsTest
{
public:
    PolygonInstanceTest()
    {
        // allow dynamic updates.
        mPolygon.SetDynamic(true);
    }

    void Render(gfx::Painter& painter) override
    {
        PacmanPolygon pacman;
        PacmanPolygon::Build(&mPolygon, mTime);

        gfx::Drawable::InstancedDraw instanced;
        instanced.gpu_id = "polygon-2d-instance-test";
        instanced.usage = gfx::BufferUsage::Static;
        instanced.content_name = "polygon-2d-instance-test";
        instanced.content_hash = 0; // irrelevant since we use static data and are not in editing mode.

        for (unsigned i=0; i<7; ++i)
        {
            for (unsigned j=0; j<10; ++j)
            {
                const auto xpos = 20.0f + j * 100.0f;
                const auto ypos = 20.0f + i * 100.0f;
                gfx::Drawable::DrawInstance inst;
                inst.model_to_world = MakeTransform(glm::vec2{xpos, ypos}, glm::vec2{80.0f, 80.0f});
                instanced.instances.push_back(inst);
            }
        }

        gfx::PolygonMeshInstance drawable(mPolygon);
        gfx::MaterialInstance material(gfx::CreateMaterialClassFromColor(gfx::Color::DarkYellow));

        gfx::Painter::DrawCommand cmd;
        cmd.drawable = &drawable;
        cmd.material = &material;
        cmd.instanced_draw = instanced;

        gfx::Painter::DrawCommandList draw_list;
        draw_list.push_back(cmd);

        gfx::Painter::RenderPassState render_pass_state;
        render_pass_state.render_pass =gfx::RenderPass::ColorPass;
        render_pass_state.cds.depth_test   = gfx::Painter::DepthTest::Disabled;
        render_pass_state.cds.stencil_func = gfx::Painter::StencilFunc::Disabled;
        render_pass_state.cds.bWriteColor  = true;

        gfx::FlatShadedColorProgram program;
        painter.Draw(draw_list, program, render_pass_state);
    }

    std::string GetName() const override
    {
        return "PolygonInstanceTest";
    }
    void Update(float dt) override
    {
        const float velocity = 5.23;
        mTime += (dt * velocity);
    }
private:
    static glm::mat4 MakeTransform(const glm::vec2& pos, const glm::vec2& size)
    {
        gfx::Transform transform;
        transform.Resize(size);
        transform.MoveTo(pos);
        return transform.GetAsMatrix();
    }
private:
    float mTime = 0.0f;
    gfx::PolygonMeshClass mPolygon;
};

class ParticleInstanceTest : public GraphicsTest
{
public:
    ParticleInstanceTest()
    {
        gfx::ParticleEngineClass::Params p;
        p.num_particles = 5;
        p.min_point_size = 20;
        p.max_point_size = 20;
        mClass = std::make_shared<gfx::ParticleEngineClass>(p);
        mInstance = std::make_unique<gfx::ParticleEngineInstance>(mClass);
    }

    void Render(gfx::Painter& painter) override
    {
        gfx::Drawable::InstancedDraw instanced;
        instanced.gpu_id = "particle-instance-test";
        instanced.usage = gfx::BufferUsage::Static;
        instanced.content_name = "particle-instance-test";
        instanced.content_hash = 0; // irrelevant since we use static data and are not in editing mode.

        for (unsigned i=0; i<7; ++i)
        {
            for (unsigned j=0; j<10; ++j)
            {
                const auto xpos = 20.0f + j * 100.0f;
                const auto ypos = 20.0f + i * 100.0f;
                gfx::Drawable::DrawInstance inst;
                inst.model_to_world = MakeTransform(glm::vec2{xpos, ypos}, glm::vec2{80.0f, 80.0f});
                instanced.instances.push_back(inst);
            }
        }

        gfx::MaterialInstance material(gfx::CreateMaterialClassFromColor(gfx::Color::DarkYellow));

        gfx::Painter::DrawCommand cmd;
        cmd.drawable = mInstance.get();
        cmd.material = &material;
        cmd.instanced_draw = instanced;

        gfx::Painter::DrawCommandList draw_list;
        draw_list.push_back(cmd);

        gfx::Painter::RenderPassState render_pass_state;
        render_pass_state.render_pass = gfx::RenderPass::ColorPass;
        render_pass_state.cds.depth_test   = gfx::Painter::DepthTest::Disabled;
        render_pass_state.cds.stencil_func = gfx::Painter::StencilFunc::Disabled;
        render_pass_state.cds.bWriteColor  = true;

        gfx::FlatShadedColorProgram program;
        painter.Draw(draw_list, program, render_pass_state);
    }

    void Start() override
    {
        gfx::Drawable::Environment env;
        mInstance->Restart(env);
    }
    void Update(float dt) override
    {
        gfx::Drawable::Environment env;
        mInstance->Update(env, dt);
    }
    std::string GetName() const override
    {
        return "ParticleInstanceTest";
    }
private:
    static glm::mat4 MakeTransform(const glm::vec2& pos, const glm::vec2& size)
    {
        gfx::Transform transform;
        transform.Resize(size);
        transform.MoveTo(pos);
        return transform.GetAsMatrix();
    }
private:
    std::shared_ptr<gfx::ParticleEngineClass> mClass;
    std::unique_ptr<gfx::ParticleEngineInstance> mInstance;
};

class OrthoDepthTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 1024.0f, 0, 768.0f, -1000.0f, 1000.0f));

        // draw two quads, confirm that that one with smaller Z value is
        // is "further into the scene", i.e. further away from the camera.

        gfx::Painter::DrawState state;
        state.depth_test = gfx::Painter::DepthTest::LessOrEQual;

        gfx::FlatShadedColorProgram program;

        {
            gfx::Transform transform;
            transform.Scale(400.0f, 400.0f);
            transform.Translate(100.0f, 100.0f, -999.0f); // close to the far plane
            painter.Draw(gfx::Rectangle(), transform, gfx::CreateMaterialFromColor(gfx::Color::Gray), state, program);
        }

        {
            gfx::Transform transform;
            transform.Scale(100.0f, 100.0f);
            transform.Translate(250.0f, 250.0f, 999.0f); // close to the near plane
            painter.Draw(gfx::Rectangle(), transform, gfx::CreateMaterialFromColor(gfx::Color::DarkRed), state, program);
        }

        // we should get a red square on top of gray square
    }
    std::string GetName() const override
    {
        return "OrthoDepthTest";
    }
    bool IsFeatureTest() const override
    {
        return false;
    }
private:
};

class MeshExplosionTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        gfx::Transform transform;
        transform.Resize(200.0f, 200.0f);
        transform.MoveTo(300.0f, 300.0f);
        painter.Draw(*mDrawable, transform, //gfx::CreateMaterialFromColor(gfx::Color::DarkGreen));
            gfx::CreateMaterialFromSprite("textures/asteroid_dark.png"));
    }
    void Start() override
    {
        auto klass = std::make_shared<gfx::RectangleClass>();
        auto inst = gfx::CreateDrawableInstance(klass);

        gfx::EffectDrawable::MeshExplosionEffectArgs args;
        args.mesh_subdivision_count = 1;
        args.shard_linear_speed = 1.0f;
        args.shard_linear_acceleration = 2.0f;
        args.shard_rotational_speed = 2.0f;
        args.shard_rotational_acceleration = 1.0f;

        auto effect = std::make_unique<gfx::EffectDrawable>(std::move(inst), base::RandomString(3));
        effect->SetEffectType(gfx::EffectDrawable::EffectType::ShardedMeshExplosion);
        effect->SetEffectArgs(args);
        effect->EnableEffect();
        mDrawable = std::move(effect);
    }
    void Update(float dt) override
    {
        if (!mDrawable)
            return;

        gfx::Drawable::Environment env;
        mDrawable->Update(env, dt);
    }

    std::string GetName() const override
    {
        return "MeshExplosionTest";
    }
private:
    std::unique_ptr<gfx::Drawable> mDrawable;
};

class WavefrontMeshTest : public GraphicsTest
{
public:
    void Render(gfx::Painter& painter) override
    {
        constexpr auto const aspect = 1024.0 / 768.0f;

        gfx::Painter p(painter);
        p.ResetViewMatrix();
        p.SetProjectionMatrix(gfx::MakePerspectiveProjection(gfx::FDegrees { 45.0f }, aspect, 1.0f, 100.0f));

        gfx::BasicLightProgram program;
        gfx::BasicLightProgram::Light light;
        light.type = gfx::BasicLightProgram::LightType::Directional;
        light.ambient_color  = gfx::Color4f(gfx::Color::Red) * 0.5;
        light.diffuse_color  = gfx::Color4f(gfx::Color::White);
        light.specular_color = gfx::Color4f(gfx::Color::Black);
        light.view_position  = glm::vec3 { 1.0f, 5.0f, 0.0f };
        light.view_direction = glm::vec3 { 0.0f, -1.0f, 0.0f };
        light.quadratic_attenuation = 0.005f;
        program.AddLight(light);

        gfx::Painter::DrawState state;
        state.depth_test   = gfx::Painter::DepthTest::LessOrEQual;
        state.stencil_func = gfx::Painter::StencilFunc::Disabled;
        state.write_color  = true;

        gfx::Transform transform;
        transform.Resize(5.0f, 5.0f, 5.0f);
        transform.RotateAroundY(base::FDegrees(180.0f));
        transform.RotateAroundY(std::sin(mTime));
        transform.RotateAroundX(std::cos(mTime));
        transform.MoveTo(0.0f, 0.0f, -10.0f);
        p.Draw(gfx::WavefrontMesh("models/bear3.obj"), transform, gfx::CreateMaterialFromColor(gfx::Color::DarkGreen),
               state, program, gfx::Painter::LegacyDrawState(gfx::Painter::Culling::Back));
    }
    void Update(float dt) override
    {
        mTime += dt;
    }
    void Start() override
    {
        mTime = 0.0f;
    }
    std::string GetName() const override
    {
        return "WavefrontMeshTest";
    }
private:
    float mTime = 0.0f;
};


class BasicFog3DTest : public GraphicsTest
{
public:
        using FogMode = gfx::GenericShaderProgram::FogMode;

    explicit BasicFog3DTest(FogMode mode) noexcept
      : mFogMode(mode)
      , mShapeIndex(2)
    {}

    void Render(gfx::Painter& painter) override
    {
        constexpr auto aspect = 1024.0 / 768.0f;
        const auto t = static_cast<float>(std::sin(mTime * 0.4) * 0.5 + 0.5);
        const auto index = mShapeIndex % 4;
        std::unique_ptr<gfx::Drawable> drawable;
        if (index == 0)
            drawable = std::make_unique<gfx::Sphere>();
        else if (index == 1)
            drawable = std::make_unique<gfx::Cone>();
        else if (index == 2)
            drawable = std::make_unique<gfx::Cube>();
        else if (index == 3)
            drawable = std::make_unique<gfx::Cylinder>();

        auto material = gfx::CreateMaterialFromColor(gfx::Color::DarkRed);
        material.SetFlag(gfx::Material::Flags::EnableFog, true);

        gfx::Painter p(painter);
        p.ResetViewMatrix();
        p.SetProjectionMatrix(gfx::MakePerspectiveProjection(gfx::FDegrees { 45.0f }, aspect, 1.0f, 100.0f));

        gfx::Painter::DrawState state;
        state.depth_test   = gfx::Painter::DepthTest::LessOrEQual;
        state.stencil_func = gfx::Painter::StencilFunc::Disabled;

        gfx::GenericShaderProgram program;
        gfx::GenericShaderProgram::Fog fog;
        fog.mode = mFogMode;
        fog.color = gfx::Color4f(gfx::Color::DarkGray);
        fog.start_depth= 0.0f;
        fog.end_depth  = 100.0f;
        fog.density    = 3.0f;

        program.EnableFeature(gfx::GenericShaderProgram::ShadingFeatures::BasicFog, true);
        program.SetFog(fog);

        for (int i = 0; i<5; ++i)
        {
            gfx::Transform transform;
            transform.Resize(2.0f, 2.0f, 2.0f);
            transform.RotateAroundY(std::sin(t));
            transform.RotateAroundX(std::cos(t));
            transform.MoveTo(-3.5f, 0.0f, -10.0f);
            transform.Translate(2.0f * std::pow(static_cast<float>(i), 1.873f), 0.0f, -5.0f * std::pow(static_cast<float>(i), 1.5f));
            p.Draw(*drawable, transform, material, state, program);
        }
    }
    std::string GetName() const override
    {
        return base::FormatString("Basic%1FogTest", mFogMode);
    }
    void KeyDown(const wdk::WindowEventKeyDown& key) override
    {
        if (key.symbol == wdk::Keysym::Space)
            ++mShapeIndex;
        else if (key.symbol == wdk::Keysym::Key1)
            mFogMode = FogMode::Linear;
        else if (key.symbol == wdk::Keysym::Key2)
            mFogMode = FogMode ::Exponential1;
        else if (key.symbol == wdk::Keysym::Key3)
            mFogMode = FogMode ::Exponential2;
    }
    void Start() override
    {
        mTime = 0.0f;
    }
    void Update(float dt) override
    {
        mTime += dt;
    }

private:
    FogMode mFogMode = FogMode::Linear;
    unsigned mShapeIndex = 0;
    float mTime = 0.0f;
};


class BasicLight3DTest : public GraphicsTest
{
public:
    using LightType = gfx::BasicLightProgram::LightType;

    BasicLight3DTest()
    {
        mLightType = LightType::Point;
    }
    explicit BasicLight3DTest(const glm::vec3& direction)
    {
        mLightType = LightType::Directional;
        mLightDirection = direction;
    }
    BasicLight3DTest(const glm::vec3& direction, base::FDegrees half_angle)
    {
        mLightType = LightType::Spot;
        mLightDirection = direction;
        mLightHalfAngle = half_angle;
    }

    void Render(gfx::Painter& painter) override
    {
        constexpr auto const aspect = 1024.0 / 768.0f;

        gfx::Painter p(painter);
        p.ResetViewMatrix();
        p.SetProjectionMatrix(gfx::MakePerspectiveProjection(gfx::FDegrees { 45.0f }, aspect, 1.0f, 100.0f));

        const float t = std::sin(mTime * 0.4) * 0.5 + 0.5;
        gfx::Painter::DrawState state;
        state.depth_test   = gfx::Painter::DepthTest::LessOrEQual;
        state.stencil_func = gfx::Painter::StencilFunc::Disabled;
        state.write_color  = true;

        const auto index = mShapeIndex % 4;
        std::unique_ptr<gfx::Drawable> drawable;
        if (index == 0)
            drawable.reset(new gfx::Sphere);
        else if (index == 1)
            drawable.reset(new gfx::Cone);
        else if (index == 2)
            drawable.reset(new gfx::Cube);
        else if (index == 3)
            drawable.reset(new gfx::Cylinder);

        auto& shape = *drawable;
        auto material = gfx::CreateMaterialFromColor(gfx::Color::White);
        auto light_pos = glm::vec3{0.0f, 2.0f, -12.0f};

        light_pos.x = 6.0 * std::sin(mTime * 0.4);

        {
            gfx::BasicLightProgram program;

            gfx::BasicLightProgram::Light light;
            light.type = mLightType;
            light.ambient_color  = gfx::Color4f(gfx::Color::Red) * 0.5;
            light.diffuse_color  = gfx::Color4f(gfx::Color::Black);
            light.specular_color = gfx::Color4f(gfx::Color::Black);
            light.view_position  = light_pos;
            light.view_direction = glm::normalize(mLightDirection);
            light.quadratic_attenuation = 0.005f;
            light.spot_half_angle = mLightHalfAngle;
            program.AddLight(light);

            gfx::Transform transform;
            transform.Resize(2.0f, 2.0f, 2.0f);
            transform.RotateAroundY(std::sin(t));
            transform.RotateAroundX(std::cos(t));
            transform.MoveTo(-4.5f, 0.0f, -13.0f);
            p.Draw(shape, transform, material, state, program);
        }

        {
            gfx::BasicLightProgram program;

            gfx::BasicLightProgram::Light light;
            light.type = mLightType;
            light.ambient_color  = gfx::Color4f(gfx::Color::Black);
            light.diffuse_color  = gfx::Color4f(gfx::Color::Green);
            light.specular_color = gfx::Color4f(gfx::Color::Black);
            light.view_position  = light_pos;
            light.view_direction = glm::normalize(mLightDirection);
            light.quadratic_attenuation = 0.005f;
            light.spot_half_angle = mLightHalfAngle;
            program.AddLight(light);

            gfx::Transform transform;
            transform.Resize(2.0f, 2.0f, 2.0f);
            transform.RotateAroundY(std::sin(t));
            transform.RotateAroundX(std::cos(t));
            transform.MoveTo(-1.5f, 0.0f, -13.0f);
            p.Draw(shape, transform, material, state, program);

        }

        {
            gfx::BasicLightProgram program;

            gfx::BasicLightProgram::Light light;
            light.type = mLightType;
            light.ambient_color  = gfx::Color4f(gfx::Color::Black);
            light.diffuse_color  = gfx::Color4f(gfx::Color::Black);
            light.specular_color = gfx::Color4f(gfx::Color::Blue);
            light.view_position  = light_pos;
            light.view_direction = glm::normalize(mLightDirection);
            light.quadratic_attenuation = 0.005f;
            light.spot_half_angle = mLightHalfAngle;
            program.AddLight(light);

            gfx::Transform transform;
            transform.Resize(2.0f, 2.0f, 2.0f);
            transform.RotateAroundY(std::sin(t));
            transform.RotateAroundX(std::cos(t));
            transform.MoveTo(1.5f, 0.0f, -13.0f);
            p.Draw(shape, transform, material, state, program);
        }

        // all components on
        {
            gfx::BasicLightProgram program;

            gfx::BasicLightProgram::Light light;
            light.type = mLightType;
            light.ambient_color  = gfx::Color4f(gfx::Color::DarkGray) * 0.5f;
            light.diffuse_color  = gfx::Color4f(gfx::Color::LightGray) * 0.8;
            light.specular_color = gfx::Color4f(gfx::Color::White) * 1.0f;
            light.view_position  = light_pos;
            light.view_direction = glm::normalize(mLightDirection);
            light.quadratic_attenuation = 0.005f;
            light.spot_half_angle = mLightHalfAngle;
            program.AddLight(light);


            gfx::Transform transform;
            transform.Resize(2.0f, 2.0f, 2.0f);
            transform.RotateAroundY(std::sin(t));
            transform.RotateAroundX(std::cos(t));
            transform.MoveTo(4.5f, 0.0f, -13.0f);
            p.Draw(shape, transform, material, state, program);

            // ground plane
            {
                //state.winding = gfx::Painter::WindigOrder::ClockWise;

                gfx::Transform transform;
                // careful here, using 0.0 for the resize factor on the Z axis is easy mistake
                // to make since logically this "plane" is razor thin (has no thickness)
                // but the right scaling coefficient is of course 1.0. Using 0 will cause
                // normal transformations to go all wrong.
                transform.Resize(10.0f, 10.0f, 1.0f);
                transform.RotateAroundX(gfx::FDegrees(-90.0f));
                transform.Translate(0.0f, -2.5f, -10.0f);
                transform.Push();
                   transform.Translate(-0.5f, -0.5f);
                   transform.RotateAroundX(gfx::FDegrees(180.0f));
                p.Draw(gfx::Rectangle(), transform, material, state, program);
            }
        }


        if (mLightType == LightType::Point || mLightType == LightType::Spot)
        {
            gfx::Transform transform;
            transform.Resize(0.2f, 0.2f, 0.2f);
            transform.MoveTo(light_pos);
            p.Draw(gfx::Cube(), transform, material);
        }
    }

    std::string GetName() const override
    {
        return base::FormatString("Basic%1Light3DTest", mLightType);
    }
    void KeyDown(const wdk::WindowEventKeyDown& key) override
    {
        if (key.symbol == wdk::Keysym::Space)
            ++mShapeIndex;
    }
    void Update(float dt) override
    {
        mTime += dt;
    }
    void Start() override
    {
        mTime = 0.0f;
    }
private:
    LightType mLightType = LightType::Point;
    glm::vec3 mLightDirection = {0.0f, 0.0f, 1.0f};
    gfx::FDegrees mLightHalfAngle;
    unsigned mShapeIndex = 0;
    float mTime = 0.0f;
};

class BasicLightMaterialTest : public GraphicsTest
{
public:
    using LightType = gfx::BasicLightProgram::LightType;

    explicit BasicLightMaterialTest(LightType light) noexcept
      : mLightType(light)
    {}
    void Render(gfx::Painter& painter) override
    {
        constexpr auto const aspect = 1024.0 / 768.0f;

        const float t = std::sin(mTime * 0.4) * 0.5 + 0.5;
        const auto x = std::cos(math::Circle * t) * 4.0f;
        const auto y = std::sin(math::Circle * t) * 4.0f;

        gfx::Painter::DrawState state;
        state.depth_test   = gfx::Painter::DepthTest::LessOrEQual;
        state.stencil_func = gfx::Painter::StencilFunc::Disabled;
        state.write_color  = true;

        gfx::Painter p(painter);
        p.ResetViewMatrix();
        p.SetProjectionMatrix(gfx::MakePerspectiveProjection(gfx::FDegrees { 45.0f }, aspect, 1.0f, 100.0f));

        gfx::BasicLightProgram program;
        gfx::BasicLightProgram::Light light;
        light.type = mLightType;
        light.ambient_color  = gfx::Color4f(gfx::Color::White) * 0.33f;
        light.diffuse_color  = gfx::Color4f(gfx::Color::White) * 1.0f;
        light.specular_color = gfx::Color4f(gfx::Color::White) * 0.8;
        light.view_position  = glm::vec3 {x, y, -10.0f};
        light.view_direction = glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - glm::vec3(x, y, 0.0f));
        light.quadratic_attenuation = 0.005f;
        light.spot_half_angle = gfx::FDegrees{30.0f};
        program.AddLight(light);

        gfx::Transform transform;
        transform.Resize(3.0f, 3.0f, 3.0f);
        transform.RotateAroundY(std::sin(t));
        transform.RotateAroundX(std::cos(t));
        transform.MoveTo(0.0f, 0.0f, -10.0f);
        p.Draw(gfx::Cube(), transform, gfx::MaterialInstance(mMaterial), state, program);


        if (mLightType == LightType::Point || mLightType == LightType::Spot)
        {
            gfx::Transform transform;
            transform.Resize(0.2f, 0.2f, 0.2f);
            transform.MoveTo(light.view_position);
            p.Draw(gfx::Cube(), transform, gfx::CreateMaterialFromColor(gfx::Color::White));
        }
    }

    std::string GetName() const override
    {
        return base::FormatString("Basic%1LightMaterialTest", mLightType);
    }
    void Start() override
    {
        mTime = 0.0f;

        mMaterial = std::make_shared<gfx::MaterialClass>(gfx::MaterialClass::Type::BasicLight);
        mMaterial->SetAmbientColor(gfx::Color::White);
        mMaterial->SetDiffuseColor(gfx::Color::White);
        mMaterial->SetSpecularColor(gfx::Color::White);
        mMaterial->SetNumTextureMaps(2);
        {
            gfx::TextureMap2D diffuse;
            diffuse.SetType(gfx::TextureMap2D::Type::Texture2D);
            diffuse.SetName("Diffuse Map");
            diffuse.SetSamplerName("kDiffuseMap");
            diffuse.SetRectUniformName("kDiffuseMapRect");
            diffuse.SetNumTextures(1);
            diffuse.SetTextureSource(0, gfx::LoadTextureFromFile("textures/wooden-crate-diffuse.png"));
            diffuse.SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            mMaterial->SetTextureMap(0, std::move(diffuse));
        }
        {
            gfx::TextureMap2D specular;
            specular.SetType(gfx::TextureMap2D::Type::Texture2D);
            specular.SetName("Specular Map");
            specular.SetSamplerName("kSpecularMap");
            specular.SetRectUniformName("kSpecularMapRect");
            specular.SetNumTextures(1);
            specular.SetTextureSource(0, gfx::LoadTextureFromFile("textures/wooden-crate-specular.png"));
            specular.SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            mMaterial->SetTextureMap(1, std::move(specular));
        }

    }
    void Update(float dt) override
    {
        mTime += dt;
    }
    void KeyDown(const wdk::WindowEventKeyDown& key) override
    {
        if (key.symbol == wdk::Keysym::Space)
        {
            ++mLightIndex;

            const auto light_index = mLightIndex % 4;
            mLightType = static_cast<LightType>(light_index);
            DEBUG("Light type changed to '%1'", mLightType);
        }
    }
private:
    LightType mLightType = LightType::Point;
    std::shared_ptr<gfx::MaterialClass> mMaterial;
    float mTime = 0.0f;
    unsigned mLightIndex = 0;
};

class BasicShadowMapTest : public GraphicsTest
{
public:
    using LightType = gfx::BasicLightProgram::LightType;

    explicit BasicShadowMapTest(LightType light) noexcept
        : mLightType(light)
    {}
    void Render(gfx::Painter& painter) override
    {
        constexpr auto aspect = 1024.0 / 768.0f;

        const float t = std::sin(mTime * 0.4) * 0.5 + 0.5;
        const auto x = std::cos(math::Circle * t);
        const auto y = std::sin(math::Circle * t);

        gfx::Painter::DrawState state;
        state.depth_test   = gfx::Painter::DepthTest::LessOrEQual;
        state.stencil_func = gfx::Painter::StencilFunc::Disabled;
        state.write_color  = true;

        glm::mat4 view;

        gfx::Painter p(painter);
        p.ResetViewMatrix();
        p.SetProjectionMatrix(gfx::MakePerspectiveProjection(gfx::FDegrees { 45.0f }, aspect, 1.0f, 100.0f));

        {
            const auto a = mTime / math::Pi;
            const auto x = std::cos(a);
            const auto y = std::sin(a);
            const auto t = y * 0.5 + 0.5f;
            const auto cam_pos = glm::vec3(x*10.0f, 3.0f, y*10.0f);
            view = glm::lookAt(cam_pos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            p.SetViewMatrix(view);
        }

        gfx::BasicLightProgram program("test");
        program.EnableFeature(gfx::BasicLightProgram::ShadingFeatures::BasicLight, true);
        program.EnableFeature(gfx::BasicLightProgram::ShadingFeatures::BasicShadows, mEnableShadows);
        program.SetShadowMapSize(1024*2, 768*2);

        auto light_world_position  = glm::vec3 {x*1.5f, 3.0f, y*1.5f }; //glm::vec3 {x, y, -10.0f};
        auto light_world_direction = glm::vec3 {0.0f, -1.0f, 0.0f};

        gfx::BasicLightProgram::Light light;
        light.type = mLightType;
        light.ambient_color  = gfx::Color4f(gfx::Color::DarkGray) * 0.5f;
        light.diffuse_color  = gfx::Color4f(gfx::Color::LightGray) * 0.8f;
        light.specular_color = gfx::Color4f(gfx::Color::White) * 1.0f;
        light.view_position  = math::TransformPosition(view, light_world_position);
        light.view_direction = math::TransformDirection(view, light_world_direction);
        light.world_direction = light_world_direction;
        light.world_position  = light_world_position;
        light.quadratic_attenuation = 0.005f;
        light.spot_half_angle = gfx::FDegrees{30.0f};
        light.far_plane = 10.0f;
        program.AddLight(light);

        gfx::Transform cube_transform;
        cube_transform.Resize(2.0f, 2.0f, 2.0f);
        //cube_transform.RotateAroundY(std::sin(t));
        //cube_transform.RotateAroundX(std::cos(t));
        cube_transform.Translate(0.0f, -1.0f, 0.0f);

        gfx::Transform plane_transform;
        plane_transform.Resize(10.0f, 10.0f, 1.0f);
        plane_transform.RotateAroundX(gfx::FDegrees(-90.0f));
        plane_transform.Translate(0.0f, -3.0f, 0.0f);
        plane_transform.Push();
          plane_transform.Translate(-0.5f, -0.5f);
          plane_transform.RotateAroundX(gfx::FDegrees(180.0f));

        const auto& cube_material = gfx::CreateMaterialFromColor(gfx::Color::DarkGreen);
        const auto& plane_material = gfx::CreateMaterialFromColor(gfx::Color::White);

        gfx::ShadowMapRenderPass shadow_pass("test", program, painter.GetDevice());
        shadow_pass.InitState();
        shadow_pass.Draw(gfx::Sphere(), cube_material, cube_transform);
        shadow_pass.Draw(gfx::Rectangle(), plane_material, plane_transform);

        // todo: dumping the depth map doesn't work yet with 2D array texture.
        /*
        if (mWriteMaps)
        {
            auto* device = painter.GetDevice();
            const auto* depth_texture = shadow_pass.GetDepthTexture(0);
            const auto light_projection = shadow_pass.GetLightProjectionType(0);
            if (light_projection == gfx::ShadowMapRenderPass::LightProjectionType::Orthographic)
            {
                auto bitmap = gfx::algo::ReadOrthographicDepthTexture(depth_texture, device);
                gfx::WritePNG(*bitmap, "depth-texture-light0.png");
            }
            else if (light_projection == gfx::ShadowMapRenderPass::LightProjectionType::Perspective)
            {
                const auto near = shadow_pass.GetLightProjectionNearPlane(0);
                const auto far  = shadow_pass.GetLightProjectionFarPlane(0);
                auto bitmap = gfx::algo::ReadPerspectiveDepthTexture(depth_texture, device, near, far);
                gfx::WritePNG(*bitmap, "depth-texture-light0.png");
            }
            mWriteMaps = false;
        }
        */


        if (mShowLightView)
            p.SetViewMatrix(shadow_pass.GetLightViewMatrix(0));

        p.Draw(gfx::Sphere(), cube_transform, gfx::CreateMaterialFromColor(gfx::Color::DarkGreen), state, program);
        p.Draw(gfx::Rectangle(), plane_transform, gfx::CreateMaterialFromColor(gfx::Color::White), state, program);

        for (unsigned light_index = 0; light_index < program.GetLightCount(); ++light_index)
        {
            const auto& light = program.GetLight(light_index);
            if (light.type == LightType::Point || light.type == LightType::Spot)
            {
                gfx::Transform transform;
                transform.Resize(0.2f, 0.2f, 0.2f);
                transform.MoveTo(light_world_position);
                p.Draw(gfx::Cube(), transform, gfx::CreateMaterialFromColor(gfx::Color::White));
            }
        }
    }

    std::string GetName() const override
    {
        return base::FormatString("Basic%1ShadowMapTest", mLightType);
    }
    void Start() override
    {
        mTime = 0.0f;
    }
    void Update(float dt) override
    {
        mTime += dt;
    }
    void KeyDown(const wdk::WindowEventKeyDown& key) override
    {
        if (key.symbol == wdk::Keysym::Space)
        {
            ++mLightIndex;

            const auto light_index = mLightIndex % 4;
            mLightType = static_cast<LightType>(light_index);
            INFO("Light type changed to '%1'", mLightType);
        }
        else if (key.symbol == wdk::Keysym::KeyQ)
            mWriteMaps = true;
        else if (key.symbol == wdk::Keysym::KeyW)
            mShowLightView = !mShowLightView;
        else if (key.symbol == wdk::Keysym::KeyS)
            mEnableShadows = !mEnableShadows;
    }
private:
    LightType mLightType = LightType::Point;
    float mTime = 0.0f;
    unsigned mLightIndex = 0;
    bool mWriteMaps = false;
    bool mShowLightView = false;
    bool mEnableShadows = true;
};

class BasicMultiLightShadowMapTest : public GraphicsTest
{
public:
    using LightType = gfx::BasicLightProgram::LightType;

    explicit BasicMultiLightShadowMapTest() noexcept
    {}
    void Render(gfx::Painter& painter) override
    {
        constexpr auto aspect = 1024.0 / 768.0f;

        const float t = std::sin(mTime * 0.4) * 0.5 + 0.5;
        const auto x = std::cos(math::Circle * t);
        const auto y = std::sin(math::Circle * t);

        gfx::Painter::DrawState state;
        state.depth_test   = gfx::Painter::DepthTest::LessOrEQual;
        state.stencil_func = gfx::Painter::StencilFunc::Disabled;
        state.write_color  = true;

        glm::mat4 view;

        gfx::Painter p(painter);
        p.ResetViewMatrix();
        p.SetProjectionMatrix(gfx::MakePerspectiveProjection(gfx::FDegrees { 45.0f }, aspect, 1.0f, 100.0f));

        {
            const auto a = mTime / math::Pi;
            const auto x = std::cos(a);
            const auto y = std::sin(a);
            const auto t = y * 0.5 + 0.5f;
            const auto cam_pos = glm::vec3(x*10.0f, 3.0f, y*10.0f);
            view = glm::lookAt(cam_pos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            p.SetViewMatrix(view);
        }

        gfx::BasicLightProgram program("test");
        program.EnableFeature(gfx::BasicLightProgram::ShadingFeatures::BasicLight, true);
        program.EnableFeature(gfx::BasicLightProgram::ShadingFeatures::BasicShadows, true);
        program.SetShadowMapSize(1024*2, 768*2);

        auto light_world_position  = glm::vec3 {x*1.5f, 3.0f, y*1.5f }; //glm::vec3 {x, y, -10.0f};
        auto light_world_direction = glm::vec3 {0.0f, -1.0f, 0.0f};

        gfx::BasicLightProgram::Light spot;
        spot.type = LightType::Spot;
        spot.ambient_color  = gfx::Color4f(gfx::Color::DarkGray) * 0.5f;
        spot.diffuse_color  = gfx::Color4f(gfx::Color::LightGray) * 0.8f;
        spot.specular_color = gfx::Color4f(gfx::Color::White) * 1.0f;
        spot.view_position  = math::TransformPosition(view, light_world_position);
        spot.view_direction = math::TransformDirection(view, light_world_direction);
        spot.world_direction = light_world_direction;
        spot.world_position  = light_world_position;
        spot.quadratic_attenuation = 0.005f;
        spot.spot_half_angle = gfx::FDegrees{30.0f};
        spot.far_plane = 10.0f;
        program.AddLight(spot);

        gfx::BasicLightProgram::Light directional;
        directional.type = LightType::Directional;
        directional.ambient_color  = gfx::Color4f(gfx::Color::DarkGray) * 0.5f;
        directional.diffuse_color  = gfx::Color4f(gfx::Color::LightGray) * 0.8f;
        directional.specular_color = gfx::Color4f(gfx::Color::White) * 1.0f;
        directional.world_direction = glm::normalize(glm::vec3(1.0f, -1.0f, 0.0f));
        directional.world_position  = glm::vec3 { 0.0f, 5.0f, 0.0f };
        directional.view_position  = math::TransformPosition(view, directional.world_position);
        directional.view_direction = math::TransformDirection(view, directional.world_direction);
        directional.quadratic_attenuation = 0.005f;
        directional.spot_half_angle = gfx::FDegrees{30.0f};
        directional.far_plane = 400.0f;
        program.AddLight(directional);

        gfx::Transform cube_transform;
        cube_transform.Resize(2.0f, 2.0f, 2.0f);
        //cube_transform.RotateAroundY(std::sin(t));
        //cube_transform.RotateAroundX(std::cos(t));
        cube_transform.Translate(0.0f, -1.0f, 0.0f);

        gfx::Transform plane_transform;
        plane_transform.Resize(10.0f, 10.0f, 1.0f);
        plane_transform.RotateAroundX(gfx::FDegrees(-90.0f));
        plane_transform.Translate(0.0f, -3.0f, 0.0f);
        plane_transform.Push();
          plane_transform.Translate(-0.5f, -0.5f);
          plane_transform.RotateAroundX(gfx::FDegrees(180.0f));

        const auto& cube_material = gfx::CreateMaterialFromColor(gfx::Color::DarkGreen);
        const auto& plane_material = gfx::CreateMaterialFromColor(gfx::Color::White);

        gfx::ShadowMapRenderPass shadow_pass("test", program, painter.GetDevice());
        shadow_pass.InitState();
        shadow_pass.Draw(gfx::Sphere(), cube_material, cube_transform);
        shadow_pass.Draw(gfx::Rectangle(), plane_material, plane_transform);

        p.Draw(gfx::Sphere(), cube_transform, gfx::CreateMaterialFromColor(gfx::Color::DarkGreen), state, program);
        p.Draw(gfx::Rectangle(), plane_transform, gfx::CreateMaterialFromColor(gfx::Color::White), state, program);

        for (unsigned light_index = 0; light_index < program.GetLightCount(); ++light_index)
        {
            const auto& light = program.GetLight(light_index);
            if (light.type == LightType::Point || light.type == LightType::Spot)
            {
                gfx::Transform transform;
                transform.Resize(0.2f, 0.2f, 0.2f);
                transform.MoveTo(light_world_position);
                p.Draw(gfx::Cube(), transform, gfx::CreateMaterialFromColor(gfx::Color::White));
            }
        }
    }

    std::string GetName() const override
    {
        return "BasicMultiLightShadowMapTest";
    }
    void Start() override
    {
        mTime = 0.0f;
    }
    void Update(float dt) override
    {
        mTime += dt;
    }
private:
    float mTime = 0.0f;
};


class BasicLightNormalMapMaterialTest : public GraphicsTest
{
public:
    using LightType = gfx::BasicLightProgram::LightType;

    explicit BasicLightNormalMapMaterialTest(LightType light) noexcept
      : mLightType(light)
    {}

    void Render(gfx::Painter& painter) override
    {
        constexpr auto const aspect = 1024.0 / 768.0f;

        const float t = std::sin(mTime * 0.4) * 0.5f + 0.5f;
        const auto x = std::cos(math::Circle * t) * 4.0f;
        const auto y = std::sin(math::Circle * t) * 4.0f;

        gfx::Painter::DrawState state;
        state.depth_test = gfx::Painter::DepthTest::LessOrEQual;
        state.stencil_func = gfx::Painter::StencilFunc::Disabled;
        state.write_color = true;

        gfx::Painter p(painter);
        p.ResetViewMatrix();
        p.SetProjectionMatrix(gfx::MakePerspectiveProjection(gfx::FDegrees{45.0f}, aspect, 1.0f, 100.0f));

        gfx::BasicLightProgram program;
        gfx::BasicLightProgram::Light light;
        light.type = mLightType;
        light.ambient_color = gfx::Color4f(gfx::Color::White) * 0.25;
        light.diffuse_color = gfx::Color4f(gfx::Color::White) * 1.0f;
        light.specular_color = gfx::Color4f(gfx::Color::White) * 0.3f;
        light.view_position = glm::vec3{x, y, -7.0f};
        light.view_direction = glm::normalize(glm::vec3(0.0f, 0.0f, -10.0f) - glm::vec3(x, y, -7.0f));
        light.quadratic_attenuation = 0.0005f;
        light.spot_half_angle = mLightHalfAngle;
        program.AddLight(light);

        gfx::Transform transform;
        transform.Resize(10.0f, 10.0f, 10.0f);
        transform.RotateAroundX(gfx::FDegrees(mPitch));
        transform.RotateAroundY(gfx::FDegrees(mRoll));
        transform.MoveTo(0.0f, 0.0f, mZ);
        if (gfx::Is2DShape(*mShape))
        {
            transform.Push();
                transform.Translate(-0.5f, -0.5f);
                transform.RotateAroundZ(gfx::FDegrees(180.0f));
                transform.RotateAroundY(gfx::FDegrees(180.0f));
        }

        p.Draw(*mShape, transform, gfx::MaterialInstance(mMaterial), state, program);

        if (mLightType == LightType::Point || mLightType == LightType::Spot)
        {
            gfx::Transform transform;
            transform.Resize(0.2f, 0.2f, 0.2f);
            transform.MoveTo(light.view_position);
            p.Draw(gfx::Cube(), transform, gfx::CreateMaterialFromColor(gfx::Color::White));
        }
    }

    std::string GetName() const override
    {
        return base::FormatString("Basic%1LightNormalMapMaterialTest", mLightType);
    }
    void Start() override
    {
        mTime = 0.0f;
        mPitch = 0.0f;
        mRoll = 0.0f;
        mZ = -10.0f;

        mMaterial = std::make_shared<gfx::MaterialClass>(gfx::MaterialClass::Type::BasicLight);
        mMaterial->SetAmbientColor(gfx::Color::White);
        mMaterial->SetDiffuseColor(gfx::Color::White);
        mMaterial->SetSpecularColor(gfx::Color::White);
        mMaterial->SetNumTextureMaps(2);
        {
            gfx::TextureMap2D diffuse;
            diffuse.SetType(gfx::TextureMap2D::Type::Texture2D);
            diffuse.SetName("Diffuse Map");
            diffuse.SetSamplerName("kDiffuseMap");
            diffuse.SetRectUniformName("kDiffuseMapRect");
            diffuse.SetNumTextures(1);
            diffuse.SetTextureSource(0, gfx::LoadTextureFromFile("textures/ground-diffuse.png"));
            diffuse.SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            mMaterial->SetTextureMap(0, std::move(diffuse));
        }
        {
            gfx::TextureMap2D normal;
            normal.SetType(gfx::TextureMap2D::Type::Texture2D);
            normal.SetName("Normal Map");
            normal.SetSamplerName("kNormalMap");
            normal.SetRectUniformName("kNormalMapRect");
            normal.SetNumTextures(1);
            normal.SetTextureSource(0, gfx::LoadTextureFromFile("textures/ground-normal.png"));
            normal.SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            normal.GetTextureSource(0)->SetColorSpace(gfx::TextureSource::ColorSpace::Linear);
            mMaterial->SetTextureMap(1, std::move(normal));
        }

        mShape = std::make_unique<gfx::Rectangle>();

    }
    void Update(float dt) override
    {
        mTime += dt;

    }
    void KeyDown(const wdk::WindowEventKeyDown& key) override
    {
        if (key.symbol == wdk::Keysym::Space)
        {
            ++mLightIndex;

            const auto light_index = mLightIndex % 4;
            mLightType = static_cast<LightType>(light_index);
            DEBUG("Light type changed to '%1'", mLightType);
        }
        else if (key.symbol == wdk::Keysym::Key1)
            mShape = std::make_unique<gfx::Rectangle>();
        else if (key.symbol == wdk::Keysym::Key2)
            mShape = std::make_unique<gfx::Cube>();
        else if (key.symbol == wdk::Keysym::Key3)
            mShape = std::make_unique<gfx::Pyramid>();
        else if (key.symbol == wdk::Keysym::Key4)
            mShape = std::make_unique<gfx::Sphere>();
        else if (key.symbol == wdk::Keysym::KeyW)
            mPitch -= 2.0f;
        else if (key.symbol == wdk::Keysym::KeyS)
            mPitch += 2.0f;
        else if (key.symbol == wdk::Keysym::KeyA)
            mRoll -= 2.0f;
        else if (key.symbol == wdk::Keysym::KeyD)
            mRoll += 2.0f;
        else if (key.symbol == wdk::Keysym::KeyZ && key.modifiers.test(wdk::Keymod::Shift))
            mZ += 0.5f;
        else if (key.symbol == wdk::Keysym::KeyZ)
            mZ -= 0.5f;
    }
private:
    LightType mLightType = LightType::Point;
    gfx::FDegrees mLightHalfAngle = gfx::FDegrees{30.0f};
    std::shared_ptr<gfx::MaterialClass> mMaterial;
    std::unique_ptr<gfx::Drawable> mShape;
    float mTime = 0.0f;
    float mPitch = 0.0f;
    float mRoll = 0.0f;
    float mZ = -10.0f;
    unsigned mLightIndex = 0;
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
    int version = 2;
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
        else if (!std::strcmp(argv[i], "--es3"))
            version = 3;
    }

    // context integration glue code that puts together
    // wdk::Context and gfx::Device
    class WindowContext : public dev::Context
    {
    public:
        WindowContext(wdk::Config::Multisampling sampling, bool srgb, bool debug, int version)
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

            mVersion  = version;
            mConfig   = std::make_unique<wdk::Config>(attrs);
            mContext  = std::make_unique<wdk::Context>(*mConfig, version, 0,  debug, wdk::Context::Type::OpenGL_ES);
            mVisualID = mConfig->GetVisualID();
            mDebug = debug;
        }
        void Display() override
        {
            mContext->SwapBuffers();
        }
        void* Resolve(const char* name) override
        {
            return mContext->Resolve(name);
        }
        void MakeCurrent() override
        {
            mContext->MakeCurrent(mSurface.get());
        }
        Version GetVersion() const override
        {
            if (mVersion == 2)
                return Version::OpenGL_ES2;
            else if (mVersion ==3 )
                return Version::OpenGL_ES3;
            else BUG("Unknown OpenGL ES version");
        }
        bool IsDebug() const override
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
        int mVersion = 0;
    };

    auto context = std::make_shared<WindowContext>(sampling, srgb, debug_context, version);
    auto dev_device  = dev::CreateDevice(context);
    auto gfx_device = gfx::CreateDevice(dev_device->GetSharedGraphicsDevice());
    auto painter = gfx::Painter::Create(gfx_device);
    painter->SetEditingMode(false);
    painter->SetDebugMode(debug_context);

    using RandomGen = math::RandomGenerator<float, 0xdeadbeef>;
    RandomGen rg;

    gfx::EffectDrawable::SetRandomGenerator([&rg](float min, float max) {
        return rg(min, max);
    });
    gfx::ParticleEngineClass::SetRandomGenerator([&rg](float min, float max) {
        return rg(min, max);
    });

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
    tests.emplace_back(new TextureEdgeTest);
    tests.emplace_back(new TextureColorExtractTest);
    tests.emplace_back(new GradientTest);
    tests.emplace_back(new SpriteTest);
    tests.emplace_back(new SpriteSheetTest);
    tests.emplace_back(new StencilCoverTest);
    tests.emplace_back(new StencilExposeTest);
    tests.emplace_back(new PolygonTest);
    tests.emplace_back(new TileBatchTest);
    tests.emplace_back(new TileOverdrawTest);
    tests.emplace_back(new JankTest);
    tests.emplace_back(new MegaParticleTest);
    tests.emplace_back(new VSyncTest);
    tests.emplace_back(new NullTest);
    tests.emplace_back(new ScissorTest);
    tests.emplace_back(new ViewportTest);
    tests.emplace_back(new sRGBWindowTest);
    tests.emplace_back(new sRGBTextureSampleTest);
    tests.emplace_back(new PremultiplyAlphaTest);
    tests.emplace_back(new PrecisionTest);
    tests.emplace_back(new Draw3DTest);
    tests.emplace_back(new Shape3DTest);
    tests.emplace_back(new DepthLayerTest);
    tests.emplace_back(new OrthoDepthTest);
    tests.emplace_back(new MeshExplosionTest);
    tests.emplace_back(new WavefrontMeshTest);

    // GL ES3 specific tests
    if (version == 3)
    {
        tests.emplace_back(new FramebufferTest);
        tests.emplace_back(new Simple2DInstanceTest);
        tests.emplace_back(new Simple3DInstanceTest);
        tests.emplace_back(new PolygonInstanceTest);
        tests.emplace_back(new ParticleInstanceTest);
        // under ES3 because we require uniform buffers

        tests.emplace_back(new BasicLight3DTest);
        tests.emplace_back(new BasicLight3DTest(glm::vec3{-1.0f, -1.0f, 0.0f}));
        tests.emplace_back(new BasicLight3DTest(glm::vec3{0.0f, -1.0f, 0.0f}, gfx::FDegrees(25.0f)));

        tests.emplace_back(new BasicLightMaterialTest(BasicLightMaterialTest::LightType::Point));
        tests.emplace_back(new BasicLightMaterialTest(BasicLightMaterialTest::LightType::Spot));
        tests.emplace_back(new BasicLightMaterialTest(BasicLightMaterialTest::LightType::Directional));

        tests.emplace_back(new BasicLightNormalMapMaterialTest(BasicLightNormalMapMaterialTest::LightType::Point));
        tests.emplace_back(new BasicLightNormalMapMaterialTest(BasicLightNormalMapMaterialTest::LightType::Spot));
        tests.emplace_back(new BasicLightNormalMapMaterialTest(BasicLightNormalMapMaterialTest::LightType::Directional));

        tests.emplace_back(new BasicFog3DTest(BasicFog3DTest::FogMode::Linear));
        tests.emplace_back(new BasicFog3DTest(BasicFog3DTest::FogMode::Exponential1));
        tests.emplace_back(new BasicFog3DTest(BasicFog3DTest::FogMode::Exponential2));

        tests.emplace_back(new BasicShadowMapTest(BasicShadowMapTest::LightType::Directional));
        tests.emplace_back(new BasicShadowMapTest(BasicShadowMapTest::LightType::Spot));
        tests.emplace_back(new BasicShadowMapTest(BasicShadowMapTest::LightType::Point));

        tests.emplace_back(new BasicMultiLightShadowMapTest());
    }

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
        else if (key.symbol == wdk::Keysym::KeyR)
        {
            gfx_device->DeleteShaders();
            gfx_device->DeletePrograms();
            gfx_device->DeleteGeometries();
            DEBUG("Reload!");
        }
        else if (key.symbol == wdk::Keysym::KeyS && key.modifiers.test(wdk::Keymod::Control))
        {
            static unsigned screenshot_number = 0;
            const auto& name = "demo_" + std::to_string(screenshot_number) + ".png";
            auto rgba = gfx_device->ReadColorBuffer(surface_width, surface_height);
            gfx::SetAlphaToOne(rgba);
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
        else
        {
            tests[current_test_index]->KeyDown(key);
        }
        stop_for_input = false;
    };

    // render in the window
    context->SetWindowSurface(window);
    context->SetSwapInterval(swap_interval);

    if (testing)
    {
        const float dt = 1.0f/60.0f;

        std::filesystem::create_directory("test-results");

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
                gfx_device->ClearColor(gfx::Color4f(0.2f, 0.3f, 0.4f, 1.0f));
                gfx_device->ClearDepth(1.0f);
                painter->SetViewport(0, 0, surface_width, surface_height);
                painter->SetSurfaceSize(surface_width, surface_height);
                painter->SetProjectionMatrix(gfx::MakeOrthographicProjection((float)surface_width , (float)surface_height));
                // render the test.
                test->Render(*painter);

                gfx::Bitmap<gfx::Pixel_RGBA> result = gfx_device->ReadColorBuffer(surface_width, surface_height);
                gfx::SetAlphaToOne(result);

                const auto& result_file_name    = base::FormatString("test-results/%1_%2_%3_Result.png", test->GetName(), i, sampling);
                const auto& reference_file_name = base::FormatString("test-results/%1_%2_%3_Reference.png", test->GetName(), i, sampling);
                const auto& delta_file_name     = base::FormatString("test-results/%1_%2_%3_Delta.png", test->GetName(), i, sampling);
                if (issue_gold)
                {
                    const auto& gold_file_name  = base::FormatString("gold/%1_%2_%3_Gold.png", test->GetName(), i, sampling);
                    gfx::WritePNG(result, gold_file_name);
                    INFO("Wrote new gold image. '%1'", gold_file_name);
                }

                if (!base::FileExists(reference_file_name))
                {
                    gfx_device->EndFrame(true /*display*/);
                    gfx_device->CleanGarbage(120, gfx::Device::GCFlags::Textures);
                    // the result is the new gold image. should be eye-balled and verified.
                    gfx::WritePNG(result, reference_file_name);
                    INFO("Wrote new local reference image. '%1'", reference_file_name);
                    continue;
                }

                stop_for_input = true;

                // load gold image
                gfx::Image img(reference_file_name);

                const auto& gold = img.AsBitmap<gfx::Pixel_RGBA>();
                const auto& gold_view = gold.GetPixelReadView();
                const auto& result_view = result.GetPixelReadView();

                // for some threshold value listing see the unit_test_bitmap which can produce/print
                // a table of various thresholds.
                const gfx::PixelEquality::ThresholdPrecision mse_comparator(1000.0);
                const gfx::USize mse_block_size(8, 8);

                if (!gfx::PixelBlockCompareBitmaps(gold_view, result_view, mse_block_size, mse_comparator))
                {
                    ERROR("'%1' vs '%2' FAILED.", reference_file_name, result_file_name);
                    if (gold.GetWidth() != result.GetWidth() || gold.GetHeight() != result.GetHeight())
                    {
                        ERROR("Image dimensions mismatch: Gold = %1x%1 vs. Result = %2x%3",
                            gold.GetWidth(), gold.GetHeight(),
                            result.GetWidth(), result.GetHeight());
                    }
                    else
                    {
                        // generate difference visualization file.
                        gfx::Bitmap<gfx::Pixel_RGBA> diff;
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
                        gfx::WritePNG(diff, delta_file_name);
                    }
                    gfx::WritePNG(result, result_file_name);
                    test_result = EXIT_FAILURE;
                }
                else
                {
                    INFO("'%1' vs '%2' OK.", reference_file_name, result_file_name);
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
            gfx_device->ClearDepth(1.0f);
            painter->SetViewport(0, 0, surface_width, surface_height);
            painter->SetSurfaceSize(surface_width, surface_height);
            painter->SetProjectionMatrix(gfx::MakeOrthographicProjection((float)surface_width, (float)surface_height));
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

                base::FlushGlobalLog();
                base::FlushThreadLog();
            }
        }
    }

    context->Dispose();
    return test_result;
}
