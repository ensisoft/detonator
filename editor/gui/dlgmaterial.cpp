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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"

#include "warnpop.h"

#include "editor/app/workspace.h"
#include "editor/app/eventlog.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/material_instance.h"
#include "graphics/drawable.h"
#include "graphics/drawing.h"
#include "graphics/material_class.h"

namespace {
    constexpr unsigned BoxMargin = 20;
    inline unsigned GetBoxWidth(const glm::vec2& scale)
    { return 100  * scale.x; }
    inline unsigned GetBoxHeight(const glm::vec2& scale)
    { return 100 * scale.y;}

} // namespace

namespace gui
{

DlgMaterial::DlgMaterial(QWidget* parent, const app::Workspace* workspace, bool expand_maps)
  : QDialog(parent)
  , mWorkspace(workspace)
  , mPreviewScale(1.0f, 1.0f)
  , mExpandMaps(expand_maps)
{
    mUI.setupUi(this);

    setMouseTracking(true);
    // do the graphics dispose in finished handler which is triggered
    // regardless whether we do accept/reject or the user clicks the X
    // or presses Esc.
    connect(this, &QDialog::finished, mUI.widget, &GfxWidget::dispose);

    mUI.widget->onPaintScene = std::bind(&DlgMaterial::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onInitScene = [&](unsigned, unsigned) {
        mUI.widget->StartPaintTimer();
    };
    mUI.widget->onKeyPress = std::bind(&DlgMaterial::KeyPress, this, std::placeholders::_1);
    mUI.widget->onMousePress = std::bind(&DlgMaterial::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseWheel = std::bind(&DlgMaterial::MouseWheel, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&DlgMaterial::MouseDoubleClick, this, std::placeholders::_1);

    QByteArray geometry;
    if (mWorkspace->GetUserProperty("dlg_material_geometry", &geometry))
        this->restoreGeometry(geometry);

    ListMaterials("");
}

void DlgMaterial::SetSelectedMaterialId(const app::AnyString& id)
{
    mSelectedMaterialId = id;
}

void DlgMaterial::SetSelectedTextureMapId(const app::AnyString& id)
{
    mSelectedTextureMapId = id;
}

unsigned DlgMaterial::GetTileIndex() const
{
    return GetValue(mUI.tileIndex);
}

void DlgMaterial::SetTileIndex(unsigned index)
{
    SetValue(mUI.tileIndex, index);
}

void DlgMaterial::on_btnAccept_clicked()
{
    const_cast<app::Workspace*>(mWorkspace)->SetUserProperty("dlg_material_geometry", saveGeometry());

    // auto default bites again!
    if (mSelectedMaterialId.IsEmpty())
        reject();
    else  accept();
}
void DlgMaterial::on_btnCancel_clicked()
{
    const_cast<app::Workspace*>(mWorkspace)->SetUserProperty("dlg_material_geometry", saveGeometry());

    reject();
}

void DlgMaterial::on_vScroll_valueChanged()
{
    mScrollOffsetRow = mUI.vScroll->value();
}

void DlgMaterial::on_filter_textChanged(const QString& text)
{
    mScrollOffsetRow = 0;
    mNumVisibleRows  = 0;

    ListMaterials(text);

    mFirstPaint = true;

    for (size_t i=0; i<mMaterials.size(); ++i)
    {
        if (IsSelectedMaterial(i))
            return;
    }
    mSelectedMaterialId.Clear();
    mSelectedTextureMapId.Clear();
}

void DlgMaterial::PaintScene(gfx::Painter& painter, double dt)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    const auto BoxWidth = GetBoxWidth(mPreviewScale);
    const auto BoxHeight = GetBoxHeight(mPreviewScale);

    const auto num_visible_cols = width / (BoxWidth + BoxMargin);
    const auto num_visible_rows = height / (BoxHeight + BoxMargin);

    if (mFirstPaint)
    {
        if (!mSelectedMaterialId.IsEmpty())
        {
            size_t selected_material_row = 0;
            size_t selected_material_col = 0;

            for (size_t i=0; i<mMaterials.size(); ++i)
            {
                selected_material_col = i % num_visible_cols;
                selected_material_row = i / num_visible_cols;
                if (IsSelectedMaterial(i))
                    break;
            }
            const auto row_height = BoxHeight + BoxMargin;
            const auto row_ypos   = (selected_material_row + 1 ) * row_height;
            if (row_ypos > height)
            {
                mScrollOffsetRow = (row_ypos - height) / row_height + 1;
                QSignalBlocker s(mUI.vScroll);
                mUI.vScroll->setValue(mScrollOffsetRow);
            }
        }
        mFirstPaint = false;
    }

    const auto xoffset  = (width - ((BoxWidth + BoxMargin) * num_visible_cols)) / 2;
    const auto yoffset  = -int(mScrollOffsetRow) * (BoxHeight + BoxMargin);
    unsigned index = 0;

    SetValue(mUI.groupBox, "Material Library");

    for (index=0; index<mMaterials.size(); ++index)
    {
        auto klass = mMaterials[index].material;

        const auto col = index % num_visible_cols;
        const auto row = index / num_visible_cols;
        const auto xpos = xoffset + col * (BoxWidth + BoxMargin);
        const auto ypos = yoffset + row * (BoxHeight + BoxMargin);

        gfx::FRect  rect;
        rect.Resize(BoxWidth, BoxHeight);
        rect.Move(xpos, ypos);
        rect.Translate(BoxMargin*0.5f, BoxMargin*0.5f);
        if (!DoesIntersect(rect, gfx::FRect(0.0f, 0.0f, width, height)))
            continue;

        if (!base::Contains(mFailedMaterials, klass->GetId()))
        {
            gfx::MaterialInstance material(klass);
            material.SetRuntime(mUI.widget->GetTime());
            material.SetUniform("kTileIndex", (float) GetValue(mUI.tileIndex));
            material.SetUniform("active_texture_map", mMaterials[index].texture_map_id);
            gfx::FillRect(painter, rect, material);
            if (material.HasError())
            {
                mFailedMaterials.insert(klass->GetId());
            }
        }
        else
        {
            const auto [corner, _1, _2, _3] = rect.GetCorners();
            ShowError("Broken\nMaterial", corner, painter);
        }

        if (IsSelectedMaterial(index))
        {
            gfx::DrawRectOutline(painter, rect, gfx::Color::Green, 2.0f);
            SetValue(mUI.groupBox, app::toString("Material Library - %1", klass->GetName()));
        }
    }

    const auto num_total_rows = index / num_visible_cols + 1;
    if (num_total_rows > num_visible_rows)
    {
        const auto num_scroll_steps = num_total_rows - num_visible_rows;
        QSignalBlocker blocker(mUI.vScroll);
        mUI.vScroll->setVisible(true);
        mUI.vScroll->setMaximum(num_scroll_steps);
        if (num_visible_rows != mNumVisibleRows)
        {
            mUI.vScroll->setValue(0);
            mNumVisibleRows = num_visible_rows;
        }
    }
    else
    {
        mUI.vScroll->setVisible(false);
    }
}

void DlgMaterial::MousePress(QMouseEvent* mickey)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();

    const auto BoxWidth = GetBoxWidth(mPreviewScale);
    const auto BoxHeight = GetBoxHeight(mPreviewScale);

    const auto num_cols = width / (BoxWidth + BoxMargin);
    const auto xoffset  = (width - ((BoxWidth + BoxMargin) * num_cols)) / 2;
    const auto yoffset  = mScrollOffsetRow * (BoxHeight + BoxMargin);

    const auto widget_xpos = mickey->pos().x();
    const auto widget_ypos = mickey->pos().y();
    const auto col = (widget_xpos - xoffset) / (BoxWidth + BoxMargin);
    const auto row = (widget_ypos + yoffset) / (BoxHeight + BoxMargin);
    const auto index = row * num_cols + col;

    if (index >= mMaterials.size())
        return;
    mSelectedMaterialId = mMaterials[index].material_id;
    mSelectedTextureMapId = mMaterials[index].texture_map_id;
}

void DlgMaterial::MouseDoubleClick(QMouseEvent* mickey)
{
    const_cast<app::Workspace*>(mWorkspace)->SetUserProperty("dlg_material_geometry", saveGeometry());

    MousePress(mickey);
    accept();
}

void DlgMaterial::MouseWheel(QWheelEvent* wheel)
{
    const QPoint& num_degrees = wheel->angleDelta() / 8;
    const QPoint& num_steps = num_degrees / 15;
    // only consider the wheel scroll steps on the vertical
    // axis for zooming.
    // if steps are positive the wheel is scrolled away from the user
    // and if steps are negative the wheel is scrolled towards the user.
    const int num_zoom_steps = num_steps.y();

    unsigned max = mUI.vScroll->maximum();

    for (int i=0; i<std::abs(num_zoom_steps); ++i)
    {
        if (num_zoom_steps > 0)
            mScrollOffsetRow = mScrollOffsetRow > 0 ? mScrollOffsetRow - 1 : 0;
        else if (num_zoom_steps < 0)
            mScrollOffsetRow = mScrollOffsetRow < max ? mScrollOffsetRow + 1 : mScrollOffsetRow;
    }

    QSignalBlocker blocker(mUI.vScroll);
    mUI.vScroll->setValue(mScrollOffsetRow);
}

bool DlgMaterial::KeyPress(QKeyEvent* event)
{
    const auto key = event->key();

    if (key == Qt::Key_Escape)
    {
        reject();
        return true;
    }
    else if (key == Qt::Key_Return)
    {
        if (mSelectedMaterialId.IsEmpty())
            return false;
        accept();
        return true;
    }

    if (mMaterials.empty())
        return false;

    size_t index = 0;
    for (; index < mMaterials.size(); ++index)
    {
        if (IsSelectedMaterial(index))
            break;
    }
    if (index == mMaterials.size())
    {
        mScrollOffsetRow    = 0;
        mSelectedMaterialId = mMaterials[0].material_id;
        mSelectedTextureMapId = mMaterials[0].texture_map_id;
    }

    const auto BoxWidth  = GetBoxWidth(mPreviewScale);
    const auto BoxHeight = GetBoxHeight(mPreviewScale);

    const auto width    = mUI.widget->width();
    const auto height   = mUI.widget->height();
    const auto num_cols = width / (BoxWidth + BoxMargin);
    const auto num_rows = mMaterials.size() / num_cols;

    if (key == Qt::Key_Left)
    {
        if (index > 0)
            index--;
        else index = mMaterials.size()-1;
    }
    else if(key == Qt::Key_Right)
    {
        if (index < mMaterials.size() - 1)
            index++;
        else index = 0;
    }
    else if (key == Qt::Key_Down)
    {
        index += num_cols;
        if (index >= mMaterials.size())
            index = mMaterials.size() - 1;
    }
    else if (key == Qt::Key_Up)
    {
        index -= num_cols;
        if (index >= mMaterials.size())
            index = 0;
    }

    const auto visible_rows = height / (BoxHeight + BoxMargin);

    const auto row = index / num_cols;
    if (row < mScrollOffsetRow || row > mScrollOffsetRow + visible_rows)
        mScrollOffsetRow = row;

    ASSERT(index < mMaterials.size());
    mSelectedMaterialId = mMaterials[index].material_id;
    mSelectedTextureMapId = mMaterials[index].texture_map_id;
    return true;
}

void DlgMaterial::ListMaterials(const QString &filter_string)
{
    mMaterials.clear();

    const auto& resource_list = mWorkspace->ListAllMaterials();

    for (const auto& resource : resource_list)
    {
        if (!filter_string.isEmpty())
        {
            if (!resource.name.contains(filter_string, Qt::CaseInsensitive))
                continue;
        }

        auto klass = app::ResourceCast<gfx::MaterialClass>(*resource.resource).GetSharedResource();
        const auto type = klass->GetType();
        if (mExpandMaps && (type == gfx::MaterialClass::Type::Sprite || type == gfx::MaterialClass::Type::Texture))
        {
            for (unsigned i=0; i<klass->GetNumTextureMaps(); ++i)
            {
                const auto* map = klass->GetTextureMap(i);
                if (map == nullptr)
                    continue;
                Material m;
                m.material = klass;
                m.texture_map_id = map->GetId();
                m.material_id = klass->GetId();
                mMaterials.push_back(m);
            }
            DEBUG("homo");
        }
        else
        {
            Material m;
            m.material = klass;
            m.material_id = klass->GetId();
            mMaterials.push_back(m);
        }
    }
}
bool DlgMaterial::IsSelectedMaterial(size_t index) const
{
    ASSERT(index < mMaterials.size());
    if (mMaterials[index].material_id != mSelectedMaterialId)
        return false;
    if (mSelectedTextureMapId.IsEmpty())
        return true;

    return mMaterials[index].texture_map_id == mSelectedTextureMapId;
}

DlgTileChooser::DlgTileChooser(QWidget* parent, std::shared_ptr<const gfx::MaterialClass> klass) noexcept
  : QDialog(parent)
  , mMaterial(std::move(klass))
  , mPreviewScale(1.0f, 1.0)
{
    mUI.setupUi(this);
    SetVisible(mUI.filter, false);
    SetVisible(mUI.lblTileIndex, false);
    SetVisible(mUI.tileIndex, false);

    setMouseTracking(true);
    // do the graphics dispose in finished handler which is triggered
    // regardless whether we do accept/reject or the user clicks the X
    // or presses Esc.
    connect(this, &QDialog::finished, mUI.widget, &GfxWidget::dispose);

    mUI.widget->onPaintScene = std::bind(&DlgTileChooser::PaintScene, this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onKeyPress   = std::bind(&DlgTileChooser::KeyPress, this, std::placeholders::_1);
    mUI.widget->onMousePress = std::bind(&DlgTileChooser::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseWheel = std::bind(&DlgTileChooser::MouseWheel, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&DlgTileChooser::MouseDoubleClick, this, std::placeholders::_1);
    mUI.widget->onInitScene = [&](unsigned, unsigned) {
        mUI.widget->StartPaintTimer();

    };

    if (mMaterial->GetType() == gfx::MaterialClass::Type::Tilemap)
    {
        const auto& texture_map_id = mMaterial->GetActiveTextureMap();
        const auto* texture_map = mMaterial->FindTextureMapById(texture_map_id);
        if (!texture_map)
            return;

        if (!texture_map->GetNumTextures())
            return;

        const auto* texture_source = texture_map->GetTextureSource(0);
        const auto& texture_image = texture_source->GetData();
        if (!texture_image)
            return;

        const auto texture_width = texture_image->GetWidth();
        const auto texture_height = texture_image->GetHeight();
        if (texture_width == 0 || texture_height == 0)
            return;

        const auto tile_offset = mMaterial->GetTileOffset();
        const auto tile_padding = mMaterial->GetTilePadding();
        const auto tile_size = mMaterial->GetTileSize();

        const auto tile_width  = tile_size.x + 2 * tile_padding.x;
        const auto tile_height = tile_size.y + 2 * tile_padding.y;

        mTileRows = (texture_height - tile_offset.y) / tile_height;
        mTileCols = (texture_width - tile_offset.x) / tile_width;
    }

    SetValue(mUI.groupBox, app::toString("Tile Material '%1'", mMaterial->GetName()));

    QTimer::singleShot(100, this, [this]() {
        mUI.filter->clearFocus();
        mUI.widget->raise();
        mUI.widget->activateWindow();
        mUI.widget->setFocus();
    });
}

void DlgTileChooser::on_btnAccept_clicked()
{
    // auto default bites again!
    accept();
}
void DlgTileChooser::on_btnCancel_clicked()
{
    reject();
}

void DlgTileChooser::on_vScroll_valueChanged()
{
    mScrollOffsetRow = mUI.vScroll->value();
}

void DlgTileChooser::PaintScene(gfx::Painter& painter, double secs)
{

    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    const auto BoxWidth = GetBoxWidth(mPreviewScale);
    const auto BoxHeight = GetBoxHeight(mPreviewScale);

    const auto num_visible_cols = width / (BoxWidth + BoxMargin);
    const auto num_visible_rows = height / (BoxHeight + BoxMargin);

    const auto xoffset  = (width - ((BoxWidth + BoxMargin) * num_visible_cols)) / 2;
    const auto yoffset  = -int(mScrollOffsetRow) * (BoxHeight + BoxMargin);
    unsigned index = 0;

    const auto tile_count = mTileRows * mTileCols;

    for (index=0; index<tile_count; ++index)
    {
        const auto col = index % num_visible_cols;
        const auto row = index / num_visible_cols;
        const auto xpos = xoffset + col * (BoxWidth + BoxMargin);
        const auto ypos = yoffset + row * (BoxHeight + BoxMargin);

        gfx::FRect  rect;
        rect.Resize(BoxWidth, BoxHeight);
        rect.Move(xpos, ypos);
        rect.Translate(BoxMargin*0.5f, BoxMargin*0.5f);
        if (!DoesIntersect(rect, gfx::FRect(0.0f, 0.0f, width, height)))
            continue;

        gfx::MaterialInstance material(mMaterial);
        material.SetRuntime(mUI.widget->GetTime());
        material.SetUniform("kTileIndex", (float)index);
        gfx::FillRect(painter, rect, material);

        if (index != mTileIndex)
            continue;

        gfx::DrawRectOutline(painter, rect, gfx::Color::Green, 2.0f);
    }

    const auto num_total_rows = index / num_visible_cols + 1;
    if (num_total_rows > num_visible_rows)
    {
        const auto num_scroll_steps = num_total_rows - num_visible_rows;
        QSignalBlocker blocker(mUI.vScroll);
        mUI.vScroll->setVisible(true);
        mUI.vScroll->setMaximum(num_scroll_steps);
        if (num_visible_rows != mNumVisibleRows)
        {
            mUI.vScroll->setValue(0);
            mNumVisibleRows = num_visible_rows;
        }
    }
    else
    {
        mUI.vScroll->setVisible(false);
    }
}

void DlgTileChooser::MousePress(QMouseEvent* mickey)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();

    const auto BoxWidth = GetBoxWidth(mPreviewScale);
    const auto BoxHeight = GetBoxHeight(mPreviewScale);

    const auto num_cols = width / (BoxWidth + BoxMargin);
    const auto xoffset  = (width - ((BoxWidth + BoxMargin) * num_cols)) / 2;
    const auto yoffset  = mScrollOffsetRow * (BoxHeight + BoxMargin);

    const auto widget_xpos = mickey->pos().x();
    const auto widget_ypos = mickey->pos().y();
    const auto col = (widget_xpos - xoffset) / (BoxWidth + BoxMargin);
    const auto row = (widget_ypos + yoffset) / (BoxHeight + BoxMargin);
    const auto index = row * num_cols + col;

    const auto tile_count = mTileRows * mTileCols;

    if (index >= tile_count)
        return;

    mTileIndex = index;
}

void DlgTileChooser::MouseDoubleClick(QMouseEvent* mickey)
{
    MousePress(mickey);
    accept();
}

void DlgTileChooser::MouseWheel(QWheelEvent* wheel)
{
    const QPoint& num_degrees = wheel->angleDelta() / 8;
    const QPoint& num_steps = num_degrees / 15;
    // only consider the wheel scroll steps on the vertical
    // axis for zooming.
    // if steps are positive the wheel is scrolled away from the user
    // and if steps are negative the wheel is scrolled towards the user.
    const int num_zoom_steps = num_steps.y();

    unsigned max = mUI.vScroll->maximum();

    for (int i=0; i<std::abs(num_zoom_steps); ++i)
    {
        if (num_zoom_steps > 0)
            mScrollOffsetRow = mScrollOffsetRow > 0 ? mScrollOffsetRow - 1 : 0;
        else if (num_zoom_steps < 0)
            mScrollOffsetRow = mScrollOffsetRow < max ? mScrollOffsetRow + 1 : mScrollOffsetRow;
    }

    QSignalBlocker blocker(mUI.vScroll);
    mUI.vScroll->setValue(mScrollOffsetRow);
}

bool DlgTileChooser::KeyPress(QKeyEvent* event)
{
    const auto key = event->key();

    if (key == Qt::Key_Escape)
    {
        reject();
        return true;
    }
    else if (key == Qt::Key_Return)
    {
        accept();
        return true;
    }

    const auto tile_count = mTileRows * mTileCols;
    if (tile_count == 0)
        return true;

    auto index = mTileIndex;

    if (index == tile_count)
        mScrollOffsetRow = 0;

    const auto BoxWidth  = GetBoxWidth(mPreviewScale);
    const auto BoxHeight = GetBoxHeight(mPreviewScale);

    const auto width    = mUI.widget->width();
    const auto height   = mUI.widget->height();
    const auto num_cols = width / (BoxWidth + BoxMargin);
    const auto num_rows = tile_count / num_cols;

    if (key == Qt::Key_Left)
    {
        if (index > 0)
            index--;
        else index = tile_count-1;
    }
    else if(key == Qt::Key_Right)
    {
        if (index < tile_count - 1)
            index++;
        else index = 0;
    }
    else if (key == Qt::Key_Down)
    {
        index += num_cols;
        if (index >= tile_count)
            index = tile_count - 1;
    }
    else if (key == Qt::Key_Up)
    {
        index -= num_cols;
        if (index >= tile_count)
            index = 0;
    }

    const auto visible_rows = height / (BoxHeight + BoxMargin);

    const auto row = index / num_cols;
    if (row < mScrollOffsetRow || row > mScrollOffsetRow + visible_rows)
        mScrollOffsetRow = row;

    ASSERT(index < tile_count);
    mTileIndex = index;
    return true;
}


} // namespace
