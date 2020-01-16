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

#define LOGTAG "workspace"

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#  include <QByteArray>
#  include <QFile>
#include "warnpop.h"

#include "eventlog.h"
#include "workspace.h"
#include "utility.h"

namespace app
{

Workspace::Workspace()
{
    DEBUG("Create workspace");
}

Workspace::~Workspace()
{
    DEBUG("Destroy workspace");
}

bool Workspace::LoadWorkspace(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }
    const auto& buff = file.readAll(); // QByteArray
    if (buff.isEmpty())
    {
        WARN("No workspace content found in file: '%1'", filename);
        return false;
    }

    const auto* beg = buff.data();
    const auto* end = buff.data() + buff.size();
    // todo: this can throw, use the nothrow and log error
    const auto& json = nlohmann::json::parse(beg, end);

    std::vector<gfx::Material> materials;

    if (json.contains("materials"))
    {
        for (const auto& json_mat : json["materials"].items())
        {
            bool success  = false;
            auto material = gfx::Material::FromJson(json_mat.value(), &success);
            if (!success)
            {
                ERROR("Failed to load material properties.");
                continue;
            }
            DEBUG("Loaded material: '%1'", material.GetName());
            materials.push_back(std::move(material));
        }
    }
    mMaterials = std::move(materials);
    mFilename = filename;
    return true;
}

bool Workspace::SaveWorkspace(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }

    nlohmann::json json;
    for (const auto& material : mMaterials)
    {
        json["materials"].push_back(material.ToJson());
    }

    if (json.is_null())
    {
        WARN("Workspace contains no actual data. Skipped saving.");
        return true;
    }

    const auto& str = json.dump(2);
    if (file.write(&str[0], str.size()) == -1)
    {
        ERROR("File write failed. '%1'", filename);
        return false;
    }
    file.flush();
    file.close();
    mFilename = filename;
    return true;
}

QStringList Workspace::ListMaterials() const
{
    QStringList list;
    for (const auto& mat : mMaterials)
    {
        list.append(app::fromUtf8(mat.GetName()));
    }
    return list;
}

QStringList Workspace::ListDrawables() const
{
    QStringList list;
    return list;
}

void Workspace::SaveMaterial(const gfx::Material& material)
{
    for (auto& mat : mMaterials)
    {
        if (mat.GetName() == material.GetName())
        {
            mat = material;
            return;
        }
    }
    mMaterials.push_back(material);
}

bool Workspace::HasMaterial(const QString& name) const
{
    for (const auto& mat : mMaterials)
    {
        if (app::fromUtf8(mat.GetName()) == name)
            return true;
    }
    return false;
}

} // namespace
