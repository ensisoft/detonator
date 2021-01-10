// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include "warnpush.h"
#  include "ui_dlgmaterial.h"
#  include <QTimer>
#  include <QDialog>
#include "warnpop.h"

#include <vector>

#include "graphics/fwd.h"

namespace app {
    class Workspace;
}

namespace gui
{
    class DlgMaterial : public QDialog
    {
        Q_OBJECT
    public:
        DlgMaterial(QWidget* parent, const app::Workspace* workspace,
                    const QString& material);
        QString GetSelectedMaterialId() const
        { return mSelectedMaterialId; }
    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_vScroll_valueChanged();
    private:
        void PaintScene(gfx::Painter& painter, double secs);
        void MousePress(QMouseEvent* mickey);
        void MouseWheel(QWheelEvent* wheel);
        bool KeyPress(QKeyEvent* key);
    private:
        Ui::DlgMaterial mUI;
    private:
        QTimer mTimer;
        const app::Workspace* mWorkspace = nullptr;
        unsigned mScrollOffsetRow = 0;
        unsigned mNumVisibleRows = 0;
        std::vector<QString> mMaterialIds;
        QString mSelectedMaterialId;
    };
}