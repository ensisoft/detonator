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

#include "graphics/simple_shape.h"
#include "graphics/tilebatch.h"
#include "graphics/guidegrid.h"
#include "graphics/material_instance.h"
#include "engine/camera.h"
#include "editor/gui/dlgtiletool.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/utility.h"
#include "editor/gui/types.h"
#include "editor/gui/nerd.h"
#include "editor/gui/settings.h"

namespace {
    constexpr auto PaletteIndexAutomatic = -1;

    gui::Size2Df GetMaterialPreviewScale(const game::TilemapClass& klass)
    {
        const auto perspective = klass.GetPerspective();
        if (perspective == game::TilemapClass::Perspective::AxisAligned)
            return {1.0f, 1.0};
        else if (perspective == game::TilemapClass::Perspective::Dimetric ||
                 perspective == game::TilemapClass::Perspective::Isometric)
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

    connect(&mTimer, &QTimer::timeout, mUI.widget, &GfxWidget::triggerPaint);
    connect(this, &QDialog::finished, mUI.widget, &GfxWidget::dispose);

    mUI.widget->onPaintScene       = std::bind(&DlgTileTool::PaintScene, this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onKeyPress         = std::bind(&DlgTileTool::KeyPress, this, std::placeholders::_1);
    mUI.widget->onMousePress       = std::bind(&DlgTileTool::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&DlgTileTool::MouseDoubleClick, this, std::placeholders::_1);
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
        ShowCurrentTile();
    }

    // hide this for now
    SetVisible(mUI.transform, false);
    SetValue(mUI.zoom, 1.0f);
}

void DlgTileTool::SaveState(Settings& settings) const
{
    settings.SetValue("dialog", "geometry", saveGeometry());
    settings.SaveWidget("dialog", mUI.zoom);
    settings.SaveWidget("dialog", mUI.widget);
    settings.SaveWidget("dialog", mUI.chkGrid);
}
void DlgTileTool::LoadState(const Settings& settings)
{
    QByteArray geometry;
    if (settings.GetValue("dialog", "geometry", &geometry))
        restoreGeometry(geometry);

    settings.LoadWidget("dialog", mUI.zoom);
    settings.LoadWidget("dialog", mUI.widget);
    settings.LoadWidget("dialog", mUI.chkGrid);
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
    tool->width         = 1;
    tool->height        = 1;

    TileTool::Tile tile;
    tile.material      = "_checkerboard";
    tile.palette_index = PaletteIndexAutomatic;
    tool->tiles.push_back(std::move(tile));
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
    if (auto* tile = GetCurrentTile())
    {
        DlgMaterial dlg(this, mWorkspace, false);
        dlg.SetSelectedMaterialId(tile->material);
        dlg.SetTileIndex(tile->tile_index);
        dlg.SetPreviewScale(GetMaterialPreviewScale(*mClass));
        if (dlg.exec() == QDialog::Rejected)
            return;

        tile->material = dlg.GetSelectedMaterialId();
        tile->tile_index = dlg.GetTileIndex();
        tile->apply_material = true;

        ShowCurrentTool();
        ShowCurrentTile();
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
    ModifyCurrentTile();
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
    ModifyCurrentTile();
}

void DlgTileTool::on_toolPaletteIndex_valueChanged(int)
{
    ModifyCurrentTile();
}
void DlgTileTool::on_toolValue_valueChanged(int)
{
    ModifyCurrentTile();
}

void DlgTileTool::on_material_toggled()
{
    ModifyCurrentTile();
}
void DlgTileTool::on_data_toggled()
{
    ModifyCurrentTile();
}

void DlgTileTool::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void DlgTileTool::on_tileCol_valueChanged(int)
{
    if (const auto* tool = GetCurrentTool())
    {
        int value = GetValue(mUI.tileCol);
        if (value >= tool->width)
            SetValue(mUI.tileCol, tool->width-1);

        ShowCurrentTile();
    }
}
void DlgTileTool::on_tileRow_valueChanged(int)
{
    if (const auto* tool = GetCurrentTool())
    {
        int value = GetValue(mUI.tileRow);
        if (value >= tool->height)
            SetValue(mUI.tileRow, tool->height-1);

        ShowCurrentTile();
    }
}

void DlgTileTool::on_tileIndex_valueChanged(int)
{
    ModifyCurrentTile();
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

        if (auto* tool = GetCurrentTool())
        {
            for (auto& tile : tool->tiles)
            {
                if (tile.material == resource->GetId())
                    tile.material = "_checkerboard";
            }
        }

        ShowCurrentTile();
    }
}
void DlgTileTool::ResourceUpdated(const app::Resource* resource)
{
    if (resource->IsMaterial())
    {
        const auto& materials = mWorkspace->ListAllMaterials();
        SetList(mUI.cmbToolMaterial, materials);

        if (auto* tool = GetCurrentTool())
        {
            for (auto& tile : tool->tiles)
            {
                if (tile.material == resource->GetId())
                    tile.material_instance.reset();
            }
        }

        ShowCurrentTile();
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

    for (unsigned row=0; row<tool->height; ++row)
    {
        for (unsigned col=0; col<tool->width; ++col)
        {
            const auto& tile_data = tool->tiles[row * tool->width + col];
            if (!tile_data.apply_material)
                continue;

            gfx::TileBatch::Tile tile;
            tile.pos.x = col - (tool->width / 2.0f);
            tile.pos.y = row - (tool->height / 2.0f);
            tile.pos.z = 0;
            tile.data.x = tile_data.tile_index;

            gfx::TileBatch batch;
            batch.AddTile(tile);
            batch.SetTileWorldSize(tile_size * cuboid_scale);
            batch.SetTileRenderWidth(render_size.x * tile_render_width_scale);
            batch.SetTileRenderHeight(render_size.y * tile_render_height_scale);
            batch.SetTileShape(gfx::TileBatch::TileShape::Automatic);
            if (perspective == game::Tilemap::Perspective::AxisAligned)
                batch.SetProjection(gfx::TileBatch::Projection::AxisAligned);
            else if (perspective == game::Tilemap::Perspective::Dimetric)
                batch.SetProjection(gfx::TileBatch::Projection::Dimetric);
            else if (perspective == game::Tilemap::Perspective::Isometric)
                batch.SetProjection(gfx::TileBatch::Projection::Isometric);
            else BUG("missing tile projection.");

            // re-create the material if the tool's material setting has changed.
            if (!tile_data.material_instance ||
                 tile_data.material_instance->GetClassId() != tile_data.material)
            {
                auto klass = mWorkspace->GetMaterialClassById(tile_data.material);
                tile_data.material_instance = gfx::CreateMaterialInstance(klass);
            }

            scene_painter.Draw(batch, tile_projection_transform_matrix, *tile_data.material_instance);
        }
    }

    const auto tool_cols_tiles = tool->width;
    const auto tool_rows_tiles = tool->height;
    const auto tile_grid_width = tool_cols_tiles * tile_width_units;
    const auto tile_grid_height = tool_rows_tiles * tile_height_units;

    // visualize the tool tile grid
    if (GetValue(mUI.chkGrid))
    {
        gfx::Transform transform;
        transform.Resize(tile_grid_width, tile_grid_height);
        transform.Translate(-tile_grid_width * 0.5f, -tile_grid_height * 0.5f);
        tile_painter.Draw(gfx::Grid(tool_cols_tiles - 1, tool_rows_tiles - 1, true), transform,
                          gfx::CreateMaterialFromColor(gfx::Color::LightGray));
    }

    // visualize the currently selected tile
    {
        const unsigned current_tile_col = GetValue(mUI.tileCol);
        const unsigned current_tile_row = GetValue(mUI.tileRow);
        gfx::Transform transform;
        transform.Resize(tile_width_units, tile_height_units);
        transform.Translate(-tile_grid_width * 0.5f, -tile_grid_height * 0.5f);
        transform.Translate(tile_width_units * current_tile_col,
                            tile_height_units * current_tile_row);
        tile_painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Outline), transform,
                          gfx::CreateMaterialFromColor(gfx::Color::Green), 2.0f);
    }

    // visualize the tile under the mouse
    unsigned tile_col = 0;
    unsigned tile_row = 0;
    if (GetTileUnderMouse(&tile_col, &tile_row))
    {
        gfx::Transform transform;
        transform.Resize(tile_width_units, tile_height_units);
        transform.Translate(-tile_grid_width *0.5f, -tile_grid_height * 0.5f);
        transform.Translate(tile_width_units * tile_col,
                            tile_height_units * tile_row);
        tile_painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Outline), transform,
                          gfx::CreateMaterialFromColor(gfx::Color::HotPink), 2.0f);
    }

}

bool DlgTileTool::KeyPress(QKeyEvent* event)
{
    const auto key = event->key();

    auto move_selection = [this](int dx, int dy) {
        if (auto* tool = GetCurrentTool())
        {
            int col = GetValue(mUI.tileCol);
            int row = GetValue(mUI.tileRow);
            if (col + dx >= 0 && col + dx < int(tool->width))
                SetValue(mUI.tileCol, col + dx);
            if (row + dy >= 0 && row + dy < int(tool->height))
                SetValue(mUI.tileRow, row + dy);
            ShowCurrentTile();
        }
    };

    if (key == Qt::Key_Escape)
    {
        close();
    }
    else if (key == Qt::Key_Space)
    {
        on_btnSelectToolMaterial_clicked();
    }
    else if (key == Qt::Key_Delete || key == Qt::Key_Backspace)
    {
        if (auto* tile = GetCurrentTile())
        {
            tile->material = "_checkerboard";
            tile->apply_material = false;
            ShowCurrentTile();
        }
    }
    else if (key == Qt::Key_Up)
    {
        move_selection(0, -1);
    }
    else if (key == Qt::Key_Down)
    {
        move_selection(0, 1);
    }
    else if (key == Qt::Key_Left)
    {
        move_selection(-1, 0);
    }
    else if (key == Qt::Key_Right)
    {
        move_selection(1, 0);
    }
    return true;
}

void DlgTileTool::MousePress(QMouseEvent* event)
{
    unsigned tile_col = 0;
    unsigned tile_row = 0;
    if  (GetTileUnderMouse(&tile_col, &tile_row))
    {
        SetValue(mUI.tileCol, tile_col);
        SetValue(mUI.tileRow, tile_row);
        ShowCurrentTile();
    }
}

void DlgTileTool::MouseDoubleClick(QMouseEvent* mickey)
{
    unsigned tile_col = 0;
    unsigned tile_row = 0;
    if (GetTileUnderMouse(&tile_col, &tile_row))
    {
        SetValue(mUI.tileCol, tile_col);
        SetValue(mUI.tileRow, tile_row);
        on_btnSelectToolMaterial_clicked();
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

        SetValue(mUI.toolName,         tool->name);
        SetValue(mUI.cmbToolFunction,  tool->tool);
        SetValue(mUI.cmbToolShape,     tool->shape);
        SetValue(mUI.toolWidth,        tool->width);
        SetValue(mUI.toolHeight,       tool->height);
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

void DlgTileTool::ShowCurrentTile()
{
    if (const auto* tile = GetCurrentTile())
    {
        SetValue(mUI.cmbToolMaterial,  ListItemId(tile->material));
        SetValue(mUI.toolPaletteIndex, tile->palette_index);
        SetValue(mUI.toolValue,        tile->value);
        SetValue(mUI.material,         tile->apply_material);
        SetValue(mUI.data,             tile->apply_value);
        SetValue(mUI.tileIndex,        tile->tile_index);
        SetEnabled(mUI.currentTile, true);

        if (mWorkspace->IsUserDefinedResource(tile->material))
            SetEnabled(mUI.btnEditToolMaterial, true);
        else SetEnabled(mUI.btnEditToolMaterial, false);
    }
    else
    {
        SetValue(mUI.cmbToolMaterial, -1);
        SetValue(mUI.toolPaletteIndex, 0);
        SetValue(mUI.toolValue,        0);
        SetValue(mUI.material,         false);
        SetValue(mUI.data,             false);
        SetEnabled(mUI.currentTile, false);
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

TileTool::Tile* DlgTileTool::GetCurrentTile()
{
    if (auto* tool = GetCurrentTool())
    {
        const unsigned col = GetValue(mUI.tileCol);
        const unsigned row = GetValue(mUI.tileRow);
        ASSERT(col < tool->width);
        ASSERT(row < tool->height);
        auto& tile = tool->tiles[row * tool->width + col];
        return &tile;
    }
    return nullptr;
}

TileTool::Tile* DlgTileTool::GetTileUnderMouse(unsigned* col, unsigned* row)
{
    auto* tool = GetCurrentTool();
    if (!tool)
        return nullptr;

    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();

    const auto& mickey = mUI.widget->mapFromGlobal(QCursor::pos());
    if (mickey.x() < 0 || mickey.x() > width)
        return nullptr;
    if (mickey.y() < 0 || mickey.y() > height)
        return nullptr;

    const auto perspective = mClass->GetPerspective();
    const auto tile_scaler       = 1.0f;
    const auto tile_width_units  = mClass->GetTileWidth() * tile_scaler;
    const auto tile_height_units = mClass->GetTileHeight() * tile_scaler;
    const auto tile_depth_units  = mClass->GetTileDepth() * tile_scaler;

    const auto tile_render_width_scale  = mClass->GetTileRenderWidthScale();
    const auto tile_render_height_scale = mClass->GetTileRenderHeightScale();

    const auto tool_cols_tiles = tool->width;
    const auto tool_rows_tiles = tool->height;

    const auto tile_grid_width = tool_cols_tiles * tile_width_units;
    const auto tile_grid_height = tool_rows_tiles * tile_height_units;

    const glm::vec2 tile_coord = MapWindowCoordinateToWorld(mUI, mState, mickey, perspective);
    const unsigned tile_col = (tile_coord.x + tile_grid_width * 0.5f) / tile_width_units;
    const unsigned tile_row = (tile_coord.y + tile_grid_height * 0.5f) / tile_height_units;
    VERBOSE("TileTool tile plane coordinate = %1 maps to => row=%2. col=%3", tile_coord, tile_row, tile_col);
    if (tile_col >= tool_cols_tiles)
        return nullptr;
    if (tile_row >= tool_rows_tiles)
        return nullptr;

    *col = tile_col;
    *row = tile_row;
    return &tool->tiles[tile_row * tool->width + tile_col];
}

void DlgTileTool::SetCurrentTool(const QString& id)
{
    SetValue(mUI.cmbTool, ListItemId(id));
}

void DlgTileTool::ModifyCurrentTool()
{
    if (auto* tool = GetCurrentTool())
    {
        const auto previous_width = tool->width;
        const auto previous_height = tool->height;

        tool->name   = (const QString&)GetValue(mUI.toolName);
        tool->tool   = GetValue(mUI.cmbToolFunction);
        tool->width  = GetValue(mUI.toolWidth);
        tool->height = GetValue(mUI.toolHeight);
        tool->shape  = GetValue(mUI.cmbToolShape);
        if (previous_width == tool->width && previous_height == tool->height)
            return;

        auto previous_tiles = std::move(tool->tiles);
        tool->tiles = std::vector<TileTool::Tile> {};

        for (unsigned row=0; row<tool->height; ++row)
        {
            for (unsigned col=0; col<tool->width; ++col)
            {
                if (row < previous_height && col < previous_width)
                {
                    const auto index = row * previous_width + col;
                    tool->tiles.push_back(std::move(previous_tiles[index]));
                }
                else
                {
                    TileTool::Tile tile;
                    tile.material       = "_checkerboard";
                    tile.palette_index  = PaletteIndexAutomatic;
                    tile.apply_material = false;
                    tile.apply_value    = false;
                    tool->tiles.push_back(std::move(tile));
                }
            }
        }
        const auto tile_col = GetValue(mUI.tileCol);
        const auto tile_row = GetValue(mUI.tileRow);
        if (tile_col >= tool->width)
            SetValue(mUI.tileCol, tool->width - 1);
        if (tile_row >= tool->height)
            SetValue(mUI.tileRow, tool->height - 1);
    }
}

void DlgTileTool::ModifyCurrentTile()
{
    if (auto* tile = GetCurrentTile())
    {
        tile->material       = GetItemId(mUI.cmbToolMaterial);
        tile->palette_index  = GetValue(mUI.toolPaletteIndex);
        tile->value          = GetValue(mUI.toolValue);
        tile->apply_material = GetValue(mUI.material);
        tile->apply_value    = GetValue(mUI.data);
        tile->tile_index     = GetValue(mUI.tileIndex);

        if (tile->apply_material)
        {
            if (!mWorkspace->IsValidMaterial(tile->material))
                tile->material = "_checkerboard";
        }
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
