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
#include "utility.h"

namespace app
{
    // Editor app resource object. These are objects that the user
    // manipulates and manages through the Editor application's UI.
    // Each resource contains the actual underlying resource object
    // that is for example a gfx::Material or scene::Animation.
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
            // It's a scene description
            Scene,
            // It's an audio track
            AudioTrack
        };
        virtual ~Resource() = default;
        // Get the human readable name of the resource.
        virtual QString GetName() const = 0;
        // Get the type of the resource.
        virtual Type GetType() const = 0;
        // Serialize the content into JSON
        virtual void Serialize(nlohmann::json& json) const =0;
        // Serialize additional non-content properties into JSON
        virtual void Serialize(QJsonObject& json) const = 0;
        // Returns true if resource has a property by the given name.
        virtual bool HasProperty(const QString& name) const = 0;
        // Set a property value. If the property exists already the previous
        // value is overwritten. Otherwise it's added.
        virtual void SetProperty(const QString& name, const QVariant& value) = 0;
        // Return the value of the property identied by name.
        // If the property doesn't exist returns default value.
        virtual QVariant GetProperty(const QString& name, const QVariant& def) const = 0;
        // Return the value of the property identified by name.
        // If the property doesn't exist returns a null variant.
        virtual QVariant GetProperty(const QString& name) const = 0;
        // Load the additional properies from the json object.
        virtual void LoadProperties(const QJsonObject& json) = 0;
        // Make a duplicate of the current resource.
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

        template<typename T>
        T GetProperty(const QString& name, const T& def) const
        {
            if (!HasProperty(name))
                return def;
            const auto& ret = GetProperty(name);
            return qvariant_cast<T>(ret);
        }
        template<typename T>
        bool GetProperty(const QString& name, T* out) const
        {
            if (!HasProperty(name))
                return false;
            const auto& ret = GetProperty(name);
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
        struct ResourceTypeTraits<gfx::PolygonClass> {
            static constexpr auto Type = app::Resource::Type::CustomShape;
        };
    } // detail


    template<typename Content>
    class GameResource : public Resource
    {
    public:
        using GfxType = Content;
        static constexpr auto TypeValue = detail::ResourceTypeTraits<Content>::Type;

        GameResource(const Content& content, const QString& name)
            : mName(name)
        {
            mContent = std::make_shared<Content>(content);
        }
        GameResource(Content&& content, const QString& name)
            : mName(name)
        {
            mContent = std::make_shared<Content>(std::move(content));
        }
        GameResource(const GameResource& other)
        {
            mContent = std::make_shared<Content>(*other.mContent);
            mProps   = other.mProps;
            mName = other.mName;
        }
        virtual QString GetName() const override
        {
           return mName;
        }
        virtual Type GetType() const override
        {
            return TypeValue;
        }
        virtual void Serialize(nlohmann::json& json) const override
        {
            nlohmann::json content_json = mContent->ToJson();
            // tag some additional data with the content's JSON
            // in this case just the name so that we can map the object
            // to its additional properties.
            // note that we could basically put all the properties here as well
            // but then we'd have to serialize everything in QVariant manually.
            // and there could be possibilities for name conflicts
            ASSERT(content_json.contains("resource_name") == false);
            content_json["resource_name"] = app::ToUtf8(mName);

            if constexpr (TypeValue == Resource::Type::Material)
                json["materials"].push_back(content_json);
            else if (TypeValue == Resource::Type::ParticleSystem)
                json["particles"].push_back(content_json);
            else if (TypeValue == Resource::Type::Animation)
                json["animations"].push_back(content_json);
            else if (TypeValue == Resource::Type::CustomShape)
                json["shapes"].push_back(content_json);
        }
        virtual void Serialize(QJsonObject& json) const override
        {
            if constexpr (TypeValue == Resource::Type::Material)
                json["material_" + mName]  = QJsonObject::fromVariantMap(mProps);
            else if (TypeValue == Resource::Type::ParticleSystem)
                json["particle_" + mName] = QJsonObject::fromVariantMap(mProps);
            else if (TypeValue == Resource::Type::Animation)
                json["animation_" + mName] = QJsonObject::fromVariantMap(mProps);
            else if (TypeValue == Resource::Type::CustomShape)
                json["shape_" + mName] = QJsonObject::fromVariantMap(mProps);
        }
        virtual bool HasProperty(const QString& name) const override
        { return mProps.contains(name); }
        virtual void SetProperty(const QString& name, const QVariant& value) override
        { mProps[name] = value; }
        virtual QVariant GetProperty(const QString& name, const QVariant& def) const override
        {
            QVariant ret = mProps[name];
            if (ret.isNull())
                return def;
            return ret;
        }
        virtual QVariant GetProperty(const QString& name) const override
        { return mProps[name]; }
        virtual void LoadProperties(const QJsonObject& object) override
        {
            if constexpr (TypeValue == Resource::Type::Material)
                mProps = object["material_" + mName].toObject().toVariantMap();
            else if (TypeValue == Resource::Type::ParticleSystem)
                mProps = object["particle_" + mName].toObject().toVariantMap();
            else if (TypeValue == Resource::Type::Animation)
                mProps = object["animation_" + mName].toObject().toVariantMap();
            else if (TypeValue == Resource::Type::CustomShape)
                mProps = object["shape_" + mName].toObject().toVariantMap();
        }
        virtual std::unique_ptr<Resource> Clone() const override
        { return std::make_unique<GameResource>(*this); }

        void UpdateContent(const Content& data)
        { *mContent = data; }
        void UpdateProperties(const QVariantMap& props)
        { mProps = props;}

        Content* GetContent()
        { return mContent.get(); }
        const Content* GetContent() const
        { return mContent.get(); }
        const QVariantMap& GetProperies() const
        { return mProps;}

        std::shared_ptr<const Content> GetSharedResource() const
        { return mContent; }

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
        //Content mContent;
        QString mName;
        QVariantMap mProps;
    };

    template<typename T> inline
    const GameResource<T>& ResourceCast(const Resource& res)
    {
        using ResourceType = GameResource<T>;
        const auto* ptr = dynamic_cast<const ResourceType*>(&res);
        ASSERT(ptr);
        return *ptr;
    }
    template<typename T> inline
    GameResource<T>& ResourceCast(Resource& res)
    {
        using ResourceType = GameResource<T>;
        auto* ptr = dynamic_cast<ResourceType*>(&res);
        ASSERT(ptr);
        return *ptr;
    }

    using MaterialResource       = GameResource<gfx::MaterialClass>;
    using ParticleSystemResource = GameResource<gfx::KinematicsParticleEngineClass>;
    using CustomShapeResource    = GameResource<gfx::PolygonClass>;
    using AnimationResource      = GameResource<game::AnimationClass>;

} // namespace
