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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include "ui_dlgmaterial.h"
#  include <QDialog>
#  include <QString>
#include "warnpop.h"

#include <vector>
#include <unordered_set>

#include "graphics/fwd.h"
#include "editor/app/types.h"
#include "editor/app/resource.h"
#include "editor/gui/types.h"

namespace app {
    class Workspace;
}

namespace gui
{
    class DlgMaterial : public QDialog
    {
        Q_OBJECT

    public:
        DlgMaterial(QWidget* parent, const app::Workspace* workspace, const app::AnyString& material);
        DlgMaterial(QWidget* parent, const app::Workspace* workspace);

        app::AnyString GetSelectedMaterialId() const
        { return mSelectedMaterialId; }
        void SetPreviewScale(const Size2Df& scale)
        { mPreviewScale = scale; }
        void SetPreviewScale(float x, float y)
        { mPreviewScale = Size2Df(x, y); }

        void SetMaterialId(const app::AnyString& material);

        unsigned GetTileIndex() const;
        void SetTileIndex(unsigned index);

    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_vScroll_valueChanged();
        void on_filter_textChanged(const QString&);
    private:
        void PaintScene(gfx::Painter& painter, double secs);
        void MousePress(QMouseEvent* mickey);
        void MouseDoubleClick(QMouseEvent* mickey);
        void MouseWheel(QWheelEvent* wheel);
        bool KeyPress(QKeyEvent* key);
    private:
        Ui::DlgMaterial mUI;
    private:
        QString mSelectedMaterialId;
        const app::Workspace* mWorkspace = nullptr;
        unsigned mScrollOffsetRow = 0;
        unsigned mNumVisibleRows = 0;
        bool mFirstPaint = true;
        app::ResourceList mMaterials;
        Size2Df mPreviewScale;
        std::unordered_set<std::string> mFailedMaterials;
    };

    class DlgTileChooser : public QDialog
    {
        Q_OBJECT
    public:
        DlgTileChooser(QWidget* parent, std::shared_ptr<const gfx::MaterialClass> klass) noexcept;

        unsigned GetTileIndex() const noexcept
        {
            return mTileIndex;
        }
        void SetTileIndex(unsigned index) noexcept
        {
            mTileIndex = index;
        }

    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_vScroll_valueChanged();

    private:
        void PaintScene(gfx::Painter& painter, double secs);
        void MousePress(QMouseEvent* mickey);
        void MouseDoubleClick(QMouseEvent* mickey);
        void MouseWheel(QWheelEvent* wheel);
        bool KeyPress(QKeyEvent* key);

    private:
        Ui::DlgMaterial mUI;

    private:
        std::shared_ptr<const gfx::MaterialClass> mMaterial;
        unsigned mTileIndex = 0;
        unsigned mScrollOffsetRow = 0;
        unsigned mNumVisibleRows = 0;
        unsigned mTileRows = 0;
        unsigned mTileCols = 0;
        Size2Df mPreviewScale;
    };

} // namespace
