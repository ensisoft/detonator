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
#include "gamelib/gfxfactory.h"
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
                      public game::GfxFactory,
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
        { return static_cast<int>(mResources.size()); }
        virtual int columnCount(const QModelIndex&) const override
        { return 2; }

        // QAbstractFileEngineHandler implementation
        virtual QAbstractFileEngine* create(const QString& file) const override;

        std::shared_ptr<gfx::Material> MakeMaterial(const QString& name)
        { return MakeMaterial(ToUtf8(name)); }
        std::shared_ptr<gfx::Drawable> MakeDrawable(const QString& name)
        { return MakeDrawable(ToUtf8(name)); }

        // gfxfactory implementation
        virtual std::shared_ptr<gfx::Material> MakeMaterial(const std::string& name) const override;
        virtual std::shared_ptr<gfx::Drawable> MakeDrawable(const std::string& name) const override;

        // gfx::ResourceLoader implementation
        virtual std::string ResolveFile(gfx::ResourceLoader::ResourceType type, const std::string& file) const override;

        // Try to load the content of the workspace from the files in the given
        // directory.
        bool Load(const QString& dir);

        // Add a file to the workspace and return a path to the file encoded in
        // "workspace file" notation.
        QString AddFileToWorkspace(const QString& file);

        // convenience wrapper
        std::string AddFileToWorkspace(const std::string& file)
        {
            return ToUtf8(AddFileToWorkspace(FromUtf8(file)));
        }

        // Map a "workspace file" to a file path in the file system.
        QString MapFileToFilesystem(const QString& file) const;

        // Open new workspace in the given directory.
        bool OpenNew(const QString& dir);

        // Try to save the contents of the workspace in the previously opened
        // directory.
        bool Save();

        // Get human readable name for the workspace.
        QString GetName() const;
        // Get current workspace directory
        QString GetDir() const
        { return mWorkspaceDir; }

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

        // Save a new resource in the workspace. If the resource by the same type
        // and name exists it's overwritten, otherwise a new resource is added to
        // the workspace.
        template<typename T>
        void SaveResource(const GameResource<T>& resource)
        {
            const auto& name = resource.GetName();
            const auto  type = resource.GetType();
            for (size_t i=0; i<mResources.size(); ++i)
            {
                auto& res = mResources[i];
                if (res->GetName() != name || res->GetType() != type)
                    continue;

                // overwrite the existing resource type. the caller
                // is expected to have confirmed with the user that this is OK
                // note that we must make sure to not only update the
                // contained resource parameters but also the application level
                // resource object and since that is a polymorphic type
                // we must allocate new resource object.
                mResources[i] = std::make_unique<GameResource<T>>(resource);

                // see if there are instances of this resource in use that we
                // also want to update with the latest changes.
                // note that there can be multiple handles here!
                // here we're only interested in updating the gfx object content (parameters)
                const auto* source = resource.GetContent();

                for (auto& handle : mPrivateInstances)
                {
                    handle->UpdateInstance(name, type, (const void*)source);
                }
                return;
            }
            // if we're here no such resource exists yet.
            // Create a new resource and add it to the list of resources.
            beginInsertRows(QModelIndex(), mResources.size(), mResources.size());
            mResources.push_back(std::make_unique<GameResource<T>>(resource));
            endInsertRows();

            auto& back = mResources.back();
            emit NewResourceAvailable(back.get());
        }

        // Returns whether a material by this name already exists or not.
        // Only checks for the materials that the user has defined and
        // added to the workspace. Primitives are not included. For
        // validation use IsValidMaterial.
        bool HasMaterial(const QString& name) const;
        // Returns whether a drawable by this name already exists or not.
        // Only checks for the user defined drawables. Primitives are not included.
        // For validation use IsValidDrawable.
        bool HasDrawable(const QString& name) const;
        // Returns whether a particle system by this name already exists or not.
        bool HasParticleSystem(const QString& name) const;

        // Checks whether the material name is a valid material name.
        // Includes primitives and user defined materials.
        bool IsValidMaterial(const QString& name) const;
        // Checks whether the drawable name is a valid drawable naem.
        // Includes primitives and user defined drawable shapes.
        bool IsValidDrawable(const QString& name) const;

        // Returns whether some particular resource exists or not.
        // Only checks for a user defined resource not for primitives.
        bool HasResource(const QString& name, Resource::Type type) const;

        // Get the Qt data model implementation for displaying the
        // workspace resources in a Qt widget (table widget)
        QAbstractTableModel* GetResourceModel()
        { return this; }

        // Get the number of resources.
        size_t GetNumResources() const
        { return mResources.size(); }

        // Get resource at a specific index in the list of all resources.
        Resource& GetResource(size_t i)
        {
            ASSERT(i < mResources.size());
            return *mResources[i];
        }

        // Get resource at a specific index in the list of all resources.
        const Resource& GetResource(size_t i) const
        {
            ASSERT(i < mResources.size());
            return *mResources[i];
        }

        // Get a resource identified by name and type.
        // The resource must exist.
        Resource& GetResource(const QString& name, Resource::Type type);
        // Get a resource identified by name and type.
        // The resource must exist.
        const Resource& GetResource(const QString& name, Resource::Type type) const;

        // Delete the resources identified by the selection list.
        // The list can contain multiple items and can be discontinuous
        // and unsorted. Afterwards it will be sorted (ascending) based
        // on the item row numbers.
        void DeleteResources(QModelIndexList& list);

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
        { mUserProperties[name] = value; }
        // Return the value of the property identied by name.
        // If the property doesn't exist returns default value.
        QVariant GetUserProperty(const QString& name, const QVariant& def) const
        {
            QVariant ret = mUserProperties[name];
            if (ret.isNull())
                return def;
            return ret;
        }
        // Return the value of the property identified by name.
        // If the property doesn't exist returns a null variant.
        QVariant GetUserProperty(const QString& name) const
        { return mUserProperties[name]; }

        // Return the value of the property identified by name.
        // If the property doesn't exist returns the default value.
        template<typename T>
        T GetUserProperty(const QString& name, const T& def) const
        {
            if (!HasUserProperty(name))
                return def;
            const auto& ret = GetUserProperty(name);
            return qvariant_cast<T>(ret);
        }
        // Get the value of a property.
        // If the property exists returns true and stores the value in the T pointer
        // otherwise return false and T* is left unmodified.
        template<typename T>
        bool GetUserProperty(const QString& name, T* out) const
        {
            if (!HasUserProperty(name))
                return false;
            const auto& ret = GetUserProperty(name);
            *out = qvariant_cast<T>(ret);
            return true;
        }

        // Project settings.
        struct ProjectSettings {
            unsigned multisample_sample_count = 0;
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
            // the assumed window width when lauching the application
            // with its own window.
            unsigned window_width = 1024;
            // the assumed window height  when lauching the application
            // with its own window.
            unsigned window_height = 768;
            // window flag to set fullscreen on start.
            bool window_set_fullscreen = false;
            // window flag to allow window to be resized.
            bool window_can_resize = true;
            // window flag to control window border
            bool window_has_border = true;
            // how many times the app ticks per second.
            unsigned ticks_per_second = 1;
            // how many times the app updates per second.
            unsigned updates_per_second = 60;
            // Working folder when playing the app in the editor.
            QString working_folder = "${workspace}";
            // Arguments for when playing the app in the editor.
            QString command_line_arguments;
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

    signals:
        // this signal is emitted *after* a new resource has been
        // added to the list of resources.
        void NewResourceAvailable(const Resource* resource);

        // This signal is emitted after a resource object has been
        // removed from the list of resources but before it is's
        // deleted. The pointer to the resource is provided for
        // convenience for accessing any wanted properties as
        // long as you don't hold on to the object. After your
        // signal handler returns the pointer is no longer valid!
        void ResourceToBeDeleted(const Resource* resource);

    private:
        bool LoadContent(const QString& file);
        bool LoadWorkspace(const QString& file);
        void LoadUserSettings(const QString& file);
        bool SaveContent(const QString& file) const;
        bool SaveWorkspace(const QString& file) const;
        void SaveUserSettings(const QString& file) const;

    private:
        // this is the list of resources that we save/load
        // from the workspace data files. these are created
        // and edited by the user through the editor.
        std::vector<std::unique_ptr<Resource>> mResources;
        // we keep track of private instances of resource objects
        // through non-owning pointer.
        // such instances came into life when resources of other
        // types use other resources, for example when an animation
        // uses a material but the reason why we track them here
        // is that we can reflect the changes done to the resources
        // in the system that uses it!
        mutable std::vector<std::unique_ptr<ResourceHandle>> mPrivateInstances;
    private:
        QString mWorkspaceDir;
        // workspace specific properties
        QVariantMap mProperties;
        // user specific workspace properties.
        QVariantMap mUserProperties;
        // workspace/project settings.
        ProjectSettings mSettings;
    };

} // namespace
