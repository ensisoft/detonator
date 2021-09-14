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

#include "base/assert.h"
#include "audio/loader.h"
#include "audio/format.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/resource.h"
#include "graphics/device.h"
#include "engine/classlib.h"
#include "engine/loader.h"
#include "resource.h"
#include "utility.h"

namespace app
{
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
                      public audio::Loader

    {
        Q_OBJECT

    public:
        Workspace(const QString& dir);
       ~Workspace();

        // QAbstractTableModel implementation
        virtual QVariant data(const QModelIndex& index, int role) const override;
        virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        virtual int rowCount(const QModelIndex&) const override
        { return static_cast<int>(mVisibleCount); }
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
        virtual engine::ClassHandle<const gfx::MaterialClass> FindMaterialClassById(const std::string& id) const override;
        virtual engine::ClassHandle<const gfx::DrawableClass> FindDrawableClassById(const std::string& id) const override;
        virtual engine::ClassHandle<const game::EntityClass> FindEntityClassByName(const std::string& name) const override;
        virtual engine::ClassHandle<const game::EntityClass> FindEntityClassById(const std::string& id) const override;
        virtual engine::ClassHandle<const game::SceneClass> FindSceneClassByName(const std::string& name) const override;
        virtual engine::ClassHandle<const game::SceneClass> FindSceneClassById(const std::string& id) const override;
        // game::GameDataLoader implementation.
        virtual engine::GameDataHandle LoadGameData(const std::string& URI) const override;
        virtual engine::GameDataHandle LoadGameDataFromFile(const std::string& filename) const override;
        // gfx::ResourceLoader implementation
        virtual gfx::ResourceHandle LoadResource(const std::string& URI) override;
        // audio::Loader implementation
        virtual std::ifstream OpenStream(const std::string& URI) const override;

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
        std::unique_ptr<gfx::Material> MakeMaterialByName(const QString& name) const;
        std::unique_ptr<gfx::Drawable> MakeDrawableByName(const QString& name) const;
        std::shared_ptr<const gfx::MaterialClass> GetMaterialClassByName(const QString& name) const;
        std::shared_ptr<const gfx::MaterialClass> GetMaterialClassByName(const char* name) const;
        std::shared_ptr<const gfx::MaterialClass> GetMaterialClassById(const QString& id) const;
        std::shared_ptr<const gfx::DrawableClass> GetDrawableClassByName(const QString& name) const;
        std::shared_ptr<const gfx::DrawableClass> GetDrawableClassByName(const char* name) const;
        std::shared_ptr<const gfx::DrawableClass> GetDrawableClassById(const QString& id) const;
        std::shared_ptr<const game::EntityClass> GetEntityClassByName(const QString& name) const;
        std::shared_ptr<const game::EntityClass> GetEntityClassById(const QString& id) const;

        // Try to load the contents of the workspace from the current workspace dir.
        // Returns true on success. Any errors are logged.
        bool LoadWorkspace();
        // Try to save the contents of the workspace into the current workspace dir.
        // Returns true on success. Any errors are logged.
        bool SaveWorkspace();

        // Map a file to the workspace and return a file URI
        QString MapFileToWorkspace(const QString& file) const;
        std::string MapFileToWorkspace(const std::string& file) const;

        // Map a "workspace file" to a file path in the file system.
        QString MapFileToFilesystem(const QString& uri) const;
        QString MapFileToFilesystem(const std::string& uri) const;

        // Save a resource in the workspace. If the resource by the same id
        // already exists it's overwritten, otherwise a new resource is added to
        // the workspace.
        void SaveResource(const Resource& resource);

        // Get human readable name for the workspace.
        QString GetName() const;
        // Get current workspace directory
        QString GetDir() const
        { return mWorkspaceDir; }

        using ResourceList = std::vector<ListItem>;

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

        ResourceList ListCursors() const;

        // Map material id to its human readable name.
        QString MapMaterialIdToName(const QString& id) const;
        QString MapMaterialIdToName(const std::string& id) const
        { return MapMaterialIdToName(FromUtf8(id)); }
        // Map drawable id to its human readable name.
        QString MapDrawableIdToName(const QString& id) const;
        QString MapDrawableIdToName(const std::string& id) const
        { return MapDrawableIdToName(FromUtf8(id)); }
        QString MapEntityIdToName(const QString& id) const;
        QString MapEntityIdToName(const std::string& id) const
        { return MapEntityIdToName(FromUtf8(id)); }

        QString MapResourceIdToName(const QString& id) const;
        QString MapResourceIdToName(const std::string& id) const
        { return MapResourceIdToName(FromUtf8(id)); }

        // Checks whether the material class id is a valid material class.
        // Includes primitives and user defined materials.
        bool IsValidMaterial(const QString& klass) const;
        bool IsValidMaterial(const std::string& klass) const
        { return IsValidMaterial(FromUtf8(klass)); }
        // Checks whether the drawable class id is a valid drawable class.
        // Includes primitives and user defined drawable shapes.
        bool IsValidDrawable(const QString& klass) const;
        bool IsValidDrawable(const std::string& klass) const
        { return IsValidDrawable(FromUtf8(klass)); }

        bool IsValidScript(const QString& id) const;
        bool IsValidScript(const std::string& id) const
        { return IsValidScript(FromUtf8(id)); }

        // Get the Qt data model implementation for2 displaying the
        // workspace resources in a Qt widget (table widget)
        QAbstractTableModel* GetResourceModel()
        { return this; }

        // Get the number of all resources including
        // user defined and primitive resources.
        size_t GetNumResources() const
        { return mResources.size(); }
        size_t GetNumPrimitiveResources() const
        { return mResources.size() - mVisibleCount; }
        size_t GetNumUserDefinedResources() const
        { return mVisibleCount; }

        // Get resource at a specific index in the list of all resources.
        Resource& GetResource(size_t i);
        // Get a user defined resource.
        Resource& GetUserDefinedResource(size_t index);
        // Get a user defined resource.
        Resource& GetPrimitiveResource(size_t index);
        // Get a resource identified by name and type. The resource must exist.
        Resource& GetResourceByName(const QString& name, Resource::Type type);
        // Get a resource identified by id. The resource must exist.
        Resource& GetResourceById(const QString& id);
        // Find resource by id. Returns nullptr if it doesn't exist.
        Resource* FindResourceById(const QString& id);
        // Find resource by name and type. Returns nullptr if not found.
        Resource* FindResourceByName(const QString& name, Resource::Type type);
        // Get a resource identified by name and type.
        // The resource must exist.
        const Resource& GetResourceByName(const QString& name, Resource::Type type) const;
        // Find resource by id. Returns nullptr if it doesn't exist.
        const Resource* FindResourceById(const QString& id) const;
        // Find resource by name and type. Returns nullptr if not found.
        const Resource* FindResourceByName(const QString& name, Resource::Type type) const;
        // Get resource at a specific index in the list of all resources.
        const Resource& GetResource(size_t i) const;
        const Resource& GetUserDefinedResource(size_t index) const;
        const Resource& GetPrimitiveResource(size_t index) const;
        // Delete the resources identified by the selection list.
        // The list can contain multiple items and can be discontinuous
        // and unsorted. Afterwards it will be sorted (ascending) based
        // on the item row numbers.
        void DeleteResources(const QModelIndexList& list);
        void DeleteResources(std::vector<size_t> indices);
        void DeleteResource(size_t index);
        // Create duplicate copies of the selected resources.
        void DuplicateResources(const QModelIndexList& list);
        void DuplicateResources(std::vector<size_t> indices);
        void DuplicateResource(size_t index);

        bool ExportResources(const QModelIndexList& list, const QString& filename) const;

        static
        bool ImportResources(const QString& filename, std::vector<std::unique_ptr<Resource>>& resources);

        // Import and create new resource based on a file.
        // Currently supports creating material resources out of
        // images (.jpeg and .png) files
        void ImportFilesAsResource(const QStringList& files);

        // Perform periodic workspace tick for cleaning up resources
        // that are no longer used/referenced.
        void Tick();

        // == Workspace properties ==

        // Returns true if workspace has a property by the given name.
        bool HasProperty(const QString& name) const
        { return mProperties.contains(name); }
        // Set a property value. If the property exists already the previous
        // value is overwritten. Otherwise it's added.
        void SetProperty(const QString& name, const QVariant& value)
        { mProperties[name] = value; }
        // Return the value of the property identied by name.
        // If the property doesn't exist returns default value.
        QVariant GetProperty(const QString& name, const QVariant& def) const
        {
            QVariant ret = mProperties[name];
            if (ret.isNull())
                return def;
            return ret;
        }
        // Return the value of the property identified by name.
        // If the property doesn't exist returns a null variant.
        QVariant GetProperty(const QString& name) const
        { return mProperties[name]; }

        // Return the value of the property identified by name.
        // If the property doesn't exist returns the default value.
        template<typename T>
        T GetProperty(const QString& name, const T& def) const
        {
            if (!HasProperty(name))
                return def;
            const auto& ret = GetProperty(name);
            return qvariant_cast<T>(ret);
        }
        // Get the value of a property.
        // If the property exists returns true and stores the value in the T pointer
        // otherwise return false and T* is left unmodified.
        template<typename T>
        bool GetProperty(const QString& name, T* out) const
        {
            if (!HasProperty(name))
                return false;
            const auto& ret = GetProperty(name);
            *out = qvariant_cast<T>(ret);
            return true;
        }

        // User specific workspace properties.
        bool HasUserProperty(const QString& name) const
        { return mUserProperties.contains(name); }
        // Set a property value. If the property exists already the previous
        // value is overwritten. Otherwise it's added.
        void SetUserProperty(const QString& name, const QVariant& value)
        {
            mUserProperties[name] = value;
            emit UserPropertyUpdated(name, value);
        }
        void SetUserProperty(const QString& name, const QByteArray& bytes)
        {
            // QByteArray goes into variant but the JSON serialization through
            // QJsonObject::fromVariantMap doesn't seem to work right.
            QString base64 = bytes.toBase64();
            mUserProperties[name] = base64;
            emit UserPropertyUpdated(name, base64);
        }
        // Return the value of the property identified by name.
        // If the property doesn't exist returns the default value.
        QByteArray GetUserProperty(const QString& name, const QByteArray& def) const
        {
            QVariant ret = mUserProperties[name];
            if (ret.isNull())
                return def;
            QString base64 = ret.toString();
            if (!base64.isEmpty())
                return QByteArray::fromBase64(base64.toLatin1());
            return QByteArray();
        }
        // Return the value of the property identified by name.
        // If the property doesn't exist returns the default value.
        template<typename T>
        T GetUserProperty(const QString& name, const T& def) const
        {
            QVariant ret = mUserProperties[name];
            if (ret.isNull())
                return def;
            return qvariant_cast<T>(ret);
        }
        // Get the value of a property.
        // If the property exists returns true and stores the value in the T pointer
        // otherwise return false and T* is left unmodified.
        template<typename T>
        bool GetUserProperty(const QString& name, T* out) const
        {
            QVariant ret = mUserProperties[name];
            if (ret.isNull())
                return false;
            *out = qvariant_cast<T>(ret);
            return true;
        }
        bool GetUserProperty(const QString& name, QByteArray* out) const
        {
            QVariant ret = mUserProperties[name];
            if (ret.isNull())
                return false;
            QString base64 = ret.toString();
            if (base64.isEmpty())
                return false;

            *out = QByteArray::fromBase64(base64.toLatin1());
            return true;
        }

        // Project settings.
        struct ProjectSettings {
            enum class WindowMode {
                // Use fullscreen rendering surface (it's still a window but
                // conceptually slightly different)
                // The size of the rendering surface will be determined
                // by the resolution of the display.
                Fullscreen,
                // Use a window of specific rendering surface size
                // border and resize settings.
                Windowed,
            };
            // sample count when using multi-sampled render targets.
            unsigned multisample_sample_count = 4;
            // Unique identifier for the project.
            QString application_identifier;
            // user defined name of the application.
            QString application_name;
            // User defined version of the application.
            QString application_version;
            // The library (.so or .dll) that contains the application
            // entry point and game::App implementation.
            QString application_library_lin = "app://libGameEngine.so";
            QString application_library_win = "app://GameEngine.dll";
            QString GetApplicationLibrary() const
            {
            #if defined(POSIX_OS)
                return application_library_lin;
            #else
                return application_library_win;
            #endif
            }
            void SetApplicationLibrary(QString library)
            {
            #if defined(POSIX_OS)
                application_library_lin = library;
            #else
                application_library_win = library;
            #endif
            }

            // default texture minification filter.
            gfx::Device::MinFilter default_min_filter = gfx::Device::MinFilter::Trilinear;
            // default texture magnification filter.
            gfx::Device::MagFilter default_mag_filter = gfx::Device::MagFilter::Linear;
            // The starting window mode.
            WindowMode window_mode = WindowMode::Windowed;
            // the assumed window width when launching the application
            // with its own window, i.e. when window_mode is Windowed.
            unsigned window_width = 1024;
            // the assumed window height  when launching the application
            // with its own window.
            unsigned window_height = 768;
            // window flag to allow window to be resized.
            bool window_can_resize = true;
            // window flag to control window border
            bool window_has_border = true;
            // vsync or not.
            bool window_vsync = false;
            // whether to use/show the native window system mouse cursor/pointer.
            bool window_cursor = true;
            // whether to use sRGB color space or not (not using sRGB implies linear)
            bool config_srgb = true;
            // flag to indicate whether the mouse should be
            // grabbed and confined within the window.
            bool grab_mouse = false;
            // how many times the app ticks per second.
            unsigned ticks_per_second = 1;
            // how many times the app updates per second.
            unsigned updates_per_second = 60;
            // Working folder when playing the app in the editor.
            QString working_folder = "${workspace}";
            // Arguments for when playing the app in the editor.
            QString command_line_arguments;
            // Use a separate game host process for playing the app.
            // Using a separate process will protect the editor
            // process from errors in the game app but it might
            // make debugging the game app more complicated.
            bool use_gamehost_process = true;
            // physics settings
            // number of velocity iterations per physics simulation step.
            unsigned num_velocity_iterations = 8;
            // number of position iterations per physics simulation step.
            unsigned num_position_iterations = 3;
            // gravity vector for physics simulation
            glm::vec2 gravity = {0.0f, 10.0f};
            // scaling factor for mapping game world to physics world and back
            glm::vec2 physics_scale = {100.0f, 100.0f};
            // game's logical viewport width. this is *not* the final
            // viewport (game decides that) but only for visualization
            // in the editor.
            unsigned viewport_width = 1024;
            // game's logical viewport height. this is *not* the final
            // viewport (game decides that) but only for visualization
            // in the editor.
            unsigned viewport_height = 768;
            // The default engine clear color.
            QColor clear_color = {50, 77, 100, 255};
            // Whether the game should render a custom mouse pointer or not.
            bool mouse_pointer_visible = true;
            // What is the drawable shape of the game's custom mouse pointer.
            QString mouse_pointer_drawable = "_arrow_cursor";
            // What is the material of the game's custom mouse pointer.
            QString mouse_pointer_material = "_silver";
            // Where is the custom mouse pointer hotspot. (Normalized)
            glm::vec2 mouse_pointer_hotspot = {0.0f, 0.0f};
            // What is the pixel size of the game's custom mouse pointer.
            glm::vec2 mouse_pointer_size = {20.0f, 20.0f};
            // name of the game's main script
            QString game_script = "game.lua";
            // Audio PCM data type.
            audio::SampleType audio_sample_type = audio::SampleType::Float32;
            // Number of audio output channels. 1 = monoaural, 2 stereo.
            audio::Channels audio_channels = audio::Channels::Stereo;
            // Audio output sample rate.
            unsigned audio_sample_rate = 44100;
            // Expected approximate audio buffer size in milliseconds.
            unsigned audio_buffer_size = 20;
        };

        const ProjectSettings& GetProjectSettings() const
        { return mSettings; }
        ProjectSettings& GetProjectSettings()
        { return mSettings; }
        void SetProjectSettings(const ProjectSettings& settings)
        { mSettings = settings; }
        void SetProjectId(const QString& id)
        { mSettings.application_identifier = id; }

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
            // Padding pixels to use in order to create some extra "cushion" pixels
            // between adjacent packed textures in order to mitigate sampling
            // artifacts. If textures are packed next to each other the sampling
            // might sample pixels that belong to another texture depending
            // on the filtering setting on the sampler. the padding pixels
            // are filtered from the source texture.
            unsigned texture_padding = 0;
        };

        // Pack the selected resources into a deployable "package".
        // This includes copying the resource files such as fonts, textures and shaders
        // and also building content packages such as texture atlas(ses).
        // The directory in which the output is to be placed will have it's
        // previous contents OVERWRITTEN.
        // Returns true if everything went smoothly, otherwise false
        // and the package process might have failed in some way.
        // Errors/warnings encountered during the process will be logged.
        bool PackContent(const std::vector<const Resource*>& resources, const ContentPackingOptions& options);

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

        // this signal is emitted *after* a new resource has been
        // added to the list of resources.
        void NewResourceAvailable(const Resource* resource);
        // this signal is emitted after an existing resource has been updated
        // i.e. saved. The resource object is the *new* resource object
        // that replaces a previous resource of the same type and name.
        void ResourceUpdated(const Resource* resource);

        // This signal is emitted after a resource object has been
        // removed from the list of resources but before it is's
        // deleted. The pointer to the resource is provided for
        // convenience for accessing any wanted properties as
        // long as you don't hold on to the object. After your
        // signal handler returns the pointer is no longer valid!
        void ResourceToBeDeleted(const Resource* resource);

        // This signal is emitted intermittedly during  the
        // execution of PackContent.
        // Action is the name of the current action (human readable)
        // and step is the number of the current step out of total
        // steps under this action.
        void ResourcePackingUpdate(const QString& action, int step, int total);

    private:
        bool LoadContent(const QString& file);
        bool LoadProperties(const QString& file);
        void LoadUserSettings(const QString& file);
        bool SaveContent(const QString& file) const;
        bool SaveProperties(const QString& file) const;
        void SaveUserSettings(const QString& file) const;

    private:
        // this is the list of resources that we save/load
        // from the workspace data files. these are created
        // and edited by the user through the editor.
        std::vector<std::unique_ptr<Resource>> mResources;
        std::size_t mVisibleCount = 0;
    private:
        const QString mWorkspaceDir;
        // workspace specific properties
        QVariantMap mProperties;
        // user specific workspace properties.
        QVariantMap mUserProperties;
        // workspace/project settings.
        ProjectSettings mSettings;
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
    protected:
        bool filterAcceptsRow(int row, const QModelIndex& parent) const override
        {
            if (!mWorkspace)
                return false;
            const auto& resource = mWorkspace->GetUserDefinedResource(row);
            if (!mBits.test(resource.GetType()))
                return false;
            return true;
        }
    private:
        base::bitflag<Show> mBits;
        const Workspace* mWorkspace = nullptr;
    };


} // namespace
