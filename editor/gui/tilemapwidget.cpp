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
#include "graphics/utility.h"
#include "editor/app/format.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/resource.h"
#include "editor/app/utility.h"
#include "editor/gui/tool.h"
#include "editor/gui/nerd.h"
#include "editor/gui/tilemapwidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/settings.h"
#include "editor/gui/dlgtilelayer.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/palettematerial.h"

namespace{
    constexpr auto PaletteIndexAutomatic = -1;

    gui::Size2Df GetMaterialPreviewScale(const game::TilemapClass& klass)
    {
        const auto perspective = klass.GetPerspective();
        if (perspective == game::Perspective::AxisAligned)
            return {1.0f, 1.0};
        else if (perspective == game::Perspective::Dimetric)
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
    virtual void Write(const void* ptr, size_t bytes, size_t offset) override
    {
        ASSERT(offset + bytes <= mBytes.size());

        std::memcpy(&mBytes[offset], ptr, bytes);
    }
    virtual void Read(void* ptr, size_t bytes, size_t offset) const override
    {
        ASSERT(offset + bytes <= mBytes.size());

        std::memcpy(ptr, &mBytes[offset],bytes);
    }
    virtual size_t AppendChunk(size_t bytes) override
    {
        const auto offset = mBytes.size();
        mBytes.resize(offset + bytes);
        //DEBUG("Allocated new map layer data chunk. [offset=%1, bytes=%2]", offset, Bytes {bytes});
        return offset;
    }
    virtual size_t GetByteCount() const override
    {
        return mBytes.size();
    }
    virtual void Resize(size_t bytes) override
    {
        mBytes.resize(bytes);
        //DEBUG("Resized map layer buffer to new size. [bytes=%1]", Bytes {bytes});
    }

    virtual void ClearChunk(const void* value, size_t value_size, size_t offset, size_t num_values) override
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
    LayerModel(TilemapWidget::State& state)
      : mState(state)
    {}
    virtual QVariant data(const QModelIndex& index, int role) const override
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
        return QVariant();
    }
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            if (section == 0) return "Type";
            else if (section == 1) return "Name";
            else BUG("Missing layer table column index.");
        }
        return QVariant();
    }
    virtual int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mState.klass->GetNumLayers());
    }
    virtual int columnCount(const QModelIndex&) const override
    {
        return 2;
    }
    void AddLayer(std::shared_ptr<game::TilemapLayerClass> layer)
    {
        const auto count = static_cast<int>(mState.klass->GetNumLayers());
        QAbstractTableModel::beginInsertRows(QModelIndex(), count, count);
        mState.klass->AddLayer(layer);
        QAbstractTableModel::endInsertRows();
    }
    void DeleteLayer(size_t index)
    {
        QAbstractTableModel::beginRemoveRows(QModelIndex(), index, index);
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
    virtual void Render(gfx::Painter&, gfx::Painter& tile_painter) const override
    {
        const auto movement = mWorldPos - mWorldStartPos;
        if (movement.x <= 0.0f || movement.y <= 0.0f)
            return;

        gfx::Transform model;
        model.Scale(movement.x, movement.y);
        model.MoveTo(mWorldStartPos.x, mWorldStartPos.y);
        tile_painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline), model,
                    gfx::CreateMaterialFromColor(gfx::Color::Green));

    }
    virtual void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
    {
        mWorldPos = mickey.MapToPlane();
    }
    virtual void MousePress(const MouseEvent& mickey, gfx::Transform&) override
    {
        mWorldPos = mickey.MapToPlane();
        mWorldStartPos = mWorldPos;
    }
    virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform&) override
    {
        const auto movement = mWorldPos - mWorldStartPos;
        const auto selection_width  = movement.x;
        const auto selection_height = movement.y;
        if (selection_width < 0.0f || selection_height < 0.0f)
            return true;

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
        //DEBUG("Tile selection. [x=%1, y=%2, width=%3, height=%4]", selection_tile_xpos, selection_tile_ypos,
        //      selection_tile_width, selection_tile_height);
        if (selection_tile_width == 0 || selection_tile_height == 0)
            return true;

        TileSelection tile_selection;
        tile_selection.start_row = selection_tile_ypos;
        tile_selection.start_col = selection_tile_xpos;
        tile_selection.width     = selection_tile_width;
        tile_selection.height    = selection_tile_height;
        mState.selection = tile_selection;
        return true;
    }
private:
    const game::Tilemap& mMap;
    const game::TilemapLayer& mLayer;
    TilemapWidget::State& mState;
private:
    glm::vec2 mWorldStartPos;
    glm::vec2 mWorldPos;
};

class TilemapWidget::TileBrushTool : public MouseTool
{
public:
    TileBrushTool(const Tool& tool, State& state, game::TilemapLayer& layer)
      : mTool(tool)
      , mState(state)
      , mLayer(layer)
    {
        auto klass = state.workspace->GetMaterialClassById(tool.material);
        mMaterial = gfx::CreateMaterialInstance(klass);
    }
    virtual void Render(gfx::Painter& scene_painter, gfx::Painter& tile_painter) const override
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
        const auto cuboid_scale = engine::GetTileCuboidFactors(perspective);
        const auto tile_size = glm::vec3{tile_width_units, tile_height_units, tile_depth_units};
        const auto render_size = engine::ComputeTileRenderSize(tile_projection_transform_matrix,
                                                               {tile_width_units, tile_height_units},
                                                               perspective);
        gfx::FRect tool_bounds;
        gfx::TileBatch batch;
        for (unsigned row=0; row<mTool.height; ++row)
        {
            for (unsigned col=0; col<mTool.width; ++col)
            {
                const auto tile_row = mTileRow + row - mTool.height / 2;
                const auto tile_col = mTileCol + col - mTool.width / 2;
                if (tile_row < 0 || tile_row >= mLayer.GetHeight()||
                    tile_col < 0 || tile_col >= mLayer.GetWidth())
                    continue;

                gfx::TileBatch::Tile tile;
                tile.pos.y = tile_row;
                tile.pos.x = tile_col;
                tile.pos.z = mLayer.GetDepth();
                batch.AddTile(tile);

                gfx::FRect rect;
                rect.Resize(tile_width_units, tile_height_units);
                rect.Translate(tile.pos.x * tile_width_units, tile.pos.y * tile_height_units);
                tool_bounds = Union(tool_bounds, rect);
            }
        }
        batch.SetTileWorldSize(tile_size * cuboid_scale);
        batch.SetTileRenderWidth(render_size.x * tile_render_width_scale);
        batch.SetTileRenderHeight(render_size.y * tile_render_height_scale);
        batch.SetTileShape(gfx::TileBatch::TileShape::Automatic);
        if (perspective == game::Perspective::AxisAligned)
            batch.SetProjection(gfx::TileBatch::Projection::AxisAligned);
        else if (perspective == game::Perspective::Dimetric)
            batch.SetProjection(gfx::TileBatch::Projection::Dimetric);
        else BUG("missing tile projection.");

        // draw the tilebatch
        scene_painter.Draw(batch, tile_projection_transform_matrix, *mMaterial);

        // draw a rect around the tool on the tile plane
        {
            gfx::Transform model;
            model.Resize(tool_bounds);
            model.MoveTo(tool_bounds);
            tile_painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Outline), model,
                              gfx::CreateMaterialFromColor(gfx::Color4f(gfx::Color::HotPink, 0.8)));
        }


    }

    virtual void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
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
    virtual void MousePress(const MouseEvent& mickey, gfx::Transform&) override
    {
        if (mickey->button() != Qt::LeftButton)
            return;

        mActive = true;

        if (mLayer->HasRenderComponent() && mTool.apply_material)
        {
            // Figure out a new palette entry if needed.
            if (mTool.palette_index == PaletteIndexAutomatic)
            {
                mTool.palette_index = mLayer->FindMaterialIndex(mTool.material);
                if (mTool.palette_index == 0xff)
                    mTool.palette_index = mLayer->FindNextAvailableMaterialIndex();
                // validation should have been done before the tool was started.
                ASSERT(mTool.palette_index >= 0 && mTool.palette_index < 0xff);
            }
            auto* layer_klass = mState.klass->FindLayerById(mLayer.GetClassId());
            layer_klass->SetPaletteMaterialId(mTool.material, mTool.palette_index);
        }

        // apply tool on the spot already, so that if the user simply
        // clicks the button without moving the mouse the tile under
        // the mouse is edited as expected
        ApplyTool();
    }
    virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform&)  override
    {
        if (mickey->button() == Qt::LeftButton)
            mActive = false;
        return false;
    }

    void ApplyTool()
    {
        for (unsigned row=0; row<mTool.height; ++row)
        {
            for (unsigned col=0; col<mTool.width; ++col)
            {
                const auto tile_row = mTileRow + row - mTool.height / 2;
                const auto tile_col = mTileCol + col - mTool.width / 2;
                // discard tiles outside of the actual tile area.
                if (tile_row < 0 || tile_row >= mLayer.GetHeight()||
                    tile_col < 0 || tile_col >= mLayer.GetWidth())
                    continue;

                // apply the material assigned to the tool onto the tiles
                // in the current layer's render component.
                if (mLayer->HasRenderComponent() && mTool.apply_material)
                    mLayer.SetTilePaletteIndex(mTool.palette_index, tile_row, tile_col);

                // apply the data value assigned to the tool onto the tiles
                // in the current layer's data component.
                if (mLayer->HasDataComponent() && mTool.apply_value)
                    mLayer.SetTileValue(mTool.value, tile_row, tile_col);
            }
        }

    }
    void SetTileRow(int row)
    { mTileRow = row; }
    void SetTileCol(int col)
    { mTileCol = col; }
private:
    TilemapWidget::Tool mTool;
    TilemapWidget::State& mState;
    game::TilemapLayer& mLayer;
    std::unique_ptr<gfx::Material> mMaterial;
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
    mUI.widget->onKeyPress     = std::bind(&TilemapWidget::KeyPress,     this, std::placeholders::_1);
    mUI.widget->onPaintScene   = std::bind(&TilemapWidget::PaintScene,   this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onZoomIn       = [this]() { MouseZoom(std::bind(&TilemapWidget::ZoomIn, this)); };
    mUI.widget->onZoomOut      = [this]() { MouseZoom(std::bind(&TilemapWidget::ZoomOut, this)); };

    connect(mUI.layers->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &TilemapWidget::LayerSelectionChanged);
    // connect workspace signals for resource management
    connect(workspace, &app::Workspace::ResourceAdded,   this, &TilemapWidget::ResourceAdded);
    connect(workspace, &app::Workspace::ResourceRemoved, this, &TilemapWidget::ResourceRemoved);
    connect(workspace, &app::Workspace::ResourceUpdated, this, &TilemapWidget::ResourceUpdated);

    PopulateFromEnum<gui::GridDensity>(mUI.cmbGrid);
    PopulateFromEnum<game::TilemapClass::Perspective>(mUI.cmbPerspective);
    PopulateFromEnum<game::TilemapLayerClass::Type>(mUI.cmbLayerType);
    PopulateFromEnum<game::TilemapLayerClass::Storage>(mUI.cmbLayerStorage);
    PopulateFromEnum<game::TilemapLayerClass::Cache>(mUI.cmbLayerCache);
    PopulateFromEnum<game::TilemapLayerClass::Resolution>(mUI.cmbLayerResolution);
    PopulateFromEnum<TilemapWidget::ToolFunction>(mUI.cmbToolFunction);
    PopulateFromEnum<TilemapWidget::ToolShape>(mUI.cmbToolShape);
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.zoom, 1.0f);
    const auto& materials = mState.workspace->ListAllMaterials();
    SetList(mUI.cmbToolMaterial, materials);
    SetList(mUI.cmbTileMaterial, materials);
    SetRange(mUI.toolValue, -0x800000, 0xffffff); // min is 24 bit signed and max is 24bit unsigned
    SetRange(mUI.tileValue, -0x800000, 0xffffff); // min is 24 bit signed and max is 24bit unsigned
    SetEnabled(mUI.actionPalette, false);

    // generate a list of widgets for the layer color palette.
    for (int i=0; i<256; ++i)
    {
        auto* widget = new PaletteMaterial(mState.workspace, this);
        widget->SetIndex(i);
        widget->SetLabel(QString("#%1").arg(i));
        widget->SetMaterialPreviewScale(Size2Df(1.0f, 1.0f));
        widget->setObjectName(QString::number(i));
        // FUCKING Qt layout is suddenly going all bonkers without this!
        // First it was working fine and then it's not working. like WTF!?
        widget->setMaximumWidth(330);

        mUI.layout->addWidget(widget);
        mPaletteMaterialWidgets.push_back(widget);
        connect(widget, &PaletteMaterial::ValueChanged, this, &TilemapWidget::PaletteMaterialChanged);
    }

    GenerateTools();
    UpdateToolCombo();
    UpdateToolToolbar();
    SetCurrentTool(mTools[0].id);
    ShowCurrentTool();

    DisplayMapProperties();
    DisplayLayerProperties();
    DisplayCurrentCameraLocation();
    setWindowTitle("My Map");
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
        layer.Load(data, 1024 /*default cache size*/);
        layer.SetFlags(layer.GetClass().GetFlags());
    }

    // Update material scale in palette widgets
    for (auto& widget : mPaletteMaterialWidgets)
    {
        widget->SetMaterialPreviewScale(GetMaterialPreviewScale(*mState.klass));
    }


    int current_layer = -1;
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

    mTools.clear();

    size_t num_tools = 0;
    GetProperty(resource, "num_tools", &num_tools);
    for (size_t i=0;  i<num_tools; ++i)
    {
        QJsonObject json;
        GetProperty(resource, PropertyKey("tool", i), &json);
        Tool tool;
        ToolFromJson(tool, json);
        mTools.push_back(std::move(tool));
    }

    mModel->Reset();
    SelectRow(mUI.layers, current_layer);
    ReplaceDeletedResources();
    UpdateToolCombo();
    UpdateToolToolbar();
    SetCurrentTool(mTools.empty() ? "" : mTools[0].id);
    ShowCurrentTool();
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
void TilemapWidget::InitializeSettings(const UISettings& settings)
{
    SetValue(mUI.chkShowViewport, settings.show_viewport);
    SetValue(mUI.chkShowGrid,     settings.show_grid);
    SetValue(mUI.chkShowOrigin,   settings.show_origin);
    SetValue(mUI.cmbGrid,         settings.grid);
    SetValue(mUI.zoom,            settings.zoom);
}
void TilemapWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    for (auto* action : mToolActions)
        bar.addAction(action);
    bar.addSeparator();
    bar.addAction(mUI.actionPalette);

}
void TilemapWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    for (auto* action : mToolActions)
        menu.addAction(action);
    menu.addSeparator();
    menu.addAction(mUI.actionPalette);
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

    settings.SetValue("Tilemap", "num_tools", mTools.size());
    for (size_t i=0; i<mTools.size(); ++i)
    {
        const auto& tool = mTools[i];
        QJsonObject json;
        ToolIntoJson(tool, json);
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
        Tool tool;
        ToolFromJson(tool, json);
        mTools.push_back(std::move(tool));
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
        layer.Load(data, 1024 /*default cache size*/);
        layer.SetFlags(layer.GetClass().GetFlags());
    }

    // Update material scale in palette widgets
    for (auto& widget : mPaletteMaterialWidgets)
    {
        widget->SetMaterialPreviewScale(GetMaterialPreviewScale(*mState.klass));
    }

    mModel->Reset();
    SelectRow(mUI.layers, current_layer);
    UpdateToolCombo();
    UpdateToolToolbar();
    SetCurrentTool(mTools.empty() ? "" : mTools[0].id);
    ShowCurrentTool();
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
    mUI.widget->reloadShaders();
}
void TilemapWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();
}
void TilemapWidget::Shutdown()
{
    mUI.widget->dispose();
}
void TilemapWidget::Update(double dt)
{
    mCurrentTime += dt;

    mAnimator.Update(mUI, mState);

}
void TilemapWidget::Render()
{
    mUI.widget->triggerPaint();
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
        mCurrentTool.reset();
        UncheckTools();
    }
    else if (mState.selection.has_value())
    {
        mState.selection.reset();
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
    if (key->key() == Qt::Key_Space)
        return OpenMaterialPaletteOnCurrentTool();
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
    return false;
}

bool TilemapWidget::GetStats(Stats* stats) const
{
    stats->time  = 0.0f;
    stats->graphics.valid = true;
    stats->graphics.fps   = mUI.widget->getCurrentFPS();
    stats->graphics.vsync = mUI.widget->haveVSYNC();
    const auto& dev_stats = mUI.widget->getDeviceResourceStats();
    stats->device.static_vbo_mem_alloc    = dev_stats.static_vbo_mem_alloc;
    stats->device.static_vbo_mem_use      = dev_stats.static_vbo_mem_use;
    stats->device.dynamic_vbo_mem_alloc   = dev_stats.dynamic_vbo_mem_alloc;
    stats->device.dynamic_vbo_mem_use     = dev_stats.dynamic_vbo_mem_use;
    stats->device.streaming_vbo_mem_use   = dev_stats.streaming_vbo_mem_use;
    stats->device.streaming_vbo_mem_alloc = dev_stats.streaming_vbo_mem_alloc;
    return true;
}
void TilemapWidget::Refresh()
{}

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
        mState.selection.reset();

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
            instance.Load(dst_data, 1024);
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

    SetProperty(resource, "num_tools", mTools.size());
    for (size_t i=0; i<mTools.size(); ++i)
    {
        const auto& tool = mTools[i];
        QJsonObject json;
        ToolIntoJson(tool, json);
        SetProperty(resource, PropertyKey("tool", i), json);
    }

    mState.workspace->SaveResource(resource);

    DisplayLayerProperties();
    mHash = hash;
}

void TilemapWidget::on_actionPalette_triggered()
{
    OpenMaterialPaletteOnCurrentTool();
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
    if (layer_class->HasRenderComponent())
    {
        const auto& material = dlg.GetMaterialId();
        if (material.empty())
        {
            // the max palette index value indicates "no value set"
            const auto max_palette_index = layer_class->GetMaxPaletteIndex();
            layer_class->SetDefaultTilePaletteMaterialIndex(max_palette_index);
        }
        else
        {
            // set the first material in the palette at index to the
            // material that was chosen in the new layer dialog.
            layer_class->SetDefaultTilePaletteMaterialIndex(0);
            layer_class->SetPaletteMaterialId(material, 0);
        }
    }
    if (layer_class->HasDataComponent())
    {
        layer_class->SetDefaultTileDataValue(dlg.GetDataValue());
    }
    mModel->AddLayer(layer_class);

    // create scratch layer data buffer for storing the edits
    auto data = std::make_shared<LayerData>();
    layer_class->Initialize(map_width, map_height, *data);

    mLayerData[layer_class->GetId()] = data;

    // create an instance of the layer for render visualization.
    auto layer_instance = game::CreateTilemapLayer(layer_class, map_width, map_height);
    layer_instance->Load(data, 1024 /* default cache size */);
    mState.map->AddLayer(std::move(layer_instance));

    SelectLastRow(mUI.layers);
    DisplayLayerProperties();
    DisplayMapProperties();
}

void TilemapWidget::on_btnDeleteLayer_clicked()
{
    const auto& indices = GetSelection(mUI.layers);
    if (indices.isEmpty())
        return;
    const auto index  = indices[0].row();
    const auto& layer = mState.klass->GetLayer(index);
    const auto& data  = mLayerData[layer.GetId()];
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

void TilemapWidget::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void TilemapWidget::on_cmbTool_currentIndexChanged(int index)
{
    SetCurrentTool(mTools[index].id);
    ShowCurrentTool();
}
void TilemapWidget::on_cmbTool_editTextChanged(const QString& text)
{
    if (auto* tool = GetCurrentTool())
    {
        // careful, this signal is invoked also when the user
        // has changed the combo index.
        tool->name = text;
        UpdateToolToolbar();
        UpdateToolCombo();
    }
}

void TilemapWidget::on_btnAddTool_clicked()
{
    Tool tool;
    tool.name          = "My Tool";
    tool.id            = app::RandomString();
    tool.tool          = ToolFunction::TileBrush;
    tool.shape         = ToolShape::Rectangle;
    tool.width         = 10;
    tool.height        = 10;
    tool.material      = "_checkerboard";
    tool.palette_index = PaletteIndexAutomatic;
    mTools.push_back(tool);
    UpdateToolToolbar();
    UpdateToolCombo();
    SetCurrentTool(tool.id);
    ShowCurrentTool();
    SetEnabled(mUI.btnDelTool, true);
}
void TilemapWidget::on_btnDelTool_clicked()
{
    if (auto* tool = GetCurrentTool())
    {
        QString other;
        for (auto it = mTools.begin(); it != mTools.end(); ++it)
        {
            if ((*it).id == tool->id)
            {
                it = mTools.erase(it);
                if (it != mTools.end())
                    other = it->id;
                break;
            }
            other = it->id;
        }
        UpdateToolToolbar();
        UpdateToolCombo();
        SetCurrentTool(other);
        ShowCurrentTool();
    }
    SetEnabled(mUI.btnDelTool, !mTools.empty());
}

void TilemapWidget::on_btnSelectToolMaterial_clicked()
{
    if (auto* tool = GetCurrentTool())
    {
        DlgMaterial dlg(this, mState.workspace, tool->material);
        dlg.SetPreviewScale(GetMaterialPreviewScale(*mState.klass));
        if (dlg.exec() == QDialog::Rejected)
            return;
        tool->material = app::ToUtf8(dlg.GetSelectedMaterialId());
        ShowCurrentTool();
    }
}

void TilemapWidget::on_btnSetToolMaterialParams_clicked()
{

}

void TilemapWidget::on_btnEditToolMaterial_clicked()
{
    const QString materialId = GetItemId(mUI.cmbToolMaterial);
    emit OpenResource(materialId);
}

void TilemapWidget::on_btnResetPaletteIndex_clicked()
{
    SetValue(mUI.toolPaletteIndex, -1);
    ModifyCurrentTool();
}

void TilemapWidget::on_cmbToolFunction_currentIndexChanged(int)
{
    ModifyCurrentTool();
    ShowCurrentTool();
}
void TilemapWidget::on_cmbToolShape_currentIndexChanged(int)
{
    ModifyCurrentTool();
}
void TilemapWidget::on_toolWidth_valueChanged(int)
{
    ModifyCurrentTool();
}
void TilemapWidget::on_toolHeight_valueChanged(int)
{
    ModifyCurrentTool();
}

void TilemapWidget::on_cmbToolMaterial_currentIndexChanged(int)
{
    ModifyCurrentTool();
    ShowCurrentTool();
}

void TilemapWidget::on_toolPaletteIndex_valueChanged(int)
{
    ModifyCurrentTool();
}
void TilemapWidget::on_toolValue_valueChanged(int)
{
    ModifyCurrentTool();
}

void TilemapWidget::on_chkToolMaterial_stateChanged(int)
{
    ModifyCurrentTool();
}
void TilemapWidget::on_chkToolValue_stateChanged(int)
{
    ModifyCurrentTool();
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
void TilemapWidget::on_layerDepth_valueChanged(int)
{
    ModifyCurrentLayer();
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

void TilemapWidget::on_cmbTileMaterial_currentIndexChanged(int)
{
    if (!mState.selection.has_value())
        return;
    const auto& selection = mState.selection.value();

    auto* klass = GetCurrentLayer();
    auto* layer = GetCurrentLayerInstance();
    if (!klass || !layer || !layer->HasRenderComponent())
        return;

    const std::string& material = GetItemId(mUI.cmbTileMaterial);

    auto palette_index = klass->FindMaterialIndex(material);
    if (palette_index == 0xff)
        palette_index = klass->FindNextAvailableMaterialIndex();
    if (palette_index == 0xff)
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Warning);
        msg.setWindowTitle("Layer Palette is Full");
        msg.setText(app::toString("The material palette on current layer '%1' is full and no more materials can be added to it.\n\n"
                                  "You can select a material index to overwrite manually in the tool setting.\n"
                                  "Reusing a material index *will* overwrite that material.", klass->GetName()));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.exec();
        return;
    }
    klass->SetPaletteMaterialId(material, palette_index);
    for (unsigned row=0; row<selection.height; ++row)
    {
        for (unsigned col=0; col<selection.width; ++col)
        {
            const auto tile_row = selection.start_row + row;
            const auto tile_col = selection.start_col + col;
            ASSERT(layer->SetTilePaletteIndex(palette_index, tile_row, tile_col));
        }
    }
    ClearUnusedPaletteEntries();
    UpdateLayerPalette();
}

void TilemapWidget::on_btnSelectTileMaterial_clicked()
{
    if (!mState.selection.has_value())
        return;

    const auto& selection = mState.selection.value();

    auto* klass = GetCurrentLayer();
    auto* layer = GetCurrentLayerInstance();
    if (!klass || !layer || !layer->HasRenderComponent())
        return;

    DlgMaterial dlg(this, mState.workspace, "");
    dlg.SetPreviewScale(GetMaterialPreviewScale(*mState.klass));
    if (dlg.exec() == QDialog::Rejected)
        return;
    const auto& material = app::ToUtf8(dlg.GetSelectedMaterialId());

    auto palette_index = klass->FindMaterialIndex(material);
    if (palette_index == 0xff)
        palette_index = klass->FindNextAvailableMaterialIndex();
    if (palette_index == 0xff)
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Warning);
        msg.setWindowTitle("Layer Palette is Full");
        msg.setText(app::toString("The material palette on current layer '%1' is full and no more materials can be added to it.\n\n"
                                  "You can select a material index to overwrite manually in the tool setting.\n"
                                  "Reusing a material index *will* overwrite that material.", klass->GetName()));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.exec();
        return;
    }
    klass->SetPaletteMaterialId(material, palette_index);
    for (unsigned row=0; row<selection.height; ++row)
    {
        for (unsigned col=0; col<selection.width; ++col)
        {
            const auto tile_row = selection.start_row + row;
            const auto tile_col = selection.start_col + col;
            ASSERT(layer->SetTilePaletteIndex(palette_index, tile_row, tile_col));
        }
    }
    ClearUnusedPaletteEntries();
    UpdateLayerPalette();
    DisplaySelection();
}

void TilemapWidget::on_btnDeleteTileMaterial_clicked()
{
    if (!mState.selection.has_value())
        return;

    const auto& selection = mState.selection.value();

    auto* klass = GetCurrentLayer();
    auto* layer = GetCurrentLayerInstance();
    if (!klass || !layer || !layer->HasRenderComponent())
        return;

    const auto nothing_index = layer->GetMaxPaletteIndex();
    for (unsigned row=0; row<selection.height; ++row)
    {
        for (unsigned col=0; col<selection.width; ++col)
        {
            const auto tile_row = selection.start_row + row;
            const auto tile_col = selection.start_col + col;
            ASSERT(layer->SetTilePaletteIndex(nothing_index, tile_row, tile_col));
        }
    }
    ClearUnusedPaletteEntries();
    UpdateLayerPalette();
    DisplaySelection();
}

void TilemapWidget::on_btnEditTileMaterial_clicked()
{
    if (!mState.selection.has_value())
        return;

    const auto& selection = mState.selection.value();

    auto* klass = GetCurrentLayer();
    auto* layer = GetCurrentLayerInstance();
    if (!klass || !layer || !layer->HasRenderComponent())
        return;

    const auto nothing_index = layer->GetMaxPaletteIndex();
    for (unsigned row=0; row<selection.height; ++row)
    {
        for (unsigned col=0; col<selection.width; ++col)
        {
            const auto tile_row = selection.start_row + row;
            const auto tile_col = selection.start_col + col;
            uint8_t palette_index = 0;
            ASSERT(layer->GetTilePaletteIndex(&palette_index, tile_row, tile_col));
            if (palette_index == layer->GetMaxPaletteIndex())
                continue;
            const auto& materialId = klass->GetPaletteMaterialId(palette_index);
            emit OpenResource(app::FromUtf8(materialId));
        }
    }
}

void TilemapWidget::on_tileValue_valueChanged(int)
{
    if (!mState.selection.has_value())
        return;

    auto* klass = GetCurrentLayer();
    auto* layer = GetCurrentLayerInstance();
    if (!klass || !layer || !layer->HasDataComponent())
        return;

    const auto& selection = mState.selection.value();
    const int32_t tile_value = GetValue(mUI.tileValue);

    for (unsigned row=0; row<selection.height; ++row)
    {
        for (unsigned col=0; col<selection.width; ++col)
        {
            const auto tile_row = selection.start_row + row;
            const auto tile_col = selection.start_col + col;
            ASSERT(layer->SetTileValue(tile_value, tile_row, tile_col));
        }
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
    QAction* action = qobject_cast<QAction*>(sender());

    StartTool(action->data().toString());

}

void TilemapWidget::StartTool(const QString& id)
{
    mCurrentTool.reset();

    SetCurrentTool(id);
    ShowCurrentTool();

    auto* tool  = GetCurrentTool();
    auto* layer = GetCurrentLayerInstance();
    ASSERT(tool);
    ASSERT(layer);

    const auto tile_width  = mState.klass->GetTileWidth();
    const auto tile_height = mState.klass->GetTileHeight();
    const auto tile_scaler = layer->GetTileSizeScaler();
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
        auto mouse_tool = std::make_unique<TileBrushTool>(*tool, mState, *layer);
        mouse_tool->SetTileCol(tile_col);
        mouse_tool->SetTileRow(tile_row);
        mCurrentTool = std::move(mouse_tool);
    }
    SetEnabled(mUI.actionPalette, true);

    for (auto* action : mToolActions)
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

void TilemapWidget::ResourceAdded(const app::Resource* resource)
{
    if (resource->IsMaterial())
    {
        const auto& materials = mState.workspace->ListAllMaterials();
        for (auto* widget : mPaletteMaterialWidgets)
            widget->UpdateMaterialList(materials);

        SetList(mUI.cmbToolMaterial, materials);
        SetList(mUI.cmbTileMaterial, materials);
    }
}
void TilemapWidget::ResourceRemoved(const app::Resource* resource)
{
    if (resource->IsMaterial())
    {
        ReplaceDeletedResources();

        const auto& materials = mState.workspace->ListAllMaterials();
        for (auto* widget : mPaletteMaterialWidgets)
            widget->UpdateMaterialList(materials);

        mState.renderer.ClearPaintState();

        SetList(mUI.cmbToolMaterial, materials);
        SetList(mUI.cmbTileMaterial, materials);
        DisplayLayerProperties();
        ShowCurrentTool();
    }
}
void TilemapWidget::ResourceUpdated(const app::Resource* resource)
{
    if (resource->IsMaterial())
    {
        const auto& materials = mState.workspace->ListAllMaterials();
        for (auto* widget : mPaletteMaterialWidgets)
            widget->UpdateMaterialList(materials);

        mState.renderer.ClearPaintState();

        SetList(mUI.cmbToolMaterial, materials);
        SetList(mUI.cmbTileMaterial, materials);
    }
}

void TilemapWidget::LayerSelectionChanged(const QItemSelection, const QItemSelection)
{
    const auto* current = GetCurrentLayer();
    if (!current)
    {
        mCurrentTool.reset();
        UncheckTools();
    }
    DisplayLayerProperties();
    DisplaySelection();

    for (auto* action : mToolActions)
        SetEnabled(action, current != nullptr);
}

void TilemapWidget::PaletteMaterialChanged(const PaletteMaterial* material)
{
    if (auto* layer = GetCurrentLayer())
    {
        layer->SetPaletteMaterialId(material->GetMaterialId(), material->GetIndex());
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
    SetValue(mUI.cmbLayerCache,      -1);
    SetValue(mUI.layerDepth,          0);
    SetValue(mUI.chkLayerVisible,   false);
    SetValue(mUI.chkLayerEnabled,   false);
    SetValue(mUI.chkLayerReadOnly,  false);
    SetEnabled(mUI.btnDeleteLayer,      false);
    SetEnabled(mUI.btnEditLayer,        false);
    SetEnabled(mUI.btnDeleteLayer,      false);
    SetEnabled(mUI.layerProperties,     false);
    SetEnabled(mUI.layerPalette,        false);

    for (auto* widget : mPaletteMaterialWidgets)
    {
        widget->ResetMaterial();
        widget->setVisible(false);
    }

    if (const auto* layer = GetCurrentLayer())
    {
        const auto* inst = GetCurrentLayerInstance();

        SetValue(mUI.layerName,          layer->GetName());
        SetValue(mUI.layerID,            layer->GetId());
        SetValue(mUI.layerFileName,      layer->GetDataUri());
        SetValue(mUI.cmbLayerType,       layer->GetType());
        SetValue(mUI.cmbLayerStorage,    layer->GetStorage());
        SetValue(mUI.cmbLayerCache,      layer->GetCache());
        SetValue(mUI.cmbLayerResolution, layer->GetResolution());
        SetValue(mUI.layerDepth,         layer->GetDepth() * -1);
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

        if (layer->HasRenderComponent())
        {
            mUI.scrollAreaWidgetContents->setUpdatesEnabled(false);
            mUI.scrollArea->setUpdatesEnabled(false);

            const auto type = layer->GetType();
            const auto palette_max = game::TilemapLayerClass::GetMaxPaletteIndex(type);
            for (unsigned i=0; i<palette_max; ++i)
            {
                auto* widget = mPaletteMaterialWidgets[i];
                widget->setVisible(true);
                widget->SetMaterial(layer->GetPaletteMaterialId(i));
            }
            SetEnabled(mUI.layerPalette, true);

            mUI.scrollAreaWidgetContents->setUpdatesEnabled(true);
            mUI.scrollArea->setUpdatesEnabled(true);
        }
        SetEnabled(mUI.btnDeleteLayer,  true);
        SetEnabled(mUI.layerProperties, true);
        SetEnabled(mUI.layerPalette,    true);
    }
}

void TilemapWidget::DisplaySelection()
{
    SetEnabled(mUI.selection, false);
    SetEnabled(mUI.cmbTileMaterial, false);
    SetEnabled(mUI.btnDeleteTileMaterial, false);
    SetEnabled(mUI.btnEditTileMaterial, false);
    SetValue(mUI.cmbTileMaterial, -1);
    SetPlaceholderText(mUI.cmbTileMaterial, "");

    SetValue(mUI.tileValue, 0);
    SetEnabled(mUI.tileValue, false);

    if (!mState.selection.has_value())
        return;

    const auto* layer = GetCurrentLayerInstance();
    if (!layer)
        return;

    const auto& selection = mState.selection.value();

    SetEnabled(mUI.selection, true);

    if (layer->HasRenderComponent())
    {
        SetEnabled(mUI.cmbTileMaterial, true);
        SetEnabled(mUI.btnDeleteTileMaterial, true);
        SetEnabled(mUI.btnEditTileMaterial, true);
        std::set<uint8_t> indices;
        for (unsigned row=0; row<selection.height; ++row)
        {
            for  (unsigned col=0; col<selection.width; ++col)
            {
                uint8_t palette_index = 0;
                const auto tile_row = selection.start_row + row;
                const auto tile_col = selection.start_col + col;
                ASSERT(layer->GetTilePaletteIndex(&palette_index, tile_row, tile_col));
                indices.insert(palette_index);
                if (indices.size() > 1)
                    break;
            }
        }
        if (indices.size() == 1)
        {
            const auto& material = layer->GetPaletteMaterialId(*indices.begin());
            SetValue(mUI.cmbTileMaterial, ListItemId(material));
            SetPlaceholderText(mUI.cmbTileMaterial, "Nothing");
        }
        else
        {
            SetValue(mUI.cmbTileMaterial, -1);
            SetPlaceholderText(mUI.cmbTileMaterial, "[Multiple]");
        }
    }
    if (layer->HasDataComponent())
    {
        SetEnabled(mUI.tileValue, true);
        int32_t tile_value = 0;
        ASSERT(layer->GetTileValue(&tile_value, selection.start_row, selection.start_col));
        SetValue(mUI.tileValue, tile_value);
    }
}

void TilemapWidget::UpdateLayerPalette()
{
    if (const auto* layer = GetCurrentLayer())
    {
        if (layer->HasRenderComponent())
        {
            for (unsigned i = 0; i < layer->GetMaxPaletteIndex(); ++i)
            {
                auto* widget = mPaletteMaterialWidgets[i];
                widget->SetMaterial(layer->GetPaletteMaterialId(i));
            }
        }
    }
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

    const auto perspective = mState.klass->GetPerspective();
    // Create tile painter for drawing in tile coordinate space.
    gfx::Painter tile_painter(painter.GetDevice());
    tile_painter.SetViewMatrix(CreateViewMatrix(mUI, mState, perspective));
    tile_painter.SetProjectionMatrix(CreateProjectionMatrix(mUI, perspective));
    tile_painter.SetPixelRatio({1.0f*xs*zoom, 1.0f*ys*zoom});
    tile_painter.SetViewport(0, 0, width, height);
    tile_painter.SetSurfaceSize(width, height);

    gfx::Painter scene_painter(painter.GetDevice());
    scene_painter.SetViewMatrix(CreateViewMatrix(mUI, mState, game::Perspective::AxisAligned));
    scene_painter.SetProjectionMatrix(CreateProjectionMatrix(mUI, game::Perspective::AxisAligned));
    scene_painter.SetPixelRatio({1.0f*xs*zoom, 1.0f*ys*zoom});
    scene_painter.SetViewport(0, 0, width, height);
    scene_painter.SetSurfaceSize(width, height);

    // draw the background grid in tilespace and project with the map perspective
    if (GetValue(mUI.chkShowGrid))
    {
        DrawCoordinateGrid(tile_painter, grid, zoom, xs, ys, width, height);
    }

    // render the actual tilemap
    {
        class DrawHook : public engine::TileBatchDrawHook
        {
        public:
            explicit DrawHook(const State& state, const game::TilemapLayer* layer, const gfx::Painter& painter)
              : mState(state)
              , mLayer(layer)
              , mPainter(painter)
            {
                auto klass = std::make_shared<gfx::MaterialClass>(gfx::MaterialClass::Type::Texture);
            }
            virtual void EndDrawBatch(const engine::TileBatch& batch, const glm::mat4& model, gfx::Painter& painter) override
            {
                if (!mState.selection.has_value() || !mLayer)
                    return;

                const auto layer_index = mState.map->FindLayerIndex(mLayer);
                if (batch.layer != layer_index)
                    return;

                const auto perspective = mState.map->GetPerspective();
                const auto& selection = mState.selection.value();

                for (const auto& tile : batch.tiles)
                {
                    if (tile.pos.x < selection.start_col ||
                        tile.pos.x >= selection.start_col + selection.width)
                        continue;
                    else if (tile.pos.y < selection.start_row ||
                             tile.pos.y >= selection.start_row + selection.height)
                        continue;

                    gfx::TileBatch tiles;
                    tiles.AddTile(tile);
                    tiles.SetTileWorldSize(batch.tile_size);
                    tiles.SetTileRenderSize(batch.render_size);
                    tiles.SetTileShape(gfx::TileBatch::TileShape::Automatic);
                    if (perspective == game::Perspective::AxisAligned)
                        tiles.SetProjection(gfx::TileBatch::Projection::AxisAligned);
                    else if (perspective == game::Perspective::Dimetric)
                        tiles.SetProjection(gfx::TileBatch::Projection::Dimetric);
                    else BUG("unknown projection");

                    DrawEdges(painter, mPainter, tiles, *batch.material, model, mState.klass->GetId());
                }
            }
        private:
            const State& mState;
            const game::TilemapLayer* mLayer = nullptr;
            const gfx::Painter& mPainter;
        } hook(mState, GetCurrentLayerInstance(), painter);

        gfx::Painter p(painter.GetDevice());

        engine::Renderer::Camera camera;
        camera.position   = glm::vec2{mState.camera_offset_x, mState.camera_offset_y};
        camera.scale      = glm::vec2{xs * zoom, ys * zoom };
        camera.rotation   = GetValue(mUI.rotation);
        camera.viewport   = game::FRect(-width*0.5f, -height*0.5f, width, height);
        mState.renderer.SetCamera(camera);

        engine::Renderer::Surface surface;
        surface.viewport = gfx::IRect(0, 0, width, height);
        surface.size     = gfx::USize(width, height);
        mState.renderer.SetSurface(surface);

        const bool show_render_layers = GetValue(mUI.chkShowRenderLayers);
        const bool show_data_layers = GetValue(mUI.chkShowDataLayers);

        mState.renderer.BeginFrame();
        mState.renderer.Draw(*mState.map, p, &hook, show_render_layers, show_data_layers);
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
        const auto cuboid_scale = engine::GetTileCuboidFactors(perspective);

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
                              gfx::CreateMaterialFromColor(gfx::Color::Green));
        }

        // draw the selection if any
        if (mState.selection.has_value())
        {
            const auto& selection = mState.selection.value();
            gfx::Transform model;
            model.Scale(layer_tile_width * selection.width,
                        layer_tile_height * selection.height);
            model.MoveTo(layer_tile_width * selection.start_col,
                         layer_tile_height * selection.start_row);
            //tile_painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Outline, 2.0f), model,
            //                  gfx::CreateMaterialFromColor(gfx::Color::Green));
        }

        // visualize the tile under the mouse pointer.
        const auto& mickey = mUI.widget->mapFromGlobal(QCursor::pos());
        if (mickey.x() >= 0 && mickey.x() < width &&
            mickey.y() >= 0 && mickey.y() < height && !mCurrentTool)
        {
            const glm::vec2 tile_coord = MapWindowCoordinateToWorld(mUI, mState, mickey, perspective);
            const unsigned tile_col = tile_coord.x / layer_tile_width;
            const unsigned tile_row = tile_coord.y / layer_tile_height;
            if (tile_col < layer->GetWidth() && tile_row < layer->GetHeight())
            {
                gfx::Transform model;
                model.Scale(layer_tile_width, layer_tile_height);
                model.MoveTo(layer_tile_width * tile_col, layer_tile_height * tile_row);
                tile_painter.Draw(gfx::Rectangle(gfx::Rectangle::Style::Outline, 2.0f), model,
                                  gfx::CreateMaterialFromColor(gfx::Color::HotPink));

                const glm::vec2 corner = engine::ProjectPoint(tile_painter.GetProjMatrix(),
                                                         tile_painter.GetViewMatrix(),
                                                         painter.GetProjMatrix(),
                                                         painter.GetViewMatrix(),
                                                         glm::vec3 { layer_tile_width * (tile_col+1),
                                                                     layer_tile_height * (tile_row+1), 0.0f });

                ShowMessage(base::FormatString("%1, %2", tile_row, tile_col), corner + glm::vec2{10.0f, 10.0f}, painter);
            }
        }
    }

    if (mCurrentTool)
    {
        mCurrentTool->Render(scene_painter, tile_painter);
    }

    // a test case for mapping the corners of the tilemap from the isometric space
    // into our 2D axis aligned space. this is here simply because of convenience.
#if 0
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
            const auto size_factors = engine::GetTileCuboidFactors(mState.klass->GetPerspective());
            //const auto size_factors = glm::vec3 { 1.0f, 1.0f, 0.8994f };

            auto checkerboard = mState.workspace->GetMaterialClassById("_checkerboard");
            gfx::Transform model;
            model.Translate(0.5f, 0.5f, -0.5f);
            model.Scale(base_size * size_factors);
            tile_painter.Draw(gfx::Cube(), model, gfx::MaterialClassInst(checkerboard));
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
                const auto& p = engine::TilePlaneToScene(glm::vec4{point.x, point.y, point.z, 1.0f}, mState.klass->GetPerspective());
                gfx::Transform model;
                model.Scale(10.0f, 10.0f);
                model.Translate(p.x, p.y);
                model.Translate(-5.0f, -5.0f);
                //scene_painter.Draw(gfx::Circle(), model, gfx::CreateMaterialFromColor(gfx::Color::Green));
            }
        }
    }
#endif

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

    if (!mState.klass->GetNumLayers())
    {
        ShowMessage("Map has no layers to visualize.", painter);
        return;
    }

    PrintMousePos(mUI, mState, painter, perspective);
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

    if (!mCurrentTool && mickey->button() == Qt::LeftButton)
    {
        mState.selection.reset();
        if (auto* layer = GetCurrentLayerInstance())
        {
            mCurrentTool.reset(new TileSelectTool(*mState.map, *layer, mState));
        }
    }
    else if(!mCameraTool &&  (mickey->button() == Qt::RightButton))
    {
        mCameraTool.reset(new PerspectiveCorrectCameraTool(mUI, mState));
    }

    if (mCurrentTool && mickey->button() == Qt::LeftButton)
    {
        const auto* layer = GetCurrentLayerInstance();
        const auto* tool  = GetCurrentTool();
        if (!ValidateToolAgainstLayer(*tool, *layer))
        {
            mCurrentTool.reset();
            UncheckTools();
            return;
        }
        mCurrentTool->MousePress(mickey);
        UpdateLayerPalette();
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
            mCurrentTool.reset();
            DisplaySelection();
            DisplayLayerProperties();
            DisplayMapProperties();
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

}

void TilemapWidget::MouseWheel(QWheelEvent* wheel)
{
    if (!mCurrentTool)
        return;

    auto* layer = GetCurrentLayerInstance();
    if (!layer)
        return;

    // a bit of a kludge but the GfxWidget transforms Ctrl+mouse wheel to
    // zoom in/out events.
    if (wheel->modifiers() == Qt::ControlModifier)
        return;

    const QPoint& num_degrees = wheel->angleDelta() / 8;
    const QPoint& num_steps = num_degrees / 15;
    // only consider the wheel scroll steps on the vertical axis.
    // if steps are positive the wheel is scrolled away from the user
    // and if steps are negative the wheel is scrolled towards the user.
    const int num_vertical_steps = num_steps.y();

    size_t tool_index = 0;
    auto* tool = GetCurrentTool(&tool_index);
    ASSERT(tool);

    // scroll over the tools using the mouse wheel.
    if (wheel->modifiers() == Qt::ShiftModifier)
    {
        const int curr_tool_index = tool_index;
        const int next_tool_index = math::wrap(0, (int)(mTools.size() - 1), curr_tool_index - num_vertical_steps);
        StartTool(mTools[next_tool_index].id);
    }
    else
    {
        const auto& materials = mState.workspace->ListAllMaterials();
        int index;
        for (index=0; index<materials.size(); ++index)
        {
            if (materials[index].id == tool->material)
                break;
        }
        index = math::wrap(0, (int)materials.size()-1, index - num_vertical_steps);
        SetValue(mUI.cmbToolMaterial, ListItemId(materials[index].id));
        ModifyCurrentTool();
        StartTool(mTools[tool_index].id);
    }
}

void TilemapWidget::MouseZoom(std::function<void()> zoom_function)
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
    if (key->key() == Qt::Key_Escape)
    {
        OnEscape();
        return true;
    }
    else if (key->key() == Qt::Key_Delete)
        on_btnDeleteTileMaterial_clicked();
    else if (key->key() == Qt::Key_Space)
        return OpenMaterialPaletteOnCurrentTool();
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
    return false;
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

void TilemapWidget::GenerateTools()
{
    if (!mTools.empty())
        return;

    {
        Tool conf;
        conf.id     = app::RandomString();
        conf.tool   = ToolFunction::TileBrush;
        conf.shape  = ToolShape::Rectangle;
        conf.name   = "Brush 1";
        conf.width  = 1;
        conf.height = 1;
        conf.material = "_checkerboard";
        conf.palette_index = PaletteIndexAutomatic;
        mTools.push_back(conf);
    }

    {
        Tool conf;
        conf.id     = app::RandomString();
        conf.tool   = ToolFunction::TileBrush;
        conf.shape  = ToolShape::Rectangle;
        conf.name   = "Brush 2";
        conf.width  = 2;
        conf.height = 2;
        conf.material = "_checkerboard";
        conf.palette_index = PaletteIndexAutomatic;
        mTools.push_back(conf);
    }

    {
        Tool conf;
        conf.id     = app::RandomString();
        conf.tool   = ToolFunction::TileBrush;
        conf.shape  = ToolShape::Rectangle;
        conf.name   = "Brush 3";
        conf.width  = 4;
        conf.height = 4;
        conf.material = "_checkerboard";
        conf.palette_index = PaletteIndexAutomatic;
        mTools.push_back(conf);
    }

    {
        Tool conf;
        conf.id     = app::RandomString();
        conf.tool   = ToolFunction::TileBrush;
        conf.shape  = ToolShape::Rectangle;
        conf.name   = "Brush 4";
        conf.width  = 10;
        conf.height = 10;
        conf.material = "_checkerboard";
        conf.palette_index = PaletteIndexAutomatic;
        mTools.push_back(conf);
    }

    {
        Tool conf;
        conf.id     = app::RandomString();
        conf.tool   = ToolFunction::TileBrush;
        conf.shape  = ToolShape::Rectangle;
        conf.name   = "Brush 5";
        conf.width  = 20;
        conf.height = 20;
        conf.material = "_checkerboard";
        conf.palette_index = PaletteIndexAutomatic;
        mTools.push_back(conf);
    }

    {
        Tool conf;
        conf.id     = app::RandomString();
        conf.tool   = ToolFunction::TileBrush;
        conf.shape  = ToolShape::Rectangle;
        conf.name   = "Brush 6";
        conf.width  = 50;
        conf.height = 50;
        conf.material = "_checkerboard";
        conf.palette_index = PaletteIndexAutomatic;
        mTools.push_back(conf);
    }

    {
        Tool conf;
        conf.id     = app::RandomString();
        conf.tool   = ToolFunction::TileBrush;
        conf.shape  = ToolShape::Rectangle;
        conf.name   = "Brush 7";
        conf.width  = 100;
        conf.height = 100;
        conf.material = "_checkerboard";
        conf.palette_index = PaletteIndexAutomatic;
        mTools.push_back(conf);
    }

    {
        Tool conf;
        conf.id     = app::RandomString();
        conf.tool   = ToolFunction::TileBrush;
        conf.shape  = ToolShape::Rectangle;
        conf.name   = "Brush 8";
        conf.width  = 256;
        conf.height = 256;
        conf.material = "_checkerboard";
        conf.palette_index = PaletteIndexAutomatic;
        mTools.push_back(conf);
    }

    {
        Tool conf;
        conf.id     = app::RandomString();
        conf.tool   = ToolFunction::TileBrush;
        conf.shape  = ToolShape::Rectangle;
        conf.name   = "Brush 9";
        conf.width  = 512;
        conf.height = 512;
        conf.material = "_checkerboard";
        conf.palette_index = PaletteIndexAutomatic;
        mTools.push_back(conf);
    }

}
void TilemapWidget::UpdateToolCombo()
{
    std::vector<ListItem> items;
    for (const auto tool : mTools)
    {
        ListItem item;
        item.name = tool.name;
        item.id   = tool.id;
        items.push_back(std::move(item));
    }
    SetList(mUI.cmbTool, items);
}

void TilemapWidget::UpdateToolToolbar()
{
    for (auto* action : mToolActions)
    {
        delete action;
    }
    mToolActions.clear();

    const auto* layer = GetCurrentLayer();

    for (unsigned i=0; i<mTools.size(); ++i)
    {
        const auto& tool = mTools[i];
        QAction* action = new QAction(this);
        action->setText(tool.name);
        action->setData(tool.id);
        action->setCheckable(true);
        action->setChecked(false);
        action->setEnabled(layer != nullptr);
        if (i < 9)
            action->setShortcut(QKeySequence(Qt::CTRL | (Qt::Key_1 + i)));

        if (tool.tool == ToolFunction::TileBrush)
            action->setIcon(QIcon("icons:brush.png"));

        connect(action, &QAction::triggered,this, &TilemapWidget::StartToolAction);
        mToolActions.push_back(action);
    }

    emit RefreshActions();
}

void TilemapWidget::ModifyCurrentTool()
{
    if (auto* tool = GetCurrentTool())
    {
        tool->name           = (const QString&)GetValue(mUI.cmbTool);
        tool->tool           = GetValue(mUI.cmbToolFunction);
        tool->width          = GetValue(mUI.toolWidth);
        tool->height         = GetValue(mUI.toolHeight);
        tool->shape          = GetValue(mUI.cmbToolShape);
        tool->material       = GetItemId(mUI.cmbToolMaterial);
        tool->palette_index  = GetValue(mUI.toolPaletteIndex);
        tool->value          = GetValue(mUI.toolValue);
        tool->apply_material = GetValue(mUI.chkToolMaterial);
        tool->apply_value    = GetValue(mUI.chkToolValue);
    }
}

void TilemapWidget::UncheckTools()
{
    for (auto* action : mToolActions)
        action->setChecked(false);

    SetEnabled(mUI.actionPalette, false);
}

void TilemapWidget::ShowCurrentTool()
{
    if (auto* tool = GetCurrentTool())
    {
        SetEnabled(mUI.cmbTool,                  true);
        SetEnabled(mUI.cmbToolFunction,          true);
        SetEnabled(mUI.cmbToolShape,             true);
        SetEnabled(mUI.toolWidth,                true);
        SetEnabled(mUI.toolHeight,               true);
        SetEnabled(mUI.cmbToolMaterial,          true);
        SetEnabled(mUI.btnSelectToolMaterial,    true);
        SetEnabled(mUI.btnSetToolMaterialParams, false);
        SetEnabled(mUI.btnEditToolMaterial,      true);
        SetEnabled(mUI.chkToolMaterial,          true);
        SetEnabled(mUI.chkToolValue,             true);

        SetValue(mUI.cmbToolMaterial,  ListItemId(tool->material));
        SetValue(mUI.cmbToolFunction,  tool->tool);
        SetValue(mUI.cmbToolShape,     tool->shape);
        SetValue(mUI.toolWidth,        tool->width);
        SetValue(mUI.toolHeight,       tool->height);
        SetValue(mUI.toolPaletteIndex, tool->palette_index);
        SetValue(mUI.toolValue,        tool->value);
        SetValue(mUI.chkToolMaterial,  tool->apply_material);
        SetValue(mUI.chkToolValue,     tool->apply_value);

        if (mState.workspace->IsUserDefinedResource(tool->material))
            SetEnabled(mUI.btnEditToolMaterial, true);
        else SetEnabled(mUI.btnEditToolMaterial, false);
    }
    else
    {

        SetEnabled(mUI.cmbTool,                  false);
        SetEnabled(mUI.cmbToolFunction,          false);
        SetEnabled(mUI.cmbToolShape,             false);
        SetEnabled(mUI.toolWidth,                false);
        SetEnabled(mUI.toolHeight,               false);
        SetEnabled(mUI.cmbToolMaterial,          false);
        SetEnabled(mUI.btnSelectToolMaterial,    false);
        SetEnabled(mUI.btnSetToolMaterialParams, false);
        SetEnabled(mUI.btnEditToolMaterial,      false);
        SetEnabled(mUI.toolValue,                false);
        SetEnabled(mUI.chkToolMaterial,          false);
        SetEnabled(mUI.chkToolValue,             false);

        SetValue(mUI.cmbToolFunction, -1);
        SetValue(mUI.cmbToolShape,    -1);
        SetValue(mUI.toolWidth,        0);
        SetValue(mUI.toolHeight,       0);
        SetValue(mUI.cmbToolMaterial, -1);
        SetValue(mUI.toolValue,        0);
    }
}

TilemapWidget::Tool* TilemapWidget::GetCurrentTool(size_t* index)
{
    if (mTools.empty())
        return nullptr;

    size_t counter = 0;
    if (index == nullptr)
        index = &counter;

    for (*index=0; *index<mTools.size(); ++*index)
    {
        if (mTools[*index].id == GetItemId(mUI.cmbTool))
            return &mTools[*index];
    }
    BUG("No such tool was found.");
    return nullptr;
}

void TilemapWidget::SetCurrentTool(const QString& id)
{
    SetValue(mUI.cmbTool, ListItemId(id));
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

        auto* instance = GetCurrentLayerInstance();
        instance->SetFlags(layer->GetFlags());
        instance->FlushCache();
        instance->Save();
        instance->Load(mLayerData[layer->GetId()], 1024);
    }
}

bool TilemapWidget::ValidateToolAgainstLayer(const Tool& tool, const game::TilemapLayer& layer)
{
    if (!layer.HasRenderComponent())
        return true;

    if (tool.palette_index == PaletteIndexAutomatic)
    {
        const auto& klass = layer.GetClass();
        if (klass.FindMaterialIndex(tool.material) != 0xff)
            return true;
        if (klass.FindNextAvailableMaterialIndex() == 0xff)
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Warning);
            msg.setWindowTitle("Layer Palette is Full");
            msg.setText(app::toString("The material palette on current layer '%1' is full and no more materials can be added to it.\n\n"
                           "You can select a material index to overwrite manually in the tool setting.\n"
                           "Reusing a material index *will* overwrite that material.", klass.GetName()));
            msg.setStandardButtons(QMessageBox::Ok);
            msg.exec();
            return false;
        }
    }
    return true;
}

void TilemapWidget::ToolIntoJson(const Tool& tool, QJsonObject& json) const
{
    app::JsonWrite(json, "tool",           tool.tool);
    app::JsonWrite(json, "shape",          tool.shape);
    app::JsonWrite(json, "id",             tool.id);
    app::JsonWrite(json, "name",           tool.name);
    app::JsonWrite(json, "material",       tool.material);
    app::JsonWrite(json, "value",          tool.value);
    app::JsonWrite(json, "index",          tool.palette_index);
    app::JsonWrite(json, "width",          tool.width);
    app::JsonWrite(json, "height",         tool.height);
    app::JsonWrite(json, "apply_material", tool.apply_material);
    app::JsonWrite(json, "apply_value",    tool.apply_value);
}

void TilemapWidget::ToolFromJson(Tool& tool, const QJsonObject& json) const
{
    app::JsonReadSafe(json, "tool",           &tool.tool);
    app::JsonReadSafe(json, "shape",          &tool.shape);
    app::JsonReadSafe(json, "id",             &tool.id);
    app::JsonReadSafe(json, "name",           &tool.name);
    app::JsonReadSafe(json, "material",       &tool.material);
    app::JsonReadSafe(json, "value",          &tool.value);
    app::JsonReadSafe(json, "index",          &tool.palette_index);
    app::JsonReadSafe(json, "width",          &tool.width);
    app::JsonReadSafe(json, "height",         &tool.height);
    app::JsonReadSafe(json, "apply_material", &tool.apply_material);
    app::JsonReadSafe(json, "apply_value",    &tool.apply_value);
}

void TilemapWidget::ReplaceDeletedResources()
{
    for (auto& tool : mTools)
    {
        if (tool.material.empty())
            continue;
        if (mState.workspace->IsValidMaterial(tool.material))
            continue;
        tool.material = "_checkerboard";
        WARN("Tilemap brush tool material was reset to checkerboard. [tool='%1']", tool.name);
    }
    for (unsigned i=0; i<mState.klass->GetNumLayers(); ++i)
    {
        auto& layer = mState.klass->GetLayer(i);
        if (!layer.HasRenderComponent())
            continue;
        for (unsigned i=0; i<layer.GetMaxPaletteIndex(); ++i)
        {
            const auto& material = layer.GetPaletteMaterialId(i);
            if (material.empty())
                continue;
            if (mState.workspace->IsValidMaterial(material))
                continue;

            layer.SetPaletteMaterialId("_checkerboard", i);
            WARN("Tilemap layer palette material was reset to checkerboard. [layer='%1', index='%2']",
                 layer.GetName(), i);
        }
    }
}

void TilemapWidget::ClearUnusedPaletteEntries()
{
    auto* klass = GetCurrentLayer();
    auto* layer = GetCurrentLayerInstance();
    if (!layer || !klass || !klass->HasRenderComponent())
        return;

    std::set<uint8_t> indices;
    for (unsigned i=0; i<layer->GetMaxPaletteIndex(); ++i)
        indices.insert(i);

    for (unsigned row=0; row<layer->GetHeight(); ++row)
    {
        for (unsigned col=0; col<layer->GetWidth(); ++col)
        {
            uint8_t palette_index = 0;
            ASSERT(layer->GetTilePaletteIndex(&palette_index, row, col));
            indices.erase(palette_index);
        }
    }
    for (auto i: indices)
    {
        klass->ClearMaterialId(i);
    }
}

bool TilemapWidget::OpenMaterialPaletteOnCurrentTool()
{
    if (!mCurrentTool)
        return false;

    if (auto* tool = GetCurrentTool())
    {
        DlgMaterial dlg(this, mState.workspace, tool->material);
        dlg.SetPreviewScale(GetMaterialPreviewScale(*mState.klass));
        if (dlg.exec() == QDialog::Rejected)
            return false;
        SetValue(mUI.cmbToolMaterial, ListItemId(dlg.GetSelectedMaterialId()));
        ModifyCurrentTool();
        StartTool(tool->id);
        return true;
    }
    return false;
}

bool TilemapWidget::SelectLayerOnKey(unsigned int index)
{
    if (index < mState.klass->GetNumLayers())
    {
        SelectRow(mUI.layers, index);
        DisplayLayerProperties();
        return true;
    }
    return false;
}

} // namespace
