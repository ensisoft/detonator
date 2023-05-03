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
#include "game/animation.h"
#include "editor/gui/dlgmaterialparams.h"
#include "editor/gui/utility.h"
#include "editor/gui/uniform.h"
#include "editor/app/eventlog.h"

namespace {
template<typename KlassType>
void SetMaterialParam(KlassType* klass, const gui::Uniform* widget)
{
    const auto& str = app::ToUtf8(widget->GetName());
    const auto type = widget->GetType();

    if (type == gui::Uniform::Type::Int)
        klass->SetMaterialParam(str, widget->GetAsInt());
    else if (type == gui::Uniform::Type::Float)
        klass->SetMaterialParam(str, widget->GetAsFloat());
    else if (type == gui::Uniform::Type::Vec2)
        klass->SetMaterialParam(str, widget->GetAsVec2());
    else if (type == gui::Uniform::Type::Vec3)
        klass->SetMaterialParam(str, widget->GetAsVec3());
    else if (type == gui::Uniform::Type::Vec4)
        klass->SetMaterialParam(str, widget->GetAsVec4());
    else if (type == gui::Uniform::Type::Color)
        klass->SetMaterialParam(str, gui::ToGfx(widget->GetAsColor()));
    else  BUG("Unexpected material uniform type.");

}
} // namespace

namespace gui
{

DlgMaterialParams::DlgMaterialParams(QWidget* parent, game::DrawableItemClass* item)
  : QDialog(parent)
  , mItem(item)
  , mActuator(nullptr)
  , mOldParams(item->GetMaterialParams())
{
    mUI.setupUi(this);
    SetVisible(mUI.builtInParams,  false);
    SetVisible(mUI.grpMessage,     false);
    SetVisible(mUI.customUniforms, false);
}

DlgMaterialParams::DlgMaterialParams(QWidget* parent, game::DrawableItemClass* item, game::MaterialActuatorClass* actuator)
  : QDialog(parent)
  , mItem(item)
  , mActuator(actuator)
  , mOldParams(actuator->GetMaterialParams())
{
    mUI.setupUi(this);
    SetVisible(mUI.builtInParams,  false);
    SetVisible(mUI.grpMessage,     false);
    SetVisible(mUI.customUniforms, false);
}

void DlgMaterialParams::AdaptInterface(const app::Workspace* workspace, const gfx::MaterialClass* material)
{
    if (material->GetType() != gfx::MaterialClass::Type::Custom)
    {
        if (material->IsStatic())
        {
            SetVisible(mUI.grpMessage,     true);
            SetVisible(mUI.builtInParams,  false);
            SetValue(mUI.lblMessage, "Material uses static properties and cannot apply instance parameters.\n\n"
                                     "You can change this in the material editor by toggling off \"static instance\" flag.");
        }
        else
        {
            if (mActuator)
            {
                if (mItem->HasMaterialParam("kGamma"))
                {
                    if (const auto* param = mActuator->FindMaterialParam("kGamma"))
                    {
                        if (const auto* gamma = std::get_if<float>(param))
                            SetValue(mUI.gamma, *gamma);
                    }
                    else if (const auto* param = mItem->FindMaterialParam("kGamma"))
                    {
                        if (const auto* gamma = std::get_if<float>(param))
                            SetValue(mUI.gamma, *gamma);
                    }
                    SetVisible(mUI.lblGamma,      true);
                    SetVisible(mUI.gamma,         true);
                    SetVisible(mUI.btnResetGamma, true);
                }
                else
                {
                    SetVisible(mUI.lblGamma,      false);
                    SetVisible(mUI.gamma,         false);
                    SetVisible(mUI.btnResetGamma, false);
                }

                if (mItem->HasMaterialParam("kBaseColor"))
                {
                    if (const auto* param = mActuator->FindMaterialParam("kBaseColor"))
                    {
                        if (const auto* color = std::get_if<game::Color4f>(param))
                            SetValue(mUI.baseColor, *color);
                    }
                    else if (const auto* param = mItem->FindMaterialParam("kBaseColor"))
                    {
                        if (const auto* color = std::get_if<game::Color4f>(param))
                            SetValue(mUI.baseColor, *color);
                    }
                    SetVisible(mUI.lblBaseColor,      true);
                    SetVisible(mUI.baseColor,         true);
                    SetVisible(mUI.btnResetBaseColor, true);
                }
                else
                {
                    SetVisible(mUI.lblBaseColor,      false);
                    SetVisible(mUI.baseColor,         false);
                    SetVisible(mUI.btnResetBaseColor, false);
                }

                if (!mItem->HasMaterialParam("kGamma") && !mItem->HasMaterialParam("kBaseColor"))
                {
                    SetVisible(mUI.grpMessage, true);
                    SetValue(mUI.lblMessage, "This entity node has no material parameters available for changing.\n"
                                             "To make a material parameters editable here they must first be added\n"
                                             "to the the entity node itself in the entity editor.");
                }
                else
                {
                    SetVisible(mUI.builtInParams, true);
                }
            }
            else
            {
                if (const auto* param = mItem->FindMaterialParam("kGamma"))
                {
                    if (const auto* gamma = std::get_if<float>(param))
                        SetValue(mUI.gamma, *gamma);
                }
                if (const auto* param = mItem->FindMaterialParam("kBaseColor"))
                {
                    if (const auto* color = std::get_if<game::Color4f>(param))
                        SetValue(mUI.baseColor, *color);
                }

                std::vector<ResourceListItem> maps;
                for (unsigned i=0; i<material->GetNumTextureMaps(); ++i)
                {
                    const auto* map = material->GetTextureMap(i);
                    ResourceListItem item;
                    item.id   = map->GetId();
                    item.name = map->GetName();
                    maps.push_back(item);
                }
                SetList(mUI.textureMaps, maps);
                if (mItem->HasActiveTextureMap())
                    SetValue(mUI.textureMaps, ListItemId(mItem->GetActiveTextureMap()));

                if (maps.empty())
                {
                    SetVisible(mUI.textureMaps, false);
                    SetVisible(mUI.lblTextureMap, false);
                    SetVisible(mUI.btnResetActiveMap, false);
                }

                SetVisible(mUI.builtInParams, true);
            }
        }
    }
    else if(material->GetType() == gfx::MaterialClass::Type::Custom)
    {
        SetVisible(mUI.customUniforms, false);

        // try to load the .json file that should contain the meta information
        // about the shader input parameters.
        auto uri = material->GetShaderUri();
        if (uri.empty())
        {
            SetValue(mUI.lblMessage, "Material doesn't have shader (.glsl file) URI set.");
            SetVisible(mUI.grpMessage, true);
            return;
        }

        boost::replace_all(uri, ".glsl", ".json");
        const auto [parse_success, json, error] = base::JsonParseFile(workspace->MapFileToFilesystem(uri));
        if (!parse_success)
        {
            ERROR("Failed to parse the shader description file. [file='%1', error='%2']", uri, error);
            SetValue(mUI.lblMessage, "Failed to parse shader descriptor file.");
            SetVisible(mUI.grpMessage, true);
            return;
        }
        if (!json.contains("uniforms"))
        {
            SetValue(mUI.lblMessage, "The shader doesn't use any material parameters.");
            SetVisible(mUI.grpMessage, true);
            return;
        }

        mUI.customUniforms->setLayout(new QGridLayout);
        auto* layout = qobject_cast<QGridLayout*>(mUI.customUniforms->layout());
        auto widget_row = 0;
        auto widget_col = 0;
        bool have_any   = false;
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

            if (mActuator && !mItem->HasMaterialParam(name))
                continue;

            have_any = true;

            auto* label = new QLabel(this);
            SetValue(label, desc);
            layout->addWidget(label, widget_row, 0);

            auto* widget = new Uniform(this);
            widget->SetType(type);
            widget->SetName(app::FromUtf8(name));
            connect(widget, &Uniform::ValueChanged, this, &DlgMaterialParams::UniformValueChanged);
            layout->addWidget(widget, widget_row, 1);
            mUniforms.push_back(widget);

            auto* chk = new QCheckBox(this);
            chk->setText("Override");
            chk->setProperty("uniform_name", app::FromUtf8(name));
            connect(chk, &QCheckBox::toggled, this, &DlgMaterialParams::ToggleUniform);
            layout->addWidget(chk, widget_row, 2);

            if (mActuator)
            {
                if (const auto* uniform = mActuator->FindMaterialParam(name))
                {
                    std::visit([widget](const auto& variant_value) {
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
            }
            else
            {
                if (const auto* uniform = mItem->FindMaterialParam(name))
                {
                    std::visit([widget](const auto& variant_value) {
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
            }
            widget_row++;
        }
        if (have_any)
        {
            SetVisible(mUI.customUniforms, true);
        }
        else
        {
            SetVisible(mUI.grpMessage, true);
            SetValue(mUI.lblMessage, "This entity node has no material parameters available for changing.\n"
                                     "To make a material parameters editable here they must first be added\n"
                                     "to the the entity node itself in the entity editor.");
        }
    }
}

void DlgMaterialParams::on_btnResetGamma_clicked()
{
    SetValue(mUI.gamma, -0.1f);
    if (mActuator)
    {
        mActuator->DeleteMaterialParam("kGamma");
        if (const auto* param = mItem->FindMaterialParam("kGamma"))
        {
            if (const auto* val = std::get_if<float>(param))
                SetValue(mUI.gamma, *val);
        }
    }
    else
    {
        mItem->DeleteMaterialParam("kGamma");
    }
}

void DlgMaterialParams::on_btnResetBaseColor_clicked()
{
    mUI.baseColor->clearColor();
    if (mActuator)
    {
        mActuator->DeleteMaterialParam("kBaseColor");
        if (const auto* param = mItem->FindMaterialParam("kBaseColor"))
        {
            if (const auto* color = std::get_if<game::Color4f>(param))
                SetValue(mUI.baseColor, *color);
        }
    }
    else
    {
        mItem->DeleteMaterialParam("kBaseColor");
    }
}

void DlgMaterialParams::on_btnResetActiveMap_clicked()
{
    SetValue(mUI.textureMaps, -1);
    if (mActuator)
    {
        // todo
    }
    else
    {
        mItem->ResetActiveTextureMap();
    }
}

void DlgMaterialParams::on_gamma_valueChanged(double)
{
    if (mActuator)
        mActuator->SetMaterialParam("kGamma", GetValue(mUI.gamma));
    else mItem->SetMaterialParam("kGamma", GetValue(mUI.gamma));
}

void DlgMaterialParams::on_textureMaps_currentIndexChanged(int)
{
    if (mActuator)
    {

    }
    else
    {
        mItem->SetActiveTextureMap(GetItemId(mUI.textureMaps));
    }
}

void DlgMaterialParams::on_baseColor_colorChanged(QColor color)
{
    if (mActuator)
        mActuator->SetMaterialParam("kBaseColor", ToGfx(color));
    else mItem->SetMaterialParam("kBaseColor", ToGfx(color));
}

void DlgMaterialParams::on_btnAccept_clicked()
{
    accept();
}

void DlgMaterialParams::on_btnCancel_clicked()
{
    // put back the old values.
    if (mActuator)
        mActuator->SetMaterialParams(std::move(mOldParams));
    else mItem->SetMaterialParams(std::move(mOldParams));

    reject();
}

void DlgMaterialParams::UniformValueChanged(const gui::Uniform* widget)
{
    if (mActuator)
        SetMaterialParam(mActuator, widget);
    else SetMaterialParam(mItem, widget);
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

    const auto& uniform_name = app::ToUtf8(name);

    if (checked)
    {
        if (mActuator)
        {
            SetMaterialParam(mActuator, widget);
        }
        else
        {
            SetMaterialParam(mItem, widget);
        }
        SetEnabled(widget, true);
    }
    else
    {
        if (mActuator)
        {
            mActuator->DeleteMaterialParam(uniform_name);
            if (const auto* uniform = mItem->FindMaterialParam(uniform_name))
            {
                std::visit([widget](const auto& variant_value) {
                    widget->SetValue(variant_value);
                }, *uniform);
            }
        }
        else
        {
            mItem->DeleteMaterialParam(uniform_name);
        }
        SetEnabled(widget, false);
    }
}

} // namespace