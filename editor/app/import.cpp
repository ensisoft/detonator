// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#define LOGTAG "app"

#include "config.h"

#include "warnpush.h"
#  include <QFileInfo>
#include "warnpop.h"

#include <limits>
#include <algorithm>

#include "base/utility.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/geometry.h"
#include "editor/app/import.h"
#include "editor/app/eventlog.h"

#include "base/snafu.h"

namespace {
    static app::AnyString GetAssimpTexture(aiTextureType type, const aiMaterial* mat)
    {
        aiString str;
        mat->GetTexture(type, 0, &str);
        return str.C_Str();
    }
    static app::AnyString GetAssimpString(const char* key, int foo, int bar, const aiMaterial* mat)
    {
        aiString str;
        mat->Get(key, foo, bar, str);
        return str.C_Str();
    }
    static gfx::Color4f GetAssimpColor(const char* key, int foo, int bar, const aiMaterial* mat)
    {
        aiColor3D col;
        mat->Get(key, foo, bar, col);
        return {col.r, col.g, col.b, 1.0};
    }
} // namespace

namespace app
{
class ModelImporter::DrawableModel : public QAbstractTableModel
{
public:
    DrawableModel(State& state)
      : mState(state)
    {}

    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        if (role == Qt::DisplayRole)
        {
            const auto& item = mState.drawables[index.row()];
            if (index.column() == 0) return item.name;
            else if (index.column() == 1) return item.vertices;
            else if (index.column() == 2) return item.material;
        }
        return {};
    }
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            if (section == 0) return "Name";
            else if (section == 1) return "Vertices";
            else if (section == 2) return "Material";
        }
        return {};
    }
    virtual int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mState.drawables.size());
    }
    virtual int columnCount(const QModelIndex&) const override
    {
        return 3;
    }

    void Reset(std::vector<DrawableInfo>&& info)
    {
        beginResetModel();
        mState.drawables = std::move(info);
        endResetModel();
    }
private:
    State& mState;
};


class ModelImporter::MaterialModel : public QAbstractTableModel
{
public:
    MaterialModel(State& state)
      : mState(state)
    {}

    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        if (role == Qt::DisplayRole)
        {
            const auto& item = mState.materials[index.row()];
            if (index.column() == 0) return item.key;
        }
        return {};
    }
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            if (section == 0) return "Name";
        }
        return {};
    }
    virtual int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mState.materials.size());
    }
    virtual int columnCount(const QModelIndex&) const override
    {
        return 1;
    }

    void Reset(std::vector<MaterialInfo>&& info)
    {
        beginResetModel();
        mState.materials = std::move(info);
        endResetModel();
    }

private:
    State& mState;
};


ModelImporter::ModelImporter()
{
    mDrawableModel.reset(new DrawableModel(mState));
    mMaterialModel.reset(new MaterialModel(mState));
}

ModelImporter::~ModelImporter() = default;

QAbstractTableModel* ModelImporter::GetDrawableModel()
{
    return mDrawableModel.get();
}
QAbstractTableModel* ModelImporter::GetMaterialModel()
{
    return mMaterialModel.get();
}

const ModelImporter::MaterialInfo* ModelImporter::FindMaterial(const AnyString& key) const noexcept
{
    for (const auto& material : mState.materials)
    {
        if (material.key == key)
            return &material;
    }
    return nullptr;
}

bool ModelImporter::LoadModel(const app::AnyString& file)
{
    Assimp::Importer importer;

    const auto* scene = importer.ReadFile(file,
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType);
    if (scene == nullptr)
    {
        ERROR("Failed to import file. [file='%1', error='%2']", file, importer.GetErrorString());
        return false;
    }

    const auto& file_path = GetFilePath(file);

    std::vector<DrawableInfo> drawable_infos;
    std::vector<MaterialInfo> material_infos;


    // import each material by converting into our "native" format
    for (size_t i=0; i<scene->mNumMaterials; ++i)
    {
        const aiMaterial* mat   = scene->mMaterials[i];
        const auto name         = GetAssimpString(AI_MATKEY_NAME, mat);
        const auto diffuse_map  = GetAssimpTexture(aiTextureType_DIFFUSE, mat);
        const auto ambient_map  = GetAssimpTexture(aiTextureType_AMBIENT, mat);
        const auto specular_map = GetAssimpTexture(aiTextureType_SPECULAR, mat);

        const auto& diffuse_color  = GetAssimpColor(AI_MATKEY_COLOR_DIFFUSE, mat);
        const auto& ambient_color  = GetAssimpColor(AI_MATKEY_COLOR_AMBIENT, mat);
        const auto& specular_color = GetAssimpColor(AI_MATKEY_COLOR_SPECULAR, mat);

        DEBUG("Found material '%1'.", name);
        DEBUG(" Diffuse map: '%1'.", diffuse_map);
        DEBUG(" Ambient map: '%1'.", ambient_map);
        DEBUG(" Specular map: '%1'.", specular_map);

        auto material = std::make_shared<gfx::MaterialClass>(gfx::MaterialClass::Type::Texture);
        material->SetName(name);
        material->SetSurfaceType(gfx::MaterialClass::SurfaceType::Opaque);
        material->SetBaseColor(gfx::Color::White);

        if (!diffuse_map.IsEmpty())
        {
            const AnyString& file = JoinPath(file_path, diffuse_map);
            const AnyString& uri = toString("fs://%1", file);

            auto texture = std::make_unique<gfx::detail::TextureFileSource>();
            texture->SetColorSpace(gfx::TextureSource::ColorSpace::sRGB);
            texture->SetFileName(uri);
            texture->SetName(diffuse_map);

            auto map = std::make_unique<gfx::TextureMap>();
            map->SetType(gfx::TextureMap::Type::Texture2D);
            map->SetName("Diffuse");
            map->SetNumTextures(1);
            map->SetTextureSource(0, std::move(texture));

            material->SetNumTextureMaps(1);
            material->SetTextureMap(0, std::move(map));
        }

        MaterialInfo info;
        info.key   = name;
        info.klass = material;
        material_infos.push_back(info);
    }


    float scale_factor = 1.0f;

    gfx::VertexBuffer vertex_buffer;
    vertex_buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::ModelVertex3D>());

    gfx::IndexBuffer index_buffer;
    index_buffer.SetType(gfx::Geometry::IndexType::Index32);

    gfx::CommandBuffer command_buffer;

    glm::vec3 min_values = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    glm::vec3 max_values = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };

    for (size_t i=0; i<scene->mNumMeshes; ++i)
    {
        const auto* drawable = scene->mMeshes[i];
        const auto* material = scene->mMaterials[drawable->mMaterialIndex];

        if (!drawable->HasPositions())
        {
            WARN("Skipping a sub-mesh without positional information.");
            continue;
        }

        const auto vertex_buffer_offset = vertex_buffer.GetCount();
        const auto index_buffer_offset  = index_buffer.GetCount();
        for (size_t j=0; j<drawable->mNumVertices; ++j)
        {
            gfx::ModelVertex3D vertex;
            vertex.aPosition.x = drawable->mVertices[j].x * scale_factor;
            vertex.aPosition.y = drawable->mVertices[j].y * scale_factor;
            vertex.aPosition.z = drawable->mVertices[j].z * scale_factor;

            min_values.x = std::min(min_values.x, vertex.aPosition.x);
            min_values.y = std::min(min_values.y, vertex.aPosition.y);
            min_values.z = std::min(min_values.z, vertex.aPosition.z);

            max_values.x = std::max(max_values.x, vertex.aPosition.x);
            max_values.y = std::max(max_values.y, vertex.aPosition.y);
            max_values.z = std::max(max_values.z, vertex.aPosition.z);

            if (drawable->HasNormals())
            {
                vertex.aNormal.x   = drawable->mNormals[j].x;
                vertex.aNormal.y   = drawable->mNormals[j].y;
                vertex.aNormal.z   = drawable->mNormals[j].z;
            }
            if (drawable->HasTextureCoords(0))
            {
                vertex.aTexCoord.x = drawable->mTextureCoords[0][j].x;
                vertex.aTexCoord.y = 1.0 - drawable->mTextureCoords[0][j].y;
            }
            if (drawable->HasTangentsAndBitangents())
            {
                vertex.aTangent.x = drawable->mTangents[j].x;
                vertex.aTangent.y = drawable->mTangents[j].y;
                vertex.aTangent.z = drawable->mTangents[j].z;
                vertex.aBitangent.x = drawable->mBitangents[j].x;
                vertex.aBitangent.y = drawable->mBitangents[j].y;
                vertex.aBitangent.z = drawable->mBitangents[j].z;
            }
            if (drawable->HasTextureCoords(1))
            {
                WARN("Model has sub-meshes with multiple textures. "
                     "Currently only one set of texture coordinates is supported.");
            }

            vertex_buffer.PushBack(&vertex);
        }

        // append the indices for all the faces that belong to this (sub-)mesh
        for (size_t j=0; j<drawable->mNumFaces; ++j)
        {
            const auto* face = &drawable->mFaces[j];
            if (face->mNumIndices != 3)
            {
                WARN("Found a non-triangular face. Ignored.");
                continue;
            }

            using i32 = gfx::Index32;
            index_buffer.PushBack((i32)vertex_buffer_offset + face->mIndices[0]);
            index_buffer.PushBack((i32)vertex_buffer_offset + face->mIndices[1]);
            index_buffer.PushBack((i32)vertex_buffer_offset + face->mIndices[2]);
        }

        // draw command to draw the vertices of the submesh.
        gfx::Geometry::DrawCommand cmd;
        cmd.type   = gfx::Geometry::DrawType::Triangles;
        cmd.offset = index_buffer_offset;
        cmd.count  = index_buffer.GetCount() - index_buffer_offset;
        command_buffer.PushBack(cmd);

        aiString drawableName;
        aiString materialName;
        drawableName = drawable->mName;
        material->Get(AI_MATKEY_NAME, materialName);

        {
            DrawableInfo info;
            info.name     = drawableName.C_Str();
            info.material = materialName.C_Str();
            info.vertices = drawable->mNumVertices;
            // this refers to the sequence of draw commands that are needed
            // to draw this sub-mesh. currently, we only have 1 such command.
            info.draw_cmd_start = command_buffer.GetCount() - 1;
            info.draw_cmd_count = 1;

            drawable_infos.push_back(std::move(info));
        }
        DEBUG("Found sub-mesh '%1' with material '%2'.", drawableName.C_Str(), materialName.C_Str());
    }

    auto mesh = std::make_shared<gfx::PolygonMeshClass>();
    mesh->SetVertexLayout(gfx::GetVertexLayout<gfx::ModelVertex3D>());
    mesh->SetVertexBuffer(std::move(vertex_buffer));
    mesh->SetIndexBuffer(std::move(index_buffer));
    mesh->SetCommandBuffer(std::move(command_buffer));
    mesh->SetMeshType(gfx::PolygonMeshClass::MeshType::Model3D);

    for (const auto& draw : drawable_infos)
    {
        // this draw command refers to a sub-sequence of draw commands
        // that define the whole polygon mesh. In other words the mesh
        // has all the data and all the draw commands and a sub-mesh
        // will only draw a sub-sequence of those draw commands.
        gfx::DrawableClass::DrawCmd cmd;
        cmd.draw_cmd_count = draw.draw_cmd_count;
        cmd.draw_cmd_start = draw.draw_cmd_start;
        mesh->SetSubMeshDrawCmd(app::ToUtf8(draw.name), cmd);
    }

    mMesh = std::move(mesh);

    mMinValues = min_values;
    mMaxValues = max_values;
    mMaterialModel->Reset(std::move(material_infos));
    mDrawableModel->Reset(std::move(drawable_infos));
    return true;
}

} // app
