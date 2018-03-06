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
#  include <boost/random/mersenne_twister.hpp>
#include "warnpop.h"

#include <cmath>
#include <ctime>

#include "audio/sample.h"
#include "audio/player.h"
#include "base/logging.h"
#include "graphics/device.h"
#include "graphics/painter.h"
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

// generate a random number in the range of min max (inclusive)
template<typename T>
T rand(T min, T max)
{
    static boost::random::mt19937 generator;

    const auto range = max - min;
    const auto value = (double)generator() / (double)generator.max();
    return min + (range * value);
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

template<typename T>
T wrap(T max, T min, T val)
{
    if (val > max)
        return min;
    if (val < min)
        return max;
    return val;
}

template<typename T>
T clamp(T min, T val, T max)
{
    if (val < min)
        return min;
    if (val > max)
        return max;
    return val;
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

    // paint the user interface state
    virtual void paint(QPainter& painter, const QRectF& area, const QPointF& unit) = 0;

    // paint the user interface state with the custom painter
    virtual void paint(Painter& painter, const TransformState& transform) const
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

    //
    virtual void paint(QPainter& painter, TransformState& state) = 0;

protected:
private:
};

class GameWidget::Asteroid : public GameWidget::Animation
{
public:
    Asteroid(const QVector2D& direction)
    {
        mX = rand(0.0f, 1.0f);
        mY = rand(0.0f, 1.0f);
        mVelocity  = 0.08 + rand(0.0, 0.08);
        mScale     = rand(0.2, 0.8);
        mTexture   = rand(0, 2);
        mDirection = direction;
    }
    virtual bool update(float dt, TransformState& state) override
    {
        const auto d = mDirection * mVelocity * (dt / 1000.0f);
        mX = wrap(1.0f, -0.2f, mX + d.x());
        mY = wrap(1.0f, -0.2f, mY + d.y());
        return true;
    }
    virtual void paint(QPainter& painter, TransformState& state) override
    {
        static QPixmap textures[] = {
            QPixmap(R("textures/asteroid0.png")),
            QPixmap(R("textures/asteroid1.png")),
            QPixmap(R("textures/asteroid2.png"))
        };
        const QPixmap texture = textures[mTexture];
        const QRectF rect = state.viewRect();
        QRectF target(0.0f, 0.0f, texture.width() * mScale,
            texture.height() * mScale);

        const auto x = mX * rect.width();
        const auto y = mY * rect.height();
        target.moveTo(x, y);
        painter.drawPixmap(target, texture, texture.rect());
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
    Explosion(QVector2D position, float start, float lifetime) :
        position_(position), start_(start), life_(lifetime), time_(0), scale_(1.0)
    {}

    virtual bool update(float dt, TransformState& state) override
    {
        time_ += dt;
        if (time_ < start_)
            return true;

        if (time_ - start_ > life_)
            return false;

        return true;
    }
    virtual void paint(QPainter& painter, TransformState& state) override
    {
        if (time_ < start_)
            return;

        const auto unitScale = state.getScale();
        const auto position  = state.toViewSpace(position_);

        // explosion texture has 80 phases for the explosion
        const auto phase  = life_ / 80.0;
        const int  index  = (time_ - start_) / phase;

        const auto row = index / 10;
        const auto col = index % 10;

        // each explosion texture is 100x100px
        const auto w = 100;
        const auto h = 100;
        const auto x = col * w;
        const auto y = row * h;
        const QRect src(x, y, w, h);

        const auto scaledWidth  = unitScale.x() * scale_;
        const auto scaledHeight = unitScale.x() * scale_; // * ascpect;

        QRectF dst(0, 0, scaledWidth, scaledHeight);
        dst.moveTo(position -
            QPointF(scaledWidth / 2.0, scaledHeight / 2.0));

        QPixmap tex = Explosion::texture();
        painter.drawPixmap(dst, tex, src);
    }

    void setScale(float scale)
    { scale_ = scale; }

    QVector2D getPosition() const
    { return position_; }

private:
    static QPixmap texture()
    {
        static QPixmap px(R("textures/ExplosionMap.png"));
        return px;
    }

private:
    QVector2D position_;
    float start_;
    float life_;
    float time_;
private:
    float scale_;
};


// "fire" sparks emitter, high velocity  (needs texturing)
class GameWidget::Sparks : public GameWidget::Animation
{
public:
    Sparks(QVector2D position, float start, float lifetime)
        : start_(start), life_(lifetime), time_(0), color_(0xff, 0xff, 0xff)
    {
        const auto particles = 100;
        const auto angle = (M_PI * 2) / particles;

        for (int i=0; i<particles; ++i)
        {
            particle p;
            const auto r = (float)std::rand() / RAND_MAX;
            const auto v = (float)std::rand() / RAND_MAX;
            const auto a = i * angle + angle * r;
            p.dir.setX(std::cos(a));
            p.dir.setY(std::sin(a));
            p.dir *= v;
            p.pos  = position;
            p.a    = 0.8f;
            particles_.push_back(p);
        }
    }
    virtual bool update(float dt, TransformState&) override
    {
        time_ += dt;
        if (time_ < start_)
            return true;

        if (time_ - start_ > life_)
            return false;

        for (auto& p : particles_)
        {
            p.pos += p.dir * (dt / 2500.0);
            p.a = clamp(0.0, p.a - (dt / 2000.0), 1.0);
        }
        return true;
    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {
        if (time_ < start_)
            return;

        QColor color(color_);
        QBrush brush(color);
        for (const auto& particle : particles_)
        {
            color.setAlpha(0xff * particle.a);
            brush.setColor(color);
            const auto pos = state.toViewSpace(particle.pos);
            painter.fillRect(pos.x(),pos.y(), 2, 2, brush);
        }
    }

    void setColor(QColor color)
    { color_ = color; }

private:
    struct particle {
        QVector2D dir;
        QVector2D pos;
        float a;
    };
    std::vector<particle> particles_;
private:
    float start_;
    float life_;
    float time_;
private:
    QColor color_;
};

class GameWidget::Smoke : public GameWidget::Animation
{
public:
    Smoke(QVector2D position, float start, float lifetime)
        : position_(position), starttime_(start), lifetime_(lifetime), time_(0), scale_(1.0)
    {}

    virtual bool update(float dt, TransformState&) override
    {
        time_ += dt;
        if (time_ < starttime_)
            return true;

        if (time_ - starttime_ > lifetime_)
            return false;

        return true;
    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {
        if (time_ < starttime_)
            return;

        const auto unitScale = state.getScale();

        const auto fps = 10.0;
        const auto frames = 25;
        const auto frameInterval = (1000.0 / fps);
        const auto curr = (int)(time_ / frameInterval) % frames;
        const auto next = (curr + 1) % frames;
        const auto lerp = (fmodf(time_, frameInterval)) / frameInterval;

        const auto currPixmap = loadTexture(curr);
        const auto nextPixmap = loadTexture(next);

        const auto opacity = 1.0 * (1.0 - ((float)time_ / (float)lifetime_));
        const auto opa = painter.opacity();

        // note that the pixmaps are not necessarily equal size.
        {
            const auto aspect = (float)currPixmap.height() / (float)currPixmap.width();
            const auto pxw = unitScale.x() * scale_;
            const auto pxh = unitScale.x() * aspect * scale_;
            QRectF target(0, 0, pxw, pxh);
            target.moveTo(state.toViewSpace(position_) - QPointF(pxw/2.0, pxh/2.0));
            painter.setOpacity(opacity * (1.0 - lerp));
            painter.drawPixmap(target, currPixmap, currPixmap.rect());
        }
        {
            const auto aspect = (float)nextPixmap.height() / (float)nextPixmap.width();
            const auto pxw = unitScale.x() * scale_;
            const auto pxh = unitScale.x() * aspect * scale_;
            QRectF target(0, 0, pxw, pxh);
            target.moveTo(state.toViewSpace(position_) - QPointF(pxw/2.0, pxh/2.0));
            painter.setOpacity(opacity * lerp);
            painter.drawPixmap(target, nextPixmap, nextPixmap.rect());
        }
        painter.setOpacity(opa);
    }

    void setScale(float scale)
    { scale_ = scale; }

    static void prepare()
    {
        loadTexture(0);
    }
private:
    static std::vector<QPixmap> loadTextures()
    {
        std::vector<QPixmap> textures;
        for (int i=0; i<=24; ++i)
        {
            const auto name = QString("textures/smoke/blackSmoke%1.png").arg(i);
            textures.push_back(R(name));
        }
        return textures;
    }
    static QPixmap loadTexture(unsigned index)
    {
        static auto textures = loadTextures();
        Q_ASSERT(index < textures.size());
        return textures[index];
    }

private:
    QVector2D position_;
private:
    float starttime_;
    float lifetime_;
    float time_;
private:
    float scale_;
};


// slower moving debris, remnants of enemy. uses enemy texture as particle texture.
class GameWidget::Debris : public GameWidget::Animation
{
public:
    Debris(QPixmap texture, QVector2D position, float start, float lifetime)
        : texture_(texture), start_(start), life_(lifetime), time_(0), scale_(1.0)
    {
        const auto xparticles = 4;
        const auto yparticles = 2;

        const auto particleWidth  = texture.width() / xparticles;
        const auto particleHeight = texture.height() / yparticles;
        const auto numParticles   = xparticles * yparticles;

        const auto angle = (M_PI * 2) / numParticles;

        for (auto i=0; i<numParticles; ++i)
        {
            const auto col = i % xparticles;
            const auto row = i / xparticles;
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
            p.rotation_coefficient = rand(-1.0f, 1.0f);
            particles_.push_back(p);
        }
    }
    virtual bool update(float dt, TransformState&) override
    {
        time_ += dt;
        if (time_ < start_)
            return true;

        if  (time_ - start_ > life_)
            return false;

        for (auto& p : particles_)
        {
            p.pos += p.dir * (dt / 4500.0);
            p.alpha = clamp(0.0, p.alpha - (dt / 3000.0), 1.0);
            p.angle += ((M_PI * 2) * (dt / 2000.0) * p.rotation_coefficient);
        }

        return true;
    }
    virtual void paint(QPainter& painter, TransformState& state) override
    {
        if (time_ < start_)
            return;

        const auto spriteScale = state.getScale();

        for (const auto& p : particles_)
        {
            const auto pos = state.toViewSpace(p.pos);

            const float width  = p.rc.width();
            const float height = p.rc.height();
            const float aspect = height / width;
            const float scaledWidth  = spriteScale.x();
            const float scaledHeight = scaledWidth * aspect;

            QRect target(pos.x(), pos.y(), scaledWidth, scaledHeight);

            QTransform rotation;
            rotation.translate(pos.x() + scaledWidth / 2, pos.y() + scaledHeight / 2);
            rotation.rotateRadians(p.angle, Qt::ZAxis);
            rotation.translate(-pos.x() - scaledWidth / 2, -pos.y() - scaledHeight / 2);
            painter.setTransform(rotation);
            painter.setOpacity(p.alpha);
            painter.drawPixmap(target, texture_, p.rc);
        }
        painter.resetTransform();
        painter.setOpacity(1.0);
    }

    float getScale() const
    { return scale_; }

    void setScale(float f)
    { scale_ = f;}

private:
    struct particle {
        QRect rc;
        QVector2D dir;
        QVector2D pos;
        float angle = 0.0f;
        float alpha = 0.0f;
        float rotation_coefficient = 1.0f;
    };
    std::vector<particle> particles_;
private:
    QPixmap texture_;
    float start_;
    float life_;
    float time_;
private:
    float scale_;
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

    Invader(QVector2D position, QString str, float velocity, ShipType type) :
        position_(position), text_(str), time_(0), expire_(0), velocity_(velocity), type_(type)
    {
        shield_ = false;
    }

    void setPosition(QVector2D position)
    {
        position_ = position;
    }

    bool update(float dt, TransformState& state)
    {
        const auto v = getVelocityVector(state);
        position_ += (v * dt);
        if (expire_)
        {
            time_ += dt;
            if (time_ > expire_)
                return false;
        }
        return true;
    }

    float getScale() const
    {
        switch (type_)
        {
            case ShipType::Slow:  return 5.0f;
            case ShipType::Fast:  return 4.0f;
            case ShipType::Boss:  return 6.5f;
            case ShipType::Tough: return 3.5f;
        }
        return 1.0f;
    }

    void paint(QPainter& painter, TransformState& state)
    {
        // offset the texture to be centered around the position
        const auto unitScale   = state.getScale();
        const auto spriteScale = state.getScale() * getScale();
        const auto position    = state.toViewSpace(position_);

        // draw the ship texture
        QPixmap ship = getShipTexture(type_);
        const float shipWidth  = ship.width();
        const float shipHeight = ship.height();
        const float shipAspect = shipHeight / shipWidth;
        const float shipScaledWidth  = spriteScale.x();
        const float shipScaledHeight = shipScaledWidth * shipAspect;

        // draw the jet stream first
        QPixmap jet = getJetStreamTexture(type_);
        const float jetWidth  = jet.width();
        const float jetHeight = jet.height();
        const float jetAspect = jetHeight / jetWidth;
        const float jetScaledWidth  = spriteScale.x();
        const float jetScaledHeight = jetScaledWidth * jetAspect;

        // set the target rectangle with the dimensions of the
        // sprite we want to draw.
        QRectF target(0.0f, 0.0f, shipScaledWidth, shipScaledHeight);
        // offset it so that the center is aligned with the unit position
        target.moveTo(position -
            QPointF(shipScaledWidth / 2.0, shipScaledHeight / 2.0));
        //painter.draw(target, ship, ship.rect()); // draw the jet stream first.

        QRectF shipRect = target;

        target.translate(shipScaledWidth*0.6, (shipScaledHeight - jetScaledHeight) / 2.0);
        target.setSize(QSize(jetScaledWidth, jetScaledHeight));
        painter.drawPixmap(target, jet, jet.rect());
        target.translate(jetScaledWidth*0.75, 0);

        painter.drawPixmap(shipRect, ship, ship.rect());

        // draw the kill string
        QFont font;
        font.setFamily("Monospace");
        font.setPixelSize(unitScale.y() / 1.75);

        QPen pen;
        pen.setWidth(2);
        pen.setColor(Qt::darkYellow);
        painter.setFont(font);
        painter.setPen(pen);
        painter.drawText(target, Qt::AlignVCenter, text_);

        if (shield_)
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

    // get current position
    QVector2D getPosition() const
    {
        return position_;
    }

    // get the position at ticksLater ticks time
    QVector2D getFuturePosition(float dtLater, const TransformState& state) const
    {
        const auto v = getVelocityVector(state);
        return position_ + v * dtLater;
    }

    void expireIn(quint64 ms)
    { expire_ = ms; }

    void setViewString(QString str)
    {
        text_ = str;
    }
    QPixmap getTexture() const
    { return getShipTexture(type_); }

    void setShield(bool onOff)
    { shield_ = onOff; }

private:
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
        const auto velo = QVector2D(-x, y) * velocity_;
        return velo;
    }

private:
    QVector2D position_;
    QString text_;
    float time_;
    float expire_;
private:
    float velocity_;
private:
    ShipType type_;
private:
    bool shield_;
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
        position_.setX(rand(0.0, 1.0));
        position_.setY(rand(0.0, 1.0));

        const auto x = rand(-1.0, 1.0);
        const auto y = rand(-1.0, 1.0);
        direction_ = QVector2D(x, y);
        direction_.normalize();
    }

    virtual bool update(float dt, TransformState& state) override
    {
        runtime_ += dt;
        if (runtime_ > lifetime_)
            return false;

        QVector2D fuzzy;
        fuzzy.setY(std::sin((fmodf(runtime_, 3000) / 3000.0) * 2 * M_PI));
        fuzzy.setX(direction_.x());
        fuzzy.normalize();

        position_ += (dt / 10000.0) * fuzzy;
        const auto x = position_.x();
        const auto y = position_.y();
        position_.setX(wrap(1.0f, 0.0f, x));
        position_.setY(wrap(1.0f, 0.0f, y));
        return true;
    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {
        const auto phase = 1000 / 10.0;
        const auto index = unsigned(runtime_ / phase) % 6;

        const auto pixmap = loadTexture(index);

        const auto pxw = pixmap.width();
        const auto pxh = pixmap.height();
        //const auto aspect = (float)pxh / (float)pxw;

        QRectF target(0, 0, pxw, pxh);
        target.moveTo(state.toViewSpace(position_));

        const auto opa = painter.opacity();

        painter.setOpacity(1.0);
        painter.drawPixmap(target, pixmap, pixmap.rect());
        painter.setOpacity(opa);

    }
    static void prepare()
    {
        loadTexture(0);
    }

    static bool shouldMakeRandomAppearance()
    {
        if (rand(0, 5000) == 7)
            return true;
        return false;
    }
private:
    static std::vector<QPixmap> loadTextures()
    {
        std::vector<QPixmap> v;
        for (int i=1; i<=6; ++i)
        {
            const auto name = QString("textures/alien/e_f%1").arg(i);
            v.push_back(R(name));
        }
        return v;
    }
    static QPixmap loadTexture(unsigned index)
    {
        static auto textures = loadTextures();
        Q_ASSERT(index < textures.size());
        return textures[index];
    }
private:
    float lifetime_ = 10000.0f;
    float runtime_  = 0.0f;
private:
    QVector2D direction_;
    QVector2D position_;
};


class GameWidget::BigExplosion : public GameWidget::Animation
{
public:
    BigExplosion(float lifetime) : lifetime_(lifetime), time_(0)
    {}

    virtual bool update(float dt, TransformState& state) override
    {
        time_ += dt;
        if (time_ > lifetime_)
            return false;
        return true;
    }
    virtual void paint(QPainter& painter, TransformState& state) override
    {
        const auto phase  = lifetime_ / 90.0;
        const int  index  = time_ / phase;
        if (index >= 90)
            return;
        const auto pixmap = loadTexture(index);

        const auto pxw = pixmap.width();
        const auto pxh = pixmap.height();
        const auto aspect = (float)pxh / (float)pxw;

        const auto numExplosions = 3;

        const auto explosionWidth  = state.viewWidth();
        const auto explosionHeight = explosionWidth * aspect;
        const auto xpos = state.viewWidth() / (numExplosions + 1);
        const auto ypos = (state.viewHeight() - explosionHeight) / 2;

        auto yoffset = 50.0;
        auto xoffset = -explosionWidth / 2.0;
        for (auto i=0; i<numExplosions; ++i)
        {
            painter.drawPixmap((i+1) * xpos + xoffset, ypos + yoffset, explosionWidth, explosionHeight, pixmap);
            yoffset *= -1;
        }
    }

    static void prepare()
    {
        loadTexture(0);
    }

private:
    static std::vector<QPixmap> loadTextures()
    {
        std::vector<QPixmap> v;
        for (int i=1; i<=90; ++i)
        {
            const auto name = QString("textures/bomb/explosion1_00%1.png").arg(i);
            v.push_back(R(name));
        }
        return v;
    }
    static QPixmap loadTexture(unsigned index)
    {
        static auto textures = loadTextures();
        Q_ASSERT(index < textures.size());
        return textures[index];
    }
private:
    float lifetime_;
    float time_;
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
        for (std::size_t i=0; i<800; ++i)
        {
            particle p;
            p.x = rand(0.0, 1.0);
            p.y = rand(0.0, 1.0);
            p.a = 0.5 + rand(0.0, 0.5);
            p.v = 0.05 + rand(0.0, 0.05);
            p.b = !(i % 7);
            mParticles.push_back(p);
        }
        mBackground = R("textures/SpaceBackground.png");
        mDirection  = direction;
    }
    void paint(QPainter& painter, const QRectF& rect, const QPointF&)
    {
        // draw the space background.
        QBrush space(Qt::black);
        painter.fillRect(rect, space);
        painter.drawPixmap(rect, mBackground, mBackground.rect());

        // draw the little stars/particles
        QBrush star(Qt::white);
        QColor col(Qt::white);

        for (const auto& p : mParticles)
        {
            col.setAlpha(p.a * 0xff);
            star.setColor(col);
            const auto x = p.x * rect.width();
            const auto y = p.y * rect.height();
            painter.fillRect(x, y, 1 + p.b, 1 + p.b, star);
        }
    }
    void update(float dt)
    {
        const auto d = mDirection * (dt / 1000.0);
        for (auto& p : mParticles)
        {
            p.x = wrap(1.0f, 0.0f, p.x + d.x() * p.v);
            p.y = wrap(1.0f, 0.0f, p.y + d.y() * p.v);
        }
    }
private:
    struct particle {
        float x = 0.0f;
        float y = 0.0f;
        float a = 0.0f;;
        float v = 0.0f;;
        int   b = 0;
    };
    std::vector<particle> mParticles;
    QVector2D mDirection;
    QPixmap mBackground;
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

    virtual void paint(Painter& painter, const TransformState& parentTransform) const override
    {
        const auto cols = 7;
        const auto rows = 6;
        TransformState myTransform(parentTransform.viewRect(), cols, rows);

        QRectF rc = myTransform.toViewSpaceRect(QPoint(3, 4), QPoint(4, 5));
        const auto x = rc.x();
        const auto y = rc.y();
        const auto w = rc.width();
        const auto h = rc.height();

        Transform dt, mt;
        dt.MoveTo(x, y);
        dt.Resize(w, h);

        mt.MoveTo(x, y+2);
        mt.Resize(w, h-4);

        SlidingGlintEffect effect;
        effect.SetAppRuntime(mTotalTimeRun/1000.0f);
        painter.DrawMasked(Rect(), dt, Rect(), mt, effect);
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
                mCurrentProfileIndex = wrap(numProfilesMax, numProfilesMin, mCurrentProfileIndex - 1);
            }
            else
            {
                mCurrentLevelIndex = wrap(numLevelsMax, numLevelsMin, mCurrentLevelIndex -1);
            }
            bPlaySound = true;
        }
        else if (key == Qt::Key_Right)
        {
            if (mCurrentRowIndex == 0)
            {
                mCurrentProfileIndex = wrap(numProfilesMax, numProfilesMin,  mCurrentProfileIndex + 1);
            }
            else
            {
                mCurrentLevelIndex = wrap(numLevelsMax, numLevelsMin, mCurrentLevelIndex + 1);
            }
            bPlaySound = true;
        }
        else if (key == Qt::Key_Up)
        {
            mCurrentRowIndex = wrap(1, 0, mCurrentRowIndex - 1);
        }
        else if (key == Qt::Key_Down)
        {
            mCurrentRowIndex = wrap(1, 0, mCurrentRowIndex + 1);
        }

        if (bPlaySound && mPlaySounds)
        {
        #ifdef ENABLE_AUDIO
            static auto swoosh = std::make_shared<AudioSample>(R("sounds/Slide_Soft_00.ogg"), "swoosh");
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

#ifndef ENABLE_AUDIO
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
                "(c) 2014-2016 Ensisoft\n"
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
    PlayGame(const Game::setup& setup, Level& level, Game& game) : mSetup(setup), mLevel(level), mGame(game)
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
                mGame.play(&mLevel, mSetup);
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
                    Game::bomb bomb;
                    mGame.ignite(bomb);
                    mCurrentText.clear();
                }
                else if (mCurrentText == "WARP")
                {
                    Game::timewarp warp;
                    warp.duration = 4000;
                    warp.factor = 0.2f;
                    mGame.enter(warp);
                    mCurrentText.clear();
                }
                else
                {
                    Game::missile missile;
                    missile.position = mMissileLaunchPosition; // todo: fix this
                    missile.string   = mCurrentText.toLower();
                    if (mGame.fire(missile))
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
        const auto& score  = mGame.getScore();
        const auto& result = score.maxpoints ?
            (float)score.points / (float)score.maxpoints * 100 : 0.0;
        const auto& format = QString("%1").arg(result, 0, 'f', 0);
        const auto bombs   = mGame.numBombs();
        const auto warps   = mGame.numWarps();

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
    const Game::setup mSetup;
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
#ifdef ENABLE_AUDIO
    // sound effects FX
    static auto sndExplosion = std::make_shared<AudioSample>(R("sounds/explode.wav"), "explosion");
#endif

    QFontDatabase::addApplicationFont(R("fonts/ARCADE.TTF"));

    BigExplosion::prepare();
    Smoke::prepare();
    UFO::prepare();

    mGame.reset(new Game(GameCols, GameRows));

    mGame->onMissileKill = [&](const Game::invader& i, const Game::missile& m, unsigned killScore)
    {
        auto it = mInvaders.find(i.identity);

        TransformState state(rect(), ViewCols, ViewRows);

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

        invader->expireIn(missileFlyTime);
        explosion->setScale(invader->getScale() * 1.5);
        smoke->setScale(invader->getScale() * 2.5);
        sparks->setColor(QColor(255, 255, 68));

        mAnimations.push_back(std::move(invader));
        mAnimations.push_back(std::move(missile));
        mAnimations.push_back(std::move(smoke));
        mAnimations.push_back(std::move(debris));
        mAnimations.push_back(std::move(sparks));
        mAnimations.push_back(std::move(explosion));
        mAnimations.push_back(std::move(score));

        mInvaders.erase(it);

#ifdef ENABLE_AUDIO
        if (mPlaySounds)
        {
            g_audio->play(sndExplosion, std::chrono::milliseconds(missileFlyTime));
        }
#endif
    };

    mGame->onMissileDamage = [&](const Game::invader& i, const Game::missile& m)
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

        sparks->setColor(Qt::darkGray);

        auto viewString = i.viewList.join("");
        inv->setViewString(viewString);

        mAnimations.push_back(std::move(missile));
        mAnimations.push_back(std::move(sparks));
    };

    mGame->onMissileFire = mGame->onMissileDamage;

    mGame->onBombKill = [&](const Game::invader& i, const Game::bomb& b, unsigned killScore)
    {
        auto it = mInvaders.find(i.identity);
        auto& inv = it->second;

        std::unique_ptr<Animation> explosion(new Explosion(inv->getPosition(), 0, 1000));
        std::unique_ptr<Animation> score(new Score(inv->getPosition(), 1000, 2000, killScore));
        mAnimations.push_back(std::move(explosion));
        mAnimations.push_back(std::move(score));

        mInvaders.erase(it);
    };

    mGame->onBombDamage = [&](const Game::invader& i, const Game::bomb& b)
    {
        auto it = mInvaders.find(i.identity);
        auto& inv = it->second;
        auto str = i.viewList.join("");
        inv->setViewString(str);
    };

    mGame->onBomb = [&](const Game::bomb& b)
    {
        std::unique_ptr<Animation> explosion(new BigExplosion(1500));


        mAnimations.push_back(std::move(explosion));
    };

    mGame->onWarp = [&](const Game::timewarp& w)
    {
        DEBUG("begin time warp");
        mWarpFactor    = w.factor;
        mWarpRemaining = w.duration;
    };

    mGame->onToggleShield = [&](const Game::invader& i, bool onOff)
    {
        auto it = mInvaders.find(i.identity);
        auto& inv = it->second;
        inv->setShield(onOff);
    };

    mGame->onInvaderSpawn = [&](const Game::invader& inv)
    {
        TransformState state(rect(), ViewCols, ViewRows);

        const auto pos = state.toNormalizedViewSpace(GameSpace{inv.xpos, inv.ypos+1});

        Invader::ShipType type = Invader::ShipType::Slow;
        if (inv.type == Game::InvaderType::boss)
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
        invader->setShield(inv.shield_on_ticks != 0);
        mInvaders[inv.identity] = std::move(invader);
    };


    mGame->onInvaderVictory = [&](const Game::invader& inv)
    {
        mInvaders.erase(inv.identity);
    };

    // invader is almost escaping unharmed. we help the player to learn
    // by changing the text from chinese to the pinyin kill string
    mGame->onInvaderWarning = [&](const Game::invader& inv)
    {
        auto it  = mInvaders.find(inv.identity);
        auto str = inv.killList.join("");
        it->second->setViewString(str);
    };

    mGame->onLevelComplete = [&](const Game::score& score)
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
            mGame->tick();

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
    mCustomGraphicsDevice  = GraphicsDevice::Create(GraphicsDevice::Type::OpenGL_ES2);
    mCustomGraphicsPainter = Painter::Create(mCustomGraphicsDevice);
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

    mBackground->paint(painter, rect(), state.getScale());

    // paint animations
    for (auto& anim : mAnimations)
    {
        anim->paint(painter, state);
    }

    // paint the invaders
    const bool bIsGameRunning = mStates.top()->isGameRunning();
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

    // do a second pass painter using the custom painter.
    // since we're drawing using the same OpenGL context the
    // state management is somewhat tricky.
    painter.beginNativePainting();

    GraphicsDevice::StateBuffer currentState;
    mCustomGraphicsDevice->GetState(&currentState);
    mCustomGraphicsPainter->SetViewport(0, 0, width(), height());

    mStates.top()->paint(*mCustomGraphicsPainter, state);

    mCustomGraphicsDevice->SetState(currentState);
    painter.endNativePainting();


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

                Game::setup setup;
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
                    mGame->quitLevel();
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
#ifdef ENABLE_AUDIO
    static auto music = std::make_shared<AudioSample>(R("music/awake10_megaWall.ogg"), "MainMusic");

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
