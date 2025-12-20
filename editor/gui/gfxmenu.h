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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QPalette>
#  include <QPoint>
#  include <QList>
#  include <QAction>
#  include <QObject>
#include "warnpop.h"

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class QAction;

#include <variant>
#include <vector>
#include <functional>

#include "graphics/bitmap.h"
#include "graphics/device.h"
#include "graphics/painter.h"
#include "graphics/color4f.h"
#include "graphics/material_instance.h"

namespace gui
{
    // This is a custom context menu class that uses our gfx lib to render.
    // Why you ask? Well.. it's a story of incompetence at Qt which requires
    // us to use a QWindow for OpenGL rendering (for performance) and that
    // does not work with other QWidget based stuff such as te normal QMenu.
    // So this class is a custom replacement for that.
    class GfxMenu
    {
    public:
        bool IsEnabled() const
        {
            return mEnabled;
        }
        auto GetText() const
        {
            return mMenuText;
        }
        auto GetIcon() const
        {
            return mMenuIcon;
        }
        void SetIcon(QIcon icon)
        {
            mMenuIcon = icon;
        }

        void SetText(QString text)
        {
            mMenuText = std::move(text);
        }
        void SetEnabled(bool enabled)
        {
            mEnabled = enabled;
        }

        void AddSubMenu(GfxMenu menu);

        void AddAction(QAction* action)
        {
            Action a;
            a.action = action;
            mMenuItems.push_back(a);
        }
        void AddActions(const QList<QAction*>& list)
        {
            for (auto* action : list)
            {
                AddAction(action);
            }
        }

        void AddAction(QString text, std::function<void()> callback)
        {
            auto action = std::make_shared<QAction>();
            action->setText(text);
            QObject::connect(action.get(), &QAction::triggered, callback);

            Action a;
            a.action = action.get();
            a.owning = std::move(action);
            mMenuItems.push_back(std::move(a));
        }

        void AddSeparator()
        {
            mMenuItems.push_back(Separator{});
        }

        void MouseMove(const QMouseEvent* mickey);
        void MousePress(const QMouseEvent* mickey);
        void MouseRelease(const QMouseEvent* mickey);
        void MouseWheel(const QWheelEvent* wheel);
        void KeyPress(const QKeyEvent* key);

        void Initialize(const QRect& rect);

        void SetMenuPosition(const QPoint& position)
        {
            mMenuPosition = position;
        }

        QAction* GetResult() const;

        // called by the gfx widget. Position is in the top left
        // menu position expressed in painter's render target's size units
        // (normally window surface pixels)
        void Render(gfx::Painter& painter) const;

    private:
        struct Submenu {
            // lazy to make a move ctor etc..
            std::shared_ptr<GfxMenu> menu;
            std::shared_ptr<const gfx::IBitmap> icon;
            std::string bitmap_gpu_id;
            std::string bitmap_gpu_name;
        };
        struct Separator{};
        struct Action {
            QAction* action = nullptr;
            std::shared_ptr<const gfx::IBitmap> icon;
            std::string bitmap_gpu_id;
            std::string bitmap_gpu_name;
            std::shared_ptr<QAction> owning;
        };
        using MenuItem = std::variant<Action,Separator, Submenu>;

        unsigned mFontSize = 0;
        unsigned mIconAreaWidth = 0;
        unsigned mMenuItemHeight = 0;
        unsigned mSeparatorHeight = 0;
        unsigned mMenuHeight = 0;
        unsigned mMenuWidth  = 0;
        std::vector<MenuItem> mMenuItems;
        std::size_t mCurrentItem = 0xff;
        QPoint mMenuPosition;
        QString mMenuText;
        QIcon mMenuIcon;
        bool mEnabled = true;
    private:
        gfx::MaterialInstance CreateMaterial(QPalette::ColorRole role,
            QPalette::ColorGroup group = QPalette::ColorGroup::Active) const;
        gfx::Color4f CreateColor(QPalette::ColorRole role,
            QPalette::ColorGroup group = QPalette::ColorGroup::Active) const;
        QPalette mPalette;
    };

} // namespace