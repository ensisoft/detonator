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

#include "engine/camera.h"
#include "editor/gui/dlgtiletool.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/utility.h"
#include "editor/gui/types.h"
#include "editor/gui/nerd.h"

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

    connect(&mTimer, &QTimer::timeout, mUI.widget, &GfxWidget::triggerPaint);
    connect(this, &QDialog::finished, mUI.widget, &GfxWidget::dispose);

    mUI.widget->onPaintScene = std::bind(&DlgTileTool::PaintScene, this, std::placeholders::_1,
                                         std::placeholders::_2);
    mUI.widget->onZoomIn = [this]() {
        const float value = GetValue(mUI.zoom);
        SetValue(mUI.zoom, value + 0.1f);
    };
    mUI.widget->onZoomOut = [this]() {
        const float value = GetValue(mUI.zoom);
        SetValue(mUI.zoom, value - 0.1f);
    };

    mTimer.setInterval(1000.0/60.0f);
    mTimer.start();

    UpdateToolCombo();
    if (!mTools->empty())
    {
        SetCurrentTool((*mTools)[0]->id);
        ShowCurrentTool();
    }

    // hide this for now
    SetVisible(mUI.transform, false);
    SetValue(mUI.zoom, 1.0f);
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

void DlgTileTool::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
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

        if (mMaterial && mMaterial->GetClassId() == resource->GetId())
            mMaterial.reset();
    }
}
void DlgTileTool::ResourceUpdated(const app::Resource* resource)
{
    if (resource->IsMaterial())
    {
        const auto& materials = mWorkspace->ListAllMaterials();
        SetList(mUI.cmbToolMaterial, materials);

        if (mMaterial && mMaterial->GetClassId() == resource->GetId())
            mMaterial.reset();
    }
}

void DlgTileTool::PaintScene(gfx::Painter& painter, double)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const float zoom = GetValue(mUI.zoom);
    const float xs = GetValue(mUI.scaleY);
    const float ys = GetValue(mUI.scaleY);

    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    const auto* tool = GetCurrentTool();
    if (!tool)
        return;

    const auto perspective = mClass->GetPerspective();
    const auto tile_scaler       = 1.0f;
    const auto tile_width_units  = mClass->GetTileWidth() * tile_scaler;
    const auto tile_height_units = mClass->GetTileHeight() * tile_scaler;
    const auto tile_depth_units  = mClass->GetTileDepth() * tile_scaler;



    // Create tile painter for drawing in tile coordinate space.
    gfx::Painter tile_painter(painter.GetDevice());
    tile_painter.SetViewMatrix(CreateViewMatrix(mUI, mState, perspective));
    tile_painter.SetProjectionMatrix(CreateProjectionMatrix(mUI, engine::Projection::Orthographic));
    tile_painter.SetPixelRatio({1.0f*xs*zoom, 1.0f*ys*zoom});
    tile_painter.SetViewport(0, 0, width, height);
    tile_painter.SetSurfaceSize(width, height);

    gfx::Painter scene_painter(painter.GetDevice());
    scene_painter.SetViewMatrix(CreateViewMatrix(mUI, mState, engine::GameView::AxisAligned));
    scene_painter.SetProjectionMatrix(CreateProjectionMatrix(mUI, engine::Projection::Orthographic));
    scene_painter.SetPixelRatio({1.0f*xs*zoom, 1.0f*ys*zoom});
    scene_painter.SetViewport(0, 0, width, height);
    scene_painter.SetSurfaceSize(width, height);

    // This matrix will project a coordinate in isometric tile world space into
    // 2D screen space/surface coordinate.
    const auto& tile_projection_transform_matrix = engine::GetProjectionTransformMatrix(tile_painter.GetProjMatrix(),
                                                                                        tile_painter.GetViewMatrix(),
                                                                                        scene_painter.GetProjMatrix(),
                                                                                        scene_painter.GetViewMatrix());

    const auto tile_render_width_scale  = mClass->GetTileRenderWidthScale();
    const auto tile_render_height_scale = mClass->GetTileRenderHeightScale();
    const auto cuboid_scale = engine::GetTileCuboidFactors(perspective);
    const auto tile_size = glm::vec3{tile_width_units, tile_height_units, tile_depth_units};
    const auto render_size = engine::ComputeTileRenderSize(tile_projection_transform_matrix,
                                                           {tile_width_units, tile_height_units},
                                                           perspective);

    gfx::TileBatch batch;
    for (unsigned row=0; row<tool->height; ++row)
    {
        for (unsigned col=0; col<tool->width; ++col)
        {
            gfx::TileBatch::Tile tile;
            tile.pos.x = col - (tool->width / 2.0f);
            tile.pos.y = row - (tool->height / 2.0f);
            tile.pos.z = 0;
            batch.AddTile(tile);
        }
    }
    batch.SetTileWorldSize(tile_size * cuboid_scale);
    batch.SetTileRenderWidth(render_size.x * tile_render_width_scale);
    batch.SetTileRenderHeight(render_size.y * tile_render_height_scale);
    batch.SetTileShape(gfx::TileBatch::TileShape::Automatic);
    if (perspective == game::Tilemap::Perspective::AxisAligned)
        batch.SetProjection(gfx::TileBatch::Projection::AxisAligned);
    else if (perspective == game::Tilemap::Perspective::Dimetric)
        batch.SetProjection(gfx::TileBatch::Projection::Dimetric);
    else BUG("missing tile projection.");

    // re-create the material if the tool's material setting has changed.
    if (!mMaterial || mMaterial->GetClassId() != tool->material)
    {
        auto klass = mWorkspace->GetMaterialClassById(tool->material);
        mMaterial = gfx::CreateMaterialInstance(klass);
    }

    scene_painter.Draw(batch, tile_projection_transform_matrix, *mMaterial);

    const auto tool_cols_tiles  = tool->width;
    const auto tool_rows_tiles  = tool->height;
    const auto tile_grid_width  = tool_cols_tiles * tile_width_units;
    const auto tile_grid_height = tool_rows_tiles * tile_height_units;

    gfx::Transform transform;
    transform.Resize(tile_grid_width, tile_grid_height);
    transform.Translate(-tile_grid_width*0.5f, -tile_grid_height*0.5f);
    tile_painter.Draw(gfx::Grid(tool_cols_tiles - 1, tool_rows_tiles - 1, true),
                      transform,
                      gfx::CreateMaterialFromColor(gfx::Color::LightGray));

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