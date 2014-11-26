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

class PaintState 
{
public:
    PaintState(QRect rect) : rect_(rect)
    {}

    QRect viewRect() const 
    { return rect_; }

    int viewWidth() const 
    { return rect_.width(); }

    int viewHeight() const 
    { return rect_.height(); }

    QPoint toViewSpace(QPoint world)
    {
        const auto xpos = world.x() * scale_.x() + 
            offset_.x() * scale_.x();
        const auto ypos = world.y() * scale_.y() + 
            offset_.y() * scale_.y();

        return {xpos, ypos};
    }
    QPointF toViewSpace(QPointF world)
    {
        const auto xpos = world.x() * scale_.x() + 
            offset_.x() * scale_.x();
        const auto ypos = world.y() * scale_.y() +
            offset_.y() * scale_.y();

        return {xpos, ypos};
    }

    void setUnitOffset(QPoint offset)
    {
        offset_ = offset;
    }

    void setScale(QPoint xy)
    {
        scale_ = xy;
    }
    QPoint getScale() const 
    { return scale_; }

private:
    QRect rect_;
    QPoint scale_;
    QPoint offset_;
};

class GameWidget::Invader
{
public:
    Invader(unsigned x, unsigned y, unsigned character) : xpos_(x), ypos_(y), offset_(0)
    {
        font_.setFamily("Monospace");
        text_.append(character);
    }

    void setPosition(unsigned x, unsigned y)
    {
        xpos_ = x;
        ypos_ = y;
    }

    void update(quint64 time_delta, quint64 time_tick)
    {
        offset_ = double(time_delta) / double(time_tick);
    }

    void paint(QPainter& painter, PaintState& state, Game& game)
    {
        const auto dim = state.getScale();
        const auto beg = state.toViewSpace(QPointF(xpos_ - offset_ - 1, ypos_));
        const auto end = state.toViewSpace(QPointF(xpos_ - offset_, ypos_ + 1));

        font_.setPixelSize(dim.y());

        QPen pen;
        QRectF rect(beg, end);
        pen.setWidth(2);
        pen.setColor(Qt::darkCyan);
        painter.setFont(font_);
        painter.setPen(pen);
        painter.drawText(rect, Qt::AlignCenter, text_);
    }
private:
    double xpos_;
    double ypos_;
    double offset_;
    QFont    font_;
    QString  text_;
};

class GameWidget::Missile
{
public:
    Missile(unsigned x, unsigned y, unsigned value) : xpos_(x), ypos_(y)
    {
        for (int i=3; i>=0; --i)
        {
            char c = (value >> (i * 8)) & 0xff;
            if (c)
                text_.push_back(c);
        }
        offset_ = 0;
        font_   = loadFont(":/fonts/ARCADE.TTF", "Arcade");
    }

    void setPosition(unsigned x, unsigned y)
    {
        xpos_ = x;
        ypos_ = y;
    }

    void update(quint64 time_delta, quint64 time_tick)
    {
        offset_ = (double)time_delta / (double)time_tick;
    }

    void paint(QPainter& painter, PaintState& state, Game& game)
    {
        const auto dim = state.getScale();
        const auto beg = state.toViewSpace(QPointF(xpos_ + offset_, ypos_));
        const auto end = state.toViewSpace(QPointF(xpos_ + offset_ + 1, ypos_ + 1));

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
    double xpos_;
    double ypos_;
    double offset_;    
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

    void paint(QPainter& painter, PaintState& state, Game& game)
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

    void paint(QPainter& painter, PaintState& state, Game& game)
    {
        const auto dim = state.getScale();
        const auto beg = state.toViewSpace(QPoint(0, 0));
        const auto end = state.toViewSpace(QPoint(game.width(), 1));
        const QRect rect(beg, end);

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
            QString("Level %1 Score %2 High Score %3")
            .arg(0)
            .arg(score)
            .arg(hiscore), &out);
    }
private:
    QFont font_;
    QPixmap life_;
};

// player representation on the screen (the vertical caret)
class GameWidget::Player 
{
public:
    Player() : ypos_(0), blink_(0), value_(0)
    {
        font_ = loadFont(":/fonts/ARCADE.TTF", "Arcade");
    }

    void paint(QPainter& painter, PaintState& state, Game& game)
    {
        QPen pen;
        pen.setColor(Qt::darkGray);
        pen.setWidth(10);
        painter.setPen(pen);

        const auto beg = state.toViewSpace(QPoint(0, ypos_));
        const auto end = state.toViewSpace(QPoint(1, ypos_+1));
        const auto dim = state.getScale();

        painter.drawLine(beg.x(), beg.y(), beg.x(), beg.y() + dim.y());

        QRect rect(beg, end);

        font_.setPixelSize(dim.y() / 2);
        pen.setWidth(2);
        painter.setPen(pen);
        painter.setFont(font_);
        painter.drawText(rect, Qt::AlignCenter, input_);
    }

    void keyPress(QKeyEvent* press, Game& game)
    {
        const auto mod = press->modifiers();
        const auto key = press->key();
        if (mod == Qt::ControlModifier)
        {
            if (key == Qt::Key_N)
            {
                ypos_ = game.moveDown();
            }
            else if (key == Qt::Key_P)
            {
                ypos_ = game.moveUp();
            }
        }
        else if (key == Qt::Key_Space || key == Qt::Key_Return)
        {
            game.fire(value_);
            input_.clear();
            value_ = 0;

        }
        else if (input_.size() < 4)
        {
            if (key >= 0x41 && key <= 0x5a)
            {
                QChar c(key);
                input_.append(c);
                value_ <<= 8;
                value_ |= key;
            }
        }
    }
private:
    bool isBlank()
    {
        //return (blink_++ % 2);
        return false;
    }

private:
    unsigned ypos_;
    unsigned blink_;    
    unsigned value_;
    QString  input_;
    QFont    font_;
};


class GameWidget::Debug
{
public:
    void paint(QPainter& painter, PaintState& state, Game& game)
    {
        QPen pen;
        pen.setWidth(1);
        pen.setColor(Qt::darkGray);
        painter.setPen(pen);

        const auto num_cols = game.width();
        const auto num_rows = game.height();

        const auto dim = state.getScale();
        const auto ori = state.toViewSpace(QPoint(0, 0));
        const auto end = state.toViewSpace(QPoint(num_cols, num_rows));

        // draw grid lines parallel to the X axis
        for (unsigned i=0; i<=num_rows; ++i)
        {
            const auto y = ori.y() + i * dim.y();
            painter.drawLine(ori.x(), y, end.x(), y);
        }

        // draw gri lines pallel to the Y axis
        for (unsigned i=0; i<=num_cols; ++i)
        {
            const auto x = ori.x() + i * dim.x();
            painter.drawLine(x, ori.y(), x, end.y());
        }
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

    void paint(QPainter& painter, PaintState& state, Game& game)
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
            "Ctrl + N - Move down\n"
            "Ctrl + P - Move up  \n"
            "Space - Fire        \n"
            "Esc - Exit          \n\n"
            "Press Space to play!\n");
    }
private:
    QFont font_;
};

GameWidget::GameWidget(QWidget* parent) : QWidget(parent)
{
    game_.reset(new Game(20, 10));

    game_->on_invader_kill = [&](const Game::invader& i, const Game::missile& m) 
    {

        missiles_.erase(m.identity);
        invaders_.erase(i.identity);

        qDebug() << "Invader kill";
    };

    game_->on_missile_fail = [&](const Game::invader& i, const Game::missile& m) 
    {
        missiles_.erase(m.identity);

        qDebug() << "Missile fail";
    };


    game_->on_invader_spawn = [&](const Game::invader& inv) {
        std::unique_ptr<Invader> invader(new Invader(inv.cell, inv.row, inv.character));
        invaders_[inv.identity] = std::move(invader);
    };

    game_->on_invader_victory = [&](const Game::invader& inv) {
        invaders_.erase(inv.identity);
    };

    game_->on_missile_fire = [&](const Game::missile& m) {
        std::unique_ptr<Missile> missile(new Missile(m.cell, m.row, m.value));
        missiles_[m.identity] = std::move(missile);
    };

    game_->on_missile_prune = [&](const Game::missile& m) {
        missiles_.erase(m.identity);
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
    missiles_.clear();

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
        game_->tick();
        const auto& missiles = game_->missiles();
        const auto& invaders = game_->invaders();
        for (const auto& m : missiles)
        {
            auto it = missiles_.find(m.identity);
            Q_ASSERT(it != std::end(missiles_));
            it->second->setPosition(m.cell, m.row);
        }

        for (const auto& i : invaders)
        {
            auto it = invaders_.find(i.identity);
            Q_ASSERT(it != std::end(invaders_));
            it->second->setPosition(i.cell, i.row);
        }
        time_delta_ = 0;
    }

    update();

    time_stamp_ = time;
}

void GameWidget::paintEvent(QPaintEvent* paint)
{
    PaintState state(rect());

    QPainter painter(this);
    painter.setRenderHints(QPainter::HighQualityAntialiasing);

    const auto widget_width_px  = width();
    const auto widget_height_px = height();
    const auto num_cols = game_->width();
    const auto num_rows = game_->height() + 1; 
    const auto cell_width  = widget_width_px / num_cols;
    const auto cell_height = widget_height_px / num_rows;

    state.setScale(QPoint(cell_width, cell_height));

    // we divide the widget's client area into a equal sized cells
    // according to the game's size. We also add one extra row 
    // for HUD display. (at the top of the screen)

    const auto time  = timer_.elapsed();
    const auto delta = time - time_stamp_;

    background_->update(delta);
    background_->paint(painter, state, *game_);
    if (welcome_)
    {
        welcome_->update(delta);
        welcome_->paint(painter, state, *game_);
        return;
    }

    display_->paint(painter, state, *game_);

    state.setUnitOffset(QPoint(0, 1));

    player_->paint(painter, state, *game_);

    const auto tick = 1000 / speed_;

    for (auto& pair : missiles_)
    {
        auto& missile = pair.second;
        missile->update(time_delta_, tick);
        missile->paint(painter, state, *game_);
    }

    for (auto& pair : invaders_)
    {
        auto& invader = pair.second;
        invader->update(time_delta_, tick);
        invader->paint(painter, state, *game_);
    }
}

void GameWidget::keyPressEvent(QKeyEvent* press)
{
    const auto key = press->key();
    const auto mod = press->modifiers();

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