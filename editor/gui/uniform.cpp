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

void Uniform::SetType(Type type)
{
    HideEverything();

    if (type == Type::Float)
    {
        mUI->value_x->setVisible(true);
    }
    else if (type == Type::Int)
    {
        mUI->value_i->setVisible(true);
    }
    else if (type == Type::Vec2)
    {
        mUI->value_x->setVisible(true);
        mUI->value_y->setVisible(true);
        mUI->label_x->setVisible(true);
        mUI->label_y->setVisible(true);
    }
    else if (type == Type::Vec3)
    {
        mUI->value_x->setVisible(true);
        mUI->value_y->setVisible(true);
        mUI->value_z->setVisible(true);
        mUI->label_x->setVisible(true);
        mUI->label_y->setVisible(true);
        mUI->label_z->setVisible(true);
    }
    else if (type == Type::Vec4)
    {
        mUI->value_x->setVisible(true);
        mUI->value_y->setVisible(true);
        mUI->value_z->setVisible(true);
        mUI->value_w->setVisible(true);
        mUI->label_x->setVisible(true);
        mUI->label_y->setVisible(true);
        mUI->label_z->setVisible(true);
        mUI->label_w->setVisible(true);
    }
    else if (type == Type::Color)
        mUI->color->setVisible(true);
    else if (type == Type::String)
        mUI->string->setVisible(true);

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
    mUI->label_x->setVisible(false);
    mUI->label_y->setVisible(false);
    mUI->label_z->setVisible(false);
    mUI->label_w->setVisible(false);
    mUI->value_x->setVisible(false);
    mUI->value_y->setVisible(false);
    mUI->value_z->setVisible(false);
    mUI->value_w->setVisible(false);
    mUI->color->setVisible(false);
    mUI->string->setVisible(false);
    mUI->value_i->setVisible(false);
}

} // namespace
