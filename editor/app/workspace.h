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
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>

#include "graphics/drawable.h"
#include "graphics/material.h"
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
        bool LoadWorkspace(const QString& file);

        // Save the contents of the workspace in the given JSON file.
        bool SaveWorkspace(const QString& file);

        // Get a list of material names in the workspace.
        QStringList ListMaterials() const;

        // Get a list of drawable names in the workspace.
        QStringList ListDrawables() const;

        // Set material properties for the material by the given name.
        // If no such material is yet set in the workspace it will be added.
        void SaveMaterial(const gfx::Material& material);

        // Returns whether a material by this name already exists or not.
        bool HasMaterial(const QString& name) const;

        QString GetCurrentFilename() const
        { return mFilename; }

        QAbstractTableModel* GetResourceModel()
        { return this; }

    private:
        class Resource
        {
        public:
            enum class Type {
                Material,
                Drawable,
                Animation,
                Scene,
                AudioTrack
            };
            virtual ~Resource() = default;
            virtual QString GetName() const = 0;
            virtual Type GetType() const = 0;
            virtual void Serialize(nlohmann::json& json) const =0;
        private:
        };
        class MaterialResource : public Resource
        {
        public:
            MaterialResource(const gfx::Material& material)
              : mMaterial(material)
            {}
            MaterialResource(gfx::Material&& material)
              : mMaterial(std::move(material))
            {}
            virtual QString GetName() const override
            { return app::fromUtf8(mMaterial.GetName()); }
            virtual Type GetType() const override
            { return Type::Material; }
            virtual void Serialize(nlohmann::json& json) const override
            {
                  json["materials"].push_back(mMaterial.ToJson());
            }
        private:
            const gfx::Material mMaterial;
        };
    private:
        std::vector<std::unique_ptr<Resource>> mResources;
    private:
        QString mFilename;
    };

} // namespace
