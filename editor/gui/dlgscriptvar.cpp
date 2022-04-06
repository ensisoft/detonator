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
#include "game/scriptvar.h"

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
    SetValue(mUI.varID, variable.GetId());
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
    switch ((game::ScriptVar::Type)GetValue(mUI.varType))
    {
        case game::ScriptVar::Type::Vec2:
            {
                glm::vec2 val;
                val.x = GetValue(mUI.vec2ValueX);
                val.y = GetValue(mUI.vec2ValueY);
                mVar.SetNewValueType(val);
            }
            break;
        case game::ScriptVar::Type::Integer:
            mVar.SetNewValueType((int)GetValue(mUI.intValue));
            break;
        case game::ScriptVar::Type::String:
            mVar.SetNewValueType((std::string)GetValue(mUI.strValue));
            break;
        case game::ScriptVar::Type::Float:
            mVar.SetNewValueType((float)GetValue(mUI.floatValue));
            break;
        case game::ScriptVar::Type::Boolean:
            mVar.SetNewValueType(false);
            mVar.SetNewValueType((bool)GetValue(mUI.boolValueTrue));
            break;
    }
    mVar.SetName(GetValue(mUI.varName));
    mVar.SetReadOnly(GetValue(mUI.chkReadOnly));
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

DlgScriptVal::DlgScriptVal(QWidget* parent, game::ScriptVar::VariantType& value)
  : QDialog(parent)
  , mVal(value)
{
    mUI.setupUi(this);
    SetVisible(mUI.props, false);
    SetVisible(mUI.value, true);

    SetVisible(mUI.strValue, false);
    SetVisible(mUI.intValue, false);
    SetVisible(mUI.floatValue, false);
    SetVisible(mUI.vec2ValueX, false);
    SetVisible(mUI.vec2ValueY, false);
    SetVisible(mUI.boolValueTrue, false);
    SetVisible(mUI.boolValueFalse, false);
    SetVisible(mUI.lblString, false);
    SetVisible(mUI.lblInteger, false);
    SetVisible(mUI.lblFloat, false);
    SetVisible(mUI.lblVec2, false);
    SetVisible(mUI.lblBool, false);

    SetEnabled(mUI.strValue, true);
    SetEnabled(mUI.intValue, true);
    SetEnabled(mUI.floatValue, true);
    SetEnabled(mUI.vec2ValueX, true);
    SetEnabled(mUI.vec2ValueY, true);
    SetEnabled(mUI.boolValueTrue, true);
    SetEnabled(mUI.boolValueFalse, true);

    const auto index = 0;

    switch (game::ScriptVar::GetTypeFromVariant(value))
    {
        case game::ScriptVar::Type::Vec2:
            {
                const auto& val = std::get<std::vector<glm::vec2>>(value)[index];
                SetValue(mUI.vec2ValueX, val.x);
                SetValue(mUI.vec2ValueY, val.y);
                SetVisible(mUI.vec2ValueX, true);
                SetVisible(mUI.vec2ValueY, true);
                mUI.vec2ValueX->setFocus();
            }
            break;
        case game::ScriptVar::Type::Float:
            {
                SetValue(mUI.floatValue, std::get<std::vector<float>>(value)[index]);
                SetVisible(mUI.floatValue, true);
            }
            break;
        case game::ScriptVar::Type::Integer:
            {
                SetValue(mUI.intValue, std::get<std::vector<int>>(value)[index]);
                SetVisible(mUI.intValue, true);
            }
            break;
        case game::ScriptVar::Type::String:
            {
                SetValue(mUI.strValue, std::get<std::vector<std::string>>(value)[index]);
                SetVisible(mUI.strValue, true);
            }
            break;
        case game::ScriptVar::Type::Boolean:
            {
                const auto val = std::get<std::vector<bool>>(value)[index];
                SetValue(mUI.boolValueTrue, val == true);
                SetValue(mUI.boolValueFalse, val == false);
                SetVisible(mUI.boolValueTrue, true);
                SetVisible(mUI.boolValueFalse, true);
                mUI.boolValueTrue->setFocus();
            }
            break;
        default:  BUG("Unhandled ScriptVar value type.");
    }
    adjustSize();
}

void DlgScriptVal::on_btnAccept_clicked()
{
    switch (game::ScriptVar::GetTypeFromVariant(mVal))
    {
        case game::ScriptVar::Type::Vec2:
            {
                glm::vec2 val;
                val.x = GetValue(mUI.vec2ValueX);
                val.y = GetValue(mUI.vec2ValueY);
                mVal = std::vector<glm::vec2> {val};
            }
            break;
        case game::ScriptVar::Type::Float:
            {
                const auto val = (float)GetValue(mUI.floatValue);
                mVal = std::vector<float> {val};
            }
            break;
        case game::ScriptVar::Type::Integer:
            {
                const auto val = (int)GetValue(mUI.intValue);
                mVal = std::vector<int> { val };
            }
            break;
        case game::ScriptVar::Type::String:
            {
                const auto val = (std::string)GetValue(mUI.strValue);
                mVal =  std::vector<std::string> {val};
            }
            break;
        case game::ScriptVar::Type::Boolean:
            {
                const auto val = (bool)GetValue(mUI.boolValueTrue);
                mVal = std::vector<bool> { val };
            }
            break;
        default:  BUG("Unhandled ScriptVar value type.");
    }
    accept();
}

void DlgScriptVal::on_btnCancel_clicked()
{
    reject();
}


} // namespace
