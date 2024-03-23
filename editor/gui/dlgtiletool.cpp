// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#define LOGTAG "gui"

#include "editor/gui/dlgtiletool.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/utility.h"
#include "editor/gui/types.h"

namespace {
    constexpr auto PaletteIndexAutomatic = -1;

    gui::Size2Df GetMaterialPreviewScale(const game::TilemapClass& klass)
    {
        const auto perspective = klass.GetPerspective();
        if (perspective == game::TilemapClass::Perspective::AxisAligned)
            return {1.0f, 1.0};
        else if (perspective == game::TilemapClass::Perspective::Dimetric)
            return {1.0f, 2.0f};
        else BUG("Unknown perspective");
        return {1.0f, 1.0f};
    }
} // namespace

namespace gui
{

DlgTileTool::DlgTileTool(const app::Workspace* workspace, QWidget* parent, ToolBox* tools)
  : QDialog(parent)
  , mWorkspace(workspace)
  , mTools(tools)
{
    mUI.setupUi(this);

    PopulateFromEnum<TileToolFunction>(mUI.cmbToolFunction);
    PopulateFromEnum<TileToolShape>(mUI.cmbToolShape);

    const auto& materials = mWorkspace->ListAllMaterials();
    SetList(mUI.cmbToolMaterial, materials);
    SetRange(mUI.toolValue, -0x800000, 0xffffff); // min is 24 bit signed and max is 24bit unsigned

    connect(workspace, &app::Workspace::ResourceAdded,   this, &DlgTileTool::ResourceAdded);
    connect(workspace, &app::Workspace::ResourceRemoved, this, &DlgTileTool::ResourceRemoved);
    connect(workspace, &app::Workspace::ResourceUpdated, this, &DlgTileTool::ResourceUpdated);

    UpdateToolCombo();
    if (!mTools->empty())
    {
        SetCurrentTool((*mTools)[0]->id);
        ShowCurrentTool();
    }
}

void DlgTileTool::on_cmbTool_currentIndexChanged(int index)
{
    SetCurrentTool((*mTools)[index]->id);
    ShowCurrentTool();
}
void DlgTileTool::on_toolName_editingFinished()
{
    ModifyCurrentTool();
    UpdateToolCombo();
}

void DlgTileTool::on_btnAddTool_clicked()
{
    auto tool = std::make_shared<TileTool>();
    tool->name          = "My Tool";
    tool->id            = app::RandomString();
    tool->tool          = TileToolFunction::TileBrush;
    tool->shape         = TileToolShape::Rectangle;
    tool->width         = 10;
    tool->height        = 10;
    tool->material      = "_checkerboard";
    tool->palette_index = PaletteIndexAutomatic;
    mTools->push_back(tool);
    UpdateToolCombo();
    SetCurrentTool(tool->id);
    ShowCurrentTool();
    SetEnabled(mUI.btnDelTool, true);
}

void DlgTileTool::on_btnDelTool_clicked()
{
    if (auto* tool = GetCurrentTool())
    {
        QString other;
        for (auto it = mTools->begin(); it != mTools->end(); ++it)
        {
            if ((*it)->id == tool->id)
            {
                it = mTools->erase(it);
                if (it != mTools->end())
                    other = (*it)->id;
                break;
            }
            other = (*it)->id;
        }
        UpdateToolCombo();
        SetCurrentTool(other);
        ShowCurrentTool();
    }
    SetEnabled(mUI.btnDelTool, !mTools->empty());
}

void DlgTileTool::on_btnSelectToolMaterial_clicked()
{
    if (auto* tool = GetCurrentTool())
    {
        DlgMaterial dlg(this, mWorkspace, tool->material);
        dlg.SetPreviewScale(GetMaterialPreviewScale(*mClass));
        if (dlg.exec() == QDialog::Rejected)
            return;
        tool->material = app::ToUtf8(dlg.GetSelectedMaterialId());
        ShowCurrentTool();
    }
}

void DlgTileTool::on_btnSetToolMaterialParams_clicked()
{
    // todo:
}

void DlgTileTool::on_btnEditToolMaterial_clicked()
{
    ActionEvent::OpenResource open;
    open.id = GetItemId(mUI.cmbToolMaterial);
    ActionEvent::Post(open);
}

void DlgTileTool::on_btnResetPaletteIndex_clicked()
{
    SetValue(mUI.toolPaletteIndex, -1);
    ModifyCurrentTool();
}

void DlgTileTool::on_cmbToolFunction_currentIndexChanged(int)
{
    ModifyCurrentTool();
    ShowCurrentTool();
}
void DlgTileTool::on_cmbToolShape_currentIndexChanged(int)
{
    ModifyCurrentTool();
}
void DlgTileTool::on_toolWidth_valueChanged(int)
{
    ModifyCurrentTool();
}
void DlgTileTool::on_toolHeight_valueChanged(int)
{
    ModifyCurrentTool();
}

void DlgTileTool::on_cmbToolMaterial_currentIndexChanged(int)
{
    ModifyCurrentTool();
    ShowCurrentTool();
}

void DlgTileTool::on_toolPaletteIndex_valueChanged(int)
{
    ModifyCurrentTool();
}
void DlgTileTool::on_toolValue_valueChanged(int)
{
    ModifyCurrentTool();
}

void DlgTileTool::on_material_toggled()
{
    ModifyCurrentTool();
}
void DlgTileTool::on_data_toggled()
{
    ModifyCurrentTool();
}

void DlgTileTool::ResourceAdded(const app::Resource* resource)
{
    if (resource->IsMaterial())
    {
        const auto& materials = mWorkspace->ListAllMaterials();
        SetList(mUI.cmbToolMaterial, materials);
    }
}
void DlgTileTool::ResourceRemoved(const app::Resource* resource)
{
    if (resource->IsMaterial())
    {
        const auto& materials = mWorkspace->ListAllMaterials();
        SetList(mUI.cmbToolMaterial, materials);
    }
}
void DlgTileTool::ResourceUpdated(const app::Resource* resource)
{
    if (resource->IsMaterial())
    {
        const auto& materials = mWorkspace->ListAllMaterials();
        SetList(mUI.cmbToolMaterial, materials);
    }
}

void DlgTileTool::ShowCurrentTool()
{
    if (auto* tool = GetCurrentTool())
    {
        SetEnabled(mUI.cmbTool,                  true);
        SetEnabled(mUI.toolName,                 true);
        SetEnabled(mUI.cmbToolFunction,          true);
        SetEnabled(mUI.cmbToolShape,             true);
        SetEnabled(mUI.toolWidth,                true);
        SetEnabled(mUI.toolHeight,               true);
        SetEnabled(mUI.cmbToolMaterial,          true);
        SetEnabled(mUI.btnSelectToolMaterial,    true);
        SetEnabled(mUI.btnSetToolMaterialParams, false);
        SetEnabled(mUI.btnEditToolMaterial,      true);
        SetEnabled(mUI.material,                 true);
        SetEnabled(mUI.data,                     true);

        SetValue(mUI.cmbToolMaterial,  ListItemId(tool->material));
        SetValue(mUI.toolName,         tool->name);
        SetValue(mUI.cmbToolFunction,  tool->tool);
        SetValue(mUI.cmbToolShape,     tool->shape);
        SetValue(mUI.toolWidth,        tool->width);
        SetValue(mUI.toolHeight,       tool->height);
        SetValue(mUI.toolPaletteIndex, tool->palette_index);
        SetValue(mUI.toolValue,        tool->value);
        SetValue(mUI.material,         tool->apply_material);
        SetValue(mUI.data,             tool->apply_value);

        if (mWorkspace->IsUserDefinedResource(tool->material))
            SetEnabled(mUI.btnEditToolMaterial, true);
        else SetEnabled(mUI.btnEditToolMaterial, false);
    }
    else
    {

        SetEnabled(mUI.cmbTool,                  false);
        SetEnabled(mUI.toolName,                 false);
        SetEnabled(mUI.cmbToolFunction,          false);
        SetEnabled(mUI.cmbToolShape,             false);
        SetEnabled(mUI.toolWidth,                false);
        SetEnabled(mUI.toolHeight,               false);
        SetEnabled(mUI.cmbToolMaterial,          false);
        SetEnabled(mUI.btnSelectToolMaterial,    false);
        SetEnabled(mUI.btnSetToolMaterialParams, false);
        SetEnabled(mUI.btnEditToolMaterial,      false);
        SetEnabled(mUI.toolValue,                false);
        SetEnabled(mUI.material,                 false);
        SetEnabled(mUI.data,                     false);

        SetValue(mUI.toolName, QString(""));
        SetValue(mUI.cmbToolFunction, -1);
        SetValue(mUI.cmbToolShape,    -1);
        SetValue(mUI.toolWidth,        0);
        SetValue(mUI.toolHeight,       0);
        SetValue(mUI.cmbToolMaterial, -1);
        SetValue(mUI.toolValue,        0);
    }
}

TileTool* DlgTileTool::GetCurrentTool()
{
    if (mTools->empty())
        return nullptr;

    for (size_t index=0; index<mTools->size(); ++index)
    {
        if ((*mTools)[index]->id == GetItemId(mUI.cmbTool))
            return (*mTools)[index].get();
    }
    BUG("No such tool was found.");
    return nullptr;
}

void DlgTileTool::SetCurrentTool(const QString& id)
{
    SetValue(mUI.cmbTool, ListItemId(id));
}

void DlgTileTool::ModifyCurrentTool()
{
    if (auto* tool = GetCurrentTool())
    {
        tool->name           = (const QString&)GetValue(mUI.toolName);
        tool->tool           = GetValue(mUI.cmbToolFunction);
        tool->width          = GetValue(mUI.toolWidth);
        tool->height         = GetValue(mUI.toolHeight);
        tool->shape          = GetValue(mUI.cmbToolShape);
        tool->material       = GetItemId(mUI.cmbToolMaterial);
        tool->palette_index  = GetValue(mUI.toolPaletteIndex);
        tool->value          = GetValue(mUI.toolValue);
        tool->apply_material = GetValue(mUI.material);
        tool->apply_value    = GetValue(mUI.data);
    }
}

void DlgTileTool::UpdateToolCombo()
{
    std::vector<ListItem> items;
    for (const auto& tool : *mTools)
    {
        ListItem item;
        item.name = tool->name;
        item.id   = tool->id;
        items.push_back(std::move(item));
    }
    SetList(mUI.cmbTool, items);

    if (NotifyToolBoxUpdate)
        NotifyToolBoxUpdate();
}



} // namespace