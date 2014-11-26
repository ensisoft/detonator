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
#  include <QResource>
#  include <QtDebug>
#include "warnpop.h"

#include "gamewidget.h"
#include "game.h"
#include "level.h"

namespace {

QFont loadFont(const QString& file, const QString& family)
{
    const auto ret = QFontDatabase::addApplicationFont(file);
    if (ret == -1)
        throw std::runtime_error("failed to load font");

    QFont font(family);
    //if (!font.isValid())
    //    throw std::runtime_error("no such font");
    return font;
}

}  // namespace

namespace invaders
{

// game space is discrete space from 0 to game::width-1 on X axis
// and from 0 to game::height-1 on Y axis
struct GameSpace {
    unsigned x, y;

    QPoint toPoint() const 
    { return {(int)x, (int)y}; }
};

struct GameSpaceF {
    float x, y;

    QPointF toPoint() const 
    { return {x, y}; }
};

// normalized widget/view space is expressed with floats from 0 to 1.0 on Y and X
// 0.0, 0.0 being the window top left and 1.0,1.0 being the window bottom right
struct NormalizedViewSpace {
    float x, y;

    QPointF toPoint() const 
    { return {x, y}; }

    NormalizedViewSpace& operator += (const NormalizedViewSpace& rhs) 
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
    NormalizedViewSpace& operator -= (const NormalizedViewSpace& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return  *this;
    }
};

NormalizedViewSpace operator - (const NormalizedViewSpace& lhs, const NormalizedViewSpace& rhs)
{
    return { lhs.x - rhs.x,
             lhs.y - rhs.y };
}


// eventually QPainter paint operations require coordinates in pixel space.
struct ViewSpace {
    int x, y;

    QPoint toPoint() const 
    { return {x, y }; }
};

class TransformState 
{
public:
    TransformState(const QWidget* widget, const Game* game)
    {
        // we divide the widget's client area into a equal sized cells
        // according to the game's size. We also add one extra row 
        // for HUD display at the top of the screen and for the player
        // at the bottom of the screen
        const auto game_width  = game->width();
        const auto game_height = game->height() + 2;
        const auto widget_width  = widget->width();
        const auto widget_height = widget->height();

        widget_ = QPoint(widget_width, widget_height);
        game_   = QPoint(game_width, game_height);
        scale_  = QPoint(widget_width / game_width, widget_height / game_height);
    }

    ViewSpace toViewSpace(GameSpace game) const 
    {
        const int xpos = game.x * scale_.x();
        const int ypos = game.y * scale_.y();
        return {xpos, ypos};
    }

    ViewSpace toViewSpace(NormalizedViewSpace norm) const 
    {
        const int xpos = widget_.x() * norm.x;
        const int ypos = widget_.y() * norm.y;
        return { xpos, ypos };
    }

    NormalizedViewSpace toNormalizedViewSpace(GameSpace game) const 
    {
        const float xpos = (float)game.x * scale_.x();
        const float ypos = (float)game.y * scale_.y();

        return { xpos / widget_.x(), 
                 ypos / widget_.y() };
    }

    NormalizedViewSpace toNormalizedViewSpace(GameSpaceF game) const 
    {
        const float xpos = game.x * scale_.x();
        const float ypos = game.y * scale_.y();

        return { xpos / widget_.x(), 
                 ypos / widget_.y() };        
    }


    QPointF toNormalizedViewSpace(QPoint widget)
    {
        const auto xpos = (float)widget.x() / widget_.x();
        const auto ypos = (float)widget.y() / widget_.y();
        return { xpos, ypos };
    }

    QPoint getScale() const 
    { 
        return scale_; 
    }

    // get whole widget rect in widget coordinates
    QRect viewRect() const 
    { 
        return {0, 0, widget_.x(), widget_.y()}; 
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

    int numRows() const 
    { 
        return game_.y(); 
    }

    int numCols() const 
    { 
        return game_.x();
    }

private:
    QPoint widget_;
    QPoint game_;
    QPoint scale_;
};

class GameWidget::Animation
{
public:
    virtual ~Animation() = default;

    // returns true if animation is still valid
    // otherwise false and the animation is expired.
    virtual bool update(quint64 td) = 0;

    // 
    virtual void paint(QPainter& painter, TransformState& state) = 0;
protected:
private:
};

//
class GameWidget::MissileLaunch : public GameWidget::Animation
{
public:
    MissileLaunch(std::unique_ptr<Invader> i,
        std::unique_ptr<Missile> m) : invader_(std::move(i)), missile_(std::move(m))
    {}
    virtual bool update(quint64 td) override
    {
        return false;
    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {}

private:
    std::unique_ptr<Invader> invader_;
    std::unique_ptr<Missile> missile_;
};


class GameWidget::Invader
{
public:
    Invader(unsigned x, unsigned y, unsigned character)
    {
        font_.setFamily("Monospace");
        text_.append(character);
    }

    // set position from game space coordinates
    void setPosition(unsigned x, unsigned y, TransformState& state)
    {
        position_ = state.toNormalizedViewSpace(GameSpace{x, y + 1});
    }

    // set position in normalized view space
    void setPosition(float x, float y)
    {
        position_ = NormalizedViewSpace{x, y};
    }

    void update(quint64 time_delta, quint64 time_tick, TransformState& state)
    {
        const auto frac = (float)time_delta / (float)time_tick;
        offset_ = state.toNormalizedViewSpace(GameSpaceF{frac, 0.0});
    }

    void paint(QPainter& painter, TransformState& state, Game& game)
    {
        const auto dim  = state.getScale();
        const auto pos  = state.toViewSpace(position_ - offset_);
        const auto beg  = pos.toPoint();
        const auto end  = beg + dim; 

        font_.setPixelSize(dim.y());

        QPen pen;
        QRect rect(beg, end);
        pen.setWidth(2);
        pen.setColor(Qt::darkCyan);
        painter.setFont(font_);
        painter.setPen(pen);
        painter.drawText(rect, Qt::AlignCenter, text_);
    }
private:
    NormalizedViewSpace position_;
    NormalizedViewSpace offset_;
    QFont    font_;
    QString  text_;
};

class GameWidget::Missile
{
public:
    Missile(float x, float y, unsigned value) : xpos_(x), ypos_(y)
    {
        for (int i=3; i>=0; --i)
        {
            char c = (value >> (i * 8)) & 0xff;
            if (c)
                text_.push_back(c);
        }
        font_ = loadFont(":/fonts/ARCADE.TTF", "Arcade");
    }

    // set position in normalized widget space
    void setPosition(float x, float y)
    {
        xpos_ = x;
        ypos_ = y;
    }

    void paint(QPainter& painter, TransformState& state, Game& game)
    {
        const auto dim = state.getScale();
        const auto beg = QPointF(xpos_, ypos_);
        const auto end = QPointF(xpos_ + dim.x(), ypos_ + dim.y());

        font_.setPixelSize(dim.y() / 2);

        QPen pen;
        QRectF rect(beg, end);
        pen.setWidth(2);
        pen.setColor(Qt::darkGray);
        painter.setFont(font_);
        painter.setPen(pen);
        painter.drawText(rect, Qt::AlignCenter, text_);
    }
private:
    float xpos_;
    float ypos_;
    QString  text_;
    QFont    font_;
};

float wrap(float max, float val, float wrap)
{
    if (val > max)
        return wrap;
    return val;
}

// game space background rendering
class GameWidget::Background
{
public:
    Background()
    {
        std::srand(std::time(nullptr));

        direction_ = QVector2D(4, 3);
        direction_.normalize();

        for (std::size_t i=0; i<200; ++i)
        {
            particle p;
            p.x = (float)std::rand() / RAND_MAX;
            p.y = (float)std::rand() / RAND_MAX;
            p.a = 1.0;
            p.v = 0.08 + (0.05 * (float)std::rand() / RAND_MAX);
            particles_.push_back(p);
        }
    }

    void paint(QPainter& painter, TransformState& state, Game& game)
    {
        QBrush space(Qt::black);
        painter.fillRect(state.viewRect(), space);

        QBrush star(Qt::white);

        for (const auto& p : particles_)
        {
            const auto x = p.x * state.viewWidth();
            const auto y = p.y * state.viewHeight();
            painter.fillRect(x, y, 1, 1, star);
        }

    }
    void update(quint64 time)
    {
        const auto d = direction_ * (time / 1000.0);
        for (auto& p : particles_)
        {
            p.x = wrap(1.0, p.x + d.x() * p.v, 0.0);
            p.y = wrap(1.0, p.y + d.y() * p.v, 0.0);
        }
    }
private:
    struct particle {
        float x, y;
        float a; 
        float v;
    };
    std::vector<particle> particles_; 
private:
    QVector2D direction_;
};

// Heads Up Display, display scoreboards etc.
class GameWidget::Display
{
public:
    Display() : font_(loadFont(":/fonts/ARCADE.TTF", "Arcade")), life_(":/resource/icons/ico_life.png")
    {}

    void paint(QPainter& painter, TransformState& state, Game& game)
    {
        const auto dim = state.getScale();
        const auto beg = state.toViewSpace(GameSpace{0, 0});
        const auto end = state.toViewSpace(GameSpace{game.width(), 1});
        const QRect rect(beg.toPoint(), end.toPoint());

        QPen pen;
        pen.setColor(Qt::darkGreen);
        pen.setWidth(1);
        font_.setPixelSize(dim.y() / 2);

        const auto score = game.score();
        const auto hiscore = game.highScore();
        QRect out;
        painter.setFont(font_);
        painter.setPen(pen);
        painter.drawText(rect, 
            Qt::AlignLeft | Qt::AlignVCenter,
            QString("Level %1 Score %2")
            .arg(0)
            .arg(score), &out);
    }
private:
    QFont font_;
    QPixmap life_;
};

// player representation on the screen (the vertical caret)
class GameWidget::Player 
{
public:
    Player() : font_(loadFont(":/fonts/ARCADE.TTF", "Arcade")), text_("Start typing")
    {}

    void paint(QPainter& painter, TransformState& state, Game& game)
    {
        const unsigned ypos = state.numRows() - 1;
        const auto dim = state.getScale();        
        const auto beg = state.toViewSpace(GameSpace{0, ypos});
        const auto end = state.toViewSpace(GameSpace{game.width(), ypos + 1});
        QRect rect(beg.toPoint(), end.toPoint());

        font_.setPixelSize(dim.y() / 2);

        QPen pen;
        pen.setColor(Qt::darkGray);
        pen.setWidth(2);
        painter.setPen(pen);
        painter.setFont(font_);        
        painter.drawText(rect, Qt::AlignCenter, text_);
    }

    void keyPress(QKeyEvent* press, Game& game)
    {
        if (text_ == "Start typing")
            text_.clear();

        const auto key = press->key();
        if (key == Qt::Key_Space || key == Qt::Key_Return)
        {
            unsigned value = 0;
            for (int i=0; i<text_.size(); ++i)
            {
                const auto ch = text_[i].toLower();
                value <<= 8;
                value |= ch.toAscii();
            }
            game.fire(value);
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
private:
    QFont    font_;
    QString  text_;
};


class GameWidget::Debug
{
public:
    void paint(QPainter& painter, TransformState& state, Game& game)
    {
        //QPen pen;
        //pen.setWidth(1);
        //pen.setColor(Qt::darkGray);
        //painter.setPen(pen);

        //const auto num_cols = game.width();
        //const auto num_rows = game.height();

        //const auto dim = state.getScale();
        //const auto ori = state.toViewSpace(QPoint(0, 0));
        //const auto end = state.toViewSpace(QPoint(num_cols, num_rows));

        // draw grid lines parallel to the X axis
        //for (unsigned i=0; i<=num_rows; ++i)
        //{
        //    const auto y = ori.y() + i * dim.y();
        //    painter.drawLine(ori.x(), y, end.x(), y);
        //}

        // draw gri lines pallel to the Y axis
        //for (unsigned i=0; i<=num_cols; ++i)
        //{
        //    const auto x = ori.x() + i * dim.x();
        //    painter.drawLine(x, ori.y(), x, end.y());
        //}
    }
};

// initial greeting and instructions
class GameWidget::Welcome 
{
public:
    Welcome() : font_(loadFont(":/fonts/ARCADE.TTF", "Arcade"))
    {}

    void update(quint64 time)
    {}

    void paint(QPainter& painter, TransformState& state, Game& game)
    {
        const auto dim  = state.getScale();
        const auto rect = state.viewRect();

        font_.setPixelSize(dim.y() / 2);

        QPen pen;
        pen.setWidth(1);
        pen.setColor(Qt::darkGray);
        painter.setPen(pen);
        painter.setFont(font_);

        painter.drawText(rect,
            Qt::AlignCenter,
            "Evil chinese characters are attacking!\n"
            "Only you can stop them by typing the right pinyin.\n"
            "Good luck.\n\n"
            "Enter - Fire        \n"
            "Esc - Exit          \n\n"
            "Press Space to play!\n");
    }
private:
    QFont font_;
};

GameWidget::GameWidget(QWidget* parent) : QWidget(parent)
{
    game_.reset(new Game(20, 10));

    game_->on_invader_kill = [&](const Game::invader& i)
    {
        invaders_.erase(i.identity);
    };

    game_->on_invader_spawn = [&](const Game::invader& inv) {
        std::unique_ptr<Invader> invader(new Invader(inv.xpos, inv.ypos, inv.character));
        invaders_[inv.identity] = std::move(invader);
    };

    game_->on_invader_victory = [&](const Game::invader& inv) {
        invaders_.erase(inv.identity);
    };

    background_.reset(new Background);
    welcome_.reset(new Welcome);

    // enable keyboard events
    setFocusPolicy(Qt::StrongFocus);
    startTimer(0);

    time_delta_ = 0;
    time_total_ = 0;
    speed_  = 2;
    timer_.start();
}

GameWidget::~GameWidget()
{}

void GameWidget::startGame()
{
    player_.reset(new Player);
    display_.reset(new Display);
    welcome_.reset();

    level_ = 0; // start at level 0 (doh)
    speed_ = 2; // initial speed at 2 units per tick

    game_->play(*levels_[0]);

    invaders_.clear();

    qDebug() << "New game";
}

void GameWidget::loadLevel(const QString& file)
{
    qDebug() << "Loading level: " << file;

    std::unique_ptr<Level> level(new Level);
    level->load(file);
    levels_.push_back(std::move(level));
}

void GameWidget::timerEvent(QTimerEvent* timer)
{
    // milliseconds
    const auto time = timer_.elapsed();
    const auto tick = 1000 / speed_;
    const auto elap = time - time_stamp_;

    time_delta_ += elap;
    time_total_ += elap;
    if (time_delta_ >= tick && game_)
    {
        TransformState state(this, game_.get());

        // advance game by one tick
        game_->tick();

        // synchronize invader position with the actual game position
        const auto& invaders = game_->invaders();

        for (const auto& i : invaders)
        {
            auto it   = invaders_.find(i.identity);
            auto& inv = it->second;
            inv->setPosition(i.xpos, i.ypos, state);
        }
        time_delta_ = 0;
    }

    update();

    time_stamp_ = time;
}

void GameWidget::paintEvent(QPaintEvent* paint)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::HighQualityAntialiasing);

    TransformState state(this, game_.get());

    const auto time  = timer_.elapsed();
    const auto delta = time - time_stamp_;
    const auto tick  = 1000 / speed_;    

    background_->update(delta);
    background_->paint(painter, state, *game_);
    if (welcome_)
    {
        welcome_->update(delta);
        welcome_->paint(painter, state, *game_);
        return;
    }

    display_->paint(painter, state, *game_);

    for (auto& pair : invaders_)
    {
        auto& invader = pair.second;
        invader->update(time_delta_, tick, state);
        invader->paint(painter, state, *game_);
    }

    for (auto it = std::begin(animations_); it != std::end(animations_);)
    {
        auto& anim = *it;
        if (!anim->update(delta))
        {
            it = animations_.erase(it);
            continue;
        }
        anim->paint(painter, state);
        ++it;
    }

    player_->paint(painter, state, *game_);    
}

void GameWidget::keyPressEvent(QKeyEvent* press)
{
    const auto key = press->key();

    if (welcome_)
    {
        if (key == Qt::Key_Escape)
            emit quitGame();
        else if (key == Qt::Key_Space)
            startGame();
        return;
    }

    if (key == Qt::Key_Escape)
    {
        welcome_.reset(new Welcome());
        return;
    }

    player_->keyPress(press, *game_);
    update();
}

} // invaders