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

#include "../config.h"
#include "../warnpush.h"
#  include <QtGui/QPaintEvent>
#  include <QtGui/QKeyEvent>
#  include <QtGui/QPen>
#  include <QtGui/QBrush>
#  include <QtGui/QColor>
#  include <QtGui/QPainter>
#  include <QtGui/QFontDatabase>
#  include <QtGui/QCursor>
#  include <QResource>
#  include <QtDebug>
#include "../warnpop.h"

#include "gamewidget.h"
#include "game.h"

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

    QRect rect() const 
    { return rect_; }

    QPoint toViewSpace(QPoint world)
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

class GameWidget::Entity
{
public:
    virtual ~Entity() = default;

    virtual void paint(QPainter& painter, PaintState& state, Game& game) = 0;
protected:
private:
};

class GameWidget::Background
{
public:
    void paint(QPainter& painter, PaintState& state, Game& game)
    {
        QBrush brush(Qt::black);
        painter.fillRect(state.rect(), brush);
    }
private:

};

class GameWidget::Display
{
public:
    Display() : arcade_(loadFont(":/fonts/ARCADE.TTF", "Arcade"))
    {}

    void paint(QPainter& painter, PaintState& state, Game& game)
    {
        const auto dim = state.getScale();
        const auto beg = state.toViewSpace(QPoint(0, 0));
        const auto end = state.toViewSpace(QPoint(1, 1));

        QPen pen;
        pen.setColor(Qt::darkGreen);
        pen.setWidth(1);
        arcade_.setPixelSize(dim.y());

        painter.setFont(arcade_);
        painter.setPen(pen);
        painter.drawText(end, 
            QString("Score %1 High Score %2").arg(0).arg(1));
    }
private:
    QFont arcade_;
};

class GameWidget::Starfield
{
public:
    Starfield()
    {}
   ~Starfield()
    {}


};

class GameWidget::Player 
{
public:
    Player() : ypos_(0), blink_(0)
    {}

    void paint(QPainter& painter, PaintState& state, Game& game)
    {
        QPen pen;
        pen.setColor(Qt::darkRed);
        pen.setWidth(10);
        painter.setPen(pen);

        const auto beg = state.toViewSpace(QPoint(0, ypos_));
        const auto end = state.toViewSpace(QPoint(0, ypos_+1));
        const auto dim = state.getScale();
        if (!isBlank())
            painter.drawLine(beg, end);

        QFont font;
        font.setPixelSize(dim.y());
        pen.setWidth(2);
        painter.setPen(pen);
        painter.drawText(end, text_);
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
            qDebug() << "ypos " << ypos_;
        }
        else if (key == Qt::Key_Space || key == Qt::Key_Return)
        {
            text_.clear();
        }
        else
        {
            QChar c(key);
            if (c.isPrint())
                text_.append(c);
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
    QString text_;
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

GameWidget::GameWidget(QWidget* parent) : QWidget(parent)
{
    game_.reset(new Game(20, 10));

    background_.reset(new Background);
    display_.reset(new Display);
    debug_.reset(new Debug);
    player_.reset(new Player);

    // enable keyboard events
    setFocusPolicy(Qt::StrongFocus);
    startTimer(0);
}

GameWidget::~GameWidget()
{}

void GameWidget::timerEvent(QTimerEvent* timer)
{
    update();
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

    background_->paint(painter, state, *game_);
    display_->paint(painter, state, *game_);


    state.setUnitOffset(QPoint(0, 1));
    debug_->paint(painter, state, *game_);
    player_->paint(painter, state, *game_);
    // for (auto& entity : entities_)
    // {
    //     entity->paint(painter, state);
    // }
}

void GameWidget::keyPressEvent(QKeyEvent* press)
{
    player_->keyPress(press, *game_);

    update();
}

} // invaders