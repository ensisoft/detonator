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
private:
};

class StencilTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        // draw a gradient in the background
        {
            gfx::Material material(gfx::Material::Type::Gradient);
            material.SetColorMapColor(gfx::Color::Red, gfx::Material::ColorIndex::TopLeft);
            material.SetColorMapColor(gfx::Color::Green, gfx::Material::ColorIndex::BottomLeft);
            material.SetColorMapColor(gfx::Color::Blue, gfx::Material::ColorIndex::BottomRight);
            material.SetColorMapColor(gfx::Color::Black, gfx::Material::ColorIndex::TopRight);
            gfx::Transform transform;
            transform.Resize(1024, 768);
            painter.Draw(gfx::Rectangle(), transform, material);
        }

        {
            gfx::Material material(gfx::Material::Type::Texture);
            material.AddTexture("textures/Checkerboard.png");
            gfx::Transform mask;
            mask.Resize(400, 400);
            mask.Translate(200 + std::cos(mTime) * 200, 200 + std::sin(mTime) * 200);
            gfx::Transform shape;
            shape.Resize(1024, 768);
            painter.DrawMasked(gfx::Rectangle(), shape, gfx::Circle(), mask, material);
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

class GradientTest : public GraphicsTest
{
public:
    virtual void Render(gfx::Painter& painter) override
    {
        gfx::Material material(gfx::Material::Type::Gradient);
        material.SetColorMapColor(gfx::Color::Red, gfx::Material::ColorIndex::TopLeft);
        material.SetColorMapColor(gfx::Color::Green, gfx::Material::ColorIndex::BottomLeft);
        material.SetColorMapColor(gfx::Color::Blue, gfx::Material::ColorIndex::BottomRight);
        material.SetColorMapColor(gfx::Color::Black, gfx::Material::ColorIndex::TopRight);
        gfx::FillRect(painter, gfx::FRect(0, 0, 400, 400), material);
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
            gfx::Material material(gfx::Material::Type::Texture);
            material.AddTexture("textures/uv_test_512.png");
            material.SetTextureRect(0, gfx::Rect(0.0f, 0.0f, 1.0f, 1.0f));
            gfx::FillRect(painter, gfx::FRect(0, 0, 128, 128), material);

            material.SetTextureScaleX(2.0);
            material.SetTextureScaleY(2.0);
            material.SetTextureWrapX(gfx::Material::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(150, 0, 128, 128), material);

            material.SetTextureWrapX(gfx::Material::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(300, 0, 128, 128), material);

            material.SetTextureScaleX(-2.0);
            material.SetTextureScaleY(-2.0);
            material.SetTextureWrapX(gfx::Material::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(450, 0, 128, 128), material);
        }

        // texture box > 1.0
        // todo: maybe just limit the box to 0.0, 1.0 range and dismiss this case ?
        {
            // clamp
            gfx::Material material(gfx::Material::Type::Texture);
            material.AddTexture("textures/uv_test_512.png");
            material.SetTextureRect(0, gfx::FRect(0.0, 0.0, 2.0, 1.0));
            material.SetTextureWrapX(gfx::Material::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(0, 150, 128, 128), material);

            material.SetTextureRect(0, gfx::FRect(0.0, 0.0, 2.0, 2.0));
            material.SetTextureWrapX(gfx::Material::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Clamp);
            gfx::FillRect(painter, gfx::FRect(150, 150, 128, 128), material);

            material.SetTextureRect(0, gfx::FRect(0.0, 0.0, 2.0, 2.0));
            material.SetTextureWrapX(gfx::Material::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(300, 150, 128, 128), material);

        }

        // texture box < 1.0
        {
            // basic case. sampling within the box.

            gfx::Material material(gfx::Material::Type::Texture);
            material.AddTexture("textures/uv_test_512.png");
            material.SetTextureRect(0, gfx::FRect(0.5, 0.5, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(0, 300, 128, 128), material);

            // clamping with texture boxing.
            material.SetTextureMinFilter(gfx::Material::MinTextureFilter::Linear);
            material.SetTextureRect(0, gfx::FRect(0.0, 0.0, 0.5, 0.5));
            material.SetTextureWrapX(gfx::Material::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Clamp);
            material.SetTextureScaleX(2.0);
            material.SetTextureScaleY(2.0);
            gfx::FillRect(painter, gfx::FRect(150, 300, 128, 128), material);

            // should be 4 squares each brick color (the top left quadrant of the source texture)
            material.SetTextureRect(0, gfx::FRect(0.0, 0.0, 0.5, 0.5));
            material.SetTextureScaleX(2.0);
            material.SetTextureScaleY(2.0);
            material.SetTextureWrapX(gfx::Material::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Repeat);
            gfx::FillRect(painter, gfx::FRect(300, 300, 128, 128), material);

            material.SetTextureRect(0, gfx::FRect(0.25, 0.25, 0.5, 0.5));
            material.SetTextureScaleX(1.0f);
            material.SetTextureScaleY(1.0f);
            material.SetTextureWrapX(gfx::Material::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Clamp);
            material.SetTextureRect(0, gfx::FRect(0.25, 0.25, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(450, 300, 128, 128), material);

            material.SetTextureRect(0, gfx::FRect(0.25, 0.25, 0.5, 0.5));
            material.SetTextureScaleX(2.0f);
            material.SetTextureScaleY(2.0f);
            material.SetTextureWrapX(gfx::Material::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Clamp);
            material.SetTextureRect(0, gfx::FRect(0.25, 0.25, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(600, 300, 128, 128), material);

            material.SetTextureRect(0, gfx::FRect(0.25, 0.25, 0.5, 0.5));
            material.SetTextureScaleX(2.0f);
            material.SetTextureScaleY(2.0f);
            material.SetTextureWrapX(gfx::Material::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Repeat);
            material.SetTextureRect(0, gfx::FRect(0.25, 0.25, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(750, 300, 128, 128), material);
        }

        // texture velocity
        {
            gfx::Material material(gfx::Material::Type::Texture);
            material.AddTexture("textures/uv_test_512.png");
            material.SetRuntime(mTime);
            material.SetTextureWrapX(gfx::Material::TextureWrapping::Repeat);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Repeat);

            material.SetTextureVelocityX(0.2);
            gfx::FillRect(painter, gfx::FRect(0, 450, 128, 128), material);

            material.SetTextureVelocityX(0.0);
            material.SetTextureVelocityY(0.2);
            gfx::FillRect(painter, gfx::FRect(150, 450, 128, 128), material);

            material.SetTextureVelocityX(0.25);
            material.SetTextureVelocityY(0.2);
            material.SetTextureRect(0, gfx::FRect(0.25, 0.25, 0.5, 0.5));
            gfx::FillRect(painter, gfx::FRect(300, 450, 128, 128), material);

            material.SetTextureVelocityX(0.0f);
            material.SetTextureVelocityY(0.0f);
            material.SetTextureVelocityZ(3.134);
            material.SetTextureWrapX(gfx::Material::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Clamp);
            material.SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            gfx::FillRect(painter, gfx::FRect(450, 450, 128, 128), material);

            material.SetTextureVelocityX(0.0f);
            material.SetTextureVelocityY(0.0f);
            material.SetTextureVelocityZ(-3.134);
            material.SetTextureWrapX(gfx::Material::TextureWrapping::Clamp);
            material.SetTextureWrapY(gfx::Material::TextureWrapping::Clamp);
            material.SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            gfx::FillRect(painter, gfx::FRect(600, 450, 128, 128), material);
        }
    }
    virtual void Update(float dt)
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
    virtual void Render(gfx::Painter& painter) override
    {
        {
            gfx::Material material(gfx::Material::Type::Sprite);
            material.AddTexture("textures/red_64x64.png");
            material.AddTexture("textures/green_64x64.png");
            material.SetTextureMinFilter(gfx::Material::MinTextureFilter::Linear);
            material.SetFps(0.5); // two seconds to a frame
            material.SetRuntime(mTime);
            material.SetBlendFrames(false);
            gfx::FillRect(painter, gfx::FRect(20, 20, 200, 200), material);
        }

        {
            gfx::Material material(gfx::Material::Type::Sprite);
            material.SetSurfaceType(gfx::Material::SurfaceType::Transparent);
            material.AddTexture("textures/bird/frame-1.png");
            material.AddTexture("textures/bird/frame-2.png");
            material.AddTexture("textures/bird/frame-3.png");
            material.AddTexture("textures/bird/frame-4.png");
            material.AddTexture("textures/bird/frame-5.png");
            material.AddTexture("textures/bird/frame-6.png");
            material.AddTexture("textures/bird/frame-7.png");
            material.AddTexture("textures/bird/frame-8.png");
            material.SetFps(10.0f);
            material.SetRuntime(mTime);
            material.SetBlendFrames(true);
            gfx::FillRect(painter, gfx::FRect(250, 250, 300, 300), material);
        }
    }

    virtual void Update(float dt) override
    { mTime += dt; }
    virtual std::string GetName() const override
    { return "SpriteTest"; }
private:
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

            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::RoundRectangle(gfx::Drawable::Style::Solid), transform, materials[0]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::RoundRectangle(gfx::Drawable::Style::Solid), transform, materials[1]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::RoundRectangle(gfx::Drawable::Style::Outline), transform, materials[1]);
            transform.Translate(110.0f, 0.0f);
            painter.Draw(gfx::RoundRectangle(gfx::Drawable::Style::Wireframe), transform, materials[1]);
        }
    }
    virtual void Update(float dts)
    {}
    virtual std::string GetName() const override
    { return "ShapesTest"; }
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
    virtual std::string GetName() const override
    { return "ParticleTest"; }
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

        // text color with gradient
        {
            gfx::Transform transform;
            transform.Resize(300, 200);
            transform.Translate(-150, -100);
            transform.Translate(150, 450);

            gfx::TextBuffer buff(300, 200);
            buff.AddText("Gradient text", "fonts/AtariFontFullVersion.ttf", 20);
            painter.Draw(gfx::Rectangle(), transform,
                gfx::BitmapText(buff)
                    .SetBaseColor(gfx::Color::White)
                    .SetColorMapColor(gfx::Color::Red, gfx::Material::ColorIndex::TopLeft)
                    .SetColorMapColor(gfx::Color::Blue, gfx::Material::ColorIndex::TopRight)
                    .SetColorMapColor(gfx::Color::Yellow, gfx::Material::ColorIndex::BottomLeft)
                    .SetColorMapColor(gfx::Color::Green, gfx::Material::ColorIndex::BottomRight));

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
    virtual std::string GetName() const override
    { return "TextTest"; }
private:
    float mTime = 0.0f;
};

int main(int argc, char* argv[])
{
    base::OStreamLogger logger(std::cout);
    base::SetGlobalLog(&logger);
    DEBUG("It's alive!");
    INFO("Copyright (c) 2010-2019 Sami Vaisanen");
    INFO("http://www.ensisoft.com");
    INFO("http://github.com/ensisoft/pinyin-invaders");

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
    std::vector<std::unique_ptr<GraphicsTest>> tests;
    tests.emplace_back(new TransformTest);
    tests.emplace_back(new RenderTextTest);
    tests.emplace_back(new RenderParticleTest);
    tests.emplace_back(new ShapesTest);
    tests.emplace_back(new TextureTest);
    tests.emplace_back(new GradientTest);
    tests.emplace_back(new SpriteTest);
    tests.emplace_back(new StencilTest);

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

    if (testing)
    {
        const float dt = 1.0f/60.0f;

        for (auto& test : tests)
        {
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

                device->BeginFrame();
                device->ClearColor(gfx::Color::Black);
                painter->SetViewport(0, 0, window.GetSurfaceWidth(), window.GetSurfaceHeight());
                // render the test.
                test->Render(*painter);

                const gfx::Bitmap<gfx::RGBA>& result = device->ReadColorBuffer(window.GetSurfaceWidth(),
                    window.GetSurfaceHeight());

                const auto& resultfile = base::FormatString("Result_%1_%2_%3_.png", test->GetName(), i, sampling);
                const auto& goldfile   = base::FormatString("Gold_%1_%2_%3_.png", test->GetName(), i, sampling);
                const auto& deltafile  = base::FormatString("Delta_%1_%2_%3_.png", test->GetName(), i, sampling);
                if (!base::FileExists(goldfile) || issue_gold)
                {
                    device->EndFrame(true /*display*/);
                    device->CleanGarbage(120);
                    // the result is the new gold image. should be eye balled and verified.
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
                }
                else
                {
                    INFO("'%1' vs '%2' OK.", goldfile, resultfile);
                    stop_for_input = false;
                }

                device->EndFrame(true /* display */);
                device->CleanGarbage(120);

                if (stop_for_input)
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
    }

    context->Dispose();
    return 0;
}
