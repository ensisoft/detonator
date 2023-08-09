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
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>

#include "base/assert.h"
#include "base/logging.h"
#include "base/utility.h"
#include "base/json.h"
#include "base/math.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/drawing.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "engine/ui.h"
#include "engine/classlib.h"
#include "engine/data.h"
#include "engine/loader.h"

namespace {
bool ReadColor(const nlohmann::json& json, const std::string& name, gfx::Color4f* out)
{
    if (base::JsonReadSafe(json, name.c_str(), out))
        return true;

    gfx::Color value = gfx::Color::Black;
    if (!base::JsonReadSafe(json, name.c_str(), &value))
        return false;
    *out = gfx::Color(value);
    return true;
}
} // namespace

namespace engine
{
namespace detail {

UIMaterial::MaterialClass UIGradient::GetClass(const ClassLibrary*, const Loader*) const
{
    auto material = std::make_shared<gfx::GradientClass>(gfx::MaterialClass::Type::Gradient);
    material->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    material->SetColor(mColorMap[0], ColorIndex::TopLeft);
    material->SetColor(mColorMap[1], ColorIndex::TopRight);
    material->SetColor(mColorMap[2], ColorIndex::BottomLeft);
    material->SetColor(mColorMap[3], ColorIndex::BottomRight);
    material->SetName("UIGradient");
    if (mGamma)
        material->SetGamma(mGamma.value());
    return material;
}
bool UIGradient::FromJson(const nlohmann::json& json)
{
    if (!ReadColor(json, "color0", &mColorMap[0]) ||
        !ReadColor(json, "color1", &mColorMap[1]) ||
        !ReadColor(json, "color2", &mColorMap[2]) ||
        !ReadColor(json, "color3", &mColorMap[3]))
        return false;
    float gamma = 1.0f;
    if (base::JsonReadSafe(json, "gamma", &gamma))
        mGamma = gamma;
    return true;
}
void UIGradient::IntoJson(nlohmann::json& json) const
{
    base::JsonWrite(json, "color0", mColorMap[0]);
    base::JsonWrite(json, "color1", mColorMap[1]);
    base::JsonWrite(json, "color2", mColorMap[2]);
    base::JsonWrite(json, "color3", mColorMap[3]);
    if (mGamma)
        base::JsonWrite(json, "gamma", mGamma.value());
}

UIMaterial::MaterialClass UIColor::GetClass(const ClassLibrary*, const Loader*) const
{
    auto material = std::make_shared<gfx::ColorClass>(gfx::MaterialClass::Type::Color);
    material->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    material->SetBaseColor(mColor);
    material->SetName("UIColor");
    return material;
}

bool UIColor::FromJson(const nlohmann::json& json)
{
    return ReadColor(json, "color", &mColor);
}

void UIColor::IntoJson(nlohmann::json& json) const
{
    base::JsonWrite(json, "color", mColor);
}

UIMaterial::MaterialClass UIMaterialReference::GetClass(const ClassLibrary* classlib, const Loader*) const
{
    auto klass = classlib->FindMaterialClassById(mMaterialId);
    // currently, the material style that is associated with a paint struct
    // can use class names too. so also try to find with the name.
    if (klass == nullptr)
        klass = classlib->FindMaterialClassByName(mMaterialId);
    if (klass == nullptr)
        WARN("Unresolved UI material. [material='%1']", mMaterialId);
    return klass;
}

bool UIMaterialReference::FromJson(const nlohmann::json& json)
{
    if (!base::JsonReadSafe(json, "material", &mMaterialId))
        return false;
    return true;
}
void UIMaterialReference::IntoJson(nlohmann::json& json) const
{
    base::JsonWrite(json, "material", mMaterialId);
}

bool UIMaterialReference::IsAvailable(const ClassLibrary& loader) const
{
    auto klass = loader.FindMaterialClassById(mMaterialId);
    return klass != nullptr;
}

UIMaterial::MaterialClass UITexture::GetClass(const ClassLibrary*, const Loader* loader) const
{
    ASSERT(loader);

    auto material = std::make_shared<gfx::TextureMap2DClass>(gfx::MaterialClass::Type::Texture);
    material->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    material->SetTexture(gfx::LoadTextureFromFile(mTextureUri));
    material->SetName("UITexture");
    material->GetTextureMap(0)->GetTextureSource(0)->SetName("UITexture/" + mTextureName);

    if (mMetafileUri.empty() || mTextureName.empty())
        return material;

    const auto& data = loader->LoadEngineDataUri(mMetafileUri);
    if (!data)
    {
        WARN("Failed to load packed UITexture texture descriptor meta file. [uri='%1']", mMetafileUri);
        return material;
    }
    DEBUG("Loaded UITexture descriptor meta file. [uri='%1']", mMetafileUri);
    const auto* beg = (const char*)data->GetData();
    const auto* end = (const char*)data->GetData() + data->GetByteSize();
    const auto& [ok, json, error] = base::JsonParse(beg, end);
    if (!ok)
    {
        WARN("Failed to parse packed UITexture JSON. [uri='%1', error='%2']", mMetafileUri, error);
        return material;
    }

    unsigned img_width_px  = 0;
    unsigned img_height_px = 0;
    base::JsonReadSafe(json, "image_width", &img_width_px);
    base::JsonReadSafe(json, "image_height", &img_height_px);
    if (!img_width_px || !img_height_px)
    {
        WARN("Packed UITexture texture size not know. [uri='%1']", mMetafileUri);
        return material;
    }

    unsigned img_rect_width_px  = 0;
    unsigned img_rect_height_px = 0;
    unsigned img_rect_xpos_px = 0;
    unsigned img_rect_ypos_px = 0;
    for (const auto& item : json["images"].items())
    {
        const auto& img_json = item.value();
        std::string name;
        base::JsonReadSafe(img_json, "name", &name);
        if (name != mTextureName)
            continue;

        base::JsonReadSafe(img_json, "width", &img_rect_width_px);
        base::JsonReadSafe(img_json, "height", &img_rect_height_px);
        base::JsonReadSafe(img_json, "xpos", &img_rect_xpos_px);
        base::JsonReadSafe(img_json, "ypos", &img_rect_ypos_px);
        break;
    }
    if (!img_rect_width_px || !img_rect_height_px)
    {
        WARN("Packed UITexture sub rectangle description not found. [uri='%1', name='%2']", mMetafileUri, mTextureName);
        return material;
    }
    gfx::FRect  rect;
    rect.SetX(img_rect_xpos_px / (float)img_width_px);
    rect.SetY(img_rect_ypos_px / (float)img_height_px);
    rect.SetWidth(img_rect_width_px / (float)img_width_px);
    rect.SetHeight(img_rect_height_px / (float)img_height_px);
    material->SetTextureRect(rect);
    return material;
}

bool UITexture::FromJson(const nlohmann::json& json)
{
    base::JsonReadSafe(json, "texture", &mTextureUri);
    base::JsonReadSafe(json, "metafile", &mMetafileUri);
    base::JsonReadSafe(json, "name", &mTextureName);
    return true;
}

void UITexture::IntoJson(nlohmann::json& json) const
{
    base::JsonWrite(json, "texture", mTextureUri);
    base::JsonWrite(json, "metafile", mMetafileUri);
    base::JsonWrite(json, "name", mTextureName);
}

} // detail

UIStyle::MaterialClass UIStyle::MakeMaterial(const std::string& str) const
{
    ASSERT(mClassLib);
    auto [ok, json, error] = base::JsonParse(str);
    if (!ok)
    {
        ERROR("Failed to parse UI style material string. [error='%1']", error);
        return nullptr;
    }
    UIMaterial::Type type;
    if (!base::JsonReadSafe(json, "type", &type))
    {
        ERROR("Failed to resolve UI style material string material type.");
        return nullptr;
    }
    std::unique_ptr<UIMaterial> factory;
    if (type == UIMaterial::Type::Null)
        factory.reset(new detail::UINullMaterial());
    else if (type == UIMaterial::Type::Color)
        factory.reset(new detail::UIColor);
    else if (type == UIMaterial::Type::Gradient)
        factory.reset(new detail::UIGradient);
    else if (type == UIMaterial::Type::Reference)
        factory.reset(new detail::UIMaterialReference);
    else if (type == UIMaterial::Type::Texture)
        factory.reset(new detail::UITexture);
    else BUG("Unhandled material type.");

    if (!factory->FromJson(json))
    {
        WARN("Failed to parse UI style material string.");
        return nullptr;
    }
    return factory->GetClass(mClassLib, mLoader);
}

std::optional<UIStyle::MaterialClass> UIStyle::GetMaterial(const std::string& key) const
{
    auto it = mMaterials.find(key);
    if (it == mMaterials.end())
        return std::nullopt;
    return it->second->GetClass(mClassLib, mLoader);
}

UIProperty UIStyle::GetProperty(const std::string& key) const
{
    auto it = mProperties.find(key);
    if (it == mProperties.end())
        return UIProperty();
    return UIProperty(it->second);
}

bool UIStyle::ParseStyleString(const std::string& tag, const std::string& style)
{
    auto [ok, json, error] = base::JsonParse(style);
    if (!ok)
    {
        ERROR("Failed to parse UI style string. [tag=%1, error='%2']", tag, error);
        return false;
    }
    std::vector<PropertyPair> props;
    if (!ParseProperties(json, props))
        return false;

    std::vector<MaterialPair> materials;
    if (!ParseMaterials(json, materials))
        return false;

    for (auto& p : props)
    {
        if (base::StartsWith(p.key, tag))
            mProperties[p.key] = std::move(p.value);
        else mProperties[tag + "/" + p.key] = std::move(p.value);
    }
    for (auto& m : materials)
    {
        if (base::StartsWith(m.key, tag))
            mMaterials[m.key] = std::move(m.material);
        else mMaterials[tag + "/" + m.key] = std::move(m.material);
    }
    return true;
}

bool UIStyle::HasProperty(const std::string& key) const
{ return mProperties.find(key) != mProperties.end(); }
bool UIStyle::HasMaterial(const std::string& key) const
{ return mMaterials.find(key) != mMaterials.end(); }

void UIStyle::DeleteProperty(const std::string& key)
{
    auto it = mProperties.find(key);
    if (it == mProperties.end())
        return;
    mProperties.erase(it);
}

void UIStyle::DeleteProperties(const std::string& filter)
{
    for (auto it = mProperties.begin(); it != mProperties.end();)
    {
        const auto& key = (*it).first;
        if (base::Contains(key, filter))
            it = mProperties.erase(it);
        else ++it;
    }
}
void UIStyle::GatherProperties(const std::string& filter, std::vector<PropertyKeyValue>* props) const
{
    for (const auto& pair : mProperties)
    {
        const auto& key = pair.first;
        if (!base::Contains(key, filter))
            continue;
        PropertyKeyValue kv;
        kv.key  = key;
        kv.prop = pair.second;
        props->push_back(std::move(kv));
    }
}


void UIStyle::DeleteMaterial(const std::string& key)
{
    auto it = mMaterials.find(key);
    if (it == mMaterials.end())
        return;
    mMaterials.erase(it);
}

void UIStyle::DeleteMaterials(const std::string& filter)
{
    for (auto it = mMaterials.begin(); it != mMaterials.end();)
    {
        const auto& key = (*it).first;
        if (base::Contains(key, filter))
            it = mMaterials.erase(it);
        else ++it;
    }
}

const UIMaterial* UIStyle::GetMaterialType(const std::string& key) const
{
    auto it = mMaterials.find(key);
    if (it == mMaterials.end())
        return nullptr;
    return (*it).second.get();
}

bool UIStyle::LoadStyle(const nlohmann::json& json)
{
    std::vector<PropertyPair> props;
    if (!ParseProperties(json, props))
        return false;
    std::vector<MaterialPair> materials;
    if (!ParseMaterials(json, materials))
        return false;

    for (auto& p : props)
    {
        mProperties[p.key] = std::move(p.value);
    }
    for (auto& m : materials)
    {
        mMaterials[m.key] = std::move(m.material);
    }
    return true;
}
bool UIStyle::LoadStyle(const EngineData& data)
{
    const auto* beg = (const char*)data.GetData();
    const auto* end = beg + data.GetByteSize();
    auto [ok, json, error] = base::JsonParse(beg, end);
    if (!ok)
    {
        ERROR("UI style load failed with JSON parse error. [error='%1', file='%2']", error, data.GetSourceName());
        return false;
    }
    return LoadStyle(json);
}

void UIStyle::SaveStyle(nlohmann::json& json) const
{
    for (const auto& [key, val] : mProperties)
    {
        nlohmann::json prop;
        base::JsonWrite(prop, "key", key);
        if (const auto* color = std::any_cast<gfx::Color4f>(&val))
            base::JsonWrite(prop, "value", *color);
        else if(const auto* string = std::any_cast<std::string>(&val))
            base::JsonWrite(prop, "value", *string);
        else if (const auto* integer= std::any_cast<int>(&val))
            base::JsonWrite(prop, "value", *integer);
        else if (const auto* integer = std::any_cast<unsigned>(&val))
            base::JsonWrite(prop, "value", *integer);
        else if (const auto* real = std::any_cast<float>(&val))
            base::JsonWrite(prop, "value", *real);
        else if (const auto* real = std::any_cast<double>(&val))
            base::JsonWrite(prop, "value", *real);
        else if (const auto* boolean = std::any_cast<bool>(&val))
            base::JsonWrite(prop, "value", *boolean);
        else BUG("Unsupported property type.");
        json["properties"].push_back(std::move(prop));
    }
    for (const auto& [key, mat] : mMaterials)
    {
        nlohmann::json material;
        base::JsonWrite(material, "key", key);
        base::JsonWrite(material, "type", mat->GetType());
        mat->IntoJson(material);
        json["materials"].push_back(std::move(material));
    }
}

std::string UIStyle::MakeStyleString(const std::string& filter) const
{
    nlohmann::json json;
    for (const auto& p : mProperties)
    {
        const auto& key = p.first;
        const auto& val = p.second;
        if (!base::Contains(p.first, filter))
             continue;
        nlohmann::json prop;
        base::JsonWrite(prop, "key", key);

        if (const auto* color = std::any_cast<gfx::Color4f>(&val))
            base::JsonWrite(prop, "value", *color);
        else if(const auto* string = std::any_cast<std::string>(&val))
            base::JsonWrite(prop, "value", *string);
        else if (const auto* integer= std::any_cast<int>(&val))
            base::JsonWrite(prop, "value", *integer);
        else if (const auto* integer = std::any_cast<unsigned>(&val))
            base::JsonWrite(prop, "value", *integer);
        else if (const auto* real = std::any_cast<float>(&val))
            base::JsonWrite(prop, "value", *real);
        else if (const auto* real = std::any_cast<double>(&val))
            base::JsonWrite(prop, "value", *real);
        else if (const auto* boolean = std::any_cast<bool>(&val))
            base::JsonWrite(prop, "value", *boolean);
        else BUG("Unsupported property type.");
        json["properties"].push_back(std::move(prop));
    }
    for (const auto& p : mMaterials)
    {
        const auto& key = p.first;
        const auto& mat = p.second;
        if (!base::Contains(p.first, filter))
            continue;

        nlohmann::json material;
        base::JsonWrite(material, "key", key);
        base::JsonWrite(material, "type", mat->GetType());
        mat->IntoJson(material);
        json["materials"].push_back(std::move(material));
    }
    // if JSON object is "empty" then explicitly return an empty string
    // calling dump on an empty json object returns "null" string.
    if (json.empty())
        return "";
    return json.dump();
}

void UIStyle::ListMaterials(std::vector<MaterialEntry>* materials) const
{
    for (const auto& [key, material] : mMaterials)
    {
        MaterialEntry me;
        me.key = key;
        me.material = material.get();
        materials->push_back(std::move(me));
    }
}

void UIStyle::GatherMaterials(const std::string& filter, std::vector<MaterialEntry>* materials) const
{
    for (const auto& [key, material] : mMaterials)
    {
        if (!base::Contains(key, filter))
            continue;
        MaterialEntry entry;
        entry.key      = key;
        entry.material = material.get();
        materials->push_back(entry);
    }
}

bool UIStyle::PurgeUnavailableMaterialReferences()
{
    bool purged = false;
    for (auto it = mMaterials.begin(); it != mMaterials.end();)
    {
        auto* source = it->second.get();
        if (source->IsAvailable(*mClassLib))
        {
            ++it;
            continue;
        }
        it = mMaterials.erase(it);
        purged = true;
    }
    return purged;
}

// static
bool UIStyle::ParseProperties(const nlohmann::json& json, std::vector<PropertyPair>& props)
{
    if (!json.contains("properties"))
        return true;

    auto success = true;

    for (const auto& item : json["properties"].items())
    {
        const auto& json = item.value();

        gfx::Color4f color;
        std::string str_value;
        int int_value = 0;
        unsigned uint_value = 0;
        float float_value = 0.0f;
        bool bool_value   = false;

        std::string key;
        if (!base::JsonReadSafe(json, "key", &key))
        {
            WARN("Ignored JSON UI Style property without property key.");
            success = false;
            continue;
        }

        PropertyPair prop;
        if (base::JsonReadSafe(json, "value", &int_value))
            prop.value = int_value;
        else if (base::JsonReadSafe(json, "value", &uint_value))
            prop.value = uint_value;
        else if (base::JsonReadSafe(json, "value", &float_value))
            prop.value = float_value;
        else if (base::JsonReadSafe(json, "value", &str_value))
            prop.value = str_value;
        else if (base::JsonReadSafe(json, "value", &color))
            prop.value = color;
        else if (base::JsonReadSafe(json, "value", &bool_value))
            prop.value = bool_value;
        else {
            // this is not necessarily a BUG because currently the style json files are
            // hand written. thus we have to be prepared to handle unexpected cases.
            success = false;
            WARN("Ignoring unexpected UI style property type for key '%1'. ", key);
            continue;
        }
        prop.key = key;
        props.push_back(std::move(prop));
    }
    return success;
}

// static
bool UIStyle::ParseMaterials(const nlohmann::json& json, std::vector<MaterialPair>& materials)
{
    if (!json.contains("materials"))
        return true;

    auto success = true;
    for (const auto& item : json["materials"].items())
    {
        const auto& json = item.value();
        UIMaterial::Type type;
        std::string key;
        if (!base::JsonReadSafe(json, "type", &type))
        {
            WARN("Ignored JSON UI style material property with unrecognized type.");
            success = false;
            continue;
        }
        else if (!base::JsonReadSafe(json, "key", &key))
        {
            WARN("Ignored JSON UI style material property without material key.");
            success = false;
            continue;
        }

        std::unique_ptr<UIMaterial> material;
        if (type == UIMaterial::Type::Null)
            material.reset(new detail::UINullMaterial());
        else if (type == UIMaterial::Type::Color)
            material.reset(new detail::UIColor);
        else if (type == UIMaterial::Type::Gradient)
            material.reset(new detail::UIGradient);
        else if (type == UIMaterial::Type::Reference)
            material.reset(new detail::UIMaterialReference);
        else if (type == UIMaterial::Type::Texture)
            material.reset(new detail::UITexture);
        else BUG("Unhandled material type.");
        if (!material->FromJson(json))
        {
            success = false;
            WARN("Failed to parse UI Material '%1'", key);
            continue;
        }

        MaterialPair p;
        p.key = std::move(key);
        p.material = std::move(material);
        materials.push_back(std::move(p));
    }
    return success;
}

void UIPainter::DrawWidgetBackground(const WidgetId& id, const PaintStruct& ps) const
{
    if (const auto* material = GetWidgetMaterial(id, ps, "background"))
    {
        const auto shape = GetWidgetProperty(id, ps, "shape", UIStyle::WidgetShape::Rectangle);
        FillShape(ps.rect, *material, shape);
    }
}
void UIPainter::DrawWidgetBorder(const WidgetId& id, const PaintStruct& ps) const
{
    if (const auto* material = GetWidgetMaterial(id, ps, "border"))
    {
        const auto width = GetWidgetProperty(id, ps, "border-width", 1.0f);
        const auto shape = GetWidgetProperty(id, ps, "shape", UIStyle::WidgetShape::Rectangle);
        OutlineShape(ps.rect, *material, shape, width);
    }
}

void UIPainter::DrawStaticText(const WidgetId& id, const PaintStruct& ps, const std::string& text, float line_height) const
{
    if (text.empty())
        return;

    const auto& text_color = GetWidgetProperty(id, ps, "text-color", uik::Color4f(uik::Color::White));
    const auto& text_blink = GetWidgetProperty(id, ps, "text-blink", false);
    const auto& text_underline = GetWidgetProperty(id, ps, "text-underline",false);
    const auto& font_name = GetWidgetProperty(id, ps, "text-font",std::string(""));
    const auto  font_size = GetWidgetProperty(id, ps, "text-size",16);
    const auto va = GetWidgetProperty(id, ps, "text-vertical-align", UIStyle::VerticalTextAlign::Center);
    const auto ha = GetWidgetProperty(id, ps, "text-horizontal-align", UIStyle::HorizontalTextAlign::Center);
    line_height = GetWidgetProperty(id, ps, "text-line-height", line_height);

    unsigned alignment  = 0;
    unsigned properties = 0;
    if (text_blink)
        properties |= gfx::TextProp::Blinking;
    if (text_underline)
        properties |= gfx::TextProp::Underline;

    if (ha == UIStyle::HorizontalTextAlign::Left)
        alignment |= gfx::TextAlign::AlignLeft;
    else if (ha == UIStyle::HorizontalTextAlign::Center)
        alignment |= gfx::TextAlign::AlignHCenter;
    else if (ha == UIStyle::HorizontalTextAlign::Right)
        alignment |= gfx::TextAlign::AlignRight;

    if (va == UIStyle::VerticalTextAlign::Top)
        alignment |= gfx::TextAlign::AlignTop;
    else if (va == UIStyle::VerticalTextAlign::Center)
        alignment |= gfx::TextAlign::AlignVCenter;
    else if (va == UIStyle::VerticalTextAlign::Bottom)
        alignment |= gfx::TextAlign::AlignBottom;

    gfx::DrawTextRect(*mPainter, text, font_name, font_size, ps.rect, text_color, alignment, properties, line_height);
}

void UIPainter::DrawEditableText(const WidgetId& id, const PaintStruct& ps, const EditableText& text) const
{
    if (text.text.empty())
        return;

    const auto& text_color = GetWidgetProperty(id, ps, "edit-text-color", uik::Color4f(uik::Color::White));
    const auto& font_name  = GetWidgetProperty(id, ps, "edit-text-font",std::string(""));
    const auto  font_size  = GetWidgetProperty(id, ps, "edit-text-size",16);
    const unsigned alignment  = gfx::TextAlign::AlignVCenter | gfx::TextAlign::AlignLeft;
    const unsigned properties = 0;
    gfx::DrawTextRect(*mPainter, text.text, font_name, font_size, ps.rect, text_color, alignment, properties, 1.0f);
}

void UIPainter::DrawTextEditBox(const WidgetId& id, const PaintStruct& ps) const
{
    if (const auto* material = GetWidgetMaterial(id, ps, "text-edit-background"))
    {
        const auto shape = GetWidgetProperty(id, ps, "text-edit-shape", UIStyle::WidgetShape::RoundRect);
        FillShape(ps.rect, *material, shape);
    }
    if (const auto* material = GetWidgetMaterial(id, ps, "text-edit-border"))
    {
        const auto width = GetWidgetProperty(id, ps, "text-edit-border-width", 1.0f);
        const auto shape = GetWidgetProperty(id, ps, "text-edit-shape", UIStyle::WidgetShape::RoundRect);
        OutlineShape(ps.rect, *material, shape, width);
    }
}

void UIPainter::DrawWidgetFocusRect(const WidgetId& id, const PaintStruct& ps) const
{
    if (const auto* material = GetWidgetMaterial(id, ps, "focus-rect"))
    {
        const auto button_shape = GetWidgetProperty(id, ps, "button-shape", UIStyle::WidgetShape::RoundRect);
        const auto rect_shape = GetWidgetProperty(id, ps, "focus-rect-shape", button_shape);
        const auto rect_width = GetWidgetProperty(id, ps, "focus-rect-width", 1.0f);

        gfx::FRect rect = ps.rect;
        rect.Grow(-4.0f, -4.0f);
        rect.Translate(2.0f, 2.0f);
        OutlineShape(rect, *material, rect_shape, rect_width);
    }
}

void UIPainter::DrawCheckBox(const WidgetId& id, const PaintStruct& ps , bool checked) const
{
    if (const auto* material = GetWidgetMaterial(id, ps, "check-background"))
    {
        const auto shape = GetWidgetProperty(id, ps, "check-shape", UIStyle::WidgetShape::Rectangle);
        FillShape(ps.rect, *material, shape);
    }
    if (const auto* material = GetWidgetMaterial(id, ps, "check-border"))
    {
        const auto width = GetWidgetProperty(id, ps, "check-border-width", 1.0f);
        const auto shape = GetWidgetProperty(id, ps, "check-shape", UIStyle::WidgetShape::Rectangle);
        OutlineShape(ps.rect, *material, shape, width);
    }

    if (const auto* material = GetWidgetMaterial(id, ps, checked ? "check-mark-checked" : "check-mark-unchecked"))
    {
        const auto shape = GetWidgetProperty(id, ps, "check-mark-shape", UIStyle::WidgetShape::RoundRect);
        gfx::FRect mark;
        mark.Move(ps.rect.GetPosition());
        mark.Resize(ps.rect.GetSize());
        mark.Grow(-6.0f, -6.0f);
        mark.Translate(3.0f, 3.0f);
        FillShape(mark, *material, shape);
    }
}

void UIPainter::DrawRadioButton(const WidgetId& id, const PaintStruct& ps, bool selected) const
{
    if (const auto* material = GetWidgetMaterial(id, ps, "check-background"))
    {
        const auto shape = GetWidgetProperty(id, ps, "check-shape", UIStyle::WidgetShape::Circle);
        FillShape(ps.rect, *material, shape);
    }
    if (const auto* material = GetWidgetMaterial(id, ps, "check-border"))
    {
        const auto width = GetWidgetProperty(id, ps, "check-border-width", 1.0f);
        const auto shape = GetWidgetProperty(id, ps, "check-shape", UIStyle::WidgetShape::Circle);
        OutlineShape(ps.rect, *material, shape, width);
    }
    const auto* check_mark_name = selected ? "check-mark-checked" : "check-mark-unchecked";

    if (const auto* material = GetWidgetMaterial(id, ps, check_mark_name))
    {
        const auto shape = GetWidgetProperty(id, ps, "check-mark-shape", UIStyle::WidgetShape::Circle);
        gfx::FRect mark;
        mark.Move(ps.rect.GetPosition());
        mark.Resize(ps.rect.GetSize());
        mark.Grow(-6.0f, -6.0f);
        mark.Translate(3.0f, 3.0f);
        FillShape(mark, *material, shape);
    }
}

void UIPainter::DrawButton(const WidgetId& id, const PaintStruct& ps, ButtonIcon btn) const
{
    if  (const auto* material = GetWidgetMaterial(id, ps, "button-background"))
    {
        const auto shape = GetWidgetProperty(id, ps, "button-shape", UIStyle::WidgetShape::RoundRect);
        FillShape(ps.rect, *material, shape);
    }
    if (const auto* material = GetWidgetMaterial(id, ps, "button-border"))
    {
        const auto width = GetWidgetProperty(id, ps, "button-border-width", 1.0f);
        const auto shape = GetWidgetProperty(id, ps, "button-shape", UIStyle::WidgetShape::RoundRect);
        OutlineShape(ps.rect, *material, shape, width);
    }
    if (btn == ButtonIcon::None)
        return;

    const auto btn_width  = ps.rect.GetWidth();
    const auto btn_height = ps.rect.GetHeight();
    const auto min_side = std::min(btn_width, btn_height);
    const auto ico_size = min_side * 0.4f;

    // previously the only way to customize the button icon was to set a material
    // that would get applied to all button icons. this however would make it difficult
    // to use for example pre-rendered button textures (via materials) as up/down buttons.
    // therefore, an alternative/additional mechanism is added here which allows each
    // button to be customized via a specific button material name. if no such button
    // specific material is found then we render the button icon the built-in (old) way.
    if (btn == ButtonIcon::ArrowUp)
    {
        gfx::Transform icon;
        icon.Resize(ico_size, ico_size);
        icon.MoveTo(ps.rect.GetPosition());
        icon.Translate(btn_width*0.5, btn_height*0.5);
        icon.Translate(ico_size*-0.5, ico_size*-0.5);
        if (const auto* material = GetWidgetMaterial(id, ps, "button-icon-arrow-up"))
            mPainter->Draw(gfx::Rectangle(), icon, *material);
        else if (const auto* material = GetWidgetMaterial(id, ps, "button-icon"))
            mPainter->Draw(gfx::IsoscelesTriangle(), icon, *material);
    }
    else if (btn == ButtonIcon::ArrowDown)
    {
        // expecting that the material would have something like a pre-rendered down button
        // so in this case the geometry should not be rotated.
        float rotation = 0.0f;
        if (!GetWidgetMaterial(id, ps,"button-icon-arrow-down"))
            rotation = math::Pi;

        gfx::Transform icon;
        icon.Resize(ico_size, ico_size);
        icon.Translate(ico_size*-0.5, ico_size*-0.5);
        icon.RotateAroundZ(rotation);
        icon.Translate(ico_size*0.5, ico_size*0.5);
        icon.Translate(ps.rect.GetPosition());
        icon.Translate(btn_width*0.5, btn_height*0.5);
        icon.Translate(ico_size*-0.5, ico_size*-0.5);
        if (const auto* material = GetWidgetMaterial(id, ps, "button-icon-arrow-down"))
            mPainter->Draw(gfx::Rectangle(), icon, *material);
        else if (const auto* material = GetWidgetMaterial(id, ps, "button-icon"))
            mPainter->Draw(gfx::IsoscelesTriangle(), icon, *material);
    }
}

void UIPainter::DrawSlider(const WidgetId& id, const PaintStruct& ps, const uik::FRect& knob) const
{
    if (const auto* material = GetWidgetMaterial(id, ps, "slider-background"))
    {
        const auto shape = GetWidgetProperty(id, ps, "slider-shape", UIStyle::WidgetShape::RoundRect);
        FillShape(ps.rect, *material, shape);
    }
    if (ps.focused)
    {
        if (const auto* material = GetWidgetMaterial(id, ps, "focus-rect"))
        {
            const auto slider_shape = GetWidgetProperty(id, ps, "slider-shape", UIStyle::WidgetShape::RoundRect);
            const auto rect_shape = GetWidgetProperty(id, ps, "focus-rect-shape", slider_shape);
            const auto rect_width = GetWidgetProperty(id, ps, "focus-rect-width", 1.0f);

            gfx::FRect rect = ps.rect;
            rect.Grow(-4.0f, -4.0f);
            rect.Translate(2.0f, 2.0f);
            OutlineShape(rect, *material, rect_shape, rect_width);
        }
    }

    if (const auto* material = GetWidgetMaterial(id, ps, "slider-knob"))
    {
        const auto shape = GetWidgetProperty(id, ps, "slider-knob-shape", UIStyle::WidgetShape::RoundRect);
        FillShape(knob, *material, shape);
    }
    if (const auto* material = GetWidgetMaterial(id, ps, "slider-knob-border"))
    {
        const auto shape = GetWidgetProperty(id, ps, "slider-knob-shape", UIStyle::WidgetShape::RoundRect);
        const auto width = GetWidgetProperty(id, ps, "slider-knob-border-width", 1.0f);
        OutlineShape(knob, *material, shape, width);
    }

    if (const auto* material = GetWidgetMaterial(id, ps, "slider-border"))
    {
        const auto shape = GetWidgetProperty(id, ps, "slider-shape", UIStyle::WidgetShape::RoundRect);
        const auto width = GetWidgetProperty(id, ps, "slider-border-width", 1.0f);
        OutlineShape(ps.rect, *material, shape, width);
    }
}

void UIPainter::DrawProgressBar(const WidgetId& id, const PaintStruct& ps, std::optional<float> percentage) const
{
    if (const auto* material = GetWidgetMaterial(id, ps, "progress-bar-background"))
    {
        const auto shape = GetWidgetProperty(id, ps, "progress-bar-shape", UIStyle::WidgetShape::RoundRect);
        FillShape(ps.rect, *material, shape);
    }

    if (const auto* material = GetWidgetMaterial(id, ps, "progress-bar-fill"))
    {
        const auto shape = GetWidgetProperty(id, ps, "progress-bar-fill-shape", UIStyle::WidgetShape::RoundRect);
        if (percentage.has_value())
        {
            const auto value = percentage.value();
            auto fill = ps.rect;
            fill.SetWidth(ps.rect.GetWidth() * value);
            FillShape(fill, *material, shape);
        }
        else
        {
            const auto width  = ps.rect.GetWidth();
            const auto height = ps.rect.GetHeight();
            const auto progress_width   = ps.rect.GetWidth();
            const auto indicator_width  = progress_width * 0.2;
            const auto indicator_height = height;

            const auto duration = 2.0;
            const auto reminder = fmodf(ps.time, duration);
            const auto value = std::sin(reminder/duration * math::Pi*2.0);

            gfx::FRect indicator;
            indicator.SetWidth(indicator_width);
            indicator.SetHeight(indicator_height);
            indicator.Move(ps.rect.GetPosition());
            indicator.Translate(progress_width*0.5, 0.0f);
            indicator.Translate(-indicator_width*0.5, 0.0f);
            indicator.Translate(value * 0.8 * 0.5 * progress_width, 0.0f);
            FillShape(indicator, *material, shape);
        }
    }

    if (const auto* material = GetWidgetMaterial(id, ps, "progress-bar-border"))
    {
        const auto shape = GetWidgetProperty(id, ps, "progress-bar-shape", UIStyle::WidgetShape::RoundRect);
        const auto width = GetWidgetProperty(id, ps, "progress-bar-border-width", 1.0f);
        OutlineShape(ps.rect, *material, shape, width);
    }
}

void UIPainter::DrawToggle(const WidgetId& id, const PaintStruct& ps, const uik::FRect& knob, bool on_off) const
{
    if (const auto* material = GetWidgetMaterial(id, ps, on_off ? "toggle-background-on" : "toggle-background-off"))
    {
        const auto shape = GetWidgetProperty(id, ps, "toggle-shape", UIStyle::WidgetShape::RoundRect);
        FillShape(ps.rect, *material, shape);
    }
    if (ps.focused)
    {
        if (const auto* material = GetWidgetMaterial(id, ps, "focus-rect"))
        {
            const auto slider_shape = GetWidgetProperty(id, ps, "toggle-shape", UIStyle::WidgetShape::RoundRect);
            const auto rect_shape = GetWidgetProperty(id, ps, "focus-rect-shape", slider_shape);
            const auto rect_width = GetWidgetProperty(id, ps, "focus-rect-width", 1.0f);

            gfx::FRect rect = ps.rect;
            rect.Grow(-4.0f, -4.0f);
            rect.Translate(2.0f, 2.0f);
            OutlineShape(rect, *material, rect_shape, rect_width);
        }
    }

    if (const auto* material = GetWidgetMaterial(id, ps, on_off ? "toggle-knob-on" : "toggle-knob-off"))
    {
        const auto shape = GetWidgetProperty(id, ps, "toggle-knob-shape", UIStyle::WidgetShape::RoundRect);
        FillShape(knob, *material, shape);
    }

    if (const auto* material = GetWidgetMaterial(id, ps, on_off ? "toggle-knob-border-on" : "toggle-knob-border-off"))
    {
        const auto shape = GetWidgetProperty(id, ps, "toggle-knob-shape", UIStyle::WidgetShape::RoundRect);
        const auto width = GetWidgetProperty(id, ps, "toggle-knob-border-width", 1.0f);
        OutlineShape(knob, *material, shape, width);
    }

    if (const auto* material = GetWidgetMaterial(id, ps, on_off ? "toggle-border-on" : "toggle-border-off"))
    {
        const auto shape = GetWidgetProperty(id, ps, "toggle-shape", UIStyle::WidgetShape::RoundRect);
        const auto width = GetWidgetProperty(id, ps, "toggle-border-width", 1.0f);
        OutlineShape(ps.rect, *material, shape, width);
    }
}

void UIPainter::BeginDrawWidgets()
{
    // see EndDrawWidgets for more details
    for (auto& material : mWidgetMaterials)
    {
        material.used = false;
    }
}

void UIPainter::EndDrawWidgets()
{
    // erase all materials that were created based on material's
    // associated with paint operations. it's possible that those
    // definitions change on the fly when (the game can change them
    // arbitrarily) and this means that any corresponding material
    // instances that are actually no longer used must be dropped.
    base::EraseRemove(mWidgetMaterials,
        [](const auto& material) {
            return  !material.used;
        });
}

bool UIPainter::ParseStyle(const std::string& tag, const std::string& style)
{
    return mStyle->ParseStyleString(tag, style);
}

void UIPainter::DeleteMaterialInstances(const std::string& filter)
{
    for (auto it = mMaterials.begin(); it != mMaterials.end(); )
    {
        const auto& key = it->first;
        if (base::Contains(key, filter))
            it = mMaterials.erase(it);
        else ++it;
    }
}

void UIPainter::DeleteMaterialInstanceByKey(const std::string& key)
{
    auto it = mMaterials.find(key);
    if (it == mMaterials.end())
        return;
    mMaterials.erase(it);
}

void UIPainter::DeleteMaterialInstanceByClassId(const std::string& id)
{
    for (auto it = mMaterials.begin(); it != mMaterials.end();)
    {
        gfx::Material* material = it->second.get();
        if (!material || material->GetClassId() != id)
        {
            ++it;
            continue;
        }
        it = mMaterials.erase(it);
    }
}

void UIPainter::DeleteMaterialInstances()
{
    mMaterials.clear();
}

void UIPainter::Update(double time, float dt)
{
    for (auto& p : mMaterials)
    {
        // could be null to indicate "no have" ;-)
        if (p.second)
            p.second->Update(dt);
    }
}

void UIPainter::SetClip(const gfx::FRect& rect)
{
    // todo:
    // clip can be simple or more difficult to implement. If the
    // painters view matrix doesn't have rotation in non-straight angles
    // the clip rectangle will always be aligned along the render target axis
    // which means the clip can be set using device/HW clip (Painter::SetScissor)

    // however if there's some obscure rotation the clip needs to be done with for
    // example stencil buffer using 2 pass widget rendering.
}

void UIPainter::FillShape(const gfx::FRect& rect, const gfx::Material& material, UIStyle::WidgetShape shape) const
{
    if (shape == UIStyle::WidgetShape::Rectangle)
        gfx::FillShape(*mPainter, rect, gfx::Rectangle(), material);
    else if (shape == UIStyle::WidgetShape::RoundRect)
        gfx::FillShape(*mPainter, rect, gfx::RoundRectangle(), material);
    else if(shape == UIStyle::WidgetShape::Parallelogram)
        gfx::FillShape(*mPainter, rect, gfx::Parallelogram(), material);
    else if (shape == UIStyle::WidgetShape::Circle)
        gfx::FillShape(*mPainter, rect, gfx::Circle(), material);
    else if (shape == UIStyle::WidgetShape::Capsule)
        gfx::FillShape(*mPainter, rect, gfx::Capsule(), material);
    else BUG("Unknown widget shape.");
}

void UIPainter::OutlineShape(const gfx::FRect& rect, const gfx::Material& material, UIStyle::WidgetShape shape, float width) const
{
    if (shape == UIStyle::WidgetShape::Rectangle)
        gfx::DrawShapeOutline(*mPainter, rect, gfx::Rectangle(), material, width);
    else if (shape == UIStyle::WidgetShape::RoundRect)
        gfx::DrawShapeOutline(*mPainter, rect, gfx::RoundRectangle(), material, width);
    else if(shape == UIStyle::WidgetShape::Parallelogram)
        gfx::DrawShapeOutline(*mPainter, rect, gfx::Parallelogram(), material, width);
    else if (shape == UIStyle::WidgetShape::Circle)
        gfx::DrawShapeOutline(*mPainter, rect, gfx::Circle(), material, width);
    else if (shape == UIStyle::WidgetShape::Capsule)
        gfx::DrawShapeOutline(*mPainter, rect, gfx::Capsule(), material, width);
    else BUG("Unknown widget shape.");
}

bool UIPainter::GetMaterial(const std::string& key, gfx::Material** material) const
{
    auto it = mMaterials.find(key);
    if (it != mMaterials.end())
    {
        *material = it->second.get();
        return true;
    }

    auto ret = mStyle->GetMaterial(key);
    if (!ret.has_value())
        return false;

    std::unique_ptr<gfx::Material> instance;
    if (auto klass = ret.value())
    {
        instance = gfx::CreateMaterialInstance(klass);
        *material = instance.get();
    }
    mMaterials[key] = std::move(instance);
    return true;
}

gfx::Material* UIPainter::GetWidgetMaterial(const std::string& id, const PaintStruct& ps, const std::string& key) const
{
    gfx::Material* ret = nullptr;

    // if the paint operation has associated material definitions these take
    // precedence over any other styling information
    if (ps.style_materials)
    {
        std::string prefix;
        if (ps.enabled == false)
            prefix = "disabled/";
        else if (ps.pressed)
            prefix = "pressed/";
        else if (ps.focused)
            prefix = "focused/";
        else if (ps.moused)
            prefix = "mouse-over/";

        // check if we have this particular material key in the set of paint materials
        if (const auto* val = base::SafeFind(*ps.style_materials, prefix + key))
        {
            size_t hash = 0;
            hash = base::hash_combine(hash, *val);
            // look for an existing material instance
            for (auto& material : mWidgetMaterials)
            {
                if (material.hash != hash)
                    continue;
                else if (material.key != prefix + key)
                    continue;
                else if (material.widget != id)
                    continue;

                material.used = true;
                return material.material.get();
            }
            // create a new material instance if the material definition is valid/parses properly
            WidgetMaterial material;
            material.used     = true;
            material.hash     = hash;
            material.widget   = id;
            material.key      = prefix + key;
            if (auto klass = mStyle->MakeMaterial(*val))
            {
                material.material = gfx::CreateMaterialInstance(klass);
                ret = material.material.get();
            }
            mWidgetMaterials.push_back(std::move(material));
            return ret;
        }
    }

    if (ps.enabled == false)
        ret = GetWidgetMaterial(id, ps.klass, "disabled/" + key);
    else if (ps.pressed)
        ret = GetWidgetMaterial(id, ps.klass, "pressed/" + key);
    else if (ps.focused)
        ret = GetWidgetMaterial(id, ps.klass, "focused/" + key);
    else if (ps.moused)
        ret = GetWidgetMaterial(id, ps.klass, "mouse-over/" + key);
    if (!ret)
        ret = GetWidgetMaterial(id, ps.klass, key);
    return ret;
}

gfx::Material* UIPainter::GetWidgetMaterial(const std::string& id,
                                            const std::string& klass,
                                            const std::string& key) const
{
    gfx::Material* material = nullptr;
    if (GetMaterial(id + "/" + key, &material))
        return material;
    else if (GetMaterial("window/" + klass + "/" + key, &material))
        return material;
    else if (GetMaterial(klass + "/" + key, &material))
        return material;
    else if (GetMaterial("window/widget/" + key, &material))
        return material;
    else if (GetMaterial("widget/" + key, &material))
    {
        mFailedProperties.erase("widget/" + key);
        return material;
    }
    auto it = mFailedProperties.find("widget/" + key);
    if (it == mFailedProperties.end())
    {
        WARN("UI material '%1' is not defined." , key);
        mFailedProperties.insert("widget/" + key);
    }
    return nullptr;
}

UIProperty UIPainter::GetWidgetProperty(const std::string& id,
                                        const PaintStruct& ps,
                                        const std::string& key) const
{
    UIProperty prop;

    if (ps.style_properties)
    {
        // if the paint operation has an associated property map with
        // a specific property value then  that takes precedence over
        // any other style properties
        std::string prefix;
        if (ps.enabled == false)
            prefix = "disabled/";
        else if (ps.pressed)
            prefix = "pressed/";
        else if (ps.focused)
            prefix = "focused/";
        else if (ps.moused)
            prefix = "mouse-over/";

        if (const auto* val = base::SafeFind(*ps.style_properties, prefix + key))
        {
            std::visit([&prop](const auto& variant_value) {
                prop.SetValue(variant_value);
            }, *val);
            return prop;
        }
    }
    // check the properties based on some simple rules.
    // if the widget is disabled it cannot be pressed, focused or moused.
    // if the widget is enabled check whether it's
    //  1. pressed  2. focused, 3. moused
    // if none of the above rules are hit then the widget is "normal"
    if (ps.enabled == false)
        prop = GetWidgetProperty(id, ps.klass, "disabled/" + key);
    else if (ps.pressed)
        prop = GetWidgetProperty(id, ps.klass, "pressed/" + key);
    else if (ps.focused)
        prop = GetWidgetProperty(id, ps.klass, "focused/" + key);
    else if (ps.moused)
        prop = GetWidgetProperty(id, ps.klass, "mouse-over/" + key);
    if (!prop)
        prop = GetWidgetProperty(id, ps.klass, key);
    return prop;
}

UIProperty UIPainter::GetWidgetProperty(const std::string& id,
                                        const std::string& klass,
                                        const std::string& key) const
{
    auto prop = mStyle->GetProperty(id + "/" + key);
    if (prop.HasValue())
        return prop;
    prop = mStyle->GetProperty("window/" + klass + "/" + key);
    if (prop.HasValue())
        return prop;
    prop = mStyle->GetProperty(klass + "/" + key);
    if (prop.HasValue())
        return prop;
    prop = mStyle->GetProperty("window/widget/" + key);
    if (prop.HasValue())
        return prop;
    prop = mStyle->GetProperty("widget/" + key);
    if (prop.HasValue())
    {
        mFailedProperties.erase("widget/" + key);
        return prop;
    }
    auto it = mFailedProperties.find("widget/" + key);
    if (it == mFailedProperties.end())
    {
        WARN("UI style property '%1' is not defined.", key);
        mFailedProperties.insert("widget/" + key);
    }
    return UIProperty();
}

void UIKeyMap::Clear()
{
    mKeyMap.clear();
}

bool UIKeyMap::LoadKeys(const nlohmann::json& json)
{
    if (!json.contains("keys"))
        return true;

    bool success = true;
    std::vector<KeyMapping> keys;
    for (const auto& item : json["keys"].items())
    {
        const auto& obj = item.value();

        uik::VirtualKey vk = uik::VirtualKey::None;
        wdk::Keysym sym = wdk::Keysym::None;
        wdk::bitflag<wdk::Keymod> mods;
        std::string mod;
        if (!base::JsonReadSafe(obj, "sym", &sym))
        {
            WARN("Ignoring UI key mapping with unrecognized key symbol.");
            success = false;
            continue;
        }
        if (!base::JsonReadSafe(obj, "vk", &vk))
        {
            WARN("Ignoring UI key mapping with unrecognized virtual key.");
            success = false;
            continue;
        }
        // this is optionally in the JSON
        base::JsonReadSafe(obj, "mods", &mod);
        if (base::Contains(mod, "ctrl"))
            mods.set(wdk::Keymod::Control);
        if (base::Contains(mod, "shift"))
            mods.set(wdk::Keymod::Shift);
        if (base::Contains(mod, "alt"))
            mods.set(wdk::Keymod::Alt);

        //DEBUG("Parsing keymap entry. [sym=%1, mods=%2, vk=%3]", base::ToString(sym), mod, vk);

        KeyMapping key;
        key.mods = mods;
        key.sym  = sym;
        key.vk   = vk;
        keys.push_back(key);
    }
    mKeyMap = std::move(keys);
    return success;
}

bool UIKeyMap::LoadKeys(const EngineData& data)
{
    const auto* beg = (const char*)data.GetData();
    const auto* end = (const char*)data.GetData() + data.GetByteSize();
    const auto& [ok, json, error] = base::JsonParse(beg, end);
    if (!ok)
    {
        ERROR("UI Keymap load failed with JSON parse error. [error='%1', file='%2']", error, data.GetSourceName());
        return false;
    }
    return LoadKeys(json);
}

uik::VirtualKey UIKeyMap::MapKey(wdk::Keysym sym, wdk::bitflag<wdk::Keymod> mods) const
{
    for (const auto& mapping : mKeyMap)
    {
        if ((mapping.sym == sym) && (mapping.mods == mods))
            return mapping.vk;
    }
    return uik::VirtualKey::None;
}


} // namespace

