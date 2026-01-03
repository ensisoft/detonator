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

#include "base/math.h"
#include "graphics/drawing.h"
#include "graphics/material_instance.h"
#include "graphics/texture_file_source.h"
#include "editor/app/eventlog.h"
#include "editor/gui/spritewidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"

namespace {
    constexpr auto ScrollStepSize = 10.0f;

    auto MakeDemoMaterial()
    {
        static std::shared_ptr<gfx::SpriteClass> demo_material;
        if (demo_material)
            return demo_material;

        gfx::TextureMap map;
        map.SetType(gfx::TextureMap::Type::Sprite);
        map.SetName("Sample");
        map.SetSpriteFrameRate(10.0f);

        map.SetNumTextures(8);
        map.SetTextureSource(0, gfx::LoadTextureFromFile("app://textures/editor/sprite-demo/frame-1.png"));
        map.SetTextureSource(1, gfx::LoadTextureFromFile("app://textures/editor/sprite-demo/frame-2.png"));
        map.SetTextureSource(2, gfx::LoadTextureFromFile("app://textures/editor/sprite-demo/frame-3.png"));
        map.SetTextureSource(3, gfx::LoadTextureFromFile("app://textures/editor/sprite-demo/frame-4.png"));
        map.SetTextureSource(4, gfx::LoadTextureFromFile("app://textures/editor/sprite-demo/frame-5.png"));
        map.SetTextureSource(5, gfx::LoadTextureFromFile("app://textures/editor/sprite-demo/frame-6.png"));
        map.SetTextureSource(6, gfx::LoadTextureFromFile("app://textures/editor/sprite-demo/frame-7.png"));
        map.SetTextureSource(7, gfx::LoadTextureFromFile("app://textures/editor/sprite-demo/frame-8.png"));

        demo_material = std::make_shared<gfx::MaterialClass>(gfx::MaterialClass::Type::Sprite);
        demo_material->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        demo_material->SetBaseColor(gfx::Color4f(gfx::Color::LightGray, 0.46f));
        demo_material->SetNumTextureMaps(1);
        demo_material->SetActiveTextureMap(map.GetId());
        demo_material->SetTextureMap(0, std::move(map));
        return demo_material;
    }

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

    mUI.widget->Dispose();
}
void SpriteWidget::Render()
{
    mUI.widget->TriggerPaint();

}
void SpriteWidget::on_horizontalScrollBar_valueChanged(int)
{
    mTranslateX = mUI.horizontalScrollBar->value() * ScrollStepSize;
}

void SpriteWidget::PaintScene(gfx::Painter& painter, double dt)
{
    if (!mEnablePaint)
        return;

    if (mMaterial && mMaterial->GetType() == gfx::MaterialClass::Type::Sprite)
        PaintSprite(mMaterial.get(), painter, dt);
    else if (mMaterial && mMaterial->GetType() == gfx::MaterialClass::Type::Texture)
        PaintTexture(mMaterial.get(), painter, dt);
    else if (mMaterial && mMaterial->GetType() == gfx::MaterialClass::Type::Particle2D)
        PaintParticle(painter, dt);
    else
    {
        auto material = MakeDemoMaterial();
        PaintSprite(material.get(), painter, dt);

        const auto widget_height = mUI.widget->height();
        const auto widget_width = mUI.widget->width();
        ShowInstruction("Sprite frames + timeline", Rect2Df(0.0f, 0.0f, widget_width, widget_height), painter);
    }
}

void SpriteWidget::PaintTexture(const gfx::MaterialClass *klass, gfx::Painter &painter, double dt)
{
    const auto widget_height = mUI.widget->height();
    const auto widget_width = mUI.widget->width();
    const auto rect_height = widget_height - 30.0;
    const auto rect_width = rect_height + 2.0f * 5.0f; // little bit of space between items
    const auto texture_count = klass->GetNumTextureMaps();

    if (texture_count == 0)
    {
        ShowInstruction("Sprite has no texture maps.", Rect2Df(0.0f, 0.0f, widget_width, widget_height), painter);
        return;
    }

    for (unsigned texture_index=0; texture_index<texture_count; ++texture_index)
    {
        gfx::FRect rect;
        rect.Resize(rect_width, rect_height);
        rect.Translate(10.0f, 15.0f); // left margin, top margin
        rect.Translate(rect_width * texture_index, 0.0);
        rect.Translate(5.0f, 0.0f); // little padding around each rect
        rect.Translate(-mTranslateX, 0.0); // scrolling

        const auto* texture_map = klass->GetTextureMap(texture_index);
        if (!texture_map || texture_map->GetNumTextures() == 0)
        {
            gfx::DrawTextRect(painter, "Missing\nTexture", "app://fonts/orbitron-medium.otf", 14, rect,
                  gfx::Color::HotPink,
                  gfx::TextAlign::AlignHCenter | gfx::TextAlign::AlignVCenter);
            if (texture_map && texture_map->GetId() == mSelectedTextureMapId)
            {
                gfx::DrawRectOutline(painter, rect, gfx::Color::Green);
            }
            continue;
        }

        const auto* texture_src = texture_map->GetTextureSource(0);
        const auto& texture_rect = texture_map->GetTextureRect(0);

        gfx::MaterialClass temp(gfx::MaterialClass::Type::Texture);
        temp.SetSurfaceType(mMaterial->GetSurfaceType());
        temp.SetBaseColor(mMaterial->GetBaseColor());
        temp.SetTextureMinFilter(mMaterial->GetTextureMinFilter());
        temp.SetTextureMagFilter(mMaterial->GetTextureMagFilter());
        temp.SetAlphaCutoff(mMaterial->GetAlphaCutoff());
        temp.AddTexture(texture_src->Copy());
        temp.SetTextureRect(texture_rect);

        gfx::MaterialInstance instance(temp);
        instance.SetFirstRender(false);
        gfx::FillRect(painter, rect, instance);

        if (texture_map->GetId() == mSelectedTextureMapId)
        {
            gfx::DrawRectOutline(painter, rect, gfx::Color::Green);
        }
    }
    const auto render_width = rect_width * texture_count;
    ComputeScrollBars(render_width);
}

void SpriteWidget::PaintSprite(const gfx::MaterialClass* material, gfx::Painter& painter, double dt)
{
    const auto widget_height = mUI.widget->height();
    const auto widget_width = mUI.widget->width();

    const auto& active_texture_map = material->GetActiveTextureMap();
    const auto* texture_map = material->FindTextureMapById(active_texture_map);
    if (texture_map == nullptr)
    {
        ShowInstruction("Active texture map is not selected.", Rect2Df(0.0f, 0.0f, widget_width, widget_height), painter);
        return;
    }

    const auto texture_count = texture_map->GetNumTextures();
    const auto frame_count = texture_map->GetSpriteFrameCount();
    if (texture_count == 0 || frame_count == 0)
    {
        ShowInstruction("Sprite animation has no textures.", Rect2Df(0.0f, 0.0f, widget_width, widget_height), painter);
        return;
    }

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
        temp.SetSurfaceType(material->GetSurfaceType());
        temp.SetTextureMinFilter(material->GetTextureMinFilter());
        temp.SetTextureMagFilter(material->GetTextureMagFilter());
        temp.SetBaseColor(material->GetBaseColor());
        temp.SetAlphaCutoff(material->GetAlphaCutoff());
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

                gfx::MaterialInstance instance(temp);
                instance.SetFirstRender(false);
                gfx::FillRect(painter, rect, instance);
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
            temp.SetSurfaceType(material->GetSurfaceType());
            temp.SetBaseColor(material->GetBaseColor());
            temp.SetTextureMinFilter(material->GetTextureMinFilter());
            temp.SetTextureMagFilter(material->GetTextureMagFilter());
            temp.SetAlphaCutoff(material->GetAlphaCutoff());
            temp.AddTexture(texture_src->Copy());
            temp.SetTextureRect(texture_rect);

            gfx::FRect rect;
            rect.Resize(rect_width, rect_height);
            rect.Translate(10.0f, 15.0f); // left margin, top margin
            rect.Translate(rect_width * texture_index, 0.0);
            rect.Translate(5.0f, 0.0f); // little padding around each rect
            rect.Translate(-mTranslateX, 0.0); // scrolling

            gfx::MaterialInstance instance(temp);
            instance.SetFirstRender(false);
            gfx::FillRect(painter, rect, instance);
        }
    }

    DrawTime(painter, duration, cycle_width, texture_map->IsSpriteLooping());

    const auto render_width = 10.0f + duration_width + 10.0f;
    ComputeScrollBars(render_width);

    // store the "width" (in pixels) of the sprite cycle for later
    // use, i.e. when mapping click points.
    mPreviousDurationRenderWidth = static_cast<unsigned>(duration_width);
}

void SpriteWidget::PaintParticle(gfx::Painter& painter, double dt)
{
    const auto widget_height = mUI.widget->height();
    const auto widget_width = mUI.widget->width();

    const auto& active_texture_map = mMaterial->GetActiveTextureMap();
    const auto* texture_map = mMaterial->FindTextureMapById(active_texture_map);
    if (texture_map == nullptr)
    {
        ShowInstruction("Active texture map is not selected.", Rect2Df(0.0f, 0.0f, widget_width, widget_height), painter);
        return;
    }
    // the particle animation is simulated in 10 steps and the
    // time is actually a normalized particle lifetime value from 0.0f to 1.0f

    const auto frame_count = 10;
    const auto rect_height = widget_height - 30.0;
    const auto rect_width = rect_height + 2.0f * 5.0f; // little bit of space between items
    const auto cycle_width = frame_count * rect_width;
    const auto duration_whole_seconds = 1.0f;
    const auto duration_width = rect_width * frame_count;
    const auto second_width = duration_width / duration_whole_seconds;
    const auto deci_second_width = second_width / 10.0f;

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

        VerticalLine(0.0f*second_width, true);
        VerticalLine(1.0f*second_width, true);
        PrintText("0.0", 0.0f*second_width);
        PrintText("1.0", 1.0f*second_width);

        // draw deci-second ticks
        for (unsigned deci=1; deci<10; ++deci)
        {
            const float x = deci * deci_second_width;
            VerticalLine(x, false);
        }
    }

    for (unsigned i=0; i<10; ++i)
    {
        gfx::FRect rect;
        rect.Resize(rect_width, rect_height);
        rect.Translate(10.0f, 15.0f); // left margin, top margin
        rect.Translate(rect_width * i, 0.0f);
        rect.Translate(5.0f, 0.0f); // little padding around each rect.
        rect.Translate(-mTranslateX, 0.0f); // scrolling
        const float time = i * 0.1f;

        gfx::MaterialInstance instance(mMaterial);
        instance.SetRuntime(time);
        instance.SetFirstRender(false);
        gfx::FillRect(painter, rect, instance);
    }

    DrawTime(painter, 1.0f, cycle_width, true);

    const auto render_width = 10.0f + duration_width + 10.0f;
    ComputeScrollBars(render_width);

    // store the "width" (in pixels) of the sprite cycle for later
    // use, i.e. when mapping click points.
    mPreviousDurationRenderWidth = static_cast<unsigned>(duration_width);
}

void SpriteWidget::MousePress(const QMouseEvent* mickey)
{
    if (mickey->button() != Qt::LeftButton)
        return;

    if (!mCanDragTime || !mTimeBarUnderMouse)
        return;

    mDragTime = true;
}

void SpriteWidget::MouseRelease(const QMouseEvent* mickey)
{
    const auto btn = mickey->button();
    if (btn != Qt::LeftButton)
        return;

    if (!mCanDragTime || !mPreviousDurationRenderWidth)
        return;

    if (mMaterial->GetType() == gfx::MaterialClass::Type::Sprite ||
        mMaterial->GetType() == gfx::MaterialClass::Type::Particle2D)
    {
        MapFromRenderToTime();
    }

    mDragTime = false;
}

void SpriteWidget::MouseMove(const QMouseEvent* mickey)
{
    mMousePos = MapPoint(mickey->pos());

    if (!mDragTime)
        return;

    if (mMaterial->GetType() == gfx::MaterialClass::Type::Sprite ||
        mMaterial->GetType() == gfx::MaterialClass::Type::Particle2D)
    {
        MapFromRenderToTime();
    }
}

void SpriteWidget::ComputeScrollBars(unsigned render_width)
{
    const auto widget_width = mUI.widget->width();
    if (mPreviousRenderWidth == render_width && mPreviousWidgetWidth == widget_width)
        return;

    QSignalBlocker s(mUI.horizontalScrollBar);

    if (render_width > widget_width)
    {
        const auto horizontal_excess = render_width - widget_width;
        mUI.horizontalScrollBar->setMinimum(0);
        mUI.horizontalScrollBar->setMaximum(horizontal_excess / ScrollStepSize);
        mUI.horizontalScrollBar->setSingleStep(1);
        mUI.horizontalScrollBar->setValue(0);
        mTranslateX = 0;
    }
    else
    {
        mUI.horizontalScrollBar->setRange(0, 0);
        mUI.horizontalScrollBar->setValue(0);
        mTranslateX = 0.0;
    }
    mPreviousRenderWidth = render_width;
    mPreviousWidgetWidth = widget_width;
}

QPoint SpriteWidget::MapPoint(const QPoint& point) const
{
    return QPoint(mTranslateX, 0) + point - QPoint(10, 0);
}

float SpriteWidget::MapFromTimeToRender(float duration, float render_width, bool looping) const
{
    if (looping)
    {
        const auto t = mTime > duration ? std::fmod(mTime, duration) : mTime;
        const auto p = t / duration;
        return p * render_width;
    }

    const auto p = std::min(mTime / duration, 1.0);
    return p * render_width;
}

void SpriteWidget::MapFromRenderToTime()
{
    float duration = 0.0f;
    if (mMaterial->GetType() == gfx::MaterialClass::Type::Sprite)
    {
        const auto& texture_map_id = mMaterial->GetActiveTextureMap();
        const auto* texture_map = mMaterial->FindTextureMapById(texture_map_id);
        if (!texture_map || !texture_map->IsSpriteMap())
            return;
        duration = texture_map->GetSpriteCycleDuration();
    }
    else if (mMaterial->GetType() == gfx::MaterialClass::Type::Particle2D)
    {
        duration = 1.0f;
    }
    const auto duration_whole_seconds = std::ceil(duration);

    const auto xpos = static_cast<float>(mMousePos.x());
    const auto time = xpos / static_cast<float>(mPreviousDurationRenderWidth) * duration_whole_seconds;
    const auto safe_time = math::clamp(0.0f, duration, time);
    emit AdjustTime(safe_time);

    //DEBUG("duration in pixels = %1, xpos = %2, time = %3s",
    //    mPreviousDurationRenderWidth, xpos, time);

}

void SpriteWidget::DrawTime(gfx::Painter& painter, float duration, float cycle_width, bool looping)
{
    // render the time indicator as a vertical line

    const auto widget_height = mUI.widget->height();

    const float pos = MapFromTimeToRender(duration, cycle_width,looping);

    gfx::FPoint a;
    a.Translate(10.0f, 20.0f);
    a.Translate(-mTranslateX, 0.0f);
    a.Translate(pos, 0.0f);

    gfx::FPoint b;
    b.Translate(10.0f, widget_height - 15.0f);
    b.Translate(-mTranslateX, 0.0f);
    b.Translate(pos, 0.0f);

    if (std::abs(mMousePos.x() - pos) < 5)
    {
        mTimeBarUnderMouse = true;
        gfx::DebugDrawLine(painter, a, b, gfx::Color::Green, 1.0f);
    }
    else
        gfx::DebugDrawLine(painter, a, b, gfx::Color::Silver, 1.0f);
}


} // namespace
