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
#  include "ui_dlgtexturerect.h"
#  include <QTimer>
#  include <QDialog>
#  include <QPoint>
#include "warnpop.h"

#include "graphics/material.h"
namespace app {
    class Workspace;
} // namespace

namespace gui
{
    class DlgTextureRect : public QDialog
    {
        Q_OBJECT
    public:
        DlgTextureRect(QWidget* parent, app::Workspace* workspace, const gfx::FRect& rect,
            std::unique_ptr<gfx::TextureSource> texture);

        const gfx::FRect& GetRect() const
        { return mRect; }

    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_X_valueChanged(int);
        void on_Y_valueChanged(int);
        void on_W_valueChanged(int);
        void on_H_valueChanged(int);
        void on_widgetColor_colorChanged(QColor color);
        void finished();
        void timer();
    private:
        void LoadState();
        void SaveState();
        void UpdateRect();
        void OnPaintScene(gfx::Painter& painter, double secs);
        void OnMousePress(QMouseEvent* mickey);
        void OnMouseMove(QMouseEvent* mickey);
        void OnMouseRelease(QMouseEvent* mickey);
    private:
        Ui::DlgTextureRect mUI;
    private:
        QTimer mTimer;
    private:
        app::Workspace* mWorkspace = nullptr;
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
        std::unique_ptr<gfx::Material> mMaterial;
        gfx::FRect mRect;
    private:
        QPoint mStartPoint;
        QPoint mCurrentPoint;
        QPoint mTrackingOffset;
        enum class State {
            Nada, Selecting, Tracking
        };
        State mState = State::Nada;

    };

} // namespace
