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

DlgScriptVar::DlgScriptVar(const std::vector<ResourceListItem>& nodes,
                           const std::vector<ResourceListItem>& entities,
                           QWidget* parent, game::ScriptVar& variable)
  : QDialog(parent)
  , mVar(variable)
{
    mUI.setupUi(this);

    PopulateFromEnum<game::ScriptVar::Type>(mUI.varType);
    SetValue(mUI.varID,       variable.GetId());
    SetValue(mUI.varName,     variable.GetName());
    SetValue(mUI.varType,     variable.GetType());
    SetValue(mUI.chkReadOnly, variable.IsReadOnly());
    SetValue(mUI.chkArray,    variable.IsArray());
    SetValue(mUI.chkPrivate,  variable.IsPrivate());
    SetEnabled(mUI.btnAdd,    variable.IsArray());
    SetEnabled(mUI.btnDel,    variable.IsArray());
    SetList(mUI.cmbEntityNodeRef, nodes);
    SetList(mUI.cmbEntityRef, entities);

    UpdateArrayType();
    UpdateArrayIndex();
    ShowArrayValue(0);

    mUI.varName->setFocus();
}

void DlgScriptVar::on_btnAccept_clicked()
{
    if (!MustHaveInput(mUI.varName))
        return;

    mVar.SetName(GetValue(mUI.varName));
    mVar.SetReadOnly(GetValue(mUI.chkReadOnly));
    mVar.SetPrivate(GetValue(mUI.chkPrivate));
    accept();
}
void DlgScriptVar::on_btnCancel_clicked()
{
    reject();
}

void DlgScriptVar::on_btnAdd_clicked()
{
    mVar.AppendItem();
    UpdateArrayIndex();

    const auto size = mVar.GetArraySize();
    SetValue(mUI.index, size-1);
    ShowArrayValue(size-1);
}
void DlgScriptVar::on_btnDel_clicked()
{
    const auto size = mVar.GetArraySize();
    if (size == 1)
        return;

    const unsigned index = GetValue(mUI.index);
    mVar.RemoveItem(index);
    UpdateArrayIndex();

    const auto next = index > 0 ? index - 1 : 0;
    SetValue(mUI.index, next);
    ShowArrayValue(next);
}

void DlgScriptVar::on_btnResetNodeRef_clicked()
{
    SetValue(mUI.cmbEntityNodeRef, -1);
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVar::on_btnResetEntityRef_clicked()
{
    SetValue(mUI.cmbEntityRef, -1);
    SetArrayValue(GetValue(mUI.index));
}

void DlgScriptVar::on_chkArray_stateChanged(int)
{
    const bool checked = GetValue(mUI.chkArray);
    if (!checked)
    {
        mVar.ResizeToOne();
    }
    mVar.SetArray(checked);
    UpdateArrayIndex();
}

void DlgScriptVar::on_varType_currentIndexChanged(int)
{
    const auto size = mVar.GetArraySize();

    const game::ScriptVar::Type type = GetValue(mUI.varType);
    switch (type)
    {
        case game::ScriptVar::Type::Vec2:
            {
                std::vector<glm::vec2> arr;
                mVar.SetNewArrayType(std::move(arr));
                mUI.vec2ValueX->setFocus();
            }
            break;
        case game::ScriptVar::Type::Integer:
            {
                std::vector<int> arr;
                mVar.SetNewArrayType(std::move(arr));
                mUI.intValue->setFocus();
            }
            break;
        case game::ScriptVar::Type::String:
            {
                std::vector<std::string> arr;
                mVar.SetNewArrayType(std::move(arr));
                mUI.strValue->setFocus();
            }
            break;
        case game::ScriptVar::Type::Float:
            {
                std::vector<float> arr;
                mVar.SetNewArrayType(std::move(arr));
                mUI.floatValue->setFocus();
            }
            break;
        case game::ScriptVar::Type::Boolean:
            {
                std::vector<bool> arr;
                mVar.SetNewArrayType(std::move(arr));
                mUI.boolValueTrue->setFocus();
            }
            break;
        case game::ScriptVar::Type::EntityNodeReference:
            {
                std::vector<game::ScriptVar::EntityNodeReference> refs;
                mVar.SetNewArrayType(std::move(refs));
                mUI.cmbEntityNodeRef->setFocus();
            }
            break;
        case game::ScriptVar::Type::EntityReference:
            {
                std::vector<game::ScriptVar::EntityReference> refs;
                mVar.SetNewArrayType(std::move(refs));
                mUI.cmbEntityRef->setFocus();
            }
            break;
        default: BUG("Unhandled scripting variable type.");
    }
    mVar.Resize(size);

    UpdateArrayType();
    UpdateArrayIndex();
}

void DlgScriptVar::on_strValue_textChanged(const QString& text)
{
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVar::on_intValue_valueChanged(int)
{
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVar::on_floatValue_valueChanged(double)
{
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVar::on_vec2ValueX_valueChanged(double)
{
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVar::on_vec2ValueY_valueChanged(double)
{
    SetArrayValue(GetValue(mUI.index));
}

void DlgScriptVar::on_boolValueTrue_clicked(bool checked)
{
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVar::on_boolValueFalse_clicked(bool checked)
{
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVar::on_index_valueChanged(int)
{
    ShowArrayValue(GetValue(mUI.index));
}

void DlgScriptVar::on_cmbEntityRef_currentIndexChanged(int)
{
    SetArrayValue(GetValue(mUI.index));
}

void DlgScriptVar::on_cmbEntityNodeRef_currentIndexChanged(int)
{
    SetArrayValue(GetValue(mUI.index));
}

void DlgScriptVar::UpdateArrayType()
{
    SetEnabled(mUI.strValue,         false);
    SetEnabled(mUI.intValue,         false);
    SetEnabled(mUI.floatValue,       false);
    SetEnabled(mUI.vec2ValueX,       false);
    SetEnabled(mUI.vec2ValueY,       false);
    SetEnabled(mUI.boolValueTrue,    false);
    SetEnabled(mUI.boolValueFalse,   false);
    SetEnabled(mUI.cmbEntityRef,     false);
    SetEnabled(mUI.cmbEntityNodeRef, false);

    SetVisible(mUI.strValue,         false);
    SetVisible(mUI.intValue,         false);
    SetVisible(mUI.floatValue,       false);
    SetVisible(mUI.vec2ValueX,       false);
    SetVisible(mUI.vec2ValueY,       false);
    SetVisible(mUI.boolValueTrue,    false);
    SetVisible(mUI.boolValueFalse,   false);
    SetVisible(mUI.cmbEntityRef,     false);
    SetVisible(mUI.cmbEntityNodeRef, false);
    SetVisible(mUI.lblEntity,        false);
    SetVisible(mUI.lblEntityNode,    false);
    SetVisible(mUI.btnResetNodeRef,  false);
    SetVisible(mUI.btnResetEntityRef, false);

    SetVisible(mUI.lblString,     false);
    SetVisible(mUI.lblInteger,    false);
    SetVisible(mUI.lblFloat,      false);
    SetVisible(mUI.lblVec2,       false);
    SetVisible(mUI.lblBool,       false);
    SetVisible(mUI.lblEntity,     false);
    SetVisible(mUI.lblEntityNode, false);

    const auto type = mVar.GetType();
    if (type == game::ScriptVar::Type::String)
    {
        SetEnabled(mUI.strValue, true);
        SetVisible(mUI.strValue, true);
        SetVisible(mUI.lblString, true);
    }
    else if (type == game::ScriptVar::Type::Integer)
    {
        SetEnabled(mUI.intValue, true);
        SetVisible(mUI.intValue, true);
        SetVisible(mUI.lblInteger, true);
    }
    else if  (type == game::ScriptVar::Type::Float)
    {
        SetEnabled(mUI.floatValue, true);
        SetVisible(mUI.floatValue, true);
        SetVisible(mUI.lblFloat, true);
    }
    else if (type == game::ScriptVar::Type::Vec2)
    {
        SetEnabled(mUI.vec2ValueX, true);
        SetEnabled(mUI.vec2ValueY, true);
        SetVisible(mUI.vec2ValueX, true);
        SetVisible(mUI.vec2ValueY, true);
        SetVisible(mUI.lblVec2, true);
    }
    else if (type  == game::ScriptVar::Type::Boolean)
    {
        SetEnabled(mUI.boolValueTrue, true);
        SetEnabled(mUI.boolValueFalse, true);
        SetVisible(mUI.boolValueTrue, true);
        SetVisible(mUI.boolValueFalse, true);
        SetVisible(mUI.lblBool, true);
    }
    else if (type == game::ScriptVar::Type::EntityReference)
    {
        SetEnabled(mUI.cmbEntityRef, true);
        SetVisible(mUI.cmbEntityRef, true);
        SetVisible(mUI.lblEntity, true);
        SetVisible(mUI.btnResetEntityRef, true);
    }
    else if (type == game::ScriptVar::Type::EntityNodeReference)
    {
        SetEnabled(mUI.cmbEntityNodeRef, true);
        SetVisible(mUI.cmbEntityNodeRef, true);
        SetVisible(mUI.lblEntityNode, true);
        SetVisible(mUI.btnResetNodeRef, true);
    } else BUG("Unhandled scripting variable type.");

    adjustSize();
}

void DlgScriptVar::UpdateArrayIndex()
{
    const auto& size = mVar.GetArraySize();
    mUI.index->setMinimum(0);
    mUI.index->setMaximum(size-1);

    if (mVar.IsArray())
    {
        SetEnabled(mUI.index, true);
        SetEnabled(mUI.btnAdd, true);
        SetEnabled(mUI.btnDel, size > 1);
    }
    else
    {
        SetEnabled(mUI.index, GetValue(mUI.chkArray));
        SetEnabled(mUI.btnAdd, GetValue(mUI.chkArray));
        SetEnabled(mUI.btnDel, false);
    }
}

void DlgScriptVar::SetArrayValue(unsigned index)
{
    const auto type = mVar.GetType();
    if (type == game::ScriptVar::Type::String)
    {
        auto& arr = mVar.GetArray<std::string>();
        ASSERT(index < arr.size());
        arr[index] = GetValue(mUI.strValue);
    }
    else if (type == game::ScriptVar::Type::Integer)
    {
        auto& arr = mVar.GetArray<int>();
        ASSERT(index < arr.size());
        arr[index] = GetValue(mUI.intValue);
    }
    else if  (type == game::ScriptVar::Type::Float)
    {
        auto& arr = mVar.GetArray<float>();
        ASSERT(index < arr.size());
        arr[index] = GetValue(mUI.floatValue);
    }
    else if (type == game::ScriptVar::Type::Vec2)
    {
        auto& arr = mVar.GetArray<glm::vec2>();
        ASSERT(index < arr.size());
        arr[index] = glm::vec2(GetValue(mUI.vec2ValueX),
                               GetValue(mUI.vec2ValueY));
    }
    else if (type  == game::ScriptVar::Type::Boolean)
    {
        auto& arr = mVar.GetArray<bool>();
        ASSERT(index < arr.size());
        arr[index] = (bool)GetValue(mUI.boolValueTrue);
    }
    else if (type == game::ScriptVar::Type::EntityReference)
    {
        auto& arr = mVar.GetArray<game::ScriptVar::EntityReference>();
        ASSERT(index < arr.size());
        arr[index].id = app::ToUtf8(GetItemId(mUI.cmbEntityRef));
    }
    else if (type == game::ScriptVar::Type::EntityNodeReference)
    {
        auto& arr = mVar.GetArray<game::ScriptVar::EntityNodeReference>();
        ASSERT(index < arr.size());
        arr[index].id = app::ToUtf8(GetItemId(mUI.cmbEntityNodeRef));
    } else BUG("Unhandled scripting variable type.");
}

void DlgScriptVar::ShowArrayValue(unsigned index)
{
    const auto type = mVar.GetType();
    if (type == game::ScriptVar::Type::String)
    {
        const auto& arr = mVar.GetArray<std::string>();
        ASSERT(index < arr.size());
        SetValue(mUI.strValue, arr[index]);
    }
    else if (type == game::ScriptVar::Type::Integer)
    {
        const auto& arr = mVar.GetArray<int>();
        ASSERT(index < arr.size());
        SetValue(mUI.intValue, arr[index]);
    }
    else if  (type == game::ScriptVar::Type::Float)
    {
        const auto& arr = mVar.GetArray<float>();
        ASSERT(index < arr.size());
        SetValue(mUI.floatValue, arr[index]);
    }
    else if (type == game::ScriptVar::Type::Vec2)
    {
        const auto& arr = mVar.GetArray<glm::vec2>();
        ASSERT(index < arr.size());
        SetValue(mUI.vec2ValueX, arr[index].x);
        SetValue(mUI.vec2ValueY, arr[index].y);
    }
    else if (type  == game::ScriptVar::Type::Boolean)
    {
        const auto& arr = mVar.GetArray<bool>();
        ASSERT(index < arr.size());
        SetValue(mUI.boolValueTrue, arr[index] == true);
        SetValue(mUI.boolValueFalse, arr[index] == false);
    }
    else if (type == game::ScriptVar::Type::EntityReference)
    {
        const auto& arr = mVar.GetArray<game::ScriptVar::EntityReference>();
        ASSERT(index < arr.size());
        SetValue(mUI.cmbEntityRef, ListItemId(arr[index].id));
    }
    else if (type == game::ScriptVar::Type::EntityNodeReference)
    {
        const auto& arr = mVar.GetArray<game::ScriptVar::EntityNodeReference>();
        ASSERT(index < arr.size());
        SetValue(mUI.cmbEntityNodeRef, ListItemId(arr[index].id));
    }
    else BUG("Unhandled scripting variable type.");
}


DlgScriptVal::DlgScriptVal(const std::vector<ResourceListItem>& nodes,
                           const std::vector<ResourceListItem>& entities,
                           QWidget* parent, game::ScriptVar::VariantType& value, bool array)
  : QDialog(parent)
  , mVal(value)
{
    mUI.setupUi(this);
    SetList(mUI.cmbEntityNodeRef, nodes);
    SetList(mUI.cmbEntityRef, entities);

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
    SetVisible(mUI.lblEntityNode, false);
    SetVisible(mUI.cmbEntityNodeRef, false);
    SetVisible(mUI.btnResetNodeRef, false);
    SetVisible(mUI.lblEntity, false);
    SetVisible(mUI.cmbEntityRef, false);
    SetVisible(mUI.btnResetEntityRef, false);

    SetEnabled(mUI.strValue, true);
    SetEnabled(mUI.intValue, true);
    SetEnabled(mUI.floatValue, true);
    SetEnabled(mUI.vec2ValueX, true);
    SetEnabled(mUI.vec2ValueY, true);
    SetEnabled(mUI.boolValueTrue, true);
    SetEnabled(mUI.boolValueFalse, true);

    SetVisible(mUI.index, false);
    SetVisible(mUI.btnAdd, false);
    SetVisible(mUI.btnDel, false);
    SetVisible(mUI.lblIndex, false);

    if (array)
    {
        const auto size = game::ScriptVar::GetArraySize(mVal);
        SetVisible(mUI.index, true);
        SetVisible(mUI.lblIndex, true);
        SetValue(mUI.index, 0);
        SetRange(mUI.index, 0, size-1);
    }

    switch (game::ScriptVar::GetTypeFromVariant(value))
    {
        case game::ScriptVar::Type::Vec2:
            {
                SetVisible(mUI.vec2ValueX, true);
                SetVisible(mUI.vec2ValueY, true);
                SetVisible(mUI.lblVec2, true);
                mUI.vec2ValueX->setFocus();
            }
            break;
        case game::ScriptVar::Type::Float:
            {
                SetVisible(mUI.floatValue, true);
                SetVisible(mUI.lblFloat, true);
                mUI.floatValue->setFocus();
            }
            break;
        case game::ScriptVar::Type::Integer:
            {
                SetVisible(mUI.intValue, true);
                SetVisible(mUI.lblInteger, true);
                mUI.intValue->setFocus();
            }
            break;
        case game::ScriptVar::Type::String:
            {
                SetVisible(mUI.strValue, true);
                SetVisible(mUI.lblString, true);
                mUI.strValue->setFocus();
            }
            break;
        case game::ScriptVar::Type::Boolean:
            {
                SetVisible(mUI.boolValueTrue, true);
                SetVisible(mUI.boolValueFalse, true);
                SetVisible(mUI.lblBool, true);
                mUI.boolValueTrue->setFocus();
            }
            break;
        case game::ScriptVar::Type::EntityReference:
            {
                SetVisible(mUI.cmbEntityRef, true);
                SetVisible(mUI.lblEntity, true);
                SetVisible(mUI.btnResetEntityRef, true);
                mUI.cmbEntityRef->setFocus();
            }
            break;
        case game::ScriptVar::Type::EntityNodeReference:
            {
                SetVisible(mUI.cmbEntityNodeRef, true);
                SetVisible(mUI.lblEntityNode, true);
                SetVisible(mUI.btnResetNodeRef, true);
                mUI.cmbEntityNodeRef->setFocus();
            }
            break;
        default:  BUG("Unhandled ScriptVar value type.");
    }
    adjustSize();

    ShowArrayValue(0);
}

void DlgScriptVal::on_btnAccept_clicked()
{
    accept();
}

void DlgScriptVal::on_btnCancel_clicked()
{
    reject();
}

void DlgScriptVal::on_btnResetNodeRef_clicked()
{
    SetValue(mUI.cmbEntityNodeRef, -1);
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVal::on_btnResetEntityRef_clicked()
{
    SetValue(mUI.cmbEntityRef, -1);
    SetArrayValue(GetValue(mUI.index));
}

void DlgScriptVal::on_index_valueChanged(int)
{
    ShowArrayValue(GetValue(mUI.index));
}

void DlgScriptVal::on_strValue_textChanged(const QString& text)
{
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVal::on_intValue_valueChanged(int)
{
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVal::on_floatValue_valueChanged(double)
{
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVal::on_vec2ValueX_valueChanged(double)
{
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVal::on_vec2ValueY_valueChanged(double)
{
    SetArrayValue(GetValue(mUI.index));
}

void DlgScriptVal::on_boolValueTrue_clicked(bool checked)
{
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVal::on_boolValueFalse_clicked(bool checked)
{
    SetArrayValue(GetValue(mUI.index));
}
void DlgScriptVal::on_cmbEntityRef_currentIndexChanged(int)
{
    SetArrayValue(GetValue(mUI.index));
}

void DlgScriptVal::on_cmbEntityNodeRef_currentIndexChanged(int)
{
    SetArrayValue(GetValue(mUI.index));
}

void DlgScriptVal::SetArrayValue(unsigned index)
{
    const auto type = game::ScriptVar::GetTypeFromVariant(mVal);
    if (type == game::ScriptVar::Type::String)
    {
        auto& arr = game::ScriptVar::GetVectorFromVariant<std::string>(mVal);
        ASSERT(index < arr.size());
        arr[index] = GetValue(mUI.strValue);
    }
    else if (type == game::ScriptVar::Type::Integer)
    {
        auto& arr = game::ScriptVar::GetVectorFromVariant<int>(mVal);
        ASSERT(index < arr.size());
        arr[index] = GetValue(mUI.intValue);
    }
    else if  (type == game::ScriptVar::Type::Float)
    {
        auto& arr = game::ScriptVar::GetVectorFromVariant<float>(mVal);
        ASSERT(index < arr.size());
        arr[index] = GetValue(mUI.floatValue);
    }
    else if (type == game::ScriptVar::Type::Vec2)
    {
        auto& arr = game::ScriptVar::GetVectorFromVariant<glm::vec2>(mVal);
        ASSERT(index < arr.size());
        arr[index] = glm::vec2(GetValue(mUI.vec2ValueX),
                               GetValue(mUI.vec2ValueY));
    }
    else if (type  == game::ScriptVar::Type::Boolean)
    {
        auto& arr = game::ScriptVar::GetVectorFromVariant<bool>(mVal);
        ASSERT(index < arr.size());
        arr[index] = (bool) GetValue(mUI.boolValueTrue);
    }
    else if (type == game::ScriptVar::Type::EntityReference)
    {
        auto& arr = game::ScriptVar::GetVectorFromVariant<game::ScriptVar::EntityReference>(mVal);
        ASSERT(index < arr.size());
        arr[index].id = app::ToUtf8(GetItemId(mUI.cmbEntityRef));
    }
    else if (type == game::ScriptVar::Type::EntityNodeReference)
    {
        auto& arr = game::ScriptVar::GetVectorFromVariant<game::ScriptVar::EntityNodeReference>(mVal);
        ASSERT(index < arr.size());
        arr[index].id = app::ToUtf8(GetItemId(mUI.cmbEntityNodeRef));
    } else BUG("Unhandled scripting variable type.");
}

void DlgScriptVal::ShowArrayValue(unsigned index)
{
    const auto type = game::ScriptVar::GetTypeFromVariant(mVal);
    if (type == game::ScriptVar::Type::String)
    {
        const auto& arr = game::ScriptVar::GetVectorFromVariant<std::string>(mVal);
        ASSERT(index < arr.size());
        SetValue(mUI.strValue, arr[index]);
    }
    else if (type == game::ScriptVar::Type::Integer)
    {
        const auto& arr = game::ScriptVar::GetVectorFromVariant<int>(mVal);
        ASSERT(index < arr.size());
        SetValue(mUI.intValue, arr[index]);
    }
    else if  (type == game::ScriptVar::Type::Float)
    {
        const auto& arr = game::ScriptVar::GetVectorFromVariant<float>(mVal);
        ASSERT(index < arr.size());
        SetValue(mUI.floatValue, arr[index]);
    }
    else if (type == game::ScriptVar::Type::Vec2)
    {
        const auto& arr = game::ScriptVar::GetVectorFromVariant<glm::vec2>(mVal);
        ASSERT(index < arr.size());
        SetValue(mUI.vec2ValueX, arr[index].x);
        SetValue(mUI.vec2ValueY, arr[index].y);
    }
    else if (type  == game::ScriptVar::Type::Boolean)
    {
        const auto& arr = game::ScriptVar::GetVectorFromVariant<bool>(mVal);
        ASSERT(index < arr.size());
        SetValue(mUI.boolValueTrue, arr[index] == true);
        SetValue(mUI.boolValueFalse, arr[index] == false);
    }
    else if (type == game::ScriptVar::Type::EntityReference)
    {
        const auto& arr = game::ScriptVar::GetVectorFromVariant<game::ScriptVar::EntityReference>(mVal);
        ASSERT(index < arr.size());
        SetValue(mUI.cmbEntityRef, ListItemId(arr[index].id));
    }
    else if (type == game::ScriptVar::Type::EntityNodeReference)
    {
        const auto& arr = game::ScriptVar::GetVectorFromVariant<game::ScriptVar::EntityNodeReference>(mVal);
        ASSERT(index < arr.size());
        SetValue(mUI.cmbEntityNodeRef, ListItemId(arr[index].id));
    } else BUG("Unhandled scripting variable type.");
}

} // namespace
