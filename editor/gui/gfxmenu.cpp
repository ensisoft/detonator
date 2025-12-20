// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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
#  include <QApplication>
#  include <QPalette>
#  include <QAction>
#  include <QMouseEvent>
#  include <QWheelEvent>
#  include <QIcon>
#  include <QString>
#  include <QColor>
#  include <QPixmap>
#  include <QImage>
#include "warnpop.h"

#include "graphics/transform.h"
#include "graphics/material.h"
#include "graphics/drawing.h"
#include "graphics/material_instance.h"
#include "graphics/simple_shape.h"
#include "editor/app/utility.h"
#include "editor/gui/utility.h"
#include "editor/gui/gfxmenu.h"

std::shared_ptr<const gfx::IBitmap> CreateIcon(const QIcon& icon, bool enabled)
{
    if (icon.isNull())
        return nullptr;

    const auto& pixmap = icon.pixmap(16, 16, enabled ? QIcon::Mode::Active : QIcon::Mode::Disabled);
    const auto& image = pixmap.toImage();
    const auto bmp_width  = image.width();
    const auto bmp_height = image.height();
    const auto bmp_depth  = image.depth();
    if (bmp_depth == 24)
    {
        const auto& converted = image.convertToFormat(QImage::Format_RGB888);
        return std::make_shared<gfx::RgbBitmap>(reinterpret_cast<const gfx::Pixel_RGB*>(converted.bits()),
                                                bmp_width, bmp_height);
    }

    if (bmp_depth == 32)
    {
        const auto& converted = image.convertToFormat(QImage::Format_RGBA8888);
        return std::make_shared<gfx::RgbaBitmap>(reinterpret_cast<const gfx::Pixel_RGBA*>(converted.bits()),
                                                 bmp_width, bmp_height);
    }
    return nullptr;
}

namespace gui
{

void GfxMenu::AddSubMenu(GfxMenu menu)
{
    Submenu sm;
    sm.menu = std::make_unique<GfxMenu>(std::move(menu));
    mMenuItems.push_back(std::move(sm));
}

void GfxMenu::MouseMove(const QMouseEvent* mickey)
{

    const auto& p = mickey->pos() - mMenuPosition;
    const auto x = p.x();
    const auto y = p.y();

    const QRect menu_rect(0, 0, mMenuWidth, mMenuHeight);
    if (!menu_rect.contains(p))
    {
        if (mCurrentItem != 0xff)
        {
            if (auto* submenu = std::get_if<Submenu>(&mMenuItems[mCurrentItem]))
            {
                if (submenu->menu->IsEnabled())
                    submenu->menu->MouseMove(mickey);
                return;
            }
        }

        mCurrentItem = 0xff;
        return;
    }

    mCurrentItem = 0xff;

    unsigned start = 0;

    for (size_t i=0; i<mMenuItems.size(); ++i)
    {
        const auto& item = mMenuItems[i];
        if (const auto* ptr = std::get_if<Action>(&item))
        {
            if (y >= start && y <= start + mMenuItemHeight)
                mCurrentItem = i;
            start += mMenuItemHeight;
        }
        else if (const auto* ptr = std::get_if<Separator>(&item))
        {
            if (y >= start && y <= start + mSeparatorHeight)
                mCurrentItem = 0xff;
            start += mSeparatorHeight;
        }
        else if (const auto* ptr = std::get_if<Submenu>(&item))
        {
            if (y >= start && y <= start + mMenuItemHeight)
                mCurrentItem = i;
            start += mMenuItemHeight;
        }
    }
}
void GfxMenu::MousePress(const QMouseEvent* mickey)
{
}
void GfxMenu::MouseRelease(const QMouseEvent* mickey)
{
}
void GfxMenu::MouseWheel(const QWheelEvent* wheel)
{
}

void GfxMenu::KeyPress(const QKeyEvent* key)
{
}

QAction* GfxMenu::GetResult() const
{
    if (mCurrentItem != 0xff)
    {
        if (const auto* action = std::get_if<Action>(&mMenuItems[mCurrentItem]))
            return action->action;
        else if (const auto* submenu = std::get_if<Submenu>(&mMenuItems[mCurrentItem]))
            return submenu->menu->GetResult();
    }
    return nullptr;
}

void GfxMenu::Initialize(const QRect& rect)
{
    const QFont font_default;
    const QFontMetricsF font_metrics(font_default);

    mFontSize = std::min(14, (int)font_metrics.height());
    mPalette = QApplication::palette();
    mMenuWidth  = 0;
    mMenuHeight = 0;
    mMenuItemHeight = std::max(mFontSize + 5.0f, 30.0f);
    mSeparatorHeight = 10;
    mIconAreaWidth = 50;
    QString longest_text;

    for (const auto& item : mMenuItems)
    {
        if (const auto* ptr = std::get_if<Action>(&item))
        {
            const auto& text = ptr->action->text();
            if (text.length() > longest_text.length())
                longest_text = text;
        }
        else if (const auto* ptr = std::get_if<Submenu>(&item))
        {
            const auto& text = ptr->menu->GetText();
            if (text.length() > longest_text.length())
                longest_text = text;
        }
    }

    // todo: actual font metrics
    mMenuWidth = longest_text.length() * 10;
    mMenuWidth += mIconAreaWidth;

    for (auto& item : mMenuItems)
    {
        if (auto* ptr = std::get_if<Action>(&item))
        {
            const auto& icon = ptr->action->icon();
            const auto enabled = ptr->action->isEnabled();
            ptr->icon = CreateIcon(icon, enabled);
            ptr->bitmap_gpu_id = base::RandomString(10);
            ptr->bitmap_gpu_name = app::ToUtf8((ptr->action->objectName()));
            if (ptr->bitmap_gpu_name.empty())
                ptr->bitmap_gpu_name = "Menu Icon";

            mMenuHeight += mMenuItemHeight;
        }
        else if (auto* ptr = std::get_if<Separator>(&item))
        {
            mMenuHeight += mSeparatorHeight;
        }
        else if (auto* ptr = std::get_if<Submenu>(&item))
        {
            QPoint menu_pos;
            menu_pos.setX(mMenuPosition.x() + mMenuWidth);
            menu_pos.setY(mMenuPosition.y() + mMenuHeight);
            ptr->menu->SetMenuPosition(menu_pos);
            ptr->menu->Initialize(rect);

            const auto& icon = ptr->menu->GetIcon();
            const auto enabled = ptr->menu->IsEnabled();
            ptr->icon = CreateIcon(icon, enabled);
            ptr->bitmap_gpu_id = base::RandomString(10);
            ptr->bitmap_gpu_name = app::ToUtf8(ptr->menu->GetText());
            if (ptr->bitmap_gpu_name.empty())
                ptr->bitmap_gpu_name = "Menu Icon";

            mMenuHeight += mMenuItemHeight;
        }
    }

    const auto bottom = mMenuPosition.y() + mMenuHeight;
    const auto right = mMenuPosition.x() + mMenuWidth;
    if (bottom > rect.height())
    {
        const auto shift = bottom - rect.height();
        mMenuPosition.setY(mMenuPosition.y() - shift);
    }
    if (right > rect.width())
    {
        const auto shift = right - rect.width();
        mMenuPosition.setX(mMenuPosition.x() - shift);
    }

}

void GfxMenu::Render(gfx::Painter& painter) const
{
    // draw menu background
    {
        gfx::Transform model;
        model.MoveTo(ToGfx(mMenuPosition));
        model.Resize(mMenuWidth, mMenuHeight);

        // drop shadow
        {
            model.Translate(2.0f, 2.0f);
            //painter.Draw(gfx::Rectangle(), model, CreateMaterial(QPalette::Shadow));
            model.Translate(-2.0f, -2.0f);
        }

        // should be either window or base color role
        painter.Draw(gfx::Rectangle(), model, CreateMaterial(QPalette::Window));

        // for bevels and 3d effects we have light, midlight, dark, mid and shadow
        model.Resize(mMenuWidth-2.0f, mMenuHeight-2.0f);
        model.Translate(1.0f, 1.0f);
        //painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Outline), model, CreateMaterial(QPalette::Dark));

    }

    gfx::FPoint item_point;
    item_point.SetX(mMenuPosition.x());
    item_point.SetY(mMenuPosition.y());

    for (size_t i=0; i<mMenuItems.size(); ++i)
    {
        const auto& item = mMenuItems[i];
        if (i == mCurrentItem)
        {
            gfx::FRect item_box;
            item_box.Move(item_point);
            item_box.Resize(mMenuWidth, mMenuItemHeight);
            gfx::FillRect(painter, item_box, CreateColor(QPalette::AlternateBase));
            gfx::DrawRectOutline(painter, item_box, CreateColor(QPalette::Highlight));
        }

        if (const auto* ptr = std::get_if<Action>(&item))
        {
            const auto* action = ptr->action;
            const auto checked = action->isChecked();
            const auto enabled = action->isEnabled() && mEnabled;
            const auto& text = app::ToUtf8(action->text());

            gfx::FRect item_box;
            item_box.Move(item_point);
            item_box.Resize(mMenuWidth, mMenuItemHeight);
            const auto& text_area = item_box.SubRect(mIconAreaWidth, 0.0f);
            const auto& icon_area = item_box.SubRect(0.0f, 0.0f, mIconAreaWidth, mMenuItemHeight);
            const auto icon_rect = base::CenterRectOnRect(icon_area, gfx::FRect(0.0f, 0.0f, 16.0f, 16.0f));

            gfx::DrawTextRect(painter, text, "app://fonts/OpenSans-Regular.ttf", mFontSize, text_area,
                enabled ? CreateColor(QPalette::Text, QPalette::Active) : CreateColor(QPalette::Text, QPalette::Disabled),
                gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);

            if (ptr->icon)
            {
                gfx::DrawBitmap(painter, icon_rect, ptr->icon, ptr->bitmap_gpu_id, ptr->bitmap_gpu_name);
            }
            if (checked)
            {
                gfx::DrawRectOutline(painter, icon_rect, CreateColor(QPalette::Highlight));
            }

            item_point.Translate(0.0f, mMenuItemHeight);
        }
        else if (const auto* ptr = std::get_if<Separator>(&item))
        {
            gfx::FRect item_box;
            item_box.Move(item_point);
            item_box.Resize(mMenuWidth, mSeparatorHeight);
            auto color = CreateColor(QPalette::Light);
            color.SetAlpha(0.6f);
            gfx::DrawHLine(painter, item_box, color, 0.5f);

            item_point.Translate(0.0f, mSeparatorHeight);
        }
        else if (const auto* ptr = std::get_if<Submenu>(&item))
        {
            gfx::FRect item_box;
            item_box.Move(item_point);
            item_box.Resize(mMenuWidth, mMenuItemHeight);
            const auto& text_area = item_box.SubRect(mIconAreaWidth, 0.0f);
            const auto& icon_area = item_box.SubRect(0.0f, 0.0f, mIconAreaWidth, mMenuItemHeight);
            const auto& menu = ptr->menu;
            const auto& text = app::ToUtf8(menu->GetText());
            const bool enabled = menu->IsEnabled() && mEnabled;

            gfx::DrawTextRect(painter, text + " ...", "app://fonts/OpenSans-Regular.ttf", mFontSize, text_area,
                              enabled ? CreateColor(QPalette::Text, QPalette::Active) : CreateColor(QPalette::Text, QPalette::Disabled),
                              gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
            if (ptr->icon)
            {
                const auto icon_rect = base::CenterRectOnRect(icon_area, gfx::FRect(0.0f, 0.0f, 16.0f, 16.0f));
                gfx::DrawBitmap(painter, icon_rect, ptr->icon, ptr->bitmap_gpu_id, ptr->bitmap_gpu_name);
            }

            if (i == mCurrentItem && enabled)
            {
                menu->Render(painter);
            }
            item_point.Translate(0.0f, mMenuItemHeight);
        }
    }
}
gfx::MaterialInstance GfxMenu::CreateMaterial(QPalette::ColorRole role, QPalette::ColorGroup group) const
{
    return gfx::CreateMaterialFromColor(ToGfx(mPalette.color(group, role)));
}
gfx::Color4f GfxMenu::CreateColor(QPalette::ColorRole role, QPalette::ColorGroup group) const
{
    return ToGfx(mPalette.color(group, role));
}


} // namespace gui
