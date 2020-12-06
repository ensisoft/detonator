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
#  include <QAbstractTableModel>
#  include <private/qabstractfileengine_p.h> // private (deprecated) in Qt5
#  include <QString>
#  include <QMap>
#  include <QObject>
#  include <QVariant>
#include "warnpop.h"

#include <memory>
#include <vector>

#include "base/assert.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/resource.h"
#include "graphics/device.h"
#include "gamelib/classlib.h"
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
                      public game::ClassLibrary,
                      public gfx::ResourceLoader
    {
        Q_OBJECT

    public:
        Workspace();
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

        std::shared_ptr<gfx::Material> MakeMaterialByName(const QString& name) const;
        std::shared_ptr<gfx::Drawable> MakeDrawableByName(const QString& name) const;
        std::shared_ptr<const gfx::MaterialClass> GetMaterialClassByName(const QString& name) const;
        std::shared_ptr<const gfx::MaterialClass> GetMaterialClassByName(const char* name) const;
        std::shared_ptr<const gfx::DrawableClass> GetDrawableClassByName(const QString& name) const;
        std::shared_ptr<const gfx::DrawableClass> GetDrawableClassByName(const char* name) const;
        std::shared_ptr<const game::AnimationClass> GetAnimationClassByName(const QString& name) const;
        std::shared_ptr<const game::AnimationClass> GetAnimationClassById(const QString& id) const;

        // ClassLibrary implementation
        virtual std::shared_ptr<const gfx::MaterialClass> FindMaterialClass(const std::string& id) const override;
        virtual std::shared_ptr<const gfx::DrawableClass> FindDrawableClass(const std::string& id) const override;
        virtual std::shared_ptr<const game::AnimationClass> FindAnimationClassByName(const std::string& name) const override;
        virtual std::shared_ptr<const game::AnimationClass> FindAnimationClassById(const std::string& id) const override;
        virtual void LoadFromFile(const std::string&, const std::string&) override
        {}

        // gfx::ResourceLoader implementation
        virtual std::string ResolveURI(gfx::ResourceLoader::ResourceType type, const std::string& URI) const override;

        // Try to load the content of the workspace from the files in the given
        // directory. Returns true on success. Any errors are logged.
        bool LoadWorkspace(const QString& dir);
        // Make a new workspace in the given directory and then proceed as if
        // this workspace had been loaded from that directory. This is the
        // alternative to Load in terms of opening a workspace.
        // Returns true on success. Any errors are logged.
        bool MakeWorkspace(const QString& dir);
        // Try to save the contents of the workspace in the previously opened
        // directory.
        bool SaveWorkspace();
        // Close the current workspace.
        void CloseWorkspace();
        // Returns true if the workspace is currently open.
        bool IsOpen() const
        { return mIsOpen; }

        // Add a file to the workspace and return a path to the file encoded in
        // "workspace file" notation.
        QString AddFileToWorkspace(const QString& file);
        std::string AddFileToWorkspace(const std::string& file);

        // Map a "workspace file" to a file path in the file system.
        QString MapFileToFilesystem(const QString& file) const;

        // Save a new resource in the workspace. If the resource by the same type
        // and name exists it's overwritten, otherwise a new resource is added to
        // the workspace.
        void SaveResource(const Resource& resource);

        // Get human readable name for the workspace.
        QString GetName() const;
        // Get current workspace directory
        QString GetDir() const
        { return mWorkspaceDir; }

        // Get a list of user defined material names in the workspace.
        QStringList ListUserDefinedMaterials() const;
        // Get a list of all material names in the workspace
        // including the user defined materials and the "primitive" ones.
        QStringList ListAllMaterials() const;
        // Get a list of all primitive (built-in) materials.
        QStringList ListPrimitiveMaterials() const;
        // Get a list of all drawable names in the workspace
        // including the user defined drawables and the "primitive" ones.
        QStringList ListAllDrawables() const;
        // Get a list of primitive (build-in) drawables.
        QStringList ListPrimitiveDrawables() const;
        // Get a list of user defined drawables.
        QStringList ListUserDefinedDrawables() const;

        // Map material id to its human readable name.
        QString MapMaterialIdToName(const QString& id) const;
        QString MapMaterialIdToName(const std::string& id) const
        { return MapMaterialIdToName(FromUtf8(id)); }
        // Map drawable id to its human readable name.
        QString MapDrawableIdToName(const QString& id) const;
        QString MapDrawableIdToName(const std::string& id) const
        { return MapDrawableIdToName(FromUtf8(id)); }

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
        // Get a resource identified by name and type.
        // The resource must exist.
        Resource& GetResourceByName(const QString& name, Resource::Type type);
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
            // user defined name of the application.
            QString application_name;
            // User defined version of the application.
            QString application_version;
            // The library (.so or .dll) that contains the application
            // entry point and game::App implementation.
            QString application_library;
            // default texture minification filter.
            gfx::Device::MinFilter default_min_filter = gfx::Device::MinFilter::Nearest;
            // default texture magnification filter.
            gfx::Device::MagFilter default_mag_filter = gfx::Device::MagFilter::Nearest;
            // The starting window mode.
            WindowMode window_mode = WindowMode::Windowed;
            // the assumed window width when launching the application
            // with its own window, i.e. when window_mode is Windowed.
            unsigned window_width = 1024;
            // the assumed window height  when lauching the application
            // with its own window.
            unsigned window_height = 768;
            // window flag to allow window to be resized.
            bool window_can_resize = true;
            // window flag to control window border
            bool window_has_border = true;
            // vsync or not.
            bool window_vsync = false;
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
        };

        const ProjectSettings& GetProjectSettings() const
        { return mSettings; }
        ProjectSettings& GetProjectSettings()
        { return mSettings; }
        void SetProjectSettings(const ProjectSettings& settings)
        { mSettings = settings; }

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
        QString mWorkspaceDir;
        // workspace specific properties
        QVariantMap mProperties;
        // user specific workspace properties.
        QVariantMap mUserProperties;
        // workspace/project settings.
        ProjectSettings mSettings;
        bool mIsOpen = false;
    };

} // namespace
