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
#  include "ui_materialmapwidget.h"
#  include <QWidget>
#  include <QObject>
#  include <QPalette>
#include "warnpop.h"

#include <string>
#include <unordered_map>

#include "graphics/types.h"
#include "graphics/color4f.h"
#include "graphics/material_class.h"
#include "graphics/material_instance.h"
#include "editor/app/types.h"
#include "editor/gui/gfxmenu.h"

namespace gui
{
    class MaterialMapWidget : public QWidget
    {
        Q_OBJECT
    public:
        explicit MaterialMapWidget(QWidget* parent);
        ~MaterialMapWidget() override;

        void Render() const;
        void Update();
        void OpenContextMenu(const QPoint& point, GfxMenu menu);
        void CollapseAll();
        void ExpandAll();

        void SetMaterial(std::shared_ptr<const gfx::MaterialClass> klass)
        {
            mMaterial = std::move(klass);
            Update();
        }
        void ClearSelection()
        {
            mSelectedTextureMapId.clear();
            mSelectedTextureSrcId.clear();
        }
        void ClearSelectedTextureSrc()
        {
            mSelectedTextureSrcId.clear();
        }
        void ClearSelectedTextureMap()
        {
            mSelectedTextureMapId.clear();
        }
        app::AnyString GetSelectedTextureMapId() const
        {
            return mSelectedTextureMapId;
        }
        app::AnyString GetSelectedTextureSrcId() const
        {
            return mSelectedTextureSrcId;
        }
        void SetSelectedTextureMapId(const app::AnyString& id)
        {
            mSelectedTextureMapId = id;
            mSelectedTextureSrcId.clear();
        }
        void SetSelectedTextureSrcId(const app::AnyString& id)
        {
            mSelectedTextureSrcId = id;
            mSelectedTextureMapId.clear();
        }

    signals:
        void SelectionChanged();
        void CustomContextMenuRequested(const QPoint& point);

    private slots:
        void on_verticalScrollBar_valueChanged(int);

    private:
        void PaintScene(gfx::Painter& painter, double dt);
        void MousePress(const QMouseEvent* mickey);
        void MouseRelease(const QMouseEvent* mickey);
        void MouseMove(const QMouseEvent* mickey);
        void ComputeScrollBars(unsigned render_width, unsigned render_height);
        bool UnderMouse() const;
        gfx::MaterialInstance CreateMaterial(QPalette::ColorRole role,
            QPalette::ColorGroup group = QPalette::ColorGroup::Active) const;
        gfx::Color4f CreateColor(QPalette::ColorRole role,
            QPalette::ColorGroup group = QPalette::ColorGroup::Active) const;
    private:
        Ui::MaterialMapWidget mUI;
    private:
        QPalette mPalette;
        std::shared_ptr<const gfx::MaterialClass> mMaterial;
        struct MaterialMapState {
            gfx::FRect header_rect;
            gfx::FRect button_rect;
            gfx::FRect list_rect;
            bool expanded = true;
        };
        std::string mSelectedTextureMapId;
        std::string mSelectedTextureSrcId;
        std::unordered_map<std::string, MaterialMapState> mMaterialMapStates;
        float mCurrentTime = 0.0f;
        float mVerticalScroll = 0.0f;
        gfx::FPoint mCurrentMousePos;

        float mTextureItemSize = 0.0f;

        unsigned mPreviousRenderWidth = 0;
        unsigned mPreviousRenderHeight = 0;
        unsigned mPreviousWidgetWidth = 0;
        unsigned mPreviousWidgetHeight = 0;
    };

} // namespace