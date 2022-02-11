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
#  include "ui_dlgfont.h"
#  include <QElapsedTimer>
#  include <QTimer>
#  include <QDialog>
#  include <QString>
#  include <QColor>
#include "warnpop.h"

#include <vector>

#include "graphics/fwd.h"

namespace app {
    class Workspace;
}

namespace gui
{
    class DlgFont : public QDialog
    {
        Q_OBJECT
    public:
        struct DisplaySettings {
            QColor text_color;
            unsigned font_size = 0;
            bool blinking  = false;
            bool underline = false;
        };

        DlgFont(QWidget* parent, const app::Workspace* workspace, const QString& font_uri, const DisplaySettings& disp);
        QString GetSelectedFontURI() const
        { return mSelectedFontURI; }
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
        Ui::DlgFont mUI;
    private:
        const app::Workspace* mWorkspace = nullptr;
        const DisplaySettings& mDisplay;
        QTimer mTimer;
        QElapsedTimer mElapsedTimer;
        unsigned mScrollOffsetRow = 0;
        unsigned mNumVisibleRows = 0;
        unsigned mBoxWidth = 0;
        unsigned mBoxHeight = 0;
        std::vector<QString> mFonts;
        QString mSelectedFontURI;
    };
}
