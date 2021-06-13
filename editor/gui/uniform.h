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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include "ui_uniform.h"
#  include <QWidget>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include "editor/gui/utility.h"

namespace gui
{
    class Uniform : public QWidget
    {
        Q_OBJECT

    public:
        enum class Type {
            Float, Vec2, Vec3, Vec4, Color
        };
        Uniform(QWidget* parent) : QWidget(parent)
        {
            mUI.setupUi(this);
            mUI.label_x->setVisible(false);
            mUI.label_y->setVisible(false);
            mUI.label_z->setVisible(false);
            mUI.label_w->setVisible(false);
            mUI.value_x->setVisible(false);
            mUI.value_y->setVisible(false);
            mUI.value_z->setVisible(false);
            mUI.value_w->setVisible(false);
            mUI.color->setVisible(false);
        }
        void SetType(Type type)
        {
            if (type == Type::Float)
            {
                mUI.value_x->setVisible(true);
            }
            else if (type == Type::Vec2)
            {
                mUI.value_x->setVisible(true);
                mUI.value_y->setVisible(true);
                mUI.label_x->setVisible(true);
                mUI.label_y->setVisible(true);
            }
            else if (type == Type::Vec3)
            {
                mUI.value_x->setVisible(true);
                mUI.value_y->setVisible(true);
                mUI.value_z->setVisible(true);
                mUI.label_x->setVisible(true);
                mUI.label_y->setVisible(true);
                mUI.label_z->setVisible(true);
            }
            else if (type == Type::Vec4)
            {
                mUI.value_x->setVisible(true);
                mUI.value_y->setVisible(true);
                mUI.value_z->setVisible(true);
                mUI.value_w->setVisible(true);
                mUI.label_x->setVisible(true);
                mUI.label_y->setVisible(true);
                mUI.label_z->setVisible(true);
                mUI.label_w->setVisible(true);
            }
            else if (type== Type::Color)
                mUI.color->setVisible(true);
        }
        void SetValue(float value)
        {
            gui::SetValue(mUI.value_x, value);
        }
        void SetValue(const glm::vec2& value)
        {
            gui::SetValue(mUI.value_x, value.x);
            gui::SetValue(mUI.value_y, value.y);
        }
        void SetValue(const glm::vec3& value)
        {
            gui::SetValue(mUI.value_x, value.x);
            gui::SetValue(mUI.value_y, value.y);
            gui::SetValue(mUI.value_z, value.z);
        }
        void SetValue(const glm::vec4& value)
        {
            gui::SetValue(mUI.value_x, value.x);
            gui::SetValue(mUI.value_y, value.y);
            gui::SetValue(mUI.value_z, value.z);
            gui::SetValue(mUI.value_w, value.w);
        }
        void SetValue(QColor color)
        { gui::SetValue(mUI.color, color); }
        float GetAsFloat() const
        { return gui::GetValue(mUI.value_x); }
        glm::vec2 GetAsVec2() const
        {
            return {gui::GetValue(mUI.value_x),
                    gui::GetValue(mUI.value_y)};
        }
        glm::vec3 GetAsVec3() const
        {
            return {gui::GetValue(mUI.value_x),
                    gui::GetValue(mUI.value_y),
                    gui::GetValue(mUI.value_z)};
        }
        glm::vec4 GetAsVec4() const
        {
            return {gui::GetValue(mUI.value_x),
                    gui::GetValue(mUI.value_y),
                    gui::GetValue(mUI.value_z),
                    gui::GetValue(mUI.value_w)};
        }
        QColor GetAsColor() const
        {
            return mUI.color->color();
        }

        void SetName(QString name)
        { mName = name; }
        QString GetName() const
        { return mName; }
    signals:
        void ValueChanged(const Uniform* uniform);
    private slots:
        void on_value_x_valueChanged(double)
        { emit ValueChanged(this); };
        void on_value_y_valueChanged(double)
        { emit ValueChanged(this); };
        void on_value_z_valueChanged(double)
        { emit ValueChanged(this); };
        void on_value_w_valueChanged(double)
        { emit ValueChanged(this); };
        void on_color_colorChanged(QColor)
        { emit ValueChanged(this); }
    private:
        Ui::Uniform mUI;
    private:
        QString mName;
    };
} // namespace
