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
#include "warnpop.h"
#include <cmath>
#include <ctime>

#include "gamewidget.h"
#include "game.h"
#include "level.h"

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

QString F(const QString& s) 
{ 
    const auto inst = QApplication::applicationDirPath();
    return inst + s;
}

// game space is discrete space from 0 to game::width() - 1 on X axis
// and from 0 to game::height() - 1 on Y axis
// we use QPoint to represent this.

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
        // for the game in a way that doesnt depend on any actual
        // viewport size.        
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

    QPoint toViewSpace(const QPoint& game) const
    {
        const int xpos = game.x() * scale_.x() + origin_.x();
        const int ypos = game.y() * scale_.y() + origin_.y();
        return { xpos, ypos };
    }

    QPoint toViewSpace(const QVector2D& norm) const 
    {
        const int xpos = widget_.x() * norm.x() + origin_.x();
        const int ypos = widget_.y() * norm.y() + origin_.y();
        return { xpos, ypos };
    }

    QVector2D toNormalizedViewSpace(const QPoint& p) const 
    {
        const float xpos = (float)p.x() / widget_.x();
        const float ypos = (float)p.y() / widget_.y();
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
    virtual bool update(quint64 dt, float tick, TransformState& state) = 0;

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

    virtual bool update(quint64 dt, float tick, TransformState& state) override
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
        : start_(start), life_(lifetime), time_(0)
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
    virtual bool update(quint64 dt, float tick, TransformState&) override
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

        QColor color(255, 255, 68);
        QBrush brush(color);
        for (const auto& particle : particles_)
        {
            color.setAlpha(0xff * particle.a);            
            brush.setColor(color);
            const auto pos = state.toViewSpace(particle.pos);
            painter.fillRect(pos.x(),pos.y(), 2, 2, brush);
        }        
    }

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
};


// slower moving debris, remnants of enemy. uses enemy texture as particle texture.
class GameWidget::Debris : public GameWidget::Animation
{
public:
    Debris(QPixmap texture, QVector2D position, quint64 start, quint64 lifetime) 
        : texture_(texture), start_(start), life_(lifetime), time_(0)
    {
        
        const auto particleWidth  = 16; //texture.width() / 8;
        const auto particleHeight = 16; //texture.height() / 8;
        const auto xparticles = texture.width() / particleWidth;
        const auto yparticles = texture.height() / particleHeight;
        const auto numParticles = xparticles * yparticles;

        const auto angle = (M_PI * 2) / numParticles;

        for (auto i=numParticles / 2 / 2; i<numParticles / 2; ++i)
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
            p.alpha = 0.8f;
            p.angle = (M_PI * 2 ) * (float)std::rand() / RAND_MAX;
            particles_.push_back(p);
        }
    }
    virtual bool update(quint64 dt, float tick, TransformState&) override
    {
        time_ += dt;
        if (time_ < start_)
            return true;

        if  (time_ - start_ > life_)
            return false;

        for (auto& p : particles_)
        {
            p.pos += p.dir * (dt / 4500.0);
            p.alpha = clamp(0.0, p.alpha - (dt / 2000.0), 1.0);
            p.angle += (M_PI * 2) * (dt / 2000.0);
        }

        return true;
    }
    virtual void paint(QPainter& painter, TransformState& state) override
    {
        if (time_ < start_)
            return;

        for (const auto& p : particles_)
        {
            const auto pos = state.toViewSpace(p.pos);

            QTransform rotation;
            rotation.translate(pos.x(), pos.y());
            rotation.rotateRadians(p.angle, Qt::ZAxis);
            rotation.translate(-pos.x(), -pos.y());

            painter.setTransform(rotation);
            painter.setOpacity(0xff * p.alpha);
            painter.drawPixmap(pos, texture_, p.rc);
        }
        painter.resetTransform();
    }
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
};

class GameWidget::Invader : public GameWidget::Animation
{
public:
    enum class ShipType {
        cargo, cruiser, destroyer
    };

    Invader(QVector2D position, QString str, unsigned speed, ShipType type) : 
        position_(position), text_(str), time_(0), expire_(0), speed_(speed), type_(type)
    {}

    void setPosition(QVector2D position)
    {
        position_ = position;
    }

    bool update(quint64 dt, float tick, TransformState& state)
    {
        const auto unit = state.toViewSpace(QPoint(-(int)speed_, 0));
        const auto norm = state.toNormalizedViewSpace(unit);
        position_ += (norm * tick);
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
            case ShipType::cargo:     return 6.0f;
            case ShipType::cruiser:   return 4.0f;
            case ShipType::destroyer: return 9.5f;
        }
        return 1.0f;
    }

    void paint(QPainter& painter, TransformState& state)
    {
        // offset the texture to be centered around the position
        const auto unitScale   = state.getScale();
        const auto spriteScale = state.getScale() * getScale();
        const auto position    = state.toViewSpace(position_);

        QPixmap tex = Invader::texture(type_);
        const float width  = tex.width();
        const float height = tex.height();
        //const float aspect = width / height;
        //const float scaledHeight = spriteScale.y();
        //const float scaledWidth  = scaledHeight * aspect;
        const float aspect = height / width;        
        const float scaledWidth  = spriteScale.x();
        const float scaledHeight = scaledWidth * aspect;

        // set the target rectangle with the dimensions of the 
        // sprite we want to draw.
        QRect target(0, 0, scaledWidth, scaledHeight);

        // offset it so that the center is aligned with the unit position
        target.moveTo(position -
            QPoint(scaledWidth / 2.0, scaledHeight / 2.0));

        painter.drawPixmap(target, tex, tex.rect());

        target.translate(scaledWidth, 0);

        QFont font;
        font.setFamily("Monospace");
        font.setPixelSize(unitScale.y() / 2);

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
    QVector2D getPosition(float ticksLater, const TransformState& state) const 
    {
        const auto unit = state.toViewSpace(QPoint(-(int)speed_, 0));
        const auto norm = state.toNormalizedViewSpace(unit);
        return position_ + norm * ticksLater;
    }

    void expireIn(quint64 ms)
    { expire_ = ms; }

    unsigned getSpeed() const 
    { return speed_; }

    void setViewString(QString str)
    {
        text_ = str;
    }
    QPixmap getTexture() const 
    { return texture(type_); }

private:
    static const QPixmap& texture(ShipType type) 
    {
        static QPixmap textures[] = {
            QPixmap(R("textures/Cargoship.png")),            
            QPixmap(R("textures/Cruiser.png")),
            QPixmap(R("textures/Destroyer.png"))
        };
        return textures[(int)type];
    }

private:
    QVector2D position_;
    QString text_;
    quint64 time_;
    quint64 expire_;
private:
    unsigned speed_;
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

    virtual bool update(quint64 dt, float tick, TransformState& state) override
    {
        time_ += dt;
        if (time_ > life_)
            return false;

        const auto d = (float)dt / (float)life_;
        const auto p = direction_ * d;
        position_ += p;
        return true;
    }

    void paint(QPainter& painter, TransformState& state)
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


class GameWidget::BigExplosion : public GameWidget::Animation
{
public:
    BigExplosion(quint64 lifetime) : lifetime_(lifetime), time_(0)
    {}

    virtual bool update(quint64 dt, float tick, TransformState& state) override
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
            v.push_back(R("textures/bomb/explosion1_00%1.png").arg(i));
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

    virtual bool update(quint64 dt, float tick, TransformState& state) override
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
        std::srand(std::time(nullptr));

        direction_ = QVector2D(4, 3);
        direction_.normalize();

        for (std::size_t i=0; i<300; ++i)
        {
            particle p;
            p.x = (float)std::rand() / RAND_MAX;
            p.y = (float)std::rand() / RAND_MAX;
            p.a = 0.5 + (0.5 * (float)std::rand() / RAND_MAX);
            p.v = 0.05 + (0.05 * (float)std::rand() / RAND_MAX);
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
        if (key == Qt::Key_Space || key == Qt::Key_Return)
        {
            if (text_ == "BOMB")
            {
                Game::bomb b;
                game.ignite(b);
            }
            else if (text_ == "WARP")
            {
                Game::timewarp w;
                w.duration = 3000;
                w.factor   = 0.2f;
                game.enter(w);
            }
            else
            {
                Game::missile m;
                m.position = missile_;
                m.string = text_.toLower();
                game.fire(m);                
            }
            text_.clear();
        }
        else if (key == Qt::Key_Backspace)
        {
            if (!text_.isEmpty())
                text_.resize(text_.size()-1);
        }
        else if (key >= 0x41 && key <= 0x5a)
        {
            text_.append(key);
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
        painter.drawText(rect, Qt::AlignCenter,
            "Evil chinese characters are attacking!\n"
            "Only you can stop them by typing the right pinyin.\n"
            "Good luck.\n\n"
            "Enter - Fire\n"
            "Esc - Exit\n"
            "F1 - Help\n\n"
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
        drawLevel(painter, rect, *left, prev);

        if (selrow_ == 1)
        {
            if (infos_[level_].locked)
                 painter.setPen(locked);
            else painter.setPen(selected);
        }
        else
        {
            painter.setPen(regular);
        }

        rect = state.toViewSpaceRect(QPoint(3, 4), QPoint(4, 5));
        drawLevel(painter, rect, *mid, level_);

        painter.setPen(regular);
        rect = state.toViewSpaceRect(QPoint(5, 4), QPoint(6, 5));
        painter.drawRect(rect);
        drawLevel(painter, rect, *right, next);


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
    void drawLevel(QPainter& painter, const QRect& rect, const Level& level, int index)
    {
        const auto& info = infos_[index];

        QString locked;
        if (info.locked)
            locked = "Locked";
        else if (info.highScore)
            locked = QString("%1 points").arg(info.highScore);
        else locked = "Play!";

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
    level_(0), profile_(0), tickDelta_(0), timeStamp_(0), warpFactor_(1.0), warpDuration_(0), masterUnlock_(false), unlimitedBombs_(false), unlimitedWarps_(false)
{
    QFontDatabase::addApplicationFont(R("fonts/ARCADE.TTF"));

    BigExplosion::prepare();

    game_.reset(new Game(40, 10));

    game_->onMissileKill = [&](const Game::invader& i, const Game::missile& m)
    {
        auto it = invaders_.find(i.identity);

        TransformState state(rect(), *game_);

        std::unique_ptr<Invader> invader(it->second.release());

        // calculate position for the invader to be in at now + missile fly time
        // and then aim the missile to that position.
        const auto tick  = 1000.0 / profiles_[profile_].speed;

        const auto missileFlyTime   = 500;
        const auto explosionTime    = 1000;
        const auto missileFlyTicks  = missileFlyTime / tick;
        const auto missileEnd       = invader->getPosition(missileFlyTicks, state);
        const auto missileBeg       = m.position;
        const auto missileDir       = missileEnd - missileBeg;

        std::unique_ptr<Animation> missile(new Missile(missileBeg, missileDir, missileFlyTime, m.string.toUpper()));
        std::unique_ptr<Explosion> explosion(new Explosion(missileEnd, missileFlyTime, explosionTime));
        std::unique_ptr<Animation> debris(new Debris(invader->getTexture(), missileEnd, missileFlyTime, explosionTime + 100));
        std::unique_ptr<Animation> sparks(new Sparks(missileEnd, missileFlyTime, explosionTime));
        std::unique_ptr<Animation> score(new Score(missileEnd, explosionTime, 2000, i.score));        

        invader->expireIn(missileFlyTime);
        explosion->setScale(invader->getScale() * 1.5);

        animations_.push_back(std::move(invader));
        animations_.push_back(std::move(missile));
        animations_.push_back(std::move(debris));
        animations_.push_back(std::move(sparks));        
        animations_.push_back(std::move(explosion));
        animations_.push_back(std::move(score));

        invaders_.erase(it);
    };

    game_->onMissileDamage = [&](const Game::invader& i, const Game::missile& m)
    {
        auto it = invaders_.find(i.identity);
        auto& inv = it->second;

        TransformState state(rect(), *game_);

        const auto tick = 1000.0 / profiles_[profile_].speed;

        const auto missileFlyTime  = 500;
        const auto missileFlyTicks = missileFlyTime / tick;
        const auto missileEnd      =  inv->getPosition(missileFlyTicks, state);
        const auto missileBeg      = m.position;
        const auto missileDir      = missileEnd - missileBeg;

        std::unique_ptr<Animation> missile(new Missile(missileBeg, missileDir, missileFlyTime, m.string.toUpper()));
        std::unique_ptr<Animation> sparks(new Sparks(missileEnd, missileFlyTime, 500));
        //std::unique_ptr<Explosion> explosion(new Explosion(missileEnd, missileFlyTime, 500, false));
        //explosion->setScale(inv->getScale() * 0.5);

        auto viewString = i.viewList.join("");
        inv->setViewString(viewString);

        animations_.push_back(std::move(missile));
        //animations_.push_back(std::move(explosion));
        animations_.push_back(std::move(sparks));
    };

    game_->onBombKill = [&](const Game::invader& i, const Game::bomb& b)
    {
        auto it = invaders_.find(i.identity);
        auto& inv = it->second;

        std::unique_ptr<Animation> explosion(new Explosion(inv->getPosition(), 0, 1000));
        std::unique_ptr<Animation> score(new Score(inv->getPosition(), 1000, 2000, i.score));
        animations_.push_back(std::move(explosion));
        animations_.push_back(std::move(score));

        invaders_.erase(it);
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

    game_->onInvaderSpawn = [&](const Game::invader& inv, bool boss)
    {
        TransformState state(rect(), *game_);

        const auto view = state.toViewSpace(QPoint(inv.xpos, inv.ypos));
        const auto posi = state.toNormalizedViewSpace(view);        

        Invader::ShipType type = Invader::ShipType::destroyer;
        if (boss)
            type = Invader::ShipType::destroyer;
        else if (inv.speed == 1)
            type = Invader::ShipType::cargo;
        else type = Invader::ShipType::cruiser;

        const auto killString = inv.killList.join("");
        const auto viewString = inv.viewList.join("");

        std::unique_ptr<Invader> invader(new Invader(posi, viewString, inv.speed, type));
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

        auto& level   = levels_[level_];
        auto& info    = info_[level_];
        auto& profile = profiles_[profile_];
        
        const auto base    = score.points;
        const auto bonus   = profile.speed * score.points;
        const auto final   = score.points + bonus;
        const bool hiscore = final > info.highScore;
        info.highScore = std::max(info.highScore, (unsigned)final);

        unsigned unlockLevel = 0;
        if ((base / (float)score.maxpoints) > LevelUnlockCriteria)
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
    startTimer(0);

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

    level_        = levelIndex;
    profile_      = profileIndex;
    tickDelta_    = 0;
    warpFactor_   = 1.0;
    warpDuration_ = 0;
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

void GameWidget::setUnlimitedBombs(bool onOff)
{
    unlimitedBombs_ = onOff;
}


void GameWidget::timerEvent(QTimerEvent* timer)
{
    TransformState state(rect(), *game_);

    // milliseconds
    const auto now  = timer_.elapsed();
    const auto time = (now - timeStamp_);
    const auto tick  = 1000.0 / profiles_[profile_].speed;    
    if (!time) 
        return;

    background_->update(time * warpFactor_);

    if (gameIsRunning())
    {
        tickDelta_ += (time * warpFactor_);
        if (tickDelta_ >= tick)
        {
            // advance game by one tick
            game_->tick();

            // synchronize invader position with the actual game position
            const auto& invaders = game_->invaders();
            for (const auto& i : invaders)
            {
                auto it   = invaders_.find(i.identity);
                auto& inv = it->second;
                const auto view = state.toViewSpace(QPoint(i.xpos, i.ypos + 1));
                const auto pos  = state.toNormalizedViewSpace(view);
                inv->setPosition(pos);
            }
            tickDelta_ = 0;
        }
        else
        {
            // fragment of time expressed in ticks
            const auto ticks = (time * warpFactor_) / tick;
            for (auto& pair : invaders_)
            {
                auto& invader = pair.second;
                invader->update(time * warpFactor_, ticks, state);
            }
        }
    }

    const auto ticks = time / tick;

    // update animations
    for (auto it = std::begin(animations_); it != std::end(animations_);)
    {
        auto& anim = *it;
        if (!anim->update(time, ticks, state))
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
        warpDuration_ = clamp(0, (int)warpDuration_ - (int)time, (int)warpDuration_);
        if (!warpDuration_)
        {
            warpFactor_ = 1.0;
            qDebug() << "warp ended";
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

    if (help_)
    {
        help_->paint(painter, rect, state.getScale());
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
    return !menu_ && !fleet_ && !help_;
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
