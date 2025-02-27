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

#include <functional>
#include <algorithm>

#include "graphics/drawing.h"
#include "graphics/material_instance.h"
#include "editor/app/eventlog.h"
#include "editor/gui/spritewidget.h"
#include "editor/gui/utility.h"

namespace {
    constexpr auto ScrollStepSize = 10.0f;
} // namespace

namespace gui
{
SpriteWidget::SpriteWidget(QWidget* parent) : QWidget(parent)
{
    DEBUG("Create SpriteWidget");

    mUI.setupUi(this);
    mUI.widget->onPaintScene = std::bind(&SpriteWidget::PaintScene, this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMousePress = std::bind(&SpriteWidget::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&SpriteWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onMouseMove = std::bind(&SpriteWidget::MouseMove, this, std::placeholders::_1);
}
SpriteWidget::~SpriteWidget()
{
    DEBUG("Destroy SpriteWidget");

    mUI.widget->dispose();
}
void SpriteWidget::Render()
{
    mUI.widget->triggerPaint();

}
void SpriteWidget::on_horizontalScrollBar_valueChanged(int)
{
    mTranslateX = mUI.horizontalScrollBar->value() * ScrollStepSize;
}

void SpriteWidget::PaintScene(gfx::Painter& painter, double dt)
{
    if (!mMaterial)
        return;

    const auto& type = mMaterial->GetType();
    if (type != gfx::MaterialClass::Type::Sprite)
        return;

    const auto& active_texture_map = mMaterial->GetActiveTextureMap();
    const auto* texture_map = mMaterial->FindTextureMapById(active_texture_map);
    if (texture_map == nullptr || texture_map->GetType() != gfx::TextureMap::Type::Sprite)
        return;

    const auto texture_count = texture_map->GetNumTextures();
    const auto frame_count = texture_map->GetSpriteFrameCount();
    if (texture_count == 0 || frame_count == 0)
        return;

    const auto widget_height = mUI.widget->height();
    const auto widget_width = mUI.widget->width();
    const auto rect_height = widget_height - 30.0;
    const auto rect_width = rect_height + 2.0f * 5.0f; // little bit of space between items
    const auto cycle_width = frame_count * rect_width;
    const auto duration = texture_map->GetSpriteCycleDuration();
    if (duration <= 0.0f)
        return;

    const auto duration_whole_seconds = (unsigned)std::ceil(duration);
    const auto duration_width = (duration_whole_seconds / duration) * cycle_width;
    const auto deci_second_width = duration_width / duration_whole_seconds / 10.0;
    const auto second_width = duration_width / duration_whole_seconds;

    {
        gfx::FPoint a(0.0f, 0.0f);
        a.Translate(10.0f, 20.0f);
        a.Translate(-mTranslateX, 0.0f);

        gfx::FPoint  b(0.0f, 0.0f);
        b.Translate(10.0f, 20.0f);
        b.Translate(-mTranslateX, 0.0f);
        b.Translate(duration_width, 0.0f);
        gfx::DebugDrawLine(painter, a, b, gfx::Color::Silver);

        auto VerticalLine = [&painter, this](float x, bool big) {
            gfx::FPoint a;
            a.Translate(10.0f, big ? 10.0f : 20.0f);
            a.Translate(-mTranslateX, 0.0f);
            a.Translate(x, 0.0f);

            gfx::FPoint b;
            b.Translate(10.0f, 30.0f);
            b.Translate(-mTranslateX, 0.0f);
            b.Translate(x, 0.0f);

            gfx::DebugDrawLine(painter, a, b, gfx::Color::Silver);
        };

        auto PrintText = [&painter, this](const std::string& str, float x) {
            gfx::FRect txt;
            txt.Resize(20.0f, 15.0f);
            txt.Translate(10.0f, 35.0f);
            txt.Translate(-mTranslateX, 0.0f);
            txt.Translate(x, 0.0f);
            txt.Translate(-10.0f, 0.0f);
            gfx::DrawTextRect(painter, str,"app://fonts/orbitron-light.otf", 12, txt,
                              gfx::Color::Silver, gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter);
        };

        for (unsigned second=0; second<duration_whole_seconds; ++second)
        {
            VerticalLine((second+0)*second_width, true);
            PrintText(base::FormatString("%1 s", second), second*second_width);

            for (unsigned deci=1; deci<10; ++deci)
            {
                const float x = second * second_width + deci * deci_second_width;
                VerticalLine(x, false);
            }
        }
        VerticalLine(duration_whole_seconds*second_width, true);
        PrintText(base::FormatString("%1 s", duration_whole_seconds), duration_whole_seconds*second_width);
    }

    if (const auto* sprite_sheet = texture_map->GetSpriteSheet())
    {
        const auto* texture_src = texture_map->GetTextureSource(0);
        const auto& texture_rect = texture_map->GetTextureRect(0);

        gfx::MaterialClass temp(gfx::MaterialClass::Type::Texture);
        temp.SetSurfaceType(mMaterial->GetSurfaceType());
        temp.SetTextureMinFilter(mMaterial->GetTextureMinFilter());
        temp.SetTextureMagFilter(mMaterial->GetTextureMagFilter());
        temp.SetBaseColor(mMaterial->GetBaseColor());
        temp.AddTexture(texture_src->Copy());

        for (unsigned row=0; row<sprite_sheet->rows; ++row)
        {
            for (unsigned col=0; col<sprite_sheet->cols; ++col)
            {
                const auto index = row * sprite_sheet->cols + col;
                const auto width = texture_rect.GetWidth() / sprite_sheet->cols;
                const auto height = texture_rect.GetHeight() / sprite_sheet->rows;

                gfx::FRect src;
                src.Resize(width, height);
                src.Translate(texture_rect.GetX(), texture_rect.GetY());
                src.Translate(col * width, row*height);
                temp.SetTextureRect(src);

                gfx::FRect rect;
                rect.Resize(rect_width, rect_height);
                rect.Translate(10.0f, 15.0f); // left margin, top margin
                rect.Translate(rect_width * index, 0.0);
                rect.Translate(5.0f, 0.0f); // little padding around each rect
                rect.Translate(-mTranslateX, 0.0); // scrolling
                gfx::FillRect(painter, rect, gfx::MaterialInstance(temp));
            }
        }
    }
    else
    {
        for (unsigned texture_index = 0; texture_index < texture_count; ++texture_index)
        {
            const auto* texture_src = texture_map->GetTextureSource(texture_index);
            const auto& texture_rect = texture_map->GetTextureRect(texture_index);

            gfx::MaterialClass temp(gfx::MaterialClass::Type::Texture);
            temp.SetSurfaceType(mMaterial->GetSurfaceType());
            temp.SetBaseColor(mMaterial->GetBaseColor());
            temp.SetTextureMinFilter(mMaterial->GetTextureMinFilter());
            temp.SetTextureMagFilter(mMaterial->GetTextureMagFilter());
            temp.AddTexture(texture_src->Copy());
            temp.SetTextureRect(texture_rect);

            gfx::FRect rect;
            rect.Resize(rect_width, rect_height);
            rect.Translate(10.0f, 15.0f); // left margin, top margin
            rect.Translate(rect_width * texture_index, 0.0);
            rect.Translate(5.0f, 0.0f); // little padding around each rect
            rect.Translate(-mTranslateX, 0.0); // scrolling
            gfx::FillRect(painter, rect, gfx::MaterialInstance(temp));
        }
    }

    if (mRenderTime)
    {
        const auto duration = texture_map->GetSpriteCycleDuration();
        if (duration > 0.0f)
        {
            const float t = mTime;
            const float phase = texture_map->IsSpriteLooping()
                                ? std::fmod(t, duration) / duration
                                : std::min(t / duration, 1.0f);
            const auto pos = phase * cycle_width;

            gfx::FPoint a;
            a.Translate(10.0f, 20.0f);
            a.Translate(-mTranslateX, 0.0f);
            a.Translate(pos, 0.0f);

            gfx::FPoint b;
            b.Translate(10.0f, widget_height - 15.0f);
            b.Translate(-mTranslateX, 0.0f);
            b.Translate(pos, 0.0f);

            gfx::DebugDrawLine(painter, a, b, gfx::Color::Silver, 2.0f);
        }
    }

    const auto cycle_render_width = 10.0f + duration_width + 10.0f;

    if (cycle_render_width > widget_width)
    {
        QSignalBlocker s(mUI.horizontalScrollBar);
        const auto horizontal_excess = cycle_render_width - widget_width;
        mUI.horizontalScrollBar->setMinimum(0);
        mUI.horizontalScrollBar->setMaximum(horizontal_excess / ScrollStepSize);
    }
    else
    {
        QSignalBlocker s(mUI.horizontalScrollBar);
        mUI.horizontalScrollBar->setRange(0, 0);
        mUI.horizontalScrollBar->setValue(0);
        mTranslateX = 0.0;
    }
}

void SpriteWidget::MousePress(QMouseEvent* mickey)
{
    if (mickey->button() == Qt::LeftButton)
    {
        mDragTime = true;
    }
}
void SpriteWidget::MouseRelease(QMouseEvent* mickey)
{
    mDragTime = false;
}

void SpriteWidget::MouseMove(QMouseEvent* mickey)
{
    if (!mDragTime)
        return;
}

} // namespace