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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QString>
#  include <QVariant>
#  include <QJsonObject>
#  include <QIcon>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <memory>
#include <string>

#include "base/assert.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "gamelib/animation.h"
#include "gamelib/entity.h"
#include "editor/app/utility.h"

namespace app
{
    // Editor app resource object. These are objects that the user
    // manipulates and manages through the Editor application's UI.
    // Each resource contains the actual underlying resource object
    // that is for example a gfx::Material or game::Animation.
    // Normally these types are not related in any hierarchy yet in
    // the editor we want to manage/view/list/edit/delete generic
    // "resources" that the user has created/imported. This interface
    // creates that base root resource hierarchy that is only available
    // in the editor application.
    // Additionally it's possible to associate some arbitrary properties
    // with each resource object to support Editor functionality.
    class Resource
    {
    public:
        // Type of the resource.
        enum class Type {
            // It's a material
            Material,
            // It's a particle system
            ParticleSystem,
            // It's an animation
            Animation,
            // It's a custom shape (drawable)
            CustomShape,
            // It's an entity description
            Entity,
            // It's an audio track
            AudioTrack,
            // It's a generic drawable.
            Drawable
        };
        virtual ~Resource() = default;
        // Get the identifier of the class object type.
        virtual QString GetId() const = 0;
        // Get the human readable name of the resource.
        virtual QString GetName() const = 0;
        // Get the type of the resource.
        virtual Type GetType() const = 0;
        // Update the content's of this resource based on
        // the other resource where the other resource *must*
        // have the same runtime type.
        // The updated properties include the underlying content
        // the  and the properties.
        virtual void UpdateFrom(const Resource& other) = 0;
        // Set the name of the resource.
        virtual void SetName(const QString& name) = 0;
        // Mark the resource primitive or not.
        virtual void SetIsPrimitive(bool primitive) = 0;
        // Serialize the content into JSON
        virtual void Serialize(nlohmann::json& json) const =0;
        // Save additional non-content properties into JSON
        virtual void SaveProperties(QJsonObject& json) const = 0;
        // Save additional user specific properties into JSON.
        virtual void SaveUserProperties(QJsonObject& json) const = 0;
        // Returns true if the resource is considered primitive, i.e. not
        // user defined.
        virtual bool IsPrimitive() const = 0;
        // Returns true if resource has a property by the given name.
        virtual bool HasProperty(const QString& name) const = 0;
        // returns true if the resource as a user property by the given name.
        virtual bool HasUserProperty(const QString& name) const = 0;
        // Set a property value. If the property exists already the previous
        // value is overwritten. Otherwise it's added.
        virtual void SetProperty(const QString& name, const QVariant& value) = 0;
        // Set a property value. If the property exists already the previous
        // value is overwritten. Otherwise it's added.
        virtual void SetUserProperty(const QString& name, const QVariant& value) = 0;
        // Return the value of the property identified by name.
        // If the property doesn't exist returns a null variant.
        virtual QVariant GetVariantProperty(const QString& name) const = 0;
        // Return the value of the property identified by name.
        // If the property doesn't exist returns a null variant.
        virtual QVariant GetUserVariantProperty(const QString& name) const = 0;
        // Load the additional properties from the json object.
        virtual void LoadProperties(const QJsonObject& json) = 0;
        // Load the additional properties from the json object.
        virtual void LoadUserProperties(const QJsonObject& json) = 0;
        // Make an an exact copy of this resource. This means
        // that the copied resource contains all the same properties
        // as this object including the resource id.
        virtual std::unique_ptr<Resource> Copy() const = 0;
        // Make a duplicate clone of the this resource. This means
        // that the duplicated resource contains all the same properties
        // as this object but is a distinct resource object (type) and
        // has a different/unique resource id.
        virtual std::unique_ptr<Resource> Clone() const = 0;

        // helper to map the type to an icon in the application.
        QIcon GetIcon() const
        {
            switch (GetType()) {
                case Resource::Type::Material:
                    return QIcon("icons:material.png");
                case Resource::Type::ParticleSystem:
                    return QIcon("icons:particle.png");
                case Resource::Type::Animation:
                    return QIcon("icons:animation.png");
                case Resource::Type::CustomShape:
                    return QIcon("icons:polygon.png");
                case Resource::Type::Entity:
                    return QIcon("icons:entity.png");
                default: break;
            }
            return QIcon();
        }

        // helpers
        inline bool IsMaterial() const
        { return GetType() == Type::Material; }
        inline bool IsParticleEngine() const
        { return GetType() == Type::ParticleSystem; }
        inline bool IsCustomShape() const
        { return GetType() == Type::CustomShape; }
        inline bool IsEntity() const
        { return GetType() == Type::Entity; }

        template<typename T>
        T GetProperty(const QString& name, const T& def) const
        {
            if (!HasProperty(name))
                return def;
            const auto& ret = GetVariantProperty(name);
            return qvariant_cast<T>(ret);
        }
        template<typename T>
        bool GetProperty(const QString& name, T* out) const
        {
            if (!HasProperty(name))
                return false;
            const auto& ret = GetVariantProperty(name);
            *out = qvariant_cast<T>(ret);
            return true;
        }

        template<typename T>
        T GetUserProperty(const QString& name, const T& def) const
        {
            if (!HasUserProperty(name))
                return def;
            const auto& ret = GetUserVariantProperty(name);
            return qvariant_cast<T>(ret);
        }

        template<typename T>
        bool GetUserProperty(const QString& name, T* out) const
        {
            if (!HasUserProperty(name))
                return false;
            const auto& ret = GetUserVariantProperty(name);
            *out = qvariant_cast<T>(ret);
            return true;
        }

        template<typename Content>
        bool GetContent(Content** content)
        {
            *content = static_cast<Content*>(GetIf(typeid(Content)));
            return *content != nullptr;
        }
        template<typename Content>
        bool GetContent(const Content** content) const
        {
            *content = static_cast<const Content*>(GetIf(typeid(Content)));
            return *content != nullptr;
        }
        template<typename Content>
        Content* GetContent()
        {
            Content* ptr = nullptr;
            if (GetContent(&ptr))
                return ptr;
            return nullptr;
        }
        template<typename Content>
        const Content* GetContent() const
        {
            const Content* ptr = nullptr;
            if (GetContent(&ptr))
                return ptr;
            return nullptr;
        }

        std::string GetNameUtf8() const
        { return app::ToUtf8(GetName()); }
        std::string GetIdUtf8() const
        { return app::ToUtf8(GetId()); }

    protected:
        virtual void* GetIf(const std::type_info& expected_type) = 0;
        virtual const void* GetIf(const std::type_info& expected_type) const = 0;
    private:

    };

    namespace detail {
        // use template specialization to map a gfx type
        // to a Resource::Type enum value.
        template<typename T>
        struct ResourceTypeTraits;

        template<>
        struct ResourceTypeTraits<gfx::KinematicsParticleEngineClass> {
            static constexpr auto Type = app::Resource::Type::ParticleSystem;
        };
        template<>
        struct ResourceTypeTraits<gfx::MaterialClass> {
            static constexpr auto Type = app::Resource::Type::Material;
        };

        template<>
        struct ResourceTypeTraits<game::AnimationClass> {
            static constexpr auto Type = app::Resource::Type::Animation;
        };
        template<>
        struct ResourceTypeTraits<game::EntityClass> {
            static constexpr auto Type = app::Resource::Type::Entity;
        };

        template<>
        struct ResourceTypeTraits<gfx::PolygonClass> {
            static constexpr auto Type = app::Resource::Type::CustomShape;
        };
        template <>
        struct ResourceTypeTraits<gfx::DrawableClass> {
            static constexpr auto Type = app::Resource::Type::Drawable;
        };
    } // detail

    template<typename BaseTypeContent>
    class GameResourceBase : public Resource
    {
    public:
        virtual std::shared_ptr<const BaseTypeContent> GetSharedResource() const = 0;
    private:
    };

    template<typename BaseType, typename Content = BaseType>
    class GameResource : public GameResourceBase<BaseType>
    {
    public:
        static constexpr auto TypeValue = detail::ResourceTypeTraits<BaseType>::Type;

        GameResource(const Content& content, const QString& name)
        {
            mContent = std::make_shared<Content>(content);
            mName = name;
        }
        GameResource(Content&& content, const QString& name)
        {
            mContent = std::make_shared<Content>(std::move(content));
            mName    = name;
        }
        template<typename T>
        GameResource(std::unique_ptr<T> content, const QString& name)
        {
            std::shared_ptr<T> shared(std::move(content));
            mContent = std::static_pointer_cast<Content>(shared);
            mName    = name;
        }
        GameResource(const QString& name)
        {
            mContent = std::make_shared<Content>();
            mName = name;
        }
        GameResource(const GameResource& other)
        {
            mContent   = std::make_shared<Content>(*other.mContent);
            mProps     = other.mProps;
            mName      = other.mName;
            mUserProps = other.mUserProps;
            mPrimitive = other.mPrimitive;
        }

        virtual QString GetId() const override
        { return app::FromUtf8(mContent->GetId());  }
        virtual QString GetName() const override
        { return mName; }
        virtual Resource::Type GetType() const override
        { return TypeValue; }
        virtual void SetName(const QString& name) override
        { mName = name; }
        virtual void UpdateFrom(const Resource& other) override
        {
            const auto* ptr = dynamic_cast<const GameResource*>(&other);
            ASSERT(ptr != nullptr);
            mName  = ptr->mName;
            mProps = ptr->mProps;
            mUserProps = ptr->mUserProps;
            *mContent  = *ptr->mContent;
        }
        virtual void SetIsPrimitive(bool primitive) override
        { mPrimitive = primitive; }
        virtual bool IsPrimitive() const override
        { return mPrimitive; }
        virtual void Serialize(nlohmann::json& json) const override
        {
            nlohmann::json content_json = mContent->ToJson();
            // tag some additional data with the content's JSON
            ASSERT(content_json.contains("resource_name") == false);
            ASSERT(content_json.contains("resource_id") == false);
            content_json["resource_name"] = app::ToUtf8(mName);
            content_json["resource_id"]   = app::ToUtf8(GetId());

            if constexpr (TypeValue == Resource::Type::Material)
                json["materials"].push_back(content_json);
            else if (TypeValue == Resource::Type::ParticleSystem)
                json["particles"].push_back(content_json);
            else if (TypeValue == Resource::Type::Animation)
                json["animations"].push_back(content_json);
            else if (TypeValue == Resource::Type::CustomShape)
                json["shapes"].push_back(content_json);
            else if (TypeValue == Resource::Type::Entity)
                json["entities"].push_back(content_json);
        }
        virtual void SaveProperties(QJsonObject& json) const override
        {
            json[GetId()] = QJsonObject::fromVariantMap(mProps);
        }
        virtual void SaveUserProperties(QJsonObject& json) const override
        {
            json[GetId()] = QJsonObject::fromVariantMap(mUserProps);
        }
        virtual bool HasProperty(const QString& name) const override
        { return mProps.contains(name); }
        virtual bool HasUserProperty(const QString& name) const override
        { return mUserProps.contains(name); }

        virtual void SetProperty(const QString& name, const QVariant& value) override
        { mProps[name] = value; }
        virtual void SetUserProperty(const QString& name, const QVariant& value) override
        { mUserProps[name] = value; }
        virtual QVariant GetVariantProperty(const QString& name) const override
        { return mProps[name]; }
        virtual QVariant GetUserVariantProperty(const QString& name) const override
        { return mUserProps[name]; }
        virtual void LoadProperties(const QJsonObject& object) override
        {
            mProps = object[GetId()].toObject().toVariantMap();
        }
        virtual void LoadUserProperties(const QJsonObject& object) override
        {
            mUserProps = object[GetId()].toObject().toVariantMap();
        }
        virtual std::unique_ptr<Resource> Copy() const override
        { return std::make_unique<GameResource>(*this); }
        virtual std::unique_ptr<Resource> Clone() const override
        {
            auto ret = std::make_unique<GameResource>(mContent->Clone(), mName);
            ret->mProps = mProps;
            ret->mUserProps = mUserProps;
            ret->mPrimitive = mPrimitive;
            return ret;
        }

        // GameResourceBase
        virtual std::shared_ptr<const BaseType> GetSharedResource() const override
        { return mContent; }

        Content* GetContent()
        { return mContent.get(); }
        const Content* GetContent() const
        { return mContent.get(); }
        const QVariantMap& GetProperties() const
        { return mProps;}
        const QVariantMap& GetUserProperties() const
        { return mUserProps; }
        void ClearProperties()
        { mProps.clear(); }
        void ClearUserProperties()
        { mUserProps.clear(); }

        using Resource::GetContent;

        GameResource& operator=(const GameResource& other) = delete;

    protected:
        virtual void* GetIf(const std::type_info& expected_type) override
        {
            if (typeid(Content) == expected_type)
                return (void*)mContent.get();
            return nullptr;
        }
        virtual const void* GetIf(const std::type_info& expected_type) const override
        {
            if (typeid(Content) == expected_type)
                return (const void*)mContent.get();
            return nullptr;
        }
    private:
        std::shared_ptr<Content> mContent;
        QString mName;
        QVariantMap mProps;
        QVariantMap mUserProps;
        bool mPrimitive = false;
    };

    template<typename T> inline
    const GameResourceBase<T>& ResourceCast(const Resource& res)
    {
        using ResourceType = GameResourceBase<T>;
        const auto* ptr = dynamic_cast<const ResourceType*>(&res);
        ASSERT(ptr);
        return *ptr;
    }
    template<typename T> inline
    GameResourceBase<T>& ResourceCast(Resource& res)
    {
        using ResourceType = GameResourceBase<T>;
        auto* ptr = dynamic_cast<ResourceType*>(&res);
        ASSERT(ptr);
        return *ptr;
    }

    using MaterialResource       = GameResource<gfx::MaterialClass>;
    using ParticleSystemResource = GameResource<gfx::KinematicsParticleEngineClass>;
    using CustomShapeResource    = GameResource<gfx::PolygonClass>;
    using AnimationResource      = GameResource<game::AnimationClass>;
    using EntityResource         = GameResource<game::EntityClass>;

    template<typename DerivedType>
    using DrawableResource = GameResource<gfx::DrawableClass, DerivedType>;

} // namespace
