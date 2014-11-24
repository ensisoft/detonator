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
#  include <QResource>
#include "../warnpop.h"

#include "gamewidget.h"

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

struct PaintState {
    QRect rect;
};

class GameWidget::Entity
{
public:
    virtual ~Entity() = default;

    virtual void paint(QPainter& painter, PaintState& state) = 0;
protected:
private:
};

class GameWidget::Background : public Entity
{
public:
    virtual void paint(QPainter& painter, PaintState& state) override
    {
        QBrush brush(Qt::black);
        painter.fillRect(state.rect, brush);
    }
private:

};

class GameWidget::Display : public Entity
{
public:
    Display() : arcade_(loadFont(":/fonts/ARCADE.TTF", "Arcade"))
    {
        arcade_.setPointSize(40);
    }

    virtual void paint(QPainter& painter, PaintState& state) override
    {
        QPen pen;
        pen.setColor(Qt::red);
        pen.setWidth(1);

        painter.setFont(arcade_);
        painter.setPen(pen);
        painter.drawText(100, 100, "foobar");
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

};


GameWidget::GameWidget(QWidget* parent) : QWidget(parent)
{
    background_.reset(new Background);
    display_.reset(new Display);
    //entities_.emplace_back(new Background);
}

GameWidget::~GameWidget()
{}

void GameWidget::paintEvent(QPaintEvent* paint)
{
    PaintState state;
    state.rect = rect();

    QPainter painter(this);
    painter.setRenderHints(QPainter::HighQualityAntialiasing);

    background_->paint(painter, state);
    display_->paint(painter, state);

    // for (auto& entity : entities_)
    // {
    //     entity->paint(painter, state);
    // }
}

void GameWidget::keyPressEvent(QKeyEvent* press)
{}

} // invaders