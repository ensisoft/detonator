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
#  include "ui_dlgnew.h"
#  include <QDialog>
#include "warnpop.h"

#include "editor/app/workspace.h"
#include "editor/app/resource.h"
#include "editor/gui/utility.h"

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

        void SetOpenMode(const QString& mode)
        {
            SetValue(mUI.cmbOpenMode, mode);
        }

        QString GetOpenMode() const
        {
            return GetValue(mUI.cmbOpenMode);
        }

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
        void on_btnCustomShape_clicked()
        {
            mSelection = app::Resource::Type::Shape;
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
        void on_btnScript_clicked()
        {
            mSelection = app::Resource::Type::Script;
            accept();
        }
        void on_btnUI_clicked()
        {
            mSelection = app::Resource::Type::UI;
            accept();
        }
        void on_btnAudio_clicked()
        {
            mSelection = app::Resource::Type::AudioGraph;
            accept();
        }
        void on_btnTilemap_clicked()
        {
            mSelection = app::Resource::Type::Tilemap;
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


