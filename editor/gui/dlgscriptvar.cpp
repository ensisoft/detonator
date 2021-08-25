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

#include "config.h"

#include <limits>

#include "editor/gui/dlgscriptvar.h"
#include "editor/gui/utility.h"
#include "game/types.h"

namespace gui
{

DlgScriptVar::DlgScriptVar(QWidget* parent, game::ScriptVar& variable)
  : QDialog(parent)
  , mVar(variable)
{
    mUI.setupUi(this);
    // lalal these don't work ?
    // mUI.floatValue->setMaximum(std::numeric_limits<float>::max());
    // mUI.floatValue->setMinimum(std::numeric_limits<float>::min());
    // mUI.intValue->setMaximum(std::numeric_limits<int>::max());
    // mUI.intValue->setMinimum(std::numeric_limits<int>::min());
    // mUI.vec2ValueX->setMaximum(std::numeric_limits<float>::max());
    // mUI.vec2ValueX->setMinimum(std::numeric_limits<float>::min());
    // mUI.vec2ValueY->setMaximum(std::numeric_limits<float>::max());
    // mUI.vec2ValueY->setMinimum(std::numeric_limits<float>::min());

    PopulateFromEnum<game::ScriptVar::Type>(mUI.varType);
    SetValue(mUI.varName, variable.GetName());
    SetValue(mUI.varType, variable.GetType());
    SetValue(mUI.chkReadOnly, variable.IsReadOnly());
    const game::ScriptVar::Type type = GetValue(mUI.varType);
    switch (type)
    {
        case game::ScriptVar::Type::Vec2:
            {
                const auto& val = variable.GetValue<glm::vec2>();
                SetValue(mUI.vec2ValueX, val.x);
                SetValue(mUI.vec2ValueY, val.y);
            }
            break;
        case game::ScriptVar::Type::Integer:
            SetValue(mUI.intValue, variable.GetValue<int>());
            break;
        case game::ScriptVar::Type::String:
            SetValue(mUI.strValue, variable.GetValue<std::string>());
            break;
        case game::ScriptVar::Type::Float:
            SetValue(mUI.floatValue, variable.GetValue<float>());
            break;
        case game::ScriptVar::Type::Boolean:
            {
                const auto val = variable.GetValue<bool>();
                SetValue(mUI.boolValueTrue, val == true);
                SetValue(mUI.boolValueFalse, val == false);
            }
            break;
    }
    on_varType_currentIndexChanged(0);
    mUI.varName->setFocus();
}

void DlgScriptVar::on_btnAccept_clicked()
{
    if (!MustHaveInput(mUI.varName))
        return;
    const bool read_only = GetValue(mUI.chkReadOnly);
    const auto type = (game::ScriptVar::Type)GetValue(mUI.varType);
    const auto name = (std::string)GetValue(mUI.varName);
    switch (type)
    {
        case game::ScriptVar::Type::Vec2:
            {
                glm::vec2 val;
                val.x = GetValue(mUI.vec2ValueX);
                val.y = GetValue(mUI.vec2ValueY);
                mVar = game::ScriptVar(name, val, read_only);
            }
            break;
        case game::ScriptVar::Type::Integer:
            mVar = game::ScriptVar(name, (int)GetValue(mUI.intValue), read_only);
            break;
        case game::ScriptVar::Type::String:
            mVar = game::ScriptVar(name, (std::string)GetValue(mUI.strValue), read_only);
            break;
        case game::ScriptVar::Type::Float:
            mVar = game::ScriptVar(name, (float)GetValue(mUI.floatValue), read_only);
            break;
        case game::ScriptVar::Type::Boolean:
            mVar = game::ScriptVar(name, (bool)GetValue(mUI.boolValueTrue), read_only);
            break;
    }
    accept();
}
void DlgScriptVar::on_btnCancel_clicked()
{
    reject();
}

void DlgScriptVar::on_varType_currentIndexChanged(int)
{
    SetEnabled(mUI.strValue, false);
    SetEnabled(mUI.intValue, false);
    SetEnabled(mUI.floatValue, false);
    SetEnabled(mUI.vec2ValueX, false);
    SetEnabled(mUI.vec2ValueY, false);
    SetEnabled(mUI.boolValueTrue, false);
    SetEnabled(mUI.boolValueFalse, false);

    const game::ScriptVar::Type type = GetValue(mUI.varType);
    switch (type)
    {
        case game::ScriptVar::Type::Vec2:
            SetEnabled(mUI.vec2ValueX, true);
            SetEnabled(mUI.vec2ValueY, true);
            mUI.vec2ValueX->setFocus();
            break;
        case game::ScriptVar::Type::Integer:
            SetEnabled(mUI.intValue, true);
            mUI.intValue->setFocus();
            break;
        case game::ScriptVar::Type::String:
            SetEnabled(mUI.strValue, true);
            mUI.strValue->setFocus();
            break;
        case game::ScriptVar::Type::Float:
            SetEnabled(mUI.floatValue, true);
            mUI.floatValue->setFocus();
            break;
        case game::ScriptVar::Type::Boolean:
            SetEnabled(mUI.boolValueTrue, true);
            SetEnabled(mUI.boolValueFalse, true);
            mUI.boolValueTrue->setFocus();
            break;
    }
}

} // namespace
