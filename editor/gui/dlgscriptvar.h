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
#  include "ui_dlgscriptvar.h"
#  include <QDialog>
#include "warnpop.h"

#include "game/scriptvar.h"
#include "editor/gui/utility.h"

namespace gui
{
    // Dialog for editing a scripting variable, i.e. it's
    // type, data and other properties.
    class DlgScriptVar :  public QDialog
    {
        Q_OBJECT
    public:
        DlgScriptVar(const std::vector<ResourceListItem>& nodes,
                     const std::vector<ResourceListItem>& entities,
                     const std::vector<ResourceListItem>& materials,
                     QWidget* parent, game::ScriptVar& variable);
    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_btnAdd_clicked();
        void on_btnDel_clicked();
        void on_btnResetNodeRef_clicked();
        void on_btnResetEntityRef_clicked();
        void on_btnResetMaterialRef_clicked();
        void on_chkArray_stateChanged(int);
        void on_varType_currentIndexChanged(int);
        void on_strValue_textChanged(const QString& text);
        void on_intValue_valueChanged(int value);
        void on_floatValue_valueChanged(double value);
        void on_vec2ValueX_valueChanged(double value);
        void on_vec2ValueY_valueChanged(double value);
        void on_boolValueTrue_clicked(bool checked);
        void on_boolValueFalse_clicked(bool checked);
        void on_index_valueChanged(int);
        void on_cmbEntityRef_currentIndexChanged(int);
        void on_cmbEntityNodeRef_currentIndexChanged(int);
        void on_cmbMaterialRef_currentIndexChanged(int);
    private:
        void UpdateArrayIndex();
        void UpdateArrayType();
        void SetArrayValue(unsigned index);
        void ShowArrayValue(unsigned index);
    private:
        Ui::DlgScriptVar mUI;
    private:
        game::ScriptVar& mVar;
    };

    // Dialog for editing the scripting variable *data* only.
    // Uses the same UI resource as the DlgScriptVar dialog
    // but with limitations.
    class DlgScriptVal : public QDialog
    {
        Q_OBJECT
    public:
        DlgScriptVal(const std::vector<ResourceListItem>& nodes,
                     const std::vector<ResourceListItem>& entities,
                     const std::vector<ResourceListItem>& materials,
                     QWidget* parent, game::ScriptVar::VariantType& value, bool array);
    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_btnResetNodeRef_clicked();
        void on_btnResetEntityRef_clicked();
        void on_btnResetMaterialRef_clicked();
        void on_index_valueChanged(int);
        void on_strValue_textChanged(const QString& text);
        void on_intValue_valueChanged(int value);
        void on_floatValue_valueChanged(double value);
        void on_vec2ValueX_valueChanged(double value);
        void on_vec2ValueY_valueChanged(double value);
        void on_boolValueTrue_clicked(bool checked);
        void on_boolValueFalse_clicked(bool checked);
        void on_cmbEntityRef_currentIndexChanged(int);
        void on_cmbEntityNodeRef_currentIndexChanged(int);
        void on_cmbMaterialRef_currentIndexChanged(int);
    private:
        void SetArrayValue(unsigned index);
        void ShowArrayValue(unsigned index);
    private:
        Ui::DlgScriptVar mUI;
    private:
        game::ScriptVar::VariantType& mVal;

    };

} // namespace