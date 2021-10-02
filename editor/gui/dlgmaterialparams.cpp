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
#  include <nlohmann/json.hpp>
#  include <boost/algorithm/string/erase.hpp>
#include "warnpop.h"

#include "base/math.h"
#include "base/json.h"
#include "game/entity.h"
#include "editor/gui/dlgmaterialparams.h"
#include "editor/gui/utility.h"
#include "editor/gui/uniform.h"
#include "editor/app/eventlog.h"
#include "graphics/material.h"

namespace gui
{

DlgMaterialParams::DlgMaterialParams(QWidget* parent, app::Workspace* workspace, game::DrawableItemClass* item)
  : QDialog(parent)
  , mWorkspace(workspace)
  , mItem(item)
  , mOldParams(item->GetMaterialParams())
{
    mUI.setupUi(this);
    SetVisible(mUI.builtInParams, false);
    SetVisible(mUI.lblStatic, false);
    SetVisible(mUI.customUniforms, false);

    auto material = mWorkspace->GetMaterialClassById(app::FromUtf8(item->GetMaterialId()));
    if (auto* builtin = material->AsBuiltIn())
    {
        if (builtin->IsStatic())
        {
            SetVisible(mUI.lblStatic, true);
            SetVisible(mUI.builtInParams, false);
        }
        else
        {
            SetVisible(mUI.builtInParams, true);
            if (const auto* param = item->FindMaterialParam("kGamma"))
            {
                if (const auto* gamma = std::get_if<float>(param))
                    SetValue(mUI.gamma, *gamma);
            }
            if (const auto* param = item->FindMaterialParam("kBaseColor"))
            {
                if (const auto* color = std::get_if<game::Color4f>(param))
                    SetValue(mUI.baseColor, *color);
            }
        }
    }
    else if(auto* custom = material->AsCustom())
    {
        SetVisible(mUI.customUniforms, true);
        mCustomMaterial = true;

        // try to load the .json file that should contain the meta information
        // about the shader input parameters.
        auto uri = custom->GetShaderUri();
        if (uri.empty())
        {
            ERROR("Empty material shader uri.");
            return;
        }

        boost::replace_all(uri, ".glsl", ".json");
        const auto [parse_success, json, error] = base::JsonParseFile(app::ToUtf8(mWorkspace->MapFileToFilesystem(uri)));
        if (!parse_success)
        {
            ERROR("Failed to parse the shader description file '%1' %2", uri, error);
            return;
        }
        if (json.contains("uniforms"))
        {
            if (!mUI.customUniforms->layout())
                mUI.customUniforms->setLayout(new QGridLayout);
            auto* layout = qobject_cast<QGridLayout*>(mUI.customUniforms->layout());
            auto widget_row = 0;
            auto widget_col = 0;
            for (const auto& json : json["uniforms"].items())
            {
                Uniform::Type type = Uniform::Type::Float;
                std::string name = "kUniform";
                std::string desc = "Uniform";
                if (!base::JsonReadSafe(json.value(), "desc", &desc))
                    WARN("Uniform is missing 'desc' parameter.");
                if (!base::JsonReadSafe(json.value(), "name", &name))
                    WARN("Uniform is missing 'name' parameter.");
                if (!base::JsonReadSafe(json.value(), "type", &type))
                    WARN("Uniform is missing 'type' parameter.");

                auto* label = new QLabel(this);
                SetValue(label, desc);
                layout->addWidget(label, widget_row, 0);

                auto* widget = new Uniform(this);
                connect(widget, &Uniform::ValueChanged, this, &DlgMaterialParams::UniformValueChanged);
                widget->SetType(type);
                widget->SetName(app::FromUtf8(name));
                layout->addWidget(widget, widget_row, 1);
                mUniforms.push_back(widget);

                auto* chk = new QCheckBox(this);
                chk->setText("Override");
                chk->setProperty("uniform_name", app::FromUtf8(name));
                connect(chk, &QCheckBox::toggled, this, &DlgMaterialParams::ToggleUniform);
                layout->addWidget(chk, widget_row, 2);
                if (const auto* uniform = item->FindMaterialParam(name))
                {
                    std::visit([widget](const auto& variant_value)  {
                       widget->SetValue(variant_value);
                    }, *uniform);
                    SetValue(chk, true);
                    SetEnabled(widget, true);
                }
                else
                {
                    chk->setChecked(false);
                    widget->setEnabled(false);
                }

                widget_row++;
            }
        }
    }
}

void DlgMaterialParams::on_btnResetGamma_clicked()
{
    SetValue(mUI.gamma, -0.1f);
    mItem->DeleteMaterialParam("kGamma");
}

void DlgMaterialParams::on_btnResetBaseColor_clicked()
{
    mUI.baseColor->clearColor();
    mItem->DeleteMaterialParam("kBaseColor");
}

void DlgMaterialParams::on_gamma_valueChanged(double)
{
    mItem->SetMaterialParam("kGamma", GetValue(mUI.gamma));
}

void DlgMaterialParams::on_baseColor_colorChanged(QColor color)
{
    mItem->SetMaterialParam("kBaseColor", ToGfx(color));
}

void DlgMaterialParams::on_btnAccept_clicked()
{
    accept();
}

void DlgMaterialParams::on_btnCancel_clicked()
{
    // put back the old values.
    mItem->SetMaterialParams(std::move(mOldParams));
    reject();
}

void DlgMaterialParams::UniformValueChanged(const gui::Uniform* widget)
{
    const auto& str = app::ToUtf8(widget->GetName());
    const auto type = widget->GetType();
    if (type == Uniform::Type::Int)
        mItem->SetMaterialParam(str, widget->GetAsInt());
    else if (type == Uniform::Type::Float)
        mItem->SetMaterialParam(str, widget->GetAsFloat());
    else if (type == Uniform::Type::Vec2)
        mItem->SetMaterialParam(str, widget->GetAsVec2());
    else if (type == Uniform::Type::Vec3)
        mItem->SetMaterialParam(str, widget->GetAsVec3());
    else if (type == Uniform::Type::Vec4)
        mItem->SetMaterialParam(str, widget->GetAsVec4());
    else if (type == Uniform::Type::Color)
        mItem->SetMaterialParam(str, ToGfx(widget->GetAsColor()));
    else BUG("Unexpected material uniform type.");
}

void DlgMaterialParams::ToggleUniform(bool checked)
{
    const auto* chk = qobject_cast<QCheckBox*>(sender());
    const auto& name = chk->property("uniform_name").toString();

    Uniform* widget = nullptr;
    for (auto* wid : mUniforms)
    {
        if (wid->GetName() != name)
            continue;
        widget = wid;
        break;
    }
    ASSERT(widget);

    if (!checked)
    {
        mItem->DeleteMaterialParam(app::ToUtf8(name));
        SetEnabled(widget, false);
        return;
    }
    SetEnabled(widget, true);
    const auto& str = app::ToUtf8(name);
    const auto type = widget->GetType();
    if (type == Uniform::Type::Int)
        mItem->SetMaterialParam(str, widget->GetAsInt());
    else if (type == Uniform::Type::Float)
        mItem->SetMaterialParam(str, widget->GetAsFloat());
    else if (type == Uniform::Type::Vec2)
        mItem->SetMaterialParam(str, widget->GetAsVec2());
    else if (type == Uniform::Type::Vec3)
        mItem->SetMaterialParam(str, widget->GetAsVec3());
    else if (type == Uniform::Type::Vec4)
        mItem->SetMaterialParam(str, widget->GetAsVec4());
    else if (type == Uniform::Type::Color)
        mItem->SetMaterialParam(str, ToGfx(widget->GetAsColor()));
    else BUG("Unexpected material uniform type.");

}

} // namespace