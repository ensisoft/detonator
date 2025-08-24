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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include "ui_spritewidget.h"
#  include <QTimer>
#  include <QDialog>
#  include <QWidget>
#  include <QPoint>
#include "warnpop.h"

#include "graphics/material_class.h"
#include "graphics/material.h"

namespace gui
{
    class SpriteWidget : public QWidget
    {
        Q_OBJECT
    public:
        explicit SpriteWidget(QWidget* parent);
        ~SpriteWidget() override;

        void Render();

        void SetMaterial(std::shared_ptr<const gfx::MaterialClass> klass)
        {
            mMaterial = std::move(klass);
        }

        void SetSelectedTextureId(std::string id)
        {
            mSelectedTextureId = std::move(id);
        }
        void SetSelectedTextureMapId(std::string id)
        {
            mSelectedTextureMapId = std::move(id);
        }

        void SetClearColor(const QColor& color)
        {
            mUI.widget->SetClearColor(color);
        }
        void SetTime(double time)
        {
            mTime = time;
        }
        void RenderTimeBar(bool on_off)
        {
            mRenderTime = on_off;
        }

    private slots:
        void on_horizontalScrollBar_valueChanged(int);

    private:
        void PaintScene(gfx::Painter& painter, double dt);
        void PaintTexture(const gfx::MaterialClass* klass, gfx::Painter& painter, double dt);
        void PaintSprite(const gfx::MaterialClass* klass, gfx::Painter& painter, double dt);
        void MousePress(QMouseEvent* mickey);
        void MouseRelease(QMouseEvent* mickey);
        void MouseMove(QMouseEvent* mickey);
        void ComputeScrollBars(unsigned render_width);

    private:
        Ui::SpriteWidget mUI;
    private:
        std::shared_ptr<const gfx::MaterialClass> mMaterial;
        std::string mSelectedTextureId;
        std::string mSelectedTextureMapId;
        float mTranslateX = 0.0f;
        double mTime = 0.0f;
        bool mRenderTime = false;
        bool mDragTime = false;

        unsigned mPreviousRenderWidth = 0;
        unsigned mPreviousWidgetWidth = 0;
    };
} // namespace