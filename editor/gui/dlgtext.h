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
#  include "ui_dlgtext.h"
#  include <QTimer>
#include "warnpop.h"

#include "graphics/text.h"
#include "graphics/painter.h"
#include "graphics/text.h"
#include "graphics/material.h"

namespace app {
    class Workspace;
} // app

namespace gui
{
    class DlgText : public QDialog
    {
        Q_OBJECT
    public:
        DlgText(QWidget* parent, const app::Workspace* workspace, gfx::TextBuffer& text);

    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_btnSelectFont_clicked();
        void on_btnBrowseFont_clicked();
        void on_btnAdjust_clicked();
    private:
        void PaintScene(gfx::Painter& painter, double secs);
    private:
        Ui::DlgText mUI;
    private:
        const app::Workspace* mWorkspace = nullptr;
        gfx::TextBuffer& mText;
        QTimer mTimer;
        bool mAdjustOnce = false;
        std::shared_ptr<gfx::TextureMap2DClass> mClass;
        std::unique_ptr<gfx::Material> mMaterial;
    };

} // namespace
