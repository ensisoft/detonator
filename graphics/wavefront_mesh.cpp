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

#include <map>
#include <vector>

#include "base/assert.h"
#include "base/logging.h"
#include "base/wavefront.h"
#include "graphics/loader.h"
#include "graphics/utility.h"
#include "graphics/wavefront_mesh.h"

namespace
{

// out of the box support for simple obj format.
class ObjLoader
{
public:
    struct Mesh {
        std::string name;
        std::string material;
        std::size_t start;
        std::size_t count;
    };
    template<typename T>
    void Import(const T&)
    {
        // do nothing import function
    }

    void BeginGroup(const wavefront::GroupName& group)
    {}

    void BeginGroup(const wavefront::ObjectName& object)
    {
        BeginMesh(object.name);
    }

    void SetAttribute(const wavefront::MtlLib& material)
    {
        mMaterialLibrary = material.name;
    }

    void SetAttribute(const wavefront::UseMtl& use_material)
    {
        // material attribute should be associated with a mesh
        // if there's no group made yet we'll make a new one
        if (mMeshes.empty())
            BeginMesh("unknown_mesh");

        auto& current_mesh = mMeshes.back();
        current_mesh.material = use_material.name;
    }

    void Import(const wavefront::Normal& n)
    {
        mNormals.push_back(n);
    }
    void Import(const wavefront::Position& p)
    {
        mPositions.push_back(p);
    }
    void import(const wavefront::TexCoord& t)
    {
        mTexCoords.push_back(t);
    }

    void Import(const wavefront::Face& face)
    {
        if (face.vertices.size() != 3)
        {
            WARN("Non-triangular surface is not supported.");
            return;
        }

        // the spec doesn't say if grouping statements are mandatory or not
        // for the elements that follow. so if there's no g defined
        // we simply start a new mesh.
        if (mMeshes.empty())
            BeginMesh("unknown_mesh");

        mIndexBuffer.push_back(AssembleVertex(face.vertices[0]));
        mIndexBuffer.push_back(AssembleVertex(face.vertices[1]));
        mIndexBuffer.push_back(AssembleVertex(face.vertices[2]));

        auto& current_mesh = mMeshes.back();
        current_mesh.count += 3;
    }
    bool OnParseError(const std::string& line, std::size_t lineno)
    {
        ERROR("Wavefront (.obj) parse error. [line='%1']", line);
        return false;
    }

    bool OnUnknownIdentifier(const std::string& line, std::size_t lineno)
    {
        WARN("Wavefront (.obj) unknown identifier. [line='%1']", line);
        return true; // continue
    }

    const gfx::Vertex3D* GetVertexData() const
    { return mVertexBuffer.data(); }

    const gfx::Index16* GetIndexData() const
    { return mIndexBuffer.data(); }

    std::size_t GetVertexCount() const
    { return mVertexBuffer.size(); }

    std::size_t GetIndexCount() const
    { return mIndexBuffer.size(); }

    std::size_t GetMeshCount() const
    { return mMeshes.size(); }

    const Mesh& GetMesh(std::size_t i) const
    { return mMeshes[i]; }

    auto TransferVertexBuffer()
    {
        return std::move(mVertexBuffer);
    }
    auto TransferIndexBuffer()
    {
        return std::move(mIndexBuffer);
    }

private:
    gfx::Index16 AssembleVertex(const wavefront::Vertex& vertex)
    {
        if (vertex.nindex > 0xffff || vertex.pindex > 0xffff || vertex.tindex > 0xffff)
            throw std::runtime_error("vertex index exceeded");

        using u8 = std::uint64_t;

        // create vertex cache key combining the position/normal/texture index.
        // if we already have such a vertex we can just re-use the index
        // otherwise we must create a new unique vertex
        const auto key = u8(vertex.pindex) << 32 |
                         u8(vertex.nindex) << 16 |
                         u8(vertex.tindex);

        const auto it  = m_lookup.find(key);
        if (it != std::end(m_lookup))
            return it->second;

        gfx::Vertex3D vert;
        if (vertex.nindex)
        {
            ASSERT(vertex.nindex-1 < mNormals.size());
            const auto& n = mNormals[vertex.nindex-1];
            vert.aNormal = {n.x, n.y, n.z};
        }
        if (vertex.pindex)
        {
            ASSERT(vertex.pindex-1 < mPositions.size());
            const auto& p = mPositions[vertex.pindex-1];
            vert.aPosition = {p.x, p.y, p.z};
        }
        if (vertex.tindex)
        {
            ASSERT(vertex.tindex-1 < mTexCoords.size());
            const auto& t = mTexCoords[vertex.tindex-1];
            vert.aTexCoord = {t.x, t.y};
        }

        const auto Index16 = static_cast<gfx::Index16>(mVertexBuffer.size());
        mVertexBuffer.push_back(vert);

        m_lookup[key] = Index16;
        return Index16;
    }
    void BeginMesh(const std::string& name)
    {
        Mesh mesh;
        mesh.name  = name;
        mesh.start = mIndexBuffer.size();
        mesh.count = 0;
        mMeshes.push_back(std::move(mesh));
    }
private:
    std::vector<wavefront::Position>    mPositions;
    std::vector<wavefront::Normal>      mNormals;
    std::vector<wavefront::TexCoord>    mTexCoords;
    std::map<std::uint64_t, gfx::Index16> m_lookup;
private:
    std::vector<gfx::Vertex3D> mVertexBuffer;
    std::vector<gfx::Index16>  mIndexBuffer;
private:
    std::vector<Mesh> mMeshes;
    std::string mMaterialLibrary;
};

} // namespace

namespace gfx
{

bool WavefrontMesh::ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& state) const
{
    unsigned flags = 0;
    if (env.flip_uv_horizontally)
        flags |= static_cast<unsigned>(DrawableFlags::Flip_UV_Horizontally);
    if (env.flip_uv_vertically)
        flags |= static_cast<unsigned>(DrawableFlags::Flip_UV_Vertically);

    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = (*env.proj_matrix);
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
    program.SetUniform("kDrawableFlags", flags);
    return true;
}
bool WavefrontMesh::Construct(const Environment& env, Device& device, Geometry::CreateArgs& geometry) const
{
    if (mFileUri.empty())
        return false;

    Loader::ResourceDesc desc;
    desc.type = Loader::Type::Mesh;
    desc.uri  = mFileUri;
    const auto& data_buffer = LoadResource(desc);
    if (!data_buffer)
    {
        ERROR("Failed to load Wavefront (.obj) mesh. [uri='%1']", mFileUri);
        return false;
    }

    ObjLoader loader;
    const char* beg = static_cast<const char*>(data_buffer->GetData());
    const char* end = static_cast<const char*>(data_buffer->GetData()) + data_buffer->GetByteSize();
    if (!wavefront::ParseObj(beg, end, loader))
    {
        ERROR("Failed to parse Wavefront (.obj) file. [uri='%1']", mFileUri);
        return false;
    }

    auto vertex_buffer = loader.TransferVertexBuffer();
    auto index_buffer  = loader.TransferIndexBuffer();
    DEBUG("Loaded Wavefront (.obj) mesh. [uri='%1', vertices=%2, indices=%3]",
            mFileUri, vertex_buffer.size(), index_buffer.size());

    GeometryBuffer buffer;
    buffer.SetVertexLayout(GetVertexLayout<Vertex3D>());
    buffer.SetVertexBuffer(std::move(vertex_buffer));
    buffer.SetIndexBuffer(std::move(index_buffer));
    buffer.AddDrawCmd(DrawType::Triangles);

    geometry.buffer = std::move(buffer);
    geometry.usage = Geometry::Usage::Static;
    geometry.content_name = mFileUri;
    geometry.content_hash = 0; // Hmm?? todo?
    return true;
}
ShaderSource WavefrontMesh::GetShader(const Environment& env, const Device& device) const
{
    return MakeSimple3DVertexShader(device, env.use_instancing);
}
std::string WavefrontMesh::GetShaderId(const Environment& env) const
{
    if (env.use_instancing)
        return "Vertex3DShaderInstanced";

    return "Vertex3DShader";
}
std::string WavefrontMesh::GetShaderName(const Environment& env) const
{
    if (env.use_instancing)
        return "Instanced Vertex3D Shader";
    return "Vertex3D Shader";
}
std::string WavefrontMesh::GetGeometryId(const Environment& env) const
{
    return mFileUri;
}

DrawPrimitive WavefrontMesh::GetDrawPrimitive() const
{
    return DrawPrimitive::Triangles;
}

Drawable::Type WavefrontMesh::GetType() const
{
    return Type::Other;
}

} // namespace