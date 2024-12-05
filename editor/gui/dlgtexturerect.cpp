// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#include "warnpop.h"

#include <algorithm>

#include "editor/app/workspace.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "editor/gui/dlgtexturerect.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/drawing.h"
#include "graphics/material_class.h"

namespace gui
{

DlgTextureRect::DlgTextureRect(QWidget* parent, const gfx::FRect& rect, std::unique_ptr<gfx::TextureSource> texture)
    : FUDialog(parent)
    , mRect(rect)
{
    mUI.setupUi(this);

    SetupFU(this);

    mUI.widget->onPaintScene   = std::bind(&DlgTextureRect::OnPaintScene,   this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMouseMove    = std::bind(&DlgTextureRect::OnMouseMove,    this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&DlgTextureRect::OnMousePress,   this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&DlgTextureRect::OnMouseRelease, this, std::placeholders::_1);
    mUI.widget->onZoomOut = [this]() {
        float zoom = GetValue(mUI.zoom);
        SetValue(mUI.zoom, zoom - 0.1f);
    };
    mUI.widget->onZoomIn = [this]() {
        float zoom = GetValue(mUI.zoom);
        SetValue(mUI.zoom, zoom + 0.1f);
    };
    mUI.widget->onInitScene = [&](unsigned, unsigned) {
        mTimer.setInterval(1000.0/60.0);
        mTimer.start();
    };
    mUI.widget->onKeyPress = [this](QKeyEvent* key) {
        if (key->key() == Qt::Key_Escape) {
            reject();
            return true;
        }
        return false;
    };

    // do the graphics dispose in finished handler which is triggered
    // regardless whether we do accept/reject or the user clicks the X
    // or presses Esc.
    this->FUDialog::finished = [this]() {
        mUI.widget->dispose();
    };
    // render on timer.
    connect(&mTimer, &QTimer::timeout, this, &DlgTextureRect::timer);

    const auto& bitmap = texture->GetData();
    if (bitmap != nullptr)
    {
        mWidth  = bitmap->GetWidth();
        mHeight = bitmap->GetHeight();
    }
    gfx::TextureMap2DClass klass(gfx::MaterialClass::Type::Texture);
    klass.SetTexture(std::move(texture));
    klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    mMaterial = gfx::CreateMaterialInstance(klass);

    const auto& rc = mRect.Expand(gfx::USize(mWidth, mHeight));
    SetValue(mUI.X, rc.GetX());
    SetValue(mUI.Y, rc.GetY());
    SetValue(mUI.W, rc.GetWidth());
    SetValue(mUI.H, rc.GetHeight());
    SetValue(mUI.zoom, 1.0f);
}

void DlgTextureRect::LoadState(const app::Workspace* workspace,
                               const app::AnyString& dialog, const app::AnyString& uri)
{
    int xpos = 0;
    int ypos = 0;
    GetUserProperty(*workspace, PropertyKey(dialog, "zoom"),  mUI.zoom);
    GetUserProperty(*workspace, PropertyKey(dialog, "color"), mUI.widget);
    GetUserProperty(*workspace, PropertyKey(dialog, "xpos"),  &xpos);
    GetUserProperty(*workspace, PropertyKey(dialog, "ypos"),  &ypos);
    mTrackingOffset = QPoint(xpos, ypos);

    // if the texture changed then reset the tracking offset
    // since it's possible that the dimensions are totally different
    // and our tracking offset would be such that the texture would
    // not display.
    QString previous_uri;
    if (GetUserProperty(*workspace, PropertyKey(dialog, "uri"), &previous_uri))
    {
        if (previous_uri != uri)
        {
            mTrackingOffset = QPoint(0.0f, 0.0f);
            SetValue(mUI.zoom, 1.0f);
        }
    }


}
void DlgTextureRect::SaveState(app::Workspace* workspace,
                               const app::AnyString& dialog, const app::AnyString& uri) const
{
    SetUserProperty(*workspace, PropertyKey(dialog, "zoom"), mUI.zoom);
    SetUserProperty(*workspace, PropertyKey(dialog, "color"), mUI.widget);
    SetUserProperty(*workspace, PropertyKey(dialog, "xpos"), mTrackingOffset.x());
    SetUserProperty(*workspace, PropertyKey(dialog, "ypos"), mTrackingOffset.y());
    SetUserProperty(*workspace, PropertyKey(dialog, "uri"), uri);
}


void DlgTextureRect::on_btnAccept_clicked()
{
    accept();
}

void DlgTextureRect::on_btnCancel_clicked()
{
    reject();
}

void DlgTextureRect::on_X_valueChanged(int)
{
    UpdateRect();
}
void DlgTextureRect::on_Y_valueChanged(int)
{
    UpdateRect();
}
void DlgTextureRect::on_W_valueChanged(int)
{
    UpdateRect();
}
void DlgTextureRect::on_H_valueChanged(int)
{
    UpdateRect();
}

void DlgTextureRect::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void DlgTextureRect::timer()
{
    mUI.widget->triggerPaint();
}

void DlgTextureRect::UpdateRect()
{
    const int x = GetValue(mUI.X);
    const int y = GetValue(mUI.Y);
    const int w = GetValue(mUI.W);
    const int h = GetValue(mUI.H);
    mRect = gfx::FRect(x, y, w, h).Normalize(gfx::FSize(mWidth, mHeight));
}

void DlgTextureRect::OnPaintScene(gfx::Painter& painter, double)
{
    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    const float width  = mUI.widget->width();
    const float height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    const float zoom   = GetValue(mUI.zoom);
    const float img_width  = mWidth * zoom;
    const float img_height = mHeight * zoom;
    const auto xpos = (width - img_width) * 0.5f;
    const auto ypos = (height - img_height) * 0.5f;

    gfx::FRect img(0.0f, 0.0f, img_width, img_height);
    img.Translate(xpos, ypos);
    img.Translate(mTrackingOffset.x(), mTrackingOffset.y());
    gfx::FillRect(painter, img, *mMaterial);

    // draw the cross-hairs
    const int xp = mCurrentPoint.x();
    const int yp = mCurrentPoint.y();
    gfx::DebugDrawLine(painter, gfx::FPoint(xp, 0), gfx::FPoint(xp, height), gfx::Color::HotPink);
    gfx::DebugDrawLine(painter, gfx::FPoint(0, yp), gfx::FPoint(width, yp), gfx::Color::HotPink);

    const int x = (xp - xpos - mTrackingOffset.x()) / zoom;
    const int y = (yp - ypos - mTrackingOffset.y()) / zoom;
    char hallelujah[128] = {};
    std::snprintf(hallelujah, sizeof(hallelujah), "%d, %d", x, y);
    ShowMessage(hallelujah, painter);

    // go from normalized rect to texels.
    gfx::FRect rect = mRect.Expand(gfx::FSize(img_width, img_height));
    rect.Translate(xpos, ypos);
    rect.Translate(mTrackingOffset.x(), mTrackingOffset.y());
    gfx::DrawRectOutline(painter, rect, gfx::Color::Green, 1.0f);
}

void DlgTextureRect::OnMousePress(QMouseEvent* mickey)
{
    if (mickey->button() == Qt::LeftButton)
        mState = State::Selecting;
    else if (mickey->button() == Qt::RightButton)
        mState = State::Tracking;

    mStartPoint = mickey->pos();
}
void DlgTextureRect::OnMouseMove(QMouseEvent* mickey)
{
    mCurrentPoint = mickey->pos();

    if (mState == State::Selecting)
    {
        const float width  = mUI.widget->width();
        const float height = mUI.widget->height();

        const float zoom   = GetValue(mUI.zoom);
        const float img_width  = mWidth * zoom;
        const float img_height = mHeight * zoom;
        const auto xpos = (width - img_width) * 0.5f;
        const auto ypos = (height - img_height) * 0.5f;
        const QPoint origin(xpos, ypos);

        const auto start = (mStartPoint - origin - mTrackingOffset) / zoom;
        const auto current = (mCurrentPoint - origin - mTrackingOffset) / zoom;
        const auto x = start.x();
        const auto y = start.y();
        const auto w = (current - start).x();
        const auto h = (current - start).y();
        if (w <= 0 || h <= 0)
            return;

        gfx::FRect rc(x, y, w, h);
        mRect = rc.Normalize(gfx::FSize(mWidth, mHeight));
        // update the ui with the selection
        SetValue(mUI.X, rc.GetX());
        SetValue(mUI.Y, rc.GetY());
        SetValue(mUI.W, rc.GetWidth());
        SetValue(mUI.H, rc.GetHeight());
    }
    else if (mState == State::Tracking)
    {
        mTrackingOffset += (mCurrentPoint - mStartPoint);
        mStartPoint = mCurrentPoint;
    }
}
void DlgTextureRect::OnMouseRelease(QMouseEvent* mickey)
{
    mState = State::Nada;
}

} // namespace

