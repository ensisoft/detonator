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
#  include <QtGui/QApplication>
#  include <QResource>
#  include <QFileInfo>
#  include <QtDebug>
#  include <boost/random/mersenne_twister.hpp>
#include "warnpop.h"
#include <cmath>
#include <ctime>
#include "warnpush.h"
#  include "gamewidget.h"
#include "warnpop.h"
#include "game.h"
#include "level.h"

#ifdef ENABLE_AUDIO
#  include "audio.h"
namespace invaders {
  extern AudioPlayer* g_audio;
}
#endif

namespace invaders
{



const auto LevelUnlockCriteria = 0.85;

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
    TransformState(const QRect& window, const Game& game)
    {
        // we divide the widget's client area into a equal sized cells
        // according to the game's size. We also add one extra row
        // for HUD display at the top of the screen and for the player
        // at the bottom of the screen. This provides the basic layout
        // for the game in a way that doesnt depend on any actual viewport size.
        const auto numCols = game.width();
        const auto numRows = game.height() + 2;
        // divide the widget's client area int equal sized cells
        origin_ = QPoint(window.x(), window.y());
        widget_ = QPoint(window.width(), window.height());
        scale_  = QPoint(window.width() / numCols, window.height() / numRows);
        size_   = QPoint(numCols, numRows);
    }

    TransformState(const QRect& window, int numCols, int numRows)
    {
        // divide the widget's client area int equal sized cells
        origin_ = QPoint(window.x(), window.y());
        widget_ = QPoint(window.width(), window.height());
        scale_  = QPoint(window.width() / numCols, window.height() / numRows);
        size_   = QPoint(numCols, numRows);
    }

    QRect toViewSpaceRect(const QPoint& gameTopLeft, const QPoint& gameTopBottom) const
    {
        const QPoint top = toViewSpace(gameTopLeft);
        const QPoint bot = toViewSpace(gameTopBottom);
        return {top, bot};
    }

    QPoint toViewSpace(const QPoint& cell) const
    {
        const int xpos = cell.x() * scale_.x() + origin_.x();
        const int ypos = cell.y() * scale_.y() + origin_.y();
        return { xpos, ypos };
    }

    QPoint toViewSpace(const QVector2D& norm) const
    {
        const int xpos = widget_.x() * norm.x() + origin_.x();
        const int ypos = widget_.y() * norm.y() + origin_.y();
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

    QPoint getScale() const
    {
        return scale_;
    }

    QVector2D getNormalizedScale() const
    {
        const auto scale  = getScale();
        const auto width  = (float)viewWidth();
        const auto height = (float)viewHeight();
        return {scale.x() / width, scale.y() / height};
    }

    // get whole widget rect in widget coordinates
    QRect viewRect() const
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
    QPoint origin_;
    QPoint widget_;
    QPoint scale_;
    QPoint size_;
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




class GameWidget::Animation
{
public:
    virtual ~Animation() = default;

    // returns true if animation is still valid
    // otherwise false and the animation is expired.
    virtual bool update(quint64 dt, TransformState& state) = 0;

    //
    virtual void paint(QPainter& painter, TransformState& state) = 0;

protected:
private:
};

// Flame/Smoke emitter
class GameWidget::Explosion : public GameWidget::Animation
{
public:
    Explosion(QVector2D position, quint64 start, quint64 lifetime) :
        position_(position), start_(start), life_(lifetime), time_(0), scale_(1.0)
    {}

    virtual bool update(quint64 dt, TransformState& state) override
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

        QRect dst(0, 0, scaledWidth, scaledHeight);
        dst.moveTo(position -
            QPoint(scaledWidth / 2.0, scaledHeight / 2.0));

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
    quint64 start_;
    quint64 life_;
    quint64 time_;
private:
    float scale_;
};


// "fire" sparks emitter, high velocity  (needs texturing)
class GameWidget::Sparks : public GameWidget::Animation
{
public:
    Sparks(QVector2D position, quint64 start, quint64 lifetime)
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
    virtual bool update(quint64 dt, TransformState&) override
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
    quint64 start_;
    quint64 life_;
    quint64 time_;
private:
    QColor color_;
};

class GameWidget::Smoke : public GameWidget::Animation
{
public:
    Smoke(QVector2D position, quint64 start, quint64 lifetime)
        : position_(position), starttime_(start), lifetime_(lifetime), time_(0), scale_(1.0)
    {}

    virtual bool update(quint64 dt, TransformState&) override
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

        const auto fps = 10;
        const auto frames = 25;
        const auto frameInterval = (1000 / fps);
        const auto curr = (time_ / frameInterval) % frames;
        const auto next = (curr + 1) % frames;
        const auto lerp = (time_ % frameInterval) / (float)frameInterval;

        const auto currPixmap = loadTexture(curr);
        const auto nextPixmap = loadTexture(next);

        const auto opacity = 1.0 * (1.0 - ((float)time_ / (float)lifetime_));
        const auto opa = painter.opacity();

        // note that the pixmaps are not necessarily equal size.
        {
            const auto aspect = (float)currPixmap.height() / (float)currPixmap.width();
            const auto pxw = unitScale.x() * scale_;
            const auto pxh = unitScale.x() * aspect * scale_;
            QRect target(0, 0, pxw, pxh);
            target.moveTo(state.toViewSpace(position_) - QPoint(pxw/2.0, pxh/2.0));
            painter.setOpacity(opacity * (1.0 - lerp));
            painter.drawPixmap(target, currPixmap, currPixmap.rect());
        }
        {
            const auto aspect = (float)nextPixmap.height() / (float)nextPixmap.width();
            const auto pxw = unitScale.x() * scale_;
            const auto pxh = unitScale.x() * aspect * scale_;
            QRect target(0, 0, pxw, pxh);
            target.moveTo(state.toViewSpace(position_) - QPoint(pxw/2.0, pxh/2.0));
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
    quint64 starttime_;
    quint64 lifetime_;
    quint64 time_;
private:
    float scale_;
};


// slower moving debris, remnants of enemy. uses enemy texture as particle texture.
class GameWidget::Debris : public GameWidget::Animation
{
public:
    Debris(QPixmap texture, QVector2D position, quint64 start, quint64 lifetime)
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
            particles_.push_back(p);
        }
    }
    virtual bool update(quint64 dt, TransformState&) override
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
            p.angle += (M_PI * 2) * (dt / 2000.0);
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
        float angle;
        float alpha;
    };
    std::vector<particle> particles_;
private:
    QPixmap texture_;
    quint64 start_;
    quint64 life_;
    quint64 time_;
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
    {}

    void setPosition(QVector2D position)
    {
        position_ = position;
    }

    bool update(quint64 dt, TransformState& state)
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
        QRect target(0, 0, shipScaledWidth, shipScaledHeight);
        // offset it so that the center is aligned with the unit position
        target.moveTo(position -
            QPoint(shipScaledWidth / 2.0, shipScaledHeight / 2.0));
        //painter.draw(target, ship, ship.rect()); // draw the jet stream first.

        QRect shipRect = target;

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
    quint64 time_;
    quint64 expire_;
private:
    float velocity_;
private:
    ShipType type_;
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

    virtual bool update(quint64 dt, TransformState& state) override
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
        QRect rect;
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
    quint64 life_;
    quint64 time_;
    QString text_;
};

class GameWidget::Alien : public GameWidget::Animation
{
public:
    Alien() : lifetime_(10000), time_(0)
    {
        position_.setX(rand(0.0, 1.0));
        position_.setY(rand(0.0, 1.0));

        const auto x = rand(-1.0, 1.0);
        const auto y = rand(-1.0, 1.0);
        direction_ = QVector2D(x, y);
        direction_.normalize();
    }

    virtual bool update(quint64 dt, TransformState& state) override
    {
        time_ += dt;
        if (time_ > lifetime_)
            return false;

        QVector2D fuzzy;
        fuzzy.setY(std::sin(((time_ % 3000) / 3000.0) * 2 * M_PI));
        fuzzy.setX(direction_.x());
        fuzzy.normalize();

        position_ += (dt / 10000.0) * fuzzy;
        const auto x = position_.x();
        const auto y = position_.y();
        position_.setX(wrap(1.0, 0.0, x));
        position_.setY(wrap(1.0, 0.0, y));
        return true;
    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {
        const auto phase = 1000 / 10;
        const auto index = (time_ / phase) % 6;

        const auto pixmap = loadTexture(index);

        const auto pxw = pixmap.width();
        const auto pxh = pixmap.height();
        //const auto aspect = (float)pxh / (float)pxw;

        QRect target(0, 0, pxw, pxh);
        target.moveTo(state.toViewSpace(position_));

        const auto opa = painter.opacity();

        painter.setOpacity(0.5);
        painter.drawPixmap(target, pixmap, pixmap.rect());
        painter.setOpacity(opa);

    }
    static void prepare()
    {
        loadTexture(0);
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
    quint64 lifetime_;
    quint64 time_;
private:
    QVector2D direction_;
    QVector2D position_;
};


class GameWidget::BigExplosion : public GameWidget::Animation
{
public:
    BigExplosion(quint64 lifetime) : lifetime_(lifetime), time_(0)
    {}

    virtual bool update(quint64 dt, TransformState& state) override
    {
        time_ += dt;
        if (time_ > lifetime_)
            return false;
        return true;
    }
    virtual void paint(QPainter& painter, TransformState& state) override
    {
        const auto phase  = lifetime_ / 90.0;
        const auto index  = time_ / phase;
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
    quint64 lifetime_;
    quint64 time_;
};

class GameWidget::Score : public GameWidget::Animation
{
public:
    Score(QVector2D position, quint64 start, quint64 lifetime, unsigned score) :
        position_(position), start_(start), life_(lifetime), time_(0), score_(score)
    {}

    virtual bool update(quint64 dt, TransformState& state) override
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
        painter.drawText(QRect(top, end), QString("%1").arg(score_));
    }
private:
    QVector2D position_;
    quint64 start_;
    quint64 life_;
    quint64 time_;
    unsigned score_;
};



// game space background rendering
class GameWidget::Background
{
public:
    Background() : texture_(R("textures/SpaceBackground.png"))
    {
        //std::srand(std::time(nullptr));

        direction_ = QVector2D(4, 3);
        direction_.normalize();

        for (std::size_t i=0; i<800; ++i)
        {
            particle p;
            p.x = rand(0.0, 1.0);
            p.y = rand(0.0, 1.0);
            p.a = 0.5 + rand(0.0, 0.5);
            p.v = 0.05 + rand(0.0, 0.05);
            p.b = !(i % 7);
            particles_.push_back(p);
        }
    }

    void paint(QPainter& painter, const QRect& rect, const QPoint&)
    {
        QBrush space(Qt::black);
        painter.fillRect(rect, space);
        painter.drawPixmap(rect, texture_, texture_.rect());

        QBrush star(Qt::white);
        QColor col(Qt::white);

        for (const auto& p : particles_)
        {
            col.setAlpha(p.a * 0xff);
            star.setColor(col);
            const auto x = p.x * rect.width();
            const auto y = p.y * rect.height();
            painter.fillRect(x, y, 1 + p.b, 1 + p.b, star);
        }
    }
    void update(quint64 time)
    {
        const auto d = direction_ * (time / 1000.0);
        for (auto& p : particles_)
        {
            p.x = wrap(1.0, 0.0, p.x + d.x() * p.v);
            p.y = wrap(1.0, 0.0, p.y + d.y() * p.v);
        }
    }
private:
    struct particle {
        float x, y;
        float a;
        float v;
        int   b;
    };
    std::vector<particle> particles_;
private:
    QVector2D direction_;
    QPixmap texture_;
};

class GameWidget::Scoreboard
{
public:
    Scoreboard(unsigned score, unsigned bonus, bool isHighScore, int unlockedLevel)
    {
        text_.append("Level complete!\n\n");
        text_.append(QString("You scored %1 points\n").arg(score));
        text_.append(QString("Difficulty bonus %1 points\n").arg(bonus));
        text_.append(QString("Total %1 points\n\n").arg(score + bonus));

        if (isHighScore)
            text_.append("New high score!\n");

        if (unlockedLevel)
            text_.append(QString("Level %1 unlocked!\n").arg(unlockedLevel + 1));

        text_.append("\nPress Space to continue");
    }

    void paint(QPainter& painter, const QRect& area, const QPoint& scale)
    {
        QPen pen;
        pen.setWidth(1);
        pen.setColor(Qt::darkGray);

        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(scale.y() / 2);

        painter.setPen(pen);
        painter.setFont(font);
        painter.drawText(area, Qt::AlignCenter, text_);
    }
private:
    QString text_;
};

// Heads Up Display, display scoreboards etc.
class GameWidget::Display
{
public:
    Display(const Game& game) : game_(game), level_(0)
    {}
    void paint(QPainter& painter, const QRect& area, const QPoint& scale)
    {
        const auto& score  = game_.getScore();
        const auto& result = score.maxpoints ?
            (float)score.points / (float)score.maxpoints * 100 : 0.0;
        const auto& format = QString("%1").arg(result, 0, 'f', 0);
        const auto bombs   = game_.numBombs();
        const auto warps   = game_.numWarps();

        QPen pen;
        pen.setColor(Qt::darkGreen);
        pen.setWidth(1);
        painter.setPen(pen);

        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(scale.y() / 2);
        painter.setFont(font);

        painter.drawText(area, Qt::AlignCenter,
            QString("Level %1 Score %2 (%3%) | Enemies x %4 | Bombs x %5 | Warps x %6 | (F1 for help)")
                .arg(level_)
                .arg(score.points)
                .arg(format)
                .arg(score.pending)
                .arg(bombs)
                .arg(warps));
    }
    void setLevel(unsigned number)
    { level_ = number; }

private:
    const Game& game_;
    unsigned level_;
};

// player representation on the screen
class GameWidget::Player
{
public:
    Player() : text_(initString())
    {}

    void paint(QPainter& painter, const QRect& area, const TransformState& transform)
    {
        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(area.height() / 2);
        painter.setFont(font);

        QFontMetrics fm(font);
        const auto width  = fm.width(text_);
        const auto height = fm.height();

        // calculate text X, Y (top left)
        const auto x = area.x() + ((area.width() - width) / 2.0);
        const auto y = area.y() + ((area.height() - height) / 2.0);

        if (false)
        {
            QPen pen;
            pen.setWidth(2);
            pen.setColor(Qt::darkRed);
            painter.setPen(pen);
            painter.drawText(area, Qt::AlignCenter | Qt::AlignVCenter, text_);
        }

        QPen pen;
        pen.setWidth(2);
        pen.setColor(Qt::darkGray);
        painter.setPen(pen);

        QRect rect(QPoint(x, y), QPoint(x + width, y + height));
        painter.drawRect(rect);
        painter.drawText(rect, Qt::AlignCenter | Qt::AlignVCenter, text_);

        // save the missile launch position
        missile_ = transform.toNormalizedViewSpace(QPoint(x, y));
    }

    void keyPress(QKeyEvent* press, Game& game)
    {
        if (text_ == initString())
            text_.clear();

        const auto key = press->key();
        if (key == Qt::Key_Backspace)
        {
            if (!text_.isEmpty())
                text_.resize(text_.size()-1);
        }
        else if (key >= 0x41 && key <= 0x5a)
        {
            text_.append(key);
            if (text_ == "BOMB")
            {
                Game::bomb b;
                game.ignite(b);
                text_.clear();
            }
            else if (text_ == "WARP")
            {
                Game::timewarp w;
                w.duration = 4000;
                w.factor   = 0.2f;
                game.enter(w);
                text_.clear();
            }
            else
            {
                Game::missile m;
                m.position = missile_;
                m.string = text_.toLower();
                if (game.fire(m))
                    text_.clear();

            }
        }
    }
    void reset()
    {
        text_ = initString();
    }
private:
    static
    QString initString()
    {
        static const QString init {
            "Type the correct pinyin to kill the enemies!"
        };
        return init;
    }
    QString text_;
    QVector2D missile_;
};


// initial greeting and instructions
class GameWidget::Menu
{
public:
    Menu(const std::vector<std::unique_ptr<Level>>& levels,
         const std::vector<GameWidget::LevelInfo>& infos,
         const std::vector<GameWidget::Profile>& profiles)
    : levels_(levels), infos_(infos), profiles_(profiles), level_(0), profile_(0), selrow_(1)
    {}

    void update(quint64 time)
    {}

    void paint(QPainter& painter, const QRect& area, const QPoint& unit)
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

        QRect rect;

        rect = state.toViewSpaceRect(QPoint(0, 0), QPoint(cols, 3));
        painter.drawText(rect, Qt::AlignHCenter | Qt::AlignBottom,
            "Evil chinese characters are attacking!\n"
            "Only you can stop them by typing the right pinyin.\n"
            "Good luck.\n\n"
//
            "Enter - Fire\n"
            "Esc - Exit\n"
            "F1 - Help\n"
            "F2 - Settings\n"
            "F3 - About\n\n"
            "Difficulty\n");

        rect = state.toViewSpaceRect(QPoint(2, 3), QPoint(5, 4));



        TransformState sub(rect, 3, 1);
        rect = sub.toViewSpaceRect(QPoint(0, 0), QPoint(1, 1));
        if (profile_ == 0)
        {
            painter.setFont(underline);
            if (selrow_ == 0)
                painter.setPen(selected);
        }

        painter.drawText(rect, Qt::AlignTop | Qt::AlignRight, "Easy");
        painter.setPen(regular);
        painter.setFont(font);

        rect = sub.toViewSpaceRect(QPoint(1, 0), QPoint(2, 1));
        if (profile_ == 1)
        {
            painter.setFont(underline);
            if (selrow_ == 0)
                painter.setPen(selected);
        }

        painter.drawText(rect, Qt::AlignTop | Qt::AlignHCenter, "Normal");
        painter.setPen(regular);
        painter.setFont(font);

        rect = sub.toViewSpaceRect(QPoint(2, 0), QPoint(3, 1));
        if (profile_ == 2)
        {
            painter.setFont(underline);
            if (selrow_ == 0)
                painter.setPen(selected);
        }

        painter.drawText(rect, Qt::AlignTop | Qt::AlignLeft, "Chinese");
        painter.setPen(regular);
        painter.setFont(font);

        QFont small;
        small.setFamily("Arcade");
        small.setPixelSize(unit.y() / 2.5);
        painter.setFont(small);

        const auto prev = level_ > 0 ? level_ - 1 : levels_.size() - 1;
        const auto next = (level_ + 1) % levels_.size();
        const auto& left  = levels_[prev];
        const auto& mid   = levels_[level_];
        const auto& right = levels_[next];

        rect = state.toViewSpaceRect(QPoint(1, 4), QPoint(2, 5));
        drawLevel(painter, rect, *left, prev, false);

        bool hilite = false;
        if (selrow_ == 1)
        {
            if (infos_[level_].locked)
                 painter.setPen(locked);
            else painter.setPen(selected);
            hilite = true;
        }
        else
        {
            painter.setPen(regular);
        }

        rect = state.toViewSpaceRect(QPoint(3, 4), QPoint(4, 5));
        drawLevel(painter, rect, *mid, level_, hilite);

        painter.setPen(regular);
        rect = state.toViewSpaceRect(QPoint(5, 4), QPoint(6, 5));
        painter.drawRect(rect);
        drawLevel(painter, rect, *right, next, false);


        rect = state.toViewSpaceRect(QPoint(0, rows-1), QPoint(cols, rows));
        painter.setPen(regular);
        painter.setFont(font);
        painter.drawText(rect, Qt::AlignCenter, "Press Space to play!\n");
    }
    void keyPress(QKeyEvent* press)
    {
        const int numLevelsMin = 0;
        const int numLevelsMax = levels_.size() - 1;
        const int numProfilesMin = 0;
        const int numProfilesMax = 2;

        const auto key = press->key();
        if (key == Qt::Key_Left)
        {
            if (selrow_ == 0)
            {
                profile_ = wrap(numProfilesMax, numProfilesMin, (int)profile_ - 1);
            }
            else
            {
                level_ = wrap(numLevelsMax, numLevelsMin, (int)level_ -1);
            }

        }
        else if (key == Qt::Key_Right)
        {
            if (selrow_ == 0)
            {
                profile_ = wrap(numProfilesMax, numProfilesMin, (int)profile_ + 1);
            }
            else
            {
                level_ = wrap(numLevelsMax, numLevelsMin, (int)level_ + 1);
            }
        }
        else if (key == Qt::Key_Up)
        {
            selrow_ = wrap(1, 0, (int)selrow_  - 1);
        }
        else if (key == Qt::Key_Down)
        {
            selrow_ = wrap(1, 0, (int)selrow_ + 1);
        }
    }

    unsigned selectedLevel() const
    { return level_; }

    unsigned selectedProfile() const
    { return profile_; }

    void selectLevel(unsigned level)
    { level_ = level; }

    void selectProfile(unsigned profile)
    { profile_ = profile; }
private:
    void drawLevel(QPainter& painter, const QRect& rect, const Level& level, int index, bool hilite)
    {
        const auto& info = infos_[index];

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
    const std::vector<std::unique_ptr<Level>>& levels_;
    const std::vector<LevelInfo>& infos_;
    const std::vector<Profile>& profiles_;
    unsigned level_;
    unsigned profile_;
    unsigned selrow_;
};

class GameWidget::Help
{
public:
    void paint(QPainter& painter, const QRect& rect, const QPoint& scale) const
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
            "Invaders that approach the left edge will show the\n"
            "the pinyin string and score no points.\n"
            "You will lose points for invaders that you faill to kill.\n"
            "Score %1% or higher to unlock the next level.\n\n"
            "Type BOMB to ignite a bomb\n"
            "Type WARP to enter a timewarp\n\n"
            "Press Esc to exit\n").arg(str));
    }
};

class GameWidget::Settings
{
public:
    std::function<void (bool)> onToggleFullscreen;

    Settings(bool sounds, bool fullscreen) : sounds_(sounds), fullscreen_(fullscreen), setting_index_(0)
    {}
    void paint(QPainter& painter, const QRect& rect, const QPoint& scale) const
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
        const auto rows = 6;
        TransformState state(rect, cols, rows);

        QRect rc;
        rc = state.toViewSpaceRect(QPoint(0, 1), QPoint(1, 2));
        painter.drawText(rc, Qt::AlignCenter,
            "Press space to toggle a setting.");

        painter.setPen(regular);

        if (setting_index_ == 0)
            painter.setPen(selected);
        rc = state.toViewSpaceRect(QPoint(0, 2), QPoint(1, 3));
        painter.drawText(rc, Qt::AlignCenter,
            tr("Sound Effects are: %1").arg(sounds_ ? "On" : "Off"));

        painter.setPen(regular);

        rc = state.toViewSpaceRect(QPoint(0, 3), QPoint(1, 4));
        if (setting_index_ == 1)
            painter.setPen(selected);
        painter.drawText(rc, Qt::AlignCenter,
            tr("Fullscreen: %1").arg(fullscreen_ ? "On" : "Off"));

        rc = state.toViewSpaceRect(QPoint(0, 4), QPoint(1, 5));
        painter.setPen(regular);
        painter.drawText(rc, Qt::AlignCenter,
            "Press Esc to exit");
    }

    void keyPress(const QKeyEvent* keyPress)
    {
        const auto key = keyPress->key();
        if (key == Qt::Key_Space)
        {
            if (setting_index_ == 0)
                sounds_ = !sounds_;
            else if (setting_index_ == 1)
            {
                fullscreen_ = !fullscreen_;
                onToggleFullscreen(fullscreen_);
            }
        }
        else if (key == Qt::Key_Up)
        {
            if (--setting_index_ < 0)
                setting_index_ = 1;
        }
        else if (key == Qt::Key_Down)
        {
            setting_index_ = (setting_index_ + 1) % 2;
        }
    }

    bool enableSounds() const
    { return sounds_; }

    bool fullScreen() const
    { return fullscreen_; }
private:
    bool sounds_;
    bool fullscreen_;
private:
    int setting_index_;
};

class GameWidget::About
{
public:
    void paint(QPainter& painter, const QRect& area, const QPoint& scale) const
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
                "Design and programming by\n"
                "Sami Vaisanen\n"
                "(c) 2014-2016 Ensisoft\n"
                "http://www.ensisoft.com\n"
                "http://www.github.com/ensisoft/pinyin-invaders\n\n"
                "Graphics by\n"
                "Tatermand\n"
                "Gamedevtuts\n"
                "Kenney\n"
                "http://www.opengameart.org\n"
                "http://www.kenney.nl"
                ).arg(MAJOR_VERSION).arg(MINOR_VERSION));
    }
private:
};

class GameWidget::Fleet
{
public:
    Fleet(const Level& level) : level_(level)
    {}

    void update(quint64 dt)
    {}

    void paint(QPainter& painter, const QRect& area, const QPoint& scale) const
    {
        QPen pen;
        pen.setWidth(1);
        pen.setColor(Qt::darkGray);
        painter.setPen(pen);

        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(scale.y() / 2);
        painter.setFont(font);

        const auto& enemies = level_.getEnemies();
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
            painter.drawText(rect, Qt::AlignHCenter | Qt::AlignTop,
                QString("%1 (%2) \n %3")
                .arg(e.string)
                .arg(e.killstring)
                .arg(e.help));
        }

        painter.drawText(footer, Qt::AlignCenter, "Press Space to play!");
    }

private:
    const Level& level_;
};





GameWidget::GameWidget(QWidget* parent) : QWidget(parent),
    level_(0), profile_(0), tickDelta_(0), timeStamp_(0), warpFactor_(1.0), warpDuration_(0),
    masterUnlock_(false), unlimitedBombs_(false), unlimitedWarps_(false), playSounds_(true), fullScreen_(false)
{

#ifdef ENABLE_AUDIO
    static auto sndExplosion = std::make_shared<AudioSample>(R("sounds/explode.wav"));
#endif

    QFontDatabase::addApplicationFont(R("fonts/ARCADE.TTF"));

    BigExplosion::prepare();
    Smoke::prepare();
    Alien::prepare();

    game_.reset(new Game(40, 10));

    game_->onMissileKill = [&](const Game::invader& i, const Game::missile& m, unsigned killScore)
    {
        auto it = invaders_.find(i.identity);

        TransformState state(rect(), *game_);

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
        //debris->setScale(invader->getScale());
        sparks->setColor(QColor(255, 255, 68));

        animations_.push_back(std::move(invader));
        animations_.push_back(std::move(missile));
        animations_.push_back(std::move(smoke));
        animations_.push_back(std::move(debris));
        animations_.push_back(std::move(sparks));
        animations_.push_back(std::move(explosion));
        animations_.push_back(std::move(score));

        invaders_.erase(it);

#ifdef ENABLE_AUDIO
        if (playSounds_)
        {
            g_audio->play(sndExplosion, std::chrono::milliseconds(missileFlyTime));
        }
#endif
    };

    game_->onMissileDamage = [&](const Game::invader& i, const Game::missile& m)
    {
        auto it = invaders_.find(i.identity);
        auto& inv = it->second;

        TransformState state(rect(), *game_);

        const auto missileFlyTime  = 500;
        const auto missileEnd      =  inv->getFuturePosition(missileFlyTime, state);
        const auto missileBeg      = m.position;
        const auto missileDir      = missileEnd - missileBeg;

        std::unique_ptr<Animation> missile(new Missile(missileBeg, missileDir, missileFlyTime, m.string.toUpper()));
        std::unique_ptr<Sparks> sparks(new Sparks(missileEnd, missileFlyTime, 500));
        //std::unique_ptr<Explosion> explosion(new Explosion(missileEnd, missileFlyTime, 500, false));
        //explosion->setScale(inv->getScale() * 0.5);
        sparks->setColor(Qt::darkGray);

        auto viewString = i.viewList.join("");
        inv->setViewString(viewString);

        animations_.push_back(std::move(missile));
        //animations_.push_back(std::move(explosion));
        animations_.push_back(std::move(sparks));
    };

    game_->onBombKill = [&](const Game::invader& i, const Game::bomb& b, unsigned killScore)
    {
        auto it = invaders_.find(i.identity);
        auto& inv = it->second;

        std::unique_ptr<Animation> explosion(new Explosion(inv->getPosition(), 0, 1000));
        std::unique_ptr<Animation> score(new Score(inv->getPosition(), 1000, 2000, killScore));
        animations_.push_back(std::move(explosion));
        animations_.push_back(std::move(score));

        invaders_.erase(it);
    };

    game_->onBombDamage = [&](const Game::invader& i, const Game::bomb& b)
    {
        auto it = invaders_.find(i.identity);
        auto& inv = it->second;
        auto str = i.viewList.join("");
        inv->setViewString(str);
    };

    game_->onBomb = [&](const Game::bomb& b)
    {
        std::unique_ptr<Animation> explosion(new BigExplosion(1500));


        animations_.push_back(std::move(explosion));
    };

    game_->onWarp = [&](const Game::timewarp& w)
    {
        qDebug() << "begin time warp";
        warpFactor_   = w.factor;
        warpDuration_ = w.duration;
    };

    game_->onInvaderSpawn = [&](const Game::invader& inv)
    {
        TransformState state(rect(), *game_);

        const auto pos = state.toNormalizedViewSpace(GameSpace{inv.xpos, inv.ypos+1});

        Invader::ShipType type = Invader::ShipType::Slow;
        if (inv.type == Game::InvaderType::boss)
            type = Invader::ShipType::Boss;
        else if (inv.type == Game::InvaderType::regular)
        {
            if (inv.speed == 1)
                type = Invader::ShipType::Slow;
            else type = Invader::ShipType::Fast;
        }
        else if (inv.type == Game::InvaderType::special)
        {
            type = Invader::ShipType::Tough;
        }

        // the game expresses invader speed as the number of discrete steps it takes
        // per each tick of game.
        // here well want to express this velocity as a normalized distance over seconds
        const auto tick       = 1000.0 / profiles_[profile_].speed;
        const auto numTicks   = state.numCols() / (double)inv.speed;
        const auto numSeconds = tick * numTicks;
        const auto velocity   = state.numCols() / numSeconds;

        const auto killString = inv.killList.join("");
        const auto viewString = inv.viewList.join("");

        std::unique_ptr<Invader> invader(new Invader(pos, viewString, velocity, type));
        invaders_[inv.identity] = std::move(invader);
    };


    game_->onInvaderVictory = [&](const Game::invader& inv)
    {
        invaders_.erase(inv.identity);
    };

    // invader is almost escaping unharmed. we help the player to learn
    // by changing the text from chinese to the pinyin kill string
    game_->onInvaderWarning = [&](const Game::invader& inv)
    {
        auto it  = invaders_.find(inv.identity);
        auto str = inv.killList.join("");
        it->second->setViewString(str);
    };

    game_->onLevelComplete = [&](const Game::score& score)
    {
        qDebug() << "Level complete: points" << score.points << "maxpoints" << score.maxpoints;

        //auto& level   = levels_[level_];
        auto& info    = info_[level_];
        auto& profile = profiles_[profile_];

        const auto base    = score.points;
        const auto bonus   = profile.speed * score.points;
        const auto final   = score.points + bonus;
        const bool hiscore = final > info.highScore;
        info.highScore = std::max(info.highScore, (unsigned)final);

        unsigned unlockLevel = 0;
        if ((base / (float)score.maxpoints) >= LevelUnlockCriteria)
        {
            if (level_ < levels_.size())
            {
                if (info_[level_+1].locked)
                {
                    unlockLevel = level_ + 1;
                    info_[unlockLevel].locked = false;
                }
            }
        }

        score_.reset(new Scoreboard(base, bonus, hiscore, unlockLevel));

    };

    background_.reset(new Background);
    display_.reset(new Display(*game_));
    player_.reset(new Player);

    // enable keyboard events
    setFocusPolicy(Qt::StrongFocus);
    startTimer(10);

    timer_.start();
    showMenu();
}

GameWidget::~GameWidget()
{}

void GameWidget::startGame(unsigned levelIndex, unsigned profileIndex)
{
    Q_ASSERT(levelIndex < levels_.size());
    Q_ASSERT(profileIndex < profiles_.size());

    const auto& level   = levels_[levelIndex];
    const auto& profile = profiles_[profileIndex];
    qDebug() << "Start game: " << level->name() << profile.name;

    // todo: use a deterministic seed or not?
    //std::srand(std::time(nullptr));
    std::srand(0x7f6a4b + (levelIndex ^ profileIndex));

    level->reset();
    player_->reset();
    display_->setLevel(levelIndex + 1);

    Game::setup setup;
    setup.numEnemies    = profile.numEnemies;
    setup.spawnCount    = profile.spawnCount;
    setup.spawnInterval = profile.spawnInterval;
    setup.numBombs      = unlimitedBombs_ ? std::numeric_limits<unsigned>::max() : 2;
    setup.numWarps      = unlimitedWarps_ ? std::numeric_limits<unsigned>::max() : 2;
    game_->play(levels_[levelIndex].get(), setup);

    level_         = levelIndex;
    profile_       = profileIndex;
    tickDelta_     = 0;
    warpFactor_    = 1.0; //(0.4; //1.0; //0.4; //1.0;
    warpDuration_  = 0;
}

void GameWidget::loadLevels(const QString& file)
{
    levels_ = Level::loadLevels(file);

    for (const auto& level : levels_)
    {
        LevelInfo info;
        info.highScore = 0;
        info.name      = level->name();
        info.locked    = true;
        info_.push_back(info);
    }
    info_[0].locked = false;
}

void GameWidget::unlockLevel(const QString& name)
{
    for (auto& info : info_)
    {
        if (info.name != name)
            continue;

        info.locked = false;
        return;
    }
}

void GameWidget::setLevelInfo(const LevelInfo& info)
{
    for (auto& i : info_)
    {
        if (i.name != info.name)
            continue;

        i = info;
        return;
    }
}

bool GameWidget::getLevelInfo(LevelInfo& info, unsigned index) const
{
    if (index >= levels_.size())
        return false;
    info = info_[index];
    return true;
}

void GameWidget::setProfile(const Profile& profile)
{
    profiles_.push_back(profile);
}

void GameWidget::setMasterUnlock(bool onOff)
{
    masterUnlock_ = onOff;
}

void GameWidget::setUnlimitedWarps(bool onOff)
{
    unlimitedWarps_ = onOff;
}

void GameWidget::setPlaySounds(bool onOff)
{
    playSounds_ = onOff;
}

void GameWidget::setFullscreen(bool onOff)
{
    fullScreen_ = onOff;
}

void GameWidget::setUnlimitedBombs(bool onOff)
{
    unlimitedBombs_ = onOff;
}


void GameWidget::timerEvent(QTimerEvent* timer)
{
    TransformState state(rect(), *game_);

    // milliseconds
    const auto now  = timer_.elapsed();
    const auto time = now - timeStamp_;
    const auto tick = 1000.0 / profiles_[profile_].speed;
    if (!time)
        return;

    if (rand(0, 5000) == 7)
    {
        if (!alien_)
            alien_.reset(new Alien);
    }

    if (alien_)
    {
        if (!alien_->update(time * warpFactor_, state))
            alien_.reset();
    }

    background_->update(time * warpFactor_);

    if (gameIsRunning())
    {
        tickDelta_ += (time * warpFactor_);
        if (tickDelta_ >= tick)
        {
            // advance game by one tick
            game_->tick();

            tickDelta_ = tickDelta_ - tick;
        }
        // update invaders
        for (auto& pair : invaders_)
        {
            auto& invader = pair.second;
            invader->update(time * warpFactor_, state);
        }
    }

    // update animations
    for (auto it = std::begin(animations_); it != std::end(animations_);)
    {
        auto& anim = *it;
        if (!anim->update(time, state))
        {
            it = animations_.erase(it);
            continue;
        }
        ++it;
    }

    // trigger redraw
    update();

    timeStamp_ = now;

    if (warpDuration_)
    {
        if (time >= warpDuration_)
        {
            warpFactor_   = 1.0;
            warpDuration_ = 0;
            qDebug() << "warp ended";
        }
        else
        {
            warpDuration_ -= time;
        }
    }
}

void GameWidget::paintEvent(QPaintEvent* paint)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::HighQualityAntialiasing);

    TransformState state(rect(), *game_);

    const auto cols = state.numCols();
    const auto rows = state.numRows();
    const auto rect = this->rect();

    background_->paint(painter, rect, state.getScale());

    if (alien_)
        alien_->paint(painter, state);

    if (help_)
    {
        help_->paint(painter, rect, state.getScale());
        return;
    }
    else if (settings_)
    {
        settings_->paint(painter, rect, state.getScale());
        return;
    }
    else if (about_)
    {
        about_->paint(painter, rect, state.getScale());
        return;
    }
    else if (menu_)
    {
        menu_->paint(painter, rect, state.getScale());
        return;
    }
    else if (fleet_)
    {
        fleet_->paint(painter, rect, state.getScale());
        return;
    }


    if (score_ && animations_.empty())
    {
        score_->paint(painter, rect, state.getScale());
        return;
    }

    QPoint top;
    QPoint bot;
    // layout the HUD at the first "game row"
    top = state.toViewSpace(QPoint(0, 0));
    bot = state.toViewSpace(QPoint(cols, 1));
    display_->paint(painter, QRect(top, bot), state.getScale());

    // paint the invaders
    for (auto& pair : invaders_)
    {
        auto& invader = pair.second;
        invader->paint(painter, state);
    }

    // paint animations
    for (auto& anim : animations_)
    {
        anim->paint(painter, state);
    }

    // layout the player at the last "game row"
    top = state.toViewSpace(QPoint(0, rows-1));
    bot = state.toViewSpace(QPoint(cols, rows));
    player_->paint(painter, QRect(top, bot), state);
}

void GameWidget::keyPressEvent(QKeyEvent* press)
{
    const auto key = press->key();

    if (key == Qt::Key_Escape)
    {
        if (help_)
        {
            quitHelp();
            return;
        }
        else if (settings_)
        {
            quitSettings();
            return;
        }
        else if(about_)
        {
            quitAbout();
            return;
        }
        else if (menu_)
        {
            emit quitGame();
            return;
        }
        if (score_)
            quitScore();
        else if (fleet_)
            quitFleet();

        quitLevel();
        showMenu();
    }
    else if (key == Qt::Key_Space)
    {
        if (help_)
        {
            return;
        }
        else if (settings_)
        {
            settings_->keyPress(press);
            return;
        }
        else if (menu_)
        {
            const auto level   = menu_->selectedLevel();
            const auto profile = menu_->selectedProfile();
            if (!info_[level].locked || masterUnlock_)
            {
                startGame(level, profile);
                quitMenu();
                showFleet();
            }
        }
        else if (score_)
        {
            quitScore();
            showMenu();
        }
        else if (fleet_)
        {
            quitFleet();
        }
    }
    else if (key == Qt::Key_F1)
    {
        showHelp();
    }
    else if (key == Qt::Key_F2)
    {
        showSettings();
    }
    else if (key == Qt::Key_F3)
    {
        showAbout();
    }
    else if (settings_)
    {
        settings_->keyPress(press);
    }
    else if (menu_)
    {
        menu_->keyPress(press);
    }
    else if (player_)
    {
        player_->keyPress(press, *game_);
    }
    update();
}

bool GameWidget::gameIsRunning() const
{
    return !menu_ && !fleet_ && !help_ && !settings_ && !about_;
}

void GameWidget::showMenu()
{
    menu_.reset(new Menu(levels_, info_, profiles_));
    menu_->selectLevel(level_);
    menu_->selectProfile(profile_);
}

void GameWidget::showHelp()
{
    help_.reset(new Help);
}

void GameWidget::showSettings()
{
    settings_.reset(new Settings(playSounds_, fullScreen_));
    settings_->onToggleFullscreen = [this](bool onOff) {
        if (onOff)
            emit enterFullScreen();
        else emit leaveFullScreen();
    };
}

void GameWidget::showAbout()
{
    about_.reset(new About);
}

void GameWidget::quitSettings()
{
    playSounds_ = settings_->enableSounds();

    qDebug() << "PlaySounds" << (playSounds_ ? "On" : "Off");

    settings_.reset();
}

void GameWidget::quitAbout()
{
    about_.reset();
}

void GameWidget::showFleet()
{
    const auto& level = *levels_[level_];
    fleet_.reset(new Fleet(level));
}
void GameWidget::quitHelp()
{
    help_.reset();
}

void GameWidget::quitFleet()
{
    fleet_.reset();
}

void GameWidget::quitMenu()
{
    menu_.reset();
}

void GameWidget::quitScore()
{
    score_.reset();
}

void GameWidget::quitLevel()
{
    if (game_->isRunning())
    {
        game_->quitLevel();
        invaders_.clear();
        animations_.clear();
    }
    warpFactor_ = 1.0;
}

} // invaders
