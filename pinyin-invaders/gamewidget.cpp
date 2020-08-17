// Copyright (c) 2010-2014 Sami Väisänen, Ensisoft
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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include <cmath>
#include <ctime>

#include "audio/sample.h"
#include "audio/player.h"
#include "base/bitflag.h"
#include "base/format.h"
#include "base/logging.h"
#include "base/math.h"
#include "base/utility.h"
#include "graphics/image.h"
#include "graphics/device.h"
#include "graphics/text.h"
#include "graphics/painter.h"
#include "graphics/types.h"
#include "graphics/drawable.h"
#include "graphics/drawing.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "gamelib/loader.h"
#include "gamelib/animation.h"
#include "gamelib/settings.h"
#include "wdk/opengl/config.h"
#include "wdk/opengl/context.h"
#include "wdk/opengl/surface.h"
#include "wdk/window.h"
#include "wdk/events.h"
#include "wdk/system.h"
#include "gamewidget.h"
#include "game.h"
#include "level.h"

namespace invaders
{

using ParticleEngine = gfx::KinematicsParticleEngine;
using FRect  = gfx::FRect;
using IRect  = gfx::IRect;
using FPoint = gfx::FPoint;
using IPoint = gfx::IPoint;

extern audio::AudioPlayer* g_audio;
extern game::ResourceLoader* g_loader;

const auto LevelUnlockCriteria = 0.85;
const auto GameCols = 40;
const auto GameRows = 10;

template<typename To, typename From>
To* CollisionCast(From* lhs, From* rhs)
{
    To* ptr = dynamic_cast<To*>(lhs);
    if (ptr) return ptr;
    return dynamic_cast<To*>(rhs);
}

template<typename To, typename From>
To* CollisionCast(const std::unique_ptr<From>& lhs,
                  const std::unique_ptr<From>& rhs)
{
    return CollisionCast<To>(lhs.get(), rhs.get());
}


inline gfx::Material SlidingGlintEffect(float secs)
{
    return gfx::Material(gfx::Material::Type::Color)
        .SetShaderFile("shaders/es2/sliding_glint_effect.glsl")
        .SetSurfaceType(gfx::Material::SurfaceType::Transparent)
        .SetRuntime(secs);
}

inline gfx::Material ConcentricRingsEffect(float secs)
{
    return gfx::Material(gfx::Material::Type::Color)
        .SetShaderFile("shaders/es2/concentric_rings_effect.glsl")
        .SetSurfaceType(gfx::Material::SurfaceType::Transparent)
        .SetRuntime(secs);
}

//
// GridLayout divides the given area (rectangle) in whatever units (pixels really)
// into a grid of rows and columns.
// Then it provides operations for mapping points and coordinates in some space
// into the coordinate space the GridLayout is relative to.
//
// We have the following coordinate spaces:
// - layout space expressed with row/column pairs and which expresses positions
//   relative to the grid layout.
// - layout space expressed in normalized units (floats) and which expresses positions
//   relative to the grid layout so that x=0.0 maps to left edge and x=1.0 maps to right edge
//   of the space. y=0.0 maps to the top of the layout and y=1.0 maps to the bottom.
//
// Coordinates in all the above coordinate spaces get mapped to pixel space suitable
// for drawing objects in the window using QPainter or gfx::Painter.
// For example a layout space row/col pair will get mapped to the top left corner of the
// cell in window/pixel space so it's easy to draw.
//
class GridLayout
{
public:
    // Divide the given rectangle into a grid of columns and rows.
    GridLayout(const IRect& rect, unsigned num_cols, unsigned num_rows)
        : mNumCols(num_cols)
        , mNumRows(num_rows)
        , mOriginX(rect.GetX())
        , mOriginY(rect.GetY())
        , mWidth(rect.GetWidth())
        , mHeight(rect.GetHeight())
    {}

    // Map a range of cells into a rectangle so that the returned
    // rectangle covers the cells from the top left cell's top left corner
    // to the bottom right cell's bottom right corner.
    IRect MapRect(const IPoint& top_left_cell, const IPoint& bottom_right_cell) const
    {
        const FPoint& top = MapPoint(top_left_cell);
        const FPoint& bot = MapPoint(bottom_right_cell);
        const FPoint& dim = bot - top;
        return IRect(top.GetX(), top.GetY(), dim.GetX(), dim.GetY());
    }

    gfx::FRect MapGfxRect(const IPoint& top_left_cell, const IPoint& bottom_right_cell) const
    {
        const auto& rc = MapRect(top_left_cell, bottom_right_cell);
        return gfx::FRect(rc);
    }

    // Map a a grid position in layout grid space into
    // parent coordinate space.
    FPoint MapPoint(const IPoint& cell) const
    {
        const auto& scale = GetCellDimensions();
        const float xpos = cell.GetX() * scale.GetX() + mOriginX;
        const float ypos = cell.GetY() * scale.GetY() + mOriginY;
        return { xpos, ypos };
    }

    // Map a normalized position in the layout space into
    // parent coordinate space.
    FPoint MapPoint(const glm::vec2& norm) const
    {
        const float xpos = mWidth * norm.x + mOriginX;
        const float ypos = mHeight * norm.y + mOriginY;
        return FPoint(xpos, ypos);
    }

    IPoint GetCellDimensions() const
    {
        return IPoint(mWidth / mNumCols, mHeight / mNumRows);
    }

    gfx::FRect GetGfxRect() const
    {
        return {mOriginX, mOriginY, mWidth, mHeight};
    }

    unsigned GetFontSize() const
    {
        return (mHeight / mNumRows);
    }

    // Get Width of the grid layout in Pixel
    unsigned GetGridWidth() const
    { return mWidth; }

    unsigned GetGridHeight() const
    { return mHeight; }

    unsigned GetNumCols() const
    { return mNumCols; }

    unsigned GetNumRows() const
    { return mNumRows; }
private:
    const unsigned mNumCols = 0;
    const unsigned mNumRows = 0;

    // origin of this GridLayout relative to parent.
    const float mOriginX = 0.0f;
    const float mOriginY = 0.0f;

    // extents of this grid layout
    const float mWidth  = 0.0f;
    const float mHeight = 0.0f;
};

using GameLayout = GridLayout;

// Compute game layout object for the window with the given window
// width and height.
GameLayout GetGameWindowLayout(unsigned width, unsigned height)
{
    // the invader position is expressed in normalized units.
    // we need to map those coordinates into pixel space,
    // while maintaining aspect ratio and also doing a little kludge
    // to map the position so that the invaders would appear and disappear
    // smoothly instead of abruptly. For this we only map a partial
    // amount of GameCol columns in the visible window. (4 columns each side
    // will be outside of the visible window).
    // Similarly we reserve some space at the top and at the bottom of the
    // window for the HUD so that it won't obstruct the game objects.
    const auto cell_width  = width / (GameCols - 8);
    const auto cell_height = height / (GameRows + 2);
    const auto game_width  = cell_width * GameCols;
    const auto game_height = cell_height * GameRows;
    const auto half_width_diff  = (game_width - width) / 2;
    const auto half_height_diff = (height - game_height) / 2 ;

    IRect rect;
    rect.Resize(game_width, game_height);
    rect.Move(-half_width_diff, half_height_diff);

    return GameLayout(rect, GameCols, GameRows);
}

GameLayout GetGameWindowLayout(const IRect& rect)
{
    // todo: we should work out the x/y offset
    return GetGameWindowLayout(rect.GetWidth(), rect.GetHeight());
}

class GameWidget::State
{
public:
    virtual ~State() = default;

    enum class Action {
        None,
        OpenHelp,
        OpenSettings,
        OpenAbout,
        CloseState,
        QuitApp,
        NewGame
    };

    // Paint the user interface state with the painter in the render target.
    // The given rect defines the sub-rectangle (the box) inside the
    // render target where the painting should occur.
    // No scissor is set by default instead the state should set the
    // scissor as needed once the final transformation is done.
    virtual void paint(gfx::Painter& painter, const IRect& rect) const = 0;

    // map keyboard input to an action.
    virtual Action mapAction(const wdk::WindowEventKeydown& key) const = 0;

    // update the state.. umh.. state from the delta time
    virtual void update(float dt)
    {}

    // handle the raw unmapped keyboard event
    virtual void keyPress(const wdk::WindowEventKeydown& key)
    {}

    // returns true if state represents the running game.
    virtual bool isGameRunning() const
    { return false; }

    virtual void setPlaySounds(bool on)
    {}
    virtual void setMasterUnlock(bool on)
    {}

private:
};


class GameWidget::Animation
{
public:
    virtual ~Animation() = default;

    // returns true if animation is still valid
    // otherwise false and the animation is expired.
    virtual bool update(float dt) = 0;

    // Paint/Draw the animation with the painter in the render target.
    // The given rect defines the sub-rectangle (the box) inside the
    // render target where the painting should occur.
    // No scissor is set by default instead the animatiojn should
    // set the scissor as needed once the final transformation is done.
    virtual void paint(gfx::Painter& painter, const IRect& rect) = 0;

    // Get the bounds of the animation object with respect to the
    // given window rect.
    virtual FRect getBounds(const IRect& rect) const
    {
        return FRect();
    }
    enum class ColliderType {
        None,
        UFO,
        Asteroid,
    };

    virtual ColliderType getColliderType() const
    { return ColliderType::None; }

protected:
private:
};

class GameWidget::Asteroid : public GameWidget::Animation
{
public:
    Asteroid(const glm::vec2& direction)
    {
        mX = math::rand(0.0f, 1.0f);
        mY = math::rand(0.0f, 1.0f);
        mVelocity  = 0.08 + math::rand(0.0, 0.08);
        mScale     = math::rand(0.2, 0.8);
        mTexture   = math::rand(0, 2);
        mDirection = direction;
    }
    virtual bool update(float dt) override
    {
        const auto d = mDirection * mVelocity * (dt / 1000.0f);
        mX = math::wrap(-0.2f, 1.0f, mX + d.x);
        mY = math::wrap(-0.2f, 1.0f, mY + d.y);
        return true;
    }
    virtual void paint(gfx::Painter& painter, const IRect& rect) override
    {
        const auto& size = getTextureSize(mTexture);
        const char* name = getTextureName(mTexture);

        // the asteroids are just in their own space which we simply map
        // to the whole of the given rectangle
        const auto width  = rect.GetWidth();
        const auto height = rect.GetHeight();
        const auto xpos = rect.GetX();
        const auto ypos = rect.GetY();

        gfx::Transform t;
        t.Resize(size * mScale);
        t.MoveTo(width * mX + xpos, height * mY + ypos);
        painter.Draw(gfx::Rectangle(), t, gfx::TextureMap(name).SetSurfaceType(gfx::Material::SurfaceType::Transparent));
    }

    virtual FRect getBounds(const IRect& rect) const override
    {
        const auto& size = getTextureSize(mTexture);

        const auto width  = rect.GetWidth();
        const auto height = rect.GetHeight();
        const auto xpos = rect.GetX();
        const auto ypos = rect.GetY();

        FRect bounds;
        bounds.Resize(size * mScale);
        bounds.Move(width * mX + xpos, height * mY + ypos);
        return bounds;
    }
    virtual ColliderType getColliderType() const override
    { return ColliderType::Asteroid; }
private:
    static const char* getTextureName(unsigned index)
    {
        static const char* textures[] = {
            "textures/asteroid0.png",
            "textures/asteroid1.png",
            "textures/asteroid2.png"
        };
        return textures[index];
    }
    static gfx::FSize getTextureSize(unsigned index)
    {
        static gfx::FSize sizes[] = {
            gfx::FSize(78, 74),
            gfx::FSize(74, 63),
            gfx::FSize(72, 58)
        };
        return sizes[index];
    }
private:
    float mVelocity = 0.0f;
    float mScale = 1.0f;
    float mX = 0.0f;
    float mY = 0.0f;
    glm::vec2 mDirection;
    unsigned mTexture = 0;
};

// Flame/Smoke emitter
class GameWidget::Explosion : public GameWidget::Animation
{
public:
    Explosion(const glm::vec2& position, float start, float lifetime)
      : mPosition(position)
      , mStartTime(start)
      , mLifeTime(lifetime)
      , mSprite(gfx::SpriteMap())
    {
        mSprite.SetFps(80.0 / (lifetime/1000.0f));
        // each explosion frame is 100x100 px
        // and there are 80 frames total.
        for (unsigned i=0; i<80; ++i)
        {
            const auto row = i / 10;
            const auto col = i % 10;
            const auto w = 100 / 1024.0f; // normalize over width
            const auto h = 100 / 1024.0f; // normalize over height
            const gfx::FRect frame(col * w, row * h, w, h);
            mSprite.AddTexture("textures/ExplosionMap.png");
            mSprite.SetTextureRect(i, frame);
        }
    }

    virtual bool update(float dt) override
    {
        mTime += dt;
        if (mTime < mStartTime)
            return true;

        if (mTime - mStartTime > mLifeTime)
            return false;

        return true;
    }

    virtual void paint(gfx::Painter& painter, const IRect& rect) override
    {
        if (mTime < mStartTime)
            return;

        mSprite.SetRuntime((mTime - mStartTime) / 1000.0f);

        const auto& layout = GetGameWindowLayout(rect);
        const auto unitScale = layout.GetCellDimensions();
        const auto position  = layout.MapPoint(mPosition);
        const auto scaledWidth = unitScale.GetX() * mScale;
        const auto scaledHeight = unitScale.GetX() * mScale; // * aspect;

        gfx::Transform t;
        t.Resize(scaledWidth, scaledHeight);
        t.MoveTo(position - FPoint(scaledWidth / 2.0, scaledHeight / 2.0));
        painter.Draw(gfx::Rectangle(), t, mSprite);
    }

    void setScale(float scale)
    { mScale = scale; }

    glm::vec2 getPosition() const
    { return mPosition; }

private:
    const glm::vec2 mPosition;
    const float mStartTime = 0.0f;
    const float mLifeTime  = 0.0f;
    float mTime  = 0.0f;
    float mScale = 1.0f;
private:
    gfx::Material mSprite;
};


// "fire" sparks emitter, high velocity  (needs texturing)
class GameWidget::Sparks : public GameWidget::Animation
{
public:
    Sparks(const glm::vec2& position, float start, float lifetime)
        : mStartTime(start)
        , mLifeTime(lifetime)
        , mPosition(position)
    {
        ParticleEngine::Params params;
        params.max_xpos = 500;
        params.max_ypos = 500;
        params.init_rect_xpos = 250;
        params.init_rect_ypos = 250;
        params.init_rect_width = 0.0f;
        params.init_rect_height = 0.0f;
        params.num_particles = 100;
        params.min_point_size = 2.0f;
        params.max_point_size = 2.0f;
        params.min_velocity = 200.0f;
        params.max_velocity = 300.0f;
        params.mode = ParticleEngine::SpawnPolicy::Once;
        mParticles = std::make_unique<ParticleEngine>(params);
    }
    virtual bool update(float dt) override
    {
        mTimeAccum += dt;
        if (mTimeAccum < mStartTime)
            return true;

        if (mTimeAccum - mStartTime > mLifeTime)
            return false;

        mParticles->Update(dt / 1000.0f);
        return true;
    }
    virtual void paint(gfx::Painter& painter, const IRect& rect) override
    {
        if (mTimeAccum < mStartTime)
            return;

        const auto& layout = GetGameWindowLayout(rect);
        const auto pos = layout.MapPoint(mPosition);
        const auto x = pos.GetX();
        const auto y = pos.GetY();

        gfx::Transform t;
        t.Resize(500, 500);
        t.MoveTo(x-250, y-250);

        painter.Draw(*mParticles, t,
            gfx::TextureMap("textures/RoundParticle.png")
            .SetSurfaceType(gfx::Material::SurfaceType::Emissive)
            .SetBaseColor(mColor * 0.8));
    }

    void setColor(const gfx::Color4f& color)
    { mColor = color; }
private:
    float mStartTime = 0.0f;
    float mLifeTime  = 0.0f;
    float mTimeAccum = 0.0f;
    std::unique_ptr<ParticleEngine> mParticles;
private:
    glm::vec2 mPosition;
    gfx::Color4f mColor;
};

class GameWidget::Smoke : public GameWidget::Animation
{
public:
    Smoke(const glm::vec2& position, float start, float lifetime)
        : mPosition(position)
        , mStartTime(start)
        , mLifeTime(lifetime)
        , mSprite(gfx::SpriteSet())
    {
        mSprite.SetFps(10);
        for (int i=0; i<=24; ++i)
        {
            const auto& name = base::FormatString("textures/smoke/blackSmoke%1.png",  i);
            mSprite.AddTexture(name);
        }
        mSprite.SetBaseColor(gfx::Color4f(1.0f, 1.0f, 1.0f, 0.3));
    }

    virtual bool update(float dt) override
    {
        mTime += dt;
        if (mTime < mStartTime)
            return true;

        if (mTime - mStartTime > mLifeTime)
            return false;

        return true;
    }
    virtual void paint(gfx::Painter& painter, const IRect& rect) override
    {
        if (mTime < mStartTime)
            return;

        const auto time  = mTime - mStartTime;
        const auto alpha = 0.4 - 0.4 * (time / mLifeTime);
        mSprite.SetRuntime(time / 1000.0f);
        mSprite.SetBaseColor(gfx::Color4f(1.0f, 1.0f, 1.0f, alpha));

        const auto& layout = GetGameWindowLayout(rect);
        const auto unitScale = layout.GetCellDimensions();
        const auto pxw = unitScale.GetX() * mScale;
        const auto pxh = unitScale.GetY() * mScale;
        const auto pos = layout.MapPoint(mPosition);

        gfx::Transform t;
        t.Resize(pxw, pxh);
        t.MoveTo(pos - FPoint(pxw/2.0f, pxh/2.0f));
        painter.Draw(gfx::Rectangle(), t, mSprite);

    }

    void setScale(float scale)
    { mScale = scale; }

private:
    const glm::vec2 mPosition;
    const float mStartTime = 0.0f;
    const float mLifeTime  = 0.0f;
    float mTime  = 0.0f;
    float mScale = 1.0f;
private:
    gfx::Material mSprite;
};


// slower moving debris, remnants of enemy. uses enemy texture as particle texture.
class GameWidget::Debris : public GameWidget::Animation
{
public:
    // how to split the debris texture into debris rectangles
    enum {
        NumParticleCols = 4,
        NumParticleRows = 2
    };

    Debris(const std::string& texture, const glm::vec2& position, float starttime, float lifetime)
        : mStartTime(starttime)
        , mLifeTime(lifetime)
        , mTexture(texture)
    {
        const auto particleWidth  = 1.0f / NumParticleCols;
        const auto particleHeight = 1.0f / NumParticleRows;
        const auto numParticles   = NumParticleCols * NumParticleRows;

        const auto angle = (M_PI * 2) / numParticles;

        for (auto i=0; i<numParticles; ++i)
        {
            const auto col = i % NumParticleCols;
            const auto row = i / NumParticleCols;
            const auto x = col * particleWidth;
            const auto y = row * particleHeight;

            const auto r = (float)std::rand() / RAND_MAX;
            const auto v = (float)std::rand() / RAND_MAX;
            const auto a = i * angle + angle * r;

            particle p;
            p.rc  = gfx::FRect(x, y, particleWidth, particleHeight);
            p.dir.x = std::cos(a);
            p.dir.y = std::sin(a);
            p.dir *= v;
            p.pos = position;
            p.alpha = 1.0f;
            p.angle = (M_PI * 2 ) * (float)std::rand() / RAND_MAX;
            p.rotation_coefficient = math::rand(-1.0f, 1.0f);
            mParticles.push_back(p);
        }
    }
    virtual bool update(float dt) override
    {
        mTime += dt;
        if (mTime < mStartTime)
            return true;

        if  (mTime - mStartTime > mLifeTime)
            return false;

        for (auto& p : mParticles)
        {
            p.pos += p.dir * (dt / 4500.0f);
            p.alpha = math::clamp(0.0f, 1.0f, p.alpha - (dt / 3000.0f));
            p.angle += ((M_PI * 2.0f) * (dt / 2000.0f) * p.rotation_coefficient);
        }

        return true;
    }
    virtual void paint(gfx::Painter& painter, const IRect& rect) override
    {
        if (mTime < mStartTime)
            return;

        // todo: this is actually incorrect when using debris with the UFO explosion.
        const auto& layout = GetGameWindowLayout(rect);

        for (const auto& p : mParticles)
        {
            const auto pos = layout.MapPoint(p.pos);

            // todo: should fix the dimensions, i.e. we don't know what size our
            // debris should have when rendered. Also we don't know the aspect ratio
            // these should be taken as a parameter.
            const float width  = 25; //p.rc.GetWidth();
            const float height = 50; //p.rc.GetHeight();
            const float aspect = height / width;
            const float scaledWidth  = width * mScale;
            const float scaledHeight = scaledWidth * aspect;

            gfx::Transform rotation;
            rotation.Resize(scaledWidth, scaledHeight);
            rotation.Translate(-scaledWidth / 2, -scaledHeight / 2);
            rotation.Rotate(p.angle);
            rotation.Translate(scaledWidth / 2, scaledHeight / 2);
            rotation.Translate(pos);
            painter.Draw(gfx::Rectangle(), rotation,
                gfx::TextureMap(mTexture)
                .SetSurfaceType(gfx::Material::SurfaceType::Transparent)
                .SetTextureRect(0, p.rc)
                .SetBaseColor(gfx::Color4f(gfx::Color::White, p.alpha)));
        }
    }

    void setTextureScaleFromWidth(float width)
    {
        gfx::Image file(mTexture);
        const auto particleWidth = (file.GetWidth() / NumParticleCols);
        mScale = width / particleWidth;
    }

    void setTextureScale(float scale)
    { mScale = scale; }

private:
    struct particle {
        gfx::FRect rc;
        glm::vec2 dir;
        glm::vec2 pos;
        float angle = 0.0f;
        float alpha = 0.0f;
        float rotation_coefficient = 1.0f;
    };
    std::vector<particle> mParticles;
private:
    const float mStartTime = 0.0f;;
    const float mLifeTime  = 0.0f;;
    float mTime  = 0.0f;
    float mScale = 1.0f;
    std::string mTexture;

};

class GameWidget::Invader : public GameWidget::Animation
{
public:
    enum class ShipType {
        Slow,
        Fast,
        Tough,
        Boss
    };

    Invader(const glm::vec2& position, const std::wstring& str, float velocity, ShipType type)
      : mPosition(position)
      , mText(str)
      , mVelocity(velocity)
      , mShipType(type)
    {
        // keep in mind that the exact shape of the jet stream depends
        // on the contours of the ship in the texture.
        // for example a ship that has only one exhaust pipe in the
        // middle of the ship would not emit exhaust particles for the
        // entire height of the ship.
        // therefore we must still look up this data from the image
        // even if we're not actually using the image data per se anymore.
        const gfx::Image ship(getShipTextureIdentifier(type));
        const gfx::Image jet(getJetStreamTextureIdentifier(type));
        mShipWidth  = ship.GetWidth();
        mShipHeight = ship.GetHeight();
        // see comments above about the shape and size of the exhaust area
        mJetWidth  = jet.GetWidth();
        mJetHeight = jet.GetHeight();
    }

    virtual bool update(float dt) override
    {
        const auto direction = glm::vec2(-1.0f, 0.0f);

        mPosition += mVelocity * dt * direction;
        if (mMaxLifeTime)
        {
            mLifeTime += dt;
            if (mLifeTime > mMaxLifeTime)
                return false;
        }
        if (mParticles)
            mParticles->Update(dt/1000.0f);
        return true;
    }

    virtual void paint(gfx::Painter& painter, const IRect& rect) override
    {
        const auto& layout = GetGameWindowLayout(rect);

        // offset the texture to be centered around the position
        const auto unitScale   = layout.GetCellDimensions();
        const auto spriteScale = layout.GetCellDimensions();
        const auto position    = layout.MapPoint(mPosition);

        const float shipWidth  = mShipWidth;
        const float shipHeight = mShipHeight;
        const float shipAspect = shipHeight / shipWidth;
        const float shipScaledWidth  = spriteScale.GetX() * getScale();
        const float shipScaledHeight = shipScaledWidth * shipAspect;

        const float jetWidth  = mJetWidth;
        const float jetHeight = mJetHeight;
        const float jetAspect = jetHeight / jetWidth;
        const float jetScaledWidth  = spriteScale.GetX() * getScale();
        const float jetScaledHeight = jetScaledWidth * jetAspect;

        if (!mParticles)
        {
            ParticleEngine::Params params;
            params.init_rect_width  = 0.0f;
            params.init_rect_height = jetScaledHeight;
            params.max_xpos = jetScaledWidth;
            params.max_ypos = jetScaledHeight;
            params.num_particles = 200;
            params.min_velocity = 100.0f;
            params.max_velocity = 150.0f;
            params.min_point_size = 20.0f;
            params.max_point_size = 30.0f;
            params.direction_sector_start_angle = 0.0f;
            params.direction_sector_size = 0.0f;
            params.rate_of_change_in_size_wrt_time = -20.0f;
            params.mode = ParticleEngine::SpawnPolicy::Continuous;
            mParticles = std::make_unique<ParticleEngine>(params);
        }
        // set the target rectangle with the dimensions of the
        // sprite we want to draw.
        // the ship rect is the coordinate to which the jet stream and
        // the text are relative to.
        // the ship's x,y coordinate is offset so that the center of the
        // sprite is where the ship's game space coordinate maps to.
        const auto shipTopLeft = position -
            FPoint(shipScaledWidth / 2.0f, shipScaledHeight / 2.0f);

        // have to do a little fudge here since the scarab ship has
        // a contour such that positioning the particle engine just behind
        // the ship texture will leave a silly gap between the ship and the
        // particles.
        const auto fudgeFactor = mShipType == ShipType::Slow ? 0.8 : 1.0f;

        gfx::Transform t;
        t.Resize(jetScaledWidth, jetScaledHeight);
        t.MoveTo(shipTopLeft);
        t.Translate(shipScaledWidth * fudgeFactor, (shipScaledHeight - jetScaledHeight) / 2.0f);

        // draw the particles first
        painter.Draw(*mParticles, t,
            gfx::TextureMap("textures/BlackSmoke.png")
            .SetSurfaceType(gfx::Material::SurfaceType::Emissive)
            .SetBaseColor(getJetStreamColor(mShipType) * 0.6));

        t.Reset();
        t.Resize(shipScaledWidth, shipScaledHeight);
        t.MoveTo(shipTopLeft);

        // then draw the ship so that it creates a nice clear cut where
        // the exhaust particles begin at the end of the ship
        painter.Draw(gfx::Rectangle(), t,
            gfx::TextureMap(getShipTextureIdentifier(mShipType))
                .SetSurfaceType(gfx::Material::SurfaceType::Transparent));

        const auto fontsize = unitScale.GetY() / 1.75;
        gfx::TextBuffer text(shipScaledWidth, shipScaledHeight);
        gfx::TextBuffer::Text text_and_style;
        text_and_style.text = base::ToUtf8(mText);
        text_and_style.font = "fonts/SourceHanSerifTC-SemiBold.otf";
        text_and_style.fontsize = fontsize;
        text_and_style.halign = gfx::TextBuffer::HorizontalAlignment::AlignLeft;
        text_and_style.valign = gfx::TextBuffer::VerticalAlignment::AlignCenter;
        text.AddText(std::move(text_and_style));

        t.Translate(shipScaledWidth * 0.6 + jetScaledWidth * 0.75, 0);

        painter.Draw(gfx::Rectangle(), t, gfx::BitmapText(text)
            .SetBaseColor(gfx::Color::DarkYellow));

        if (mShieldIsOn)
        {
            FRect rect;
            // we don't bother to calculate the size for the shield properly
            // in order to cover the whole ship. instead we use a little fudge
            // factor to expand the shield.
            const auto fudge = 1.25f;
            const auto width = shipScaledWidth;
            gfx::Transform t;
            t.Resize(width * fudge, width * fudge);
            t.MoveTo(shipTopLeft);
            t.Translate((width - shipScaledWidth) * -0.5,
                        (width - shipScaledWidth) * -0.5);
            painter.Draw(gfx::Rectangle(), t,
                gfx::TextureMap("textures/spr_shield.png")
                    .SetSurfaceType(gfx::Material::SurfaceType::Transparent));
        }
    }

    float getScale() const
    {
        switch (mShipType)
        {
            case ShipType::Slow:  return 5.0f;
            case ShipType::Fast:  return 4.0f;
            case ShipType::Boss:  return 6.5f;
            case ShipType::Tough: return 3.5f;
        }
        return 1.0f;
    }

    // get the invader position in game space at some later time
    // where time is in seconds.
    glm::vec2 getFuturePosition(float seconds) const
    {
        const auto direction = glm::vec2(-1.0f, 0.0f);

        return mPosition + seconds * mVelocity * direction;
    }

    // get current position
    glm::vec2 getPosition() const
    { return mPosition; }

    void setMaxLifetime(float ms)
    { mMaxLifeTime = ms; }

    void setViewString(const std::wstring& str)
    { mText = str; }

    std::string getTextureName() const
    { return getShipTextureIdentifier(mShipType); }

    void enableShield(bool onOff)
    { mShieldIsOn = onOff; }

private:
    static gfx::Color4f getJetStreamColor(ShipType type)
    {
        static gfx::Color4f colors[] = {
            gfx::Color4f(117, 221, 234, 100),
            gfx::Color4f(252, 214, 131, 100),
            gfx::Color4f(126, 200, 255, 100),
            gfx::Color4f(5, 244, 159, 100)
        };
        return colors[(int)type];
    }
    static const char* getShipTextureIdentifier(ShipType type)
    {
        static const char* textures[] = {
            "textures/Cricket.png",
            "textures/Mantis.png",
            "textures/Scarab.png",
            "textures/Locust.png"
        };
        return textures[(int)type];
    }

    static const char* getJetStreamTextureIdentifier(ShipType type)
    {
        static const char* textures[] = {
            "textures/Cricket_jet.png",
            "textures/Mantis_jet.png",
            "textures/Scarab_jet.png",
            "textures/Locust_jet.png"
        };
        return textures[(int)type];
    }

private:
    glm::vec2 mPosition;
    std::wstring mText;
    float mLifeTime    = 0.0f;
    float mMaxLifeTime = 0.0f;
    float mVelocity    = 0.0f;
    // texture dimension in pixels
    unsigned mShipWidth   = 0.0f;
    unsigned mShipHeight  = 0.0f;
    // texture dimension in pixels
    unsigned mJetWidth    = 0.0f;
    unsigned mJetHeight   = 0.0f;
    std::unique_ptr<ParticleEngine> mParticles;
private:
    ShipType mShipType = ShipType::Slow;
private:
    bool mShieldIsOn = false;
};

class GameWidget::Missile : public GameWidget::Animation
{
public:
    Missile(const glm::vec2& position,
            const glm::vec2& direction,
            const std::wstring& str,
            float lifetime)
        : mDirection(direction)
        , mText(str)
        , mLifetime(lifetime)
        , mPosition(position)
    {}
    virtual bool update(float dt) override
    {
        mTimeAccum += dt;
        if (mTimeAccum > mLifetime)
            return false;

        const auto d = (float)dt / (float)mLifetime;
        const auto p = mDirection * d;
        mPosition += p;
        return true;
    }
    virtual void paint(gfx::Painter& painter, const IRect& rect)  override
    {
        const auto& layout = GetGameWindowLayout(rect);
        const auto dim = layout.GetCellDimensions();
        const auto pos = layout.MapPoint(mPosition);
        const auto font_size = dim.GetY() / 2;

        // todo: we used QFontMetrics before to estimate the size
        // of the bounding box for the text.
        const auto w = 100.0f;
        const auto h = font_size * 2;
        const auto p = pos - FPoint(w*0.5, h*0.5);

        gfx::DrawTextRect(painter,
            base::ToUtf8(mText),
            "fonts/ARCADE.TTF", font_size,
            gfx::FRect(p, w, h),
            gfx::Color::White);
    }

private:
    const glm::vec2 mDirection;
    const std::wstring mText;
    const float mLifetime = 0.0f;
    float mTimeAccum = 0.0f;
    glm::vec2 mPosition;
};

class GameWidget::UFO : public GameWidget::Animation
{
public:
    UFO() : mSprite(gfx::SpriteSet())
    {
        mPosition.x = math::rand(0.0, 1.0);
        mPosition.y = math::rand(0.0, 1.0);

        const auto x = math::rand(-1.0, 1.0);
        const auto y = math::rand(-1.0, 1.0);
        mDirection = glm::normalize(glm::vec2(x, y));

        mSprite.AddTexture("textures/alien/e_f1.png");
        mSprite.AddTexture("textures/alien/e_f2.png");
        mSprite.AddTexture("textures/alien/e_f3.png");
        mSprite.AddTexture("textures/alien/e_f4.png");
        mSprite.AddTexture("textures/alien/e_f5.png");
        mSprite.AddTexture("textures/alien/e_f6.png");
        mSprite.SetFps(10);
    }

    virtual bool update(float dt) override
    {
        const auto maxLifeTime = 10000.0f;

        mRuntime += dt;
        if (mRuntime >= maxLifeTime)
            return false;

        glm::vec2 fuzzy;
        fuzzy.y = std::sin((fmodf(mRuntime, 3000) / 3000.0) * 2 * M_PI);
        fuzzy.x = mDirection.x;
        fuzzy = glm::normalize(fuzzy);

        mPosition += (dt / 10000.0f) * fuzzy;
        const auto x = mPosition.x;
        const auto y = mPosition.y;
        mPosition.x = math::wrap(0.0f, 1.0f, x);
        mPosition.y = math::wrap(0.0f, 1.0f, y);
        return true;
    }

    virtual void paint(gfx::Painter& painter, const IRect& rect) override
    {
        const auto width  = rect.GetWidth();
        const auto height = rect.GetHeight();
        const auto xpos   = rect.GetX();
        const auto ypos   = rect.GetY();

        const auto sec = mRuntime / 1000.0f;
        const auto pos = FPoint(mPosition.x * width + xpos,
                                mPosition.y * height + ypos);

        mSprite.SetRuntime(sec);

        gfx::Transform rings;
        rings.Resize(200, 200);
        rings.MoveTo(pos - FPoint(100.0f, 100.0f));
        painter.Draw(gfx::Rectangle(), rings, ConcentricRingsEffect(sec));

        gfx::Transform ufo;
        ufo.Resize(40, 40);
        ufo.MoveTo(pos - FPoint(20.0f, 20.0f));
        painter.Draw(gfx::Rectangle(), ufo, mSprite);
    }

    virtual FRect getBounds(const IRect& rect) const override
    {
        const auto width  = rect.GetWidth();
        const auto height = rect.GetHeight();
        const auto xpos = rect.GetX();
        const auto ypos = rect.GetY();

        const auto pos = FPoint(mPosition.x * width + xpos,
                                mPosition.y * height + ypos);
        FRect bounds;
        bounds.Move(pos - FPoint(20.0f, 20.0f));
        bounds.Resize(40.0f, 40.0f);
        return bounds;
    }

    virtual ColliderType getColliderType() const override
    { return ColliderType::UFO; }

    void invertDirection()
    {
        mDirection *= -1.0f;
    }

    glm::vec2 getPosition() const
    { return mPosition; }

    static bool shouldMakeRandomAppearance()
    {
        if (math::rand(0, 5000) == 7)
            return true;
        return false;
    }

    std::string getTextureName() const
    { return "textures/alien/e_f1.png"; }
private:
    float mRuntime = 0.0f;
    glm::vec2 mDirection;
    glm::vec2 mPosition;
private:
    gfx::Material mSprite;
};


class GameWidget::BigExplosion : public GameWidget::Animation
{
public:
    BigExplosion(float lifetime) : mLifeTime(lifetime), mSprite(gfx::SpriteSet())
    {
        for (int i=1; i<=90; ++i)
        {
            const auto& name = base::FormatString("textures/bomb/explosion1_00%1.png", i);
            mSprite.AddTexture(name);
        }
        mSprite.SetFps(90 / (lifetime/1000.0f));
    }

    virtual bool update(float dt) override
    {
        mRunTime += dt;
        if (mRunTime > mLifeTime)
            return false;
        return true;
    }
    virtual void paint(gfx::Painter& painter, const IRect& rect) override
    {
        mSprite.SetRuntime(mRunTime / 1000.0f);

        const auto& layout = GetGameWindowLayout(rect);
        const auto ExplosionWidth = layout.GetGridWidth() * 2.0f;
        const auto ExplosionHeight = layout.GetGridHeight() * 2.3f;

        const auto x = layout.GetGridWidth() / 2 - (ExplosionWidth * 0.5);
        const auto y = layout.GetGridHeight() / 2 - (ExplosionHeight * 0.5);

        gfx::Transform bang;
        bang.Resize(ExplosionWidth, ExplosionHeight);
        bang.MoveTo(x, y);
        painter.Draw(gfx::Rectangle(), bang, mSprite);

    }
private:
    const float mLifeTime = 0.0f;
    float mRunTime = 0.0f;
private:
    gfx::Material mSprite;
};

class GameWidget::Score : public GameWidget::Animation
{
public:
    Score(glm::vec2 position, float start, float lifetime, unsigned score)
        : mPosition(position)
        , mStartTime(start)
        , mLifeTime(lifetime)
        , mScore(score)
    {}

    virtual bool update(float dt) override
    {
        mTimeAccum += dt;
        if (mTimeAccum < mStartTime)
            return true;
        return mTimeAccum - mStartTime < mLifeTime;
    }

    virtual void paint(gfx::Painter& painter, const IRect& rect) override
    {
        if (mTimeAccum < mStartTime)
            return;

        const auto& layout = GetGameWindowLayout(rect);
        const auto alpha = 1.0 - (float)(mTimeAccum - mStartTime)/ (float)mLifeTime;
        const auto dim = layout.GetCellDimensions();
        const auto top = layout.MapPoint(mPosition);

        const auto font_size = dim.GetY() / 2;

        gfx::DrawTextRect(painter,
            base::FormatString("%1", mScore),
            "fonts/ARCADE.TTF", font_size,
            gfx::FRect(top, dim.GetX() * 2.0f, dim.GetY()*1.0f),
            gfx::Color::DarkYellow,
            gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignTop);
    }

private:
    const glm::vec2 mPosition;
private:
    const float mStartTime = 0.0f;
    const float mLifeTime = 0.0f;
    const unsigned mScore = 0;
    float mTimeAccum = 0.0f;
private:
};

class GameWidget::Scoreboard : public GameWidget::State
{
public:
    Scoreboard(unsigned score, unsigned bonus, bool isHighScore, int unlockedLevel)
    {
        mText.append("Level complete!\n\n");
        mText.append(base::FormatString("You scored %1 points\n", score));
        mText.append(base::FormatString("Difficulty bonus %1 points\n", bonus));
        mText.append(base::FormatString("Total %1 points\n\n", score + bonus));

        if (isHighScore)
            mText.append("New high score!\n");

        if (unlockedLevel)
            mText.append(base::FormatString("Level %1 unlocked!\n", unlockedLevel + 1));

        mText.append("\nPress any key to continue");
    }

    virtual void paint(gfx::Painter& painter, const IRect& rect) const override
    {
        const GridLayout layout(rect, 1, 20);

        const auto width  = layout.GetGridWidth();
        const auto height = layout.GetGridHeight();

        gfx::DrawTextRect(painter,
            mText,
            "fonts/ARCADE.TTF", layout.GetFontSize(),
            gfx::FRect(0, 0, width, height),
            gfx::Color::White);
    }

    virtual Action mapAction(const wdk::WindowEventKeydown&) const override
    {
        return Action::CloseState;
    }
private:
    std::string mText;
};


// initial greeting and instructions
class GameWidget::MainMenu : public GameWidget::State
{
public:
    MainMenu(const std::vector<std::unique_ptr<Level>>& levels,
             const std::vector<GameWidget::LevelInfo>& infos,
             bool bPlaySounds)
    : mLevels(levels)
    , mInfos(infos)
    , mPlaySounds(bPlaySounds)
    {}

    virtual void update(float dt) override
    { mTotalTimeRun += dt; }

    virtual void paint(gfx::Painter& painter, const IRect& rect) const override
    {
        const auto cols = 7;
        const auto rows = 6;
        const GridLayout layout(rect, cols, rows);

        const auto font_size_l = layout.GetFontSize() * 0.25;
        const auto font_size_s = layout.GetFontSize() * 0.2;

        gfx::DrawTextRect(painter,
            "Evil chinese characters are attacking!\n"
            "Only you can stop them by typing the right pinyin.\n"
            "Good luck.\n\n"
            "Esc - Exit\n"
            "F1 - Help\n"
            "F2 - Settings\n"
            "F3 - Credits\n\n"
            "Difficulty",
            "fonts/ARCADE.TTF", font_size_l,
            layout.MapGfxRect(IPoint(0, 0), IPoint(cols, 3)),
            gfx::Color::White,
            gfx::AlignHCenter | gfx::AlignVCenter);

        // draw the difficulty settings
        {
            const GridLayout temp(layout.MapRect(IPoint(2, 3), IPoint(5, 4)), 3, 1);
            gfx::DrawTextRect(painter,
                "Easy",
                "fonts/ARCADE.TTF", font_size_s,
                temp.MapGfxRect(IPoint(0, 0), IPoint(1, 1)),
                mCurrentRowIndex == 0 && mCurrentProfileIndex == 0 ?  gfx::Color::Gold : gfx::Color::White,
                gfx::AlignTop | gfx::AlignRight,
                mCurrentProfileIndex == 0 ? gfx::TextProp::Underline : 0);
            gfx::DrawTextRect(painter,
                "Normal",
                "fonts/ARCADE.TTF", font_size_s,
                temp.MapGfxRect(IPoint(1, 0), IPoint(2, 1)),
                mCurrentRowIndex == 0 && mCurrentProfileIndex == 1 ? gfx::Color::Gold : gfx::Color::White,
                gfx::AlignTop | gfx::AlignHCenter,
                mCurrentProfileIndex == 1 ? gfx::TextProp::Underline : 0);
            gfx::DrawTextRect(painter,
                "Chinese",
                "fonts/ARCADE.TTF", font_size_s,
                temp.MapGfxRect(IPoint(2, 0), IPoint(3, 1)),
                mCurrentRowIndex == 0 && mCurrentProfileIndex == 2 ? gfx::Color::Gold : gfx::Color::White,
                gfx::AlignTop | gfx::AlignLeft,
                mCurrentProfileIndex == 2 ? gfx::TextProp::Underline : 0);
        }

        // Draw the levesl
        const auto prev_level_index = mCurrentLevelIndex > 0 ? mCurrentLevelIndex - 1 : mLevels.size() - 1;
        const auto next_level_index = (mCurrentLevelIndex + 1) % mLevels.size();
        drawLevel(painter, layout.MapGfxRect(IPoint(1, 4), IPoint(2, 5)),
            prev_level_index, font_size_s, false);
        drawLevel(painter, layout.MapGfxRect(IPoint(3, 4), IPoint(4, 5)),
            mCurrentLevelIndex, font_size_s, mCurrentRowIndex == 1);
        drawLevel(painter, layout.MapGfxRect(IPoint(5, 4), IPoint(6, 5)),
            next_level_index, font_size_s, false);

        // Draw a little glint effect on top of the middle rectangle
        gfx::DrawRectOutline(painter,
            layout.MapGfxRect(IPoint(3, 4), IPoint(4, 5)),
            SlidingGlintEffect(mTotalTimeRun/1000.0f), 1);

        const auto& play_or_not = mInfos[mCurrentLevelIndex].locked ?
            "This level is locked!" : "Press Space to play!";
        gfx::DrawTextRect(painter,
            play_or_not,
            "fonts/ARCADE.TTF", font_size_l,
            layout.MapGfxRect(IPoint(0, rows-1), IPoint(cols, rows)),
            gfx::Color::White,
            gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter,
            gfx::TextProp::Blinking);
    }

    virtual Action mapAction(const wdk::WindowEventKeydown& key) const
    {
        switch (key.symbol)
        {
            case wdk::Keysym::F1:
                return Action::OpenHelp;
            case wdk::Keysym::F2:
                return Action::OpenSettings;
            case wdk::Keysym::F3:
                return Action::OpenAbout;
            case wdk::Keysym::Escape:
                return Action::QuitApp;
            case wdk::Keysym::Space:
                {
                    if (!mInfos[mCurrentLevelIndex].locked || mMasterUnlock)
                    {
                        return Action::NewGame;
                    }
                }
                break;
        }
        return Action::None;
    }

    virtual void keyPress(const wdk::WindowEventKeydown& key) override
    {
        const int numLevelsMin = 0;
        const int numLevelsMax = mLevels.size() - 1;
        const int numProfilesMin = 0;
        const int numProfilesMax = 2;

        bool bPlaySound = false;

        const auto sym = key.symbol;
        if (sym == wdk::Keysym::ArrowLeft)
        {
            if (mCurrentRowIndex == 0)
            {
                mCurrentProfileIndex = math::wrap(numProfilesMin, numProfilesMax, mCurrentProfileIndex - 1);
            }
            else
            {
                mCurrentLevelIndex = math::wrap(numLevelsMin, numLevelsMax, mCurrentLevelIndex -1);
            }
            bPlaySound = true;
        }
        else if (sym == wdk::Keysym::ArrowRight)
        {
            if (mCurrentRowIndex == 0)
            {
                mCurrentProfileIndex = math::wrap(numProfilesMin, numProfilesMax,  mCurrentProfileIndex + 1);
            }
            else
            {
                mCurrentLevelIndex = math::wrap(numLevelsMin, numLevelsMax, mCurrentLevelIndex + 1);
            }
            bPlaySound = true;
        }
        else if (sym == wdk::Keysym::ArrowUp)
        {
            mCurrentRowIndex = math::wrap(0, 1, mCurrentRowIndex - 1);
        }
        else if (sym == wdk::Keysym::ArrowDown)
        {
            mCurrentRowIndex = math::wrap(0, 1, mCurrentRowIndex + 1);
        }

        if (bPlaySound && mPlaySounds)
        {
        #ifdef GAME_ENABLE_AUDIO
            auto swoosh = std::make_unique<audio::AudioFile>("sounds/Slide_Soft_00.ogg", "swoosh");
            g_audio->Play(std::move(swoosh));
        #endif
        }
    }

    virtual void setPlaySounds(bool on) override
    { mPlaySounds = on; }

    virtual void setMasterUnlock(bool on) override
    { mMasterUnlock = on; }

    size_t getLevelIndex() const
    {
        return static_cast<size_t>(mCurrentLevelIndex);
    }
    size_t getProfileIndex() const
    {
        return static_cast<size_t>(mCurrentProfileIndex);
    }

private:
    void drawLevel(gfx::Painter& painter, const gfx::FRect& rect,
        unsigned index, unsigned font_size, bool hilite) const
    {
        const auto& level = mLevels[index];
        const auto& info = mInfos[index];
        const auto& text = info.locked ? std::string("Locked") :
            info.highScore ? base::FormatString("%1 points", info.highScore) :
            std::string("Play");
        const auto outline_width = 2;
        const auto outline_color = hilite ?
            (info.locked ? gfx::Color::Red : gfx::Color::Green) : gfx::Color::DarkGray;

        gfx::DrawRectOutline(painter, rect, gfx::Color4f(outline_color, 0.7f), 4);
        gfx::DrawTextRect(painter,
            base::FormatString("Level %1\n%2\n%3", index + 1, level->GetName(), text),
            "fonts/ARCADE.TTF", font_size,
            rect, outline_color,
            gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter);
    }
private:
    const std::vector<std::unique_ptr<Level>>& mLevels;
    const std::vector<LevelInfo>& mInfos;
    int mCurrentLevelIndex   = 0;
    int mCurrentProfileIndex = 0;
    int mCurrentRowIndex  = 1;
private:
    float mTotalTimeRun = 0.0f;
private:
    bool mPlaySounds = true;
    bool mMasterUnlock = false;

};

class GameWidget::GameHelp : public GameWidget::State
{
public:
    virtual void update(float dt)
    {}

    virtual Action mapAction(const wdk::WindowEventKeydown& key) const override
    {
        if (key.symbol == wdk::Keysym::Escape)
            return Action::CloseState;
        return Action::None;
    }
    virtual void keyPress(const wdk::WindowEventKeydown&) override
    {}

    virtual void paint(gfx::Painter& painter, const IRect& rect) const override
    {
        const GridLayout layout(rect, 1, 20);

        gfx::DrawTextRect(painter,
            base::FormatString(
                "Kill the invaders by typing the correct pinyin.\n"
                "You get scored based on how fast you kill and\n"
                "how complicated the characters are.\n\n"
                "Invaders that approach the left edge will show\n"
                "the pinyin string and score no points.\n"
                "You will lose points for invaders that you faill to kill.\n"
                "Score %1% or higher to unlock the next level.\n\n"
                "Type BOMB to ignite a bomb.\n"
                "Type WARP to enter a time warp.\n"
                "Press Space to clear the input.\n\n"
                "Press Esc to exit\n", (int)(LevelUnlockCriteria * 100)),
            "fonts/ARCADE.TTF", layout.GetFontSize(),
            layout.GetGfxRect(),
            gfx::Color::White);
    }
private:
};

class GameWidget::Settings : public GameWidget::State
{
public:
    std::function<void (bool)> onToggleFullscreen;
    std::function<void (bool)> onTogglePlayMusic;
    std::function<void (bool)> onTogglePlaySounds;

    Settings(bool music, bool sounds, bool fullscreen)
      : mPlayMusic(music)
      , mPlaySounds(sounds)
      , mFullscreen(fullscreen)
    {}

    virtual void paint(gfx::Painter& painter, const IRect& rect) const override
    {
        const GridLayout layout(rect, 1, 7);

        const auto font_size = layout.GetFontSize() * 0.3;

        gfx::DrawTextRect(painter,
            "Press space to toggle a setting.",
            "fonts/ARCADE.TTF", font_size,
            layout.MapGfxRect(IPoint(0, 1), IPoint(1, 2)),
            gfx::Color::White);
#ifdef GAME_ENABLE_AUDIO
        gfx::DrawTextRect(painter,
            base::FormatString("Sounds Effects: %1", mPlaySounds ? "On" : "Off"),
            "fonts/ARCADE.TTF", font_size,
            layout.MapGfxRect(IPoint(0, 2), IPoint(1, 3)),
            mSettingIndex == 0 ? gfx::Color::Green : gfx::Color::White);
        gfx::DrawTextRect(painter,
            base::FormatString("Awesome Music: %1", mPlayMusic ? "On" : "Off"),
            "fonts/ARCADE.TTF", font_size,
            layout.MapGfxRect(IPoint(0, 3), IPoint(1, 4)),
            mSettingIndex == 1 ? gfx::Color::Green : gfx::Color::White);
#else
        gfx::DrawTextRect(painter,
            "Audio is not supported on this platform."
            "fonts/ARCADE.TTF", font_size,
            layout.MapGfxRect(IPoint(0, 2), IPoint(1, 4)),
            gfx::Color::DarkGray);
#endif
        gfx::DrawTextRect(painter,
            base::FormatString("Fullscreen: %1", mFullscreen ? "On" : "Off"),
            "fonts/ARCADE.TTF", font_size,
            layout.MapGfxRect(IPoint(0, 4), IPoint(1, 5)),
            mSettingIndex == 2 ? gfx::Color::Green : gfx::Color::White);

        gfx::DrawTextRect(painter,
            "Press Esc to exit",
            "fonts/ARCADE.TTF", font_size,
            layout.MapGfxRect(IPoint(0, 5), IPoint(1, 6)),
            gfx::Color::White);

    }

    virtual Action mapAction(const wdk::WindowEventKeydown& key) const override
    {
        if (key.symbol == wdk::Keysym::Escape)
            return Action::CloseState;
        return Action::None;
    }

    virtual void keyPress(const wdk::WindowEventKeydown& key) override
    {
        const auto sym = key.symbol;
        if (sym == wdk::Keysym::Space)
        {
            if (mSettingIndex == 0)
            {
                mPlaySounds = !mPlaySounds;
                onTogglePlaySounds(mPlaySounds);
            }
            else if (mSettingIndex == 1)
            {
                mPlayMusic = !mPlayMusic;
                onTogglePlayMusic(mPlayMusic);
            }
            else if (mSettingIndex == 2)
            {
                mFullscreen = !mFullscreen;
                onToggleFullscreen(mFullscreen);
            }
        }
        else if (sym == wdk::Keysym::ArrowUp)
        {
            if (--mSettingIndex < 0)
                mSettingIndex = 2;
        }
        else if (sym == wdk::Keysym::ArrowDown)
        {
            mSettingIndex = (mSettingIndex + 1) % 3;
        }
    }
private:
    bool mPlayMusic  = false;
    bool mPlaySounds = false;
    bool mFullscreen = false;
private:
    int mSettingIndex = 0;
};

class GameWidget::About : public State
{
public:
    virtual void paint(gfx::Painter& painter, const IRect& rect) const override
    {
        const GridLayout layout(rect, 1, 20);

        gfx::DrawTextRect(painter,
            base::FormatString(
                "Pinyin-Invaders %1.%2\n\n"
                "Design and programming by:\n"
                "Sami Vaisanen\n"
                "(c) 2014-2019 Ensisoft\n"
                "http://www.ensisoft.com\n"
                "http://www.github.com/ensisoft/pinyin-invaders\n\n"
                "Graphics by:\n"
                "Tatermand, Gamedevtuts, Kenney\n"
                "http://www.opengameart.org\n"
                "http://www.kenney.nl\n\n"
                "Music by:\n"
                "level27\n"
                "http://soundcloud.com/level27\n\n"
                "Press Esc to exit", MAJOR_VERSION, MINOR_VERSION),
            "fonts/ARCADE.TTF", layout.GetFontSize(),
            layout.GetGfxRect(),
            gfx::Color::White);
    }
    virtual Action mapAction(const wdk::WindowEventKeydown& key) const override
    {
        if (key.symbol == wdk::Keysym::Escape)
            return Action::CloseState;
        return Action::None;
    }
private:
};

class GameWidget::PlayGame : public GameWidget::State
{
public:
    PlayGame(const Game::Setup& setup, Level& level, Game& game) : mSetup(setup), mLevel(level), mGame(game)
    {}

    virtual void paint(gfx::Painter& painter, const IRect& rect) const override
    {
        switch (mState)
        {
            case GameState::Prepare:
                paintFleet(painter, rect);
                break;
            case GameState::Playing:
                paintHUD(painter, rect);
                break;
        }
    }

    virtual Action mapAction(const wdk::WindowEventKeydown& key) const override
    {
        const auto sym = key.symbol;
        if (sym == wdk::Keysym::Escape)
            return Action::CloseState;

        switch (mState)
        {
            case GameState::Prepare:
                break;

            case GameState::Playing:
                if (sym == wdk::Keysym::F1)
                    return Action::OpenHelp;
                else if (sym == wdk::Keysym::F2)
                    return Action::OpenSettings;
                break;
        }
        return Action::None;
    }
    virtual void keyPress(const wdk::WindowEventKeydown& key) override
    {
        const auto sym = key.symbol;

        if (mState == GameState::Prepare)
        {
            if (sym == wdk::Keysym::Space)
            {
                std::srand(0x7f6a4b);
                mLevel.Reset();
                mGame.Play(&mLevel, mSetup);
                mState = GameState::Playing;
            }
        }
        else if (mState == GameState::Playing)
        {
            if (sym == wdk::Keysym::Backspace)
            {
                if (!mCurrentText.empty())
                    mCurrentText.pop_back();
            }
            else if (sym == wdk::Keysym::Space)
            {
                mCurrentText.clear();
            }
            else if (sym >= wdk::Keysym::KeyA && sym <= wdk::Keysym::KeyZ) // todo: should use character event
            {
                mCurrentText.append(base::Widen(wdk::ToString(sym)));
                if (mCurrentText == L"BOMB")
                {
                    Game::Bomb bomb;
                    mGame.IgniteBomb(bomb);
                    mCurrentText.clear();
                }
                else if (mCurrentText == L"WARP")
                {
                    Game::Timewarp warp;
                    warp.duration = 4000;
                    warp.factor = 0.2f;
                    mGame.EnterTimewarp(warp);
                    mCurrentText.clear();
                }
                else
                {
                    Game::Missile missile;
                    // we can directly map the launch position into game space
                    // i.e. it's in the middle of the bottom row
                    missile.launch_position_x = 0.5;
                    missile.launch_position_y = 1.0f;
                    missile.string  = base::ToLower(mCurrentText);
                    if (mGame.FireMissile(missile))
                        mCurrentText.clear();
                }
            }
        }
    }
    virtual bool isGameRunning() const override
    {
        return mState == GameState::Playing;
    }

private:
    void paintFleet(gfx::Painter& painter, const IRect& rect) const
    {
        const auto& enemies = mLevel.GetEnemies();
        const auto cols = 3;
        const auto rows = (enemies.size() / cols) + 3;
        const GridLayout layout(rect, cols, rows);

        const auto font_size_s = layout.GetFontSize() * 0.15;
        const auto font_size_l = layout.GetFontSize() * 0.2;
        const auto header = layout.MapGfxRect(IPoint(0, 0), IPoint(cols, 1));
        const auto footer = layout.MapGfxRect(IPoint(0, rows-1), IPoint(cols, rows));

        gfx::DrawTextRect(painter,
            "Kill the following enemies\n",
            "fonts/ARCADE.TTF", font_size_l,
            header,
            gfx::Color::White);
        gfx::DrawTextRect(painter,
            "Press Space to play!",
            "fonts/ARCADE.TTF", font_size_l,
            footer,
            gfx::Color::White,
            gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter,
            gfx::TextProp::Blinking);

        for (size_t i=0; i<enemies.size(); ++i)
        {
            const auto& e = enemies[i];
            const auto col = i % cols;
            const auto row = i / cols;
            const auto& rect = layout.MapGfxRect(IPoint(col, row + 1), IPoint(col + 1, row + 2));
            gfx::DrawTextRect(painter,
                base::FormatString("%1 %2", e.viewstring, e.killstring),
                "fonts/SourceHanSerifTC-SemiBold.otf", font_size_l,
                rect,
                gfx::Color::White,
                gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignTop);
            gfx::DrawTextRect(painter,
                base::ToUtf8(e.help),
                "fonts/ARCADE.TTF", font_size_s,
                rect,
                gfx::Color::White,
                gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter);
        }
    }

    void paintHUD(gfx::Painter& painter, const IRect& rect) const
    {
        const auto& score  = mGame.GetScore();
        const auto& result = score.maxpoints ?
            (float)score.points / (float)score.maxpoints * 100 : 0.0;
        const auto bombs   = mGame.GetNumBombs();
        const auto warps   = mGame.GetNumWarps();

        const auto& layout = GetGameWindowLayout(rect);
        const auto font_size = layout.GetFontSize() * 0.5;

        gfx::DrawTextRect(painter,
            base::FormatString("Score %1 (%2%) / Enemies x %3 / Bombs x %4 / Warps x %5 (F1 for Help)",
                score.points, (int)result, score.pending, bombs, warps),
            "fonts/ARCADE.TTF", font_size,
            layout.MapGfxRect(IPoint(0, -1), IPoint(GameCols, 0)),
            gfx::Color::White);
        gfx::DrawTextRect(painter,
            mCurrentText.empty() ? "Type the correct pinyin to kill the enemies!" : base::ToUtf8(mCurrentText),
            "fonts/ARCADE.TTF", font_size,
            layout.MapGfxRect(IPoint(0, GameRows), IPoint(GameCols, GameRows+1)),
            gfx::Color::White,
            gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter,
            mCurrentText.empty() ? gfx::TextProp::Blinking : 0);
    }
private:
    const Game::Setup mSetup;
    Level& mLevel;
    Game&  mGame;

    enum class GameState {
        Prepare,
        Playing
    };
    GameState mState = GameState::Prepare;
    std::wstring mCurrentText;
};


GameWidget::GameWidget(wdk::Window& window) : mWindow(window)
{
    mGame.reset(new Game(GameCols, GameRows));

    mGame->onMissileKill = [&](const Game::Invader& i, const Game::Missile& m, unsigned killScore)
    {
        auto it = mInvaders.find(i.identity);
        const auto w = mWindow.GetSurfaceWidth();
        const auto h = mWindow.GetSurfaceHeight();
        const auto layout = GetGameWindowLayout(w, h);
        const auto scale = layout.GetCellDimensions();

        std::unique_ptr<Invader> invader(it->second.release());

        // calculate position for the invader to be in at now + missile fly time
        // and then aim the missile to that position.
        const auto missileFlyTime   = 500;
        const auto explosionTime    = 1000;
        const auto missileEnd       = invader->getFuturePosition(missileFlyTime/1000.0f);
        const auto missileBeg       = glm::vec2(m.launch_position_x, m.launch_position_y);
        const auto missileDir       = missileEnd - missileBeg;

        std::unique_ptr<Animation> missile(new Missile(missileBeg, missileDir, base::ToUpper(m.string), missileFlyTime));
        std::unique_ptr<Explosion> explosion(new Explosion(missileEnd, missileFlyTime, explosionTime));
        std::unique_ptr<Smoke> smoke(new Smoke(missileEnd, missileFlyTime + 100, explosionTime + 500));
        std::unique_ptr<Debris> debris(new Debris(invader->getTextureName(), missileEnd, missileFlyTime, explosionTime + 500));
        std::unique_ptr<Sparks> sparks(new Sparks(missileEnd, missileFlyTime, explosionTime));
        std::unique_ptr<Animation> score(new Score(missileEnd, explosionTime, 2000, killScore));

        invader->setMaxLifetime(missileFlyTime);
        explosion->setScale(invader->getScale() * 1.5);
        smoke->setScale(invader->getScale() * 2.5);
        sparks->setColor(gfx::Color4f(255, 255, 68, 180));
        debris->setTextureScaleFromWidth(scale.GetX());

        mAnimations.push_back(std::move(invader));
        mAnimations.push_back(std::move(missile));
        mAnimations.push_back(std::move(smoke));
        mAnimations.push_back(std::move(debris));
        mAnimations.push_back(std::move(sparks));
        mAnimations.push_back(std::move(explosion));
        mAnimations.push_back(std::move(score));

        mInvaders.erase(it);

#ifdef GAME_ENABLE_AUDIO
        if (mPlaySounds)
        {
            // sound effects FX
            auto sndExplosion = std::make_unique<audio::AudioFile>("sounds/explode.wav", "explosion");
            g_audio->Play(std::move(sndExplosion), std::chrono::milliseconds(missileFlyTime));
        }
#endif
    };

    mGame->onMissileDamage = [&](const Game::Invader& i, const Game::Missile& m)
    {
        auto it = mInvaders.find(i.identity);
        auto& inv = it->second;

        const auto missileFlyTime  = 500;
        const auto missileEnd      = inv->getFuturePosition(missileFlyTime/1000.0f);
        const auto missileBeg      = glm::vec2(m.launch_position_x, m.launch_position_y);
        const auto missileDir      = missileEnd - missileBeg;

        std::unique_ptr<Animation> missile(new Missile(missileBeg, missileDir, base::ToUpper(m.string), missileFlyTime));
        std::unique_ptr<Sparks> sparks(new Sparks(missileEnd, missileFlyTime, 500));

        sparks->setColor(gfx::Color::DarkGray);

        std::wstring viewstring;
        for (const auto& s : i.viewList)
            viewstring += s;

        inv->setViewString(viewstring);

        mAnimations.push_back(std::move(missile));
        mAnimations.push_back(std::move(sparks));
    };

    mGame->onMissileFire = mGame->onMissileDamage;

    mGame->onBombKill = [&](const Game::Invader& i, const Game::Bomb& b, unsigned killScore)
    {
        auto it = mInvaders.find(i.identity);
        auto& inv = it->second;

        std::unique_ptr<Animation> explosion(new Explosion(inv->getPosition(), 0, 1000));
        std::unique_ptr<Animation> score(new Score(inv->getPosition(), 1000, 2000, killScore));
        mAnimations.push_back(std::move(explosion));
        mAnimations.push_back(std::move(score));

        mInvaders.erase(it);
    };

    mGame->onBombDamage = [&](const Game::Invader& i, const Game::Bomb& b)
    {
        auto it = mInvaders.find(i.identity);
        auto& inv = it->second;
        std::wstring viewstring;
        for (const auto& s: i.viewList)
            viewstring += s;
        inv->setViewString(viewstring);
    };

    mGame->onBomb = [&](const Game::Bomb& b)
    {
        std::unique_ptr<Animation> explosion(new BigExplosion(1500));


        mAnimations.push_back(std::move(explosion));
    };

    mGame->onWarp = [&](const Game::Timewarp& w)
    {
        DEBUG("begin time warp");
        mWarpFactor    = w.factor;
        mWarpRemaining = w.duration;
    };

    mGame->onToggleShield = [&](const Game::Invader& i, bool onOff)
    {
        auto it = mInvaders.find(i.identity);
        auto& inv = it->second;
        inv->enableShield(onOff);
    };

    mGame->onInvaderSpawn = [&](const Game::Invader& inv)
    {
        Invader::ShipType type = Invader::ShipType::Slow;
        if (inv.type == Game::InvaderType::Boss)
        {
            type = Invader::ShipType::Boss;
        }
        else
        {
            if (inv.speed == 1)
            {
                if (inv.killList.size() == 1)
                {
                    type = Invader::ShipType::Slow;
                }
                else
                {
                    type = Invader::ShipType::Fast;
                }
            }
            else
            {
                type = Invader::ShipType::Tough;
            }
        }

        // where's the invader ?
        // transform the position into normalized coordinates
        const float x = inv.xpos / (float)GameCols;
        const float y = inv.ypos / (float)GameRows;

        // the game expresses invader speed as the number of discrete steps it takes
        // per each tick of game.
        // here well want to express this velocity as a normalized distance over seconds
        const auto tick_length_secs = 1000.0 / mProfiles[mCurrentProfile].speed;
        const auto journey_duration_in_ticks = GameCols / (double)inv.speed;
        const auto journey_duration_in_secs  = tick_length_secs * journey_duration_in_ticks;
        const auto velocity  = 1.0f / journey_duration_in_secs;

        std::wstring viewstring;
        for (const auto& s : inv.viewList)
            viewstring += s;

        std::unique_ptr<Invader> invader(new Invader(glm::vec2(x, y), viewstring, velocity, type));
        invader->enableShield(inv.shield_on_ticks != 0);
        mInvaders[inv.identity] = std::move(invader);
    };


    mGame->onInvaderVictory = [&](const Game::Invader& inv)
    {
        mInvaders.erase(inv.identity);
    };

    // invader is almost escaping unharmed. we help the player to learn
    // by changing the text from chinese to the pinyin kill string
    mGame->onInvaderWarning = [&](const Game::Invader& inv)
    {
        auto it  = mInvaders.find(inv.identity);
        std::wstring killstr;
        for (const auto& s : inv.killList)
            killstr += s;

        it->second->setViewString(killstr);
    };

    mGame->onLevelComplete = [&](const Game::Score& score)
    {
        DEBUG("Level complete %1 / %2 points (points / max)",  score.points, score.maxpoints);

        auto& info    = mLevelInfos[mCurrentLevel];
        auto& profile = mProfiles[mCurrentProfile];

        const auto base    = score.points;
        const auto bonus   = profile.speed * score.points;
        const auto final   = score.points + bonus;
        const bool hiscore = final > info.highScore;
        info.highScore = std::max(info.highScore, (unsigned)final);

        unsigned unlockLevel = 0;
        if ((base / (float)score.maxpoints) >= LevelUnlockCriteria)
        {
            if (mCurrentLevel < mLevels.size() - 1)
            {
                if (mLevelInfos[mCurrentLevel + 1].locked)
                {
                    unlockLevel = mCurrentLevel + 1;
                    mLevelInfos[unlockLevel].locked = false;
                }
            }
        }
        auto scoreboard = std::make_unique<Scoreboard>(base, bonus, hiscore, unlockLevel);
        mStates.pop();
        mStates.push(std::move(scoreboard));
    };

    // in this space all the background objects travel to the same direction

    for (size_t i=0; i<20; ++i)
    {
        const glm::vec2 spaceJunkDirection(-1, 0);
        mAnimations.emplace_back(new Asteroid(glm::normalize(spaceJunkDirection)));
    }

    // initialize the input/state stack with the main menu.
    auto menu = std::make_unique<MainMenu>(mLevels, mLevelInfos, true);
    mStates.push(std::move(menu));
}

GameWidget::~GameWidget() = default;

void GameWidget::initArgs(int argc, char* argv[])
{
    for (int i=1; i<argc; ++i)
    {
        const std::string a(argv[i]);
        if (a == "--unlock-all")
            mMasterUnlock = true;
        else if (a == "--unlimited-warps")
            mUnlimitedWarps = true;
        else if (a == "--unlimited-bombs")
            mUnlimitedBombs = true;
        else if (a == "--show-fps")
            mShowFps = true;
    }
}

void GameWidget::load(const game::Settings& settings)
{
    const auto width      = settings.GetValue("window", "width", 1200);
    const auto height     = settings.GetValue("window", "height", 700);
    const auto fullscreen = settings.GetValue("window", "fullscreen", false);
    const auto play_sound = settings.GetValue("audio", "sound", true);
    const auto play_music = settings.GetValue("audio", "music", true);
    const auto& levels    = settings.GetValue("game", "levels", std::vector<std::string>());

    // load the immutable level data.
    mLevels = Level::LoadLevels(L"data/levels.txt");

    for (const auto& level : mLevels)
    {
        LevelInfo info;
        info.highScore = 0;
        info.name      = level->GetName();
        info.locked    = true;
        mLevelInfos.push_back(info);
        if (!level->Validate())
            throw std::runtime_error(base::FormatString("Broken level detected: '%s'", level->GetName()));
    }
    // always make sure that the first level is unlocked so it's possible to play.
    mLevelInfos[0].locked = false;

    // load the saved level data, highscores and such
    for (int i=0; i<levels.size(); ++i)
    {
        const auto& level_name = levels[i];
        LevelInfo info;
        info.name      = base::FromUtf8(level_name);
        info.highScore = settings.GetValue(level_name, "highscore", 0);
        info.locked    = settings.GetValue(level_name, "locked", true);

        for (auto& i : mLevelInfos)
        {
            if (i.name != info.name)
                continue;
            i = info;
            break;
        }
    }

    // setup the game difficulty/game play profiles
    const Profile EASY    {L"Easy",    1.6f, 2, 7, 30};
    const Profile MEDIUM  {L"Medium",  1.8f, 2, 4, 35};
    const Profile CHINESE {L"Chinese", 2.0f, 2, 4, 40};
    mProfiles.push_back(EASY);
    mProfiles.push_back(MEDIUM);
    mProfiles.push_back(CHINESE);

    // adjust window based on settings.
    if (fullscreen)
    {
        mWindow.SetFullscreen(true);
    }
    else
    {
        const auto aspect = (float)GameRows / (float)GameCols;
        const auto surface_width = width != 0
            ? width : GameCols * 20;
        const auto surface_height = height != 0
            ? height : surface_width * aspect;
        mWindow.SetSize(surface_width, surface_height);
    }

    mPlayMusic  = play_music;
    mPlaySounds = play_sound;
}

void GameWidget::save(game::Settings& settings)
{
    settings.SetValue("window", "width",  mWindow.GetSurfaceWidth());
    settings.SetValue("window", "height", mWindow.GetSurfaceHeight());
    settings.SetValue("window", "fullscreen", mWindow.IsFullscreen());
    settings.SetValue("audio", "sound", mPlaySounds);
    settings.SetValue("audio", "music", mPlayMusic);

    std::vector<std::string> level_names;

    // save level data, highscores and such
    for (const auto& info : mLevelInfos)
    {
        const auto& level_name = base::ToUtf8(info.name);
        level_names.push_back(level_name);
        settings.SetValue(level_name, "highscore", info.highScore);
        settings.SetValue(level_name, "locked", info.locked);
    }
    settings.SetValue("game", "levels", level_names);
}

void GameWidget::launch()
{
    mStates.top()->setPlaySounds(mPlaySounds);
    mStates.top()->setMasterUnlock(mMasterUnlock);
    playMusic();
}

void GameWidget::updateGame(float dt)
{

#ifdef GAME_ENABLE_AUDIO
    // handle audio events.
    audio::AudioPlayer::TrackEvent event;
    while (g_audio->GetEvent(&event))
    {
        DEBUG("Audio event (%1)", event.id);
        if (event.id != mMusicTrackId)
            continue;
        mMusicTrackId = 0;
        mMusicTrackIndex++;
        playMusic();
    }

#endif

    const auto time = dt * mWarpFactor;
    const auto tick = 1000.0 / mProfiles[mCurrentProfile].speed;

    if (UFO::shouldMakeRandomAppearance())
    {
        mAnimations.emplace_back(new UFO);
    }

    auto* background = g_loader->FindAnimation("Space");
    background->Update(time/1000.0f);

    mStates.top()->update(time);

    const bool bGameIsRunning = mStates.top()->isGameRunning();

    if (bGameIsRunning)
    {
        mTickDelta += (time);
        if (mTickDelta >= tick)
        {
            // advance game by one tick
            mGame->Tick();

            mTickDelta = mTickDelta - tick;
        }
        // update invaders
        for (auto& pair : mInvaders)
        {
            auto& invader = pair.second;
            invader->update(time);
        }
    }

    // update animations
    for (auto it = std::begin(mAnimations); it != std::end(mAnimations);)
    {
        auto& anim = *it;
        if (!anim->update(time))
        {
            it = mAnimations.erase(it);
            continue;
        }
        ++it;
    }

    // do some simple collision resolution.
    using CollisionType = base::bitflag<Animation::ColliderType>;
    static const CollisionType Asteroid_UFO_Collision =
        { Animation::ColliderType::UFO, Animation::ColliderType::Asteroid };
    static const CollisionType UFO_UFO_Collision =
        { Animation::ColliderType::UFO, Animation::ColliderType::UFO };

    const auto w = mWindow.GetSurfaceWidth();
    const auto h = mWindow.GetSurfaceHeight();
    const IRect rect(0, 0, w , h);

    for (auto it = std::begin(mAnimations); it != std::end(mAnimations);)
    {
        auto& lhsAnim = *it;
        const auto lhsColliderType = lhsAnim->getColliderType();
        if (lhsColliderType == Animation::ColliderType::None)
        {
            ++it;
            continue;
        }

        auto other = std::find_if(std::begin(mAnimations), std::end(mAnimations),
            [&](auto& animation) {
                const auto type = animation->getColliderType();
                if (type == Animation::ColliderType::None)
                    return false;
                if (animation == lhsAnim)
                    return false;
                const auto collision = CollisionType {lhsColliderType, type };
                if (collision == Asteroid_UFO_Collision || collision == UFO_UFO_Collision)
                {
                    const auto& lhsBounds = lhsAnim->getBounds(rect);
                    const auto& rhsBounds = animation->getBounds(rect);
                    const auto& intersect = Intersect(lhsBounds, rhsBounds);
                    if (!intersect.IsEmpty())
                        return true;
                }
                return false;
            });
        if (other == std::end(mAnimations))
        {
            ++it;
            continue;
        }
        auto& rhsAnim = *other;
        const auto rhsColliderType = rhsAnim->getColliderType();
        const auto collision = CollisionType { lhsColliderType, rhsColliderType };
        if (collision == Asteroid_UFO_Collision)
        {
            DEBUG("UFO - Asteroid collision!");
            const auto* UFO = CollisionCast<const GameWidget::UFO>(lhsAnim, rhsAnim);
            const auto position = UFO->getPosition();
            const auto startNow = 0.0f;
            const auto lifetime = 1000.0f;

            std::unique_ptr<Explosion> explosion(new Explosion(position, startNow, lifetime));
            std::unique_ptr<Debris> debris(new Debris(UFO->getTextureName(),
                position, startNow, lifetime + 500));
            explosion->setScale(3.0f);
            mAnimations.push_back(std::move(debris));
            mAnimations.push_back(std::move(explosion));

            if (lhsColliderType == Animation::ColliderType::UFO)
                it = mAnimations.erase(it);
            else mAnimations.erase(other);
            continue;
        }
        else if (collision == UFO_UFO_Collision)
        {
            DEBUG("UFO - UFO collision!");
            dynamic_cast<UFO*>(lhsAnim.get())->invertDirection();
            dynamic_cast<UFO*>(rhsAnim.get())->invertDirection();
        }
        ++it;
    }

    if (mWarpRemaining)
    {
        if (time >= mWarpRemaining)
        {
            mWarpFactor    = 1.0;
            mWarpRemaining = 0;
            DEBUG("Warp ended");
        }
        else
        {
            mWarpRemaining -= dt;
        }
    }
}

void GameWidget::renderGame(gfx::Device& device, gfx::Painter& painter)
{
    // implement simple painter's algorithm here
    // i.e. paint the game scene from back to front.
    const auto w = mWindow.GetSurfaceWidth();
    const auto h = mWindow.GetSurfaceHeight();
    const IRect rect(0, 0, w , h);

    device.BeginFrame();
    painter.SetViewport(0, 0, w, h);
    device.ClearColor(gfx::Color::Black);

    // paint the background
    {
        const auto* anim = g_loader->FindAnimation("Space");
        const auto* node = anim->FindNodeByName("Background");
        const auto& rect = anim->GetBoundingBox(node);
        gfx::Transform view;
        view.Scale(w / rect.GetWidth(), h / rect.GetHeight());
        anim->Draw(painter, view);
    }

    // then paint the animations on top of the background
    for (auto& anim : mAnimations)
    {
        anim->paint(painter, rect);
    }

    const bool bIsGameRunning = mStates.top()->isGameRunning();
    // paint the invaders if the game is running, need to check whether
    // the game is running or not because it could be paused
    // because the player is looking at the settings/help

    if (bIsGameRunning)
    {
        for (auto& pair : mInvaders)
        {
            auto& invader = pair.second;
            invader->paint(painter, rect);
        }
    }

    // finally paint the menu/HUD
    mStates.top()->paint(painter, rect);

    if (mShowFps)
    {
        gfx::DrawTextRect(painter,
            base::FormatString("FPS: %1", mCurrentfps),
            "fonts/ARCADE.TTF", 28,
            gfx::FRect(10, 20, 150, 100),
            gfx::Color::DarkRed,
            gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignTop);
    }

    device.EndFrame();
    device.CleanGarbage(120);
}

void GameWidget::OnKeydown(const wdk::WindowEventKeydown& key)
{
    const auto sym = key.symbol;
    const auto mod = key.modifiers;
    if (sym == wdk::Keysym::KeyR && mod.test(wdk::Keymod::Shift))
    {
        DEBUG("Recompile shaders");
        //mCustomGraphicsDevice->DeleteShaders();
        //mCustomGraphicsDevice->DeletePrograms();
        return;
    }
    else if (sym == wdk::Keysym::KeyN && mod.test(wdk::Keymod::Shift))
    {
        DEBUG("Next music track");
        if (mPlayMusic)
        {
        #ifdef GAME_ENABLE_AUDIO
            g_audio->Cancel(mMusicTrackId);
            mMusicTrackId = 0;
            mMusicTrackIndex++;
            playMusic();
        #endif
        }
        return;
    }

    const auto action = mStates.top()->mapAction(key);
    switch (action)
    {
        case State::Action::None:
            mStates.top()->keyPress(key);
            break;
        case State::Action::OpenHelp:
            mStates.push(std::make_unique<GameHelp>());
            break;
        case State::Action::OpenSettings:
            {
                auto settings = std::make_unique<Settings>(mPlayMusic, mPlaySounds,
                    mWindow.IsFullscreen());
                settings->onToggleFullscreen = [this](bool fullscreen) {
                    mWindow.SetFullscreen(fullscreen);
                };
                settings->onTogglePlayMusic = [this](bool play) {
                    mPlayMusic = play;
                    playMusic();
                };
                settings->onTogglePlaySounds = [this](bool play) {
                    mPlaySounds = play;
                };
                mStates.push(std::move(settings));
            }
            break;
        case State::Action::OpenAbout:
            mStates.push(std::make_unique<About>());
            break;
        case State::Action::QuitApp:
            mRunning = false;
            break;
        case State::Action::NewGame:
            {
                // todo: packge the parameters with the return action
                // and get rid of this casting here.
                const auto* mainMenu = dynamic_cast<const MainMenu*>(mStates.top().get());
                const auto levelIndex   = mainMenu->getLevelIndex();
                const auto profileIndex = mainMenu->getProfileIndex();

                ASSERT(mLevels.size() == mLevelInfos.size());
                ASSERT(levelIndex < mLevels.size());
                ASSERT(profileIndex < mProfiles.size());

                const auto& profile = mProfiles[profileIndex];
                const auto& level   = mLevels[levelIndex];
                DEBUG("Start game: %1 / %2", level->GetName(), profile.name);

                Game::Setup setup;
                setup.numEnemies    = profile.numEnemies;
                setup.spawnCount    = profile.spawnCount;
                setup.spawnInterval = profile.spawnInterval;
                setup.numBombs      = mUnlimitedBombs ? std::numeric_limits<unsigned>::max() : 2;
                setup.numWarps      = mUnlimitedWarps ? std::numeric_limits<unsigned>::max() : 2;
                auto playing = std::make_unique<PlayGame>(setup, *mLevels[levelIndex], *mGame);
                mStates.push(std::move(playing));

                mCurrentLevel   = levelIndex;
                mCurrentProfile = profileIndex;
                mTickDelta  = 0;
                mWarpFactor = 1.0;
                mWarpRemaining = 0.0f;
            }
            break;
        case State::Action::CloseState:
            {
                const bool bIsGameRunning = mStates.top()->isGameRunning();
                if (bIsGameRunning)
                {
                    mGame->Quit();
                    mInvaders.clear();
                    mAnimations.clear();
                }
                mStates.pop();
                mStates.top()->setPlaySounds(mPlaySounds);
                mStates.top()->setMasterUnlock(mMasterUnlock);
            }
            break;
    }
}

void GameWidget::OnWantClose(const wdk::WindowEventWantClose& close)
{
    mRunning = false;
}

void GameWidget::playMusic()
{
#ifdef GAME_ENABLE_AUDIO
    static const char* tracks[] = {
        "music/awake10_megaWall.ogg"
    };

    if (mPlayMusic)
    {
        if (mMusicTrackId)
        {
            DEBUG("Resume music");
             g_audio->Resume(mMusicTrackId);
        }
        else
        {
            const auto num_tracks  = sizeof(tracks) / sizeof(tracks[0]);
            const auto track_index = mMusicTrackIndex % num_tracks;
            DEBUG("Play music track: %1, '%2'", track_index, tracks[track_index]);
            auto music = std::make_unique<audio::AudioFile>(tracks[track_index], "MainMusic");
            mMusicTrackId = g_audio->Play(std::move(music));
        }
    }
    else
    {
        if (mMusicTrackId)
        {
            DEBUG("Stop music");
            g_audio->Pause(mMusicTrackId);
        }
    }
#endif
}

} // invaders
