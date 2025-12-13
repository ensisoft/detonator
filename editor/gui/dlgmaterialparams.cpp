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
#include "game/timeline_animation.h"
#include "game/timeline_material_animator.h"
#include "graphics/material.h"
#include "graphics/material_class.h"
#include "graphics/material_instance.h"
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
    SetVisible(mUI.builtInParams,        false);
    SetVisible(mUI.grpMessage,           false);
    SetVisible(mUI.customUniforms,       false);
    SetVisible(mUI.colorUniforms,        false);
    SetVisible(mUI.lblTileIndex,         false);
    SetVisible(mUI.tileIndex,            false);
    SetVisible(mUI.btnResetTileIndex,    false);
    SetVisible(mUI.lblTextureScale,      false);
    SetVisible(mUI.textureScaleX,        false);
    SetVisible(mUI.textureScaleY,        false);
    SetVisible(mUI.btnResetTextureScale, false);
}

DlgMaterialParams::DlgMaterialParams(QWidget* parent, game::DrawableItemClass* item, game::MaterialAnimatorClass* actuator)
  : QDialog(parent)
  , mItem(item)
  , mActuator(actuator)
  , mOldParams(actuator->GetMaterialParams())
{
    mUI.setupUi(this);
    SetVisible(mUI.builtInParams,        false);
    SetVisible(mUI.grpMessage,           false);
    SetVisible(mUI.customUniforms,       false);
    SetVisible(mUI.colorUniforms,        false);
    SetVisible(mUI.lblTileIndex,         false);
    SetVisible(mUI.tileIndex,            false);
    SetVisible(mUI.btnResetTileIndex,    false);
    SetVisible(mUI.lblTextureScale,      false);
    SetVisible(mUI.textureScaleX,        false);
    SetVisible(mUI.textureScaleY,        false);
    SetVisible(mUI.btnResetTextureScale, false);
}

void DlgMaterialParams::AdaptInterface(const app::Workspace* workspace, const gfx::MaterialClass* material)
{
    const auto type = material->GetType();
    if (type != gfx::MaterialClass::Type::Custom)
    {
        if (material->IsStatic())
        {
            SetVisible(mUI.grpMessage, true);
            SetValue(mUI.lblMessage, "Material uses static properties and cannot apply instance parameters.\n\n"
                                     "You can change this in the material editor by toggling off \"static instance\" flag.");
            return;
        }
    }

    if (type == gfx::MaterialClass::Type::Color ||
        type == gfx::MaterialClass::Type::Sprite ||
        type == gfx::MaterialClass::Type::Texture ||
        type == gfx::MaterialClass::Type::Tilemap)
    {
        mColorUniforms.push_back({"Base color",
                                  gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::BaseColor),
                                  material->GetColor(gfx::MaterialClass::ColorIndex::BaseColor)});
    }
    else if (type == gfx::MaterialClass::Type::Gradient)
    {
        mColorUniforms.push_back({"Gradient color 0",
                                  gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::GradientColor0),
                                  material->GetColor(gfx::MaterialClass::ColorIndex::GradientColor0)});
        mColorUniforms.push_back({"Gradient color 1",
                                  gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::GradientColor1),
                                  material->GetColor(gfx::MaterialClass::ColorIndex::GradientColor1)});
        mColorUniforms.push_back({"Gradient color 2",
                                  gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::GradientColor2),
                                  material->GetColor(gfx::MaterialClass::ColorIndex::GradientColor2)});
        mColorUniforms.push_back({"Gradient color 3",
                                  gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::GradientColor3),
                                  material->GetColor(gfx::MaterialClass::ColorIndex::GradientColor3)});
    }
    else if (type == gfx::MaterialClass::Type::BasicLight)
    {
        mColorUniforms.push_back({"Ambient color",
                                  gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::AmbientColor),
                                  material->GetColor(gfx::MaterialClass::ColorIndex::AmbientColor)});
        mColorUniforms.push_back({"Diffuse color",
                                  gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::DiffuseColor),
                                  material->GetColor(gfx::MaterialClass::ColorIndex::DiffuseColor)});
        mColorUniforms.push_back({"Specular color",
                                  gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::SpecularColor),
                                  material->GetColor(gfx::MaterialClass::ColorIndex::SpecularColor)});
    }
    else if (type == gfx::MaterialClass::Type::Particle2D)
    {
        mColorUniforms.push_back({"Start color",
                                  gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::ParticleStartColor),
                                  material->GetColor(gfx::MaterialClass::ColorIndex::ParticleStartColor)});
        mColorUniforms.push_back({"Mid color",
                                  gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::ParticleMidColor),
                                  material->GetColor(gfx::MaterialClass::ColorIndex::ParticleMidColor)});
        mColorUniforms.push_back({"End color",
                                  gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::ParticleEndColor),
                                  material->GetColor(gfx::MaterialClass::ColorIndex::ParticleEndColor)});
    }

    if (type != gfx::MaterialClass::Type::Custom)
    {
        if (mActuator)
        {
            auto* layout = new QGridLayout;
            mUI.colorUniforms->setLayout(layout);

            for (int row=0; row<mColorUniforms.size(); ++row)
            {
                const auto& uniform = mColorUniforms[row];

                auto* label = new QLabel(this);
                SetValue(label, uniform.desc);

                auto* reset = new QToolButton(this);
                reset->setIcon(QIcon("icons:reset.png"));
                reset->setEnabled(false);

                auto* selector = new color_widgets::ColorSelector(this);
                selector->clearColor();
                selector->setPlaceholderText("No Change");

                if (const auto* param = mActuator->FindMaterialParam(uniform.name))
                {
                    if (const auto* color = std::get_if<game::Color4f>(param))
                    {
                        SetValue(selector, *color);
                        reset->setEnabled(true);
                        mKnownChanges.insert(uniform.name);
                    }
                }

                layout->addWidget(label,    row, 0);
                layout->addWidget(selector, row, 1);
                layout->addWidget(reset,    row, 2);

                QObject::connect(selector, &color_widgets::ColorSelector::colorChanged, this, [uniform, this, selector, reset](const QColor& color) {
                    mActuator->SetMaterialParam(uniform.name, gui::ToGfx(color));
                });
                QObject::connect(selector, &color_widgets::ColorSelector::acceptChange, this, [uniform, this, selector, reset]() {
                    mKnownChanges.insert(uniform.name);
                    reset->setEnabled(true);
                    // DEBUG("color change accept");
                });
                QObject::connect(selector, &color_widgets::ColorSelector::rejectChange, this, [uniform, this, selector, reset]() {
                    if (!base::Contains(mKnownChanges, uniform.name))
                        mActuator->DeleteMaterialParam(uniform.name);

                    // DEBUG("color change reject");
                });

                QObject::connect(reset, &QToolButton::clicked, this, [uniform, this, selector, reset]() {
                    mActuator->DeleteMaterialParam(uniform.name);
                    selector->clearColor();
                    reset->setEnabled(false);
                    mKnownChanges.erase(uniform.name);
                });
            }
            SetVisible(mUI.colorUniforms, true);
        }
        else
        {
            auto* layout = new QGridLayout;
            mUI.colorUniforms->setLayout(layout);

            for (int row=0; row<mColorUniforms.size(); ++row)
            {
                const auto& uniform = mColorUniforms[row];

                auto* label = new QLabel(this);
                SetValue(label, uniform.desc);

                auto* reset = new QToolButton(this);
                reset->setIcon(QIcon("icons:reset.png"));
                reset->setEnabled(false);

                auto* selector = new color_widgets::ColorSelector(this);
                selector->clearColor();
                selector->setPlaceholderText("Material Default");
                selector->setComparisonColor(FromGfx(uniform.material_default));

                if (const auto* param = mItem->FindMaterialParam(uniform.name))
                {
                    if (const auto* color = std::get_if<game::Color4f>(param))
                    {
                        SetValue(selector, *color);
                        reset->setEnabled(true);
                        mKnownChanges.insert(uniform.name);
                    }
                }

                layout->addWidget(label,    row, 0);
                layout->addWidget(selector, row, 1);
                layout->addWidget(reset,    row, 2);

                QObject::connect(selector, &color_widgets::ColorSelector::colorChanged, this, [uniform, this, selector, reset](const QColor& color) {
                    mItem->SetMaterialParam(uniform.name, gui::ToGfx(color));
                    // DEBUG("color change");
                });
                QObject::connect(selector, &color_widgets::ColorSelector::acceptChange, this, [uniform, this, selector, reset]() {
                    mKnownChanges.insert(uniform.name);
                    reset->setEnabled(true);
                    // DEBUG("color change accept");
                });
                QObject::connect(selector, &color_widgets::ColorSelector::rejectChange, this, [uniform, this, selector, reset]() {
                    if (!base::Contains(mKnownChanges, uniform.name))
                        mItem->DeleteMaterialParam(uniform.name);

                    // DEBUG("color change reject");
                });

                QObject::connect(reset, &QToolButton::clicked, this, [uniform, this, selector, reset]() {
                    mKnownChanges.erase(uniform.name);
                    mItem->DeleteMaterialParam(uniform.name);
                    selector->clearColor();
                    reset->setEnabled(false);
                });
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
            {
                SetValue(mUI.textureMaps, ListItemId(mItem->GetActiveTextureMap()));
                SetEnabled(mUI.btnResetActiveMap, true);
            }
            else
            {
                SetEnabled(mUI.btnResetActiveMap, false);
            }

            SetVisible(mUI.colorUniforms, !mColorUniforms.empty());
            SetVisible(mUI.builtInParams, !maps.empty());

            if (type == gfx::MaterialClass::Type::Tilemap)
            {
                SetVisible(mUI.lblTileIndex,      true);
                SetVisible(mUI.tileIndex,         true);
                SetVisible(mUI.btnResetTileIndex, true);
                if (const auto* ptr = mItem->GetMaterialParamValue<float>("kTileIndex"))
                {
                    SetValue(mUI.tileIndex, (int)*ptr);
                }
            }
            if (type == gfx::MaterialClass::Type::Texture ||  type == gfx::MaterialClass::Type::Sprite)
            {
                SetVisible(mUI.lblTextureScale,      true);
                SetVisible(mUI.textureScaleX,        true);
                SetVisible(mUI.textureScaleY,        true);
                SetVisible(mUI.btnResetTextureScale, true);
                mTextureScale = material->GetTextureScale();

                if (const auto* ptr = mItem->GetMaterialParamValue<glm::vec2>("kTextureScale"))
                {
                    SetValue(mUI.textureScaleX, ptr->x);
                    SetValue(mUI.textureScaleY, ptr->y);
                }
            }
        }
    }
    else if (type == gfx::MaterialClass::Type::Custom)
    {
        SetVisible(mUI.customUniforms, false);
        SetVisible(mUI.colorUniforms, false);
        SetVisible(mUI.builtInParams, false);

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

        if (!json.contains("uniforms") || !json["uniforms"].is_array() || json["uniforms"].empty())
        {
            SetValue(mUI.lblMessage, "The shader doesn't use any material parameters.");
            SetVisible(mUI.grpMessage, true);
            return;
        }

        auto ReadUniformDefaultFromJson = [](Uniform* widget, const nlohmann::json& json, Uniform::Type type) {
            // set default uniform value if it doesn't exist already.
            if (type == Uniform::Type::Float)
            {
                float value = 0.0f;
                base::JsonReadSafe(json, "value", &value);
                widget->SetValue(value);
            }
            else if (type == Uniform::Type::Vec2)
            {
                glm::vec2 value = {0.0f, 0.0f};
                base::JsonReadSafe(json, "value", &value);
                widget->SetValue(value);
            }
            else if (type == Uniform::Type::Vec3)
            {
                glm::vec3 value = {0.0f, 0.0f, 0.0f};
                base::JsonReadSafe(json, "value", &value);
                widget->SetValue(value);
            }
            else if (type == Uniform::Type::Vec4)
            {
                glm::vec4 value = {0.0f, 0.0f, 0.0f, 0.0f};
                base::JsonReadSafe(json, "value", &value);
                widget->SetValue(value);
            }
            else if (type == Uniform::Type::Color)
            {
                gfx::Color4f value = gfx::Color::White;
                base::JsonReadSafe(json, "value", &value);
                widget->SetValue(value);
            }
            else if (type == Uniform::Type::Int)
            {
                int value = 0;
                base::JsonReadSafe(json, "value", &value);
                widget->SetValue(value);
            }
            else BUG("Unhandled uniform type.");
        };

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

            auto* widget = new Uniform(this);
            widget->SetType(type);
            widget->SetName(app::FromUtf8(name));
            QObject::connect(widget, &Uniform::ValueChanged, this, &DlgMaterialParams::UniformValueChanged);
            mUniforms.push_back(widget);

            auto* override = new QCheckBox(this);
            override->setText("Override");
            override->setProperty("uniform_name", app::FromUtf8(name));
            QObject::connect(override, &QCheckBox::toggled, this, &DlgMaterialParams::ToggleUniform);

            if (mActuator)
            {
                if (const auto* uniform = mActuator->FindMaterialParam(name))
                {
                    std::visit([widget](const auto& variant_value) {
                        widget->SetValue(variant_value);
                    }, *uniform);
                    SetValue(override, true);
                    SetEnabled(widget, true);
                }
                else
                {
                    if (const auto* uniform = mItem->FindMaterialParam(name))
                    {
                        std::visit([widget](const auto& variant_value) {
                            widget->SetValue(variant_value);
                        }, *uniform);
                    }
                    else
                    {
                        ReadUniformDefaultFromJson(widget, json.value(), type);
                    }
                    SetValue(override, false);
                    SetEnabled(widget, false);
                }
            }
            else
            {
                if (const auto* uniform = mItem->FindMaterialParam(name))
                {
                    std::visit([widget](const auto& variant_value) {
                        widget->SetValue(variant_value);
                    }, *uniform);
                    SetValue(override, true);
                    SetEnabled(widget, true);
                }
                else
                {
                    ReadUniformDefaultFromJson(widget, json.value(), type);
                    SetValue(override, false);
                    SetEnabled(widget, false);
                }
            }
            layout->addWidget(label,    widget_row, 0);
            layout->addWidget(widget,   widget_row, 1);
            layout->addWidget(override, widget_row, 2);
            widget_row++;
        }
        SetVisible(mUI.customUniforms, true);
    }
}

void DlgMaterialParams::on_textureMaps_currentIndexChanged(int)
{
    if (mActuator)
    {
        // todo: active texture map animation support

        SetEnabled(mUI.btnResetActiveMap, true);
    }
    else
    {
        mItem->SetActiveTextureMap(GetItemId(mUI.textureMaps));
        SetEnabled(mUI.btnResetActiveMap, true);
    }
}

void DlgMaterialParams::on_btnResetActiveMap_clicked()
{
    SetValue(mUI.textureMaps, -1);
    SetEnabled(mUI.btnResetActiveMap, false);
    if (mActuator)
    {
        // todo: active texture map animation support
    }
    else
    {
        mItem->ResetActiveTextureMap();
    }
}

void DlgMaterialParams::on_tileIndex_valueChanged(int value)
{
    mItem->SetMaterialParam("kTileIndex", (float)value);
}
void DlgMaterialParams::on_btnResetTileIndex_clicked()
{
    mItem->DeleteMaterialParam("kTileIndex");
    SetValue(mUI.tileIndex, 0);
}

void DlgMaterialParams::on_textureScaleX_valueChanged(double)
{
    auto scale = mTextureScale;
    if (const auto* ptr = mItem->GetMaterialParamValue<glm::vec2>("kTextureScale"))
        scale = *ptr;

    scale.x = GetValue(mUI.textureScaleX);
    mItem->SetMaterialParam("kTextureScale", scale);
}
void DlgMaterialParams::on_textureScaleY_valueChanged(double)
{
    auto scale = mTextureScale;
    if (const auto* ptr = mItem->GetMaterialParamValue<glm::vec2>("kTextureScale"))
        scale = *ptr;

    scale.y = GetValue(mUI.textureScaleY);
    mItem->SetMaterialParam("kTextureScale", scale);
}
void DlgMaterialParams::on_btnResetTextureScale_clicked()
{
    mItem->DeleteMaterialParam("kTextureScale");
    SetValue(mUI.textureScaleX, mTextureScale.x);
    SetValue(mUI.textureScaleY, mTextureScale.y);
}

void DlgMaterialParams::on_btnAccept_clicked()
{
    accept();

    if (!mActuator)
        return;

    // if we're using this dialog to adjust material actuator
    // then let's make sure that the uniforms exist in the
    // drawable item, and if they don't then let's initialize
    // them based on the material defaults. The value is needed
    // as the initial value for the uniform interpolation.

    for (const auto& uniform : mColorUniforms)
    {
        if (const auto* param = mActuator->FindMaterialParam(uniform.name))
        {
            if (!mItem->HasMaterialParam(uniform.name))
            {
                mItem->SetMaterialParam(uniform.name, uniform.material_default);
                DEBUG("Added new drawable item material uniform for animation. [uniform=%1]", uniform.name);
            }
        }
    }

    for (const auto& uniform : mUniforms)
    {
        if (const auto* param = mActuator->FindMaterialParam(uniform->GetName()))
        {
            if (!mItem->HasMaterialParam(uniform->GetName()))
            {
                SetMaterialParam(mItem, uniform);
                DEBUG("Added new drawable item material uniform for animation. [uniform=%1]", uniform->GetName());
            }
        }
    }
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
    {
        SetMaterialParam(mActuator, widget);
    }
    else
    {
        SetMaterialParam(mItem, widget);
    }
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
