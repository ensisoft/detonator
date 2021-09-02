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

#include "warnpush.h"
#  include <nlohmann/json_fwd.hpp>
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <any>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include "base/assert.h"
#include "uikit/painter.h"
#include "graphics/color4f.h"
#include "graphics/material.h"
#include "graphics/fwd.h"

namespace engine
{
    class ClassLibrary;
    class GameData;

    // Interface for abstracting away how UI materials are sourced
    // or created.
    class UIMaterial
    {
    public:
        using MaterialClass = std::shared_ptr<const gfx::MaterialClass>;
        // What's the type of the UI material source.
        enum class Type {
            // Material is generated and it uses a gradient color map.
            Gradient,
            // Material is generated and it uses a single color.
            Color,
            // Material is a reference to another material resource
            // that needs to be loaded through the ClassLibrary.
            Reference,
            // Material uses a texture.
            Texture,
            // There's no material to be used.
            Null
        };
        virtual ~UIMaterial() = default;
        // Load/generate the actual material class.
        virtual MaterialClass GetClass(const ClassLibrary& loader) const = 0;
        // Get the type of the UI material.
        virtual Type GetType() const = 0;
        // Load state from JSON. Returns true if successful.
        virtual bool FromJson(const nlohmann::json& json) = 0;
        // Save the state into the given JSON object.
        virtual void IntoJson(nlohmann::json& json) const = 0;
        // Returns true if the material can still be resolved to a
        // gfx material class. In cases where the material to be used
        // is a material that needs to be loaded through the the classlib
        // it's possible that it's actually no longer available. In such
        // cases the function should return false.
        virtual bool IsAvailable(const ClassLibrary& loader) const
        { return true; }
    private:
    };

    namespace detail {
        // Null material is used to indicate the *intended* absence of a material.
        class UINullMaterial : public UIMaterial
        {
        public:
            virtual MaterialClass GetClass(const ClassLibrary&) const override
            { return nullptr; }
            virtual Type GetType() const override
            { return Type::Null; }
            virtual bool FromJson(const nlohmann::json&) override
            { return true; }
            virtual void IntoJson(nlohmann::json&) const override
            {}
        private:
        };
        // Create material based on a simple texture.
        class UITexture : public UIMaterial
        {
        public:
            virtual MaterialClass GetClass(const ClassLibrary&) const override;
            virtual Type GetType() const override
            { return Type::Texture; }
            virtual bool FromJson(const nlohmann::json&) override;
            virtual void IntoJson(nlohmann::json& json) const override;
            std::string GetTextureUri() const
            { return mTextureUri; }
            void SetTextureUri(const std::string& uri)
            { mTextureUri = uri; }
        private:
            std::string mTextureUri;
        };
        // Create material from a gradient spec.
        class UIGradient : public UIMaterial
        {
        public:
            using ColorIndex = gfx::GradientClass::ColorIndex;

            UIGradient() = default;
            UIGradient(const gfx::Color4f& top_left,
                       const gfx::Color4f& top_right,
                       const gfx::Color4f& bottom_left,
                       const gfx::Color4f& bottom_right)
            {
                SetColor(top_left, ColorIndex::TopLeft);
                SetColor(top_right, ColorIndex::TopRight);
                SetColor(bottom_left, ColorIndex ::BottomLeft);
                SetColor(bottom_right, ColorIndex::BottomRight);
            }

            virtual MaterialClass GetClass(const ClassLibrary& loader) const override;
            virtual Type GetType() const override
            { return Type::Gradient; }
            virtual bool FromJson(const nlohmann::json& json) override;
            virtual void IntoJson(nlohmann::json& json) const override;
            UIGradient& SetColor(const gfx::Color4f& color, ColorIndex index)
            {
                if (index == ColorIndex::TopLeft)
                    mColorMap[0] = color;
                else if (index == ColorIndex::TopRight)
                    mColorMap[1] = color;
                else if (index == ColorIndex::BottomLeft)
                    mColorMap[2] = color;
                else if (index == ColorIndex::BottomRight)
                    mColorMap[3] = color;
                else BUG("incorrect color index");
                return *this;
            }
            gfx::Color4f GetColor(ColorIndex index) const
            {
                if (index == ColorIndex::TopLeft)
                    return mColorMap[0];
                else if (index == ColorIndex::TopRight)
                    return mColorMap[1];
                else if (index == ColorIndex::BottomLeft)
                    return mColorMap[2];
                else if (index == ColorIndex::BottomRight)
                    return mColorMap[3];
                else BUG("incorrect color index");
                return gfx::Color4f();
            }
            gfx::Color4f GetColor(unsigned index) const
            {
                ASSERT(index < 4);
                return mColorMap[index];
            }
            void SetGamma(float gamma)
            { mGamma = gamma; }
            float GetGamma() const
            { return mGamma.value_or(1.0f); }
        private:
            gfx::Color4f mColorMap[4];
            std::optional<float> mGamma;
        };
        // Create material from a color spec.
        class UIColor : public UIMaterial
        {
        public:
            UIColor() = default;
            UIColor(const gfx::Color4f& color) : mColor(color)
            {}
            virtual MaterialClass GetClass(const ClassLibrary& loader) const override;
            virtual Type GetType() const override
            { return Type::Color; }
            virtual bool FromJson(const nlohmann::json& json) override;
            virtual void IntoJson(nlohmann::json& json) const override;
            void SetColor(const gfx::Color4f& color)
            { mColor = color; }
            gfx::Color4f GetColor() const
            { return mColor; }
        private:
            gfx::Color4f mColor;
        };
        // Material is actually a reference to a material loadable through
        // the class loader and needs to be resolved.
        class UIMaterialReference : public UIMaterial
        {
        public:
            UIMaterialReference(std::string id)
              : mMaterialId(std::move(id))
            {}
            UIMaterialReference() = default;
            virtual MaterialClass GetClass(const ClassLibrary& loader) const override;
            virtual bool FromJson(const nlohmann::json& json) override;
            virtual void IntoJson(nlohmann::json& json) const override;
            virtual Type GetType() const override
            { return Type::Reference; }
            virtual bool IsAvailable(const ClassLibrary& loader) const override;

            std::string GetMaterialId() const
            { return mMaterialId; }
            void SetMaterialId(std::string id)
            { mMaterialId = std::move(id); }
        private:
            std::string mMaterialId;
        };

    } // detail

    class UIProperty
    {
    public:
        UIProperty() = default;
        UIProperty(const std::any& value) : mValue(value) {}
        UIProperty(std::any&& value) : mValue(std::move(value)) {}

        // Construct a new property using the primitive value.
        template<typename T>
        UIProperty(T&& value) : mValue(std::forward<T>(value))
        {}

        // Returns true if property holds a value.
        bool HasValue() const
        { return mValue.has_value(); }
        // Returns true if property holds a value.
        operator bool() const
        { return HasValue(); }
        std::any GetAny() const
        { return mValue; }

        // Get a string property value. Returns the given default value
        // if no property was available.
        // Overload to help resolve issue wrt const char* vs std::string property type.
        std::string GetValue(const std::string& value) const
        {
            std::string out;
            if (!mValue.has_value())
                return value;
            else if (!GetRawValue(&out))
                return value;
            return out;
        }
        // Get a string property value. Returns true if value was available
        // and the value is stored into str.
        // Overload to help resolve issue wrt const char* vs std::string property type.
        bool GetValue(std::string* str) const
        {
            if (!mValue.has_value())
                return false;
            return GetRawValue(str);
        }

        // Get property value. Returns the actual property value always,
        // which must always be available and have the matching type.
        // The caller (programmer) is responsible for ensuring that these two
        // invariants are correctly observed.
        template<typename T>
        T GetValue() const
        {
            T out;
            ASSERT(mValue.has_value() && "No property value set.");
            ASSERT(GetValueInternal(&out) && "Property type mismatch.");
            return out;
        }

        // Get property value T. Returns the given default value if no
        // property was available.
        template<typename T>
        T GetValue(const T& value) const
        {
            T out;
            if (!mValue.has_value())
                return value;
            else if (!GetValueInternal(&out))
                return value;
            return out;
        }
        // Get a property value. Returns true if the value was available
        // and stores the actual value into value ptr.
        // Otherwise returns false and value is not changed.
        template<typename T>
        bool GetValue(T* value) const
        {
            if (!mValue.has_value())
                return false;
            return GetValueInternal(value);
        }

        template<typename T>
        void SetValue(const T& value)
        { mValue = value; }
        void SetValue(const std::string& str)
        { mValue = str; }
        void SetValue(const char* str)
        { mValue = std::string(str); }
    private:
        template<typename T>
        bool GetValueInternal(T* out) const
        {
            ASSERT(mValue.has_value());
            // if the raw value has the matching type we're done
            if (GetRawValue(out))
                return true;
            // if the value we want to read is an enum then it's possible
            // that is encoded as a string.
            if constexpr (std::is_enum<T>::value) {
                std::string str;
                if (!GetRawValue(&str))
                    return false;
                const auto& enum_val = magic_enum::enum_cast<T>(str);
                if (!enum_val)
                    return false;
                *out = enum_val.value();
                return true;
            }
            return false;
        }

        bool GetValueInternal(gfx::Color4f* color) const
        {
            ASSERT(mValue.has_value());
            if (GetRawValue(color))
                return true;
            // maybe the color is encoded as a string color name.
            gfx::Color name = gfx::Color::Black;
            if (GetValueInternal(&name)) {
                *color = gfx::Color4f(name);
                return true;
            }
            return false;
        }

        template<typename T>
        bool GetRawValue(T* out) const
        {
            ASSERT(mValue.has_value());
            if (mValue.type() != typeid(T))
                return false;
            *out = std::any_cast<T>(mValue);
            return true;
        }
        bool GetRawValue(std::string* out) const
        {
            ASSERT(mValue.has_value());
            if (mValue.type() == typeid(std::string))
                *out = std::any_cast<std::string>(mValue);
            else if (mValue.type() == typeid(const char*))
                *out = std::any_cast<const char*>(mValue);
            else return false;
            return true;
        }
    private:
        std::any mValue;
    };


    // Style resolves generic and widget specific material references
    // as well as generic and widget specific properties to actual
    // material class objects and property values.
    class UIStyle
    {
    public:
        using WidgetId = uik::Painter::WidgetId;
        using MaterialClass = std::shared_ptr<const gfx::MaterialClass>;

        enum class HorizontalTextAlign {
            Left, Center, Right
        };
        enum class VerticalTextAlign {
            Top, Center, Bottom
        };
        enum class WidgetShape {
            Rectangle,
            RoundRect,
            Parallelogram,
            Circle
        };

        UIStyle(const ClassLibrary* classlib = nullptr)
          : mClassLib(classlib)
        {}

        std::optional<MaterialClass> GetMaterial(const std::string& key) const;
        UIProperty GetProperty(const std::string& key) const;
        bool ParseStyleString(const WidgetId& id, const std::string& style);

        // Set the class library loader. This is needed when a style references a material
        // that needs to be loaded through the class library.
        void SetClassLibrary(const ClassLibrary* classlib)
        { mClassLib = classlib; }
        // Returns if the style contains a property under the given property key.
        bool HasProperty(const std::string& key) const;
        // Delete a style property under the given property key.
        void DeleteProperty(const std::string& key);
        // Delete style properties that match with the given filter.
        void DeleteProperties(const std::string& filter);
        struct PropertyKeyValue {
            std::string key;
            UIProperty prop;
        };
        // Gather properties that match the given filter into a vector.
        void GatherProperties(const std::string& filter, std::vector<PropertyKeyValue>* props) const;
        // Set a new property value under the given property key.
        template<typename T>
        void SetProperty(const std::string& key, const T& value)
        {
            // have to make special case for enum and convert into a string
            // since if the property would be serialized (MakeStyleString)
            // we'd not be able do the conversion into string without
            // knowing the type of the enum.
            if constexpr (std::is_enum<T>::value) {
                mProperties[key] = std::string(magic_enum::enum_name(value));
                return;
            }
            mProperties[key] = value;
        }
        void SetProperty(const std::string& key, const UIProperty& prop)
        { mProperties[key] = prop.GetAny(); }
        // Set a new material setting under the given material key.
        void SetMaterial(const std::string& key, std::unique_ptr<UIMaterial> material)
        { mMaterials[key] = std::move(material); }
        // Set a new material setting under the given material key.
        template<typename T>
        void SetMaterial(const std::string& key, T value)
        { mMaterials[key] = std::make_unique<T>(std::move(value)); }
        // Delete the material setting under the given material key.
        void DeleteMaterial(const std::string& key);
        // Delete the material settings that match the given filter.
        void DeleteMaterials(const std::string& filter);
        // Get the type of the UI material under the given material key.
        const UIMaterial* GetMaterialType(const std::string& key) const;
        // Load style properties and materials based on the JSON.
        bool LoadStyle(const nlohmann::json& json);
        bool LoadStyle(const GameData& data);
        // Collect the properties and materials that match the given filter
        // into a style string. The style string is in a format supported by
        // ParseStyleString.
        std::string MakeStyleString(const std::string& filter) const;

        void ClearProperties()
        { mProperties.clear(); }
        void ClearMaterials()
        { mMaterials.clear(); }

        bool PurgeUnavailableMaterialReferences();
    private:
        struct PropertyPair {
            std::string key;
            std::any    value;
        };
        struct MaterialPair {
            std::string key;
            std::unique_ptr<UIMaterial> material;
        };
        static bool ParseProperties(const nlohmann::json& json, std::vector<PropertyPair>& props);
        static bool ParseMaterials(const nlohmann::json& json, std::vector<MaterialPair>& materials);

    private:
        const ClassLibrary* mClassLib = nullptr;
        std::unordered_map<std::string, std::any> mProperties;
        std::unordered_map<std::string, std::unique_ptr<UIMaterial>> mMaterials;
    };

    // Implementation of ui kit painter that uses the graphics/
    // subsystem to perform widget drawing. The styling and style
    // property management is delegated to the UIStyle class.
    class UIPainter : public uik::Painter
    {
    public:
        UIPainter() = default;
        UIPainter(UIStyle* style, gfx::Painter* painter)
                : mStyle(style)
                , mPainter(painter)
        {}
        // uik::Painter implementation.
        virtual void DrawWidgetBackground(const WidgetId& id, const PaintStruct& ps) const override;
        virtual void DrawWidgetBorder(const WidgetId& id, const PaintStruct& ps) const override;
        virtual void DrawWidgetFocusRect(const WidgetId& id, const PaintStruct& ps) const override;
        virtual void DrawStaticText(const WidgetId& id, const PaintStruct& ps, const std::string& text, float line_height) const override;
        virtual void DrawEditableText(const WidgetId& id, const PaintStruct& ps, const EditableText& text) const override;
        virtual void DrawTextEditBox(const WidgetId& id, const PaintStruct& ps) const override;
        virtual void DrawCheckBox(const WidgetId& id, const PaintStruct& ps, bool checked) const override;
        virtual void DrawButton(const WidgetId& id, const PaintStruct& ps, ButtonIcon btn) const override;
        virtual void DrawSlider(const WidgetId& id, const PaintStruct& ps, const uik::FRect& knob) const override;
        virtual void DrawProgressBar(const WidgetId&, const PaintStruct& ps, float percentage) const override;
        virtual bool ParseStyle(const WidgetId& id, const std::string& style) override;

        // About deleting material instances.
        // This is mostly useful when designing the UI in the editor and changes
        // in the styling properties/materials are taking place.

        // Delete all material instances.
        void DeleteMaterialInstances();
        // Delete a specific material instance identified by the property key.
        // I.e. id + "/" + "property name". Example as1243sad/background
        void DeleteMaterialInstanceByKey(const std::string& key);
        // Delete any material instance(s) that have the given material class
        // as identified by the class id.
        void DeleteMaterialInstanceByClassId(const std::string& id);

        // Update all materials for material animations.
        void Update(double time, float dt);

        // Set the style reference for accessing styling properties.
        void SetStyle(UIStyle* style)
        { mStyle = style; }
        // Set the actual gfx painter object used to perform the
        // painting operations.
        void SetPainter(gfx::Painter* painter)
        { mPainter = painter; }

    private:
        void SetClip(const gfx::FRect& rect);
        void FillShape(const gfx::FRect& rect, const gfx::Material& material, UIStyle::WidgetShape shape) const;
        void OutlineShape(const gfx::FRect& rect, const gfx::Material& material, UIStyle::WidgetShape shape, float width) const;
        bool GetMaterial(const std::string& key, gfx::Material** material) const;
        gfx::Material* GetWidgetMaterial(const std::string& id,
                                         const PaintStruct& ps,
                                         const std::string& key) const;
        gfx::Material* GetWidgetMaterial(const std::string& id,
                                         const std::string& klass,
                                         const std::string& key) const;
        UIProperty GetWidgetProperty(const std::string& id,
                                     const PaintStruct& ps,
                                     const std::string& key) const;
        UIProperty GetWidgetProperty(const std::string& id,
                                     const std::string& klass,
                                     const std::string& key) const;
        template<typename T> inline
        T GetWidgetProperty(const std::string& id,
                            const PaintStruct& ps,
                            const std::string& key,
                            const T& value) const
        {
            const auto& p = GetWidgetProperty(id, ps, key);
            return p.GetValue(value);
        }
    private:
        UIStyle* mStyle = nullptr;
        gfx::Painter* mPainter = nullptr;

        // material instances.
        mutable std::unordered_map<std::string,
                std::unique_ptr<gfx::Material>> mMaterials;
        // failed properties. use this to avoid spamming
        // the log with properties that are failing on every
        // single iteration of paint.
        mutable std::unordered_set<std::string> mFailedProperties;
    };


} // namespace
