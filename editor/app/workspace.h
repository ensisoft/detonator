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
#  include <quazip/quazip.h>
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

class QuaZip;

namespace app
{
    class ResourceArchive
    {
    public:
        ResourceArchive();

        bool Open(const QString& zip_file);

        bool ReadFile(const QString& file, QByteArray* array) const;

        void SetImportSubFolderName(const QString& name)
        { mSubFolderName = name; }
        void SetResourceNamePrefix(const QString& prefix)
        { mNamePrefix = prefix; }
        QString GetImportSubFolderName() const
        { return mSubFolderName; }
        QString GetResourceNamePrefix() const
        { return mNamePrefix; }

        size_t GetNumResources() const
        { return mResources.size(); }
        const Resource& GetResource(size_t index) const
        { return *base::SafeIndex(mResources, index); }
        void IgnoreResource(size_t index)
        { mIgnoreSet.insert(index); }
        bool IsIndexIgnored(size_t index) const
        { return base::Contains(mIgnoreSet, index); }
    private:
        bool FindZipFile(const QString& unix_style_name) const;
    private:
        friend class Workspace;
        QString mZipFile;
        QString mSubFolderName;
        QString mNamePrefix;
        QFile mFile;
        mutable QuaZip mZip;
        std::vector<std::unique_ptr<Resource>> mResources;
        std::set<size_t> mIgnoreSet;
    };

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
        virtual gfx::ResourceHandle LoadResource(const std::string& URI) override;
        // audio::Loader implementation
        virtual audio::SourceStreamHandle OpenAudioStream(const std::string& URI,
            AudioIOStrategy strategy, bool enable_file_caching) const override;
        // game::Loader implementation
        virtual game::TilemapDataHandle LoadTilemapData(const game::Loader::TilemapDataDesc& desc) const override;

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
        std::shared_ptr<const gfx::MaterialClass> GetMaterialClassByName(const AnyString& name) const;
        std::shared_ptr<const gfx::MaterialClass> GetMaterialClassById(const AnyString& id) const;
        std::shared_ptr<const gfx::DrawableClass> GetDrawableClassByName(const AnyString& name) const;
        std::shared_ptr<const gfx::DrawableClass> GetDrawableClassById(const AnyString& id) const;
        std::shared_ptr<const game::EntityClass> GetEntityClassByName(const AnyString& name) const;
        std::shared_ptr<const game::EntityClass> GetEntityClassById(const AnyString& id) const;
        std::shared_ptr<const game::TilemapClass> GetTilemapClassById(const AnyString& id) const;

        // Try to load the contents of the workspace from the current workspace dir.
        // Returns true on success. Any errors are logged.
        bool LoadWorkspace(MigrationLog* log = nullptr);
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

        ResourceList ListCursors() const;

        ResourceList ListDataFiles() const;

        // List user defined resource dependencies.
        ResourceList ListDependencies(const QModelIndexList& list) const;
        ResourceList ListDependencies(std::vector<size_t> indices) const;
        ResourceList ListDependencies(std::size_t index) const;

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
        void DeleteResource(const AnyString& id);

        // Create duplicate copies of the selected resources.
        void DuplicateResources(const QModelIndexList& list);
        void DuplicateResources(std::vector<size_t> indices);
        void DuplicateResource(size_t index);

        bool ExportResourceJson(const QModelIndexList& list, const QString& filename) const;
        bool ExportResourceJson(const std::vector<size_t>& indices, const QString& filename) const;

        static
        bool ImportResourcesFromJson(const QString& filename, std::vector<std::unique_ptr<Resource>>& resources);

        // Import and create new resource based on a file.
        // Currently supports creating material resources out of
        // images (.jpeg and .png) files
        void ImportFilesAsResource(const QStringList& files);

        // Perform periodic workspace tick for cleaning up resources
        // that are no longer used/referenced.
        void Tick();

        // == Workspace properties ==

        // Returns true if workspace has a property by the given name.
        bool HasProperty(const PropertyKey& key) const
        { return mProperties.contains(key); }
        // Set a property value. If the property exists already the previous
        // value is overwritten. Otherwise, it's added.
        void SetProperty(const PropertyKey& key, const QVariant& value)
        { mProperties[key] = value; }
        void SetProperty(const PropertyKey& key, const QString& value)
        { mProperties[key] = value; }
        void SetProperty(const PropertyKey& key, const std::string& value)
        { mProperties[key] = FromUtf8(value); }
        void SetProperty(const PropertyKey& key, const AnyString& value)
        { mProperties[key] = value.GetWide(); }
        // Return the value of the property identified by name.
        // If the property doesn't exist returns default value.
        QVariant GetProperty(const PropertyKey& name, const QVariant& def) const
        {
            QVariant ret = mProperties[name];
            if (ret.isNull())
                return def;
            return ret;
        }
        // Return the value of the property identified by name.
        // If the property doesn't exist returns a null variant.
        QVariant GetProperty(const PropertyKey& name) const
        { return mProperties[name]; }

        // Return the value of the property identified by name.
        // If the property doesn't exist returns the default value.
        template<typename T>
        T GetProperty(const PropertyKey& name, const T& def) const
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
        bool GetProperty(const PropertyKey& name, T* out) const
        {
            if (!HasProperty(name))
                return false;
            const auto& ret = GetProperty(name);
            *out = qvariant_cast<T>(ret);
            return true;
        }
        void DeleteProperty(const PropertyKey& key)
        { mProperties.remove(key);  }

        // User specific workspace properties.
        bool HasUserProperty(const PropertyKey& name) const
        { return mUserProperties.contains(name); }
        // Set a property value. If the property exists already the previous
        // value is overwritten. Otherwise, it's added.
        void SetUserProperty(const PropertyKey& name, const QVariant& value)
        {
            mUserProperties[name] = value;
            emit UserPropertyUpdated(name, value);
        }
        void SetUserProperty(const PropertyKey& name, const QByteArray& bytes)
        {
            // QByteArray goes into variant but the JSON serialization through
            // QJsonObject::fromVariantMap doesn't seem to work right.
            QString base64 = bytes.toBase64();
            mUserProperties[name] = base64;
            emit UserPropertyUpdated(name, base64);
        }
        // Return the value of the property identified by name.
        // If the property doesn't exist returns the default value.
        QByteArray GetUserProperty(const PropertyKey& name, const QByteArray& def) const
        {
            QVariant ret = mUserProperties[name];
            if (ret.isNull())
                return def;
            QString base64 = ret.toString();
            if (!base64.isEmpty())
                return QByteArray::fromBase64(base64.toLatin1());
            return {};
        }
        // Return the value of the property identified by name.
        // If the property doesn't exist returns the default value.
        template<typename T>
        T GetUserProperty(const PropertyKey& name, const T& def) const
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
        bool GetUserProperty(const PropertyKey& name, T* out) const
        {
            QVariant ret = mUserProperties[name];
            if (ret.isNull())
                return false;
            *out = qvariant_cast<T>(ret);
            return true;
        }
        bool GetUserProperty(const PropertyKey& name, QByteArray* out) const
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
        void DeleteUserProperty(const PropertyKey& key)
        { mUserProperties.remove(key); }

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
            // Debug font (if any) used by the engine to print debug messages.
            QString debug_font;
            bool debug_show_fps = false;
            bool debug_show_msg = false;
            bool debug_draw = false;
            bool debug_print_fps = false;
            // logging flags. may or may not be overridden by some UI/interface
            // when running/launching the game. For example the GameMain may provide
            // command line flags to override these settings.
            bool log_debug = false;
            bool log_warn  = true;
            bool log_info  = true;
            bool log_error = true;

            // How the HTML5 canvas is sized on the page.
            enum class CanvasMode {
                // Canvas has a fixed size
                Default,
                // Canvas is resized to match the available client area in the page.
                SoftFullScreen
            };
            CanvasMode canvas_mode = CanvasMode::Default;
            // WebGL power preference.
            enum class PowerPreference {
                // Request a default power preference setting
                Default,
                // Request a low power mode that prioritizes power saving and battery over render perf
                LowPower,
                // Request a high performance mode that prioritizes rendering perf
                // over battery life/power consumption
                HighPerf
            };
            PowerPreference webgl_power_preference = PowerPreference::HighPerf;
            // HTML5 WebGL canvas render target width.
            unsigned canvas_width = 1024;
            // HTML5 WebGL canvas render target height
            unsigned canvas_height= 768;
            // WebGL doesn't have a specific MSAA/AA setting, only a boolean flag
            bool webgl_antialias = true;
            // flag to control HTML5 developer UI
            bool html5_developer_ui = false;
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
            // Flag to indicate whether to save and restore the window
            // geometry between application runs.
            bool save_window_geometry = false;
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
            bool enable_physics = true;
            // number of velocity iterations per physics simulation step.
            unsigned num_velocity_iterations = 8;
            // number of position iterations per physics simulation step.
            unsigned num_position_iterations = 3;
            // gravity vector for physics simulation
            glm::vec2 physics_gravity = {0.0f, 10.0f};
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
            enum class MousePointerUnits {
                Pixels, Units
            };
            // what are the units for the mouse pointer size.
            MousePointerUnits mouse_pointer_units = MousePointerUnits::Pixels;
            // name of the game's main script
            QString game_script = "ws://lua/game.lua";
            // Audio PCM data type.
            audio::SampleType audio_sample_type = audio::SampleType::Float32;
            // Number of audio output channels. 1 = monoaural, 2 stereo.
            audio::Channels audio_channels = audio::Channels::Stereo;
            // Audio output sample rate.
            unsigned audio_sample_rate = 44100;
            // Expected approximate audio buffer size in milliseconds.
            unsigned audio_buffer_size = 20;
            // flag to control PCM caching to avoid duplicate decoding
            bool enable_audio_pcm_caching = false;
            using DefaultAudioIOStrategy = engine::FileResourceLoader::DefaultAudioIOStrategy;
            DefaultAudioIOStrategy desktop_audio_io_strategy = DefaultAudioIOStrategy::Automatic;
            DefaultAudioIOStrategy wasm_audio_io_strategy = DefaultAudioIOStrategy::Automatic;
            // Which script to run when previewing an entity.
            QString preview_entity_script = "app://scripts/preview/entity.lua";
            // Which script to run when previewing a scene.
            QString preview_scene_script = "app://scripts/preview/scene.lua";
        };

        const ProjectSettings& GetProjectSettings() const
        { return mSettings; }
        ProjectSettings& GetProjectSettings()
        { return mSettings; }
        void SetProjectSettings(const ProjectSettings& settings)
        { mSettings = settings; }
        void SetProjectId(const QString& id)
        { mSettings.application_identifier = id; }

        struct ExportOptions {
            QString zip_file;
        };
        bool ExportResourceArchive(const std::vector<const Resource*>& resources, const ExportOptions& options);

        bool ImportResourceArchive(ResourceArchive& zip);

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
        bool BuildReleasePackage(const std::vector<const Resource*>& resources, const ContentPackingOptions& options);

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

        // This signal is emitted intermittently during  the
        // execution of BuildReleasePackage.
        // Action is the name of the current action (human-readable)
        // and step is the number of the current step out of total
        // steps under this action.
        void ResourcePackingUpdate(const QString& action, int step, int total);

    private:
        bool LoadContent(const QString& file, MigrationLog* log);
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
        void SetFilterString(const QString& string)
        { mFilterString = string; }
        const QString& GetFilterString() const
        { return mFilterString; }
    protected:
        bool filterAcceptsRow(int row, const QModelIndex& parent) const override
        {
            if (!mWorkspace)
                return false;
            const auto& resource = mWorkspace->GetUserDefinedResource(row);
            if (!mBits.test(resource.GetType()))
                return false;
            if (mFilterString.isEmpty())
                return true;
            const auto& name = resource.GetName();
            return name.contains(mFilterString, Qt::CaseInsensitive);
        }
    private:
        base::bitflag<Show> mBits;
        QString mFilterString;
        const Workspace* mWorkspace = nullptr;
    };


} // namespace
