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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"

#include "warnpop.h"

#include <algorithm>

#include "editor/gui/utility.h"
#include "editor/gui/dlgtexturerect.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/drawing.h"

namespace gui
{

DlgTextureRect::DlgTextureRect(QWidget* parent, const gfx::FRect& rect, std::unique_ptr<gfx::TextureSource> texture)
    : QDialog(parent)
    , mRect(rect)
{
    mUI.setupUi(this);
    mUI.widget->onPaintScene = std::bind(&DlgTextureRect::OnPaintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMouseMove = std::bind(&DlgTextureRect::OnMouseMove,
        this, std::placeholders::_1);
    mUI.widget->onMousePress = std::bind(&DlgTextureRect::OnMousePress,
        this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&DlgTextureRect::OnMouseRelease,
        this, std::placeholders::_1);
    // do the graphics dispose in finished handler which is triggered
    // regardless whether we do accept/reject or the user clicks the X
    // or presses Esc.
    connect(this, &QDialog::finished, this, &DlgTextureRect::finished);
    // render on timer.
    connect(&mTimer, &QTimer::timeout, this, &DlgTextureRect::timer);

    mUI.widget->onInitScene = [&](unsigned, unsigned) {
        mTimer.setInterval(1000.0/60.0);
        mTimer.start();
    };

    const auto& bitmap = texture->GetData();
    if (bitmap != nullptr)
    {
        mWidth  = bitmap->GetWidth();
        mHeight = bitmap->GetHeight();
    }
    gfx::MaterialClass klass;
    klass.AddTexture(std::move(texture));
    klass.SetType(gfx::MaterialClass::Type::Texture);
    klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    mMaterial = gfx::CreateMaterialInstance(klass);

    const auto& rc = mRect.Expand(gfx::USize(mWidth, mHeight));
    mUI.edtX->setText(QString::number(rc.GetX()));
    mUI.edtY->setText(QString::number(rc.GetY()));
    mUI.edtW->setText(QString::number(rc.GetWidth()));
    mUI.edtH->setText(QString::number(rc.GetHeight()));
}

void DlgTextureRect::on_btnAccept_clicked()
{
    accept();
}

void DlgTextureRect::on_btnCancel_clicked()
{
    reject();
}

void DlgTextureRect::finished()
{
    mUI.widget->dispose();
}
void DlgTextureRect::timer()
{
    mUI.widget->triggerPaint();
}

void DlgTextureRect::OnPaintScene(gfx::Painter& painter, double)
{
    const float width  = mUI.widget->width();
    const float height = mUI.widget->height();
    const float scale = std::min(width / mWidth,
        height / mHeight);

    painter.SetViewport(0, 0, width, height);

    const auto render_width = mWidth * scale;
    const auto render_height = mHeight * scale;
    const auto xpos = (width - render_width) * 0.5f;
    const auto ypos = (height - render_height) * 0.5f;
    gfx::FillRect(painter, gfx::FRect(xpos, ypos, render_width, render_height), *mMaterial);

    // draw the cross hairs
    const auto x = mCurrentPoint.x();
    const auto y = mCurrentPoint.y();
    gfx::DrawLine(painter, gfx::FPoint(x, 0), gfx::FPoint(x, height),
        gfx::Color::HotPink);
    gfx::DrawLine(painter, gfx::FPoint(0, y), gfx::FPoint(width, y),
        gfx::Color::HotPink);

    if (mDragging)
    {
        const auto width  = mCurrentPoint.x() - mStartPoint.x();
        const auto height = mCurrentPoint.y() - mStartPoint.y();
        if (width <= 0 || height <= 0)
            return;
        gfx::DrawRectOutline(painter, gfx::FRect(mStartPoint.x(), mStartPoint.y(),
            width, height), gfx::Color::Green, 1.0f);
    }
    else
    {
        // go from normalized rect to texels.
        gfx::FRect rect = mRect.Expand(gfx::FSize(mWidth*scale, mHeight*scale));
        rect.Translate(xpos, ypos);
        gfx::DrawRectOutline(painter, rect, gfx::Color::Green, 1.0f);
    }
}

void DlgTextureRect::OnMousePress(QMouseEvent* mickey)
{
    mDragging = true;
    mStartPoint = mickey->pos();
}
void DlgTextureRect::OnMouseMove(QMouseEvent* mickey)
{
    mCurrentPoint = mickey->pos();
}
void DlgTextureRect::OnMouseRelease(QMouseEvent* mickey)
{
    mDragging = false;

    const float width  = mUI.widget->width();
    const float height = mUI.widget->height();
    const float scale = std::min(width / mWidth,
        height / mHeight);
    const auto render_width = mWidth * scale;
    const auto render_height = mHeight * scale;
    const auto xpos = (width - render_width) * 0.5f;
    const auto ypos = (height - render_height) * 0.5f;

    const auto w = mCurrentPoint.x() - mStartPoint.x();
    const auto h = mCurrentPoint.y() - mStartPoint.y();
    const auto x = mStartPoint.x() - xpos;
    const auto y = mStartPoint.y() - ypos;
    if (w <= 0 || h <= 0)
        return;

    const auto& rc = gfx::FRect(x/scale, y/scale, w/scale, h/scale);
    // store in normalized form.
    mRect = rc.Normalize(gfx::FSize(mWidth, mHeight));
    // update the ui with the selection
    mUI.edtX->setText(QString::number((int)rc.GetX()));
    mUI.edtY->setText(QString::number((int)rc.GetY()));
    mUI.edtW->setText(QString::number((int)rc.GetWidth()));
    mUI.edtH->setText(QString::number((int)rc.GetHeight()));
}

} // namespace

