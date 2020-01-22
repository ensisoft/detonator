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

#include "config.h"

#include "warnpush.h"
#  include <QAbstractTableModel>
#  include <QString>
#  include <QMap>
#include "warnpop.h"

#include <memory>
#include <vector>

#include "base/assert.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "resource.h"
#include "utility.h"

namespace app
{
    class Workspace : public QAbstractTableModel
    {
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

        // Load the contents of the workspace from the given JSON file.
        bool LoadContent(const QString& file);
        // Load workspace data from the workspace JSON file.
        bool LoadWorkspace(const QString& file);

        // Save the contents of the workspace in the given JSON file.
        bool SaveContent(const QString& file) const;
        // Save the workspace data to the workspace JSON file.
        bool SaveWorkspace(const QString& file) const;

        // Get a list of material names in the workspace.
        QStringList ListMaterials() const;

        // Get a list of drawable names in the workspace.
        QStringList ListDrawables() const;

        // Set material properties for the material by the given name.
        // If no such material is yet set in the workspace it will be added.
        void SaveMaterial(const MaterialResource& material);

        void SaveParticleSystem(const ParticleSystemResource& resource);

        // Returns whether a material by this name already exists or not.
        bool HasMaterial(const QString& name) const;
        // Returns whether a particle system by this name already exists or not.
        bool HasParticleSystem(const QString& name) const;

        // Returns whether some particular resource exists or not.
        bool HasResource(const QString& name, Resource::Type type) const;

        QAbstractTableModel* GetResourceModel()
        { return this; }

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

        void DeleteResources(QModelIndexList& list);

    private:
        std::vector<std::unique_ptr<Resource>> mResources;
    private:
        mutable QString mContentFile;
        mutable QString mWorkspaceFile;
    };

} // namespace
