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
#  include "ui_dlgmaterialparams.h"
#  include <QDialog>
#include "warnpop.h"

#include <vector>

#include "editor/app/workspace.h"
#include "game/fwd.h"

namespace gui
{
    class Uniform;

    class DlgMaterialParams : public QDialog
    {
        Q_OBJECT

    public:
        DlgMaterialParams(QWidget* parent, app::Workspace* workspace, game::DrawableItemClass* item);
    private slots:
        void on_btnResetGamma_clicked();
        void on_btnResetBaseColor_clicked();
        void on_baseColor_colorChanged(QColor);
        void on_gamma_valueChanged(double);
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void UniformValueChanged(const gui::Uniform* uniform);
        void ToggleUniform(bool checked);
    private:
        Ui::DlgMaterialParams mUI;
    private:
        app::Workspace* mWorkspace = nullptr;
        game::DrawableItemClass* mItem = nullptr;
        game::DrawableItemClass::MaterialParamMap mOldParams;
        std::vector<gui::Uniform*> mUniforms;
        bool mCustomMaterial = false;
    };

} // namespace
