// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include "base/assert.h"
#include "base/math.h"
#include "base/logging.h"
#include "data/reader.h"
#include "data/writer.h"
#include "data/json.h"
#include "base/json.h"
#include "graphics/polygon_mesh.h"
#include "graphics/drawcmd.h"
#include "graphics/loader.h"
#include "graphics/utility.h"
#include "graphics/shader_source.h"

namespace gfx
{

void PolygonMeshClass::SetIndexBuffer(IndexBuffer&& buffer) noexcept
 {
    if (!mData.has_value())
        mData = InlineData {};

    auto& data = mData.value();
    data.index_type = buffer.GetType();
    data.indices    = std::move(buffer).GetIndexBuffer();
}

void PolygonMeshClass::SetIndexBuffer(const IndexBuffer& buffer)
{
    if (!mData.has_value())
        mData = InlineData {};

    auto& data = mData.value();
    data.indices    = buffer.GetIndexBuffer();
    data.index_type = buffer.GetType();
}

void PolygonMeshClass::SetVertexLayout(VertexLayout layout) noexcept
{
    if (!mData.has_value())
        mData = InlineData {};

    auto& data = mData.value();
    data.layout = std::move(layout);
}

void PolygonMeshClass::SetCommandBuffer(const std::vector<Geometry::DrawCommand>& cmds)
{
    if (!mData.has_value())
        mData = InlineData {};

    auto& data = mData.value();
    data.cmds = cmds;
}

void PolygonMeshClass::SetCommandBuffer(std::vector<Geometry::DrawCommand>&& cmds) noexcept
{
    if (!mData.has_value())
        mData = InlineData {};

    auto& data = mData.value();
    data.cmds = std::move(cmds);
}

void PolygonMeshClass::SetCommandBuffer(CommandBuffer&& buffer) noexcept
{
    SetCommandBuffer(std::move(buffer).GetCommandBuffer());
}

void PolygonMeshClass::SetCommandBuffer(const CommandBuffer& buffer)
{
    SetCommandBuffer(buffer.GetCommandBuffer());
}

void PolygonMeshClass::SetVertexBuffer(VertexBuffer&& buffer) noexcept
{
    SetVertexLayout(buffer.GetLayout());
    SetVertexBuffer(std::move(buffer).GetVertexBuffer());
}

void PolygonMeshClass::SetVertexBuffer(std::vector<uint8_t>&& buffer) noexcept
{
    if (!mData.has_value())
        mData = InlineData {};

    auto& data = mData.value();
    data.vertices = std::move(buffer);
}

void PolygonMeshClass::SetVertexBuffer(const VertexBuffer& buffer)
{
    SetVertexBuffer(buffer.GetVertexBuffer());
    SetVertexLayout(buffer.GetLayout());
}

void PolygonMeshClass::SetVertexBuffer(const std::vector<uint8_t>& buffer)
{
    if (!mData.has_value())
        mData = InlineData {};

    auto& data = mData.value();
    data.vertices = buffer;
}

const VertexLayout* PolygonMeshClass::GetVertexLayout() const noexcept
{
    if (mData.has_value())
        return &mData.value().layout;

    return nullptr;
}

const void* PolygonMeshClass::GetVertexBufferPtr() const noexcept
{
    if (mData.has_value())
    {
        if (!mData.value().vertices.empty())
            return mData.value().vertices.data();
    }
    return nullptr;
}

size_t PolygonMeshClass::GetNumDrawCmds() const noexcept
{
    if (mData.has_value())
        return mData.value().cmds.size();

    return 0;
}

size_t PolygonMeshClass::GetVertexBufferSize() const noexcept
{
    if (mData.has_value())
        return mData.value().vertices.size();

    return 0;
}

const Geometry::DrawCommand* PolygonMeshClass::GetDrawCmd(size_t index) const noexcept
{
    if (mData.has_value())
    {
        const auto& draws = mData.value().cmds;
        return &draws[index];
    }
    return nullptr;
}

std::string PolygonMeshClass::GetGeometryId(const Environment& env) const
{
    return mId;
}

std::string PolygonMeshClass::GetShaderId(const Environment& env) const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mMesh);
    hash = base::hash_combine(hash, mShaderSrc);
    hash = base::hash_combine(hash, env.instanced_draw);
    return std::to_string(hash);
}

std::string PolygonMeshClass::GetShaderName(const Environment& env) const
{
    if (HasShaderSrc())
        return mName;

    return base::FormatString("%1,%2",
        env.instanced_draw ? "Instanced" : "", mMesh);
}

ShaderSource PolygonMeshClass::GetShader(const Environment& env, const Device& device) const
{
    ShaderSource src;

    const auto mesh = GetMeshType();
    if (mesh == MeshType::Simple2D)
        src = MakeSimple2DVertexShader(device, env.instanced_draw);
    else if (mesh == MeshType::Simple3D)
        src = MakeSimple3DVertexShader(device, env.instanced_draw);
    else if (mesh == MeshType::Model3D)
        src = MakeModel3DVertexShader(device, env.instanced_draw); // todo:
    else BUG("No such vertex shader");

    if (!mShaderSrc.empty())
    {
        src.ReplaceToken("CUSTOM_VERTEX_TRANSFORM", mShaderSrc);
        src.AddPreprocessorDefinition("CUSTOM_VERTEX_TRANSFORM");
        src.AddDebugInfo("Mesh Class Name", mName);
        src.AddDebugInfo("Mesh Class ID", mId);
        src.AddDebugInfo("Mesh Type", base::ToString(mMesh));
    }
    return src;
}

void PolygonMeshClass::SetSubMeshDrawCmd(const std::string& key, const DrawCmd& cmd)
{
    mSubMeshes[key] = cmd;
}

const PolygonMeshClass::DrawCmd* PolygonMeshClass::GetSubMeshDrawCmd(const std::string& key) const noexcept
{
    return base::SafeFind(mSubMeshes, key);
}

std::size_t PolygonMeshClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mStatic);
    hash = base::hash_combine(hash, mContentHash);
    hash = base::hash_combine(hash, mContentUri);
    hash = base::hash_combine(hash, mShaderSrc);
    hash = base::hash_combine(hash, mMesh);

    std::set<std::string> keys;
    for (const auto& it : mSubMeshes)
        keys.insert(it.first);

    for (const auto& key : keys)
    {
        const auto& cmd = mSubMeshes.find(key);
        hash = base::hash_combine(hash, key);
        hash = base::hash_combine(hash, cmd->second.draw_cmd_start);
        hash = base::hash_combine(hash, cmd->second.draw_cmd_count);
    }

    if (mData.has_value())
    {
        const auto& data = mData.value();
        hash = base::hash_combine(hash, data.layout.GetHash());
        hash = base::hash_combine(hash, data.vertices);

        // BE-AWARE, padding might make this non-deterministic!
        //hash = base::hash_combine(hash, data.cmds);

        for (const auto& cmd : data.cmds)
        {
            hash = base::hash_combine(hash, cmd.type);
            hash = base::hash_combine(hash, cmd.count);
            hash = base::hash_combine(hash, cmd.offset);
        }
    }
    return hash;
}

std::unique_ptr<DrawableClass> PolygonMeshClass::Clone() const
{
    auto ret = std::make_unique<PolygonMeshClass>(*this);
    ret->mId = base::RandomString(10);
    return ret;
}
std::unique_ptr<DrawableClass> PolygonMeshClass::Copy() const
{
    return std::make_unique<PolygonMeshClass>(*this);
}

void PolygonMeshClass::IntoJson(data::Writer& writer) const
{
    writer.Write("id",     mId);
    writer.Write("name",   mName);
    writer.Write("static", mStatic);
    writer.Write("uri",    mContentUri);
    writer.Write("src",    mShaderSrc);
    writer.Write("mesh",   mMesh);

    if (mData.has_value())
    {
        auto inline_chunk = writer.NewWriteChunk();

        const auto& data = mData.value();
        data.layout.IntoJson(*inline_chunk);

        const VertexStream vertex_stream(data.layout, data.vertices);
        vertex_stream.IntoJson(*inline_chunk);

        const CommandStream command_stream(data.cmds);
        command_stream.IntoJson(*inline_chunk);

        const IndexStream index_stream(data.indices, data.index_type);
        index_stream.IntoJson(*inline_chunk);

        writer.Write("inline_data", std::move(inline_chunk));
    }

    static_assert(sizeof(unsigned) == 4, "4 bytes required for unsigned.");

    const uint64_t hash = mContentHash;
    const unsigned hi = (hash >> 32) & 0xffffffff;
    const unsigned lo = (hash >> 0 ) & 0xffffffff;
    writer.Write("content_hash_lo", lo);
    writer.Write("content_hash_hi", hi);

    for (const auto& it : mSubMeshes)
    {
        auto chunk = writer.NewWriteChunk();
        chunk->Write("key", it.first);
        chunk->Write("start", (unsigned)it.second.draw_cmd_start);
        chunk->Write("count", (unsigned)it.second.draw_cmd_count);
        writer.AppendChunk("meshes", std::move(chunk));
    }

}

bool PolygonMeshClass::FromJson(const data::Reader& reader)
{
    bool ok = true;
    ok &= reader.Read("id",     &mId);
    ok &= reader.Read("name",   &mName);
    ok &= reader.Read("static", &mStatic);
    ok &= reader.Read("uri",    &mContentUri);
    ok &= reader.Read("mesh",   &mMesh);
    if (reader.HasValue("src"))
        ok &= reader.Read("src", &mShaderSrc);

    if (const auto& inline_chunk = reader.GetReadChunk("inline_data"))
    {
        InlineData data;
        ok &= data.layout.FromJson(*inline_chunk);

        VertexBuffer vertex_buffer(&data.vertices);
        ok &= vertex_buffer.FromJson(*inline_chunk);

        CommandBuffer command_buffer(&data.cmds);
        ok &= command_buffer.FromJson(*inline_chunk);

        IndexBuffer index_buffer(&data.indices);
        ok &= index_buffer.FromJson(*inline_chunk);
        data.index_type = index_buffer.GetType();

        mData = std::move(data);
    }

    // legacy load
    if (reader.HasArray("vertices") && reader.HasArray("draws"))
    {
        VertexBuffer vertexBuffer(gfx::GetVertexLayout<Vertex2D>());

        for (unsigned i=0; i<reader.GetNumChunks("vertices"); ++i)
        {
            const auto& chunk = reader.GetReadChunk("vertices", i);
            float x, y, s, t;
            ok &= chunk->Read("x", &x);
            ok &= chunk->Read("y", &y);
            ok &= chunk->Read("s", &s);
            ok &= chunk->Read("t", &t);

            Vertex2D vertex;
            vertex.aPosition.x = x;
            vertex.aPosition.y = y;
            vertex.aTexCoord.x = s;
            vertex.aTexCoord.y = t;
            vertexBuffer.PushBack(&vertex);
        }

        std::vector<Geometry::DrawCommand> cmds;
        for (unsigned i=0; i<reader.GetNumChunks("draws"); ++i)
        {
            const auto& chunk = reader.GetReadChunk("draws", i);
            unsigned offset = 0;
            unsigned count  = 0;
            Geometry::DrawCommand cmd;
            ok &=  chunk->Read("type",   &cmd.type);
            ok &=  chunk->Read("offset", &offset);
            ok &=  chunk->Read("count",  &count);

            cmd.offset = offset;
            cmd.count  = count;
            cmds.push_back(cmd);
        }
        InlineData data;
        data.vertices = std::move(vertexBuffer).GetVertexBuffer();
        data.cmds     = std::move(cmds);
        data.layout   = gfx::GetVertexLayout<gfx::Vertex2D>();
        mData = std::move(data);
    }

    static_assert(sizeof(unsigned) == 4, "4 bytes required for unsigned.");

    uint64_t hash = 0;
    unsigned hi = 0;
    unsigned lo = 0;
    ok &= reader.Read("content_hash_lo", &lo);
    ok &= reader.Read("content_hash_hi", &hi);
    hash = (uint64_t(hi) << 32) | uint64_t(lo);
    mContentHash = hash;

    for (unsigned i=0; i<reader.GetNumChunks("meshes"); ++i)
    {
        const auto& chunk = reader.GetReadChunk("meshes", i);
        std::string key;
        unsigned count = 0;
        unsigned start = 0;
        ok &= chunk->Read("key", &key);
        ok &= chunk->Read("count", &count);
        ok &= chunk->Read("start", &start);
        DrawCmd cmd;
        cmd.draw_cmd_count = count;
        cmd.draw_cmd_start = start;
        mSubMeshes[key] = cmd;
    }

    return ok;
}

bool PolygonMeshClass::Construct(const Environment& env, Geometry::CreateArgs& create) const
{
    const auto usage = mStatic ? Geometry::Usage::Static
                               : Geometry::Usage::Dynamic;

    auto& geometry_buffer = create.buffer;
    ASSERT(geometry_buffer.GetNumDrawCmds() == 0);

    if (mData.has_value())
    {
        const auto& data = mData.value();
        create.usage  = usage;
        create.content_hash = GetContentHash();
        create.content_name = mName;

        geometry_buffer.SetVertexLayout(data.layout);
        geometry_buffer.UploadVertices(data.vertices.data(), data.vertices.size());

        if (!data.indices.empty())
            geometry_buffer.UploadIndices(data.indices.data(), data.indices.size(), data.index_type);

        for (const auto& cmd : data.cmds)
        {
            geometry_buffer.AddDrawCmd(cmd);
        }
    }

    if (mContentUri.empty())
        return true;

    gfx::Loader::ResourceDesc desc;
    desc.uri  = mContentUri;
    desc.id   = mId;
    desc.type = gfx::Loader::Type::Mesh;
    const auto& data_buffer = LoadResource(desc);
    if (!data_buffer)
    {
        ERROR("Failed to load polygon mesh. [uri='%1']", mContentUri);
        return false;
    }

    const char* beg = (const char*)data_buffer->GetData();
    const char* end = (const char*)data_buffer->GetData() + data_buffer->GetByteSize();
    auto [success, json, error] = base::JsonParse(beg, end);
    if (!success)
    {
        ERROR("Failed to parse geometry buffer. [uri='%1', error='%2'].", mContentUri, error);
        return false;
    }

    data::JsonObject reader(std::move(json));

    VertexBuffer vertex_buffer;
    if (!vertex_buffer.FromJson(reader))
    {
        ERROR("Failed to load polygon mesh vertex buffer. [uri='%1']", mContentUri);
        return false;
    }
    if (!vertex_buffer.Validate())
    {
        ERROR("Polygon mesh vertex buffer is not valid. [uri='%1']", mContentUri);
        return false;
    }

    CommandBuffer command_buffer;
    if (!command_buffer.FromJson(reader))
    {
        ERROR("Failed to load polygon mesh command buffer. [uri='%1']", mContentUri);
        return false;
    }

    IndexBuffer index_buffer;
    if (!index_buffer.FromJson(reader))
    {
        ERROR("Failed to load polygon mesh index buffer. [uri='%1']", mContentUri);
        return false;
    }

    create.usage = usage;
    create.content_name = mName;
    create.content_hash = GetContentHash();

    geometry_buffer.SetVertexLayout(vertex_buffer.GetLayout());
    geometry_buffer.UploadVertices(vertex_buffer.GetBufferPtr(), vertex_buffer.GetBufferSize());
    geometry_buffer.UploadIndices(index_buffer.GetBufferPtr(), index_buffer.GetBufferSize(), index_buffer.GetType());
    geometry_buffer.SetDrawCommands(command_buffer.GetCommandBuffer());
    return true;
}

PolygonMeshInstance::PolygonMeshInstance(std::shared_ptr<const PolygonMeshClass> klass, std::string sub_mesh_key ) noexcept
    : mClass(std::move(klass))
    , mSubMeshKey(std::move(sub_mesh_key))
    , mRandom(math::rand<0x12e4584>(0.0f, 1.0f))
{}

PolygonMeshInstance::PolygonMeshInstance(const PolygonMeshClass& klass, std::string sub_mesh_key)
    : mClass(std::make_shared<PolygonMeshClass>(klass))
    , mSubMeshKey(std::move(sub_mesh_key))
    , mRandom(math::rand<0x12e4584>(0.0f, 1.0f))
{}

void PolygonMeshInstance::ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const
{
    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = *env.proj_matrix;
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
    program.SetUniform("kTime", (float)mTime);
    program.SetUniform("kRandom",(float)mRandom);
}

ShaderSource PolygonMeshInstance::GetShader(const Environment& env, const Device& device) const
{
    return mClass->GetShader(env, device);
}

std::string PolygonMeshInstance::GetGeometryId(const Environment& env) const
{
    return mClass->GetGeometryId(env);
}

bool PolygonMeshInstance::Construct(const Environment& env, Geometry::CreateArgs& create) const
{
    return mClass->Construct(env, create);
}

bool PolygonMeshInstance::Construct(const Environment& env, const InstancedDraw& draw, gfx::InstancedDraw::CreateArgs& args) const
{
    InstancedDrawBuffer buffer;
    buffer.SetInstanceDataLayout(GetInstanceDataLayout<InstanceAttribute>());
    buffer.Resize(draw.instances.size());

    for (size_t i=0; i<draw.instances.size(); ++i)
    {
        const auto& instance = draw.instances[i];
        InstanceAttribute ia;
        ia.iaModelVectorX = ToVec(instance.model_to_world[0]);
        ia.iaModelVectorY = ToVec(instance.model_to_world[1]);
        ia.iaModelVectorZ = ToVec(instance.model_to_world[2]);
        ia.iaModelVectorW = ToVec(instance.model_to_world[3]);
        buffer.SetInstanceData(ia, i);
    }

    // we're not making any contribution to the instance data here, therefore
    // the hash and usage are exactly what the caller specified.
    args.usage = draw.usage;
    args.content_hash = draw.content_hash;
    args.content_name = draw.content_name;
    args.buffer = std::move(buffer);
    return true;
}

void PolygonMeshInstance::Update(const Environment& env, float dt)
{
    mTime += dt;
}

Drawable::DrawCmd PolygonMeshInstance::GetDrawCmd() const
{
    if (mSubMeshKey.empty())
        return Drawable::GetDrawCmd();

    if (const auto* cmd = mClass->GetSubMeshDrawCmd(mSubMeshKey))
        return *cmd;

    if (!mError)
    {
        WARN("No such polygon-mesh sub-mesh was found. [key='%1']", mSubMeshKey);
    }

    mError = true;
    return {0, 0};
}

Drawable::Usage PolygonMeshInstance::GetGeometryUsage() const
{
    if (mClass->IsStatic())
        return Usage::Static;

    return Usage::Dynamic;
}

size_t PolygonMeshInstance::GetGeometryHash() const
{
    return mClass->GetContentHash();
}

std::string PolygonMeshInstance::GetShaderId(const Environment& env) const
{
    return mClass->GetShaderId(env);
}

std::string PolygonMeshInstance::GetShaderName(const Environment& env) const
{
    return mClass->GetShaderName(env);
}

} // namespace
