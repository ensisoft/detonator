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
#  include "ui_dlgnew.h"
#  include <QDialog>
#include "warnpop.h"

#include "editor/app/workspace.h"
#include "editor/app/resource.h"

namespace gui
{
    class DlgNew :  public QDialog
    {
        Q_OBJECT
    public:
        DlgNew(QWidget* parent) : QDialog(parent)
        {
            mUI.setupUi(this);
        }
        app::Resource::Type GetType() const
        { return mSelection; }

    private slots:
        void on_btnMaterial_clicked()
        {
            mSelection = app::Resource::Type::Material;
            accept();
        }
        void on_btnParticle_clicked()
        {
            mSelection = app::Resource::Type::ParticleSystem;
            accept();
        }
        void on_btnAnimation_clicked()
        {
            mSelection = app::Resource::Type::Animation;
            accept();
        }
        void on_btnCustomShape_clicked()
        {
            mSelection = app::Resource::Type::CustomShape;
            accept();
        }
        void on_btnEntity_clicked()
        {
            mSelection = app::Resource::Type::Entity;
            accept();
        }
        void on_btnScene_clicked()
        {
            mSelection = app::Resource::Type::Scene;
            accept();
        }
        void on_btnCancel_clicked()
        {
            reject();
        }

    private:
        Ui::DlgNew mUI;
    private:
        app::Resource::Type mSelection;
    };
} // namespace


