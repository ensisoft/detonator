// Copyright (C) 2020-2022 Sami Väisänen
// Copyright (C) 2020-2022 Ensisoft http://www.ensisoft.com
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

#include "config.h"

#include "warnpush.h"
#  include <QAbstractTableModel>
#  include <QMessageBox>
#include "warnpop.h"

#include <functional>
#include <cstring>

#include "data/json.h"
#include "game/loader.h"
#include "engine/camera.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/material_instance.h"
#include "graphics/utility.h"
#include "graphics/simple_shape.h"
#include "graphics/material_class_api.h"
#include "graphics/material_uniform.h"
#include "graphics/tilebatch.h"
#include "editor/app/format.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/resource.h"
#include "editor/app/utility.h"
#include "editor/gui/main.h"
#include "editor/gui/tool.h"
#include "editor/gui/nerd.h"
#include "editor/gui/tilemapwidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/settings.h"
#include "editor/gui/dlgtilelayer.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/dlgtiletool.h"
#include "editor/gui/palettematerial.h"
#include "editor/gui/translation.h"

namespace{

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

} // namespce

namespace gui
{

class TilemapWidget::LayerData : public game::TilemapData
{
public:
    void Write(const void* ptr, size_t bytes, size_t offset) override
    {
        ASSERT(offset + bytes <= mBytes.size());

        std::memcpy(&mBytes[offset], ptr, bytes);
    }
    void Read(void* ptr, size_t bytes, size_t offset) const override
    {
        ASSERT(offset + bytes <= mBytes.size());

        std::memcpy(ptr, &mBytes[offset],bytes);
    }
    size_t AppendChunk(size_t bytes) override
    {
        const auto offset = mBytes.size();
        mBytes.resize(offset + bytes);
        //DEBUG("Allocated new map layer data chunk. [offset=%1, bytes=%2]", offset, Bytes {bytes});
        return offset;
    }
    size_t GetByteCount() const override
    {
        return mBytes.size();
    }
    void Resize(size_t bytes) override
    {
        mBytes.resize(bytes);
        //DEBUG("Resized map layer buffer to new size. [bytes=%1]", Bytes {bytes});
    }

    void ClearChunk(const void* value, size_t value_size, size_t offset, size_t num_values) override
    {
        ASSERT(offset + value_size * num_values <= mBytes.size());

        for (size_t i=0; i<num_values; ++i)
        {
            const auto buffer_offset = offset + i * value_size;
            std::memcpy(&mBytes[buffer_offset], value, value_size);
        }
    }

    void Delete(const game::TilemapClass& map, const game::TilemapLayerClass& layer, app::Workspace& workspace)
    {
        std::string uri;
        std::string id;
        for (const auto& item : workspace.ListDataFiles())
        {
            const auto* resource = item.resource;
            const app::DataFile* data = nullptr;
            resource->GetContent(&data);
            if (data->GetTypeTag() != app::DataFile::TypeTag::TilemapData)
                continue;

            if (data->GetOwnerId() == layer.GetId())
            {
                uri = data->GetFileURI();
                id  = data->GetId();
                break;
            }
        }
        if (!id.empty())
        {
            // this will also delete the data file if any.
            workspace.DeleteResource(id);
        }
    }
    bool Load(const game::TilemapClass& map, const game::TilemapLayerClass& layer, const QString& file)
    {
        const auto tile_size = layer.GetTileDataSize();
        const auto num_cols  = layer.MapDimension(map.GetMapWidth());
        const auto num_rows  = layer.MapDimension(map.GetMapHeight());

        if (!app::ReadBinaryFile(file, mBytes))
        {
            ERROR("Failed to read layer data file. [file='%1']", file);
            return false;
        }
        DEBUG("Loaded layer data file. [file='%1', bytes=%2]", file, Bytes{mBytes.size()});
        return true;
    }

    bool Save(const game::TilemapClass& map, const game::TilemapLayerClass& layer, const QString& file) const
    {
        if (!app::WriteBinaryFile(file, mBytes))
        {
            ERROR("Failed to write layer data file. [file='%1']", file);
            return false;
        }
        DEBUG("Wrote tilemap layer data in a temp file. [file='%1', bytes=%2]", file, Bytes{mBytes.size()});
        return true;
    }

    bool Load(const game::TilemapClass& map, const game::TilemapLayerClass& layer, const app::Workspace& workspace)
    {
        const auto tile_size = layer.GetTileDataSize();
        const auto num_cols  = layer.MapDimension(map.GetMapWidth());
        const auto num_rows  = layer.MapDimension(map.GetMapHeight());

        std::string uri;
        for (const auto& item : workspace.ListDataFiles())
        {
            const auto* resource = item.resource;
            const app::DataFile* data = nullptr;
            resource->GetContent(&data);
            if (data->GetTypeTag() != app::DataFile::TypeTag::TilemapData)
                continue;
            if (data->GetOwnerId() == layer.GetId()) {
                uri = data->GetFileURI();
                break;
            }
        }
        if (uri.empty())
        {
            DEBUG("Tilemap layer has no data saved. [layer='%1']", layer.GetName());
            return false;
        }
        const auto& file = workspace.MapFileToFilesystem(uri);
        if (!app::ReadBinaryFile(file, mBytes))
        {
            ERROR("Failed to read layer data file. [layer='%1', file='%2']", layer.GetName(), file);
            return false;
        }
        DEBUG("Loaded layer data. [layer='%1', file='%2', bytes=%3]", layer.GetName(), file, Bytes {mBytes.size()});
        return true;
    }
    bool Save(const game::TilemapClass& map, game::TilemapLayerClass& layer, app::Workspace& workspace) const
    {
        // first write the binary data out into the file.
        const auto& data = workspace.GetSubDir("data");
        const auto& name = app::toString("%1.bin", layer.GetId());
        const auto& file = app::JoinPath(data, name);
        if (!app::WriteBinaryFile(file, mBytes))
        {
            ERROR("Failed to write layer data file. [file='%1']", file);
            return false;
        }
        INFO("Saved tilemap layer data file '%1'.", file);
        const auto& resource_name = app::toString("%1 Layer Data", map.GetName());
        // check if we already have an associated data file resource
        // for this tile map layer.
        for (const auto& item : workspace.ListDataFiles())
        {
            const app::DataFile* blob = nullptr;
            const app::Resource* resource = item.resource;
            resource->GetContent(&blob);
            if (blob->GetOwnerId() == layer.GetId())
            {
                if (item.name != resource_name)
                {
                    // hack
                    const_cast<app::Resource*>(resource)->SetName(resource_name);
                    workspace.UpdateResource(resource);
                }
                return true;
            }
        }

        // make a new data resource which refers to the datafile
        const auto uri = workspace.MapFileToWorkspace(file);
        app::DataFile res;
        res.SetFileURI(uri);
        res.SetOwnerId(layer.GetId());
        res.SetTypeTag(app::DataFile::TypeTag::TilemapData);
        app::DataResource  resource(res, resource_name);
        workspace.SaveResource(resource);
        // save the data file URI mapping into the layer object.
        layer.SetDataUri(app::ToUtf8(uri));
        layer.SetDataId(res.GetId());
        return true;
    }
    size_t GetHash() const
    {
        size_t hash = 0;
        for (auto c : mBytes)
            hash = base::hash_combine(hash, c);
        return hash;
    }

private:
    std::vector<unsigned char> mBytes;
};

class TilemapWidget::LayerModel : public QAbstractTableModel
{
public:
    explicit LayerModel(TilemapWidget::State& state) noexcept
      : mState(state)
    {}
    QVariant data(const QModelIndex& index, int role) const override
    {
        const auto col = index.column();
        const auto row = index.row();
        const auto& layer = mState.klass->GetLayer(row);

        if (role == Qt::DisplayRole)
        {
            if (col == 0) return app::toString(layer.GetType());
            else if (col == 1) return app::toString(layer.GetName());
            else BUG("Missing layer table column index.");
        }
        else if (role == Qt::DecorationRole)
        {
            if (col == 0)
            {
                if (layer.TestFlag(game::TilemapLayerClass::Flags::VisibleInEditor))
                    return QIcon("icons:eye.png");
                else return QIcon("icons:crossed_eye.png");
            }
        }
        return {};
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            if (section == 0) return "Type";
            else if (section == 1) return "Name";
            else BUG("Missing layer table column index.");
        }
        return {};
    }
    int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mState.klass->GetNumLayers());
    }
    int columnCount(const QModelIndex&) const override
    {
        return 2;
    }
    void AddLayer(std::shared_ptr<game::TilemapLayerClass> layer)
    {
        const auto count = static_cast<int>(mState.klass->GetNumLayers());
        QAbstractTableModel::beginInsertRows(QModelIndex(), count, count);
        mState.klass->AddLayer(std::move(layer));
        QAbstractTableModel::endInsertRows();
    }
    void DeleteLayer(size_t index)
    {
        const auto row = static_cast<int>(index);

        QAbstractTableModel::beginRemoveRows(QModelIndex(), row, row);
        mState.klass->DeleteLayer(index);
        QAbstractTableModel::endInsertRows();
    }
    void Reset()
    {
        beginResetModel();
        endResetModel();
    }
    void Refresh()
    {
        const auto rows = static_cast<int>(mState.klass->GetNumLayers());
        emit dataChanged(index(0, 0), index(rows, 2));
    }
private:
    TilemapWidget::State& mState;
};

class TilemapWidget::TileMoveTool : public MouseTool
{
public:
    TileMoveTool(State& state, game::TilemapLayer& layer)
      : mState(state)
      , mLayer(layer)
    {
        ASSERT(mState.selection.HasSelection());
        const auto& selection = mState.selection;
        mTiles.resize(selection.GetTileCount());

        // for each selected tile create a corresponding visual tile
        // for visualizing the tile that is getting moved to the user.
        for (unsigned i=0; i<selection.GetTileCount(); ++i)
        {
            const auto& tile = selection.GetTile(i);

            std::uint8_t palette_index = 0;
            ASSERT(mLayer.GetTilePaletteIndex(&palette_index, tile.y, tile.x));
            if (palette_index == mLayer.GetMaxPaletteIndex())
                continue;

            std::shared_ptr<const gfx::MaterialClass> klass;
            const auto& materialId = mLayer.GetEffectiveMaterialId(palette_index);
            if (materialId.empty())
                klass = mState.workspace->GetMaterialClassById("_checkerboard");
            else if (!mState.workspace->IsValidMaterial(materialId))
                klass = mState.workspace->GetMaterialClassById("_checkerboard");
            else klass = mState.workspace->GetMaterialClassById(materialId);

            mTiles[i].material = gfx::CreateMaterialInstance(klass);
            mTiles[i].material->SetUniform<gfx::kBaseColor>(gfx::Color::HotPink);
            mTiles[i].tile_index    = mLayer->GetPaletteMaterialTileIndex(palette_index);
            mTiles[i].palette_index = palette_index;
        }
    }
    void Render(gfx::Painter& scene_painter, gfx::Painter& tile_painter) const override
    {
        if (!mSelectionMoved)
            return;

        // This matrix will project a coordinate in isometric tile world space into
        // 2D screen space/surface coordinate.
        const auto& tile_projection_transform_matrix = engine::GetProjectionTransformMatrix(tile_painter.GetProjMatrix(),
                                                                                            tile_painter.GetViewMatrix(),
                                                                                            scene_painter.GetProjMatrix(),
                                                                                            scene_painter.GetViewMatrix());
        const auto perspective = mState.klass->GetPerspective();
        const auto tile_scaler = mLayer.GetTileSizeScaler();
        const auto tile_width_units = mState.klass->GetTileWidth() * tile_scaler;
        const auto tile_height_units = mState.klass->GetTileHeight() * tile_scaler;
        const auto tile_depth_units  = mState.klass->GetTileDepth() * tile_scaler;
        const auto tile_render_width_scale = mState.klass->GetTileRenderWidthScale();
        const auto tile_render_height_scale = mState.klass->GetTileRenderHeightScale();
        const auto tile_size = glm::vec3{tile_width_units, tile_height_units, tile_depth_units};
        const auto render_size = engine::ComputeTileRenderSize(tile_projection_transform_matrix,
                                                               {tile_width_units, tile_height_units},
                                                               perspective);
        gfx::FRect tool_bounds;

        const auto& selection = mState.selection;
        ASSERT(mTiles.size() == selection.GetTileCount());

        for (unsigned i=0; i<selection.GetTileCount(); ++i)
        {
            const auto& selected_tile = selection.GetTile(i);
            const auto& visual_tile = mTiles[i];
            if (!visual_tile.material)
                continue;

            gfx::TileBatch::Tile tile;
            tile.pos.y  = selected_tile.y;
            tile.pos.x  = selected_tile.x;
            tile.pos.z  = mLayer.GetDepth();
            tile.data.x = visual_tile.tile_index;

            gfx::TileBatch batch;
            batch.AddTile(tile);
            batch.SetTileWorldSize(tile_size);
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

            // draw the tile batch
            scene_painter.Draw(batch, tile_projection_transform_matrix, *visual_tile.material);

            gfx::FRect rect;
            rect.Resize(tile_width_units, tile_height_units);
            rect.Translate(tile.pos.x * tile_width_units, tile.pos.y * tile_height_units);
            tool_bounds = Union(tool_bounds, rect);
        }

        // draw a rect around the tool on the tile plane
        {
            gfx::Transform model;
            model.Resize(tool_bounds);
            model.MoveTo(tool_bounds);
            tile_painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Outline), model,
                              gfx::CreateMaterialFromColor(gfx::Color4f(gfx::Color::HotPink, 0.8)));
        }
    }
    void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
    {
        const auto tile_scale  = mLayer.GetTileSizeScaler();
        const auto tile_width  = mState.klass->GetTileWidth() * tile_scale;
        const auto tile_height = mState.klass->GetTileHeight() * tile_scale;

        const glm::vec2 world_pos = mickey.MapToPlane();
        const auto tile_col = int(world_pos.x / tile_width);
        const auto tile_row = int(world_pos.y / tile_height);

        const int tile_dx = tile_col - mTileCol;
        const int tile_dy = tile_row - mTileRow;
        if (Editor::DebugEditor())
            VERBOSE("Tile move tool dx=%1, dy=%2 row=%3, col=%4", tile_dx, tile_dy, tile_row, tile_col);

        if (tile_dx || tile_dy)
        {
            if (!mSelectionMoved)
            {
                const auto nothing = mLayer.GetMaxPaletteIndex();
                for (unsigned i=0; i<mState.selection.GetTileCount(); ++i)
                {
                    const auto& tile = mState.selection.GetTile(i);
                    mLayer.SetTilePaletteIndex(nothing, tile.y, tile.x);
                }
            }
            mSelectionMoved = true;

            mState.selection.ShiftSelection(tile_dx, tile_dy, mLayer.GetWidth(), mLayer.GetHeight());
        }

        mTileCol = tile_col;
        mTileRow = tile_row;
    }
    void MousePress(const MouseEvent& mickey, gfx::Transform&) override
    {
        const auto tile_scale  = mLayer.GetTileSizeScaler();
        const auto tile_width  = mState.klass->GetTileWidth() * tile_scale;
        const auto tile_height = mState.klass->GetTileHeight() * tile_scale;

        const glm::vec2 world_pos = mickey.MapToPlane();
        mTileCol  = world_pos.x / tile_width;
        mTileRow  = world_pos.y / tile_height;

        if (Editor::DebugEditor())
            VERBOSE("Tile move tool start at row=%1, col=%2", mTileRow, mTileCol);
    }
    bool MouseRelease(const MouseEvent& mickey, gfx::Transform&) override
    {
        const auto& selection = mState.selection;
        ASSERT(mTiles.size() == selection.GetTileCount());

        for (unsigned i=0; i<selection.GetTileCount(); ++i)
        {
            const auto& selected_tile = selection.GetTile(i);
            const auto& visual_tile = mTiles[i];
            if (!visual_tile.material)
                continue;

            mLayer.SetTilePaletteIndex(visual_tile.palette_index, selected_tile.y, selected_tile.x);
        }
        return true;
    }
    std::string GetName() const override
    {
        return "TileMoveTool";
    }
private:
    struct Tile {
        std::unique_ptr<gfx::Material> material;
        std::uint8_t palette_index = 0;
        std::uint32_t tile_index   = 0;
    };
    std::vector<Tile> mTiles;

    TilemapWidget::State& mState;
    game::TilemapLayer& mLayer;

    int mTileRow = 0;
    int mTileCol = 0;
    bool mSelectionMoved = false;
};

class TilemapWidget::TileSelectTool : public MouseTool
{
public:
    TileSelectTool(const game::Tilemap& map,
                   const game::TilemapLayer& layer,
                   State& state)
      : mMap(map)
      , mLayer(layer)
      , mState(state)
    {}
    void Render(gfx::Painter&, gfx::Painter& tile_painter) const override
    {
        const auto movement = mWorldPos - mWorldStartPos;
        if (movement.x <= 0.0f || movement.y <= 0.0f)
            return;

        gfx::Transform model;
        model.Scale(movement.x, movement.y);
        model.MoveTo(mWorldStartPos.x, mWorldStartPos.y);
        tile_painter.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Outline), model,
                    gfx::CreateMaterialFromColor(gfx::Color::HotPink), 2.0f);

    }
    void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
    {
        mWorldPos = mickey.MapToPlane();
    }
    void MousePress(const MouseEvent& mickey, gfx::Transform&) override
    {
        mWorldPos = mickey.MapToPlane();
        mWorldStartPos = mWorldPos;
    }
    bool MouseRelease(const MouseEvent& mickey, gfx::Transform&) override
    {
        const auto rect = GetTileRect();
        if (rect.IsEmpty())
            return true;

        TileSelection tile_selection(rect);
        tile_selection.SetResolution(mLayer->GetResolution());

        const auto mods = mickey->modifiers();
        const auto shift = mods & Qt::ShiftModifier;
        if (shift && mState.selection.HasSelection() &&
                     mState.selection.CanCombine(tile_selection))
        {
            if (tile_selection.GetTileCount() == 1)
            {
                const auto& tile = tile_selection.GetTile(0);
                if (mState.selection.IsSelected(tile))
                {
                    mState.selection.Deselect(tile);
                    return true;
                }
            }
            mState.selection = TileSelection::Combine(mState.selection, tile_selection);
            return true;
        }
        mState.selection = tile_selection;
        return true;
    }
    std::string GetName() const override
    {
        return "TileSelectTool";
    }

    base::URect GetTileRect() const
    {
        const auto movement = mWorldPos - mWorldStartPos;
        const auto selection_width  = movement.x;
        const auto selection_height = movement.y;
        if (selection_width < 0.0f || selection_height < 0.0f)
            return base::URect {};

        const auto tile_scaler = mLayer.GetTileSizeScaler();
        const auto tile_width  = mMap.GetTileWidth() * tile_scaler;
        const auto tile_height = mMap.GetTileHeight() * tile_scaler;

        const auto selection = Intersect(game::FRect(0.0f, 0.0f,
                                                     tile_width * mLayer.GetWidth(),
                                                     tile_height * mLayer.GetHeight()),
                                         game::FRect(mWorldStartPos.x, mWorldStartPos.y,
                                                     std::max(selection_width, tile_width),
                                                     std::max(selection_height, tile_height)));

        const unsigned selection_tile_xpos = selection.GetX() / tile_width;
        const unsigned selection_tile_ypos = selection.GetY() / tile_height;
        const unsigned selection_tile_width  = (unsigned)(std::ceil(selection.GetWidth() / tile_width));
        const unsigned selection_tile_height = (unsigned)(std::ceil(selection.GetHeight() / tile_height));
        return base::URect(selection_tile_xpos, selection_tile_ypos,
                           selection_tile_width, selection_tile_height);
    }

    TileSelection GetSelection() const
    {
        TileSelection selection(GetTileRect());
        selection.SetResolution(mLayer->GetResolution());
        return selection;
    }

private:
    const game::Tilemap& mMap;
    const game::TilemapLayer& mLayer;
    TilemapWidget::State& mState;
private:
    glm::vec2 mWorldStartPos = {0.0f, 0.0f};
    glm::vec2 mWorldPos = {0.0f, 0.0f};
};

class TilemapWidget::TileBrushTool : public MouseTool
{
public:
    TileBrushTool(std::shared_ptr<const TileTool> tool, State& state, game::TilemapLayer& layer)
      : mTool(std::move(tool))
      , mState(state)
      , mLayer(layer)
    {}
    void Render(gfx::Painter& scene_painter, gfx::Painter& tile_painter) const override
    {

        // This matrix will project a coordinate in isometric tile world space into
        // 2D screen space/surface coordinate.
        const auto& tile_projection_transform_matrix = engine::GetProjectionTransformMatrix(tile_painter.GetProjMatrix(),
                                                                                            tile_painter.GetViewMatrix(),
                                                                                            scene_painter.GetProjMatrix(),
                                                                                            scene_painter.GetViewMatrix());
        const auto perspective = mState.klass->GetPerspective();
        const auto tile_scaler = mLayer.GetTileSizeScaler();
        const auto tile_width_units = mState.klass->GetTileWidth() * tile_scaler;
        const auto tile_height_units = mState.klass->GetTileHeight() * tile_scaler;
        const auto tile_depth_units  = mState.klass->GetTileDepth() * tile_scaler;
        const auto tile_render_width_scale = mState.klass->GetTileRenderWidthScale();
        const auto tile_render_height_scale = mState.klass->GetTileRenderHeightScale();
        const auto tile_size = glm::vec3{tile_width_units, tile_height_units, tile_depth_units};
        const auto render_size = engine::ComputeTileRenderSize(tile_projection_transform_matrix,
                                                               {tile_width_units, tile_height_units},
                                                               perspective);
        gfx::FRect tool_bounds;

        const auto* layer = mState.klass->FindLayerById(mTool->layer);
        ASSERT(layer);
        const auto max_index = layer->GetMaxPaletteIndex();

        for (unsigned row=0; row<mTool->height; ++row)
        {
            for (unsigned col=0; col<mTool->width; ++col)
            {
                const auto tile_row = mTileRow + row - mTool->height / 2;
                const auto tile_col = mTileCol + col - mTool->width / 2;
                if (tile_row < 0 || tile_row >= mLayer.GetHeight()||
                    tile_col < 0 || tile_col >= mLayer.GetWidth())
                    continue;

                const auto& tile_data = mTool->tiles[row * mTool->width + col];
                if (!tile_data.apply_material)
                    continue;
                if (tile_data.palette_index == max_index)
                    continue;

                const auto materialId = layer->GetEffectiveMaterialId(tile_data.palette_index);
                const auto tile_index = layer->GetPaletteMaterialTileIndex(tile_data.palette_index);

                gfx::TileBatch::Tile tile;
                tile.pos.y = tile_row;
                tile.pos.x = tile_col;
                tile.pos.z = mLayer.GetDepth();
                tile.data.x = tile_index;

                gfx::TileBatch batch;
                batch.AddTile(tile);
                batch.SetTileWorldSize(tile_size);
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

                if (!tile_data.material_instance ||
                     tile_data.material_instance->GetClassId() != materialId)
                {
                    std::shared_ptr<const gfx::MaterialClass> klass;
                    if (materialId.empty())
                        klass = mState.workspace->GetMaterialClassById("_checkerboard");
                    else if (!mState.workspace->IsValidMaterial(materialId))
                        klass = mState.workspace->GetMaterialClassById("_checkerboard");
                    else klass = mState.workspace->GetMaterialClassById(materialId);

                    tile_data.material_instance = gfx::CreateMaterialInstance(klass);
                }

                // draw the tile batch
                scene_painter.Draw(batch, tile_projection_transform_matrix, *tile_data.material_instance);

                gfx::FRect rect;
                rect.Resize(tile_width_units, tile_height_units);
                rect.Translate(tile.pos.x * tile_width_units, tile.pos.y * tile_height_units);
                tool_bounds = Union(tool_bounds, rect);
            }
        }

        // draw a rect around the tool on the tile plane
        {
            gfx::Transform model;
            model.Resize(tool_bounds);
            model.MoveTo(tool_bounds);
            tile_painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Outline), model,
                              gfx::CreateMaterialFromColor(gfx::Color4f(gfx::Color::HotPink, 0.8)));
        }
    }

    void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
    {
        const auto tile_scale  = mLayer.GetTileSizeScaler();
        const auto tile_width  = mState.klass->GetTileWidth() * tile_scale;
        const auto tile_height = mState.klass->GetTileHeight() * tile_scale;

        const glm::vec2 world_pos = mickey.MapToPlane();
        mTileCol  = world_pos.x / tile_width;
        mTileRow  = world_pos.y / tile_height;

        if (mActive)
            ApplyTool();
    }
    void MousePress(const MouseEvent& mickey, gfx::Transform&) override
    {
        if (mickey->button() != Qt::LeftButton)
            return;

        mActive = true;

        // apply tool on the spot already, so that if the user simply
        // clicks the button without moving the mouse the tile under
        // the mouse is edited as expected
        ApplyTool();
    }
    bool MouseRelease(const MouseEvent& mickey, gfx::Transform&)  override
    {
        if (mickey->button() == Qt::LeftButton)
            mActive = false;
        return false;
    }

    std::string GetName() const override
    {
        return "TileBrushTool";
    }

    bool Validate() const override
    {
        return true;
    }

    void ApplyTool()
    {
        for (unsigned row=0; row<mTool->height; ++row)
        {
            for (unsigned col=0; col<mTool->width; ++col)
            {
                const auto tile_row = mTileRow + row - mTool->height / 2;
                const auto tile_col = mTileCol + col - mTool->width / 2;
                // discard tiles outside of the actual tile area.
                if (tile_row < 0 || tile_row >= mLayer.GetHeight()||
                    tile_col < 0 || tile_col >= mLayer.GetWidth())
                    continue;

                const auto& tile = mTool->tiles[row * mTool->width + col];

                // apply the material assigned to the tool onto the tiles
                // in the current layer's render component.
                if (mLayer->HasRenderComponent() && tile.apply_material)
                {
                    const auto palette_index = tile.palette_index;
                    if (mLayer->HasPaletteIndex(palette_index))
                        mLayer.SetTilePaletteIndex(tile.palette_index, tile_row, tile_col);
                }

                // apply the data value assigned to the tool onto the tiles
                // in the current layer's data component.
                if (mLayer->HasDataComponent() && tile.apply_value)
                    mLayer.SetTileValue(tile.data_value, tile_row, tile_col);
            }
        }

    }
    void SetTileRow(int row)
    { mTileRow = row; }
    void SetTileCol(int col)
    { mTileCol = col; }
private:
    std::shared_ptr<const TileTool> mTool;
    TilemapWidget::State& mState;
    game::TilemapLayer& mLayer;
    int mTileRow = 0;
    int mTileCol =0;
    bool mActive = false;
};

TilemapWidget::TilemapWidget(app::Workspace* workspace)
{
    DEBUG("Create TilemapWidget");

    mState.workspace = workspace;
    mState.klass = std::make_shared<game::TilemapClass>();
    mState.klass->SetName("My Map");
    mState.klass->SetMapWidth(512);
    mState.klass->SetMapHeight(384);
    mState.klass->SetTileWidth(10.0f);
    mState.klass->SetTileHeight(10.0f);
    mState.klass->SetTileDepth(10.0f);
    mState.map = game::CreateTilemap(mState.klass);
    mModel.reset(new LayerModel(mState));
    mState.renderer.SetEditingMode(true);
    mState.renderer.SetClassLibrary(workspace);
    mHash = mState.klass->GetHash();

    mUI.setupUi(this);
    mUI.layers->setModel(mModel.get());
    mUI.widget->onMouseMove    = std::bind(&TilemapWidget::MouseMove,    this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&TilemapWidget::MousePress,   this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&TilemapWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onMouseWheel   = std::bind(&TilemapWidget::MouseWheel,   this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&TilemapWidget::MouseDoubleClick, this, std::placeholders::_1);
    mUI.widget->onKeyPress     = std::bind(&TilemapWidget::KeyPress,     this, std::placeholders::_1);
    mUI.widget->onPaintScene   = std::bind(&TilemapWidget::PaintScene,   this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onZoomIn       = [this]() { MouseZoom(std::bind(&TilemapWidget::ZoomIn, this)); };
    mUI.widget->onZoomOut      = [this]() { MouseZoom(std::bind(&TilemapWidget::ZoomOut, this)); };

    connect(mUI.layers->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &TilemapWidget::LayerSelectionChanged);

    PopulateFromEnum<gui::GridDensity>(mUI.cmbGrid);
    PopulateFromEnum<game::TilemapClass::Perspective>(mUI.cmbPerspective);
    PopulateFromEnum<game::TilemapLayerClass::Type>(mUI.cmbLayerType);
    PopulateFromEnum<game::TilemapLayerClass::Storage>(mUI.cmbLayerStorage);
    PopulateFromEnum<game::TilemapLayerClass::Cache>(mUI.cmbLayerCache);
    PopulateFromEnum<game::TilemapLayerClass::Resolution>(mUI.cmbLayerResolution);
    PopulateFromEnum<game::TilemapLayerClass::PaletteType>(mUI.cmbPaletteType);

    mBrushMenu = new QMenu(this);
    mBrushMenu->menuAction()->setIcon(QIcon("level:brush.png"));
    mBrushMenu->menuAction()->setText("Brushes");

    mButtonBar = new QToolBar(this);
    mButtonBar->setToolButtonStyle(Qt::ToolButtonStyle::ToolButtonTextBesideIcon);
    mButtonBar->setIconSize(QSize(16, 16));
    mButtonBar->addAction(mBrushMenu->menuAction());
    mUI.toolbarLayout->addWidget(mButtonBar);

    const auto& tile_shapes = mState.workspace->ListTileShapes();

    // generate a list of widgets for the layer color palette.
    // we have a maximum of 254 palette entries for our use since
    // the highest palette entry index is used to indicate "no value set".
    for (int i=0; i<255; ++i)
    {
        auto* widget = new PaletteMaterial(mState.workspace, this);
        widget->SetPaletteIndex(i);
        widget->SetTileIndex(i);
        widget->SetOcclusion(game::TileOcclusion::None);
        widget->SetPaletteType(game::TilemapPaletteType::MultiMaterial);
        widget->SetLabel(QString("#%1").arg(i));
        widget->SetMaterialPreviewScale(Size2Df(1.0f, 1.0f));
        widget->setObjectName(QString::number(i));
        widget->UpdateDrawableList(tile_shapes);
        mUI.layout->addWidget(widget);
        mPaletteMaterialWidgets.push_back(widget);
        connect(widget, &PaletteMaterial::ValueChanged, this, &TilemapWidget::PaletteMaterialChanged);
    }

    UpdateMaterialLists();

    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.zoom, 1.0f);
    SetRange(mUI.tileValue, -0x800000, 0xffffff); // min is 24 bit signed and max is 24bit unsigned
    SetVisible(mUI.transform, false);

    DisplayMapProperties();
    DisplayLayerProperties();
    DisplayCurrentCameraLocation();
    setWindowTitle("My Map");

    auto* selection_model = mUI.layers->selectionModel();
    connect(selection_model, &QItemSelectionModel::selectionChanged, this,
            [this](const auto& selected, const auto& deselected) {
        if (!mState.selection.HasSelection())
            return;
        if (const auto* layer = GetCurrentLayer()) {
            if (layer->GetResolution() == mState.selection.GetResolution())
                return;
        }
        mState.selection.Clear();
    });

    connect(mUI.btnHamburger, &QPushButton::clicked, this, [this]() {
        if (mHamburger == nullptr)
        {
            mHamburger = new QMenu(this);
            mHamburger->addAction(mUI.chkShowViewport);
            mHamburger->addAction(mUI.chkShowOrigin);
            mHamburger->addAction(mUI.chkShowGrid);
            mHamburger->addAction(mUI.chkShowDataLayers);
            mHamburger->addAction(mUI.chkShowRenderLayers);
        }
        QPoint point;
        point.setX(0);
        point.setY(mUI.btnHamburger->width());
        mHamburger->popup(mUI.btnHamburger->mapToGlobal(point));
    });
}

TilemapWidget::TilemapWidget(app::Workspace* workspace, const app::Resource& resource)
  : TilemapWidget(workspace)
{
    DEBUG("Editing tilemap: '%1'", resource.GetName());

    const game::TilemapClass* map = nullptr;
    resource.GetContent(&map);

    mState.klass = std::make_shared<game::TilemapClass>(*map);
    mState.map   = game::CreateTilemap(mState.klass);
    mHash = mState.klass->GetHash();

    // create the data objects for each layer
    for (size_t i=0; i<mState.klass->GetNumLayers(); ++i)
    {
        auto& layer = mState.klass->GetLayer(i);
        auto data = std::make_shared<LayerData>();
        if (!data->Load(*mState.klass, layer, *mState.workspace))
        {
            layer.Initialize(mState.klass->GetMapWidth(), mState.klass->GetMapHeight(), *data);
            layer.ResetDataUri();
            layer.ResetDataId();
            WARN("Tilemap layer data buffer was re-created. [layer='%1']", layer.GetName());
        }
        mLayerData[layer.GetId()] = data;
        mHash = base::hash_combine(mHash, data->GetHash());
    }

    // load each layer instance.
    for (size_t i=0; i<mState.map->GetNumLayers(); ++i)
    {
        auto& layer = mState.map->GetLayer(i);
        auto data = mLayerData[layer.GetClassId()];
        layer.Load(data);
        layer.SetFlags(layer.GetClass().GetFlags());
    }

    // Update material scale in palette widgets
    for (auto& widget : mPaletteMaterialWidgets)
    {
        widget->SetMaterialPreviewScale(GetMaterialPreviewScale(*mState.klass));
    }


    int current_layer = -1;
    QString tile_tool_settings;
    GetUserProperty(resource, "camera_offset_x", &mState.camera_offset_x);
    GetUserProperty(resource, "camera_offset_y", &mState.camera_offset_y);
    GetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    GetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    GetUserProperty(resource, "camera_rotation", mUI.rotation);
    GetUserProperty(resource, "widget", mUI.widget);
    GetUserProperty(resource, "show_render_layers", mUI.chkShowRenderLayers);
    GetUserProperty(resource, "show_data_layers", mUI.chkShowDataLayers);
    GetUserProperty(resource, "show_viewport", mUI.chkShowViewport);
    GetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    GetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "current_layer", &current_layer);
    GetUserProperty(resource, "main_splitter", mUI.mainSplitter);
    GetUserProperty(resource, "tile_tool_settings", &tile_tool_settings);
    mTileToolSettings.FromString(tile_tool_settings);

    mTools.clear();

    size_t num_tools = 0;
    GetProperty(resource, "num_tools", &num_tools);
    for (size_t i=0;  i<num_tools; ++i)
    {
        QJsonObject json;
        GetProperty(resource, PropertyKey("tool", i), &json);
        auto tool = std::make_shared<Tool>();
        if (ToolFromJson(*tool, json))
            mTools.push_back(std::move(tool));
    }

    if (mTools.empty())
    {
        WARN("Previous tilemap tools are no longer compatible.");
        INFO("Generating new layer tools.");
        for (size_t i=0; i<mState.klass->GetNumLayers(); ++i)
        {
            const auto& layer = mState.klass->GetLayer(i);
            GenerateLayerTools(layer);
        }
    }

    mModel->Reset();
    SelectRow(mUI.layers, current_layer);
    ReplaceDeletedResources();
    UpdateToolToolbar();

    DisplayMapProperties();
    DisplayLayerProperties();
    DisplayCurrentCameraLocation();
}
TilemapWidget::~TilemapWidget()
{
    DEBUG("Destroy TilemapWidget");
}

QString TilemapWidget::GetId() const
{
    return app::FromUtf8(mState.klass->GetId());
}
QImage TilemapWidget::TakeScreenshot() const
{
    return mUI.widget->TakeScreenshot();
}

void TilemapWidget::InitializeSettings(const UISettings& settings)
{
    SetValue(mUI.chkShowViewport, settings.show_viewport);
    SetValue(mUI.chkShowGrid,     settings.show_grid);
    SetValue(mUI.chkShowOrigin,   settings.show_origin);
    SetValue(mUI.cmbGrid,         settings.grid);
    SetValue(mUI.zoom,            settings.zoom);
    SetValue(mUI.chkShowRenderLayers, true);
    SetValue(mUI.chkShowDataLayers,   true);
}
void TilemapWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionNewLayer);
}

void TilemapWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionNewLayer);
}

bool TilemapWidget::SaveState(Settings& settings) const
{
    data::JsonObject json;
    mState.klass->IntoJson(json);
    settings.SetValue("Tilemap", "content", json);
    settings.SetValue("Tilemap", "hash", mHash);
    settings.SetValue("Tilemap", "camera_offset_x", mState.camera_offset_x);
    settings.SetValue("Tilemap", "camera_offset_y", mState.camera_offset_y);
    settings.SetValue("Tilemap", "current_layer", GetCurrentRow(mUI.layers));
    settings.SaveWidget("Tilemap", mUI.widget);
    settings.SaveWidget("Tilemap", mUI.scaleX);
    settings.SaveWidget("Tilemap", mUI.scaleY);
    settings.SaveWidget("Tilemap", mUI.rotation);
    settings.SaveWidget("Tilemap", mUI.chkShowRenderLayers);
    settings.SaveWidget("Tilemap", mUI.chkShowDataLayers);
    settings.SaveWidget("Tilemap", mUI.chkShowViewport);
    settings.SaveWidget("Tilemap", mUI.chkShowOrigin);
    settings.SaveWidget("Tilemap", mUI.chkShowGrid);
    settings.SaveWidget("Tilemap", mUI.cmbGrid);
    settings.SaveWidget("Tilemap", mUI.zoom);
    settings.SaveWidget("Tilemap", mUI.mainSplitter);
    settings.SetValue("Tilemap", "tile_tool_settings", mTileToolSettings.ToString());

    settings.SetValue("Tilemap", "num_tools", mTools.size());
    for (size_t i=0; i<mTools.size(); ++i)
    {
        const auto& tool = mTools[i];
        QJsonObject json;
        ToolIntoJson(*tool, json);
        settings.SetValue("Tilemap", PropertyKey("tool", i), json);
    }

    // dump layer data in a file somewhere under the app home.
    // this is possibly using a lot of disk space, but the only other
    // option would be to overwrite whatever was currently saved in the
    // workspace (after possibly asking the user for an okay).
    settings.SetValue("Tilemap", "num_layers", mState.klass->GetNumLayers());
    for (size_t i=0; i<mState.klass->GetNumLayers(); ++i)
    {
        auto& layer_klass = mState.klass->GetLayer(i);
        auto& layer_inst = mState.map->GetLayer(i);
        layer_inst.FlushCache();
        layer_inst.Save();

        const auto* data = base::SafeFind(mLayerData, layer_klass.GetId());
        const auto& temp = app::RandomString();
        const auto& path = app::GetAppHomeFilePath("temp");
        const auto& file = app::GetAppHomeFilePath("temp/" + temp + ".bin");
        if (!(*data)->Save(*mState.klass, layer_klass, file))
        {
            return false;
        }
        QJsonObject json;
        app::JsonWrite(json, "layer_class_id", layer_klass.GetId());
        app::JsonWrite(json, "layer_data_file", file);
        settings.SetValue("Tilemap", PropertyKey("layer", i), json);
    }

    return true;
}
bool TilemapWidget::LoadState(const Settings& settings)
{
    int current_layer = -1;
    data::JsonObject json;
    settings.GetValue("Tilemap", "content", &json);
    settings.GetValue("Tilemap", "hash", &mHash);
    settings.GetValue("Tilemap", "camera_offset_x", &mState.camera_offset_x);
    settings.GetValue("Tilemap", "camera_offset_y", &mState.camera_offset_y);
    settings.GetValue("Tilemap", "current_layer", &current_layer);
    settings.LoadWidget("Tilemap", mUI.widget);
    settings.LoadWidget("Tilemap", mUI.scaleX);
    settings.LoadWidget("Tilemap", mUI.scaleY);
    settings.LoadWidget("Tilemap", mUI.rotation);
    settings.LoadWidget("Tilemap", mUI.chkShowRenderLayers);
    settings.LoadWidget("Tilemap", mUI.chkShowDataLayers);
    settings.LoadWidget("Tilemap", mUI.chkShowViewport);
    settings.LoadWidget("Tilemap", mUI.chkShowOrigin);
    settings.LoadWidget("Tilemap", mUI.chkShowGrid);
    settings.LoadWidget("Tilemap", mUI.cmbGrid);
    settings.LoadWidget("Tilemap", mUI.zoom);
    settings.LoadWidget("Tilemap", mUI.mainSplitter);

    QString tile_tool_settings;
    settings.GetValue("Tilemap", "tile_tool_settings", &tile_tool_settings);
    mTileToolSettings.FromString(tile_tool_settings);

    mState.klass = std::make_shared<game::TilemapClass>();
    if (mState.klass->FromJson(json))
        WARN("Failed to restore tilemap state.");
    mState.map  = game::CreateTilemap(mState.klass);

    mTools.clear();

    size_t num_tools  = 0;
    size_t num_layers = 0;
    settings.GetValue("Tilemap", "num_tools", &num_tools);
    settings.GetValue("Tilemap", "num_layers", &num_layers);

    for (size_t i=0; i<num_tools; ++i)
    {
        QJsonObject json;
        settings.GetValue("Tilemap", PropertyKey("tool", i), &json);
        auto tool = std::make_shared<Tool>();
        if (ToolFromJson(*tool, json))
            mTools.push_back(std::move(tool));
    }

    if (mTools.empty())
    {
        WARN("Previous tilemap tools are no longer compatible.");
        INFO("Generating new layer tools.");
        for (size_t i=0; i<mState.klass->GetNumLayers(); ++i)
        {
            const auto& layer = mState.klass->GetLayer(i);
            GenerateLayerTools(layer);
        }
    }

    const auto map_width = mState.klass->GetMapWidth();
    const auto map_height = mState.klass->GetMapHeight();

    for (size_t i=0; i<num_layers; ++i)
    {
        QJsonObject json;
        settings.GetValue("Tilemap", PropertyKey("layer", i), &json);

        std::string id;
        QString file;
        app::JsonReadSafe(json, "layer_class_id", &id);
        app::JsonReadSafe(json, "layer_data_file", &file);

        const auto& layer_klass = mState.klass->GetLayer(i);
        ASSERT(layer_klass.GetId() == id);

        auto data = std::make_shared<LayerData>();
        if (!data->Load(*mState.klass, layer_klass, file))
        {
            layer_klass.Initialize(map_width, map_height, *data);
            WARN("Tilemap layer data buffer was re-created. [layer='%1']", layer_klass.GetName());
        }
        mLayerData[id] = data;
        QFile::remove(file);
    }

    for (size_t i=0; i<mState.map->GetNumLayers(); ++i)
    {
        auto& layer = mState.map->GetLayer(i);
        auto data = mLayerData[layer.GetClassId()];
        layer.Load(data);
        layer.SetFlags(layer.GetClass().GetFlags());
    }

    // Update material scale in palette widgets
    for (auto& widget : mPaletteMaterialWidgets)
    {
        widget->SetMaterialPreviewScale(GetMaterialPreviewScale(*mState.klass));
    }

    mModel->Reset();
    SelectRow(mUI.layers, current_layer);
    UpdateToolToolbar();

    DisplayMapProperties();
    DisplayLayerProperties();
    DisplayCurrentCameraLocation();
    return true;
}
bool TilemapWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    switch (action)
    {
        case Actions::CanZoomIn:
            return CanZoomIn(mUI.zoom);
        case Actions::CanZoomOut:
            return CanZoomOut(mUI.zoom);
        case Actions::CanReloadShaders:
        case Actions::CanReloadTextures:
            return true;
        case Actions::CanScreenshot:
            return true;
    }
    return false;
}
void TilemapWidget::Cut(Clipboard& clipboard)
{}
void TilemapWidget::Copy(Clipboard& clipbboad)  const
{}
void TilemapWidget::Paste(const Clipboard& clipboard)
{}
void TilemapWidget::ZoomIn()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value + 0.1f);
}
void TilemapWidget::ZoomOut()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value - 0.1f);
}
void TilemapWidget::ReloadShaders()
{
    mUI.widget->ReloadShaders();
}
void TilemapWidget::ReloadTextures()
{
    mUI.widget->ReloadTextures();
}
void TilemapWidget::Shutdown()
{
    mUI.widget->Dispose();
}
void TilemapWidget::Update(double dt)
{
    mCurrentTime += dt;

    mAnimator.Update(mUI, mState);

}
void TilemapWidget::Render()
{
    mUI.widget->TriggerPaint();
}
void TilemapWidget::Save()
{
    on_actionSave_triggered();
}
void TilemapWidget::Undo()
{}

bool TilemapWidget::HasUnsavedChanges() const
{
    size_t hash = 0;
    hash = mState.klass->GetHash();

    for (size_t i=0; i<mState.map->GetNumLayers(); ++i)
    {
        auto& layer = mState.map->GetLayer(i);
        layer.FlushCache();
        const auto* data = base::SafeFind(mLayerData, layer.GetClassId());
        hash = base::hash_combine(hash, (*data)->GetHash());
    }
    return mHash != hash;
}
bool TilemapWidget::OnEscape()
{
     mCameraTool.reset();

    if (mCurrentTool)
    {
        StopTool();
    }
    else if (mState.selection.HasSelection())
    {
        mState.selection.Clear();
        DisplaySelection();
    }
    else if (GetCurrentLayer())
    {
        SelectRow(mUI.layers, -1);
        DisplayLayerProperties();
    }
    else
    {
        on_btnViewReset_clicked();
    }
    return false;
}
bool TilemapWidget::OnKeyDown(QKeyEvent* key)
{
    if (key->key() == Qt::Key_1)
        return SelectLayerOnKey(0);
    else if (key->key() == Qt::Key_2)
        return SelectLayerOnKey(1);
    else if (key->key() == Qt::Key_3)
        return SelectLayerOnKey(2);
    else if (key->key() == Qt::Key_4)
        return SelectLayerOnKey(3);
    else if (key->key() == Qt::Key_5)
        return SelectLayerOnKey(4);
    else if (key->key() == Qt::Key_6)
        return SelectLayerOnKey(5);
    else if (key->key() == Qt::Key_7)
        return SelectLayerOnKey(6);
    else if (key->key() == Qt::Key_8)
        return SelectLayerOnKey(7);
    else if (key->key() == Qt::Key_9)
        return SelectLayerOnKey(8);
    return false;
}

bool TilemapWidget::GetStats(Stats* stats) const
{
    stats->time  = 0.0f;
    stats->graphics.valid = true;
    stats->graphics.fps   = mUI.widget->GetCurrentFPS();
    const auto& dev_stats = mUI.widget->GetDeviceResourceStats();
    stats->device.static_vbo_mem_alloc    = dev_stats.static_vbo_mem_alloc;
    stats->device.static_vbo_mem_use      = dev_stats.static_vbo_mem_use;
    stats->device.dynamic_vbo_mem_alloc   = dev_stats.dynamic_vbo_mem_alloc;
    stats->device.dynamic_vbo_mem_use     = dev_stats.dynamic_vbo_mem_use;
    stats->device.streaming_vbo_mem_use   = dev_stats.streaming_vbo_mem_use;
    stats->device.streaming_vbo_mem_alloc = dev_stats.streaming_vbo_mem_alloc;
    return true;
}
void TilemapWidget::Refresh()
{
    RefreshPreviews(false);
}

void TilemapWidget::on_mapName_textChanged()
{
    SetMapProperties();
}
void TilemapWidget::on_cmbPerspective_currentIndexChanged(int)
{
    SetMapProperties();
}

void TilemapWidget::on_mapTileSize_valueChanged(int)
{
    SetMapProperties();
}
void TilemapWidget::on_mapHeight_valueChanged(int)
{
    if (mState.klass->GetNumLayers())
    {
        SetEnabled(mUI.btnApplyMapSize, true);
    }
    else
    {
        mState.klass->SetMapHeight(GetValue(mUI.mapHeight));
    }
}
void TilemapWidget::on_mapWidth_valueChanged(int)
{
    if (mState.klass->GetNumLayers())
    {
        SetEnabled(mUI.btnApplyMapSize, true);
    }
    else
    {
        mState.klass->SetMapWidth(GetValue(mUI.mapWidth));
    }
}

void TilemapWidget::on_tileScaleX_valueChanged(double)
{
    mState.klass->SetTileRenderWidthScale(GetValue(mUI.tileScaleX));

    for (auto& widget : mPaletteMaterialWidgets)
    {
        widget->SetMaterialPreviewScale(GetMaterialPreviewScale(*mState.klass));
    }
}
void TilemapWidget::on_tileScaleY_valueChanged(double)
{
    mState.klass->SetTileRenderHeightScale(GetValue(mUI.tileScaleY));

    for (auto& widget : mPaletteMaterialWidgets)
    {
        widget->SetMaterialPreviewScale(GetMaterialPreviewScale(*mState.klass));
    }
}

void TilemapWidget::on_btnApplyMapSize_clicked()
{
    const unsigned new_map_width = GetValue(mUI.mapWidth);
    const unsigned new_map_height = GetValue(mUI.mapHeight);
    const unsigned old_map_width = mState.klass->GetMapWidth();
    const unsigned old_map_height = mState.klass->GetMapHeight();
    if ((new_map_width != old_map_width) ||
        (new_map_height != old_map_height))
    {
        mState.selection.Clear();

        mState.klass->SetMapWidth(new_map_width);
        mState.klass->SetMapHeight(new_map_height);

        for (size_t i=0; i<mState.klass->GetNumLayers(); ++i)
        {
            auto& layer = mState.klass->GetLayer(i);
            auto src_data = mLayerData[layer.GetId()];
            auto dst_data = std::make_shared<LayerData>();
            layer.Initialize(new_map_width, new_map_height, *dst_data);

            const game::USize src_size(old_map_width, old_map_height);
            const game::USize dst_size(new_map_width, new_map_height);
            layer.ResizeCopy(src_size, dst_size, *src_data, *dst_data);

            auto& instance = mState.map->GetLayer(i);
            instance.SetMapDimensions(new_map_width, new_map_height);
            instance.Load(dst_data);
            mLayerData[layer.GetId()] = dst_data;
        }
        DisplayLayerProperties();
        DisplayMapProperties();
    }
    SetEnabled(mUI.btnApplyMapSize, false);
}

void TilemapWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.mapName))
        return;

    size_t hash = 0;
    hash = mState.klass->GetHash();

    for (size_t i=0; i<mState.klass->GetNumLayers(); ++i)
    {
        auto& layer_class = mState.klass->GetLayer(i);
        auto& layer_inst  = mState.map->GetLayer(i);
        layer_inst.FlushCache();
        layer_inst.Save();

        const auto* data = base::SafeFind(mLayerData, layer_class.GetId());
        if (!(*data)->Save(*mState.klass, layer_class, *mState.workspace))
        {
            QMessageBox msg;
            msg.setText("There was an error saving a layer data file.");
            msg.setIcon(QMessageBox::Critical);
            msg.exec();
            return;
        }
        hash = base::hash_combine(hash, (*data)->GetHash());
    }
    app::TilemapResource resource(*mState.klass, GetValue(mUI.mapName));
    SetUserProperty(resource, "camera_offset_x", mState.camera_offset_x);
    SetUserProperty(resource, "camera_offset_y", mState.camera_offset_y);
    SetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    SetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    SetUserProperty(resource, "camera_rotation", mUI.rotation);
    SetUserProperty(resource, "widget", mUI.widget);
    SetUserProperty(resource, "show_render_layers", mUI.chkShowRenderLayers);
    SetUserProperty(resource, "show_data_layers", mUI.chkShowDataLayers);
    SetUserProperty(resource, "show_viewport", mUI.chkShowViewport);
    SetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    SetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    SetUserProperty(resource, "zoom", mUI.zoom);
    SetUserProperty(resource, "current_layer", GetCurrentRow(mUI.layers));
    SetUserProperty(resource, "main_splitter", mUI.mainSplitter);
    SetUserProperty(resource, "tile_tool_settings", mTileToolSettings.ToString());

    SetProperty(resource, "num_tools", mTools.size());
    for (size_t i=0; i<mTools.size(); ++i)
    {
        const auto& tool = mTools[i];
        QJsonObject json;
        ToolIntoJson(*tool, json);
        SetProperty(resource, PropertyKey("tool", i), json);
    }

    mState.workspace->SaveResource(resource);

    DisplayLayerProperties();
    mHash = hash;
}

void TilemapWidget::on_actionMoveLayerUp_triggered()
{
    const auto index = GetSelectedIndex(mUI.layers);
    if (index.isValid())
    {
        const auto row = index.row();
        if (row == 0)
            return;
        mState.klass->SwapLayers(row, row-1);
        mState.map->SwapLayers(row, row-1);
        mModel->Refresh();
        SelectRow(mUI.layers, row -1);
    }
}
void TilemapWidget::on_actionMoveLayerDown_triggered()
{
    const auto index = GetSelectedIndex(mUI.layers);
    if (index.isValid())
    {
        const auto row = index.row();
        if (row == mState.klass->GetNumLayers() -1)
            return;
        mState.klass->SwapLayers(row, row + 1);
        mState.map->SwapLayers(row, row + 1);
        mModel->Refresh();
        SelectRow(mUI.layers, row + 1);
    }
}

void TilemapWidget::on_actionNewLayer_triggered()
{
    on_btnNewLayer_clicked();
}
void TilemapWidget::on_actionEditLayer_triggered()
{
    on_btnEditLayer_clicked();
}
void TilemapWidget::on_actionDeleteLayer_triggered()
{
    on_btnDeleteLayer_clicked();
}

void TilemapWidget::on_btnEditBrushes_clicked()
{
    if (const auto* layer = GetCurrentLayer())
    {
        const auto& layerId = layer->GetId();

        std::vector<std::shared_ptr<Tool>> tools;
        for (const auto& tool : mTools)
        {
            if (tool->layer != layerId)
                continue;
            tools.push_back(tool);
        }
        DlgTileTool dlg(mState.workspace, mState.klass.get(), layer, this, &tools);
        if (dlg.exec() == QDialog::Rejected)
            return;

        for (auto& tool : tools)
        {
            tool->layer = layerId;
        }

        base::EraseRemove(mTools, [layerId](const auto& tool) {
            return tool->layer == layerId;
        });
        base::AppendVector(mTools, tools);

        UpdateToolToolbar();
    }
}

void TilemapWidget::on_btnNewLayer_clicked()
{
    const auto map_width  = mState.klass->GetMapWidth();
    const auto map_height = mState.klass->GetMapHeight();

    DlgLayer dlg(mState.workspace, this, map_width, map_height);
    if (dlg.exec() == QDialog::Rejected)
        return;

    auto layer_class = std::make_shared<game::TilemapLayerClass>();
    layer_class->SetName(dlg.GetName());
    layer_class->SetType(dlg.GetLayerType());
    layer_class->SetStorage(dlg.GetLayerStorage());
    layer_class->SetCache(dlg.GetLayerCache());
    layer_class->SetResolution(dlg.GetLayerResolution());
    layer_class->SetPaletteType(dlg.GetPaletteType());

    // set the default values. the default tile value is used when
    // there's no actual tile data available and new block of data
    // must be initialized.
    if (layer_class->HasRenderComponent())
    {
        const auto palette_index = dlg.GetTilePaletteIndex();
        if (palette_index == -1)
        {
            // the max palette index value indicates "no value set"
            const auto max_palette_index = layer_class->GetMaxPaletteIndex();
            layer_class->SetDefaultTilePaletteMaterialIndex(max_palette_index);
        }
        else
        {
            layer_class->SetDefaultTilePaletteMaterialIndex(palette_index);
        }
    }
    if (layer_class->HasDataComponent())
    {
        const auto data_value = dlg.GetTileDataValue();
        layer_class->SetDefaultTileDataValue(data_value);
    }
    GenerateLayerTools(*layer_class);

    mModel->AddLayer(layer_class);

    // create scratch layer data buffer for storing the edits
    auto data = std::make_shared<LayerData>();
    layer_class->Initialize(map_width, map_height, *data);

    mLayerData[layer_class->GetId()] = data;

    // create an instance of the layer for render visualization.
    auto layer_instance = game::CreateTilemapLayer(layer_class, map_width, map_height);
    layer_instance->Load(data);
    mState.map->AddLayer(std::move(layer_instance));

    SelectLastRow(mUI.layers);
    DisplayLayerProperties();
    DisplayMapProperties();
    UpdateToolToolbar();
}

void TilemapWidget::on_btnDeleteLayer_clicked()
{
    const auto& indices = GetSelection(mUI.layers);
    if (indices.isEmpty())
        return;

    const auto index  = indices[0].row();
    const auto& layer = mState.klass->GetLayer(index);
    const auto& data  = mLayerData[layer.GetId()];

    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Question);
    msg.setText("Are you sure you want to delete the layer?");
    msg.setWindowTitle(app::toString("Delete Layer %1", layer.GetName()));
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    if (msg.exec() == QMessageBox::Cancel)
        return;

    data->Delete(*mState.klass, layer, *mState.workspace);

    mModel->DeleteLayer(index);
    mState.map->DeleteLayer(index);
    mLayerData.erase(layer.GetId());

    ClearSelection(mUI.layers);
    DisplayLayerProperties();
}

void TilemapWidget::on_btnEditLayer_clicked()
{

}

void TilemapWidget::on_btnViewReset_clicked()
{
    mAnimator.Reset(mUI, mState);

    // the rest of the view properties are updated in Update since they're animated/interpolated
    SetValue(mUI.scaleX, 1.0f);
    SetValue(mUI.scaleY, 1.0f);
}

void TilemapWidget::on_btnViewMinus90_clicked()
{
    mAnimator.Minus90(mUI, mState);
}
void TilemapWidget::on_btnViewPlus90_clicked()
{
    mAnimator.Plus90(mUI, mState);
}

void TilemapWidget::on_btnMoreViewportSettings_clicked()
{
    const auto visible = mUI.transform->isVisible();
    SetVisible(mUI.transform, !visible);
    if (!visible)
        mUI.btnMoreViewportSettings->setArrowType(Qt::ArrowType::DownArrow);
    else mUI.btnMoreViewportSettings->setArrowType(Qt::ArrowType::UpArrow);
}

void TilemapWidget::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void TilemapWidget::on_layers_doubleClicked(const QModelIndex& index)
{
    const auto row = index.row();
    const auto col = index.column();
    if (col == 0)
    {
        auto& layer = mState.klass->GetLayer(row);
        const bool visible = layer.TestFlag(game::TilemapLayerClass::Flags::VisibleInEditor);
        layer.SetFlag(game::TilemapLayerClass::Flags::VisibleInEditor, !visible);
        DEBUG("Toggle layer visibility. [layer='%1']", layer.GetName());
        mModel->Refresh();

    }
}

void TilemapWidget::on_layerName_textChanged()
{
    ModifyCurrentLayer();
    mModel->Refresh();
}
void TilemapWidget::on_cmbLayerCache_currentIndexChanged(int)
{
    ModifyCurrentLayer();
}

void TilemapWidget::on_cmbPaletteType_currentIndexChanged(int)
{
    ModifyCurrentLayer();
}

void TilemapWidget::on_cmbTilemap_currentIndexChanged(int)
{
    auto* layer = GetCurrentLayer();
    if (layer == nullptr)
        return;

    const std::string old_tilemap_id = layer->GetTilemapMaterialId();
    const std::string new_tilemap_id = GetItemId(mUI.cmbTilemap);

    const auto tilemap_material = mState.workspace->GetMaterialClassById(new_tilemap_id);

    // hack
    auto* material_class = const_cast<gfx::MaterialClass*>(tilemap_material.get());
    const gfx::TilemapMaterialClass tilemap(*material_class);
    const unsigned tile_count = tilemap.GetTileCount();
    const unsigned max_palette_size = layer->GetMaxPaletteIndex();

    if (tile_count > max_palette_size)
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Icon::Question);
        msg.setText("The selected tilemap contains more tiles than the layer can support.\n"
            "Are you sure you want to continue?");
        msg.setWindowTitle("Change Tilemap Palette");
        if (msg.exec() == QMessageBox::No)
        {
            SetValue(mUI.cmbTilemap, ListItemId(old_tilemap_id));
            return;
        }
    }

    if (layer->GetCurrentPaletteSize() > 0)
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Icon::Question);
        msg.setText("Are you sure you want to change the tilemap?\n"
            "This will reset the current palette.");
        msg.setWindowTitle("Change Tilemap Palette");
        if (msg.exec() == QMessageBox::No)
        {
            SetValue(mUI.cmbTilemap, ListItemId(old_tilemap_id));
            return;
        }

        layer->ClearPalette();
    }

    ModifyCurrentLayer();

    for (auto* widget : mPaletteMaterialWidgets)
    {
        widget->ClearPreview();
        widget->SetDrawable("");
        widget->SetMaterial("");
        widget->SetOcclusion(game::TileOcclusion::None);
    }
    const auto max_count = std::min(tile_count, max_palette_size);

    for (size_t tile_index=0; tile_index<max_count; ++tile_index)
    {
        const auto palette_index = tile_index;
        layer->SetPaletteOcclusion(game::TileOcclusion::None, palette_index);
        layer->SetPaletteDrawableId("", palette_index);
        layer->SetPaletteMaterialTileIndex(tile_index, palette_index);
    }

    RefreshPreviews(false);
    UpdateLayerPaletteUsage();
}

void TilemapWidget::on_layerDepth_valueChanged(int)
{
    ModifyCurrentLayer();
}

void TilemapWidget::on_layerIndex_valueChanged(int)
{
    ModifyCurrentLayer();
}

void TilemapWidget::on_renderLayer_valueChanged(int)
{
    SetMapProperties();
}

void TilemapWidget::on_chkLayerVisible_stateChanged(int)
{
    ModifyCurrentLayer();
}
void TilemapWidget::on_chkLayerEnabled_stateChanged(int)
{
    ModifyCurrentLayer();
}
void TilemapWidget::on_chkLayerReadOnly_stateChanged(int)
{
    ModifyCurrentLayer();
}

void TilemapWidget::on_tilePaletteIndex_valueChanged(int)
{
    if (!mState.selection.HasSelection())
        return;

    auto* klass = GetCurrentLayer();
    auto* layer = GetCurrentLayerInstance();
    if (!klass || !layer || !layer->HasRenderComponent())
        return;

    const auto& selection = mState.selection;
    const auto palette_index = (int8_t)GetValue(mUI.tilePaletteIndex);

    for (unsigned i=0; i<selection.GetTileCount(); ++i)
    {
        const auto& tile = selection.GetTile(i);
        ASSERT(layer->SetTilePaletteIndex(palette_index, tile.y, tile.x));
    }

    InitializePaletteEntry(*klass, palette_index);
    DisplaySelection();
}

void TilemapWidget::SelectSelectedTileMaterial()
{
    if (!mState.selection.HasSelection())
        return;

    const auto& selection = mState.selection;

    auto* klass = GetCurrentLayer();
    auto* layer = GetCurrentLayerInstance();
    if (!klass || !layer || !layer->HasRenderComponent())
        return;

    const auto did_have_focus = mUI.widget->HasInputFocus();

    // try to figure a common palette index for the current selection.
    // if there are multiple tiles selected with different palette
    // indices then the palette index will be indeterminate.
    const uint8_t current_palette_index = [&selection, &layer, &klass]() {
        uint8_t current_palette_index = 0;
        const auto& tile_one = selection.GetTile(0);
        ASSERT(layer->GetTilePaletteIndex(&current_palette_index, tile_one.y, tile_one.x));

        for (size_t i = 1; i < selection.GetTileCount(); ++i)
        {
            const auto& tile_one = selection.GetTile(0);
            const auto& tile_two = selection.GetTile(i);

            uint8_t palette_index_one = 0;
            uint8_t palette_index_two = 0;
            ASSERT(layer->GetTilePaletteIndex(&palette_index_one, tile_one.y, tile_one.x));
            ASSERT(layer->GetTilePaletteIndex(&palette_index_two, tile_two.y, tile_two.x));
            if (palette_index_one != palette_index_two)
            {
                current_palette_index = klass->GetMaxPaletteIndex();
                return klass->GetMaxPaletteIndex();
            }
            current_palette_index = palette_index_one;
        }
        return static_cast<unsigned>(current_palette_index);
    }();

    const auto palette_type = layer->GetPaletteType();
    if (palette_type == game::TilemapLayer::PaletteType::MultiMaterial)
    {
        std::unordered_set<std::string> filter_set;
        for (const auto* widget : mPaletteMaterialWidgets)
        {
            const auto palette_index = widget->GetPaletteIndex();
            const auto materialId = widget->GetMaterialId();
            if (materialId.IsEmpty())
                continue;
            else if (!mState.workspace->IsValidMaterial(materialId))
                continue;
            filter_set.insert(materialId);
        }

        DlgMaterial dlg(this, mState.workspace, false);
        dlg.ShowTileIndex(false);
        dlg.SetPreviewScale(GetMaterialPreviewScale(*mState.klass));
        dlg.SetFilterSet(std::move(filter_set));
        if (current_palette_index != klass->GetMaxPaletteIndex())
            dlg.SetSelectedMaterialId(klass->GetPaletteMaterialId(current_palette_index));

        if (dlg.exec() == QDialog::Rejected)
        {
            if (did_have_focus)
                mUI.widget->SetFocus();
            return;
        }
        const auto& materialId = dlg.GetSelectedMaterialId();
        if (materialId.IsEmpty())
        {
            if (did_have_focus)
                mUI.widget->SetFocus();
            return;
        }

        unsigned palette_index = 0;
        for (const auto* widget : mPaletteMaterialWidgets)
        {
            if (widget->GetMaterialId() == materialId)
            {
                palette_index = widget->GetPaletteIndex();
                break;
            }
        }

        InitializePaletteEntry(*klass, palette_index);

        // assign the palette index to the selected tiles.
        for (unsigned i=0; i<selection.GetTileCount(); ++i)
        {
            const auto& tile = selection.GetTile(i);
            ASSERT(layer->SetTilePaletteIndex(palette_index, tile.y, tile.x));
        }
    }
    else if (palette_type == game::TilemapLayer::PaletteType::Tilemap)
    {
        const auto& materialId = layer->GetTilemapMaterialId();
        const auto tilemap = mState.workspace->FindMaterialClassById(materialId);
        if (!tilemap)
        {
            NOTE("Current tilemap is not a valid material.");
            WARN("Cannot open tile chooser since the current tilemap material is not a valid material.");
            return;
        }

        DlgTileChooser dlg(this, tilemap);
        if (current_palette_index != klass->GetMaxPaletteIndex())
            dlg.SetTileIndex(klass->GetPaletteMaterialTileIndex(current_palette_index));

        if (dlg.exec() == QDialog::Rejected)
        {
            if (did_have_focus)
                mUI.widget->SetFocus();
            return;
        }
        const auto tile_index = dlg.GetTileIndex();
        const auto palette_index = tile_index;
        InitializePaletteEntry(*klass, palette_index);

        // assign the palette index to the selected tiles.
        for (unsigned i=0; i<selection.GetTileCount(); ++i)
        {
            const auto& tile = selection.GetTile(i);
            ASSERT(layer->SetTilePaletteIndex(palette_index, tile.y, tile.x));
        }
    }

    DisplaySelection();

    if (did_have_focus)
    {
        QTimer::singleShot(0, this, [this]() {
            mUI.widget->SetFocus();
        });
    }
}

void TilemapWidget::DeleteSelectedTileMaterial()
{
    if (!mState.selection.HasSelection())
        return;

    const auto& selection = mState.selection;

    auto* klass = GetCurrentLayer();
    auto* layer = GetCurrentLayerInstance();
    if (!klass || !layer || !layer->HasRenderComponent())
        return;

    const auto nothing_index = layer->GetMaxPaletteIndex();

    for (unsigned i=0; i<selection.GetTileCount(); ++i)
    {
        const auto& tile = selection.GetTile(i);
        ASSERT(layer->SetTilePaletteIndex(nothing_index, tile.y, tile.x));
    }

    DisplaySelection();
}

void TilemapWidget::on_tileValue_valueChanged(int)
{
    if (!mState.selection.HasSelection())
        return;

    auto* klass = GetCurrentLayer();
    auto* layer = GetCurrentLayerInstance();
    if (!klass || !layer || !layer->HasDataComponent())
        return;

    const auto& selection = mState.selection;
    const auto tile_value = (int32_t)GetValue(mUI.tileValue);

    for (unsigned i=0; i<selection.GetTileCount(); ++i)
    {
        const auto& tile = selection.GetTile(i);
        ASSERT(layer->SetTileValue(tile_value, tile.y, tile.x));
    }
}

void TilemapWidget::on_layers_customContextMenuRequested(const QPoint& point)
{
    const auto index = GetSelectedIndex(mUI.layers);

    SetEnabled(mUI.actionDeleteLayer, false);
    SetEnabled(mUI.actionMoveLayerDown, false);
    SetEnabled(mUI.actionMoveLayerUp, false);
    SetEnabled(mUI.actionEditLayer, false); // not implemented yet.

    if (index.isValid())
    {
        const auto layers = mState.klass->GetNumLayers();

        SetEnabled(mUI.actionDeleteLayer, index.isValid());
        const auto row = index.row();
        if (row > 0)
            SetEnabled(mUI.actionMoveLayerUp, true);
        if (row < layers-1)
            SetEnabled(mUI.actionMoveLayerDown, true);
    }

    QMenu menu(this);
    menu.addAction(mUI.actionNewLayer);
    menu.addAction(mUI.actionEditLayer);
    menu.addSeparator();
    menu.addAction(mUI.actionMoveLayerUp);
    menu.addAction(mUI.actionMoveLayerDown);
    menu.addSeparator();
    menu.addAction(mUI.actionDeleteLayer);
    menu.exec(QCursor::pos());
}

void TilemapWidget::StartToolAction()
{
    auto* action = qobject_cast<QAction*>(sender());
    StartTool(action->data().toString());
}

void TilemapWidget::StopTool()
{
    if (mCurrentTool)
    {
        mCurrentTool.reset();
        DisplaySelection();
        UncheckTools();
    }
}

void TilemapWidget::StartTool(const QString& id)
{
    StopTool();

    size_t tool_index = 0;
    auto* tool  = GetTool(id, &tool_index);
    auto* layer_instance = GetCurrentLayerInstance();
    auto* layer_class = GetCurrentLayer();
    ASSERT(tool);
    ASSERT(layer_instance && layer_class);

    const auto tile_width  = mState.klass->GetTileWidth();
    const auto tile_height = mState.klass->GetTileHeight();
    const auto tile_scaler = layer_instance->GetTileSizeScaler();
    const auto layer_tile_width = tile_width * tile_scaler;
    const auto layer_tile_height = tile_height * tile_scaler;

    int tile_row = 0;
    int tile_col = 0;

    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto& mickey = mUI.widget->mapFromGlobal(QCursor::pos());
    if (mickey.x() >= 0 && mickey.x() < width &&
        mickey.y() >= 0 && mickey.y() < height)
    {
        const glm::vec2 tile_coord = MapWindowCoordinateToWorld(mUI, mState, mickey, mState.klass->GetPerspective());
        tile_col = tile_coord.x / layer_tile_width;
        tile_row = tile_coord.y / layer_tile_height;
    }

    if (tool->tool == ToolFunction::TileBrush)
    {
        const auto tile_tool = mTools[tool_index];
        const auto palette_type = layer_class->GetPaletteType();
        if (palette_type == game::TilemapPaletteType::Tilemap)
        {
            for (unsigned row=0; row<tile_tool->height; ++row)
            {
                for (unsigned col=0; col<tile_tool->width; ++col)
                {
                    const auto& tile = tile_tool->tiles[row * tile_tool->width + col];
                    const auto palette_index = tile.palette_index;
                    InitializePaletteEntry(*layer_class, palette_index);
                }
            }
        }

        auto mouse_tool = std::make_unique<TileBrushTool>(mTools[tool_index], mState, *layer_instance);
        mouse_tool->SetTileCol(tile_col);
        mouse_tool->SetTileRow(tile_row);
        mCurrentTool = std::move(mouse_tool);
    }

    auto actions = mBrushMenu->actions();
    for (auto* action : actions)
    {
        if (action->data().toString() == id)
        {
            action->setChecked(true);
        }
        else
        {
            action->setChecked(false);
        }
    }
}

void TilemapWidget::OnAddResource(const app::Resource* resource)
{
    if (resource->IsMaterial())
    {
        UpdateMaterialLists();
    }
    else if (resource->IsCustomShape())
    {
        const auto& shapes = mState.workspace->ListTileShapes();
        for (auto* widget : mPaletteMaterialWidgets)
            widget->UpdateDrawableList(shapes);
    }
}
void TilemapWidget::OnRemoveResource(const app::Resource* resource)
{
    if (resource->IsMaterial())
    {
        ReplaceDeletedResources();
        UpdateMaterialLists();

        mState.renderer.ClearPaintState();

        DisplayLayerProperties();

        for (const auto& tool : mTools)
        {
            for (auto& tile : tool->tiles)
            {
                if (tile.material_instance &&
                    tile.material_instance->GetClassId() == resource->GetId())
                    tile.material_instance.reset();
            }
        }
    }
    else if (resource->IsCustomShape())
    {
        ReplaceDeletedResources();

        const auto& shapes = mState.workspace->ListTileShapes();
        for (auto* widget : mPaletteMaterialWidgets)
            widget->UpdateDrawableList(shapes);

        mState.renderer.ClearPaintState();

        DisplayLayerProperties();
    }

    if (mCurrentTool)
    {
        if (!mCurrentTool->Validate())
        {
            StopTool();
        }
    }
}
void TilemapWidget::OnUpdateResource(const app::Resource* resource)
{
    if (resource->IsMaterial())
    {
        UpdateMaterialLists();

        mState.renderer.ClearPaintState();

        if (const auto* layer = GetCurrentLayer())
        {
            bool generate_previews = false;
            for (auto* widget : mPaletteMaterialWidgets)
            {
                const auto preview_id = widget->GetPreviewId();
                if (preview_id == resource->GetId())
                {
                    generate_previews = true;
                    break;
                }
            }
            if (generate_previews)
                RefreshPreviews(true);
        }

        for (const auto& tool : mTools)
        {
            for (auto& tile : tool->tiles)
            {
                if (tile.material_instance &&
                    tile.material_instance->GetClassId() == resource->GetId())
                    tile.material_instance.reset();
            }
        }
    }
    else if (resource->IsCustomShape())
    {
        const auto& shapes = mState.workspace->ListTileShapes();
        for (auto* widget : mPaletteMaterialWidgets)
            widget->UpdateDrawableList(shapes);

        mState.renderer.ClearPaintState();
    }

    if (mCurrentTool)
    {
        if (!mCurrentTool->Validate())
        {
            StopTool();
        }
    }
}

void TilemapWidget::LayerSelectionChanged(const QItemSelection&, const QItemSelection&)
{
    StopTool();

    mState.selection.Clear();

    DisplayLayerProperties();
    DisplaySelection();
    RefreshPreviews(false);
    UpdateToolToolbar();
}

void TilemapWidget::PaletteMaterialChanged(const PaletteMaterial* widget)
{
    if (auto* layer = GetCurrentLayer())
    {
        const auto palette_type = layer->GetPaletteType();

        const auto palette_index = widget->GetPaletteIndex();
        layer->SetPaletteDrawableId(widget->GetDrawableId(), palette_index);
        layer->SetPaletteOcclusion(widget->GetOcclusion(), palette_index);
        if (palette_type == game::TilemapPaletteType::MultiMaterial)
            layer->SetPaletteMaterialId(widget->GetMaterialId(), palette_index);
        else if (palette_type == game::TilemapPaletteType::Tilemap)
            layer->SetPaletteMaterialTileIndex(widget->GetTileIndex(), palette_index);

        if (palette_type == game::TilemapPaletteType::MultiMaterial)
        {
            const auto& materialId = widget->GetMaterialId();
            if (materialId.IsEmpty())
            {
                auto* inst = GetCurrentLayerInstance();
                bool palette_entry_used = false;
                for (unsigned row=0; row<inst->GetHeight(); ++row)
                {
                    for (unsigned col=0; col<inst->GetWidth(); ++col)
                    {
                        uint8_t tile_palette_index = 0;
                        ASSERT(inst->GetTilePaletteIndex(&tile_palette_index, row, col));
                        if (tile_palette_index == palette_index)
                        {
                            palette_entry_used = true;
                            break;
                        }
                    }
                }
                if (!palette_entry_used)
                {
                    layer->DeletePaletteIndex(palette_index);
                    UpdateLayerPaletteUsage();
                }
            }
        }

        UpdateLayerPaletteUsage();
    }
}

void TilemapWidget::DisplayCurrentCameraLocation()
{
    SetValue(mUI.translateX, -mState.camera_offset_x);
    SetValue(mUI.translateY, -mState.camera_offset_y);
}

void TilemapWidget::SetMapProperties()
{
    // map width/height are not set directly but only when
    // the size adjustment is done explicitly by the user.
    // this is because automatically adjusting the map
    // size on every UI change would result in a lot of
    // excess copying and also could lead to unwanted loss of
    // data when an intermediate map size is temporarily smaller
    // when for example the last digit is erased before typing
    // in the next replacement digit.

    mState.klass->SetName(GetValue(mUI.mapName));
    mState.klass->SetTileWidth(GetValue(mUI.mapTileSize));
    mState.klass->SetTileHeight(GetValue(mUI.mapTileSize));
    mState.klass->SetTileDepth(GetValue(mUI.mapTileSize));
    mState.klass->SetPerspective(GetValue(mUI.cmbPerspective));
    mState.klass->SetTileRenderWidthScale(GetValue(mUI.tileScaleX));
    mState.klass->SetTileRenderHeightScale(GetValue(mUI.tileScaleY));
    mState.klass->SetRenderLayer(GetValue(mUI.renderLayer));
}

void TilemapWidget::SetLayerProperties()
{
    if (auto* layer = GetCurrentLayer())
    {
        layer->SetCache(GetValue(mUI.cmbLayerCache));
    }
}

void TilemapWidget::DisplayMapProperties()
{
    unsigned total = 0;
    for (size_t i=0; i<mState.klass->GetNumLayers(); ++i)
    {
        const auto& layer_class = mState.klass->GetLayer(i);
        const auto& layer_inst  = mState.map->GetLayer(i);
        const auto& data = mLayerData[layer_class.GetId()];
        total += data->GetByteCount();
        total += layer_inst.GetByteCount();
    }

    SetValue(mUI.mapName,        mState.klass->GetName());
    SetValue(mUI.mapID,          mState.klass->GetId());
    SetValue(mUI.cmbPerspective, mState.klass->GetPerspective());
    SetValue(mUI.mapTileSize,    mState.klass->GetTileWidth());
    SetValue(mUI.mapWidth,       mState.klass->GetMapWidth());
    SetValue(mUI.mapHeight,      mState.klass->GetMapHeight());
    SetValue(mUI.tileScaleX,     mState.klass->GetTileRenderWidthScale());
    SetValue(mUI.tileScaleY,     mState.klass->GetTileRenderHeightScale());
    SetValue(mUI.renderLayer,    mState.klass->GetRenderLayer());
    SetValue(mUI.mapSize,        Bytes{total});
}

void TilemapWidget::DisplayLayerProperties()
{
    SetValue(mUI.layerName,       QString(""));
    SetValue(mUI.layerID,         QString(""));
    SetValue(mUI.layerFileName,   QString(""));
    SetValue(mUI.layerFileSize,   QString(""));
    SetValue(mUI.cmbLayerType,       -1);
    SetValue(mUI.cmbLayerStorage,    -1);
    SetValue(mUI.cmbLayerResolution, -1);
    SetValue(mUI.cmbPaletteType,     -1);
    SetValue(mUI.cmbLayerCache,      -1);
    SetValue(mUI.cmbPaletteType,     -1);
    SetValue(mUI.layerDepth,          0);
    SetValue(mUI.layerIndex,          0);
    SetValue(mUI.chkLayerVisible,   false);
    SetValue(mUI.chkLayerEnabled,   false);
    SetValue(mUI.chkLayerReadOnly,  false);
    SetEnabled(mUI.btnEditLayer,        false);
    SetEnabled(mUI.btnDeleteLayer,      false);
    SetEnabled(mUI.btnEditBrushes,      false);
    SetEnabled(mUI.layerProperties,     false);
    SetEnabled(mUI.layerPalette,        false);
    SetEnabled(mUI.cmbTilemap,          false);

    SetValue(mUI.paletteUsage, 0);
    SetEnabled(mUI.paletteUsage, false);

    if (const auto* layer = GetCurrentLayer())
    {
        if (layer->HasRenderComponent())
        {
            const auto palette_type = layer->GetPaletteType();
            if (palette_type == game::TilemapLayerClass::PaletteType::Tilemap)
            {
                SetEnabled(mUI.cmbTilemap, true);
                SetList(mUI.cmbTilemap, mState.workspace->ListTilemapMaterials());
                SetValue(mUI.cmbTilemap, ListItemId(layer->GetTilemapMaterialId()));
            }
        }

        const auto* inst = GetCurrentLayerInstance();

        SetValue(mUI.layerName,          layer->GetName());
        SetValue(mUI.layerID,            layer->GetId());
        SetValue(mUI.layerFileName,      layer->GetDataUri());
        SetValue(mUI.cmbLayerType,       layer->GetType());
        SetValue(mUI.cmbLayerStorage,    layer->GetStorage());
        SetValue(mUI.cmbLayerCache,      layer->GetCache());
        SetValue(mUI.cmbPaletteType,     layer->GetPaletteType());
        SetValue(mUI.cmbLayerResolution, layer->GetResolution());
        SetValue(mUI.layerDepth,         layer->GetDepth() * -1);
        SetValue(mUI.layerIndex,         layer->GetLayer());
        SetValue(mUI.chkLayerVisible,    layer->IsVisible());
        SetValue(mUI.chkLayerEnabled,    layer->IsEnabled());
        SetValue(mUI.chkLayerReadOnly,   layer->IsReadOnly());
        if (const auto* data = base::SafeFind(mLayerData, layer->GetId()))
        {
            quint64 bytes = 0;
            bytes += (*data)->GetByteCount();
            bytes += inst->GetByteCount();
            SetValue(mUI.layerFileSize, Bytes{ bytes });
        }

        UpdateLayerPalette();
        UpdateLayerPaletteUsage();

        SetEnabled(mUI.btnDeleteLayer,  true);
        SetEnabled(mUI.btnEditBrushes,  true);
        SetEnabled(mUI.layerProperties, true);
        SetEnabled(mUI.layerPalette,    true);
    }
    else
    {
        for (auto* widget : mPaletteMaterialWidgets)
        {
            widget->SetPaletteType(game::TilemapPaletteType::MultiMaterial);
            widget->ClearPreview();
            widget->ResetMaterial();
            widget->ResetDrawable();
            widget->setEnabled(false);
        }
        UpdateLayerPaletteUsage();
    }
}

void TilemapWidget::DisplaySelection()
{
    SetValue(mUI.tileValue, 0);
    SetValue(mUI.tilePaletteIndex, 0);
    SetEnabled(mUI.selection, false);
    SetEnabled(mUI.tileValue, false);
    SetEnabled(mUI.tilePaletteIndex, false);

    const auto& selection = mState.selection;
    if (!selection.HasSelection())
        return;

    const auto* layer = GetCurrentLayerInstance();
    if (!layer)
        return;

    const auto& tile = selection.GetTile(0);

    SetEnabled(mUI.selection, true);

    if (layer->HasRenderComponent())
    {
        SetEnabled(mUI.tilePaletteIndex, true);

        uint8_t palette_index = 0;
        ASSERT(layer->GetTilePaletteIndex(&palette_index, tile.y, tile.x));
        SetValue(mUI.tilePaletteIndex, palette_index);
    }
    if (layer->HasDataComponent())
    {
        SetEnabled(mUI.tileValue, true);

        int32_t tile_value = 0;
        ASSERT(layer->GetTileValue(&tile_value, tile.y, tile.x));
        SetValue(mUI.tileValue, tile_value);
    }
}

void TilemapWidget::UpdateLayerPalette()
{
    const auto* layer = GetCurrentLayer();
    if (!layer || !layer->HasRenderComponent())
    {
        SetValue(mUI.paletteUsage, 0);
        SetEnabled(mUI.paletteUsage, false);
        return;
    }

    for (auto* widget : mPaletteMaterialWidgets)
    {
        widget->setEnabled(false);
    }

    const auto type = layer->GetType();
    const auto palette_max = game::TilemapLayerClass::GetMaxPaletteIndex(type);
    const auto tilemap_material_id = layer->GetTilemapMaterialId();
    const auto palette_type = layer->GetPaletteType();

    for (unsigned i = 0; i < palette_max; ++i)
    {
        auto* widget = mPaletteMaterialWidgets[i];
        widget->setEnabled(true);
        widget->SetPaletteType(palette_type);
        widget->SetDrawable(layer->GetPaletteDrawableId(i));
        widget->SetTileIndex(i); //layer->GetPaletteMaterialTileIndex(i));
        widget->SetOcclusion(layer->GetPaletteOcclusion(i));

        if (palette_type == game::TilemapLayerClass::PaletteType::MultiMaterial)
        {
            widget->SetMaterial(layer->GetPaletteMaterialId(i));
        }
        else if (palette_type == game::TilemapLayerClass::PaletteType::Tilemap)
        {
            widget->SetTilemapMaterialId(tilemap_material_id);
        }
    }
}

void TilemapWidget::UpdateLayerPaletteUsage()
{
    const auto* layer = GetCurrentLayer();
    if (!layer || !layer->HasRenderComponent())
    {
        SetValue(mUI.paletteUsage, 0);
        SetEnabled(mUI.paletteUsage, false);
        return;
    }

    const auto palette_max = layer->GetMaxPaletteIndex();
    const auto palette_size = layer->GetCurrentPaletteSize();
    SetMinMax(mUI.paletteUsage, 0, palette_max);
    SetValue(mUI.paletteUsage, palette_size);
    SetEnabled(mUI.paletteUsage, true);
}

void TilemapWidget::PaintScene(gfx::Painter& painter, double sec)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);
    const auto xs     = (float)GetValue(mUI.scaleX);
    const auto ys     = (float)GetValue(mUI.scaleY);
    const auto grid   = (GridDensity)GetValue(mUI.cmbGrid);

    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    const auto map_view = mState.klass->GetPerspective();
    // Create tile painter for drawing in tile coordinate space.
    gfx::Painter tile_painter(painter.GetDevice());
    tile_painter.SetViewMatrix(CreateViewMatrix(mUI, mState, map_view));
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

    // validation
    {
        struct ValidationMessage {
            std::string message;
            unsigned priority = 0;
        };
        std::vector<ValidationMessage> msgs;

        for (size_t i=0; i<mState.klass->GetNumLayers(); ++i)
        {
            const auto& layer = mState.klass->GetLayer(i);
            if (!layer.HasRenderComponent())
                continue;

            const auto palette_type = layer.GetPaletteType();

            if (palette_type == game::TilemapPaletteType::Tilemap)
            {
                const auto materialId = layer.GetTilemapMaterialId();
                if (materialId.empty())
                {
                    ValidationMessage msg;
                    msg.message = base::FormatString("Layer '%1' has no tilemap material selected.", layer.GetName());
                    msgs.push_back(std::move(msg));
                }
                else if (!mState.workspace->IsValidMaterial(materialId))
                {
                    ValidationMessage msg;
                    msg.message = base::FormatString("Layer '%1' has invalid tilemap material.", layer.GetName());
                    msgs.push_back(std::move(msg));
                }
                else
                {
                    const auto klass = mState.workspace->GetMaterialClassById(materialId);
                    if (!klass->IsTilemap())
                    {
                        ValidationMessage msg;
                        msg.message = base::FormatString("Layer '%1' has invalid tilemap material.", layer.GetName());
                        msgs.push_back(std::move(msg));
                    }
                    else
                    {
                        if (klass->GetNumTextureMaps() == 0)
                        {
                            ValidationMessage msg;
                            msg.message = base::FormatString("Layer '%1' tilemap material has no texture maps.", layer.GetName());
                            msgs.push_back(std::move(msg));
                        }
                        else
                        {
                            for (unsigned i=0; i<klass->GetNumTextureMaps(); ++i)
                            {
                                const auto* map = klass->GetTextureMap(i);
                                if (map->GetNumTextures() == 0)
                                {
                                    ValidationMessage msg;
                                    msg.message = base::FormatString("Layer '%1' tilemap material texture map '%2' is missing texture.",
                                        layer.GetName(), map->GetName());
                                    msgs.push_back(std::move(msg));
                                }
                            }
                        }
                        const auto active_texture_map_id = klass->GetActiveTextureMap();
                        if (!active_texture_map_id.empty() && klass->FindTextureMapById(active_texture_map_id) == nullptr)
                        {
                            ValidationMessage msg;
                            msg.message = base::FormatString("Layer '%1' tilemap material has invalid active texture map.",
                                layer.GetName());
                            msgs.push_back(std::move(msg));
                        }
                    }
                }
            }
            else if (palette_type == game::TilemapPaletteType::MultiMaterial)
            {
                const auto palette_size = layer.GetCurrentPaletteSize();
                if (palette_size == 0)
                {
                    ValidationMessage msg;
                    msg.priority = 2;
                    msg.message = base::FormatString("Layer %1 has no materials in palette", layer.GetName());
                    msgs.push_back(std::move(msg));
                }
            }
        }
        float xpos = 10.0f;
        float ypos = 10.0f;
        for (const auto& msg : msgs)
        {
            if (msg.priority == 0)
                ShowError(msg.message, QPointF(xpos, ypos), painter);
            else if (msg.priority == 1)
                ShowWarning(msg.message, QPointF(xpos, ypos), painter);
            else if (msg.priority == 2)
                ShowMessage(msg.message, QPointF(xpos, ypos), painter);
            ypos += 20.0f;
        }
        for (const auto& msg : msgs)
        {
            if (msg.priority == 0)
                return;
        }
    }

    // render the actual tilemap
    {
        auto* device = painter.GetDevice();

        const auto camera_position = glm::vec2{mState.camera_offset_x, mState.camera_offset_y};
        const auto camera_scale    = glm::vec2{xs, ys};
        const auto camera_rotation = (float)GetValue(mUI.rotation);

        LowLevelRenderHook low_level_render_hook(
                camera_position,
                camera_scale,
                map_view,
                camera_rotation,
                width, height,
                zoom,
                grid,
                GetValue(mUI.chkShowGrid));

        class DrawHook : public engine::LowLevelRendererHook, public engine::PacketFilter
        {
        public:
            DrawHook(const State& state, const game::TilemapLayer* layer, const gfx::Painter& painter, LowLevelRenderHook* hook, MouseTool* tool)
              : mState(state)
              , mLayer(layer)
              , mWindowPainter(painter)
              , mLowLevelRenderHook(hook)
              , mCurrentTool(tool)
            {}
            void BeginDraw(const RenderSettings& settings, const GPUResources& gpu) override
            {
                if (mLowLevelRenderHook->DrawGrid())
                    mLowLevelRenderHook->DrawGrid(settings, gpu);

                if (!mLayer)
                    return;

                // visualize the selected tiles in the grid level
                const auto tile_width  = mState.klass->GetTileWidth();
                const auto tile_height = mState.klass->GetTileHeight();
                const auto tile_depth  = mState.klass->GetTileDepth();
                const auto tile_scaler = mLayer->GetTileSizeScaler();
                const auto layer_tile_width  = tile_width * tile_scaler;
                const auto layer_tile_height = tile_height * tile_scaler;

                auto tile_painter = mLowLevelRenderHook->CreateTilePainter(gpu);

                // visualize the currently selected tiles
                const auto& selection = mState.selection;
                for (unsigned i=0; i<selection.GetTileCount(); ++i)
                {
                    const auto& tile = selection.GetTile(i);
                    gfx::Transform model;
                    model.Scale(layer_tile_width, layer_tile_height);
                    model.MoveTo(layer_tile_width * tile.x, layer_tile_height * tile.y);
                    tile_painter.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Solid), model,
                                      gfx::CreateMaterialFromColor(gfx::Color4f(gfx::Color::LightGray, 0.4f)));
                }

                // visualize the tiles currently being selected
                if (const auto* select_tool = dynamic_cast<const TileSelectTool*>(mCurrentTool))
                {
                    const auto& selection = select_tool->GetSelection();
                    if (selection.IsEmpty())
                        return;
                    for (unsigned i=0; i<selection.GetTileCount(); ++i)
                    {
                        const auto& tile = selection.GetTile(i);
                        gfx::Transform model;
                        model.Scale(layer_tile_width, layer_tile_height);
                        model.MoveTo(layer_tile_width * tile.x, layer_tile_height * tile.y);
                        tile_painter.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Solid), model,
                                          gfx::CreateMaterialFromColor(gfx::Color4f(gfx::Color::LightGray, 0.4f)));
                    }
                }
            }

            void EndDrawPacket(const RenderSettings& settings, const GPUResources& gpu, const engine::DrawPacket& packet, gfx::Painter& painter) override
            {
                if (!mState.selection.HasSelection() || !mLayer)
                    return;

                const auto* batch = dynamic_cast<const gfx::TileBatch*>(packet.drawable.get());

                const auto layer_index = mState.map->FindLayerIndex(mLayer);
                if (packet.map_layer != layer_index)
                    return;

                const auto perspective = mState.map->GetPerspective();
                const auto& selection = mState.selection;

                const auto tile_width  = mState.klass->GetTileWidth();
                const auto tile_height = mState.klass->GetTileHeight();
                const auto tile_depth  = mState.klass->GetTileDepth();
                const auto tile_scaler = mLayer->GetTileSizeScaler();
                const auto layer_tile_width  = tile_width * tile_scaler;
                const auto layer_tile_height = tile_height * tile_scaler;
                const auto layer_tile_depth  = tile_depth * tile_scaler;

                for (size_t i=0; i<batch->GetNumTiles(); ++i)
                {
                    const auto& tile = batch->GetTile(i);

                    if (!selection.IsSelected(tile.pos.x, tile.pos.y))
                        continue;

                    // indicates data layer packet
                    if (packet.domain == engine::DrawPacket::Domain::Editor)
                    {
                        gfx::Transform model;
                        model.Scale(layer_tile_width, layer_tile_height);
                        model.MoveTo(layer_tile_width * tile.pos.x, layer_tile_height * tile.pos.y);
                        painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Outline), model,
                                     gfx::CreateMaterialFromColor(gfx::Color::HotPink), 2.0f);
                    }
                    else if (packet.domain == engine::DrawPacket::Domain::Scene)
                    {
                        gfx::TileBatch tiles;
                        tiles.AddTile(tile);
                        tiles.SetTileWorldSize(batch->GetTileWorldSize());
                        tiles.SetTileRenderSize(batch->GetTileRenderSize());
                        tiles.SetTileShape(gfx::TileBatch::TileShape::Rectangle);
                        if (perspective == game::Tilemap::Perspective::AxisAligned)
                            tiles.SetProjection(gfx::TileBatch::Projection::AxisAligned);
                        else if (perspective == game::Tilemap::Perspective::Dimetric)
                            tiles.SetProjection(gfx::TileBatch::Projection::Dimetric);
                        else if (perspective == game::Tilemap::Perspective::Isometric)
                            tiles.SetProjection(gfx::TileBatch::Projection::Isometric);
                        else BUG("unknown projection");

                        //DrawEdges(painter, mWindowPainter, tiles, *packet.material, packet.transform, mState.klass->GetId());
                    }
                }
            }
            bool InspectPacket(engine::DrawPacket& packet) override
            {
                if (!mLayer)
                    return true;

                const auto& selection = mState.selection;

                const auto& packet_layer = mState.map->GetLayer(packet.map_layer);
                if (packet_layer->GetResolution() != selection.GetResolution())
                    return true;

                const auto* tile_batch = dynamic_cast<const gfx::TileBatch*>(packet.drawable.get());
                const auto layer_index = mState.map->FindLayerIndex(mLayer);


                TileSelection temp;

                if (const auto* select_tool = dynamic_cast<const TileSelectTool*>(mCurrentTool))
                {
                    temp = select_tool->GetSelection();
                    ASSERT(temp.GetResolution() == selection.GetResolution());
                    ASSERT(temp.GetResolution() == packet_layer->GetResolution());
                }

                for (size_t i=0; i<tile_batch->GetNumTiles(); ++i)
                {
                    const auto& tile = tile_batch->GetTile(i);

                    const auto selected_tile= selection.IsSelected(tile.pos.x, tile.pos.y) ||
                                             temp.IsSelected(tile.pos.x, tile.pos.y);
                    if (!selected_tile)
                        continue;

                    if (packet.domain == engine::DrawPacket::Domain::Scene)
                    {
                        auto material = packet.material->Clone();

                        // the idea here is to visually indicate not only the tile that is
                        // the current layer but also the tile that is not in the current layer.
                        // we could use different colors here but visually using a single
                        // color seems more appealing. Going to leave that layer check here
                        // for posterity however.
                        if (int(tile.pos.z) == mLayer->GetDepth())
                        {
                            material->SetUniform<gfx::kBaseColor>(gfx::Color::HotPink);
                        }
                        else
                        {
                            material->SetUniform<gfx::kBaseColor>(gfx::Color::LightGray);
                        }

                        packet.material = std::move(material);
                    }
                }

                return true;
            }
        private:
            const State& mState;
            const game::TilemapLayer* mLayer = nullptr;
            const gfx::Painter& mWindowPainter;
            const LowLevelRenderHook* mLowLevelRenderHook = nullptr;
            const MouseTool* mCurrentTool = nullptr;
        } hook(mState, GetCurrentLayerInstance(), painter, &low_level_render_hook, mCurrentTool.get());

        engine::Renderer::Camera camera;
        camera.clear_color     = mUI.widget->GetCurrentClearColor();
        camera.position        = camera_position;
        camera.scale           = camera_scale * zoom;
        camera.rotation        = camera_rotation;
        camera.viewport        = game::FRect(-width*0.5f, -height*0.5f, width, height);
        camera.map_perspective = map_view;
        mState.renderer.SetCamera(camera);

        engine::Renderer::Surface surface;
        surface.viewport = gfx::IRect(0, 0, width, height);
        surface.size     = gfx::USize(width, height);
        mState.renderer.SetSurface(surface);

        mState.renderer.SetLowLevelRendererHook(&hook);
        mState.renderer.SetPacketFilter(&hook);
        mState.renderer.SetEditingMode(true);
        mState.renderer.SetName("TilemapWidgetRenderer/" + mState.klass->GetId());
        mState.renderer.SetClassLibrary(mState.workspace);

        const bool show_render_layers = GetValue(mUI.chkShowRenderLayers);
        const bool show_data_layers = GetValue(mUI.chkShowDataLayers);

        mState.renderer.BeginFrame();
        mState.renderer.CreateFrame(*mState.map, show_render_layers, show_data_layers);
        mState.renderer.DrawFrame(*device);
        mState.renderer.EndFrame();
    }

    // render assistance when a layer is selected.
    if (const auto* layer = GetCurrentLayerInstance())
    {
        //const auto layer_index = GetCurrentLayerIndex();
        const auto tile_width  = mState.klass->GetTileWidth();
        const auto tile_height = mState.klass->GetTileHeight();
        const auto tile_depth  = mState.klass->GetTileDepth();
        const auto tile_scaler = layer->GetTileSizeScaler();
        const auto layer_tile_width  = tile_width * tile_scaler;
        const auto layer_tile_height = tile_height * tile_scaler;
        const auto layer_tile_depth  = tile_depth * tile_scaler;
        const auto layer_tile_size   = glm::vec3{layer_tile_width, layer_tile_height, layer_tile_depth};

        // draw the map boundary
        {
            const auto map_width = mState.klass->GetMapWidth();
            const auto map_height = mState.klass->GetMapHeight();
            const auto map_width_tiles = map_width * mState.klass->GetTileWidth();
            const auto map_height_tiles = map_height * mState.klass->GetTileHeight();
            gfx::Transform model;
            model.Scale(map_width_tiles, map_height_tiles);
            model.MoveTo(0.0f, 0.0f);
            tile_painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Outline), model,
                              gfx::CreateMaterialFromColor(gfx::Color::DarkYellow));
        }


        // visualize the tile under the mouse pointer.
        if (const auto& tile = FindTileUnderMouse(); tile.has_value() && !mCurrentTool)
        {
            const auto& val = tile.value();
            const auto tile_col = val.col;
            const auto tile_row = val.row;
            gfx::Transform model;
            model.Scale(layer_tile_width, layer_tile_height);
            model.MoveTo(layer_tile_width * tile_col, layer_tile_height * tile_row);

            gfx::Color4f color;

            if (mState.selection.IsSelected(tile->col, tile->row))
                color = gfx::Color4f(gfx::Color::HotPink) * 0.75f +
                        gfx::Color4f(gfx::Color::Green) * 0.25f;
            else color = gfx::Color4f(gfx::Color::HotPink);

            tile_painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Outline), model,
                              gfx::CreateMaterialFromColor(color), 2.0f);

            const glm::vec2 corner = engine::ProjectPoint(tile_painter.GetProjMatrix(),
                                                     tile_painter.GetViewMatrix(),
                                                     painter.GetProjMatrix(),
                                                     painter.GetViewMatrix(),
                                                     glm::vec3 { layer_tile_width * (tile_col+1),
                                                                 layer_tile_height * (tile_row+1), 0.0f });

            ShowMessage(base::FormatString("%1, %2", tile_row, tile_col), corner + glm::vec2{10.0f, 10.0f}, painter);
        }
    }

    if (mCurrentTool)
    {
        mCurrentTool->Render(scene_painter, tile_painter);
    }

    // a test case for mapping the corners of the tilemap from the isometric space
    // into our 2D axis aligned space. this is here simply because of convenience.
    if (Editor::DebugEditor())
    {
        const auto tile_width  = mState.klass->GetTileWidth();
        const auto tile_height = mState.klass->GetTileHeight();
        const auto tile_depth  = mState.klass->GetTileDepth();
        const auto map_width = mState.klass->GetMapWidth();
        const auto map_height = mState.klass->GetMapHeight();

        std::vector<glm::vec3> points;
        points.push_back({0.0f, 0.0f, 0.0f});
        points.push_back({tile_width*map_width, 0.0f, 0.0f});
        points.push_back({tile_width*map_width, tile_height*map_height, 0.0f});
        points.push_back({0.0f, tile_height*map_height, 0.0f});

        points.push_back({0.0f, 0.0f, tile_depth*-1.0f});
        points.push_back({tile_width*map_width, 0.0f, tile_depth*-1.0f});
        points.push_back({tile_width*map_width, tile_height*map_height, tile_depth*-1.0f});
        points.push_back({0.0f, tile_height*map_height, tile_depth*-1.0f});

        {
            const auto base_size = glm::vec3{tile_width, tile_height, tile_depth};

            auto checkerboard = mState.workspace->GetMaterialClassById("_checkerboard");
            gfx::Transform model;
            model.Translate(0.5f, 0.5f, -0.5f);
            model.Scale(base_size);
            tile_painter.Draw(gfx::Cube(), model, gfx::MaterialInstance(checkerboard));
        }


        for (const auto& point : points)
        {
            // tile plane -> painter coordinate space projection.
            {

                const auto& p = engine::ProjectPoint(tile_painter.GetProjMatrix(),
                                                     tile_painter.GetViewMatrix(),
                                                     painter.GetProjMatrix(),
                                                     painter.GetViewMatrix(),
                                                     point);
                gfx::Transform model;
                model.Scale(10.0f, 10.0f);
                model.Translate(p.x, p.y);
                model.Translate(-5.0f, -5.0f);
                //painter.Draw(gfx::Circle(), model, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
            }

            {
                const auto& p = engine::MapFromGamePlaneToViewPlane(glm::vec4{point.x, point.y, point.z, 1.0f}, mState.klass->GetPerspective());
                gfx::Transform model;
                model.Scale(10.0f, 10.0f);
                model.Translate(p.x, p.y);
                model.Translate(-5.0f, -5.0f);
                scene_painter.Draw(gfx::Circle(), model, gfx::CreateMaterialFromColor(gfx::Color::Green));
            }
        }
    }

    if (!mState.klass->GetNumLayers())
    {
        ShowInstruction(R"(
Create a new map with render and data layers.

INSTRUCTIONS
1. Adjust the tile size, map width and height on the left.
2. Add a new layer on the right under Layers, New Layer...
3. Select layer type as 'Render_xxx' for a visual layer.
4. Select layer type as 'Data_xxx' for a logical data layer.

Hit 'Save' to save the map.
        )",
        gfx::FRect(0, 0, width, height),
        painter, 28);
        return;
    }

    // draw the origin vectors in tilespace and project with the map perspective
    if (GetValue(mUI.chkShowOrigin))
    {
        gfx::Transform model_to_world;
        DrawBasisVectors(tile_painter, model_to_world);
    }

    if (GetValue(mUI.chkShowViewport))
    {
        gfx::Transform view;
        MakeViewTransform(mUI, mState, view);
        const auto& settings    = mState.workspace->GetProjectSettings();
        const float game_width  = settings.viewport_width;
        const float game_height = settings.viewport_height;
        DrawViewport(painter, view, game_width, game_height, width, height);
    }

    PrintMousePos(mUI, mState, painter, map_view);
}

void TilemapWidget::MouseMove(QMouseEvent* event)
{
    if (!mCurrentTool && !mCameraTool)
        return;

    const MouseEvent mickey(event, mUI, mState, mState.klass->GetPerspective());

    if (mCameraTool)
    {
        mCameraTool->MouseMove(mickey);
        DisplayCurrentCameraLocation();
    }

    if (mCurrentTool)
        mCurrentTool->MouseMove(mickey);
}

void TilemapWidget::MousePress(QMouseEvent* event)
{
    const MouseEvent mickey(event, mUI, mState, mState.klass->GetPerspective());

    const auto& mods = event->modifiers();
    const auto shift = mods & Qt::ShiftModifier;

    if (!mCurrentTool && mickey->button() == Qt::LeftButton)
    {
        if (auto* layer = GetCurrentLayerInstance())
        {
            const auto& layer_resolution = layer->GetClass().GetResolution();

            if (mState.selection.HasSelection() &&
                mState.selection.GetResolution() == layer_resolution && layer->HasRenderComponent() && !shift)
            {
                const auto& selection = mState.selection;

                if (const auto& tile = FindTileUnderMouse())
                {
                    if (selection.IsSelected(tile->col, tile->row))
                    {
                        mCurrentTool.reset(new TileMoveTool(mState, *layer));
                    }
                }
            }

            if (!mCurrentTool)
            {
                mCurrentTool.reset(new TileSelectTool(*mState.map, *layer, mState));
            }
        }
    }
    else if(!mCameraTool &&  (mickey->button() == Qt::RightButton))
    {
        mCameraTool.reset(new PerspectiveCorrectCameraTool(mUI, mState));
    }

    if (mCurrentTool && mickey->button() == Qt::LeftButton)
    {
        mCurrentTool->MousePress(mickey);
    }

    if (mCameraTool && mickey->button() == Qt::RightButton)
    {
        mCameraTool->MousePress(mickey);
    }
}
void TilemapWidget::MouseRelease(QMouseEvent* event)
{
    if (!mCurrentTool && !mCameraTool)
        return;

    const MouseEvent mickey(event, mUI, mState, mState.klass->GetPerspective());

    if (mickey->button() == Qt::LeftButton)
    {
        if (mCurrentTool->MouseRelease(mickey))
        {
            StopTool();
        }
    }
    else if (mickey->button() == Qt::RightButton)
    {
        if (mCameraTool->MouseRelease(mickey))
        {
            mCameraTool.reset();
        }
    }
}
void TilemapWidget::MouseDoubleClick(QMouseEvent* mickey)
{
    MousePress(mickey);
    MouseRelease(mickey);

    SelectSelectedTileMaterial();
}

void TilemapWidget::MouseWheel(QWheelEvent* wheel)
{

}

void TilemapWidget::MouseZoom(const std::function<void()>& zoom_function)
{
    // where's the mouse in the widget
    const auto& mickey = mUI.widget->mapFromGlobal(QCursor::pos());
    // can't use underMouse here because of the way the gfx widget
    // is constructed i.e QWindow and Widget as container
    if (mickey.x() < 0 || mickey.y() < 0 ||
        mickey.x() > mUI.widget->width() ||
        mickey.y() > mUI.widget->height())
        return;

    glm::vec2 mickey_pos_in_world_before_zoom;
    glm::vec2 mickey_pos_in_world_after_zoom;

    mickey_pos_in_world_before_zoom = MapWindowCoordinateToWorld(mUI, mState, mickey);

    zoom_function();

    mickey_pos_in_world_after_zoom = MapWindowCoordinateToWorld(mUI, mState, mickey);

    const auto camera_diff = mickey_pos_in_world_after_zoom - mickey_pos_in_world_before_zoom;

    mState.camera_offset_x -= camera_diff.x;
    mState.camera_offset_y -= camera_diff.y;
    DisplayCurrentCameraLocation();
}

bool TilemapWidget::KeyPress(QKeyEvent* key)
{
    int selection_dx = 0;
    int selection_dy = 0;

    if (key->key() == Qt::Key_Escape)
    {
        OnEscape();
        return true;
    }
    else if (key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace)
    {
        DeleteSelectedTileMaterial();
        return true;
    }
    else if (key->key() == Qt::Key_Space)
    {
        SelectSelectedTileMaterial();
        return true;
    }
    else if (key->key() == Qt::Key_1)
        return SelectLayerOnKey(0);
    else if (key->key() == Qt::Key_2)
        return SelectLayerOnKey(1);
    else if (key->key() == Qt::Key_3)
        return SelectLayerOnKey(2);
    else if (key->key() == Qt::Key_4)
        return SelectLayerOnKey(3);
    else if (key->key() == Qt::Key_5)
        return SelectLayerOnKey(4);
    else if (key->key() == Qt::Key_6)
        return SelectLayerOnKey(5);
    else if (key->key() == Qt::Key_7)
        return SelectLayerOnKey(6);
    else if (key->key() == Qt::Key_8)
        return SelectLayerOnKey(7);
    else if (key->key() == Qt::Key_9)
        return SelectLayerOnKey(8);
    else if (key->key() == Qt::Key_Up)
        selection_dy = -1;
    else if (key->key() == Qt::Key_Down)
        selection_dy = 1;
    else if (key->key() == Qt::Key_Left)
        selection_dx = -1;
    else if (key->key() == Qt::Key_Right)
        selection_dx = 1;
    else return false;

    const auto* layer = GetCurrentLayer();
    const auto num_cols = layer->MapDimension(mState.klass->GetMapWidth());
    const auto num_rows = layer->MapDimension(mState.klass->GetMapHeight());
    if (mState.selection.ShiftSelection(selection_dx, selection_dy, num_cols, num_rows))
        DisplaySelection();

    return true;
}

game::TilemapLayerClass* TilemapWidget::GetCurrentLayer()
{
    const auto& indices = GetSelection(mUI.layers);
    if (indices.isEmpty())
        return nullptr;

    return &mState.klass->GetLayer(indices[0].row());
}
game::TilemapLayer* TilemapWidget::GetCurrentLayerInstance()
{
    if (auto* klass = GetCurrentLayer())
    {
        return mState.map->FindLayerByClassId(klass->GetId());
    }
    return nullptr;
}

const game::TilemapLayerClass* TilemapWidget::GetCurrentLayer() const
{
    const auto& indices = GetSelection(mUI.layers);
    if (indices.isEmpty())
        return nullptr;

    return &mState.klass->GetLayer(indices[0].row());
}
const game::TilemapLayer* TilemapWidget::GetCurrentLayerInstance() const
{
    if (const auto* klass = GetCurrentLayer())
    {
        return mState.map->FindLayerByClassId(klass->GetId());
    }
    return nullptr;
}

size_t TilemapWidget::GetCurrentLayerIndex() const
{
    const auto& indices = GetSelection(mUI.layers);
    ASSERT(!indices.isEmpty());
    return indices[0].row();
}

void TilemapWidget::GenerateLayerTools(const game::TilemapLayerClass& klass)
{
    DEBUG("Generating default tools for layer. [layer='%1']", klass.GetName());
    {
        auto tool = std::make_shared<Tool>();
        tool->id     = base::RandomString(10);
        tool->tool   = ToolFunction::TileBrush;
        tool->shape  = ToolShape::Rectangle;
        tool->name   = "Stamp";
        tool->width  = 1;
        tool->height = 1;
        tool->layer = klass.GetId();

        TileTool::Tile tile;
        tile.data_value     = 0;
        tile.palette_index  = 0;
        tile.apply_material = true;
        tile.apply_value    = false;
        tool->tiles.push_back(std::move(tile));

        mTools.push_back(tool);
    }
}

void TilemapWidget::UpdateToolToolbar()
{
    mBrushMenu->clear();

    const auto* layer = GetCurrentLayer();
    if (layer == nullptr)
        return;

    unsigned shortcut_key_index = 0;

    for (const auto& tool : mTools)
    {
        if (tool->layer != layer->GetId())
            continue;

        auto* action = new QAction(this);
        action->setText(app::FromUtf8(tool->name));
        action->setData(app::FromUtf8(tool->id));
        action->setCheckable(true);
        action->setChecked(false);
        action->setEnabled(layer != nullptr);
        if (shortcut_key_index < 9)
        {
            action->setShortcut(QKeySequence(Qt::CTRL | (Qt::Key_1 + shortcut_key_index)));
            shortcut_key_index++;
        }

        if (tool->tool == ToolFunction::TileBrush)
            action->setIcon(QIcon("level:brush.png"));

        connect(action, &QAction::triggered,this, &TilemapWidget::StartToolAction);
        mBrushMenu->addAction(action);
    }
}

void TilemapWidget::UncheckTools()
{
    auto actions = mBrushMenu->actions();
    for (auto* action : actions)
        action->setChecked(false);
}

TileTool* TilemapWidget::GetTool(const QString& id, size_t* index)
{
    size_t counter = 0;
    if (index == nullptr)
        index = &counter;

    for (*index=0; *index<mTools.size(); ++*index)
    {
        if (mTools[*index]->id == app::ToUtf8(id))
            return mTools[*index].get();
    }
    BUG("No such tool was found.");
    return nullptr;
}

void TilemapWidget::ModifyCurrentLayer()
{
    if (auto* layer = GetCurrentLayer())
    {
        layer->SetName(GetValue(mUI.layerName));
        layer->SetCache(GetValue(mUI.cmbLayerCache));
        layer->SetVisible(GetValue(mUI.chkLayerVisible));
        layer->SetEnabled(GetValue(mUI.chkLayerEnabled));
        layer->SetReadOnly(GetValue(mUI.chkLayerReadOnly));
        layer->SetDepth(GetValue(mUI.layerDepth)*-1);
        layer->SetLayer(GetValue(mUI.layerIndex));
        layer->SetPaletteType(GetValue(mUI.cmbPaletteType));
        layer->SetTilemapMaterialId(GetItemId(mUI.cmbTilemap));

        auto* instance = GetCurrentLayerInstance();
        instance->SetFlags(layer->GetFlags());
        instance->FlushCache();
        instance->Save();
        instance->Load(mLayerData[layer->GetId()]);
    }
}

bool TilemapWidget::ValidateToolAgainstLayer(const Tool& tool, const game::TilemapLayer& layer)
{
    if (tool.layer != layer->GetId())
        return false;

    return true;
}

// static
void TilemapWidget::ToolIntoJson(const Tool& tool, QJsonObject& json)
{
    app::JsonWrite(json, "tool",           tool.tool);
    app::JsonWrite(json, "shape",          tool.shape);
    app::JsonWrite(json, "id",             tool.id);
    app::JsonWrite(json, "name",           tool.name);
    app::JsonWrite(json, "layer",          tool.layer);
    app::JsonWrite(json, "width",          tool.width);
    app::JsonWrite(json, "height",         tool.height);
    app::JsonWrite(json, "version",       2);

    QJsonArray tiles;
    for (size_t i=0; i<tool.height*tool.width; ++i)
    {
        ASSERT(i < tool.tiles.size());
        const auto& tile = tool.tiles[i];
        QJsonObject foo;
        app::JsonWrite(foo, "palette_index" , tile.palette_index);
        app::JsonWrite(foo, "data_value"    , tile.data_value);
        app::JsonWrite(foo, "apply_material", tile.apply_material);
        app::JsonWrite(foo, "apply_value"   , tile.apply_value);
        tiles.push_back(foo);
    }
    json["tiles"] = tiles;
}

// static
bool TilemapWidget::ToolFromJson(Tool& tool, const QJsonObject& json)
{
    unsigned tool_version = 0;
    if (!app::JsonReadSafe(json, "version", &tool_version))
        return false;

    app::JsonReadSafe(json, "tool",           &tool.tool);
    app::JsonReadSafe(json, "shape",          &tool.shape);
    app::JsonReadSafe(json, "id",             &tool.id);
    app::JsonReadSafe(json, "name",           &tool.name);
    app::JsonReadSafe(json, "layer",          &tool.layer);
    app::JsonReadSafe(json, "width",          &tool.width);
    app::JsonReadSafe(json, "height",         &tool.height);

    if (json.contains("tiles"))
    {
        const auto& tiles = json["tiles"].toArray();
        if (tiles.size() != tool.width * tool.height)
            return false;

        for (const auto& item : tiles)
        {
            TileTool::Tile tile;
            const auto& tool_json = item.toObject();
            app::JsonReadSafe(tool_json, "palette_index",  &tile.palette_index);
            app::JsonReadSafe(tool_json, "data_value",     &tile.data_value);
            app::JsonReadSafe(tool_json, "apply_material", &tile.apply_material);
            app::JsonReadSafe(tool_json, "apply_value",    &tile.apply_value);
            tool.tiles.push_back(std::move(tile));
        }
    }
    return true;
}

void TilemapWidget::ReplaceDeletedResources()
{
    for (auto& tool : mTools)
    {
        for (auto& tile : tool->tiles)
        {
            if (!tile.material_instance)
                continue;

            const auto& materialId = tile.material_instance->GetClassId();
            if (mState.workspace->IsValidMaterial(materialId))
                continue;

            tile.material_instance.reset();
        }
    }
    for (unsigned i=0; i<mState.klass->GetNumLayers(); ++i)
    {
        auto& layer = mState.klass->GetLayer(i);
        if (!layer.HasRenderComponent())
            continue;

        const auto& tilemap_material_id = layer.GetTilemapMaterialId();
        if (!tilemap_material_id.empty())
        {
            if (!mState.workspace->IsValidMaterial(tilemap_material_id))
            {
                layer.SetTilemapMaterialId("");
                WARN("Tilemap layer tilemap material was reset to nothing. [layer='%1']",
                    layer.GetName());
            }
        }

        for (unsigned i=0; i<layer.GetMaxPaletteIndex(); ++i)
        {
            const auto& material = layer.GetPaletteMaterialId(i);
            const auto& drawable = layer.GetPaletteDrawableId(i);

            if (!material.empty())
            {
                if (!mState.workspace->IsValidMaterial(material))
                {
                    layer.SetPaletteMaterialId("_checkerboard", i);
                    WARN("Tilemap layer palette material was reset to checkerboard. [layer='%1']",
                         layer.GetName());
                }
            }

            if (!drawable.empty())
            {
                if (!mState.workspace->IsValidDrawable(drawable))
                {
                    layer.SetPaletteDrawableId("", i);
                    WARN("Tilemap layer palette drawable was reset to nothing. [layer='%1']",
                        layer.GetName());
                }
            }
        }
    }
}

bool TilemapWidget::SelectLayerOnKey(unsigned int index)
{
    if (index < mState.klass->GetNumLayers())
    {
        SelectRow(mUI.layers, index);

        const auto* layer = GetCurrentLayer();
        if (mState.selection.GetResolution() != layer->GetResolution())
            mState.selection.Clear();

        DisplayLayerProperties();
        return true;
    }
    return false;
}

std::optional<TilemapWidget::Tile> TilemapWidget::FindTileUnderMouse() const
{
    const auto* layer = GetCurrentLayerInstance();
    if (!layer)
        return std::nullopt;

    const auto& mickey = mUI.widget->mapFromGlobal(QCursor::pos());
    const auto width   = mUI.widget->width();
    const auto height  = mUI.widget->height();
    if (mickey.x() < 0 || mickey.x() >= width)
        return std::nullopt;
    if (mickey.y() < 0 || mickey.y() >= height)
        return std::nullopt;

    const auto map_view = mState.klass->GetPerspective();
    const glm::vec2 tile_coord = MapWindowCoordinateToWorld(mUI, mState, mickey, map_view);

    if (!mState.map->TestPlaneCoordinate(tile_coord, *layer))
        return std::nullopt;

    const auto& ret = mState.map->MapFromPlane(tile_coord, *layer);

    return TilemapWidget::Tile { ret.col, ret.row };
}

void TilemapWidget::UpdateMaterialLists()
{
    const auto& materials = mState.workspace->ListAllMaterials();
    std::vector<app::ResourceListItem> generic;
    std::vector<app::ResourceListItem> tilemaps;
    for (const auto& item : materials)
    {
        const gfx::MaterialClass* material_class = nullptr;
        item.resource->GetContent(&material_class);
        const auto type = material_class->GetType();

        if (type == gfx::MaterialClass::Type::Tilemap)
            tilemaps.push_back(item);
        else if (type == gfx::MaterialClass::Type::Color ||
            type == gfx::MaterialClass::Type::Gradient ||
            type == gfx::MaterialClass::Type::Sprite ||
            type == gfx::MaterialClass::Type::Texture ||
            type == gfx::MaterialClass::Type::Custom)
            generic.push_back(item);
    }
    for (auto* widget : mPaletteMaterialWidgets)
    {
        widget->UpdateMaterialList(generic);
    }
    SetList(mUI.cmbTilemap, tilemaps);
}

void TilemapWidget::RefreshPreviews(bool force)
{
    // regenerate the palette thumbnails on timer since otherwise
    // getting this correct is a real pita.
    auto ClearPreviews = [this]() {
        for (auto* widget : mPaletteMaterialWidgets)
        {
            widget->ClearPreview();
        }
    };

    if (const auto* layer = GetCurrentLayer())
    {
        const auto palette = layer->GetPaletteType();
        if (palette == game::TilemapPaletteType::Tilemap)
        {
            const std::string tilemap_id = layer->GetTilemapMaterialId();
            const auto tilemap_material = mState.workspace->FindMaterialClassById(tilemap_id);
            if (tilemap_material)
            {
                // hack
                auto* material_class = const_cast<gfx::MaterialClass*>(tilemap_material.get());
                const gfx::TilemapMaterialClass tilemap(*material_class);

                for (auto* widget : mPaletteMaterialWidgets)
                {
                    if (widget->GetPreviewId() == tilemap_id && !force)
                        continue;

                    widget->SetPreview(tilemap.ExtractTile(widget->GetTileIndex()), tilemap_id);
                }
            }
            else
            {
                ClearPreviews();
            }
        }
        else if (palette == game::TilemapPaletteType::MultiMaterial)
        {
            for  (auto* widget : mPaletteMaterialWidgets)
            {
                const std::string material_id = widget->GetMaterialId();
                const std::string preview_id  = widget->GetPreviewId();
                if (material_id == preview_id && !force)
                    continue;

                if (material_id.empty())
                {
                    widget->ClearPreview();
                    continue;
                }

                if (const auto material = mState.workspace->FindMaterialClassById(material_id))
                {
                    // hack
                    auto* material_class = const_cast<gfx::MaterialClass*>(material.get());
                    const auto type = material->GetType();
                    if (type == gfx::MaterialClass::Type::Sprite)
                    {
                        const gfx::SpriteMaterialClass sprite(*material_class);
                        widget->SetPreview(sprite.ExtractPreview(), material_id);
                    }
                    else if (type == gfx::MaterialClass::Type::Texture)
                    {
                        const gfx::TextureMaterialClass texture(*material_class);
                        widget->SetPreview(texture.ExtractPreview(), material_id);
                    }
                    else
                    {
                        widget->ClearPreview();
                    }
                }
                else
                {
                    widget->ClearPreview();
                }
            }
        }
    }
    else
    {
        ClearPreviews();
    }
}

void TilemapWidget::InitializePaletteEntry(game::TilemapLayerClass& klass, size_t palette_index) const
{
    // If the palette index doesn't yet exist then we must create it.
    if (klass.HasPaletteIndex(palette_index))
        return;

    const auto type = klass.GetPaletteType();

    ASSERT(palette_index < mPaletteMaterialWidgets.size());
    auto* palette_widget = mPaletteMaterialWidgets[palette_index];
    if (type == game::TilemapLayerClass::PaletteType::Tilemap)
    {
        klass.SetPaletteOcclusion(palette_widget->GetOcclusion(), palette_index);
        klass.SetPaletteDrawableId(palette_widget->GetDrawableId(), palette_index);
        klass.SetPaletteMaterialTileIndex(palette_widget->GetTileIndex(), palette_index);
    }
    else if (type == game::TilemapLayerClass::PaletteType::MultiMaterial)
    {
        klass.SetPaletteOcclusion(palette_widget->GetOcclusion(), palette_index);
        klass.SetPaletteDrawableId(palette_widget->GetDrawableId(), palette_index);
        klass.SetPaletteMaterialId(palette_widget->GetMaterialId(), palette_index);
    }
}
} // namespace
