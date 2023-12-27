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

#include <variant>
#include <string>
#include <stack>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include "base/assert.h"
#include "base/bitflag.h"
#include "uikit/painter.h"
#include "uikit/state.h"
#include "uikit/window.h"
#include "uikit/animation.h"
#include "graphics/color4f.h"
#include "graphics/material.h"
#include "graphics/fwd.h"
#include "wdk/keys.h"
#include "wdk/bitflag.h"
#include "wdk/events.h"

namespace engine
{
    class ClassLibrary;
    class EngineData;
    class Loader;

    // Interface for abstracting away how UI materials are sourced
    // or created.
    class UIMaterial
    {
    public:
        using MaterialClass = std::shared_ptr<const gfx::MaterialClass>;
        // What's the type of the UI material source.
        enum class Type {
            // Material is generated, and it uses a gradient color map.
            Gradient,
            // Material is generated, and it uses a single color.
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
        virtual MaterialClass GetClass(const ClassLibrary* classlib, const Loader* loader) const = 0;
        // Get the type of the UI material.
        virtual Type GetType() const = 0;
        // Load state from JSON. Returns true if successful.
        virtual bool FromJson(const nlohmann::json& json) = 0;
        // Save the state into the given JSON object.
        virtual void IntoJson(nlohmann::json& json) const = 0;
        // Returns true if the material can still be resolved to a
        // gfx material class. In cases where the material to be used
        // is a material that needs to be loaded through the classlib
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
            virtual MaterialClass GetClass(const ClassLibrary*, const Loader*) const override
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
            UITexture() = default;
            UITexture(std::string uri)
              : mTextureUri(std::move(uri))
            {}
            virtual MaterialClass GetClass(const ClassLibrary*, const Loader* loader) const override;
            virtual Type GetType() const override
            { return Type::Texture; }
            virtual bool FromJson(const nlohmann::json&) override;
            virtual void IntoJson(nlohmann::json& json) const override;
            std::string GetTextureUri() const
            { return mTextureUri; }
            std::string GetMetafileUri() const
            { return mMetafileUri; }
            std::string GetTextureName() const
            { return mTextureName; }
            void SetTextureUri(const std::string& uri)
            { mTextureUri = uri; }
            void SetMetafileUri(const std::string& uri)
            { mMetafileUri = uri; }
            void SetTextureName(const std::string& name)
            { mTextureName = name; }
        private:
            std::string mTextureUri;
            std::string mTextureName;
            std::string mMetafileUri;
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

            virtual MaterialClass GetClass(const ClassLibrary*, const Loader* ) const override;
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
            virtual MaterialClass GetClass(const ClassLibrary*, const Loader*) const override;
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
            virtual MaterialClass GetClass(const ClassLibrary* classlib, const Loader*) const override;
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
        using ValueType = std::variant<gfx::Color4f, std::string,
                int, unsigned, float, double, bool>;

        UIProperty() = default;
        UIProperty(const ValueType& value) : mValue(value) {}
        UIProperty(ValueType&& value) : mValue(std::move(value)) {}

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
        ValueType GetValue() const
        { return mValue.value(); }

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
        // Otherwise, returns false and value is not changed.
        template<typename T>
        bool GetValue(T* value) const
        {
            if (!mValue.has_value())
                return false;
            return GetValueInternal(value);
        }

        template<typename T>
        void SetValue(const T& value)
        {
            if constexpr (std::is_enum<T>::value) {
                mValue = std::string(magic_enum::enum_name(value));
            } else {
                mValue = value;
            }
        }
        void SetValue(const std::string& str)
        { mValue = str; }
        void SetValue(const char* str)
        { mValue = std::string(str); }
    private:
        template<typename T>
        bool GetValueInternal(T* out) const
        {
            ASSERT(mValue.has_value());

            // if the value we want to read is an enum then it's possible
            // that is encoded as a string.
            if constexpr (std::is_enum<T>::value)
            {
                std::string str;
                if (!GetRawValue(&str))
                    return false;
                const auto& enum_val = magic_enum::enum_cast<T>(str);
                if (!enum_val)
                    return false;
                *out = enum_val.value();
                return true;
            }
            else
            {
                // if the raw value has the matching type we're done
                if (GetRawValue(out))
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
            if (!std::holds_alternative<T>(mValue.value()))
                return false;
            *out = std::get<T>(mValue.value());
            return true;
        }
        bool GetRawValue(std::string* out) const
        {
            ASSERT(mValue.has_value());
            if (std::holds_alternative<std::string>(mValue.value()))
                *out = std::get<std::string>(mValue.value());
            else return false;
            return true;
        }
    private:
        std::optional<ValueType> mValue;
    };

    class UIStyleFile
    {
    public:
        bool LoadStyle(const nlohmann::json& json);
        bool LoadStyle(const EngineData& data);
        void SaveStyle(nlohmann::json& json) const;
    private:
        std::unordered_map<std::string, UIProperty::ValueType> mProperties;
        std::unordered_map<std::string, std::unique_ptr<UIMaterial>> mMaterials;
        friend class UIStyle;
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
            Circle,
            Capsule
        };

        UIStyle(const ClassLibrary* classlib = nullptr)
          : mClassLib(classlib)
        {}

        MaterialClass MakeMaterial(const std::string& str) const;
        std::optional<MaterialClass> GetMaterial(const std::string& key) const;
        UIProperty GetProperty(const std::string& key) const;
        bool ParseStyleString(const std::string& tag, const std::string& style);

        // Set the class library loader. This is needed when a style references a material
        // that needs to be loaded through the class library.
        void SetClassLibrary(const ClassLibrary* classlib)
        { mClassLib = classlib; }
        void SetDataLoader(const Loader* loader)
        { mLoader = loader; }
        // Returns whether the style contains a property by the given property key.
        bool HasProperty(const std::string& key) const;
        // Returns whether the style contains a material by the given material name.
        bool HasMaterial(const std::string& key) const;
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
            // we'd not be able to do the conversion to string without
            // knowing the type of the enum.
            if constexpr (std::is_enum<T>::value) {
                mProperties[key] = std::string(magic_enum::enum_name(value));
            } else {
                mProperties[key] = value;
            }
        }
        void SetProperty(const std::string& key, const char* value)
        { mProperties[key] = std::string(value); }
        void SetProperty(const std::string& key, const UIProperty& prop)
        { mProperties[key] = prop.GetValue(); }
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
        bool LoadStyle(const EngineData& data);
        // Save the current style properties into a JSON object.
        void SaveStyle(nlohmann::json& json) const;
        // Collect the properties and materials that match the given filter
        // into a style string. The style string is in a format supported by
        // ParseStyleString.
        std::string MakeStyleString(const std::string& filter) const;

        struct MaterialEntry {
            std::string key;
            UIMaterial* material = nullptr;
        };
        void ListMaterials(std::vector<MaterialEntry>* materials) const;
        void GatherMaterials(const std::string& filter, std::vector<MaterialEntry>* materials) const;

        inline void ClearProperties() noexcept
        { mProperties.clear(); }
        inline void ClearMaterials() noexcept
        { mMaterials.clear(); }

        inline void SetStyleFile(std::shared_ptr<const UIStyleFile> file) noexcept
        { mStyleFile = file; }

        bool PurgeUnavailableMaterialReferences();

    private:
        const ClassLibrary* mClassLib = nullptr;
        const Loader* mLoader = nullptr;
        std::shared_ptr<const UIStyleFile> mStyleFile;
        std::unordered_map<std::string, UIProperty::ValueType> mProperties;
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
        enum class Flags {
            ClipWidgets
        };

        // uik::Painter implementation.
        virtual void BeginDrawWidgets() override;
        virtual void DrawWidgetBackground(const WidgetId& id, const PaintStruct& ps) const override;
        virtual void DrawWidgetBorder(const WidgetId& id, const PaintStruct& ps) const override;
        virtual void DrawWidgetFocusRect(const WidgetId& id, const PaintStruct& ps) const override;
        virtual void DrawStaticText(const WidgetId& id, const PaintStruct& ps, const std::string& text, float line_height) const override;
        virtual void DrawEditableText(const WidgetId& id, const PaintStruct& ps, const EditableText& text) const override;
        virtual void DrawTextEditBox(const WidgetId& id, const PaintStruct& ps) const override;
        virtual void DrawCheckBox(const WidgetId& id, const PaintStruct& ps, bool checked) const override;
        virtual void DrawRadioButton(const WidgetId& id, const PaintStruct& ps, bool selected) const override;
        virtual void DrawButton(const WidgetId& id, const PaintStruct& ps, ButtonIcon btn) const override;
        virtual void DrawSlider(const WidgetId& id, const PaintStruct& ps, const uik::FRect& knob) const override;
        virtual void DrawProgressBar(const WidgetId&, const PaintStruct& ps, std::optional<float> percentage) const override;
        virtual void DrawToggle(const WidgetId& id, const PaintStruct& ps, const uik::FRect& knob, bool on_off) const override;
        virtual void EndDrawWidgets() override;
        virtual bool ParseStyle(const std::string& tag, const std::string& style) override;

        virtual void PushMask(const MaskStruct& mask) override;
        virtual void PopMask() override;
        virtual void RealizeMask() override;

        // About deleting material instances.
        // This is mostly useful when designing the UI in the editor and changes
        // in the styling properties/materials are taking place.

        // Delete all material instances.
        void DeleteMaterialInstances();
        // Delete material instances that match the filter string.
        void DeleteMaterialInstances(const std::string& filter);
        // Delete a specific material instance identified by the property key.
        // I.e. id + "/" + "property name". Example as1243sad/background
        void DeleteMaterialInstanceByKey(const std::string& key);
        // Delete any material instance(s) that have the given material class
        // as identified by the class id.
        void DeleteMaterialInstanceByClassId(const std::string& id);

        // Update all materials for material animations.
        void Update(double time, float dt);

        // Set the style reference for accessing styling properties.
        inline void SetStyle(UIStyle* style) noexcept
        { mStyle = style; }
        // Set the actual gfx painter object used to perform the
        // painting operations.
        inline void SetPainter(gfx::Painter* painter) noexcept
        { mPainter = painter; }

        inline gfx::Painter* GetPainter() noexcept
        { return mPainter; }
        inline const gfx::Painter* GetPainter() const noexcept
        { return mPainter; }
        inline bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        inline void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }
    private:
        uint8_t StencilPass() const;
        void DrawText(const std::string& text, const std::string& font_name, int font_size,
                      const gfx::FRect& rect, const gfx::Color4f & color, unsigned alignment, unsigned properties,
                      float line_height) const;
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
                            const T& value) const;

        template<typename T> inline
        T GetWidgetProperty(const std::string& id,
                            const std::string& klass,
                            const std::string& key,
                            const T& value) const;

        template<typename RenderPass>
        void DrawShape(const gfx::FRect& rect, const gfx::Material& material, const RenderPass& pass, UIStyle::WidgetShape shape) const;

    private:
        UIStyle* mStyle = nullptr;
        mutable gfx::Painter* mPainter = nullptr;

        struct ClippingMask {
            std::string name;
            gfx::FRect rect;
            UIStyle::WidgetShape shape = UIStyle::WidgetShape::Rectangle;
        };
        std::vector<ClippingMask> mClippingMaskStack;

        base::bitflag<Flags> mFlags;

        // "static" material instances based on a combination of properties
        // from a) style (JSON) file, b) style strings that are loaded through
        // calls to ParseStyle.
        mutable std::unordered_map<std::string, std::unique_ptr<gfx::Material>> mMaterials;
        // "dynamic" materials that are only per specific widget and take
        // precedence over the base materials (see above).
        // Created and deleted on the fly based on the material information
        // that comes through the PaintStruct when painting.
        struct WidgetMaterial {
            std::size_t hash = 0;
            std::string widget;
            std::string key;
            std::unique_ptr<gfx::Material> material;
            bool used = false;
        };
        mutable std::vector<WidgetMaterial> mWidgetMaterials;

        // failed properties. use this to avoid spamming
        // the log with properties that are failing on every
        // single iteration of paint.
        mutable std::unordered_set<std::string> mFailedProperties;
    };


    class UIKeyMap
    {
    public:
        void Clear();

        bool LoadKeys(const nlohmann::json& json);
        bool LoadKeys(const EngineData& data);

        uik::VirtualKey MapKey(wdk::Keysym sym, wdk::bitflag<wdk::Keymod> mods) const;
    private:
        struct KeyMapping {
            wdk::Keysym sym = wdk::Keysym::None;
            wdk::bitflag<wdk::Keymod> mods;
            uik::VirtualKey vk = uik::VirtualKey::None;
        };
        std::vector<KeyMapping> mKeyMap;
    };

    // UI Subsystem (engine) for UI state including the window
    // stack, style and keymap information. Functionality for
    // dispatching mouse/keyboard events and painting the UI.
    class UIEngine
    {
    public:
        UIEngine();

        inline void SetEditingMode(bool on_off) noexcept
        { mEditingMode = on_off; }
        inline void SetClassLibrary(const ClassLibrary* classlib) noexcept
        { mClassLib = classlib; }
        inline void SetLoader(const Loader* loader) noexcept
        { mLoader = loader; }
        inline void SetSurfaceSize(float width, float height) noexcept
        {
            mSurfaceWidth = width;
            mSurfaceHeight = height;
        }
        inline uik::Window* GetUI() noexcept
        {
            if (mStack.empty())
                return nullptr;
            return mStack.top().get();
        }
        inline const uik::Window* GetUI() const noexcept
        {
            if (mStack.empty())
                return nullptr;
            return mStack.top().get();
        }
        bool HaveOpenUI() const noexcept;

        struct WindowOpen {
            // This is the reference to the new window that has been opened.
            // Todo: only provide this event when the window has
            // finished running it's $OnOpen animations ?
            uik::Window* window = nullptr;
        };
        struct WindowClose {
            // this could be the last reference to the window.
            // it's provided so that the game can perform actions
            // on window close and still be able to access any data
            // in the UI for any subsequent action.
            std::shared_ptr<uik::Window> window;
            // This is the result code that was passed in CloseUI call
            // and will be passed back to the game runtime ui close handlers.
            int result = 0;
        };
        struct WindowUpdate {
            // Currently, active window in the window stack.
            // could be nullptr if there is no active window.
            uik::Window* window = nullptr;
        };

        using WidgetAction = uik::Window::WidgetAction;
        using WindowAction = std::variant<WindowOpen, WindowClose, WindowUpdate>;

        void UpdateWindow(double game_time, float dt,
                          std::vector<WidgetAction>* widget_actions);
        void UpdateState(double game_time, float dt,
                         std::vector<WindowAction>* window_actions);

        void Draw(gfx::Device& device);

        void OpenUI(std::shared_ptr<uik::Window> window);
        void CloseUI(const std::string& window_name,
                     const std::string& window_id,
                     int result);

        void OnKeyDown(const wdk::WindowEventKeyDown& key, std::vector<WidgetAction>* actions);
        void OnKeyUp(const wdk::WindowEventKeyUp& key, std::vector<WidgetAction>* actions);
        void OnMouseMove(const wdk::WindowEventMouseMove& mouse, std::vector<WidgetAction>* actions);
        void OnMousePress(const wdk::WindowEventMousePress& mouse, std::vector<WidgetAction>* actions);
        void OnMouseRelease(const wdk::WindowEventMouseRelease& mouse, std::vector<WidgetAction>* actions);

    private:
        void PrepareUI(uik::Window* ui);
        void LoadStyle(const std::string& uri);
        void LoadKeymap(const std::string& uri);

        using UIKeyFunc = std::vector<uik::Window::WidgetAction> (uik::Window::*)(const uik::Window::KeyEvent&, uik::TransientState&);
        template<typename WdkEvent>
        void OnKeyEvent(const WdkEvent& key, UIKeyFunc which, std::vector<WidgetAction>* actions);

        using UIMouseFunc = std::vector<uik::Window::WidgetAction> (uik::Window::*)(const uik::Window::MouseEvent&, uik::TransientState&);
        template<typename WdkEvent>
        void OnMouseEvent(const WdkEvent& mouse, UIMouseFunc which, std::vector<WidgetAction>* actions);

        uik::MouseButton MapMouseButton(const wdk::MouseButton btn) const;
    private:
        const ClassLibrary* mClassLib = nullptr;
        const Loader* mLoader = nullptr;

        struct OpenUIAction {
            std::shared_ptr<uik::Window> window;
        };
        struct CloseUIAction {
            int result = 0;
            std::string window_id;
            std::string window_name;
        };
        using UIAction = std::variant<OpenUIAction, CloseUIAction>;
        using UIActionQueue = std::queue<UIAction>;

        bool mEditingMode = false;
        float mSurfaceWidth  = 0.0f;
        float mSurfaceHeight = 0.0f;

        UIActionQueue mUIActionQueue;
        UIPainter mPainter;
        UIStyle mStyle;
        UIKeyMap mKeyMap;
        std::stack<std::shared_ptr<uik::Window>> mStack;
        uik::TransientState mState;
        uik::AnimationStateArray mAnimationState;
    };

} // namespace
