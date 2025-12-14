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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/vec3.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <memory>
#include <cstddef>

#include "base/utility.h"
#include "graphics/drawable.h"
#include "graphics/vertex.h"

namespace gfx
{

    // Combines multiple primitive draw commands into a single
    // drawable shape.
    class PolygonMeshClass : public DrawableClass
    {
    public:
        enum class MeshType {
            // This mesh type is used for rendering 2D with Vertex2D.
            Simple2DRenderMesh,
            // This mesh type is used for rendering 2D effect with Vertex2D.
            Simple2DShardEffectMesh,
            // This mesh type is used for rendering 3D with sing Vertex3D
            Simple3DRenderMesh,
            // todo.
            Model3DRenderMesh,
            // This mesh type is used for rendering dimetric 2D tiles
            // with perceptual 3D support for things such as lights.
            Dimetric2DRenderMesh,
            // This mesh type is used for rendering isometric 2D tiles
            // with perceptual 3D support for things such as lights.
            Isometric2DRenderMesh
        };

        explicit PolygonMeshClass(std::string id = base::RandomString(10),
                                  std::string name = "") noexcept
          : mId(std::move(id))
          , mName(std::move(name))
        {}
        // Return whether the polygon's data is considered to be
        // static or not. Static content is not assumed to change
        // often and will map the polygon to a geometry object
        // based on the polygon's data. Thus, each polygon with
        // different data will have different geometry object.
        // However, If the polygon is updated frequently this would
        // then lead to the proliferation of excessive geometry objects.
        // In this case static can be set to false and the polygon
        // will map to a (single) dynamic geometry object more optimized
        // for draw/discard type of use.
        bool IsStatic() const noexcept
        { return mStatic; }
        // Set the polygon static or not. See comments in IsStatic.
        void SetStatic(bool on_off) noexcept
        { mStatic = on_off; }
        void SetDynamic(bool on_off) noexcept
        { mStatic = !on_off; }
        void SetContentHash(size_t hash) noexcept
        { mContentHash = hash; }
        size_t GetContentHash() const noexcept
        { return mContentHash; }
        bool HasInlineData() const noexcept
        { return mData.has_value(); }
        bool HasContentUri() const noexcept
        { return !mContentUri.empty(); }
        void ResetContentUri() noexcept
        { mContentUri.clear(); }
        void SetContentUri(std::string uri) noexcept
        { mContentUri = std::move(uri); }
        void SetMeshType(MeshType type) noexcept
        { mMeshType = type; }
        auto GetContentUri() const
        { return mContentUri; }
        std::string GetShaderSrc() const
        { return mShaderSrc; }
        auto GetMeshType() const noexcept
        { return mMeshType; }
        void SetShaderSrc(std::string src) noexcept
        { mShaderSrc = std::move(src); }
        bool HasShaderSrc() const noexcept
        { return !mShaderSrc.empty(); }

        void SetIndexBuffer(IndexBuffer&& buffer) noexcept;
        void SetIndexBuffer(const IndexBuffer& buffer);

        void SetVertexLayout(VertexLayout layout) noexcept;

        void SetVertexBuffer(VertexBuffer&& buffer) noexcept;
        void SetVertexBuffer(std::vector<uint8_t>&& buffer) noexcept;
        void SetVertexBuffer(const VertexBuffer& buffer);
        void SetVertexBuffer(const std::vector<uint8_t>& buffer);

        void SetCommandBuffer(CommandBuffer&& buffer) noexcept;
        void SetCommandBuffer(std::vector<Geometry::DrawCommand>&& buffer) noexcept;
        void SetCommandBuffer(const CommandBuffer& buffer);
        void SetCommandBuffer(const std::vector<Geometry::DrawCommand>& buffer);

        const VertexLayout* GetVertexLayout() const noexcept;
        const void* GetVertexBufferPtr() const noexcept;
        size_t GetVertexBufferSize() const noexcept;
        size_t GetVertexCount() const noexcept;
        size_t GetDrawCmdCount() const noexcept;
        const Geometry::DrawCommand* GetDrawCmd(size_t index) const noexcept;

        std::string GetGeometryId(const Environment& env) const;
        std::string GetShaderId(const Environment& env) const;
        std::string GetShaderName(const Environment& env) const;
        ShaderSource GetShader(const Environment& env, const Device& device) const;

        bool Construct(const Environment& env, Geometry::CreateArgs& create) const;

        void SetSubMeshDrawCmd(const std::string& key, const DrawCmd& cmd);

        const DrawCmd* GetSubMeshDrawCmd(const std::string& key) const noexcept;

        void ClearContent();

        void SetName(const std::string& name) override;

        SpatialMode GetSpatialMode() const override;
        Type GetType() const override;
        std::string GetId() const override;
        std::string GetName() const override;
        std::size_t GetHash() const override;
        std::unique_ptr<DrawableClass> Clone() const override;
        std::unique_ptr<DrawableClass> Copy() const override;
        void IntoJson(data::Writer& data) const override;
        bool FromJson(const data::Reader& data) override;

    private:
        std::string mId;
        std::string mName;
        std::size_t mContentHash = 0;
        // Content URI for a larger mesh (see InlineData)
        std::string mContentUri;
        // customized part of the vertex shader vertex transform.
        std::string mShaderSrc;
        // this is to support the simple 2D geometry with
        // only a few vertices. Could be migrated to use
        // a separate file but this is simply just so much
        // simpler for the time being even though it wastes
        // a bit of space since the data is kep around all
        // the time.
        struct InlineData {
            std::vector<uint8_t> vertices;
            std::vector<uint8_t> indices;
            std::vector<Geometry::DrawCommand> cmds;
            VertexLayout layout;
            Geometry::IndexType index_type = Geometry::IndexType::Index16;
        };
        std::optional<InlineData> mData;
        MeshType mMeshType = MeshType::Simple2DRenderMesh;
        std::unordered_map<std::string, DrawCmd> mSubMeshes;
        bool mStatic = true;
    };

    class PolygonMeshInstance : public Drawable
    {
    public:
        using MeshType = PolygonMeshClass::MeshType;

        // Data to support geometric (polygonal) tile rendering.
        // This is only used / required when the mesh type is
        // Perceptual3DTile
        struct Perceptual3DGeometry{
            // The axonometric view transformation that applies.
            glm::mat4 axonometric_model_view;
            // use the 3D data as the output from the perceptual vertex shader
            // instead of the 2D data.
            bool enable_perceptual_3D = false;
        };

        explicit PolygonMeshInstance(std::shared_ptr<const PolygonMeshClass> klass,
                                     std::string sub_mesh_key = "") noexcept;
        explicit PolygonMeshInstance(const PolygonMeshClass& klass, std::string sub_mesh_key = "");

        auto GetMeshType() const noexcept
        { return mClass->GetMeshType(); }
        std::string GetSubMeshKey() const
        { return mSubMeshKey; }
        void SetSubMeshKey(std::string key) noexcept
        { mSubMeshKey = std::move(key); }
        void SetTime(double time) noexcept
        { mTime = time; }
        void SetRandomValue(float value) noexcept
        { mRandom = value; }
        void SetPerceptualGeometry(const Perceptual3DGeometry& geometry) noexcept
        { mPerceptualGeometry = geometry; }

        bool ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& state) const override;
        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment& env) const override;
        std::string GetShaderName(const Environment& env) const override;
        std::string GetGeometryId(const Environment& env) const override;
        bool Construct(const Environment& env, Device&, Geometry::CreateArgs& create) const override;
        bool Construct(const Environment& env, Device&, const InstancedDraw& draw, gfx::InstancedDraw::CreateArgs& args) const override;
        void Update(const Environment& env, float dt) override;

        DrawCmd GetDrawCmd() const override;
        SpatialMode GetSpatialMode() const override;
        DrawPrimitive GetDrawPrimitive() const override;
        Type GetType() const override;
        Usage GetGeometryUsage() const override;

        size_t GetGeometryHash() const override;

        const DrawableClass* GetClass() const override
        { return mClass.get(); }
    private:
        std::shared_ptr<const PolygonMeshClass> mClass;
        std::optional<Perceptual3DGeometry> mPerceptualGeometry;
        std::string mSubMeshKey;
        double mTime = 0.0;
        float mRandom = 0.0f;
        mutable bool mError = false;
    };


} // namespace
