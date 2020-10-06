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
#  include "ui_dlgtexturerect.h"
#  include <QTimer>
#  include <QDialog>
#  include <QPoint>
#include "warnpop.h"

#include "graphics/material.h"

namespace gui
{
    class DlgTextureRect : public QDialog
    {
        Q_OBJECT
    public:
        DlgTextureRect(QWidget* parent, const gfx::FRect& rect,
            std::unique_ptr<gfx::TextureSource> texture);

        const gfx::FRect& GetRect() const
        { return mRect; }

    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void finished();
        void timer();
    private:
        void OnPaintScene(gfx::Painter& painter, double secs);
        void OnMousePress(QMouseEvent* mickey);
        void OnMouseMove(QMouseEvent* mickey);
        void OnMouseRelease(QMouseEvent* mickey);
    private:
        Ui::DlgTextureRect mUI;
    private:
        QTimer mTimer;
    private:
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
        gfx::Material mMaterial;
        gfx::FRect mRect;
    private:
        QPoint mStartPoint;
        QPoint mCurrentPoint;
        bool mDragging = false;
    };

} // namespace
