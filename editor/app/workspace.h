// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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
#  include <QAbstractTableModel>
#  include <QSortFilterProxyModel>
#  include <private/qabstractfileengine_p.h> // private (deprecated) in Qt5
#  include <QString>
#  include <QMap>
#  include <QObject>
#  include <QVariant>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>
#include <set>

#include "base/assert.h"
#include "audio/loader.h"
#include "audio/format.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/resource.h"
#include "graphics/device.h"
#include "engine/classlib.h"
#include "engine/loader.h"
#include "game/loader.h"
#include "editor/app/resource.h"
#include "editor/app/utility.h"
#include "editor/app/buffer.h"
#include "editor/app/project_settings.h"

namespace app
{
    class ZipArchive;

    class WorkspaceAsyncWorkObserver;

    // Workspace groups together a collection of resources
    // that user can edit and work with such as materials,
    // animations and particle engines. Each resource has
    // capability to store additional properties that are
    // only available in the editor program but are not part
    // of the gfx content's parameters. These properties are
    // persisted as part of the workspace data.
    // The workspace itself can also contain arbitrary properties
    // that can useful for various purposes.
    class Workspace : public QAbstractTableModel,
                      public QAbstractFileEngineHandler,
                      public engine::ClassLibrary,
                      public engine::Loader,
                      public gfx::Loader,
                      public audio::Loader,
                      public game::Loader

    {
        Q_OBJECT

    public:
        Workspace(const QString& dir);
       ~Workspace();

        // QAbstractTableModel implementation
        virtual QVariant data(const QModelIndex& index, int role) const override;
        virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        virtual int rowCount(const QModelIndex&) const override
        { return static_cast<int>(mUserResourceCount); }
        virtual int columnCount(const QModelIndex&) const override
        { return 2; }
        // QAbstractFileEngineHandler implementation
        virtual QAbstractFileEngine* create(const QString& file) const override;
        // game::ClassLibrary implementation
        // The methods here allow requests for resources that don't exist and return
        // nullptr as specified in the ClassLibrary API.
        virtual engine::ClassHandle<const audio::GraphClass> FindAudioGraphClassById(const std::string& id) const override;
        virtual engine::ClassHandle<const audio::GraphClass> FindAudioGraphClassByName(const std::string& name) const override;
        virtual engine::ClassHandle<const uik::Window> FindUIByName(const std::string& name) const override;
        virtual engine::ClassHandle<const uik::Window> FindUIById(const std::string& id) const override;
        virtual engine::ClassHandle<const gfx::MaterialClass> FindMaterialClassByName(const std::string& name) const override;
        virtual engine::ClassHandle<const gfx::MaterialClass> FindMaterialClassById(const std::string& id) const override;
        virtual engine::ClassHandle<const gfx::DrawableClass> FindDrawableClassById(const std::string& id) const override;
        virtual engine::ClassHandle<const game::EntityClass> FindEntityClassByName(const std::string& name) const override;
        virtual engine::ClassHandle<const game::EntityClass> FindEntityClassById(const std::string& id) const override;
        virtual engine::ClassHandle<const game::SceneClass> FindSceneClassByName(const std::string& name) const override;
        virtual engine::ClassHandle<const game::SceneClass> FindSceneClassById(const std::string& id) const override;
        virtual engine::ClassHandle<const game::TilemapClass> FindTilemapClassById(const std::string& id) const override;
        // engine::EngineDataLoader implementation.
        virtual engine::EngineDataHandle LoadEngineDataUri(const std::string& URI) const override;
        virtual engine::EngineDataHandle LoadEngineDataFile(const std::string& filename) const override;
        virtual engine::EngineDataHandle LoadEngineDataId(const std::string& id) const override;

        // gfx::ResourceLoader implementation
        virtual gfx::ResourceHandle LoadResource(const gfx::Loader::ResourceDesc& desc) override;

        // audio::Loader implementation
        virtual audio::SourceStreamHandle OpenAudioStream(const std::string& URI,
            AudioIOStrategy strategy, bool enable_file_caching) const override;
        // game::Loader implementation
        virtual game::TilemapDataHandle LoadTilemapData(const game::Loader::TilemapDataDesc& desc) const override;

        static gfx::ResourceHandle LoadAppResource(const std::string& URI);

        template<typename T>
        engine::ClassHandle<T> FindClassHandleByName(const std::string& name, Resource::Type type) const
        {
            for (const auto& resource : mResources)
            {
                if (resource->GetType() != type) continue;
                else if (resource->GetNameUtf8() != name) continue;
                return ResourceCast<T>(*resource).GetSharedResource();
            }
            return nullptr;
        }
        template<typename T>
        engine::ClassHandle<T> FindClassHandleById(const std::string& id, Resource::Type type) const
        {
            for (const auto& resource : mResources)
            {
                if (resource->GetType() != type) continue;
                else if (resource->GetIdUtf8() != id) continue;
                return ResourceCast<T>(*resource).GetSharedResource();
            }
            return nullptr;
        }

        // These are for internal use in the editor and they have different semantics
        // from the similar functions in the ClassLibrary API. Basically trying to access
        // an object that doesn't exist is considered a *bug*. Whereas the ClassLibrary methods
        // allow for access to an object that doesn't exist and simply return nullptr. It's
        // then the responsibility of the caller to determine whether that is a bug or okay
        // condition or what.
        std::unique_ptr<gfx::Material> MakeMaterialByName(const AnyString& name) const;
        std::unique_ptr<gfx::Drawable> MakeDrawableByName(const AnyString& name) const;
        std::unique_ptr<gfx::Drawable> MakeDrawableById(const AnyString& id) const;
        std::shared_ptr<const gfx::MaterialClass> GetMaterialClassByName(const AnyString& name) const;
        std::shared_ptr<const gfx::MaterialClass> GetMaterialClassById(const AnyString& id) const;
        std::shared_ptr<const gfx::DrawableClass> GetDrawableClassByName(const AnyString& name) const;
        std::shared_ptr<const gfx::DrawableClass> GetDrawableClassById(const AnyString& id) const;
        std::shared_ptr<const game::EntityClass> GetEntityClassByName(const AnyString& name) const;
        std::shared_ptr<const game::EntityClass> GetEntityClassById(const AnyString& id) const;
        std::shared_ptr<const game::TilemapClass> GetTilemapClassById(const AnyString& id) const;

        // Try to load the contents of the workspace from the current workspace dir.
        // Returns true on success. Any errors are logged.
        bool LoadWorkspace(ResourceMigrationLog* log = nullptr, WorkspaceAsyncWorkObserver* observer = nullptr);
        // Try to save the contents of the workspace into the current workspace dir.
        // Returns true on success. Any errors are logged.
        bool SaveWorkspace();

        // Map a file to the workspace and return a file URI
        AnyString MapFileToWorkspace(const AnyString& file) const;

        // Map a "workspace file" to a file path in the file system.
        AnyString MapFileToFilesystem(const AnyString& uri) const;

        // Save a resource in the workspace. If the resource by the same id
        // already exists it's overwritten, otherwise a new resource is added to
        // the workspace.
        void SaveResource(const Resource& resource);

        // Get human-readable name for the workspace.
        QString GetName() const;
        // Get current workspace directory
        QString GetDir() const;
        // Get a directory within the workspace, for example
        // data/ or lua/
        QString GetSubDir(const QString& dir, bool create = true) const;

        using ResourceList = app::ResourceList;
        ResourceList ListAudioGraphs() const;

        ResourceList ListUserDefinedUIs() const;
        // Get a list of user defined tile map resources.
        ResourceList ListUserDefinedMaps() const;
        // Get a list of user defined script resources.
        ResourceList ListUserDefinedScripts() const;
        // Get a list of user defined material names in the workspace.
        ResourceList ListUserDefinedMaterials() const;
        // Get a list of all material names in the workspace
        // including the user defined materials and the "primitive" ones.
        ResourceList ListAllMaterials() const;
        // Get a list of all primitive (built-in) materials.
        ResourceList ListPrimitiveMaterials() const;
        // Get a list of all drawable names in the workspace
        // including the user defined drawables and the "primitive" ones.
        ResourceList ListAllDrawables() const;
        // Get a list of primitive (build-in) drawables.
        ResourceList ListPrimitiveDrawables() const;
        // Get a list of user defined drawables.
        ResourceList ListUserDefinedDrawables() const;
        // Get a list of user defined entities.
        ResourceList ListUserDefinedEntities() const;
        // Get a list of user defined entity ids
        QStringList ListUserDefinedEntityIds() const;
        // Get a list of resources with the matching type and primitive flag
        ResourceList ListResources(Resource::Type type, bool primitive, bool sort = true) const;

        ResourceList ListUserDefinedResources() const;

        ResourceList ListCursors() const;

        ResourceList ListDataFiles() const;

        // List user defined resource dependencies for the given resources.
        ResourceList ListDependencies(const ModelIndexList& list) const;

        // List the resources that depend on any of the resources in the given list.
        ResourceList ListResourceUsers(const ModelIndexList& list) const;

        // List the files on the filesystem used by the user defined resources.
        // The files are provided by their URIs which can then be expanded to the
        // actual filesystem paths.
        QStringList ListFileResources(const ModelIndexList& list) const;

        // Map material ID to its human-readable name.
        QString MapMaterialIdToName(const AnyString& id) const;
        // Map drawable ID to its human-readable name.
        QString MapDrawableIdToName(const AnyString& id) const;
        QString MapEntityIdToName(const AnyString& id) const;
        QString MapResourceIdToName(const AnyString& id) const;

        // Checks whether the material class id is a valid material class.
        // Includes primitives and user defined materials.
        bool IsValidMaterial(const AnyString& id) const;
        // Checks whether the drawable class id is a valid drawable class.
        // Includes primitives and user defined drawable shapes.
        bool IsValidDrawable(const AnyString& id) const;

        bool IsValidScript(const AnyString& id) const;

        bool IsValidTilemap(const AnyString& id) const;

        bool IsUserDefinedResource(const AnyString& id) const;

        bool IsValidUI(const AnyString& id) const;

        // Get the Qt data model implementation for2 displaying the
        // workspace resources in a Qt widget (table widget)
        QAbstractTableModel* GetResourceModel()
        { return this; }

        // Get the number of all resources including
        // user defined and primitive resources.
        size_t GetNumResources() const
        { return mResources.size(); }
        size_t GetNumPrimitiveResources() const
        { return mResources.size() - mUserResourceCount; }
        size_t GetNumUserDefinedResources() const
        { return mUserResourceCount; }

        // Get resource at a specific index in the list of all resources.
        // This will get any resource regardless user defined or not.
        Resource& GetResource(const ModelIndex& index);
        Resource& GetResource(size_t index);
        // Get a user defined resource.
        Resource& GetUserDefinedResource(size_t index);
        // Get a primitive (built-in) resource.
        Resource& GetPrimitiveResource(size_t index);
        // Get a resource identified by name and type. The resource must exist.
        Resource& GetResourceByName(const QString& name, Resource::Type type);
        // Get a resource identified by id. The resource must exist.
        Resource& GetResourceById(const QString& id);
        // Find resource by id. Returns nullptr if it doesn't exist.
        Resource* FindResourceById(const AnyString& id);
        // Find resource by name and type. Returns nullptr if not found.
        Resource* FindResourceByName(const QString& name, Resource::Type type);
        // Get a resource identified by name and type.
        // The resource must exist.
        const Resource& GetResourceByName(const QString& name, Resource::Type type) const;
        // Find resource by id. Returns nullptr if it doesn't exist.
        const Resource* FindResourceById(const AnyString& id) const;
        // Find resource by name and type. Returns nullptr if not found.
        const Resource* FindResourceByName(const QString& name, Resource::Type type) const;
        // Get resource at a specific index in the list of all resources.
        // This will get any resource regardless user defined or not.
        const Resource& GetResource(const ModelIndex& index) const;
        // Get a user defined resource.
        const Resource& GetUserDefinedResource(size_t index) const;
        // Get a primitive (built-in) resource.
        const Resource& GetPrimitiveResource(size_t index) const;
        // Delete the resources identified by the selection list.
        // The list can contain multiple items and can be discontinuous
        // and unsorted. Afterwards it will be sorted (ascending) based
        // on the item row numbers.
        void DeleteResources(const ModelIndexList& list, std::vector<QString>* dead_files = nullptr);
        void DeleteResource(const AnyString& id, std::vector<QString>* dead_files = nullptr);

        // Create duplicate copies of the selected resources.
        void DuplicateResources(const ModelIndexList& list, QModelIndexList* result = nullptr);

        // Export the raw JSON of the the selected resources.
        bool ExportResourceJson(const ModelIndexList& list, const QString& filename) const;

        static
        bool ImportResourcesFromJson(const QString& filename, std::vector<std::unique_ptr<Resource>>& resources);

        // Import and create new resource based on a file.
        // Currently, supports creating material resources out of
        // images (.jpeg and .png) files
        void ImportFilesAsResource(const QStringList& files);

        // Perform periodic workspace tick for cleaning up resources
        // that are no longer used/referenced.
        void Tick();

        // == Workspace properties ==

        // Returns true if workspace has a property by the given name.
        bool HasProperty(const PropertyKey& key) const
        { return mProperties.contains(key); }
        // Delete the property from the workspace.
        void DeleteProperty(const PropertyKey& key)
        { mProperties.remove(key);  }
        // Set a property value. If the property exists already the previous
        // value is overwritten. Otherwise, it's added.
        void SetProperty(const PropertyKey& key, const PropertyValue& value)
        { SetVariantProperty(key, value); }

        // Return the value of the property identified by name.
        // If the property doesn't exist returns the default value.
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
        // Get the value of a property.
        // If the property exists returns true and stores the value in the T pointer
        // otherwise return false and T* is left unmodified.
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


        // User specific workspace properties.
        bool HasUserProperty(const PropertyKey& name) const
        { return mUserProperties.contains(name); }
        // Delete the user property from the workspace
        void DeleteUserProperty(const PropertyKey& key)
        { mUserProperties.remove(key); }
        // Set a property value. If the property exists already the previous
        // value is overwritten. Otherwise, it's added.
        void SetUserProperty(const PropertyKey& key, const PropertyValue& value)
        {
            SetUserVariantProperty(key, value);
            emit UserPropertyUpdated(key, value);
        }

        // Return the value of the property identified by name.
        // If the property doesn't exist returns the default value.
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
        // Get the value of a property.
        // If the property exists returns true and stores the value in the T pointer
        // otherwise return false and T* is left unmodified.
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

        const QVariantMap& GetProperties() const
        { return mProperties; }

        const QVariantMap& GetUserProperties() const
        { return mUserProperties; }

        using ProjectSettings = app::ProjectSettings;

        const ProjectSettings& GetProjectSettings() const
        { return mSettings; }
        ProjectSettings& GetProjectSettings()
        { return mSettings; }
        void SetProjectSettings(const ProjectSettings& settings)
        { mSettings = settings; }
        void SetProjectId(const QString& id)
        { mSettings.game_identifier = id; }

        struct ExportOptions {
            QString zip_file;
        };
        bool ExportResourceArchive(const std::vector<const Resource*>& resources, const ExportOptions& options);

        bool ImportResourceArchive(ZipArchive& zip);

        struct ContentPackingOptions {
            // the output directory into which place the packed content.
            QString directory;
            // the sub directory (package) name that will be created
            // in the output dir.
            QString package_name;
            // Whether to write the content.json file that has the workspace
            // content for the game.
            bool write_content_file = true;
            // Whether to write the config.json file that has the configuration
            // for launching the game.
            bool write_config_file = true;
            // Combine small textures into texture atlas files.
            bool combine_textures = true;
            // Resize large (oversized) textures to fit in the
            // specified min/maxtexture dimensions.
            bool resize_textures = true;
            // Max texture height. Textures larger than this will be downsized.
            unsigned max_texture_height = 1024;
            // Max texture width, Textures larger than this will be downsized.
            unsigned max_texture_width  = 1024;
            // The width of the texture pack(atlas) to use when
            // combine_textures is enabled.
            unsigned texture_pack_width = 1024;
            // The height of the texture pack(atlas) to use when
            // combine_textures is enabled.
            unsigned texture_pack_height  = 1024;
            // Padding pixels to use in order to create some extra "cushion" pixels
            // between adjacent packed textures in order to mitigate sampling
            // artifacts. If textures are packed next to each other the sampling
            // might sample pixels that belong to another texture depending
            // on the filtering setting on the sampler. the padding pixels
            // are filtered from the source texture.
            unsigned texture_padding = 0;
            // Copy/deploy the native game engine files (executables and libraries)
            bool copy_native_files = false;
            // Copy/deploy the html5/wasm game engine files (wasm and js)
            bool copy_html5_files = false;
            // Generate/write/copy the main game html5 file
            bool write_html5_game_file = false;
            // Generate/write game content file system image for html5
            bool write_html5_content_fs_image = false;
            // path to python executable. need to run emscripten file packaging script.
            QString python_executable;
            // path to the emscripten sdk.
            QString emsdk_path;
        };

        // Build the selected resources into a deployable release "package", that
        // can be redistributed to the end users.
        // This includes copying the resource files such as fonts, textures and shaders
        // and also building content packages such as texture atlas(ses).
        // The directory in which the output is to be placed will have its
        // previous contents OVERWRITTEN.
        // Returns true if everything went smoothly, otherwise false
        // and the package process might have failed in some way.
        // Errors/warnings encountered during the process will be logged.
        bool BuildReleasePackage(const std::vector<const Resource*>& resources, const ContentPackingOptions& options,
                                 WorkspaceAsyncWorkObserver* observer = nullptr);

        static void ClearAppGraphicsCache();

    public slots:
        // Save or update a resource in the workspace. If the resource by the same type
        // and name exists it's overwritten, otherwise a new resource is added to
        // the workspace.
        void UpdateResource(const Resource* resource);
        // Save or update a user property.
        void UpdateUserProperty(const QString& name, const QVariant& data);

    signals:
        // Emitted when user property is set.
        void UserPropertyUpdated(const QString& name, const QVariant& data);

        // this signal is emitted when a resource is loaded.
        void ResourceLoaded(const Resource* resource);

        // this signal is emitted *after* a new resource has been
        // added to the list of resources.
        void ResourceAdded(const Resource* resource);
        // this signal is emitted after an existing resource has been updated
        // i.e. saved. The resource object is the *new* resource object
        // that replaces a previous resource of the same type and name.
        void ResourceUpdated(const Resource* resource);

        // This signal is emitted after a resource object has been
        // removed from the list of resources but before it is deleted.
        // The pointer to the resource is provided for convenience for
        // accessing any wanted properties as long as you don't hold on
        // to the object. After your signal handler returns the pointer
        // is no longer valid!
        void ResourceRemoved(const Resource* resource);

    private:
        void ScanCppSources();
        bool LoadContent(const QString& file, ResourceMigrationLog* log, WorkspaceAsyncWorkObserver* observer);
        bool LoadProperties(const QString& file, WorkspaceAsyncWorkObserver* observer);
        void LoadUserSettings(const QString& file);
        bool SaveContent(const QString& file) const;
        bool SaveProperties(const QString& file) const;
        void SaveUserSettings(const QString& file) const;

        void SetVariantProperty(const PropertyKey& key, const QVariant& value)
        { mProperties[key] = value; }
        void SetUserVariantProperty(const PropertyKey& key, const QVariant& value)
        { mUserProperties[key] = value; }
        QVariant GetVariantProperty(const PropertyKey& key) const
        {
            if (mProperties.contains(key))
                return mProperties[key];
            return QVariant();
        }
        QVariant GetUserVariantProperty(const PropertyKey& key) const
        {
            if (mUserProperties.contains(key))
                return mUserProperties[key];
            return QVariant();
        }

    private:
        // this is the list of resources that we save/load
        // from the workspace data files. these are created
        // and edited by the user through the editor.
        std::vector<std::unique_ptr<Resource>> mResources;
        // Number of user defined resources. In the list of resources
        // the user defined resources come first and then the primitive
        // resources follow.
        std::size_t mUserResourceCount = 0;
        std::size_t mTickCount = 0;
    private:
        const QString mWorkspaceDir;
        // workspace specific properties
        QVariantMap mProperties;
        // user specific workspace properties.
        QVariantMap mUserProperties;
        // workspace/project settings.
        ProjectSettings mSettings;

    private:
        using GraphicsBufferCache = std::unordered_map<std::string,
            std::shared_ptr<const GraphicsBuffer>>;
        static GraphicsBufferCache mAppGraphicsBufferCache;
        static bool mEnableAppResourceCaching;

    };

    class WorkspaceProxy : public QSortFilterProxyModel
    {
    public:
        using Show = Resource::Type;
        WorkspaceProxy()
        {
            mBits.set_from_value(~0u);
        }
        void SetModel(const Workspace* workspace)
        { mWorkspace = workspace; }
        void SetVisible(Show what, bool yes_no)
        { mBits.set(what, yes_no); }
        bool IsShow(Show what) const
        { return mBits.test(what); }
        void SetShowBits(unsigned value)
        { mBits.set_from_value(value); }
        unsigned GetShowBits() const
        { return mBits.value(); }
        void SetFilterString(const QString& string)
        { mFilterString = string; }
        const QString& GetFilterString() const
        { return mFilterString; }

        void DebugPrint() const;
    protected:
        virtual bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
        virtual void sort(int column, Qt::SortOrder order) override;
        virtual bool lessThan(const QModelIndex& lhs, const QModelIndex& rhs) const override;
    private:
        base::bitflag<Show> mBits;
        QString mFilterString;
        const Workspace* mWorkspace = nullptr;
    };

} // namespace

