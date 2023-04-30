// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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
#  include "ui_dlgimgview.h"
#  include <QDialog>
#  include <QString>
#  include <QPixmap>
#  include <QTimer>
#include "warnpop.h"

#include <memory>
#include <optional>
#include <set>

#include "graphics/fwd.h"
#include "editor/gui/imgpack.h"

namespace app {
    class Workspace;
} // namespace

namespace gui
{
    class DlgImgView : public QDialog
    {
        Q_OBJECT

    public:
        DlgImgView(QWidget* parent);

        void LoadImage(const QString& file);
        void LoadJson(const QString& file);
        void SetDialogMode();
        void LoadGeometry();
        void LoadState();
        void SaveState() const;
        void SetWorkspace(app::Workspace* workspace)
        { mWorkspace = workspace; }

        QString GetImageFileName() const;
        QString GetJsonFileName() const;
        QString GetImageName() const;

        void ResetTransform();

        bool IsClosed() const
        { return mClosed; }
        bool HasWorkspace() const
        { return mWorkspace != nullptr; }

    private slots:
        void on_btnSelectImage_clicked();
        void on_btnSelectJson_clicked();
        void on_btnClose_clicked();
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_btnSave_clicked();
        void on_btnCutImages_clicked();
        void on_btnSelectOut_clicked();
        void on_cmbColorSpace_currentIndexChanged(int);
        void on_cmbMinFilter_currentIndexChanged(int);
        void on_cmbMagFilter_currentIndexChanged(int);
        void on_widgetColor_colorChanged(QColor color);
        void on_listWidget_itemSelectionChanged();
        void on_tabWidget_currentChanged(int);
        void on_renameTemplate_returnPressed();
        void on_tagTemplate_returnPressed();
        void finished();
        void timer();
    private:
        virtual void keyPressEvent(QKeyEvent* event) override;
        virtual bool eventFilter(QObject* destination, QEvent* event) override;
        void OnPaintScene(gfx::Painter& painter, double secs);
        void OnMousePress(QMouseEvent* mickey);
        void OnMouseMove(QMouseEvent* mickey);
        void OnMouseRelease(QMouseEvent* mickey);
        void OnMouseDoubleClick(QMouseEvent* mickey);
        bool OnKeyPress(QKeyEvent* event);
        void ToggleMouseSelection();

    private:
        Ui::DlgImgView mUI;
    private:
        app::Workspace* mWorkspace = nullptr;
        std::shared_ptr<gfx::TextureMap2DClass> mClass;
        std::unique_ptr<gfx::Material> mMaterial;
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
        QTimer mTimer;
        QPoint mTrackingOffset;
        QPoint mCurrentPoint;
        QPoint mStartPoint;
        bool mDialogMode = false;
        bool mClosed = false;

        ImagePack mPack;
        std::size_t mIndexUnderMouse = 0;
        enum class Mode {
            Nada, Tracking, Selecting
        };
        Mode mMode = Mode::Nada;
        std::set<size_t> mTilesTouched;
    };
} // namespace