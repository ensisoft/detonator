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
#  include <QJsonArray>
#  include <QSet>
#  include <QIcon>
#  include <QAbstractTableModel>
#  include <boost/logic/tribool.hpp>
#include "warnpop.h"

#include <memory>
#include <string>
#include <vector>

#include "base/assert.h"
#include "data/writer.h"
#include "data/reader.h"
#include "audio/graph.h"
#include "graphics/drawable.h"
#include "graphics/polygon_mesh.h"
#include "graphics/particle_engine.h"
#include "graphics/material.h"
#include "game/entity.h"
#include "game/scene.h"
#include "game/tilemap.h"
#include "uikit/window.h"
#include "editor/app/utility.h"
#include "editor/app/script.h"
#include "editor/app/types.h"

namespace app
{
    class ResourcePacker;
    class ResourceMigrationLog;

    // Editor app resource object. These are objects that the user
    // manipulates and manages through the Editor application's UI.
    // Each resource contains the actual underlying resource object
    // that is for example a gfx::Material or game::Entity.
    // Normally these types are not related in any hierarchy yet in
    // the editor we want to manage/view/list/edit/delete generic
    // "resources" that the user has created/imported. This interface
    // creates that base root resource hierarchy that is only available
    // in the editor application.
    // Additionally, it's possible to associate some arbitrary properties
    // with each resource object to support Editor functionality.
    class Resource
    {
    public:
        using PropertyCollection = QVariantMap;
        using TagSoup = QSet<QString>;

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
            UI,
            // It's a tilemap
            Tilemap
        };
        virtual ~Resource() = default;
        // Get the identifier of the class object type.
        virtual AnyString GetId() const = 0;
        // Get the human-readable name of the resource.
        virtual AnyString GetName() const = 0;
        // Get the type of the resource.
        virtual Type GetType() const = 0;
        virtual const TagSoup& GetTags() const = 0;
        virtual const PropertyCollection& GetProperties() const = 0;
        virtual const PropertyCollection& GetUserProperties() const = 0;
        // Copy the content object from the source resource into this resource
        virtual void CopyContent(const Resource& source) = 0;
        // Set the name of the resource.
        virtual void SetName(const AnyString& name) = 0;
        // Set the resource properties
        virtual void SetProperties(const PropertyCollection& props) = 0;
        // Set the per user resource properties
        virtual void SetUserProperties(const PropertyCollection& props) = 0;
        virtual void SetTags(const TagSoup& soup) = 0;
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
        virtual bool HasProperty(const PropertyKey& key) const = 0;
        // returns true if the resource as a user property by the given name.
        virtual bool HasUserProperty(const PropertyKey& name) const = 0;
        // Load the additional properties from the json object.
        virtual void LoadProperties(const QJsonObject& json) = 0;
        // Load the additional properties from the json object.
        virtual void LoadUserProperties(const QJsonObject& json) = 0;
        // Delete a property by the given key/name.
        virtual void DeleteProperty(const PropertyKey& key) = 0;
        // Delete a user property by the given key/name.
        virtual void DeleteUserProperty(const PropertyKey& key) = 0;
        // Make an exact copy of this resource. This means
        // that the copied resource contains all the same properties
        // as this object including the resource id.
        virtual std::unique_ptr<Resource> Copy() const = 0;
        // Make a duplicate clone of this resource. This means
        // that the duplicated resource contains all the same properties
        // as this object but is a distinct resource object (type) and
        // has a different/unique resource id.
        virtual std::unique_ptr<Resource> Clone() const = 0;
        // List the IDs of resources that this resource depends on.
        virtual QStringList ListDependencies() const = 0;
        // List the resource tags.
        virtual QStringList ListTags() const = 0;

        // Tag management
        virtual void AddTag(const AnyString& tag) = 0;
        virtual void DelTag(const AnyString& tag) = 0;
        virtual bool HasTag(const AnyString& tag) const = 0;

        virtual bool Pack(ResourcePacker& packer) = 0;

        // Migrate resource from previous version to the current version.
        virtual void Migrate(ResourceMigrationLog* log) = 0;

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
        inline bool IsTilemap() const
        { return GetType() == Type::Tilemap; }
        inline bool IsDrawable() const
        { return GetType() == Type::Drawable; }

        inline bool IsAnyDrawableType() const
        {
            return IsDrawable() || IsParticleEngine() || IsCustomShape();
        }

        // property helpers.
        inline void SetProperty(const PropertyKey& key, const PropertyValue& value)
        { SetVariantProperty(key, value); }

        // Return the value of the property identified by name.
        // If the property doesn't exist returns a null variant.
        template<typename T>
        T GetProperty(const PropertyKey& key, const T& def) const
        {
            const auto& variant = GetVariantProperty(key);
            if (variant.isNull())
                return def;

            const PropertyValue property(variant);

            T value;
            property.GetValue(&value);

            return value;
        }
        template<typename T>
        bool GetProperty(const PropertyKey& key, T* out) const
        {
            const auto& variant = GetVariantProperty(key);
            if (variant.isNull())
                return false;

            const PropertyValue property(variant);
            property.GetValue(out);
            return true;
        }

        // Set a user specific property value. If the property exists already the previous
        // value is overwritten. Otherwise, it's added.
        inline void SetUserProperty(const PropertyKey& key, const PropertyValue& value)
        { SetUserVariantProperty(key, value); }

        // Return the value of the property identified by name.
        // If the property doesn't exist returns a null variant.
        template<typename T>
        T GetUserProperty(const PropertyKey& key, const T& def) const
        {
            const auto& variant = GetUserVariantProperty(key);
            if (variant.isNull())
                return def;

            const PropertyValue property(variant);

            T value;
            property.GetValue(&value);

            return value;
        }
        template<typename T>
        bool GetUserProperty(const PropertyKey& key, T* out) const
        {
            const auto& variant = GetUserVariantProperty(key);
            if (variant.isNull())
                return false;

            const PropertyValue property(variant);
            property.GetValue(out);
            return true;
        }

        bool GetContent(const gfx::DrawableClass** content) const
        {
            if (IsAnyDrawableType())
            {
                *content = static_cast<const gfx::DrawableClass*>(GetRaw());
                return true;
            }
            return false;
        }
        bool GetContent(gfx::DrawableClass** content)
        {
            if (IsAnyDrawableType())
            {
                *content = static_cast<gfx::DrawableClass*>(GetRaw());
                return true;
            }
            return false;
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
                case Resource::Type::Tilemap:
                    return QIcon("icons:tilemap.png");
                default: break;
            }
            return QIcon();
        }
    protected:
        virtual void* GetRaw() = 0;
        virtual const void* GetRaw() const = 0;
        virtual void* GetIf(const std::type_info& expected_type) = 0;
        virtual const void* GetIf(const std::type_info& expected_type) const = 0;
        virtual void SetVariantProperty(const PropertyKey& key, const QVariant& value) = 0;
        virtual void SetUserVariantProperty(const PropertyKey& key, const QVariant& value) = 0;
        virtual QVariant GetVariantProperty(const PropertyKey& key) const = 0;
        virtual QVariant GetUserVariantProperty(const PropertyKey& key) const = 0;
    private:
    };

    namespace detail {
        // use template specialization to map a gfx type
        // to a Resource::Type enum value.
        template<typename T>
        struct ResourceTypeTraits;

        template<>
        struct ResourceTypeTraits<gfx::ParticleEngineClass> {
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
        struct ResourceTypeTraits<game::TilemapClass> {
            static constexpr auto Type = app::Resource::Type::Tilemap;
        };

        template<>
        struct ResourceTypeTraits<gfx::PolygonMeshClass> {
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

        template<typename ResourceType> inline
        QVariantMap DuplicateResourceProperties(const ResourceType& src, const ResourceType& dupe, const QVariantMap& props)
        {
            // generic shim function, simply return a copy of the original source properties.
            return props;
        }
        template<typename ResourceType> inline
        QVariantMap DuplicateUserResourceProperties(const ResourceType& src, const ResourceType& dupe, const QVariantMap& props)
        {
            // generic shim function, simply return a copy of the original source properties.
            return props;
        }

        QVariantMap DuplicateResourceProperties(const game::EntityClass& src, const game::EntityClass& dupe, const QVariantMap& props);

        template<typename ResourceType> inline
        QStringList ListResourceDependencies(const ResourceType& res, const QVariantMap& props)
        {
            return QStringList();
        }

        QStringList ListResourceDependencies(const gfx::PolygonMeshClass& poly, const QVariantMap& props);
        QStringList ListResourceDependencies(const gfx::ParticleEngineClass& particles, const QVariantMap& props);

        QStringList ListResourceDependencies(const game::EntityClass& entity, const QVariantMap& props);
        QStringList ListResourceDependencies(const game::SceneClass& scene, const QVariantMap& props);
        QStringList ListResourceDependencies(const game::TilemapClass& map, const QVariantMap& props);
        QStringList ListResourceDependencies(const uik::Window& window, const QVariantMap& props);

        template<typename ResourceType> inline
        bool PackResource(const ResourceType&, const ResourcePacker&)
        { return true; }

        bool PackResource(app::Script& script, ResourcePacker& packer);
        bool PackResource(app::DataFile& data, ResourcePacker& packer);
        bool PackResource(audio::GraphClass& audio, ResourcePacker& packer);
        bool PackResource(game::EntityClass& entity, ResourcePacker& packer);
        bool PackResource(game::TilemapClass& map, ResourcePacker& packer);
        bool PackResource(uik::Window& window, ResourcePacker& packer);
        bool PackResource(gfx::MaterialClass& material, ResourcePacker& packer);

        // Stub for migrating the low level data chunk from one version to another.
        // This stub does nothing and returns the original chunk. Resources that
        // require this functionality should then have a specialization of this stub.
        template<typename ResourceType> inline
        std::unique_ptr<data::Chunk> MigrateResourceDataChunk(std::unique_ptr<data::Chunk> chunk, ResourceMigrationLog* log)
        { return chunk; }

        template<>
        std::unique_ptr<data::Chunk> MigrateResourceDataChunk<game::EntityClass>(std::unique_ptr<data::Chunk> chunk, ResourceMigrationLog* log);

        template<typename ResourceType> inline
        void MigrateResource(const ResourceType&, ResourceMigrationLog*, unsigned old_version, unsigned new_version)
        {}

        void MigrateResource(uik::Window& window, ResourceMigrationLog* log, unsigned old_version, unsigned new_version);
        void MigrateResource(gfx::MaterialClass& material, ResourceMigrationLog* log, unsigned old_version, unsigned new_version);
        void MigrateResource(game::EntityClass& entity, ResourceMigrationLog* log, unsigned  old_version, unsigned new_version);

    } // detail

    template<typename BaseTypeContent>
    class GameResourceBase : public Resource
    {
    public:
        virtual std::shared_ptr<const BaseTypeContent> GetSharedResource() const = 0;
        virtual std::shared_ptr<BaseTypeContent> GetSharedResource() = 0;
    private:
    };

    template<typename BaseType, typename DerivedType = BaseType>
    class GameResource : public GameResourceBase<BaseType>
    {
    public:
        static constexpr auto TypeValue = detail::ResourceTypeTraits<BaseType>::Type;

        GameResource(const DerivedType& content, const AnyString& name)
        {
            mContent = std::make_shared<DerivedType>(content);
            SetName(name);
        }
        GameResource(DerivedType&& content, const AnyString& name)
        {
            mContent = std::make_shared<DerivedType>(std::move(content));
            SetName(name);
        }
        GameResource(std::shared_ptr<DerivedType> content, const AnyString& name)
        {
            mContent = content;
            SetName(name);
        }
        template<typename T>
        GameResource(std::unique_ptr<T> content, const AnyString& name)
        {
            std::shared_ptr<T> shared(std::move(content));
            mContent = std::static_pointer_cast<DerivedType>(shared);
            SetName(name);
        }
        template<typename T>
        GameResource(std::shared_ptr<T> content, const AnyString& name)
        {
            mContent = std::static_pointer_cast<DerivedType>(content);
            SetName(name);
        }
        GameResource(const AnyString& name)
        {
            mContent = std::make_shared<DerivedType>();
            SetName(name);
        }
        GameResource(const GameResource& other)
        {
            mContent   = std::make_shared<DerivedType>(*other.mContent);
            mProps     = other.mProps;
            mUserProps = other.mUserProps;
            mPrimitive = other.mPrimitive;
            mTagSoup   = other.mTagSoup;
        }

        AnyString GetId() const override
        { return mContent->GetId();  }
        AnyString GetName() const override
        { return mContent->GetName(); }
        Resource::Type GetType() const override
        { return TypeValue; }
        const Resource::TagSoup& GetTags() const override
        { return mTagSoup; }
        const Resource::PropertyCollection& GetProperties() const override
        { return mProps; }
        const Resource::PropertyCollection& GetUserProperties() const override
        { return mUserProps; }
        void SetName(const AnyString& name) override
        { mContent->SetName(name); }

        void CopyContent(const Resource& other) override
        {
            const auto* ptr = dynamic_cast<const GameResource*>(&other);
            ASSERT(ptr != nullptr);
            *mContent  = *ptr->mContent;
        }
        void SetProperties(const Resource::PropertyCollection& props) override
        { mProps = props; }
        void SetUserProperties(const Resource::PropertyCollection& props) override
        { mUserProps = props; }
        void SetTags(const Resource::TagSoup& soup) override
        { mTagSoup = soup; }

        void SetIsPrimitive(bool primitive) override
        { mPrimitive = primitive; }
        bool IsPrimitive() const override
        { return mPrimitive; }
        void Serialize(data::Writer& data) const override
        {
            auto chunk = data.NewWriteChunk();
            mContent->IntoJson(*chunk);
            // tag some additional data with the content's JSON
            //ASSERT(chunk->HasValue("resource_name") == false);
            //ASSERT(chunk->HasValue("resource_id") == false);
            chunk->Write("resource_name", mContent->GetName());
            chunk->Write("resource_id",   mContent->GetId());
            chunk->Write("resource_ver",  1);

            if constexpr (TypeValue == Resource::Type::Material)
            {
                chunk->Write("resource_ver", 4);
                data.AppendChunk("materials", std::move(chunk));
            }
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
            else if (TypeValue == Resource::Type::Tilemap)
                data.AppendChunk("tilemaps", std::move(chunk));
        }
        void SaveProperties(QJsonObject& root) const override
        {
            auto object = QJsonObject::fromVariantMap(mProps);

            QJsonArray tags;
            for (auto tag : mTagSoup)
                tags.push_back(tag);

            object["__tag_list"] = tags;
            root[GetId()] = std::move(object);
        }
        void SaveUserProperties(QJsonObject& json) const override
        { json[GetId()] = QJsonObject::fromVariantMap(mUserProps); }

        bool HasProperty(const PropertyKey& key) const override
        { return mProps.contains(key); }
        bool HasUserProperty(const PropertyKey& key) const override
        { return mUserProps.contains(key); }
        void LoadProperties(const QJsonObject& root) override
        {
            auto object = root[GetId()].toObject();

            Resource::TagSoup soup;

            if(object.contains("__tag_list"))
            {
                const auto tags = object["__tag_list"].toArray();
                for (const auto t : tags)
                    soup.insert(t.toString());

                object.remove("__tag_list");
            }

            mProps = object.toVariantMap();
            mTagSoup = std::move(soup);
        }
        void LoadUserProperties(const QJsonObject& object) override
        { mUserProps = object[GetId()].toObject().toVariantMap(); }
        void DeleteProperty(const PropertyKey& key) override
        { mProps.remove(key); }
        void DeleteUserProperty(const PropertyKey& key) override
        { mUserProps.remove(key); }
        std::unique_ptr<Resource> Copy() const override
        { return std::make_unique<GameResource>(*this); }
        std::unique_ptr<Resource> Clone() const override
        {
            auto ret = std::make_unique<GameResource>(mContent->Clone(), GetName());
            ret->mProps     = detail::DuplicateResourceProperties(*mContent, *ret->mContent, mProps);
            ret->mUserProps = detail::DuplicateUserResourceProperties(*mContent, *ret->mContent, mUserProps);
            ret->mPrimitive = mPrimitive;
            ret->mTagSoup   = mTagSoup;
            return ret;
        }
        QStringList ListDependencies() const override
        { return detail::ListResourceDependencies(*mContent, mProps); }

        QStringList ListTags() const override
        { return mTagSoup.values(); }

        void AddTag(const AnyString& tag)
        { mTagSoup.insert(tag); }
        void DelTag(const AnyString& tag)
        { mTagSoup.remove(tag); }
        bool HasTag(const AnyString& tag) const
        { return mTagSoup.contains(tag); }

        bool Pack(ResourcePacker& packer) override
        { return detail::PackResource(*mContent, packer); }
        void Migrate(ResourceMigrationLog* log) override
        {
            if constexpr (TypeValue == Resource::Type::Material)
            {
                unsigned current_version = 5;
                unsigned saved_version;
                ASSERT(Resource::GetProperty("__version", &saved_version));
                if (saved_version < current_version)
                    detail::MigrateResource(*mContent, log, saved_version, current_version);
            }
            else
            {
                unsigned current_version = 2;
                unsigned saved_version = 1;
                ASSERT(Resource::GetProperty("__version", &saved_version));
                if (saved_version < current_version)
                    detail::MigrateResource(*mContent, log, saved_version, current_version);
            }
        }

        // GameResourceBase
        std::shared_ptr<const BaseType> GetSharedResource() const override
        { return mContent; }
        std::shared_ptr<BaseType> GetSharedResource() override
        { return mContent; }

        DerivedType* GetContent()
        { return mContent.get(); }
        const DerivedType* GetContent() const
        { return mContent.get(); }
        void ClearProperties()
        { mProps.clear(); }
        void ClearUserProperties()
        { mUserProps.clear(); }

        using Resource::GetContent;

        GameResource& operator=(const GameResource& other) = delete;

    protected:
        void* GetRaw() override
        {
            return mContent.get();
        }
        const void* GetRaw() const override
        {
            return mContent.get();
        }

        void* GetIf(const std::type_info& expected_type) override
        {
            if (typeid(DerivedType) == expected_type)
                return (void*)mContent.get();
            return nullptr;
        }
        const void* GetIf(const std::type_info& expected_type) const override
        {
            if (typeid(DerivedType) == expected_type)
                return (const void*)mContent.get();
            return nullptr;
        }
        void SetVariantProperty(const PropertyKey& key, const QVariant& value) override
        { mProps[key] = value; }
        void SetUserVariantProperty(const PropertyKey& key, const QVariant& value) override
        { mUserProps[key] = value; }
        QVariant GetVariantProperty(const PropertyKey& key) const override
        { return mProps[key]; }
        QVariant GetUserVariantProperty(const PropertyKey& key) const override
        { return mUserProps[key]; }
    private:
        std::shared_ptr<DerivedType> mContent;
        QVariantMap mProps;
        QVariantMap mUserProps;
        Resource::TagSoup mTagSoup;
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
    using ParticleSystemResource = GameResource<gfx::ParticleEngineClass>;
    using CustomShapeResource    = GameResource<gfx::PolygonMeshClass>;
    using EntityResource         = GameResource<game::EntityClass>;
    using SceneResource          = GameResource<game::SceneClass>;
    using TilemapResource        = GameResource<game::TilemapClass>;
    using AudioResource          = GameResource<audio::GraphClass>;
    using ScriptResource         = GameResource<Script>;
    using DataResource           = GameResource<DataFile>;
    using UIResource             = GameResource<uik::Window>;

    template<typename DerivedType>
    using DrawableResource = GameResource<gfx::DrawableClass, DerivedType>;

    struct ResourceListItem {
        AnyString name;
        AnyString id;
        AnyString tag;
        QIcon icon;
        const Resource* resource = nullptr;
        boost::tribool selected = boost::indeterminate;
    };

    using ResourceList = std::vector<ResourceListItem>;

    class ResourceListModel : public QAbstractTableModel
    {
    public:
        explicit ResourceListModel(const ResourceList& list)
          : mResources(list)
        {}
        ResourceListModel() = default;

        QVariant data(const QModelIndex& index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        int rowCount(const QModelIndex&) const override
        { return static_cast<int>(mResources.size()); }
        int columnCount(const QModelIndex&) const override
        { return 2; }
        void SetList(const ResourceList& list);
        void Clear();

        inline const ResourceList& GetList() const noexcept
        { return mResources; }

    private:
        ResourceList mResources;
    };

} // namespace
