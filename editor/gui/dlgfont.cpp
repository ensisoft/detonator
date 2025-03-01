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
#include "editor/gui/dlgfont.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/drawing.h"
#include "graphics/text_material.h"
#include "base/utility.h"
#include "base/math.h"

namespace {
    constexpr unsigned BoxWidth  = 250;
    constexpr unsigned BoxHeight = 130;
    constexpr unsigned BoxMargin = 20;
} // namespace

namespace gui
{

DlgFont::DlgFont(QWidget* parent, const app::Workspace* workspace, const app::AnyString& font, const DisplaySettings& disp)
  : QDialog(parent)
  , mWorkspace(workspace)
  , mDisplay(disp)
  , mSelectedFontURI(font)
{
    base::AppendVector(mFonts, ListWSFonts(workspace->GetDir()));
    base::AppendVector(mFonts, ListAppFonts());

    if (!mSelectedFontURI.isEmpty())
    {
        bool found_font = false;
        for (const auto& font : mFonts)
        {
            if (font == mSelectedFontURI)
            {
                found_font = true;
                break;
            }
        }
        if (!found_font)
            mFonts.push_back(mSelectedFontURI);
    }

    mAllFonts = mFonts;

    // total fudge. the current size of the box for visualizing the
    // text is just a "random guess value" that just looked okay now at 18px.
    // the size that fit the text at any given font size is a different
    // story. since we don't have proper font metrics yet (could actually
    // use Qt font metrics here) we're just going to fudge this by scaling
    // up from 18px font to whatever is given now.
    mBoxWidth  = math::lerp(BoxWidth, BoxWidth*3, (disp.font_size-18.0f)/(74.0f-18.0f));
    mBoxHeight = math::lerp(BoxHeight, BoxHeight*3, (disp.font_size-18.0f)/(74.0f-18.0f));

    mUI.setupUi(this);

    setMouseTracking(true);
    // do the graphics dispose in finished handler which is triggered
    // regardless whether we do accept/reject or the user clicks the X
    // or presses Esc.
    connect(this, &QDialog::finished, mUI.widget, &GfxWidget::dispose);

    mUI.widget->onPaintScene = std::bind(&DlgFont::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onInitScene = [this](unsigned, unsigned) {
        const float width  = mUI.widget->width();
        const float height = mUI.widget->height();
        if ((mBoxWidth  + BoxMargin) > width || (mBoxHeight + BoxMargin) > height)
        {
            const auto xs = width / (mBoxWidth + BoxMargin);
            const auto ys = height / (mBoxHeight + BoxMargin);
            SetValue(mUI.zoom, std::min(xs, ys));
        }
        mUI.widget->StartPaintTimer();
    };
    mUI.widget->onKeyPress = std::bind(&DlgFont::KeyPress, this, std::placeholders::_1);
    mUI.widget->onMousePress = std::bind(&DlgFont::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseWheel = std::bind(&DlgFont::MouseWheel, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&DlgFont::MouseDoubleClick, this, std::placeholders::_1);

    SetValue(mUI.zoom, 1.0f);
}

void DlgFont::on_btnAccept_clicked()
{
    accept();
}
void DlgFont::on_btnCancel_clicked()
{
    reject();
}

void DlgFont::on_vScroll_valueChanged()
{
    mScrollOffsetRow = mUI.vScroll->value();
}

void DlgFont::on_filter_textChanged(const QString& text)
{
    mScrollOffsetRow = 0;
    mNumVisibleRows  = 0;

    if (text.isEmpty())
    {
        mFonts = mAllFonts;
    }
    else
    {
        mFonts.clear();
        for (const auto& fontUri : mAllFonts)
        {
            if (fontUri.contains(text, Qt::CaseInsensitive))
                mFonts.push_back(fontUri);
        }
    }
}

void DlgFont::PaintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const float zoom = GetValue(mUI.zoom);

    const unsigned box_width = mBoxWidth * zoom;
    const unsigned box_height = mBoxHeight * zoom;

    // don't render if the window is too small
    if (width < (box_width + BoxMargin) || height < (box_height + BoxMargin))
        return;

    const auto num_visible_cols = width / (box_width + BoxMargin);
    const auto num_visible_rows = height / (box_height + BoxMargin);
    const auto xoffset  = (width - ((box_width + BoxMargin) * num_visible_cols)) / 2;
    const auto yoffset  = -(float)mScrollOffsetRow * (box_height + BoxMargin);
    unsigned index = 0;

    //mMaterialIds.clear();
    if (mSelectedFontURI.isEmpty())
        SetValue(mUI.groupBox, "Font Library");
    else SetValue(mUI.groupBox, tr("Font Library - %1").arg(mSelectedFontURI));

    for (const auto& uri : mFonts)
    {
        const auto col = index % num_visible_cols;
        const auto row = index / num_visible_cols;
        const auto xpos = xoffset + col * (box_width + BoxMargin);
        const auto ypos = yoffset + row * (box_height + BoxMargin);

        gfx::FRect rect;
        rect.Resize(box_width, box_height);
        rect.Move(xpos, ypos);
        rect.Translate(BoxMargin * 0.5f, BoxMargin * 0.5f);

        if (!base::Contains(mFailedFonts, uri))
        {
            gfx::TextBuffer::Text text_and_style;
            text_and_style.text = "Quick brown fox\njumps over\nthe lazy dog.";
            text_and_style.font = app::ToUtf8(uri);
            text_and_style.fontsize = mDisplay.font_size;
            text_and_style.underline = mDisplay.underline;
            text_and_style.lineheight = 1.0f;

            gfx::TextBuffer text;
            text.SetBufferSize(box_width, box_height);
            text.SetText(std::move(text_and_style));

            gfx::TextMaterial material(std::move(text));
            material.SetRuntime(mUI.widget->GetTime());
            material.SetPointSampling(true);
            material.SetColor(ToGfx(mDisplay.text_color));

            if (!gfx::FillRect(painter, rect, material))
                mFailedFonts.insert(uri);
        }
        else
        {
            ShowError("Limited font\nor font raster error.", rect, painter, mDisplay.font_size);
        }

        if (uri == mSelectedFontURI)
        {
            gfx::DrawRectOutline(painter, rect, gfx::Color::Green, 2.0f);
        }
        ++index;
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

void DlgFont::MousePress(QMouseEvent* mickey)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const float zoom = GetValue(mUI.zoom);
    const unsigned box_width = mBoxWidth * zoom;
    const unsigned box_height = mBoxHeight * zoom;

    const auto num_cols = width / (box_width + BoxMargin);
    const auto xoffset  = (width - ((box_width + BoxMargin) * num_cols)) / 2;
    const auto yoffset  = mScrollOffsetRow * (box_height + BoxMargin);

    const auto widget_xpos = mickey->pos().x();
    const auto widget_ypos = mickey->pos().y();
    const auto col = (widget_xpos - xoffset) / (box_width + BoxMargin);
    const auto row = (widget_ypos + yoffset) / (box_height + BoxMargin);
    const auto index = row * num_cols + col;

    if (index >= mFonts.size())
        return;
    mSelectedFontURI = mFonts[index];
}

void DlgFont::MouseDoubleClick(QMouseEvent* mickey)
{
    MousePress(mickey);
    accept();
}

void DlgFont::MouseWheel(QWheelEvent* wheel)
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

bool DlgFont::KeyPress(QKeyEvent* key)
{
    const auto sym = key->key();

    if (sym == Qt::Key_Escape)
    {
        reject();
        return true;
    }
    else if (sym == Qt::Key_Return)
    {
        if (mSelectedFontURI.isEmpty())
            return false;
        accept();
        return true;
    }

    if (mFonts.empty())
        return false;

    size_t index = 0;
    for (; index < mFonts.size(); ++index)
    {
        if (mFonts[index] == mSelectedFontURI)
            break;
    }
    if (index == mFonts.size())
    {
        mScrollOffsetRow = 0;
        mSelectedFontURI = mFonts[0];
        return true;
    }

    const float zoom = GetValue(mUI.zoom);
    const unsigned box_width = mBoxWidth * zoom;

    const auto width    = mUI.widget->width();
    const auto height   = mUI.widget->height();
    const auto num_cols = width / (box_width + BoxMargin);
    const auto num_rows = mFonts.size() / num_cols;

    if (sym == Qt::Key_Left)
    {
        if (index > 0)
            index--;
        else index = mFonts.size()-1;
    }
    else if(sym == Qt::Key_Right)
    {
        if (index < mFonts.size() - 1)
            index++;
        else index = 0;
    }
    else if (sym == Qt::Key_Down)
    {
        index += num_cols;
        if (index >= mFonts.size())
            index = mFonts.size() - 1;
    }
    else if (sym == Qt::Key_Up)
    {
        index -= num_cols;
        if (index >= mFonts.size())
            index = 0;
    }

    const auto row = index / num_cols;
    if (row < mScrollOffsetRow || row > mScrollOffsetRow + mNumVisibleRows)
        mScrollOffsetRow = row;

    ASSERT(index < mFonts.size());
    mSelectedFontURI = mFonts[index];
    return true;
}

} // namespace
