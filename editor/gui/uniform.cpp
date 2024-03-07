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

#include "config.h"

#include "warnpush.h"
#  include "ui_uniform.h"
#include "warnpop.h"

#include "editor/gui/uniform.h"
#include "editor/gui/utility.h"

namespace gui
{
Uniform::Uniform(QWidget* parent) : QWidget(parent)
{
    mUI = new Ui::Uniform();
    mUI->setupUi(this);
    HideEverything();
}
Uniform::~Uniform()
{
    delete mUI;
}

void Uniform::SetType(Type type, QString suffix)
{
    HideEverything();

    if (type == Type::Float)
    {
        SetVisible(mUI->value_x, true);
        SetSuffix(mUI->value_x, suffix);
    }
    else if (type == Type::Int)
    {
        SetVisible(mUI->value_i, true);
        SetSuffix(mUI->value_i, suffix);
    }
    else if (type == Type::Vec2)
    {
        SetVisible(mUI->value_x, true);
        SetVisible(mUI->value_y, true);
        SetVisible(mUI->label_x, true);
        SetVisible(mUI->label_y, true);
        SetSuffix(mUI->value_x, suffix);
        SetSuffix(mUI->value_y, suffix);

    }
    else if (type == Type::Vec3)
    {
        SetVisible(mUI->value_x, true);
        SetVisible(mUI->value_y, true);
        SetVisible(mUI->value_z, true);
        SetVisible(mUI->label_x, true);
        SetVisible(mUI->label_y, true);
        SetVisible(mUI->label_z, true);

        SetSuffix(mUI->value_x, suffix);
        SetSuffix(mUI->value_y, suffix);
        SetSuffix(mUI->value_z, suffix);
    }
    else if (type == Type::Vec4)
    {
        SetVisible(mUI->value_x, true);
        SetVisible(mUI->value_y, true);
        SetVisible(mUI->value_z, true);
        SetVisible(mUI->value_w, true);
        SetVisible(mUI->label_x, true);
        SetVisible(mUI->label_y, true);
        SetVisible(mUI->label_z, true);
        SetVisible(mUI->label_w, true);

        SetSuffix(mUI->value_x, suffix);
        SetSuffix(mUI->value_y, suffix);
        SetSuffix(mUI->value_z, suffix);
        SetSuffix(mUI->value_w, suffix);
    }
    else if (type == Type::Color)
    {
        SetVisible(mUI->color, true);
    }
    else if (type == Type::String)
    {
        SetVisible(mUI->string, true);
    }

    mType = type;
}
void Uniform::SetValue(float value)
{
    gui::SetValue(mUI->value_x, value);
}
void Uniform::SetValue(int value)
{
    gui::SetValue(mUI->value_i, value);
}

void Uniform::SetValue(const glm::vec2& value)
{
    gui::SetValue(mUI->value_x, value.x);
    gui::SetValue(mUI->value_y, value.y);
}
void Uniform::SetValue(const glm::vec3& value)
{
    gui::SetValue(mUI->value_x, value.x);
    gui::SetValue(mUI->value_y, value.y);
    gui::SetValue(mUI->value_z, value.z);
}
void Uniform::SetValue(const glm::vec4& value)
{
    gui::SetValue(mUI->value_x, value.x);
    gui::SetValue(mUI->value_y, value.y);
    gui::SetValue(mUI->value_z, value.z);
    gui::SetValue(mUI->value_w, value.w);
}
void Uniform::SetValue(const QColor& color)
{ gui::SetValue(mUI->color, color); }
void Uniform::SetValue(const base::Color4f& color)
{ gui::SetValue(mUI->color, color); }
void Uniform::SetValue(const QString& string)
{ gui::SetValue(mUI->string, string); }
void Uniform::SetValue(const std::string& str)
{ gui::SetValue(mUI->string, str); }
float Uniform::GetAsFloat() const
{ return gui::GetValue(mUI->value_x); }
int Uniform::GetAsInt() const
{ return gui::GetValue(mUI->value_i); }
glm::vec2 Uniform::GetAsVec2() const
{
    return {gui::GetValue(mUI->value_x),
            gui::GetValue(mUI->value_y)};
}
glm::vec3 Uniform::GetAsVec3() const
{
    return {gui::GetValue(mUI->value_x),
            gui::GetValue(mUI->value_y),
            gui::GetValue(mUI->value_z)};
}
glm::vec4 Uniform::GetAsVec4() const
{
    return {gui::GetValue(mUI->value_x),
            gui::GetValue(mUI->value_y),
            gui::GetValue(mUI->value_z),
            gui::GetValue(mUI->value_w)};
}
QColor Uniform::GetAsColor() const
{ return mUI->color->color(); }

QString Uniform::GetAsString() const
{ return mUI->string->text(); }

void Uniform::HideEverything()
{
    SetVisible(mUI->label_x, false);
    SetVisible(mUI->label_y, false);
    SetVisible(mUI->label_z, false);
    SetVisible(mUI->label_w, false);
    SetVisible(mUI->value_x, false);
    SetVisible(mUI->value_y, false);
    SetVisible(mUI->value_z, false);
    SetVisible(mUI->value_w, false);
    SetVisible(mUI->color,   false);
    SetVisible(mUI->string,  false);
    SetVisible(mUI->value_i, false);
}

void Uniform::on_value_x_valueChanged(double)
{
    emit ValueChanged(this);
    mUI->value_x->setFocus();
}
void Uniform::on_value_y_valueChanged(double)
{
    emit ValueChanged(this);
    mUI->value_y->setFocus();
}
void Uniform::on_value_z_valueChanged(double)
{
    emit ValueChanged(this);
    mUI->value_z->setFocus();
}
void Uniform::on_value_w_valueChanged(double)
{
    emit ValueChanged(this);
    mUI->value_w->setFocus();
}
void Uniform::on_color_colorChanged(QColor)
{
    emit ValueChanged(this);
    mUI->color->setFocus();
}
void Uniform::on_string_editingFinished()
{
    emit ValueChanged(this);
    mUI->string->setFocus();
}
void Uniform::on_value_i_valueChanged(int)
{
    emit ValueChanged(this);
    mUI->value_i->setFocus();
}

} // namespace
