// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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
#  include <QKeyEvent>
#  include <QWheelEvent>
#  include <QKeyEvent>
#include "warnpop.h"

#include "base/format.h"
#include "base/math.h"
#include "base/utility.h"
#include "graphics/painter.h"
#include "graphics/paint_context.h"
#include "graphics/drawing.h"
#include "graphics/bitmap_generator.h"
#include "graphics/texture_file_source.h"
#include "graphics/texture_text_buffer_source.h"
#include "graphics/texture_bitmap_generator_source.h"
#include "editor/app/eventlog.h"
#include "editor/app/resource-uri.h"
#include "editor/gui/utility.h"
#include "editor/gui/translation.h"
#include "editor/gui/materialmapwidget.h"

namespace  {
std::string GetFontName(std::string font_uri)
{
    font_uri = base::ReplaceAll(font_uri, '\\', '/');
    const auto font_uri_list = base::SplitString(font_uri, '/');
    if (font_uri_list.empty())
        return "Font Name";
    return font_uri_list.back();
}

std::string DescribeTextureSource(const gfx::TextureSource* src)
{
    if (const auto* ptr = dynamic_cast<const gfx::TextureFileSource*>(src))
    {
        return ptr->GetFilename();
    }
    if (const auto* ptr = dynamic_cast<const gfx::TextureBitmapGeneratorSource*>(src))
    {
        const auto& generator = ptr->GetGenerator();
        const auto function = generator.GetFunction();
        const auto width = generator.GetWidth();
        const auto height = generator.GetHeight();
        return base::FormatString("8bit %1 alpha mask %2x%3 px", function, width, height);
    }
    if (const auto* ptr = dynamic_cast<const gfx::TextureTextBufferSource*>(src))
    {
        const auto& text_buffer = ptr->GetTextBuffer();
        const auto raster_format = text_buffer.GetRasterFormat();
        const auto& text_block = text_buffer.GetText();
        const auto font_size = text_block.fontsize;
        const auto font_name = GetFontName(text_block.font);
        if (raster_format == gfx::TextBuffer::RasterFormat::Bitmap)
            return base::FormatString("Freetype Text, %1, %2px", font_name, font_size);
        if (raster_format == gfx::TextBuffer::RasterFormat::Texture)
            return base::FormatString("Bitmap Text, %1, %2px", font_name, font_size);
        base::FormatString("Text, %1, %2", font_name, font_size);
    }
    return base::FormatString("%1, %2", TranslateEnum(src->GetSourceType()), src->GetName());
}
} //

namespace gui
{

MaterialMapWidget::MaterialMapWidget(QWidget* parent)
    : QWidget(parent)
{
    DEBUG("Create MaterialMapWidget");

    mUI.setupUi(this);
    mUI.widget->onPaintScene = std::bind(&MaterialMapWidget::PaintScene, this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMousePress = std::bind(&MaterialMapWidget::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&MaterialMapWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onMouseMove = std::bind(&MaterialMapWidget::MouseMove, this, std::placeholders::_1);
    mUI.widget->onMouseWheel = std::bind(&MaterialMapWidget::MouseWheel, this, std::placeholders::_1);
    mUI.widget->onKeyPress = std::bind(&MaterialMapWidget::KeyPress, this, std::placeholders::_1);

    mUI.widget->DrawFocusRect(false);

    mTextureItemSize = 40.0f;
    mScrollStepSize = 20;
}

MaterialMapWidget::~MaterialMapWidget()
{
    DEBUG("Destroy MaterialMapWidget");

    mUI.widget->Dispose();
}

void MaterialMapWidget::Update()
{
    for (unsigned i=0; i<mMaterial->GetNumTextureMaps(); ++i)
    {
        const auto* map = mMaterial->GetTextureMap(i);
        if (!base::Contains(mMaterialMapStates, map->GetId()))
        {
            MaterialMapState state;
            state.expanded = true;
            mMaterialMapStates[map->GetId()] = state;
        }
    }
    for (auto it = mMaterialMapStates.begin(); it != mMaterialMapStates.end();)
    {
        const auto id = it->first;
        if (mMaterial->FindTextureMapById(id))
            it = ++it;
        else it = mMaterialMapStates.erase(it);
    }
}

void MaterialMapWidget::OpenContextMenu(const QPoint& point, GfxMenu menu)
{
    mUI.widget->OpenContextMenu(point, std::move(menu));
}

void MaterialMapWidget::CollapseAll()
{
    for (auto& [id, state] : mMaterialMapStates)
    {
        state.expanded = false;
    }
}
void MaterialMapWidget::ExpandAll()
{
    for (auto& [id, state] : mMaterialMapStates)
    {
        state.expanded = true;
    }
}

void MaterialMapWidget::Render() const
{
    mUI.widget->TriggerPaint();
}

void MaterialMapWidget::on_verticalScrollBar_valueChanged(int)
{
    mVerticalScroll = mUI.verticalScrollBar->value() * mScrollStepSize;
}

void MaterialMapWidget::PaintScene(gfx::Painter& painter, double dt)
{
    // we're going to use this paint context here to capture errors and
    // simply discard them with the assumption that the material widget
    // itself will report errors to the user.
    gfx::PaintContext pc;

    const auto widget_width = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    const auto paint_width = widget_width; // - 4.0f;
    const auto paint_height = widget_height; // - 4.0f;
    const bool under_mouse = UnderMouse();

    mPalette = QApplication::palette();

    painter.ClearColor(CreateColor(QPalette::Window));

    gfx::Transform view;
    view.Translate(0.0f, -1.0f * mVerticalScroll);
    painter.SetViewMatrix(view);

    gfx::FPoint item_point;
    item_point.SetX(0.0f);
    item_point.SetY(0.0f);

    constexpr auto HeaderSize = 60.0f;
    constexpr auto HeaderIconSize = 50.0f;
    constexpr auto TextureIconSize = 30.0f;

    for (unsigned i=0; i<mMaterial->GetNumTextureMaps(); ++i)
    {
        const auto* texture_map = mMaterial->GetTextureMap(i);
        const auto texture_map_type = texture_map->GetType();
        const auto texture_map_name = texture_map->GetName();
        auto& state = mMaterialMapStates[texture_map->GetId()];

        // draw the map header
        {
            gfx::FRect header_area;
            header_area.Move(item_point);
            header_area.Resize(paint_width, HeaderSize);
            const auto icon_area = header_area.SubRect(0.0f, 0.0f, HeaderSize, HeaderSize);
            const auto text_area = header_area.SubRect(HeaderSize, 0.0f);
            const auto [text_top_rect, text_bottom_rect] = text_area.SplitVertically();
            const auto icon_rect = base::CenterRectOnRect(icon_area, gfx::FRect(0.0f, 0.0f, HeaderIconSize, HeaderIconSize));

            gfx::FRect button_rect;
            button_rect.Resize(20.0f, 20.0f);
            button_rect.Translate(item_point);
            button_rect.Translate(paint_width-20.0f, HeaderSize*0.5f);
            button_rect.Translate(-10.0f, -10.0f);

            if (mSelectedTextureMapId == texture_map->GetId())
            {
                gfx::FillRect(painter, header_area, CreateColor(QPalette::AlternateBase));
                gfx::DrawRectOutline(painter, header_area, CreateColor(QPalette::Highlight));
            }
            else if (under_mouse && header_area.TestPoint(mCurrentMousePos))
            {
                gfx::FillRect(painter, header_area, CreateColor(QPalette::AlternateBase));
            }

            if (texture_map->GetNumTextures() == 0)
            {
                 gfx::DrawImage(painter, icon_rect, res::Checkerboard, gfx::BlendMode::Opaque);
            }
            else
            {
                gfx::MaterialInstance instance(mMaterial);
                instance.SetActiveTextureMap(texture_map->GetId());
                instance.SetRuntime(mCurrentTime);
                instance.SetFirstRender(false);
                gfx::FillRect(painter, icon_rect, instance);
            }

            std::string texture_map_desc;
            if (texture_map_type == gfx::TextureMap::Type::Sprite)
            {
                const auto fps = texture_map->GetSpriteFrameRate();
                const auto srcs = texture_map->GetNumTextures();
                texture_map_desc = base::FormatString("%1, %2 texture(s), %3 FPS",
                        TranslateEnum(texture_map_type), srcs, base::fmt::Float { fps });
            }
            else if (texture_map_type == gfx::TextureMap::Type::Texture2D)
            {
                const auto srcs = texture_map->GetNumTextures();
                texture_map_desc = base::FormatString("%1 %2 texture(s)",
                    texture_map_type, srcs);
            } else BUG("Bug on texture map type.");

            gfx::DrawTextRect(painter, texture_map_name,
                "app://fonts/OpenSans-Regular.ttf", 20, text_top_rect,
                CreateColor(QPalette::Text),
                gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
            gfx::DrawTextRect(painter, texture_map_desc,
                "app://fonts/OpenSans-Regular.ttf", 14, text_bottom_rect,
                CreateColor(QPalette::Text),
                gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);

            if (under_mouse && button_rect.TestPoint(mCurrentMousePos))
                gfx::DrawRectOutline(painter, button_rect, CreateColor(QPalette::Midlight));
            if (state.expanded)
            {
                gfx::DrawButtonIcon(painter, button_rect,
                    CreateColor(QPalette::Text), gfx::ButtonIcon::ArrowDown);
            }
            else
            {
                gfx::DrawButtonIcon(painter, button_rect,
                    CreateColor(QPalette::Text), gfx::ButtonIcon::ArrowRight);
            }
            state.header_rect = header_area;
            state.button_rect = button_rect;
        }

        item_point.Translate(0.0f, HeaderSize);

        if (!state.expanded)
            continue;

        gfx::FRect list_items_rect;
        for (unsigned i=0; i<texture_map->GetNumTextures(); ++i)
        {
            gfx::FRect item_area;
            item_area.Move(item_point);
            item_area.Resize(paint_width, mTextureItemSize);

            const auto icon_area = item_area.SubRect(0.0f, 0.0f, HeaderSize, mTextureItemSize);
            const auto text_area = item_area.SubRect(HeaderSize, 0.0f);
            const auto icon_rect = base::CenterRectOnRect(icon_area, gfx::FRect(0.0f, 0.0f, TextureIconSize, TextureIconSize));

            const auto* texture_source = texture_map->GetTextureSource(i);
            const auto& texture_rect   = texture_map->GetTextureRect(i);
            if (mSelectedTextureSrcId == texture_source->GetId())
            {
                gfx::FillRect(painter, item_area, CreateColor(QPalette::AlternateBase));
                gfx::DrawRectOutline(painter, item_area, CreateColor(QPalette::Highlight));
            }
            else if (under_mouse && item_area.TestPoint(mCurrentMousePos))
            {
                gfx::FillRect(painter, item_area, CreateColor(QPalette::AlternateBase));
            }

            if (!gfx::DrawTextureSource(painter, icon_rect, *mMaterial, *texture_source, texture_rect))
            {
                gfx::DrawImage(painter, icon_rect, res::Checkerboard, gfx::BlendMode::Opaque);
            }

            gfx::DrawTextRect(painter, DescribeTextureSource(texture_source),
                "app://fonts/OpenSans-Regular.ttf", 14, text_area,
                CreateColor(QPalette::Text),
                gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);

            item_point.Translate(0.0f, mTextureItemSize);
            list_items_rect = base::Union(list_items_rect, item_area);
        }
        state.list_rect = list_items_rect;
    }

    // started at zero, accumulated until here.
    const auto total_height = static_cast<unsigned>(item_point.GetY());
    ComputeScrollBars(0, total_height);

    mCurrentTime += dt;
    if (mCurrentTime > 5.0f)
        mCurrentTime = mCurrentTime - 5.0f;
}

void MaterialMapWidget::MousePress(const QMouseEvent* mickey)
{
    const auto button = mickey->button();
    if (button != Qt::MouseButton::LeftButton)
        return;

    for (unsigned i=0; i<mMaterial->GetNumTextureMaps(); ++i)
    {
        const auto* map = mMaterial->GetTextureMap(i);
        auto& state = mMaterialMapStates[map->GetId()];
        if (state.header_rect.TestPoint(mCurrentMousePos))
        {
            if (mSelectedTextureMapId == map->GetId())
            {
                state.expanded = !state.expanded;
            }
            else
            {
                mSelectedTextureMapId = map->GetId();
                mSelectedTextureSrcId.clear();
                emit SelectionChanged();
            }
            return;
        }
        if (!state.expanded)
            continue;

        if (state.list_rect.TestPoint(mCurrentMousePos))
        {
            const auto point = mCurrentMousePos - state.list_rect.GetPosition();
            const auto index = static_cast<unsigned>(point.GetY() / mTextureItemSize);
            if (index < map->GetNumTextures())
            {
                const auto& src = map->GetTextureSource(index);
                if (mSelectedTextureSrcId != src->GetId())
                {
                    mSelectedTextureMapId.clear();
                    mSelectedTextureSrcId = src->GetId();
                    emit SelectionChanged();
                }
                return;
            }
        }
    }
    ClearSelection();
    emit SelectionChanged();
}
void MaterialMapWidget::MouseRelease(const QMouseEvent* mickey)
{
    const auto button = mickey->button();
    if (button == Qt::MouseButton::RightButton)
    {
        emit CustomContextMenuRequested(mickey->pos());
    }
}
void MaterialMapWidget::MouseMove(const QMouseEvent* mickey)
{
    const auto x = mickey->pos().x();
    const auto y = mickey->pos().y();
    mCurrentMousePos = gfx::FPoint(x, y + mVerticalScroll);
}

void MaterialMapWidget::MouseWheel(const QWheelEvent* wheel)
{
    const QPoint& num_degrees = wheel->angleDelta() / 8;
    const QPoint& num_steps = num_degrees / 15;

    const int current_scroll_step = mUI.verticalScrollBar->value();
    const int maximal_scroll_step = mUI.verticalScrollBar->maximum();
    const int next_scroll_step = math::clamp(0, maximal_scroll_step,
        current_scroll_step - num_steps.y());

    QSignalBlocker blocker(mUI.verticalScrollBar);
    mUI.verticalScrollBar->setValue(next_scroll_step);

    mVerticalScroll = next_scroll_step * mScrollStepSize;
}

bool MaterialMapWidget::KeyPress(const QKeyEvent* event)
{
    enum class ItemType {
        TextureMap, TextureSrc
    };

    struct Item {
        ItemType type;
        std::string id;
    };
    std::vector<Item> items;
    std::size_t current_index = 0xffffff;

    for (unsigned i=0; i<mMaterial->GetNumTextureMaps(); ++i)
    {
        const auto* map = mMaterial->GetTextureMap(i);
        if (mSelectedTextureMapId == map->GetId())
            current_index = items.size();

        Item map_item;
        map_item.id = map->GetId();
        map_item.type = ItemType::TextureMap;
        items.push_back(map_item);

        const auto& state = mMaterialMapStates[map->GetId()];
        if (!state.expanded)
            continue;

        for (unsigned j=0; j<map->GetNumTextures(); ++j)
        {
            const auto* src = map->GetTextureSource(j);
            if (mSelectedTextureSrcId == src->GetId())
                current_index = items.size();
            Item src_item;
            src_item.id = src->GetId();
            src_item.type = ItemType::TextureSrc;
            items.push_back(src_item);
        }
    }
    if (items.empty())
        return false;

    auto SelectItem = [this, items](size_t index) {
        if (items[index].type == ItemType::TextureMap) {
            mSelectedTextureMapId = items[index].id;
            mSelectedTextureSrcId.clear();
        } else if (items[index].type == ItemType::TextureSrc) {
            mSelectedTextureMapId.clear();
            mSelectedTextureSrcId = items[index].id;
        }
    };

    if (current_index >= items.size())
    {
        SelectItem(0);
        return true;
    }

    const auto key = event->key();
    if (key == Qt::Key_Down)
    {
        SelectItem((current_index + 1) % items.size());
        emit SelectionChanged();
    }
    else if (key == Qt::Key_Up)
    {
        SelectItem(current_index > 0 ? current_index - 1 : items.size()-1);
        emit SelectionChanged();
    }
    else if (key == Qt::Key_Right)
    {
        if (items[current_index].type == ItemType::TextureMap)
            mMaterialMapStates[items[current_index].id].expanded = true;
    }
    else if (key == Qt::Key_Left)
    {
        if (items[current_index].type == ItemType::TextureMap)
            mMaterialMapStates[items[current_index].id].expanded = false;
    }
    else return false;

    return true;
}

void MaterialMapWidget::ComputeScrollBars(unsigned render_width, unsigned render_height)
{
    const auto widget_width  = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    const auto good_vertical   = widget_height == mPreviousWidgetHeight && render_height == mPreviousRenderHeight;
    const auto good_horizontal = widget_width == mPreviousWidgetWidth && render_width == mPreviousRenderWidth;
    if (good_vertical && good_horizontal)
        return;

    //DEBUG("Recompute scrollbars. render_height=%1, widget_height=%2",
    //    render_height, widget_height);

    QSignalBlocker s(mUI.verticalScrollBar);
    if (render_height > widget_height)
    {
        const auto vertical_excess = render_height - widget_height;
        // add +1 step to make sure that if the vertical height that
        // needs to be scrolled isn't an exact multiple of the of scroll
        // step height we can still scroll enough to fully cover the
        // last item and not have it it clipped.
        const float max_scroll_steps = vertical_excess / mScrollStepSize + 1;
        const auto scroll_step = std::min(mVerticalScroll / mScrollStepSize, max_scroll_steps);
        mUI.verticalScrollBar->setMaximum(max_scroll_steps);
        mUI.verticalScrollBar->setRange(0, max_scroll_steps);
        mUI.verticalScrollBar->setSingleStep(1);
        mUI.verticalScrollBar->setValue(scroll_step);
        mVerticalScroll = scroll_step * mScrollStepSize;
    }
    else
    {
        mUI.verticalScrollBar->setRange(0, 0);
        mUI.verticalScrollBar->setValue(0);
        mVerticalScroll = 0.0f;
    }

    mPreviousRenderWidth = render_width;
    mPreviousRenderHeight = render_height;
    mPreviousWidgetWidth = widget_width;
    mPreviousWidgetHeight = widget_height;
}

bool MaterialMapWidget::UnderMouse() const
{
    const auto pos = mUI.widget->mapFromGlobal(QCursor::pos());
    const auto x = pos.x();
    const auto y = pos.y();
    if (x < 0 || x > mUI.widget->width())
        return false;
    if (y < 0 || y > mUI.widget->height())
        return false;

    return true;
}


gfx::MaterialInstance MaterialMapWidget::CreateMaterial(QPalette::ColorRole role, QPalette::ColorGroup group) const
{
    return gfx::CreateMaterialFromColor(ToGfx(mPalette.color(group, role)));
}
gfx::Color4f MaterialMapWidget::CreateColor(QPalette::ColorRole role, QPalette::ColorGroup group) const
{
    return ToGfx(mPalette.color(group, role));
}

} // namespace
