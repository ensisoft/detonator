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

#include "config.h"

#include "warnpush.h"
#  include <QString>
#  include <QVariant>
#  include <QVariantMap>
#  include <QJsonObject>
#  include <QIcon>
#include "warnpop.h"

#include <memory>
#include <string>

#include "base/assert.h"
#include "data/writer.h"
#include "audio/graph.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "game/entity.h"
#include "game/scene.h"
#include "uikit/window.h"
#include "editor/app/utility.h"
#include "editor/app/script.h"

namespace app
{
    // Editor app resource object. These are objects that the user
    // manipulates and manages through the Editor application's UI.
    // Each resource contains the actual underlying resource object
    // that is for example a gfx::Material or game::Entity.
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
            // It's a (custom) shape (drawable)
            Shape,
            // It's a generic drawable.
            Drawable,
            // It's an entity description
            Entity,
            // It's a scene description
            Scene,
            // it's a script file.
            Script,
            // it's an audio graph with a network of audio elements
            AudioGraph,
            // it's an arbitrary application/game data file
            DataFile,
            // it's a UI / window description
            UI
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
        virtual void Serialize(data::Writer& data) const =0;
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

        // helpers
        inline std::string GetNameUtf8() const
        { return app::ToUtf8(GetName()); }
        inline std::string GetIdUtf8() const
        { return app::ToUtf8(GetId()); }
        inline QIcon GetIcon() const
        { return GetIcon(GetType()); }
        inline bool IsMaterial() const
        { return GetType() == Type::Material; }
        inline bool IsParticleEngine() const
        { return GetType() == Type::ParticleSystem; }
        inline bool IsCustomShape() const
        { return GetType() == Type::Shape; }
        inline bool IsEntity() const
        { return GetType() == Type::Entity; }
        inline bool IsScene() const
        { return GetType() == Type::Scene; }
        inline bool IsScript() const
        { return GetType() == Type::Script; }
        inline bool IsAudioGraph() const
        { return GetType() == Type::AudioGraph; }
        inline bool IsDataFile() const
        { return GetType() == Type::DataFile; }
        inline bool IsUI() const
        { return GetType() == Type::UI; }

        // property helpers.
        // There's a lot of stuff that goes into QVariant but then doesn't
        // serialize correctly. For instance QColor and QByteArray.

        // Set a resource specific property value. If the property exists already the previous
        // value is overwritten. Otherwise it's added.
        inline void SetProperty(const QString& name, const QByteArray& bytes)
        { SetVariantProperty(name, bytes.toBase64()); }
        inline void SetProperty(const QString& name, const QColor& color)
        { SetVariantProperty(name, color); }
        inline void SetProperty(const QString& name, const QString& value)
        { SetVariantProperty(name, value); }
        inline void SetProperty(const QString& name, quint64 value)
        { SetVariantProperty(name, value); }
        inline void SetProperty(const QString& name, qint64 value)
        { SetVariantProperty(name, value); }
        inline void SetProperty(const QString& name, unsigned value)
        { SetVariantProperty(name, value); }
        inline void SetProperty(const QString& name, int value)
        { SetVariantProperty(name, value); }
        inline void SetProperty(const QString& name, double value)
        { SetVariantProperty(name, value); }
        inline void SetProperty(const QString& name, float value)
        { SetVariantProperty(name, value); }
        inline void SetProperty(const QString& name, const QVariantMap& map)
        {
            ASSERT(ValidateQVariantMapJsonSupport(map));
            SetVariantProperty(name, map);
        }

        // Return the value of the property identified by name.
        // If the property doesn't exist returns a null variant.
        QByteArray GetProperty(const QString& name, const QByteArray& def) const
        {
            const auto& ret  = GetVariantProperty(name);
            if (ret.isNull())
                return def;
            const auto& str = ret.toString();
            if (!str.isEmpty())
                return QByteArray::fromBase64(str.toLatin1());
            return QByteArray();
        }
        bool GetProperty(const QString& name, QByteArray* out) const
        {
            const auto& ret = GetVariantProperty(name);
            if (ret.isNull())
                return false;
            const auto& str = ret.toString();
            if (!str.isEmpty())
                *out = QByteArray::fromBase64(str.toLatin1());
            return true;
        }
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

        // Set a user specific property value. If the property exists already the previous
        // value is overwritten. Otherwise it's added.
        inline void SetUserProperty(const QString& name, const QByteArray& bytes)
        { SetUserVariantProperty(name, bytes.toBase64()); }
        inline void SetUserProperty(const QString& name, const QColor& color)
        { SetUserVariantProperty(name, color); }
        inline void SetUserProperty(const QString& name, const QString& value)
        { SetUserVariantProperty(name, value); }
        inline void SetUserProperty(const QString& name, quint64 value)
        { SetUserVariantProperty(name, value); }
        inline void SetUserProperty(const QString& name, qint64 value)
        { SetUserVariantProperty(name, value); }
        inline void SetUserProperty(const QString& name, unsigned value)
        { SetUserVariantProperty(name, value); }
        inline void SetUserProperty(const QString& name, int value)
        { SetUserVariantProperty(name, value); }
        inline void SetUserProperty(const QString& name, double value)
        { SetUserVariantProperty(name, value); }
        inline void SetUserProperty(const QString& name, float value)
        { SetUserVariantProperty(name, value); }
        inline void SetUserProperty(const QString& name, const QVariantMap& map)
        {
            ASSERT(ValidateQVariantMapJsonSupport(map));
            SetVariantProperty(name, map);
        }

        // Return the value of the property identified by name.
        // If the property doesn't exist returns a null variant.
        QByteArray GetUserProperty(const QString& name, const QByteArray& def) const
        {
            const auto& ret  = GetUserVariantProperty(name);
            if (ret.isNull())
                return def;
            const auto& str = ret.toString();
            if (!str.isEmpty())
                return QByteArray::fromBase64(str.toLatin1());
            return QByteArray();
        }
        bool GetUserProperty(const QString& name, QByteArray* out) const
        {
            const auto& ret = GetUserVariantProperty(name);
            if (ret.isNull())
                return false;
            const auto& str = ret.toString();
            if (!str.isEmpty())
                *out = QByteArray::fromBase64(str.toLatin1());
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

        static QIcon GetIcon(Resource::Type type)
        {
            switch (type) {
                case Resource::Type::Material:
                    return QIcon("icons:material.png");
                case Resource::Type::ParticleSystem:
                    return QIcon("icons:particle.png");
                case Resource::Type::Shape:
                    return QIcon("icons:polygon.png");
                case Resource::Type::Entity:
                    return QIcon("icons:entity.png");
                case Resource::Type::Scene:
                    return QIcon("icons:scene.png");
                case Resource::Type::Script:
                    return QIcon("icons:script.png");
                case Resource::Type::AudioGraph:
                    return QIcon("icons:audio.png");
                case Resource::Type::DataFile:
                    return QIcon("icons:database.png");
                case Resource::Type::UI:
                    return QIcon("icons:ui.png");
                default: break;
            }
            return QIcon();
        }
    protected:
        virtual void* GetIf(const std::type_info& expected_type) = 0;
        virtual const void* GetIf(const std::type_info& expected_type) const = 0;
        virtual void SetVariantProperty(const QString& name, const QVariant& value) = 0;
        virtual void SetUserVariantProperty(const QString& name, const QVariant& value) = 0;
        virtual QVariant GetVariantProperty(const QString& name) const = 0;
        virtual QVariant GetUserVariantProperty(const QString& name) const = 0;
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
        struct ResourceTypeTraits<game::EntityClass> {
            static constexpr auto Type = app::Resource::Type::Entity;
        };
        template<>
        struct ResourceTypeTraits<game::SceneClass> {
            static constexpr auto Type = app::Resource::Type::Scene;
        };

        template<>
        struct ResourceTypeTraits<gfx::PolygonClass> {
            static constexpr auto Type = app::Resource::Type::Shape;
        };
        template <>
        struct ResourceTypeTraits<gfx::DrawableClass> {
            static constexpr auto Type = app::Resource::Type::Drawable;
        };
        template<>
        struct ResourceTypeTraits<Script> {
            static constexpr auto Type = app::Resource::Type::Script;
        };
        template<>
        struct ResourceTypeTraits<audio::GraphClass> {
            static constexpr auto Type = app::Resource::Type::AudioGraph;
        };
        template<>
        struct ResourceTypeTraits<DataFile> {
            static constexpr auto Type = app::Resource::Type::DataFile;
        };
        template<>
        struct ResourceTypeTraits<uik::Window> {
            static constexpr auto Type = app::Resource::Type::UI;
        };
    } // detail

    template<typename BaseTypeContent>
    class GameResourceBase : public Resource
    {
    public:
        virtual std::shared_ptr<const BaseTypeContent> GetSharedResource() const = 0;
        virtual std::shared_ptr<BaseTypeContent> GetSharedResource() = 0;
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
        GameResource(std::shared_ptr<Content> content, const QString& name)
        {
            mContent = content;
            mName    = name;
        }
        template<typename T>
        GameResource(std::unique_ptr<T> content, const QString& name)
        {
            std::shared_ptr<T> shared(std::move(content));
            mContent = std::static_pointer_cast<Content>(shared);
            mName    = name;
        }
        template<typename T>
        GameResource(std::shared_ptr<T> content, const QString& name)
        {
            mContent = std::static_pointer_cast<Content>(content);
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
        {
            mName = name;
            // not all underlying resource types have the name
            // property. so hence this mess here.
            if constexpr (TypeValue == Resource::Type::Entity)
                mContent->SetName(app::ToUtf8(name));
            else if constexpr (TypeValue == Resource::Type::Scene)
                mContent->SetName(app::ToUtf8(name));
            else if constexpr (TypeValue == Resource::Type::UI)
                mContent->SetName(app::ToUtf8(name));
            else if constexpr (TypeValue == Resource::Type::AudioGraph)
                mContent->SetName(app::ToUtf8(name));
            else if constexpr (TypeValue == Resource::Type::UI)
                mContent->SetName(app::ToUtf8(name));
            else if constexpr (TypeValue == Resource::Type::Material)
                mContent->SetName(app::ToUtf8(name));
        }
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
        virtual void Serialize(data::Writer& data) const override
        {
            auto chunk = data.NewWriteChunk();
            mContent->IntoJson(*chunk);
            // tag some additional data with the content's JSON
            //ASSERT(chunk->HasValue("resource_name") == false);
            //ASSERT(chunk->HasValue("resource_id") == false);
            chunk->Write("resource_name", app::ToUtf8(mName));
            chunk->Write("resource_id",   app::ToUtf8(GetId()));

            if constexpr (TypeValue == Resource::Type::Material)
                data.AppendChunk("materials", std::move(chunk));
            else if (TypeValue == Resource::Type::ParticleSystem)
                data.AppendChunk("particles", std::move(chunk));
            else if (TypeValue == Resource::Type::Shape)
                data.AppendChunk("shapes", std::move(chunk));
            else if (TypeValue == Resource::Type::Entity)
                data.AppendChunk("entities", std::move(chunk));
            else if (TypeValue == Resource::Type::Scene)
                data.AppendChunk("scenes", std::move(chunk));
            else if (TypeValue == Resource::Type::Script)
                data.AppendChunk("scripts", std::move(chunk));
            else if (TypeValue == Resource::Type::AudioGraph)
                data.AppendChunk("audio_graphs", std::move(chunk));
            else if (TypeValue == Resource::Type::DataFile)
                data.AppendChunk("data_files", std::move(chunk));
            else if (TypeValue == Resource::Type::UI)
                data.AppendChunk("uis", std::move(chunk));
        }
        virtual void SaveProperties(QJsonObject& json) const override
        { json[GetId()] = QJsonObject::fromVariantMap(mProps); }
        virtual void SaveUserProperties(QJsonObject& json) const override
        { json[GetId()] = QJsonObject::fromVariantMap(mUserProps); }
        virtual bool HasProperty(const QString& name) const override
        { return mProps.contains(name); }
        virtual bool HasUserProperty(const QString& name) const override
        { return mUserProps.contains(name); }
        virtual void LoadProperties(const QJsonObject& object) override
        { mProps = object[GetId()].toObject().toVariantMap(); }
        virtual void LoadUserProperties(const QJsonObject& object) override
        { mUserProps = object[GetId()].toObject().toVariantMap(); }
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
        virtual std::shared_ptr<BaseType> GetSharedResource() override
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
        virtual void SetVariantProperty(const QString& name, const QVariant& value) override
        { mProps[name] = value; }
        virtual void SetUserVariantProperty(const QString& name, const QVariant& value) override
        { mUserProps[name] = value; }
        virtual QVariant GetVariantProperty(const QString& name) const override
        { return mProps[name]; }
        virtual QVariant GetUserVariantProperty(const QString& name) const override
        { return mUserProps[name]; }
    private:
        std::shared_ptr<Content> mContent;
        QString mName;
        QVariantMap mProps;
        QVariantMap mUserProps;
        bool mPrimitive = false;
    };

    class MaterialResource : public GameResourceBase<gfx::MaterialClass>
    {
    public:
        MaterialResource() = default;
        MaterialResource(const gfx::MaterialClass& klass, const QString& name)
        {
            mKlass = klass.Copy();
            mName  = name;
        }
        MaterialResource(std::shared_ptr<gfx::MaterialClass> klass, const QString& name)
        {
            mKlass = klass;
            mName  = name;
        }
        MaterialResource(std::unique_ptr<gfx::MaterialClass> klass, const QString& name)
        {
            mKlass = std::move(klass);
            mName  = name;
        }
        MaterialResource(const QString& name)
        { mName = name; }
        MaterialResource(const MaterialResource& other)
        {
            mKlass = other.mKlass->Copy();
            mName  = other.mName;
            mProps = other.mProps;
            mUserProps = other.mUserProps;
        }
        virtual QString GetId() const override
        { return app::FromUtf8(mKlass->GetId());  }
        virtual QString GetName() const override
        { return mName; }
        virtual Resource::Type GetType() const override
        { return Resource::Type::Material; }
        virtual void SetName(const QString& name) override
        { mName = name; }
        virtual void UpdateFrom(const Resource& other) override
        {
            const auto* ptr = dynamic_cast<const MaterialResource*>(&other);
            ASSERT(ptr != nullptr);
            mKlass     = ptr->mKlass->Copy();
            mName      = ptr->mName;
            mProps     = ptr->mProps;
            mUserProps = ptr->mUserProps;
        }
        virtual void SetIsPrimitive(bool primitive) override
        { mPrimitive = primitive; }
        virtual bool IsPrimitive() const override
        { return mPrimitive; }
        virtual void Serialize(data::Writer& data) const override
        {
            auto chunk = data.NewWriteChunk();
            mKlass->IntoJson(*chunk);
            chunk->Write("resource_name", app::ToUtf8(mName));
            chunk->Write("resource_id", mKlass->GetId());
            data.AppendChunk("materials", std::move(chunk));
        }
        virtual void SaveProperties(QJsonObject& json) const override
        { json[GetId()] = QJsonObject::fromVariantMap(mProps); }
        virtual void SaveUserProperties(QJsonObject& json) const override
        { json[GetId()] = QJsonObject::fromVariantMap(mUserProps); }
        virtual bool HasProperty(const QString& name) const override
        { return mProps.contains(name); }
        virtual bool HasUserProperty(const QString& name) const override
        { return mUserProps.contains(name); }
        virtual void LoadProperties(const QJsonObject& object) override
        { mProps = object[GetId()].toObject().toVariantMap(); }
        virtual void LoadUserProperties(const QJsonObject& object) override
        { mUserProps = object[GetId()].toObject().toVariantMap(); }
        virtual std::unique_ptr<Resource> Copy() const override
        {
            auto ret = std::make_unique<MaterialResource>();
            ret->mKlass     = mKlass->Copy();
            ret->mName      = mName;
            ret->mProps     = mProps;
            ret->mUserProps = mUserProps;
            ret->mPrimitive = mPrimitive;
            return ret;
        }
        virtual std::unique_ptr<Resource> Clone() const override
        {
            auto ret = std::make_unique<MaterialResource>();
            ret->mKlass = mKlass->Clone();
            ret->mName  = mName;
            ret->mProps = mProps;
            ret->mUserProps = mUserProps;
            ret->mPrimitive = mPrimitive;
            return ret;
        }

        // GameResourceBase
        virtual std::shared_ptr<const gfx::MaterialClass> GetSharedResource() const override
        { return mKlass; }
        virtual std::shared_ptr<gfx::MaterialClass> GetSharedResource() override
        { return mKlass; }
        MaterialResource& operator=(const MaterialResource&) = delete;
    protected:
        virtual void* GetIf(const std::type_info& expected_type) override
        {
            if (typeid(gfx::MaterialClass) == expected_type)
                return (void*)mKlass.get();
            return nullptr;
        }
        virtual const void* GetIf(const std::type_info& expected_type) const override
        {
            if (typeid(gfx::MaterialClass) == expected_type)
                return (const void*)mKlass.get();
            return nullptr;
        }
        virtual void SetVariantProperty(const QString& name, const QVariant& value) override
        { mProps[name] = value; }
        virtual void SetUserVariantProperty(const QString& name, const QVariant& value) override
        { mUserProps[name] = value; }
        virtual QVariant GetVariantProperty(const QString& name) const override
        { return mProps[name]; }
        virtual QVariant GetUserVariantProperty(const QString& name) const override
        { return mUserProps[name]; }
    private:
        QString mName;
        std::shared_ptr<gfx::MaterialClass> mKlass;
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

    //using MaterialResource       = GameResource<gfx::MaterialClass>;
    using ParticleSystemResource = GameResource<gfx::KinematicsParticleEngineClass>;
    using CustomShapeResource    = GameResource<gfx::PolygonClass>;
    using EntityResource         = GameResource<game::EntityClass>;
    using SceneResource          = GameResource<game::SceneClass>;
    using AudioResource          = GameResource<audio::GraphClass>;
    using ScriptResource         = GameResource<Script>;
    using DataResource           = GameResource<DataFile>;
    using UIResource             = GameResource<uik::Window>;

    template<typename DerivedType>
    using DrawableResource = GameResource<gfx::DrawableClass, DerivedType>;

} // namespace
