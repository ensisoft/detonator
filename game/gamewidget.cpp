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
#  include <QtGui/QPaintEvent>
#  include <QtGui/QKeyEvent>
#  include <QtGui/QPen>
#  include <QtGui/QBrush>
#  include <QtGui/QColor>
#  include <QtGui/QPainter>
#  include <QtGui/QFontDatabase>
#  include <QtGui/QCursor>
#  include <QtGui/QVector2D>
#  include <QtGui/QPixmap>
#  include <QtGui/QFont>
#  include <QtGui/QFontMetrics>
#  include <QApplication>
#  include <QResource>
#  include <QFileInfo>
#  include <QtDebug>
#include "warnpop.h"

#include <cmath>
#include <ctime>

#include "audio/sample.h"
#include "audio/player.h"
#include "base/bitflag.h"
#include "base/logging.h"
#include "base/math.h"
#include "graphics/device.h"
#include "graphics/painter.h"
#include "graphics/types.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "gamewidget.h"
#include "game.h"
#include "level.h"

namespace invaders
{

extern AudioPlayer* g_audio;

const auto LevelUnlockCriteria = 0.85;
const auto TextBlinkFrameCycle = 90;
const auto GameCols = 40;
const auto GameRows = 10;

// we divide the widget's client area into a equal sized cells
// according to the game's size. We also add one extra row
// for HUD display at the top of the screen and for the player
// at the bottom of the screen. This provides the basic layout
// for the game in a way that doesnt depend on any actual viewport size.
const auto ViewCols = GameCols;
const auto ViewRows = GameRows + 2;

QString R(const QString& s)
{
    QString resname = ":/dist/" + s;
    QResource resource(resname);
    if (resource.isValid())
        return resname;

    static const auto& inst = QApplication::applicationDirPath();
    return inst + "/" + s;
}


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

// game space is discrete space from 0 to game::width() - 1 on X axis
// and from 0 to game::height() - 1 on Y axis
// we use QPoint to represent this.
struct GameSpace {
    unsigned x, y;
};

// normalized widget/view space is expressed with floats from 0 to 1.0 on Y and X
// 0.0, 0.0 being the window top left and 1.0,1.0 being the window bottom right
// we use QVector2D to represent this

// eventually QPainter paint operations require coordinates in pixel space
// which runs from 0 to widget::width() on X axis and from 0 to widget::height()
// on the Y axis. this is also represented by QPoint

class TransformState
{
public:
    TransformState(const QRectF& window, float numCols, float numRows)
    {
        // divide the widget's client area int equal sized cells
        origin_ = QPointF(window.x(), window.y());
        widget_ = QPointF(window.width(), window.height());
        scale_  = QPointF(window.width() / numCols, window.height() / numRows);
        size_   = QPointF(numCols, numRows);
    }

    QRectF toViewSpaceRect(const QPointF& gameTopLeft, const QPointF& gameTopBottom) const
    {
        const QPointF top = toViewSpace(gameTopLeft);
        const QPointF bot = toViewSpace(gameTopBottom);
        return {top, bot};
    }

    QPointF toViewSpace(const QPointF& cell) const
    {
        const float xpos = cell.x() * scale_.x() + origin_.x();
        const float ypos = cell.y() * scale_.y() + origin_.y();
        return { xpos, ypos };
    }

    QPointF toViewSpace(const QVector2D& norm) const
    {
        const float xpos = widget_.x() * norm.x() + origin_.x();
        const float ypos = widget_.y() * norm.y() + origin_.y();
        return { xpos, ypos };
    }

    QVector2D toNormalizedViewSpace(const GameSpace& g) const
    {
        // we add 4 units on the gaming area on both sides to allow
        // the invaders to appear and disappear smoothly.

        const float cols = numCols() - 8.0;
        const float rows = numRows();

        const float p_scale_x = (widget_.x() - origin_.x()) / cols;
        const float p_scale_y = (widget_.y() - origin_.y()) / rows;

        const float x = g.x - 4.0;
        const float y = g.y;

        const float px = x * p_scale_x;
        const float py = y * p_scale_y;

        const float xpos = px / (widget_.x() - origin_.x());
        const float ypos = py / (widget_.y() - origin_.y());

        return {xpos, ypos};
    }

    QVector2D toNormalizedViewSpace(const QPoint& p) const
    {
        const float xpos = (float)p.x() / (widget_.x() - origin_.x());
        const float ypos = (float)p.y() / (widget_.y() - origin_.y());
        return {xpos, ypos};
    }

    QPointF getScale() const
    {
        return scale_;
    }

    QVector2D getNormalizedScale() const
    {
        const auto scale  = getScale();
        const auto width  = (float)viewWidth();
        const auto height = (float)viewHeight();
        return QVector2D(scale.x() / width, scale.y() / height);
    }

    // get whole widget rect in widget coordinates
    QRectF viewRect() const
    {
        return {origin_.x(), origin_.y(), widget_.x(), widget_.y()};
    }

    // get widget width
    int viewWidth() const
    {
        return widget_.x();
    }

    // get widget heigth
    int viewHeight() const
    {
        return widget_.y();
    }

    int numCols() const
    { return size_.x(); }

    int numRows() const
    { return size_.y(); }
private:
    QPointF origin_;
    QPointF widget_;
    QPointF scale_;
    QPointF size_;
};

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

    // paint the user interface state
    virtual void paint(QPainter& painter, const QRectF& area, const QPointF& unit) = 0;

    // paint the user interface state with the custom painter
    virtual void paintPostEffect(gfx::Painter& painter, const TransformState& transform) const
    {}

    // update the state.. umh.. state from the delta time
    virtual void update(float dt)
    {}

    // map keyboard input to an action.
    virtual Action mapAction(const QKeyEvent* press) const = 0;

    // handle the raw unmapped keyboard event
    virtual void keyPress(const QKeyEvent* press) = 0;

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
    virtual bool update(float dt, TransformState& state) = 0;

    virtual void paintPreEffect(gfx::Painter& painter, const TransformState& state)
    {}

    //
    virtual void paint(QPainter& painter, TransformState& state) = 0;

    virtual void paintPostEffect(gfx::Painter& painter, const TransformState& state)
    {}

    virtual QRectF getBounds(const TransformState& state) const
    {
        return QRectF{};
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
    Asteroid(const QVector2D& direction)
    {
        mX = math::rand(0.0f, 1.0f);
        mY = math::rand(0.0f, 1.0f);
        mVelocity  = 0.08 + math::rand(0.0, 0.08);
        mScale     = math::rand(0.2, 0.8);
        mTexture   = math::rand(0, 2);
        mDirection = direction;
    }
    virtual bool update(float dt, TransformState& state) override
    {
        const auto d = mDirection * mVelocity * (dt / 1000.0f);
        mX = math::wrap(1.0f, -0.2f, mX + d.x());
        mY = math::wrap(1.0f, -0.2f, mY + d.y());
        return true;
    }
    virtual void paintPreEffect(gfx::Painter& painter, const TransformState& state) override
    {
        const auto& size = getTextureSize(mTexture);
        const char* name = getTextureName(mTexture);

        gfx::Transform t;
        t.Resize(size * mScale);
        t.MoveTo(state.toViewSpace(QVector2D(mX, mY)));
        painter.Draw(gfx::Rect(), t, gfx::TextureFill(name).SetSurfaceType(gfx::Material::SurfaceType::Transparent));
    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {
    }
    virtual QRectF getBounds(const TransformState& state) const override
    {
        const auto& size = getTextureSize(mTexture);

        QRectF bounds;
        bounds.setSize(size * mScale);
        bounds.moveTo(state.toViewSpace(QVector2D(mX, mY)));
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
    static QSize getTextureSize(unsigned index)
    {
        static QSize sizes[] = {
            QSize(78, 74),
            QSize(74, 63),
            QSize(72, 58)
        };
        return sizes[index];
    }
private:
    float mVelocity = 0.0f;
    float mScale = 1.0f;
    float mX = 0.0f;
    float mY = 0.0f;
    QVector2D mDirection;
    unsigned mTexture = 0;
};

// Flame/Smoke emitter
class GameWidget::Explosion : public GameWidget::Animation
{
public:
    Explosion(const QVector2D& position, float start, float lifetime)
      : mPosition(position)
      , mStartTime(start)
      , mLifeTime(lifetime)
    {
        mSprite.SetTexture("textures/ExplosionMap.png");
        mSprite.SetFps(80.0 / (lifetime/1000.0f));

        // each explosion frame is 100x100 px
        // and there are 80 frames total.
        for (unsigned i=0; i<80; ++i)
        {
            const auto row = i / 10;
            const auto col = i % 10;
            const auto w = 100;
            const auto h = 100;
            gfx::SpriteMap::Frame frame;
            frame.x = col * w;
            frame.y = row * h;
            frame.w = w;
            frame.h = h;
            mSprite.AddFrame(frame);
        }
    }

    virtual bool update(float dt, TransformState& state) override
    {
        mTime += dt;
        if (mTime < mStartTime)
            return true;

        if (mTime - mStartTime > mLifeTime)
            return false;

        return true;
    }

    virtual void paintPreEffect(gfx::Painter& painter, const TransformState& state) override
    {
        if (mTime < mStartTime)
            return;

        mSprite.SetAppRuntime((mTime - mStartTime) / 1000.0f);

        const auto unitScale = state.getScale();
        const auto position  = state.toViewSpace(mPosition);
        const auto scaledWidth = unitScale.x() * mScale;
        const auto scaledHeight = unitScale.x() * mScale; // * aspect;

        gfx::Transform t;
        t.Resize(scaledWidth, scaledHeight);
        t.MoveTo(position - QPointF(scaledWidth / 2.0, scaledHeight / 2.0));
        painter.Draw(gfx::Rect(), t, mSprite);
    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {
    }

    void setScale(float scale)
    { mScale = scale; }

    QVector2D getPosition() const
    { return mPosition; }

private:
    const QVector2D mPosition;
    const float mStartTime = 0.0f;
    const float mLifeTime  = 0.0f;
    float mTime  = 0.0f;
    float mScale = 1.0f;
private:
    gfx::SpriteMap mSprite;
};


// "fire" sparks emitter, high velocity  (needs texturing)
class GameWidget::Sparks : public GameWidget::Animation
{
public:
    using ParticleEngine = gfx::TParticleEngine<
        gfx::LinearParticleMovement,
        gfx::KillParticleAtBounds>;

    Sparks(QVector2D position, float start, float lifetime)
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
        params.respawn = false;
        mParticles = std::make_unique<ParticleEngine>(params);
    }
    virtual bool update(float dt, TransformState&) override
    {
        mTimeAccum += dt;
        if (mTimeAccum < mStartTime)
            return true;

        if (mTimeAccum - mStartTime > mLifeTime)
            return false;

        mParticles->Update(dt / 1000.0f);
        return true;
    }
    virtual void paint(QPainter&, TransformState&) override
    {}

    virtual void paintPreEffect(gfx::Painter& painter, const TransformState& state) override
    {
        if (mTimeAccum < mStartTime)
            return;

        const auto pos = state.toViewSpace(mPosition);
        const auto x = pos.x();
        const auto y = pos.y();

        gfx::Transform t;
        t.Resize(500, 500);
        t.MoveTo(x-250, y-250);

        painter.Draw(*mParticles, t,
            gfx::TextureFill("textures/RoundParticle.png")
            .SetSurfaceType(gfx::Material::SurfaceType::Emissive)
            .SetRenderPoints(true)
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
    QVector2D mPosition;
    gfx::Color4f mColor;
};

class GameWidget::Smoke : public GameWidget::Animation
{
public:
    Smoke(const QVector2D& position, float start, float lifetime)
        : mPosition(position)
        , mStartTime(start)
        , mLifeTime(lifetime)
    {
        mSprite.SetFps(10);
        for (int i=0; i<=24; ++i)
        {
            const auto& name = base::FormatString("textures/smoke/blackSmoke%1.png",  i);
            mSprite.AddTexture(name);
        }
    }

    virtual bool update(float dt, TransformState&) override
    {
        mTime += dt;
        if (mTime < mStartTime)
            return true;

        if (mTime - mStartTime > mLifeTime)
            return false;

        return true;
    }
    virtual void paintPreEffect(gfx::Painter& painter, const TransformState& state) override
    {
        if (mTime < mStartTime)
            return;

        const auto time  = mTime - mStartTime;
        const auto alpha = 0.4 - 0.4 * (time / mLifeTime);
        mSprite.SetAppRuntime(time / 1000.0f);
        mSprite.SetBaseColor(gfx::Color4f(1.0f, 1.0f, 1.0f, alpha));

        const auto unitScale = state.getScale();
        const auto pxw = unitScale.x() * mScale;
        const auto pxh = unitScale.x() * mScale;
        const auto pos = state.toViewSpace(mPosition);

        gfx::Transform t;
        t.MoveTo(pos - QPointF(pxw/2.0f, pxh/2.0f));
        t.Resize(pxw, pxh);
        painter.Draw(gfx::Rect(), t, mSprite);

    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {}

    void setScale(float scale)
    { mScale = scale; }

private:
    const QVector2D mPosition;
    const float mStartTime = 0.0f;
    const float mLifeTime  = 0.0f;
    float mTime  = 0.0f;
    float mScale = 1.0f;
private:
    gfx::SpriteSet mSprite;
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

    Debris(const QPixmap& texture, const QVector2D& position, float starttime, float lifetime)
        : mStartTime(starttime)
        , mLifeTime(lifetime)
        , mTexture(texture)
    {
        const auto particleWidth  = texture.width() / NumParticleCols;
        const auto particleHeight = texture.height() / NumParticleRows;
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
            p.rc  = QRect(x, y, particleWidth, particleHeight);
            p.dir.setX(std::cos(a));
            p.dir.setY(std::sin(a));
            p.dir *= v;
            p.pos = position;
            p.alpha = 1.0f;
            p.angle = (M_PI * 2 ) * (float)std::rand() / RAND_MAX;
            p.rotation_coefficient = math::rand(-1.0f, 1.0f);
            mParticles.push_back(p);
        }
    }
    virtual bool update(float dt, TransformState&) override
    {
        mTime += dt;
        if (mTime < mStartTime)
            return true;

        if  (mTime - mStartTime > mLifeTime)
            return false;

        for (auto& p : mParticles)
        {
            p.pos += p.dir * (dt / 4500.0);
            p.alpha = math::clamp(0.0, p.alpha - (dt / 3000.0), 1.0);
            p.angle += ((M_PI * 2) * (dt / 2000.0) * p.rotation_coefficient);
        }

        return true;
    }
    virtual void paint(QPainter& painter, TransformState& state) override
    {
        if (mTime < mStartTime)
            return;

        for (const auto& p : mParticles)
        {
            const auto pos = state.toViewSpace(p.pos);

            const float width  = p.rc.width();
            const float height = p.rc.height();
            const float aspect = height / width;
            const float scaledWidth  = width * mScale;
            const float scaledHeight = scaledWidth * aspect;

            QRect target(pos.x(), pos.y(), scaledWidth, scaledHeight);

            QTransform rotation;
            rotation.translate(pos.x() + scaledWidth / 2, pos.y() + scaledHeight / 2);
            rotation.rotateRadians(p.angle, Qt::ZAxis);
            rotation.translate(-pos.x() - scaledWidth / 2, -pos.y() - scaledHeight / 2);
            painter.setTransform(rotation);
            painter.setOpacity(p.alpha);
            painter.drawPixmap(target, mTexture, p.rc);
        }
        painter.resetTransform();
        painter.setOpacity(1.0);
    }

    void setTextureScaleFromWidth(float width)
    {
        const auto particleWidth = (mTexture.width() / NumParticleCols);
        mScale = width / particleWidth;
    }

    void setTextureScale(float scale)
    { mScale = scale; }

private:
    struct particle {
        QRect rc;
        QVector2D dir;
        QVector2D pos;
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
    QPixmap mTexture;

};

class GameWidget::Invader : public GameWidget::Animation
{
public:
    struct MyParticleUpdate {
        void BeginIteration(float dt, float time, const math::Vector2D& bounds) const
        {}
        bool Update(gfx::Particle& p, float dt, float time, const math::Vector2D& bounds) const {
            // decrease the size of the particle as it approaches the edge of the
            // particle space.
            const auto bx = bounds.X();
            const auto by = bounds.Y();
            const auto x  = p.pos.X();
            const auto y  = p.pos.Y();
            const auto vdm = std::abs(y - (by * 0.5f)); // vertical distance to middle of stream
            const auto fvdm = vdm / (by * 0.5f); // fractional vmd
            const auto hdr = bx * 0.7 + (bx * 0.3 * (1.0f - fvdm)); // horizontal distance required
            const auto fd  = x / hdr; // fractional distance
            p.pointsize = 40.0f - (fd * 40.0f);
            return true;
        }
        void EndIteration() {}
    };

    using ParticleEngine = gfx::TParticleEngine<
        gfx::LinearParticleMovement,
        gfx::KillParticleAtBounds,
        MyParticleUpdate,
        gfx::KillParticleAtLifetime>;

    enum class ShipType {
        Slow,
        Fast,
        Tough,
        Boss
    };

    Invader(QVector2D position, QString str, float velocity, ShipType type)
      : mPosition(position)
      , mText(str)
      , mVelocity(velocity)
      , mShipType(type)
    {
    }

    virtual bool update(float dt, TransformState& state) override
    {
        const auto v = getVelocityVector(state);
        mPosition += (v * dt);
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

    virtual void paintPreEffect(gfx::Painter& painter, const TransformState& state) override
    {
        // offset the texture to be centered around the position
        const auto unitScale   = state.getScale();
        const auto spriteScale = state.getScale() * getScale();
        const auto position    = state.toViewSpace(mPosition);

        QPixmap ship = getShipTexture(mShipType);
        const float shipWidth  = ship.width();
        const float shipHeight = ship.height();
        const float shipAspect = shipHeight / shipWidth;
        const float shipScaledWidth  = spriteScale.x();
        const float shipScaledHeight = shipScaledWidth * shipAspect;

        QPixmap jet = getJetStreamTexture(mShipType);
        const float jetWidth  = jet.width();
        const float jetHeight = jet.height();
        const float jetAspect = jetHeight / jetWidth;
        const float jetScaledWidth  = spriteScale.x();
        const float jetScaledHeight = jetScaledWidth * jetAspect;

        if (!mParticles)
        {
            ParticleEngine::Params params;
            params.init_rect_width  = 0.0f;
            params.init_rect_height = jetScaledHeight;
            params.max_xpos = jetScaledWidth;
            params.max_ypos = jetScaledHeight;
            params.num_particles = 30;
            params.min_velocity = 100.0f;
            params.max_velocity = 150.0f;
            params.min_point_size = 40.0f;
            params.max_point_size = 40.0f;
            params.direction_sector_start_angle = 0.0f;
            params.direction_sector_size = 0.0f;
            params.respawn = true;
            mParticles = std::make_unique<ParticleEngine>(params);
        }

        // set the target rectangle with the dimensions of the
        // sprite we want to draw.
        // the ship rect is the coordinate to which the jet stream and
        // the text are relative to.
        QRectF shipRect(0.0f, 0.0f, shipScaledWidth, shipScaledHeight);
        // offset it so that the center is aligned with the unit position
        shipRect.moveTo(position -
            QPointF(shipScaledWidth / 2.0, shipScaledHeight / 2.0));

        // have to do a little fudge here since the scarab ship has
        // a contour such that positioning the particle engine just behind
        // the ship texture will leave a silly gap between the ship and the
        // particles.
        const auto fudgeFactor = mShipType == ShipType::Slow ? 0.8 : 1.0f;

        QRectF jetStreamRect = shipRect;
        jetStreamRect.translate(shipScaledWidth * fudgeFactor, (shipScaledHeight - jetScaledHeight) / 2.0);
        jetStreamRect.setSize(QSize(jetScaledWidth, jetScaledHeight));

        gfx::Transform t;
        t.MoveTo(jetStreamRect);
        t.Resize(jetStreamRect);

        painter.Draw(*mParticles, t,
            gfx::TextureFill("textures/RoundParticle.png")
            .SetSurfaceType(gfx::Material::SurfaceType::Transparent)
            .SetRenderPoints(true)
            .SetBaseColor(getJetStreamColor(mShipType)));
    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {
        // offset the texture to be centered around the position
        const auto unitScale   = state.getScale();
        const auto spriteScale = state.getScale() * getScale();
        const auto position    = state.toViewSpace(mPosition);

        QPixmap ship = getShipTexture(mShipType);
        const float shipWidth  = ship.width();
        const float shipHeight = ship.height();
        const float shipAspect = shipHeight / shipWidth;
        const float shipScaledWidth  = spriteScale.x();
        const float shipScaledHeight = shipScaledWidth * shipAspect;

        QPixmap jet = getJetStreamTexture(mShipType);
        const float jetWidth  = jet.width();
        const float jetHeight = jet.height();
        const float jetAspect = jetHeight / jetWidth;
        const float jetScaledWidth  = spriteScale.x();
        const float jetScaledHeight = jetScaledWidth * jetAspect;

        // set the target rectangle with the dimensions of the
        // sprite we want to draw.
        // the ship rect is the coordinate to which the jet stream and
        // the text are relative to.
        QRectF shipRect(0.0f, 0.0f, shipScaledWidth, shipScaledHeight);
        // offset it so that the center is aligned with the unit position
        shipRect.moveTo(position -
            QPointF(shipScaledWidth / 2.0, shipScaledHeight / 2.0));

        QRectF jetStreamRect = shipRect;
        jetStreamRect.translate(shipScaledWidth*0.6, (shipScaledHeight - jetScaledHeight) / 2.0);
        jetStreamRect.setSize(QSize(jetScaledWidth, jetScaledHeight));

        QRectF textRect = shipRect;
        textRect.translate(shipScaledWidth * 0.6 + jetScaledWidth * 0.75, 0);

        QFont font;
        font.setFamily("Monospace");
        font.setPixelSize(unitScale.y() / 1.75);
        QPen pen;
        pen.setWidth(2);
        pen.setColor(Qt::darkYellow);

        // draw textures
        painter.setFont(font);
        painter.setPen(pen);
        //painter.drawPixmap(jetStreamRect, jet, jet.rect());
        painter.drawPixmap(shipRect, ship, ship.rect());
        painter.drawText(textRect, Qt::AlignVCenter, mText);

        if (mShieldIsOn)
        {
            QPixmap shield = QPixmap(R("textures/spr_shield.png"));
            QRectF rect;
            // we don't bother to calculate the size for the shield properly
            // in order to cover the whole ship. instead we use a little fudge
            // factor to expand the shield.
            const auto fudge = 1.25f;
            const auto width = shipRect.width();
            rect.setHeight(width * fudge);
            rect.setWidth(width * fudge);
            rect.moveTo(shipRect.topLeft());
            rect.translate((rect.width()-shipRect.width()) / -2.0f,
                (rect.height()-shipRect.height()) / -2.0f);
            painter.drawPixmap(rect, shield, shield.rect());
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

    // get the position at ticksLater ticks time
    QVector2D getFuturePosition(float dtLater, const TransformState& state) const
    {
        const auto v = getVelocityVector(state);
        return mPosition + v * dtLater;
    }

    // get current position
    QVector2D getPosition() const
    { return mPosition; }

    void setPosition(QVector2D position)
    { mPosition = position; }

    void setMaxLifetime(quint64 ms)
    { mMaxLifeTime = ms; }

    void setViewString(QString str)
    { mText = str; }

    QPixmap getTexture() const
    { return getShipTexture(mShipType); }

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

    static const QPixmap& getShipTexture(ShipType type)
    {
        static QPixmap textures[] = {
            QPixmap(R("textures/Cricket.png")),
            QPixmap(R("textures/Mantis.png")),
            QPixmap(R("textures/Scarab.png")),
            QPixmap(R("textures/Locust.png"))
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

    static const QPixmap& getJetStreamTexture(ShipType type)
    {
        static QPixmap textures[] = {
            QPixmap(R("textures/Cricket_jet.png")),
            QPixmap(R("textures/Mantis_jet.png")),
            QPixmap(R("textures/Scarab_jet.png")),
            QPixmap(R("textures/Locust_jet.png"))
        };
        return textures[(int)type];
    }

private:
    QVector2D getVelocityVector(const TransformState& state) const
    {
        // todo: fix this, calculation should be in TransformState
        const float cols = state.numCols() - 8.0;
        const float pxw  = state.viewWidth() / cols;
        const float x    = pxw / state.viewWidth();
        const float y    = 0.0f;
        const auto velo = QVector2D(-x, y) * mVelocity;
        return velo;
    }

private:
    QVector2D mPosition;
    QString mText;
    float mLifeTime    = 0.0f;
    float mMaxLifeTime = 0.0f;
    float mVelocity    = 0.0f;
    std::unique_ptr<ParticleEngine> mParticles;
private:
    ShipType mShipType = ShipType::Slow;
private:
    bool mShieldIsOn = false;
};

class GameWidget::Missile : public GameWidget::Animation
{
public:
    Missile(QVector2D pos, QVector2D dir, quint64 lifetime, QString str) :
        position_(pos), direction_(dir), life_(lifetime), time_(0), text_(str)
    {}

    // set position in normalized widget space
    void setPosition(QVector2D pos)
    {
        position_ = pos;
    }

    virtual bool update(float dt, TransformState& state) override
    {
        time_ += dt;
        if (time_ > life_)
            return false;

        const auto d = (float)dt / (float)life_;
        const auto p = direction_ * d;
        position_ += p;
        return true;
    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {
        const auto dim = state.getScale();
        const auto pos = state.toViewSpace(position_);

        QFont font;
        QRectF rect;
        font.setFamily("Arcade");
        font.setPixelSize(dim.y() / 2);
        rect = QFontMetrics(font).boundingRect(text_);
        rect.moveTo(pos);

        QPen pen;
        pen.setWidth(2);
        pen.setColor(Qt::darkGray);
        painter.setFont(font);
        painter.setPen(pen);
        painter.drawText(rect, Qt::AlignCenter, text_);
    }
    QVector2D getPosition() const
    {
        return position_;
    }

private:
    QVector2D position_;
    QVector2D direction_;
    float life_;
    float time_;
    QString text_;
};

class GameWidget::UFO : public GameWidget::Animation
{
public:
    UFO()
    {
        mPosition.setX(math::rand(0.0, 1.0));
        mPosition.setY(math::rand(0.0, 1.0));

        const auto x = math::rand(-1.0, 1.0);
        const auto y = math::rand(-1.0, 1.0);
        mDirection = QVector2D(x, y);
        mDirection.normalize();

        mSprite.AddTexture("textures/alien/e_f1.png");
        mSprite.AddTexture("textures/alien/e_f2.png");
        mSprite.AddTexture("textures/alien/e_f3.png");
        mSprite.AddTexture("textures/alien/e_f4.png");
        mSprite.AddTexture("textures/alien/e_f5.png");
        mSprite.AddTexture("textures/alien/e_f6.png");
        mSprite.SetFps(10);
    }

    virtual bool update(float dt, TransformState& state) override
    {
        const auto maxLifeTime = 10000.0f;

        mRuntime += dt;
        if (mRuntime >= maxLifeTime)
            return false;

        QVector2D fuzzy;
        fuzzy.setY(std::sin((fmodf(mRuntime, 3000) / 3000.0) * 2 * M_PI));
        fuzzy.setX(mDirection.x());
        fuzzy.normalize();

        mPosition += (dt / 10000.0) * fuzzy;
        const auto x = mPosition.x();
        const auto y = mPosition.y();
        mPosition.setX(math::wrap(1.0f, 0.0f, x));
        mPosition.setY(math::wrap(1.0f, 0.0f, y));
        return true;
    }

    virtual void paintPreEffect(gfx::Painter& painter, const TransformState& state) override
    {
        const auto sec = mRuntime / 1000.0f;
        const auto pos = state.toViewSpace(mPosition);

        mSprite.SetAppRuntime(sec);

        gfx::Transform rings;
        rings.Resize(200, 200);
        rings.MoveTo(pos - QPoint(100, 100));
        painter.Draw(gfx::Rect(), rings, gfx::ConcentricRingsEffect(sec));

        gfx::Transform ufo;
        ufo.Resize(40, 40);
        ufo.MoveTo(pos - QPoint(20, 20));
        painter.Draw(gfx::Rect(), ufo, mSprite);
    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {
    }

    virtual QRectF getBounds(const TransformState& state) const override
    {
        const auto pos = state.toViewSpace(mPosition);

        QRectF bounds;
        bounds.moveTo(pos - QPoint(20, 20));
        bounds.setSize(QSize(40, 40));
        return bounds;
    }

    virtual ColliderType getColliderType() const override
    { return ColliderType::UFO; }

    void invertDirection()
    {
        mDirection *= -1.0f;
    }

    QVector2D getPosition() const
    { return mPosition; }

    static bool shouldMakeRandomAppearance()
    {
        if (math::rand(0, 5000) == 7)
            return true;
        return false;
    }

    const QPixmap& getCurrentTexture() const
    {
        // this is now a dummy function and is used
        // to get the pixmap for rendering the debris
        // animation. once the debris has been refactored
        // we can remove this too.
        static QPixmap p(R("textures/alien/e_f1.png"));
        return p;
    }
private:
    float mRuntime = 0.0f;
    QVector2D mDirection;
    QVector2D mPosition;
private:
    gfx::SpriteSet mSprite;
};


class GameWidget::BigExplosion : public GameWidget::Animation
{
public:
    BigExplosion(float lifetime) : mLifeTime(lifetime)
    {
        for (int i=1; i<=90; ++i)
        {
            const auto& name = base::FormatString("textures/bomb/explosion1_00%1.png", i);
            mSprite.AddTexture(name);
        }
        mSprite.SetFps(90 / (lifetime/1000.0f));
    }

    virtual bool update(float dt, TransformState& state) override
    {
        mRunTime += dt;
        if (mRunTime > mLifeTime)
            return false;
        return true;
    }
    virtual void paintPreEffect(gfx::Painter& painter, const TransformState& state) override
    {
        mSprite.SetAppRuntime(mRunTime / 1000.0f);

        const auto ExplosionWidth = state.viewWidth() * 2.0f;
        const auto ExplosionHeight = state.viewHeight() * 2.3f;

        const auto x = state.viewWidth() / 2 - (ExplosionWidth * 0.5);
        const auto y = state.viewHeight() / 2 - (ExplosionHeight * 0.5);

        gfx::Transform bang;
        bang.Resize(ExplosionWidth, ExplosionHeight);
        bang.MoveTo(x, y);
        painter.Draw(gfx::Rect(), bang, mSprite);

    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {
    }
private:
    const float mLifeTime = 0.0f;
    float mRunTime = 0.0f;
private:
    gfx::SpriteSet mSprite;
};

class GameWidget::Score : public GameWidget::Animation
{
public:
    Score(QVector2D position, float start, float lifetime, unsigned score) :
        position_(position), start_(start), life_(lifetime), time_(0), score_(score)
    {}

    virtual bool update(float dt, TransformState& state) override
    {
        time_ += dt;
        if (time_ < start_)
            return true;
        return time_ - start_ < life_;
    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {
        if (time_ < start_)
            return;

        const auto alpha = 1.0 - (float)(time_ - start_ )/ (float)life_;
        const auto dim = state.getScale();
        const auto top = state.toViewSpace(position_);
        const auto end = top + dim;

        QColor color(Qt::darkYellow);
        color.setAlpha(0xff * alpha);

        QPen pen;
        pen.setWidth(2);
        pen.setColor(color);
        QFont font("Arcade");
        font.setPixelSize(dim.y() / 2.0);
        painter.setPen(pen);
        painter.setFont(font);
        painter.drawText(QRectF(top, end), QString("%1").arg(score_));
    }
private:
    QVector2D position_;
private:
    float start_;
    float life_;
    float time_;
private:
    unsigned score_;
};



// game space background rendering
class GameWidget::Background
{
public:
    Background(const QVector2D& direction)
    {
        gfx::ParticleEngine::Params params;
        params.init_rect_width  = 1024;
        params.init_rect_height = 1024;
        params.max_xpos = 1024;
        params.max_ypos = 1024;
        params.num_particles = 800;
        params.min_velocity = 5.0f;
        params.max_velocity = 100.0f;
        params.min_point_size = 1.0f;
        params.max_point_size = 8.0f;
        params.direction_sector_start_angle = std::acos(direction.x());
        params.direction_sector_size = 0.0f;
        mStars = std::make_unique<gfx::ParticleEngine>(params);
    }
    void paint(gfx::Painter& painter, const QRectF& rect)
    {
        gfx::Transform t;
        t.MoveTo(0, 0);
        t.Resize(rect.width(), rect.height());

    #ifdef WINDOWS_OS
        // have a problem on windows that the background texture looks very dark
        // so we add little gamma hack here.
        const float gamma = 1.0f/1.4f;
    #else
        const float gamma = 1.0f;
    #endif

        // first draw the static background image.
        painter.Draw(gfx::Rect(), t,
            gfx::TextureFill("textures/SpaceBackground.png")
            .SetGamma(gamma));

        // then draw the particle engine
        painter.Draw(*mStars, t,
            gfx::TextureFill("textures/RoundParticle.png")
            .SetSurfaceType(gfx::Material::SurfaceType::Transparent)
            .SetRenderPoints(true));
    }

    void update(float dt)
    {
        mStars->Update(dt/1000.0f);
    }
private:
    std::unique_ptr<gfx::ParticleEngine> mStars;
};

class GameWidget::Scoreboard : public GameWidget::State
{
public:
    Scoreboard(unsigned score, unsigned bonus, bool isHighScore, int unlockedLevel)
    {
        mText.append("Level complete!\n\n");
        mText.append(QString("You scored %1 points\n").arg(score));
        mText.append(QString("Difficulty bonus %1 points\n").arg(bonus));
        mText.append(QString("Total %1 points\n\n").arg(score + bonus));

        if (isHighScore)
            mText.append("New high score!\n");

        if (unlockedLevel)
            mText.append(QString("Level %1 unlocked!\n").arg(unlockedLevel + 1));

        mText.append("\nPress any key to continue");
    }

    virtual void paint(QPainter& painter, const QRectF& area, const QPointF& scale) override
    {
        QPen pen;
        pen.setWidth(1);
        pen.setColor(Qt::darkGray);

        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(scale.y() / 2);

        painter.setPen(pen);
        painter.setFont(font);
        painter.drawText(area, Qt::AlignCenter, mText);
    }

    virtual void update(float dt) override
    {}

    virtual Action mapAction(const QKeyEvent* press) const override
    {
        return Action::CloseState;
    }
    virtual void keyPress(const QKeyEvent* press) override
    {}
private:
    QString mText;
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

    virtual void paintPostEffect(gfx::Painter& painter, const TransformState& parentTransform) const override
    {
        const auto cols = 7;
        const auto rows = 6;
        TransformState myTransform(parentTransform.viewRect(), cols, rows);

        QRectF rc = myTransform.toViewSpaceRect(QPoint(3, 4), QPoint(4, 5));
        const auto x = rc.x();
        const auto y = rc.y();
        const auto w = rc.width();
        const auto h = rc.height();

        gfx::Transform dt, mt;
        dt.MoveTo(x, y);
        dt.Resize(w, h);

        mt.MoveTo(x, y+2);
        mt.Resize(w, h-4);
        painter.DrawMasked(gfx::Rect(), dt, gfx::Rect(), mt, gfx::SlidingGlintEffect(mTotalTimeRun/1000.0f));
    }

    virtual void paint(QPainter& painter, const QRectF& area, const QPointF& unit) override
    {
        QPen regular;
        regular.setWidth(1);
        regular.setColor(Qt::darkGray);
        painter.setPen(regular);

        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(unit.y() / 2);
        painter.setFont(font);

        QFont underline;
        underline.setFamily("Arcade");
        underline.setUnderline(true);
        underline.setPixelSize(unit.y() / 2);

        QPen selected;
        selected.setWidth(2);
        selected.setColor(Qt::darkGreen);

        QPen locked;
        locked.setWidth(2);
        locked.setColor(Qt::darkRed);

        const auto cols = 7;
        const auto rows = 6;
        TransformState state(area, cols, rows);

        QRectF rect;

        rect = state.toViewSpaceRect(QPoint(0, 0), QPoint(cols, 3));
        painter.drawText(rect, Qt::AlignHCenter | Qt::AlignBottom,
            "Evil chinese characters are attacking!\n"
            "Only you can stop them by typing the right pinyin.\n"
            "Good luck.\n\n"
            "Esc - Exit\n"
            "F1 - Help\n"
            "F2 - Settings\n"
            "F3 - Credits\n\n"
            "Difficulty\n");

        rect = state.toViewSpaceRect(QPoint(2, 3), QPoint(5, 4));

        TransformState sub(rect, 3, 1);
        rect = sub.toViewSpaceRect(QPoint(0, 0), QPoint(1, 1));
        if (mCurrentProfileIndex == 0)
        {
            painter.setFont(underline);
            if (mCurrentRowIndex == 0)
                painter.setPen(selected);
        }

        painter.drawText(rect, Qt::AlignTop | Qt::AlignRight, "Easy");
        painter.setPen(regular);
        painter.setFont(font);

        rect = sub.toViewSpaceRect(QPoint(1, 0), QPoint(2, 1));
        if (mCurrentProfileIndex == 1)
        {
            painter.setFont(underline);
            if (mCurrentRowIndex == 0)
                painter.setPen(selected);
        }

        painter.drawText(rect, Qt::AlignTop | Qt::AlignHCenter, "Normal");
        painter.setPen(regular);
        painter.setFont(font);

        rect = sub.toViewSpaceRect(QPoint(2, 0), QPoint(3, 1));
        if (mCurrentProfileIndex == 2)
        {
            painter.setFont(underline);
            if (mCurrentRowIndex == 0)
                painter.setPen(selected);
        }

        painter.drawText(rect, Qt::AlignTop | Qt::AlignLeft, "Chinese");
        painter.setPen(regular);
        painter.setFont(font);

        QFont small;
        small.setFamily("Arcade");
        small.setPixelSize(unit.y() / 2.5);
        painter.setFont(small);

        const auto prev = mCurrentLevelIndex > 0 ? mCurrentLevelIndex - 1 : mLevels.size() - 1;
        const auto next = (mCurrentLevelIndex + 1) % mLevels.size();
        const auto& left  = mLevels[prev];
        const auto& mid   = mLevels[mCurrentLevelIndex];
        const auto& right = mLevels[next];

        rect = state.toViewSpaceRect(QPoint(1, 4), QPoint(2, 5));
        drawLevel(painter, rect, *left, prev, false);

        bool hilite = false;
        if (mCurrentRowIndex == 1)
        {
            if (mInfos[mCurrentLevelIndex].locked)
                 painter.setPen(locked);
            else painter.setPen(selected);
            hilite = true;
        }
        else
        {
            painter.setPen(regular);
        }

        rect = state.toViewSpaceRect(QPoint(3, 4), QPoint(4, 5));
        drawLevel(painter, rect, *mid, mCurrentLevelIndex, hilite);

        painter.setPen(regular);
        rect = state.toViewSpaceRect(QPoint(5, 4), QPoint(6, 5));
        painter.drawRect(rect);
        drawLevel(painter, rect, *right, next, false);

        static unsigned blink_text = 0;

        const bool draw_text = (blink_text % TextBlinkFrameCycle) < (TextBlinkFrameCycle / 2);
        if (draw_text)
        {
            rect = state.toViewSpaceRect(QPoint(0, rows-1), QPoint(cols, rows));
            painter.setPen(regular);
            painter.setFont(font);
            if (mInfos[mCurrentLevelIndex].locked)
            {
                painter.drawText(rect, Qt::AlignCenter, "This level is locked!\n");
            }
            else
            {
                painter.drawText(rect, Qt::AlignCenter, "Press Space to play!\n");
            }
        }
        ++blink_text;
    }

    virtual Action mapAction(const QKeyEvent* event) const
    {
        switch (event->key())
        {
            case Qt::Key_F1:
                return Action::OpenHelp;
            case Qt::Key_F2:
                return Action::OpenSettings;
            case Qt::Key_F3:
                return Action::OpenAbout;
            case Qt::Key_Escape:
                return Action::QuitApp;
            case Qt::Key_Space:
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

    virtual void keyPress(const QKeyEvent* press) override
    {
        const int numLevelsMin = 0;
        const int numLevelsMax = mLevels.size() - 1;
        const int numProfilesMin = 0;
        const int numProfilesMax = 2;

        bool bPlaySound = false;

        const auto key = press->key();
        if (key == Qt::Key_Left)
        {
            if (mCurrentRowIndex == 0)
            {
                mCurrentProfileIndex = math::wrap(numProfilesMax, numProfilesMin, mCurrentProfileIndex - 1);
            }
            else
            {
                mCurrentLevelIndex = math::wrap(numLevelsMax, numLevelsMin, mCurrentLevelIndex -1);
            }
            bPlaySound = true;
        }
        else if (key == Qt::Key_Right)
        {
            if (mCurrentRowIndex == 0)
            {
                mCurrentProfileIndex = math::wrap(numProfilesMax, numProfilesMin,  mCurrentProfileIndex + 1);
            }
            else
            {
                mCurrentLevelIndex = math::wrap(numLevelsMax, numLevelsMin, mCurrentLevelIndex + 1);
            }
            bPlaySound = true;
        }
        else if (key == Qt::Key_Up)
        {
            mCurrentRowIndex = math::wrap(1, 0, mCurrentRowIndex - 1);
        }
        else if (key == Qt::Key_Down)
        {
            mCurrentRowIndex = math::wrap(1, 0, mCurrentRowIndex + 1);
        }

        if (bPlaySound && mPlaySounds)
        {
        #ifdef GAME_ENABLE_AUDIO
            static auto swoosh = std::make_shared<AudioSample>("sounds/Slide_Soft_00.ogg", "swoosh");
            g_audio->play(swoosh);
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
    void drawLevel(QPainter& painter, const QRectF& rect, const Level& level, int index, bool hilite)
    {
        const auto& info = mInfos[index];

        QString locked;
        if (info.locked)
            locked = "Locked";
        else if (info.highScore)
            locked = QString("%1 points").arg(info.highScore);
        else locked = "Play!";

        if (hilite)
        {
            QPen glow   = painter.pen();
            QPen normal = painter.pen();
            glow.setWidth(12);
            painter.setPen(glow);
            painter.setOpacity(0.2);
            painter.drawRect(rect);
            painter.setOpacity(1.0);
            painter.setPen(normal);
        }

        painter.drawRect(rect);
        painter.drawText(rect, Qt::AlignCenter,
            QString("Level %1\n%2\n%3").arg(index+1).arg(level.name()).arg(locked));
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

    virtual Action mapAction(const QKeyEvent* event) const override
    {
        if (event->key() == Qt::Key_Escape)
            return Action::CloseState;
        return Action::None;
    }
    virtual void keyPress(const QKeyEvent* event) override
    {}

    virtual void paint(QPainter& painter, const QRectF& rect, const QPointF& scale) override
    {
        QPen pen;
        pen.setWidth(1);
        pen.setColor(Qt::darkGray);

        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(scale.y() / 2);

        auto str = QString("%1").arg(LevelUnlockCriteria * 100, 0, 'f', 0);

        painter.setPen(pen);
        painter.setFont(font);
        painter.drawText(rect, Qt::AlignCenter,
            QString("Kill the invaders by typing the correct pinyin.\n"
            "You get scored based on how fast you kill and\n"
            "how complicated the characters are.\n\n"
            "Invaders that approach the left edge will show\n"
            "the pinyin string and score no points.\n"
            "You will lose points for invaders that you faill to kill.\n"
            "Score %1% or higher to unlock the next level.\n\n"
            "Type BOMB to ignite a bomb.\n"
            "Type WARP to enter a time warp.\n"
            "Press Space to clear the input.\n\n"
            "Press Esc to exit\n").arg(str));
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

    virtual void update(float dt) override
    {}

    virtual void paint(QPainter& painter, const QRectF& rect, const QPointF& scale) override
    {
        QPen regular;
        regular.setWidth(1);
        regular.setColor(Qt::darkGray);

        QPen selected;
        selected.setWidth(1);
        selected.setColor(Qt::darkGreen);

        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(scale.y() / 2);

        painter.setPen(regular);
        painter.setFont(font);

        QFont underline;
        underline.setFamily("Arcade");
        underline.setUnderline(true);
        underline.setPixelSize(scale.y() / 2);

#ifndef GAME_ENABLE_AUDIO
        painter.drawText(rect, Qt::AlignCenter,
            "Audio is not supported on this platform.\n\n"
            "Press Esc to exit\n");
        return;
#endif

        const auto cols = 1;
        const auto rows = 7;
        TransformState state(rect, cols, rows);

        QRectF rc;
        rc = state.toViewSpaceRect(QPointF(0, 1), QPointF(1, 2));
        painter.drawText(rc, Qt::AlignCenter,
            "Press space to toggle a setting.");

        painter.setPen(regular);

        if (mSettingIndex == 0)
            painter.setPen(selected);
        rc = state.toViewSpaceRect(QPoint(0, 2), QPoint(1, 3));
        painter.drawText(rc, Qt::AlignCenter,
            tr("Sound Effects: %1").arg(mPlaySounds ? "On" : "Off"));

        painter.setPen(regular);

        if (mSettingIndex == 1)
            painter.setPen(selected);
        rc = state.toViewSpaceRect(QPoint(0, 3), QPoint(1, 4));
        painter.drawText(rc, Qt::AlignCenter,
            tr("Awesome Music: %1").arg(mPlayMusic ? "On" : "Off"));

        painter.setPen(regular);

        rc = state.toViewSpaceRect(QPoint(0, 4), QPoint(1, 5));
        if (mSettingIndex == 2)
            painter.setPen(selected);
        painter.drawText(rc, Qt::AlignCenter,
            tr("Fullscreen: %1").arg(mFullscreen ? "On" : "Off"));

        rc = state.toViewSpaceRect(QPoint(0, 5), QPoint(1, 6));
        painter.setPen(regular);
        painter.drawText(rc, Qt::AlignCenter,
            "Press Esc to exit");
    }

    virtual Action mapAction(const QKeyEvent* press) const override
    {
        if (press->key() == Qt::Key_Escape)
            return Action::CloseState;
        return Action::None;
    }

    virtual void keyPress(const QKeyEvent* keyPress) override
    {
        const auto key = keyPress->key();
        if (key == Qt::Key_Space)
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
        else if (key == Qt::Key_Up)
        {
            if (--mSettingIndex < 0)
                mSettingIndex = 2;
        }
        else if (key == Qt::Key_Down)
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
    virtual void paint(QPainter& painter, const QRectF& area, const QPointF& scale) override
    {
        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(scale.y() / 2);
        painter.setFont(font);

        QPen pen;
        pen.setWidth(1);
        pen.setColor(Qt::darkGray);
        painter.setPen(pen);

        painter.drawText(area, Qt::AlignCenter,
            QString::fromUtf8("Pinyin-Invaders %1.%2\n\n"
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
                "cynicmusic\n"
                "http://www.cynicmusic.com\n\n"
                "Press Esc to exit"
                ).arg(MAJOR_VERSION).arg(MINOR_VERSION));
    }

    virtual void update(float dt) override
    {}

    virtual Action mapAction(const QKeyEvent* press) const override
    {
        if (press->key() == Qt::Key_Escape)
            return Action::CloseState;
        return Action::None;
    }
    virtual void keyPress(const QKeyEvent* press) override
    {}
private:
};

class GameWidget::PlayGame : public GameWidget::State
{
public:
    PlayGame(const Game::Setup& setup, Level& level, Game& game) : mSetup(setup), mLevel(level), mGame(game)
    {
        mCurrentText = initString();
    }

    virtual void paint(QPainter& painter, const QRectF& area, const QPointF& unit) override
    {
        switch (mState)
        {
            case GameState::Prepare:
                paintFleet(painter, area, unit);
                break;
            case GameState::Playing:
                {
                    TransformState state(area, ViewCols, ViewRows);

                    QPointF top;
                    QPointF bot;

                    // layout the HUD at the first "game row"
                    top = state.toViewSpace(QPoint(0, 0));
                    bot = state.toViewSpace(QPoint(ViewCols, 1));
                    paintHUD(painter, QRectF(top, bot), unit);

                    // paint the player at the last "game row"
                    top = state.toViewSpace(QPoint(0, ViewRows-1));
                    bot = state.toViewSpace(QPoint(ViewCols, ViewRows));
                    paintPlayer(painter, QRectF(top, bot), area, unit);
                }
                break;
        }
    }
    virtual void update(float dt) override
    {}

    virtual Action mapAction(const QKeyEvent* press) const override
    {
        const auto key = press->key();
        if (key == Qt::Key_Escape)
            return Action::CloseState;

        switch (mState)
        {
            case GameState::Prepare:
                break;

            case GameState::Playing:
                if (key == Qt::Key_F1)
                    return Action::OpenHelp;
                else if (key == Qt::Key_F2)
                    return Action::OpenSettings;
                break;
        }
        return Action::None;
    }
    virtual void keyPress(const QKeyEvent* press) override
    {
        const auto key = press->key();

        if (mState == GameState::Prepare)
        {
            if (key == Qt::Key_Space)
            {
                std::srand(0x7f6a4b);
                mLevel.reset();
                mGame.Play(&mLevel, mSetup);
                mState = GameState::Playing;
            }
        }
        else if (mState == GameState::Playing)
        {
            if (mCurrentText == initString())
                mCurrentText.clear();

            if (key == Qt::Key_Backspace)
            {
                if (!mCurrentText.isEmpty())
                    mCurrentText.chop(1);
            }
            else if (key == Qt::Key_Space)
            {
                mCurrentText.clear();
            }
            else if (key >= 0x41 && key <= 0x5a)
            {
                mCurrentText.append(key);
                if (mCurrentText == "BOMB")
                {
                    Game::Bomb bomb;
                    mGame.IgniteBomb(bomb);
                    mCurrentText.clear();
                }
                else if (mCurrentText == "WARP")
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
                    missile.position = mMissileLaunchPosition; // todo: fix this
                    missile.string   = mCurrentText.toLower();
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
    void paintFleet(QPainter& painter, const QRectF& area, const QPointF& scale) const
    {
        QPen pen;
        pen.setWidth(1);
        pen.setColor(Qt::darkGray);
        painter.setPen(pen);

        QFont bigFont;
        bigFont.setFamily("Arcade");
        bigFont.setPixelSize(scale.y() / 2);
        painter.setFont(bigFont);

        QFont smallFont;
        smallFont.setFamily("Arcade");
        smallFont.setPixelSize(scale.y() / 3);

        const auto& enemies = mLevel.getEnemies();
        const auto cols = 3;
        const auto rows = (enemies.size() / cols) + 2;

        TransformState state(area, cols, rows);
        const auto header = state.toViewSpaceRect(QPoint(0, 0), QPoint(cols, 1));
        const auto footer = state.toViewSpaceRect(QPoint(0, rows-1), QPoint(cols, rows));

        painter.drawText(header, Qt::AlignCenter,
            "Kill the following enemies\n");

        for (size_t i=0; i<enemies.size(); ++i)
        {
            const auto& e   = enemies[i];
            const auto col  = i % cols;
            const auto row  = i / cols;
            const auto rect = state.toViewSpaceRect(QPoint(col, row + 1), QPoint(col+1, row+2));
            painter.setFont(bigFont);
            painter.drawText(rect, Qt::AlignHCenter | Qt::AlignTop,
                QString("%1 %2\n\n")
                .arg(e.string)
                .arg(e.killstring));
            painter.setFont(smallFont);
            painter.drawText(rect, Qt::AlignHCenter | Qt::AlignTop,
                QString("\n\n\n%1").arg(e.help));
        }

        static unsigned text_blink = 0;

        const bool draw_text = (text_blink % TextBlinkFrameCycle) < (TextBlinkFrameCycle / 2);
        if (draw_text)
        {
            painter.setFont(bigFont);
            painter.drawText(footer, Qt::AlignCenter, "\n\nPress Space to play!");
        }
        ++text_blink;
    }

    void paintHUD(QPainter& painter, const QRectF& area, const QPointF& unit) const
    {
        const auto& score  = mGame.GetScore();
        const auto& result = score.maxpoints ?
            (float)score.points / (float)score.maxpoints * 100 : 0.0;
        const auto& format = QString("%1").arg(result, 0, 'f', 0);
        const auto bombs   = mGame.GetNumBombs();
        const auto warps   = mGame.GetNumWarps();

        QPen pen;
        pen.setColor(Qt::darkGreen);
        pen.setWidth(1);
        painter.setPen(pen);

        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(unit.y() / 2);
        painter.setFont(font);

        painter.drawText(area, Qt::AlignCenter,
            QString("Score %2 (%3%) | Enemies x %4 | Bombs x %5 | Warps x %6 | (F1 for help)")
                .arg(score.points)
                .arg(format)
                .arg(score.pending)
                .arg(bombs)
                .arg(warps));
    }

    void paintPlayer(QPainter& painter, const QRectF& area, const QRectF& window, const QPointF& unit)
    {
        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(area.height() / 2);
        painter.setFont(font);

        QFontMetrics fm(font);
        const auto width  = fm.width(mCurrentText);
        const auto height = fm.height();

        // calculate text X, Y (top left)
        const auto x = area.x() + ((area.width() - width) / 2.0);
        const auto y = area.y() + ((area.height() - height) / 2.0);

        QPen pen;
        pen.setWidth(2);
        pen.setColor(Qt::darkGray);
        painter.setPen(pen);

        QRect rect(QPoint(x, y), QPoint(x + width, y + height));
        painter.drawRect(rect);
        painter.drawText(rect, Qt::AlignCenter | Qt::AlignVCenter, mCurrentText);

        TransformState transform(window, ViewCols, ViewRows);

        // save the missile launch position
        // todo: fix this
        mMissileLaunchPosition = transform.toNormalizedViewSpace(QPoint(x, y));
    }

    static QString initString()
    { return "Type the correct pinyin to kill the enemies!"; }

private:
    const Game::Setup mSetup;
    Level& mLevel;
    Game&  mGame;

    enum class GameState {
        Prepare,
        Playing
    };
    GameState mState = GameState::Prepare;
    QString mCurrentText;
    QVector2D mMissileLaunchPosition;
};


GameWidget::GameWidget()
{
#ifdef GAME_ENABLE_AUDIO
    // sound effects FX
    static auto sndExplosion = std::make_shared<AudioSample>("sounds/explode.wav", "explosion");
#endif

    QFontDatabase::addApplicationFont(R("fonts/ARCADE.TTF"));

    mGame.reset(new Game(GameCols, GameRows));

    mGame->onMissileKill = [&](const Game::Invader& i, const Game::Missile& m, unsigned killScore)
    {
        auto it = mInvaders.find(i.identity);

        TransformState state(rect(), ViewCols, ViewRows);
        const auto scale = state.getScale();

        std::unique_ptr<Invader> invader(it->second.release());

        // calculate position for the invader to be in at now + missile fly time
        // and then aim the missile to that position.
        const auto missileFlyTime   = 500;
        const auto explosionTime    = 1000;
        const auto missileEnd       = invader->getFuturePosition(missileFlyTime, state);
        const auto missileBeg       = m.position;
        const auto missileDir       = missileEnd - missileBeg;

        std::unique_ptr<Animation> missile(new Missile(missileBeg, missileDir, missileFlyTime, m.string.toUpper()));
        std::unique_ptr<Explosion> explosion(new Explosion(missileEnd, missileFlyTime, explosionTime));
        std::unique_ptr<Smoke> smoke(new Smoke(missileEnd, missileFlyTime + 100, explosionTime + 500));
        std::unique_ptr<Debris> debris(new Debris(invader->getTexture(), missileEnd, missileFlyTime, explosionTime + 500));
        std::unique_ptr<Sparks> sparks(new Sparks(missileEnd, missileFlyTime, explosionTime));
        std::unique_ptr<Animation> score(new Score(missileEnd, explosionTime, 2000, killScore));

        invader->setMaxLifetime(missileFlyTime);
        explosion->setScale(invader->getScale() * 1.5);
        smoke->setScale(invader->getScale() * 2.5);
        sparks->setColor(gfx::Color4f(255, 255, 68, 180));
        debris->setTextureScaleFromWidth(scale.x());

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
            g_audio->play(sndExplosion, std::chrono::milliseconds(missileFlyTime));
        }
#endif
    };

    mGame->onMissileDamage = [&](const Game::Invader& i, const Game::Missile& m)
    {
        auto it = mInvaders.find(i.identity);
        auto& inv = it->second;

        TransformState state(rect(), ViewCols, ViewRows);

        const auto missileFlyTime  = 500;
        const auto missileEnd      =  inv->getFuturePosition(missileFlyTime, state);
        const auto missileBeg      = m.position;
        const auto missileDir      = missileEnd - missileBeg;

        std::unique_ptr<Animation> missile(new Missile(missileBeg, missileDir, missileFlyTime, m.string.toUpper()));
        std::unique_ptr<Sparks> sparks(new Sparks(missileEnd, missileFlyTime, 500));

        sparks->setColor(gfx::Color::DarkGray);

        auto viewString = i.viewList.join("");
        inv->setViewString(viewString);

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
        auto str = i.viewList.join("");
        inv->setViewString(str);
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
        TransformState state(rect(), ViewCols, ViewRows);

        const auto pos = state.toNormalizedViewSpace(GameSpace{inv.xpos, inv.ypos+1});

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

        // the game expresses invader speed as the number of discrete steps it takes
        // per each tick of game.
        // here well want to express this velocity as a normalized distance over seconds
        const auto tick       = 1000.0 / mProfiles[mCurrentProfile].speed;
        const auto numTicks   = state.numCols() / (double)inv.speed;
        const auto numSeconds = tick * numTicks;
        const auto velocity   = state.numCols() / numSeconds;

        const auto killString = inv.killList.join("");
        const auto viewString = inv.viewList.join("");

        std::unique_ptr<Invader> invader(new Invader(pos, viewString, velocity, type));
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
        auto str = inv.killList.join("");
        it->second->setViewString(str);
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
    QVector2D spaceJunkDirection(4, 3);
    spaceJunkDirection.normalize();

    // create the background object.
    mBackground.reset(new Background(spaceJunkDirection));

    for (size_t i=0; i<20; ++i)
    {
        mAnimations.emplace_back(new Asteroid(spaceJunkDirection));
    }

    // initialize the input/state stack with the main menu.
    auto menu = std::make_unique<MainMenu>(mLevels, mLevelInfos, true);
    mStates.push(std::move(menu));

    // enable keyboard events
    setFocusPolicy(Qt::StrongFocus);

    // indicates that the widget has no background and
    // the system doesn't automatically paint the background.
    // this is fine for us since we draw everything everytime anyway.
    setAttribute(Qt::WA_NoSystemBackground);

    // indicates that the widget draws all its pixels every time,
    // thus there's no need to erase widget before painting.
    setAttribute(Qt::WA_OpaquePaintEvent);
}

GameWidget::~GameWidget() = default;

void GameWidget::loadLevels(const QString& file)
{
    mLevels = Level::loadLevels(file);

    for (const auto& level : mLevels)
    {
        LevelInfo info;
        info.highScore = 0;
        info.name      = level->name();
        info.locked    = true;
        mLevelInfos.push_back(info);
        if (!level->validate())
        {
            auto name  = level->name();
            auto ascii = name.toStdString();
            qFatal("Level is broken: %s!", ascii.c_str());
        }
    }
    mLevelInfos[0].locked = false;
}

void GameWidget::unlockLevel(const QString& name)
{
    for (auto& info : mLevelInfos)
    {
        if (info.name != name)
            continue;

        info.locked = false;
        return;
    }
}

void GameWidget::setLevelInfo(const LevelInfo& info)
{
    for (auto& i : mLevelInfos)
    {
        if (i.name != info.name)
            continue;

        i = info;
        return;
    }
}

bool GameWidget::getLevelInfo(LevelInfo& info, unsigned index) const
{
    if (index >= mLevels.size())
        return false;
    info = mLevelInfos[index];
    return true;
}

void GameWidget::setProfile(const Profile& profile)
{
    mProfiles.push_back(profile);
}

void GameWidget::launchGame()
{
    playMusic();
}

void GameWidget::updateGame(float dt)
{

#ifdef GAME_ENABLE_AUDIO
    // handle audio events.
    AudioPlayer::TrackEvent event;
    while (g_audio->get_event(&event))
    {
        DEBUG("Audio event (%1)", event.id);
    }

#endif


    TransformState state(rect(), GameCols, GameRows + 2);

    const auto time = dt * mWarpFactor;
    const auto tick = 1000.0 / mProfiles[mCurrentProfile].speed;

    if (UFO::shouldMakeRandomAppearance())
    {
        mAnimations.emplace_back(new UFO);
    }

    mBackground->update(time);

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
            invader->update(time, state);
        }
    }

    // update animations
    for (auto it = std::begin(mAnimations); it != std::end(mAnimations);)
    {
        auto& anim = *it;
        if (!anim->update(time, state))
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
                    const auto& lhsBounds = lhsAnim->getBounds(state);
                    const auto& rhsBounds = animation->getBounds(state);
                    if (lhsBounds.intersects(rhsBounds) || rhsBounds.intersects(lhsBounds))
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
            std::unique_ptr<Debris> debris(new Debris(UFO->getCurrentTexture(),
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

void GameWidget::renderGame()
{
    repaint();
}

void GameWidget::setPlaySounds(bool onOff)
{
    mPlaySounds = onOff;
    mStates.top()->setPlaySounds(onOff);
}

void GameWidget::setMasterUnlock(bool onOff)
{
    mMasterUnlock = onOff;
    mStates.top()->setMasterUnlock(onOff);
}

void GameWidget::initializeGL()
{
    DEBUG("Initialize OpenGL");
    // create custom painter for fancier shader based effects.
    mCustomGraphicsDevice  = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2);
    mCustomGraphicsPainter = gfx::Painter::Create(mCustomGraphicsDevice);
}

void GameWidget::closeEvent(QCloseEvent* close)
{
    mRunning = false;
}

void GameWidget::paintEvent(QPaintEvent* paint)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::HighQualityAntialiasing);

    TransformState state(rect(), ViewCols, ViewRows);

    // implement simple painter's algorithm here
    // i.e. paint the game scene from back to front.

    const bool bIsGameRunning = mStates.top()->isGameRunning();

    // do a first pass using custom painter
    {
        painter.beginNativePainting();

        gfx::Device::StateBuffer currentState;
        mCustomGraphicsDevice->GetState(&currentState);
        mCustomGraphicsPainter->SetViewport(0, 0, width(), height());

        mBackground->paint(*mCustomGraphicsPainter, rect());

        for (auto& anim : mAnimations)
        {
            anim->paintPreEffect(*mCustomGraphicsPainter, state);
        }

        if (bIsGameRunning)
        {
            for (auto& pair : mInvaders)
            {
                auto& invader = pair.second;
                invader->paintPreEffect(*mCustomGraphicsPainter, state);
            }
        }

        mCustomGraphicsDevice->SetState(currentState);
        painter.endNativePainting();
    }


    // paint animations
    for (auto& anim : mAnimations)
    {
        anim->paint(painter, state);
    }

    // paint the invaders
    if (bIsGameRunning)
    {
        for (auto& pair : mInvaders)
        {
            auto& invader = pair.second;
            invader->paint(painter, state);
        }
    }

    // finally paint the menu/HUD
    mStates.top()->paint(painter, rect(), state.getScale());

    {
        // do a second pass painter using the custom painter.
        // since we're drawing using the same OpenGL context the
        // state management is somewhat tricky.
        painter.beginNativePainting();

        gfx::Device::StateBuffer currentState;
        mCustomGraphicsDevice->GetState(&currentState);
        mCustomGraphicsPainter->SetViewport(0, 0, width(), height());

        for (auto& anim : mAnimations)
        {
            anim->paintPostEffect(*mCustomGraphicsPainter, state);
        }

        if (bIsGameRunning)
        {
            for (auto& pair : mInvaders)
            {
                auto& invader = pair.second;
                invader->paintPostEffect(*mCustomGraphicsPainter, state);
            }
        }

        mStates.top()->paintPostEffect(*mCustomGraphicsPainter, state);

        mCustomGraphicsDevice->SetState(currentState);
        painter.endNativePainting();
    }

    if (mShowFps)
    {
        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(18);

        QPen pen;
        pen.setWidth(1);
        pen.setColor(Qt::darkRed);
        painter.setFont(font);
        painter.setPen(pen);
        painter.drawText(QPointF(10.0f, 20.0f), QString("fps: %1").arg(mCurrentfps));
    }
}

void GameWidget::keyPressEvent(QKeyEvent* press)
{
    const auto key = press->key();
    const auto mod = press->modifiers();
    if (key == Qt::Key_R && mod == Qt::ShiftModifier)
    {
        DEBUG("Recompile shaders");
        mCustomGraphicsDevice->DeleteShaders();
        mCustomGraphicsDevice->DeletePrograms();
        return;
    }

    const auto action = mStates.top()->mapAction(press);
    switch (action)
    {
        case State::Action::None:
            mStates.top()->keyPress(press);
            break;
        case State::Action::OpenHelp:
            mStates.push(std::make_unique<GameHelp>());
            break;
        case State::Action::OpenSettings:
            {
                auto settings = std::make_unique<Settings>(mPlayMusic, mPlaySounds, isFullScreen());
                settings->onToggleFullscreen = [this](bool fullscreen) {
                    if (fullscreen)
                    {
                        showFullScreen();
                        QApplication::setOverrideCursor(Qt::BlankCursor);
                    }
                    else
                    {
                        showNormal();
                        QApplication::restoreOverrideCursor();
                    }
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
            close();
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
                DEBUG("Start game: %1 / %2", level->name(), profile.name);

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

void GameWidget::playMusic()
{
#ifdef GAME_ENABLE_AUDIO
    static auto music = std::make_shared<AudioSample>("music/awake10_megaWall.ogg", "MainMusic");

    if (mPlayMusic)
    {
        DEBUG("Play music");

        if (mMusicTrackId)
        {
             g_audio->resume(mMusicTrackId);
        }
        else
        {
            mMusicTrackId = g_audio->play(music, true);
        }
    }
    else
    {
        DEBUG("Stop music");

        if (mMusicTrackId)
        {
            g_audio->pause(mMusicTrackId);
        }
    }
#endif
}

} // invaders
