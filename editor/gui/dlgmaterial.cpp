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

#include "editor/app/workspace.h"
#include "editor/app/eventlog.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/utility.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/drawing.h"

namespace {
    constexpr unsigned BoxSize = 100;
    constexpr unsigned BoxMargin = 20;
} // namespace

namespace gui
{

DlgMaterial::DlgMaterial(QWidget* parent, const app::Workspace* workspace, const app::AnyString& material)
  : QDialog(parent)
  , mSelectedMaterialId(material)
  , mWorkspace(workspace)
{
    mUI.setupUi(this);

    setMouseTracking(true);
    // do the graphics dispose in finished handler which is triggered
    // regardless whether we do accept/reject or the user clicks the X
    // or presses Esc.
    connect(this, &QDialog::finished, mUI.widget, &GfxWidget::dispose);
    // render on timer
    connect(&mTimer, &QTimer::timeout, mUI.widget, &GfxWidget::triggerPaint);

    mUI.widget->onPaintScene = std::bind(&DlgMaterial::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onInitScene = [&](unsigned, unsigned) {
        mTimer.setInterval(1000.0/60.0);
        mTimer.start();
    };
    mUI.widget->onKeyPress = std::bind(&DlgMaterial::KeyPress, this, std::placeholders::_1);
    mUI.widget->onMousePress = std::bind(&DlgMaterial::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseWheel = std::bind(&DlgMaterial::MouseWheel, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&DlgMaterial::MouseDoubleClick, this, std::placeholders::_1);

    mElapsedTimer.start();

    QByteArray geometry;
    if (mWorkspace->GetUserProperty("dlg_material_geometry", &geometry))
        this->restoreGeometry(geometry);

    mMaterials = mWorkspace->ListAllMaterials();
}

void DlgMaterial::on_btnAccept_clicked()
{
    const_cast<app::Workspace*>(mWorkspace)->SetUserProperty("dlg_material_geometry", saveGeometry());

    accept();
}
void DlgMaterial::on_btnCancel_clicked()
{
    const_cast<app::Workspace*>(mWorkspace)->SetUserProperty("dlg_material_geometry", saveGeometry());

    reject();
}

void DlgMaterial::on_vScroll_valueChanged()
{
    mScrollOffsetRow = mUI.vScroll->value();
}

void DlgMaterial::PaintScene(gfx::Painter& painter, double secs)
{
    const auto time_milliseconds = mElapsedTimer.elapsed();
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    const auto num_visible_cols = width / (BoxSize + BoxMargin);
    const auto num_visible_rows = height / (BoxSize + BoxMargin);

    if (mFirstPaint)
    {
        if (!mSelectedMaterialId.isEmpty())
        {
            size_t selected_material_row = 0;
            size_t selected_material_col = 0;

            for (size_t i=0; i<mMaterials.size(); ++i)
            {
                selected_material_col = i % num_visible_cols;
                selected_material_row = i / num_visible_cols;
                if (mMaterials[i].id == mSelectedMaterialId)
                    break;
            }
            const auto row_height = BoxSize + BoxMargin;
            const auto row_ypos   = (selected_material_row + 1 ) * row_height;
            if (row_ypos > height)
            {
                mScrollOffsetRow = (row_ypos - height) / row_height + 1;
                QSignalBlocker s(mUI.vScroll);
                mUI.vScroll->setValue(mScrollOffsetRow);
            }
        }
        mFirstPaint = false;
    }

    const auto xoffset  = (width - ((BoxSize + BoxMargin) * num_visible_cols)) / 2;
    const auto yoffset  = -mScrollOffsetRow * (BoxSize + BoxMargin);
    unsigned index = 0;

    SetValue(mUI.groupBox, "Material Library");

    for (index=0; index<mMaterials.size(); ++index)
    {
        const auto* resource = mMaterials[index].resource;

        auto klass = app::ResourceCast<gfx::MaterialClass>(*resource).GetSharedResource();

        const auto col = index % num_visible_cols;
        const auto row = index / num_visible_cols;
        const auto xpos = xoffset + col * (BoxSize + BoxMargin);
        const auto ypos = yoffset + row * (BoxSize + BoxMargin);

        gfx::FRect  rect;
        rect.Resize(BoxSize, BoxSize);
        rect.Move(xpos, ypos);
        rect.Translate(BoxMargin*0.5f, BoxMargin*0.5f);
        if (!DoesIntersect(rect, gfx::FRect(0.0f, 0.0f, width, height)))
            continue;

        gfx::MaterialClassInst material(klass);
        material.SetRuntime(time_milliseconds / 1000.0);
        gfx::FillRect(painter, rect, material);

        if (mMaterials[index].id != mSelectedMaterialId)
            continue;

        gfx::DrawRectOutline(painter, rect, gfx::Color::Green, 2.0f);
        SetValue(mUI.groupBox, app::toString("Material Library - %1", mMaterials[index].name));
    }

    const auto num_total_rows = index / num_visible_cols + 1;
    if (num_total_rows > num_visible_rows)
    {
        const auto num_scroll_steps = num_total_rows - num_visible_rows;
        QSignalBlocker blocker(mUI.vScroll);
        mUI.vScroll->setVisible(true);
        mUI.vScroll->setMaximum(num_scroll_steps);
        if (num_visible_rows != mNumVisibleRows)
        {
            mUI.vScroll->setValue(0);
            mNumVisibleRows = num_visible_rows;
        }
    }
    else
    {
        mUI.vScroll->setVisible(false);
    }
}

void DlgMaterial::MousePress(QMouseEvent* mickey)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();

    const auto num_cols = width / (BoxSize + BoxMargin);
    const auto xoffset  = (width - ((BoxSize + BoxMargin) * num_cols)) / 2;
    const auto yoffset  = mScrollOffsetRow * (BoxSize + BoxMargin);

    const auto widget_xpos = mickey->pos().x();
    const auto widget_ypos = mickey->pos().y();
    const auto col = (widget_xpos - xoffset) / (BoxSize + BoxMargin);
    const auto row = (widget_ypos + yoffset) / (BoxSize + BoxMargin);
    const auto index = row * num_cols + col;

    if (index >= mMaterials.size())
        return;
    mSelectedMaterialId = mMaterials[index].id;
}

void DlgMaterial::MouseDoubleClick(QMouseEvent* mickey)
{
    const_cast<app::Workspace*>(mWorkspace)->SetUserProperty("dlg_material_geometry", saveGeometry());

    MousePress(mickey);
    accept();
}

void DlgMaterial::MouseWheel(QWheelEvent* wheel)
{
    const QPoint& num_degrees = wheel->angleDelta() / 8;
    const QPoint& num_steps = num_degrees / 15;
    // only consider the wheel scroll steps on the vertical
    // axis for zooming.
    // if steps are positive the wheel is scrolled away from the user
    // and if steps are negative the wheel is scrolled towards the user.
    const int num_zoom_steps = num_steps.y();

    unsigned max = mUI.vScroll->maximum();

    for (int i=0; i<std::abs(num_zoom_steps); ++i)
    {
        if (num_zoom_steps > 0)
            mScrollOffsetRow = mScrollOffsetRow > 0 ? mScrollOffsetRow - 1 : 0;
        else if (num_zoom_steps < 0)
            mScrollOffsetRow = mScrollOffsetRow < max ? mScrollOffsetRow + 1 : mScrollOffsetRow;
    }

    QSignalBlocker blocker(mUI.vScroll);
    mUI.vScroll->setValue(mScrollOffsetRow);
}

bool DlgMaterial::KeyPress(QKeyEvent* key)
{
    if (key->key() == Qt::Key_Escape)
    {
        reject();
        return true;
    }
    return false;
}

} // namespace
