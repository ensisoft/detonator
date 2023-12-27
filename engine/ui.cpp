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
#include "base/box.h"
#include "base/logging.h"
#include "base/utility.h"
#include "base/json.h"
#include "base/math.h"
#include "base/trace.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/drawing.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "graphics/renderpass.h"
#include "graphics/utility.h"
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

struct PropertyPair {
    std::string key;
    engine::UIProperty::ValueType value;
};
struct MaterialPair {
    std::string key;
    std::unique_ptr<engine::UIMaterial> material;
};

bool ParseProperties(const nlohmann::json& json, std::vector<PropertyPair>& props)
{
    if (!json.contains("properties"))
        return true;

    auto success = true;

    for (const auto& item : json["properties"].items())
    {
        const auto& json = item.value();

        PropertyPair prop;
        if (!base::JsonReadSafe(json, "key", &prop.key))
        {
            WARN("Ignored JSON UI Style property without property key.");
            success = false;
            continue;
        }
        if (!base::JsonReadSafe(json, "value", &prop.value))
        {
            // this is not necessarily a BUG because currently the style json files are
            // hand written. thus we have to be prepared to handle unexpected cases.
            success = false;
            WARN("Ignoring unexpected UI style property. [key='%1']", prop.key);
            continue;
        }
        props.push_back(std::move(prop));
    }
    return success;
}
bool ParseMaterials(const nlohmann::json& json, std::vector<MaterialPair>& materials)
{
    if (!json.contains("materials"))
        return true;

    using namespace engine;

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
            WARN("Failed to parse UI material. [key='%1']", key);
            continue;
        }

        MaterialPair p;
        p.key = std::move(key);
        p.material = std::move(material);
        materials.push_back(std::move(p));
    }
    return success;
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
    return material;
}
bool UIGradient::FromJson(const nlohmann::json& json)
{
    if (!ReadColor(json, "color0", &mColorMap[0]) ||
        !ReadColor(json, "color1", &mColorMap[1]) ||
        !ReadColor(json, "color2", &mColorMap[2]) ||
        !ReadColor(json, "color3", &mColorMap[3]))
        return false;
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

    // if there's no associated image meta file we assume that the image file is a non-packed
    // image file, i.e. not a texture "atlas". alternatively if the name isn't set
    // we can't identify the texture object in the atlas even if using an atlas.
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
        WARN("Packed UITexture texture size is not known (missing image_width or image_height). [uri='%1', name='%2']", mMetafileUri, mTextureName);
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
        WARN("Packed UITexture sub-rectangle description is not found. [uri='%1', name='%2']", mMetafileUri, mTextureName);
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

bool UIStyleFile::LoadStyle(const nlohmann::json& json)
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

bool UIStyleFile::LoadStyle(const EngineData& data)
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

void UIStyleFile::SaveStyle(nlohmann::json& json) const
{
    for (const auto& [key, val] : mProperties)
    {
        nlohmann::json prop;
        base::JsonWrite(prop, "key", key);
        base::JsonWrite(prop, "value", val);
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
    if (it != mMaterials.end())
        return it->second->GetClass(mClassLib, mLoader);

    if (!mStyleFile)
        return std::nullopt;

    it = mStyleFile->mMaterials.find(key);
    if (it != mStyleFile->mMaterials.end())
        return it->second->GetClass(mClassLib, mLoader);

    return std::nullopt;
}

UIProperty UIStyle::GetProperty(const std::string& key) const
{
    auto it = mProperties.find(key);
    if (it != mProperties.end())
        return UIProperty(it->second);

    if (!mStyleFile)
        return UIProperty();

    it = mStyleFile->mProperties.find(key);
    if (it != mStyleFile->mProperties.end())
        return UIProperty(it->second);

    return UIProperty();
}

bool UIStyle::ParseStyleString(const std::string& tag, const std::string& style)
{
    auto [ok, json, error] = base::JsonParse(style);
    if (!ok)
    {
        ERROR("Failed to parse UI style string. [tag='%1', error='%2']", tag, error);
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
{
    if (base::Contains(mProperties, key))
        return true;

    if (mStyleFile && base::Contains(mStyleFile->mProperties, key))
        return true;

    return false;
}
bool UIStyle::HasMaterial(const std::string& key) const
{
    if (base::Contains(mMaterials, key))
        return true;

    if (mStyleFile && base::Contains(mStyleFile->mMaterials, key))
        return true;

    return false;
}

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

    if (!mStyleFile)
        return;

    for (const auto& pair : mStyleFile->mProperties)
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
    if (it != mMaterials.end())
        return (*it).second.get();

    if (!mStyleFile)
        return nullptr;

    it = mStyleFile->mMaterials.find(key);
    if (it != mStyleFile->mMaterials.end())
        return (*it).second.get();

    return nullptr;
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
        base::JsonWrite(prop, "value", val);
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

    if (mStyleFile)
        mStyleFile->SaveStyle(json);
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
        base::JsonWrite(prop, "value", val);
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

    if (mStyleFile)
    {
        for (const auto& p : mStyleFile->mProperties)
        {
            const auto& key = p.first;
            const auto& val = p.second;
            if (!base::Contains(p.first, filter))
                continue;
            nlohmann::json prop;
            base::JsonWrite(prop, "key", key);
            base::JsonWrite(prop, "value", val);
            json["properties"].push_back(std::move(prop));
        }
        for (const auto& p : mStyleFile->mMaterials)
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

    if (!mStyleFile)
        return;

    for (const auto& [key, material] : mStyleFile->mMaterials)
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

    if (!mStyleFile)
        return;

    for (const auto& [key, material] : mStyleFile->mMaterials)
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
    else BUG("Unknown horizontal text alignment.");

    if (va == UIStyle::VerticalTextAlign::Top)
        alignment |= gfx::TextAlign::AlignTop;
    else if (va == UIStyle::VerticalTextAlign::Center)
        alignment |= gfx::TextAlign::AlignVCenter;
    else if (va == UIStyle::VerticalTextAlign::Bottom)
        alignment |= gfx::TextAlign::AlignBottom;
    else BUG("Unknown vertical text alignment.");

    DrawText(text, font_name, font_size, ps.rect, text_color, alignment, properties, line_height);
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
    DrawText(text.text, font_name, font_size, ps.rect, text_color, alignment, properties, 1.0f);
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

void UIPainter::PushMask(const MaskStruct& mask)
{
    if (mask.klass == "form" || mask.klass == "groupbox")
    {
        ClippingMask clip;
        clip.name  = mask.name;
        clip.rect  = mask.rect;
        clip.shape = GetWidgetProperty(mask.id, mask.klass, "shape", UIStyle::WidgetShape::Rectangle);

        // offset the masking area by the thickness of the border
        const auto border_thickness = GetWidgetProperty(mask.id, mask.klass, "border-width", 1.0f);
        clip.rect.Grow(-2.0*border_thickness, -2.0*border_thickness);
        clip.rect.Translate(border_thickness, border_thickness);

        mClippingMaskStack.push_back(std::move(clip));
    }
    else BUG("Unimplemented clipping mask for widget klass.");
}
void UIPainter::PopMask()
{
    ASSERT(!mClippingMaskStack.empty());

    mClippingMaskStack.pop_back();
}

void UIPainter::RealizeMask()
{

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

uint8_t UIPainter::StencilPass() const
{
    // we're using the stencil buffer here to setup a mask that is
    // a combination of all the clipping masks currently in the
    // clipping stack.
    // Each widget pushes their clipping shape onto the stack prior
    // to rendering their child and for each child render the final
    // clipping mask is the combination of all the parents' clipping
    // masks. (basically the intersection)
    // Note that you might think that taking the first parent's mask
    // would be sufficient but that isn't because the parent could be
    // clipped against it's parent etc.

    if (mClippingMaskStack.empty() || !mFlags.test(Flags::ClipWidgets))
        return 0;

    // start with clear 0 stencil. Each mask tests against the stencil
    // value which increments on every write.
    // can't just bitwise and since the stencil bits outside the rasterized
    // shape are *not* modified (obv)

    mPainter->ClearStencil(gfx::StencilClearValue(0));

    gfx::StencilWriteValue stencil_val = 0;

    for (const auto& mask : mClippingMaskStack)
    {
        gfx::StencilMaskPass overlap(gfx::StencilWriteValue(stencil_val), *mPainter,
                                     gfx::StencilMaskPass::StencilFunc::OverlapIncrement);
        DrawShape(mask.rect, gfx::CreateMaterialFromColor(gfx::Color::White), overlap, mask.shape);
        ++stencil_val;
    }
    return stencil_val;
}

void UIPainter::DrawText(const std::string& text, const std::string& font_name, int font_size,
                         const gfx::FRect& rect, const gfx::Color4f & color, unsigned alignment, unsigned properties,
                         float line_height) const
{
    auto raster_width  =  (unsigned)math::clamp(0.0f, 2048.0f, rect.GetWidth());
    auto raster_height =  (unsigned)math::clamp(0.0f, 2048.0f, rect.GetHeight());
    const bool underline = properties & gfx::TextProp::Underline;
    const bool blinking  = properties & gfx::TextProp::Blinking;

    // if the text is set to be blinking do a sharp cut off
    // and when we have the "off" interval then simply don't
    // render the text.
    if (blinking)
    {
        const auto fps = 1.5;
        const auto full_period = 2.0 / fps;
        const auto half_period = full_period * 0.5;
        const auto time = fmodf(base::GetTime(), full_period);
        if (time >= half_period)
            return;
    }

    auto material = gfx::CreateMaterialFromText(text, font_name, color, font_size,
                                                raster_width, raster_height,
                                                alignment, properties, line_height);

    if (const auto value = StencilPass())
    {
        gfx::StencilTestColorWritePass pass(gfx::StencilPassValue(value), *mPainter);
        DrawShape(rect, material, pass, UIStyle::WidgetShape::Rectangle);
    }
    else
    {
        gfx::GenericRenderPass pass(*mPainter);
        DrawShape(rect, material, pass, UIStyle::WidgetShape::Rectangle);
    }

    //gfx::DrawTextRect(*mPainter, text, font_name, font_size, rect, color, alignment, properties, line_height);
}

void UIPainter::FillShape(const gfx::FRect& rect, const gfx::Material& material, UIStyle::WidgetShape shape) const
{
    if (const auto value = StencilPass())
    {
        gfx::StencilTestColorWritePass pass(gfx::StencilPassValue(value), *mPainter);
        DrawShape(rect, material, pass, shape);
    }
    else
    {
        gfx::GenericRenderPass pass(*mPainter);
        DrawShape(rect, material, pass, shape);
    }
}

void UIPainter::OutlineShape(const gfx::FRect& shape_rect, const gfx::Material& material, UIStyle::WidgetShape shape, float thickness) const
{
    const auto width  = shape_rect.GetWidth();
    const auto height = shape_rect.GetHeight();
    const auto x = shape_rect.GetX();
    const auto y = shape_rect.GetY();

    gfx::FRect mask_rect;
    const auto mask_width  = width - 2 * thickness;
    const auto mask_height = height - 2 * thickness;
    mask_rect.Resize(mask_width, mask_height);
    mask_rect.Translate(x + thickness, y + thickness);

    if (const auto stencil_value = StencilPass())
    {
        const gfx::StencilMaskPass mask(gfx::StencilWriteValue(0), *mPainter,
                                     gfx::StencilMaskPass::StencilFunc::Overwrite);
        DrawShape(mask_rect, gfx::CreateMaterialFromColor(gfx::Color::White), mask, shape);

        const gfx::StencilTestColorWritePass cover(stencil_value, *mPainter);
        DrawShape(shape_rect, material, cover, shape);
    }
    else
    {
        const gfx::StencilMaskPass overlap(gfx::StencilClearValue(1),
                                           gfx::StencilWriteValue(0), *mPainter,
                                           gfx::StencilMaskPass::StencilFunc::Overwrite);
        DrawShape(mask_rect, gfx::CreateMaterialFromColor(gfx::Color::White), overlap, shape);

        const gfx::StencilTestColorWritePass cover(gfx::StencilPassValue(1), *mPainter);
        DrawShape(shape_rect, material, cover, shape);
    }
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
        WARN("UI material is not defined. [key='%1']" , key);
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
        WARN("UI style property is not defined. [key='%1']", key);
        mFailedProperties.insert("widget/" + key);
    }
    return UIProperty();
}

template<typename T> inline
T UIPainter::GetWidgetProperty(const std::string& id,
                    const PaintStruct& ps,
                    const std::string& key,
                    const T& value) const
{
    const auto& p = GetWidgetProperty(id, ps, key);
    return p.GetValue(value);
}
template<typename T> inline
T UIPainter::GetWidgetProperty(const std::string& id,
                    const std::string& klass,
                    const std::string& key,
                    const T& value) const
{
    const auto& p = GetWidgetProperty(id, klass, key);
    return p.GetValue(value);
}

template<typename RenderPass>
void UIPainter::DrawShape(const gfx::FRect& rect, const gfx::Material& material, const RenderPass& pass, UIStyle::WidgetShape shape) const
{
    gfx::Transform transform;
    transform.Resize(rect);
    transform.Translate(rect);

    if (shape == UIStyle::WidgetShape::Rectangle)
        pass.Draw(gfx::Rectangle(), transform, material);
    else if (shape == UIStyle::WidgetShape::RoundRect)
        pass.Draw(gfx::RoundRectangle(), transform, material);
    else if (shape == UIStyle::WidgetShape::Circle)
        pass.Draw(gfx::Circle(), transform, material);
    else if (shape == UIStyle::WidgetShape::Capsule)
        pass.Draw(gfx::Capsule(), transform, material);
    else if (shape == UIStyle::WidgetShape::Parallelogram)
        pass.Draw(gfx::Parallelogram(), transform, material);
    else BUG("Missing mask shape case.");
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

UIEngine::UIEngine()
{
    mPainter.SetStyle(&mStyle);
    mPainter.SetFlag(UIPainter::Flags::ClipWidgets, true);
}

void UIEngine::UpdateWindow(double game_time, float dt,
                      std::vector<WidgetAction>* widget_actions)
{
    if (auto* ui = GetUI())
    {
        // Update the UI materials in order to do material animation.
        TRACE_CALL("UIPainter::Update", mPainter.Update(game_time, dt));

        // Update the UI widgets in order to do widget animation.
        TRACE_CALL("Window::Update", ui->Update(mState, game_time, dt, &mAnimationState));

        // Poll widget for pending actions for example
        // something based on previous keyboard/mouse input.
        TRACE_CALL("Window::PollAction", ui->PollAction(mState, game_time, dt, widget_actions));

        // Trigger widget animations based on the actions
        // that we have received. For example $OnClick etc.
        TRACE_CALL("Window::TriggerAnimations", ui->TriggerAnimations(*widget_actions, mState, mAnimationState));
    }
}

void UIEngine::UpdateState(double game_time, float dt,
                           std::vector<WindowAction>* window_actions)
{
    // The UI stack manipulation is done through a queue so that
    // the actions execute in order and the next action will not start
    // until the previous UI stack operation has completed. This is important
    // for example in the following scenario:
    //
    // CloseUI(0)
    // OpenUI('MainMenu')
    //
    // the open cannot run until the previous UI has closed properly which
    // means that any pending $OnClose animation has finished running.
    //
    while (!mUIActionQueue.empty())
    {
        const auto& action = mUIActionQueue.front();
        if (const auto* closing_action = std::get_if<CloseUIAction>(&action))
        {
            auto* ui = GetUI();
            if (ui == nullptr)
            {
                WARN("Request to close a UI but there's no such UI open. [ui='%1']]", closing_action->window_name);
                mUIActionQueue.pop();
                continue;
            }

            if (ui->IsClosed(mState, &mAnimationState))
            {
                DEBUG("Closing UI '%1'", ui->GetName());

                int close_result = 0;
                mState.GetValue("close_result", &close_result);
                mState.DeleteValue("close_result");

                // generate a close event
                {
                    WindowClose close;
                    close.result = close_result;
                    close.window = mStack.top();
                    window_actions->push_back(std::move(close));
                }

                // pop this UI off of the UI stack
                mStack.pop();

                // GetUI could be nullptr at this point.
                PrepareUI(GetUI());

                // generate an update event.
                {
                    WindowUpdate update;
                    update.window = GetUI(); // could be nullptr
                    window_actions->push_back(std::move(update));
                }

                mUIActionQueue.pop();
            }
            else if (!ui->IsClosing(mState))
            {
                mState.SetValue("close_result", closing_action->result);
                ui->Close(mState, &mAnimationState);
                break;

            } else break;
        }
        else if (const auto* open_action = std::get_if<OpenUIAction>(&action))
        {
            mStack.push(open_action->window);

            auto* ui = GetUI();

            PrepareUI(ui);

            // generate open event
            {
                WindowOpen open;
                open.window = ui;
                window_actions->push_back(open);
            }
            // generate update event
            {
                WindowUpdate update;
                update.window = ui;
                window_actions->push_back(update);
            }

            mUIActionQueue.pop();

            DEBUG("Opened new UI '%1'", ui->GetName());
        }
    }
}

void UIEngine::Draw(gfx::Device& device)
{
    auto* ui = GetUI();
    if (!ui)
        return;

    // the viewport retains the UI's aspect ratio and is centered in the middle
    // of the rendering surface.
    const auto& window_rect = ui->GetBoundingRect();
    const float width  = window_rect.GetWidth();
    const float height = window_rect.GetHeight();
    const float scale  = std::min(mSurfaceWidth / width, mSurfaceHeight / height);
    const float device_viewport_width  = width * scale;
    const float device_viewport_height = height * scale;

    gfx::IRect device_viewport;
    device_viewport.Move((mSurfaceWidth - device_viewport_width)*0.5,
                         (mSurfaceHeight - device_viewport_height)*0.5);
    device_viewport.Resize(device_viewport_width, device_viewport_height);

    gfx::Painter painter(&device);
    painter.SetSurfaceSize(mSurfaceWidth, mSurfaceHeight);
    painter.SetPixelRatio(glm::vec2{1.0f, 1.0f});
    painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(0, 0, width, height));
    painter.SetViewport(device_viewport);
    painter.SetEditingMode(mEditingMode);

    mStyle.SetClassLibrary(mClassLib);
    mStyle.SetDataLoader(mLoader);

    mPainter.SetPainter(&painter);
    TRACE_CALL("Window::Paint", ui->Paint(mState, mPainter, base::GetTime(), nullptr));
    mPainter.SetPainter(nullptr);
}

void UIEngine::OpenUI(std::shared_ptr<uik::Window> window)
{
    OpenUIAction action;
    action.window = window;
    mUIActionQueue.push(action);
}

void UIEngine::CloseUI(const std::string& window_name,
                       const std::string& window_id,
                       int result)
{
    if (mUIActionQueue.empty())
    {
        CloseUIAction action;
        action.window_name = window_name;
        action.window_id   = window_id;
        action.result      = result;
        mUIActionQueue.push(action);
        return;
    }
    // filter out repeat input here, so for example if the game reacts to
    // a key press such as Escape to close an UI, it's possible the key
    // press is dispatched multiple times. if each key press callback
    // then does Game:CloseUI(0) the state will end wrong since the close
    // is done multiple times and multiple windows will close unless
    // duplicate closes are filtered out and the intention (most likely)
    // was to close just the top most window.
    //
    // Keep in mind that we don't have a mechanism to expose transient state
    // to the game, i.e. there's no window:IsClosing() type of functionality
    // available in the window object. Arguably this would be useful for
    // the game programmer but that would result in mixing the window's
    // persistent and transient state together which we want to avoid or then
    // find a way to expose the transient window state separately somehow.
    //
    // But ultimately it would seem that this is a problem that can (and should?)
    // be solved for everyone once by simply filtering out superfluous window
    // close requests. This should make life easier for all games for once and
    // for all. Theoretically it could be possible that the game *wants to*
    // open the same window type multiple times, but really I expect this
    // to be an unwanted condition caused by superfluous input handling.
    //
    const auto& back = mUIActionQueue.back();
    if (const auto* last_close = std::get_if<CloseUIAction>(&back))
    {
        // If there's a window ID, and it's a dupe with what was already
        // posted onto the UI stack queue ignore it.
        if (!last_close->window_id.empty() && !window_id.empty())
        {
            if (last_close->window_id == window_id)
            {
                WARN("Ignored duplicate/superfluous UI close. [ui='%1']", window_name);
                return;
            }
        }

        // If there's target window name, and it's a dupe then ignore it.
        if (!last_close->window_name.empty() && !window_name.empty())
        {
            if (last_close->window_name == window_name)
            {
                WARN("Ignored duplicate/superfluous UI close. [ui='%1']", window_name);
                return;
            }
        }
    }

    // window name  is also used as a conditional close,
    // in other words the close takes place only if there is a window
    // with that name open at the right time.
    // this is useful for closing UIs that are conditionally open.
    //
    auto window_stack = mStack;
    auto pending_actions = mUIActionQueue;
    while (!pending_actions.empty())
    {
        const auto& action = pending_actions.front();
        if (const auto* closing_action = std::get_if<CloseUIAction>(&action))
            window_stack.pop();
        else if (const auto* opening_action = std::get_if<OpenUIAction>(&action))
            window_stack.push(opening_action->window);
        else BUG("Missing UI action handling.");
        pending_actions.pop();
    }
    if (window_stack.empty())
        return;
    const auto& future_top_most_window = window_stack.top();
    if (future_top_most_window->GetName() != window_name)
        return;

    CloseUIAction action;
    action.window_name = window_name;
    action.window_id   = window_id;
    action.result      = result;
    mUIActionQueue.push(action);
}

bool UIEngine::HaveOpenUI() const noexcept
{
    auto* ui = GetUI();
    if (ui == nullptr)
        return false;

    // disregard window if it's closing or already closed. this filters
    // out keyboard and mouse event input that would otherwise be dispatched
    // to the window's input handler callbacks
    if (ui->IsClosing(mState) || ui->IsClosing(mState))
        return false;

    return true;
}

void UIEngine::OnKeyDown(const wdk::WindowEventKeyDown &key, std::vector<WidgetAction>* actions)
{
    OnKeyEvent(key, &uik::Window::KeyDown, actions);
}

void UIEngine::OnKeyUp(const wdk::WindowEventKeyUp &key, std::vector<WidgetAction>* actions)
{
    OnKeyEvent(key, &uik::Window::KeyDown, actions);
}

void UIEngine::OnMouseMove(const wdk::WindowEventMouseMove& mouse, std::vector<WidgetAction>* actions)
{
    OnMouseEvent(mouse, &uik::Window::MouseMove, actions);
}
void UIEngine::OnMousePress(const wdk::WindowEventMousePress& mouse, std::vector<WidgetAction>* actions)
{
    OnMouseEvent(mouse, &uik::Window::MousePress, actions);
}
void UIEngine::OnMouseRelease(const wdk::WindowEventMouseRelease& mouse, std::vector<WidgetAction>* actions)
{
    OnMouseEvent(mouse, &uik::Window::MouseRelease, actions);
}

void UIEngine::PrepareUI(uik::Window *ui)
{
    if (!ui)
        return;

    LoadStyle(ui->GetStyleName());
    LoadKeymap(ui->GetKeyMapFile());

    mPainter.DeleteMaterialInstances();
    mState.Clear();
    mAnimationState.clear();

    ui->Style(mPainter);

    // todo: open doesn't execute in consistent state since the open might
    // have executed before (and some widgets were for example animated and their
    // state was changed)
    // but when open is called again the animation state is cleared thus resulting
    // in half-ass animation state.
    // we should keep the state available in the state stack then the problem
    // is solved.
    ui->Open(mState, &mAnimationState);
}

void UIEngine::LoadStyle(const std::string& uri)
{
    // todo: if the style loading somehow fails, then what?
    mStyle.ClearProperties();
    mStyle.ClearMaterials();

    auto data = mLoader->LoadEngineDataUri(uri);
    if (!data)
    {
        ERROR("Failed to load UI style. [uri='%1']", uri);
        return;
    }

    if (!mStyle.LoadStyle(*data))
    {
        ERROR("Failed to parse UI style. [uri='%1']", uri);
        return;
    }
    DEBUG("Loaded UI style file successfully. [uri='%1']", uri);
}
void UIEngine::LoadKeymap(const std::string &uri)
{
    mKeyMap.Clear();

    auto data = mLoader->LoadEngineDataUri(uri);
    if (!data)
    {
        ERROR("Failed to load UI keymap data. [uri='%1']", uri);
        return;
    }
    if (!mKeyMap.LoadKeys(*data))
    {
        ERROR("Failed to parse UI keymap. [uri='%1']", uri);
        return;
    }
    DEBUG("Loaded UI keymap successfully. [uri='%1']", uri);
}

template<typename WdkEvent>
void UIEngine::OnKeyEvent(const WdkEvent& key, UIKeyFunc which, std::vector<WidgetAction>* actions)
{
    if (!HaveOpenUI())
        return;

    auto* ui = GetUI();
    if (!ui->TestFlag(uik::Window::Flags::EnableVirtualKeys))
        return;

    const auto vk = mKeyMap.MapKey(key.symbol, key.modifiers);
    if (vk == uik::VirtualKey::None)
        return;

    if (base::IsDebugLogEnabled())
    {
        std::string mod_string;
        if (key.modifiers.test(wdk::Keymod::Control))
            mod_string += "Ctrl+";
        if (key.modifiers.test(wdk::Keymod::Shift))
            mod_string += "Shift+";
        if (key.modifiers.test(wdk::Keymod::Alt))
            mod_string += "Alt+";
        DEBUG("UI virtual key mapping %1%2 => %3", mod_string, base::ToString(key.symbol), vk);
    }

    uik::Window::KeyEvent event;
    event.key  = vk;
    event.time = base::GetTime();
    *actions = (ui->*which)(event, mState);

    ui->TriggerAnimations(*actions, mState, mAnimationState);
}

template<typename WdkEvent>
void UIEngine::OnMouseEvent(const WdkEvent& mickey, UIMouseFunc which, std::vector<WidgetAction>* actions)
{
    if (!HaveOpenUI())
        return;

    auto* ui = GetUI();

    const auto& rect   = ui->GetBoundingRect();
    const float width  = rect.GetWidth();
    const float height = rect.GetHeight();
    const float scale  = std::min(mSurfaceWidth/width, mSurfaceHeight/height);
    const glm::vec2 window_size(width, height);
    const glm::vec2 surface_size(mSurfaceWidth, mSurfaceHeight);
    const glm::vec2 viewport_size = glm::vec2(width, height) * scale;
    const glm::vec2 viewport_origin = (surface_size - viewport_size) * glm::vec2(0.5, 0.5);
    const glm::vec2 mickey_pos_win(mickey.window_x, mickey.window_y);
    const glm::vec2 mickey_pos_uik = (mickey_pos_win - viewport_origin) / scale;

    uik::Window::MouseEvent event;
    event.time   = base::GetTime();
    event.button = MapMouseButton(mickey.btn);
    event.native_mouse_pos = uik::FPoint(mickey_pos_win.x, mickey_pos_win.y);
    event.window_mouse_pos = uik::FPoint(mickey_pos_uik.x, mickey_pos_uik.y);

    *actions = (ui->*which)(event, mState);

    ui->TriggerAnimations(*actions, mState, mAnimationState);
}

uik::MouseButton UIEngine::MapMouseButton(const wdk::MouseButton btn) const
{
    if (btn == wdk::MouseButton::None)
        return uik::MouseButton::None;
    else if (btn == wdk::MouseButton::Left)
        return uik::MouseButton::Left;
    else if (btn == wdk::MouseButton::Right)
        return uik::MouseButton::Right;
    else if (btn == wdk::MouseButton::Wheel)
        return uik::MouseButton::Wheel;
    else if (btn == wdk::MouseButton::WheelScrollUp)
        return uik::MouseButton::WheelUp;
    else if (btn == wdk::MouseButton::WheelScrollDown)
        return uik::MouseButton::WheelDown;
    WARN("Unmapped wdk mouse button '%1'", base::ToString(btn));
    return uik::MouseButton::None;
}

} // namespace

