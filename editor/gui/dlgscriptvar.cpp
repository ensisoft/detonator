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

#include "config.h"

#include <limits>

#include "editor/gui/dlgscriptvar.h"
#include "editor/gui/utility.h"
#include "engine/types.h"

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
