// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>

#include "base/assert.h"
#include "base/logging.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/drawing.h"
#include "graphics/painter.h"
#include "engine/ui.h"
#include "engine/classlib.h"

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

namespace game
{
namespace detail {

UIMaterial::MaterialClass UIGradient::GetClass(const ClassLibrary& loader) const
{
    auto material = std::make_shared<gfx::MaterialClass>();
    material->SetType(gfx::MaterialClass::Type::Gradient);
    material->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    material->SetColorMapColor(mColorMap[0], ColorIndex::TopLeft);
    material->SetColorMapColor(mColorMap[1], ColorIndex::TopRight);
    material->SetColorMapColor(mColorMap[2], ColorIndex::BottomLeft);
    material->SetColorMapColor(mColorMap[3], ColorIndex::BottomRight);
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
}

UIMaterial::MaterialClass UIColor::GetClass(const ClassLibrary& loader) const
{
    auto material = std::make_shared<gfx::MaterialClass>();
    material->SetType(gfx::MaterialClass::Type::Color);
    material->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    material->SetBaseColor(mColor);
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

UIMaterial::MaterialClass UIMaterialReference::GetClass(const ClassLibrary& loader) const
{
    auto klass = loader.FindMaterialClassById(mMaterialId);
    if (klass == nullptr)
        WARN("Unresolved UI material ID '%1'", mMaterialId);
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

UIMaterial::MaterialClass UITexture::GetClass(const ClassLibrary&) const
{
    auto material = std::make_shared<gfx::MaterialClass>();
    material->SetType(gfx::MaterialClass::Type::Texture);
    material->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    material->AddTexture(mTextureUri);
    return material;
}

bool UITexture::FromJson(const nlohmann::json& json)
{
    if (!base::JsonReadSafe(json, "texture", &mTextureUri))
        return false;
    return true;
}

void UITexture::IntoJson(nlohmann::json& json) const
{
    base::JsonWrite(json, "texture", mTextureUri);
}

} // detail

std::optional<UIStyle::MaterialClass> UIStyle::GetMaterial(const std::string& key) const
{
    ASSERT(mLoader);
    auto it = mMaterials.find(key);
    if (it == mMaterials.end())
        return std::nullopt;
    return it->second->GetClass(*mLoader);
}

UIProperty UIStyle::GetProperty(const std::string& key) const
{
    auto it = mProperties.find(key);
    if (it == mProperties.end())
        return UIProperty();
    return UIProperty(it->second);
}

bool UIStyle::ParseStyleString(const WidgetId& id, const std::string& style)
{
    auto [ok, json, error] = base::JsonParse(style.begin(), style.end());
    if (!ok)
    {
        ERROR("Failed to parse widget ('%1') style.", id);
        ERROR("JSON parse error: '%1'", error);
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
        if (base::StartsWith(p.key, id))
            mProperties[p.key] = std::move(p.value);
        else mProperties[id + "/" + p.key] = std::move(p.value);
    }
    for (auto& m : materials)
    {
        if (base::StartsWith(m.key, id))
            mMaterials[m.key] = std::move(m.material);
        else mMaterials[id + "/" + m.key] = std::move(m.material);
    }
    return true;
}


bool UIStyle::HasProperty(const std::string& key) const
{ return mProperties.find(key) != mProperties.end(); }

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
        if (base::StartsWith(key, filter))
            it = mProperties.erase(it);
        else ++it;
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
        if (base::StartsWith(key, filter))
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
    return json.dump();
}

bool UIStyle::PurgeUnavailableMaterialReferences()
{
    bool purged = false;
    for (auto it = mMaterials.begin(); it != mMaterials.end();)
    {
        auto* source = it->second.get();
        if (source->IsAvailable(*mLoader))
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

        if (!material)
        {
            // this is not necessarily a BUG because currently the style json files are
            // hand written. thus we have to be prepared to handle unexpected
            // cases. That being said it's possible that this else is hit because
            // the conditionals above have not been updated. douh.
            success = false;
            WARN("Ignoring unexpected material type for key '%1'.", key);
            continue;
        }

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

void UIPainter::DrawWidgetText(const WidgetId& id, const PaintStruct& ps, const std::string& text, float line_height) const
{
    if (text.empty())
        return;

    const auto& text_color = GetWidgetProperty(id, ps, "text-color", uik::Color4f(uik::Color::White));
    const auto& text_blink = GetWidgetProperty(id, ps, "text-blink", false);
    const auto& text_underline = GetWidgetProperty(id, ps, "text-underline",false);
    const auto& font_name = GetWidgetProperty(id, ps, "font-name",std::string(""));
    const auto  font_size = GetWidgetProperty(id, ps, "font-size",16);
    const auto va = GetWidgetProperty(id, ps, "text-vertical-align", UIStyle::VerticalTextAlign::Center);
    const auto ha = GetWidgetProperty(id, ps, "text-horizontal-align", UIStyle::HorizontalTextAlign::Center);

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

void UIPainter::DrawFocusRect(const WidgetId& id, const PaintStruct& ps) const
{
    // todo:
}

void UIPainter::DrawCheckBox(const WidgetId& id, const PaintStruct& ps , bool checked) const
{
    if (const auto* material = GetWidgetMaterial(id, ps, "check-background"))
    {
        const auto shape = GetWidgetProperty(id, ps, "check-background-shape", UIStyle::WidgetShape::Rectangle);
        FillShape(ps.rect, *material, shape);
    }
    if (const auto* material = GetWidgetMaterial(id, ps, "check-border"))
    {
        const auto width = GetWidgetProperty(id, ps, "check-border-width", 1.0f);
        const auto shape = GetWidgetProperty(id, ps, "check-background-shape", UIStyle::WidgetShape::Rectangle);
        OutlineShape(ps.rect, *material, shape, width);
    }
    if (!checked)
        return;

    if (const auto* material = GetWidgetMaterial(id, ps, "check-mark"))
    {
        const auto shape = GetWidgetProperty(id, ps, "check-shape", UIStyle::WidgetShape::RoundRect);
        gfx::FRect mark;
        mark.Move(ps.rect.GetPosition());
        mark.Resize(ps.rect.GetSize());
        mark.Grow(-6.0f, -6.0f);
        mark.Translate(3.0f, 3.0f);
        FillShape(mark, *material, shape);
    }
}

void UIPainter::DrawButton(const WidgetId& id, const PaintStruct& ps, Button btn) const
{
    // todo:
}
void UIPainter::DrawProgressBar(const WidgetId&, const PaintStruct& ps, float percentage) const
{
    // todo:
}

bool UIPainter::ParseStyle(const WidgetId& id , const std::string& style)
{
    if (mStyle->ParseStyleString(id, style))
        return true;
    WARN("Failed to parse widget ('%1') style string.", id);
    return false;
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
        if (!material || (*material)->GetId() != id)
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
    else if (GetMaterial(klass + "/" + key, &material))
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
    prop = mStyle->GetProperty(klass + "/" + key);
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

} // namespace

