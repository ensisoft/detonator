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
            Simple2D,
            Simple3D,
            Model3D
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
        inline bool IsStatic() const noexcept
        { return mStatic; }
        // Set the polygon static or not. See comments in IsStatic.
        inline void SetStatic(bool on_off) noexcept
        { mStatic = on_off; }
        inline void SetDynamic(bool on_off) noexcept
        { mStatic = !on_off; }
        inline void SetContentHash(size_t hash) noexcept
        { mContentHash = hash; }
        inline size_t GetContentHash() const noexcept
        { return mContentHash; }
        inline bool HasInlineData() const noexcept
        { return mData.has_value(); }
        inline bool HasContentUri() const noexcept
        { return !mContentUri.empty(); }
        inline void ResetContentUri() noexcept
        { mContentUri.clear(); }
        inline void SetContentUri(std::string uri) noexcept
        { mContentUri = std::move(uri); }
        inline void SetMeshType(MeshType type) noexcept
        { mMesh = type; }
        inline std::string GetContentUri() const
        { return mContentUri; }
        inline MeshType GetMeshType() const noexcept
        { return mMesh; }

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
        size_t GetNumDrawCmds() const noexcept;
        const Geometry::DrawCommand* GetDrawCmd(size_t index) const noexcept;

        std::string GetGeometryId(const Environment& env) const;

        bool Construct(const Environment& env, Geometry::CreateArgs& create) const;

        void SetSubMeshDrawCmd(const std::string& key, const DrawCmd& cmd);

        const DrawCmd* GetSubMeshDrawCmd(const std::string& key) const noexcept;

        Type GetType() const override
        { return Type::Polygon; }
        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        void SetName(const std::string& name) override
        { mName = name; }
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
        MeshType mMesh = MeshType::Simple2D;
        std::unordered_map<std::string, DrawCmd> mSubMeshes;
        bool mStatic = true;
    };

    class PolygonMeshInstance : public Drawable
    {
    public:
        using MeshType = PolygonMeshClass::MeshType;

        explicit PolygonMeshInstance(std::shared_ptr<const PolygonMeshClass> klass,
                                     std::string sub_mesh_key = "") noexcept
          : mClass(std::move(klass))
          , mSubMeshKey(std::move(sub_mesh_key))
        {}
        explicit PolygonMeshInstance(const PolygonMeshClass& klass, std::string sub_mesh_key = "")
          : mClass(std::make_shared<PolygonMeshClass>(klass))
          , mSubMeshKey(std::move(sub_mesh_key))
        {}

        inline MeshType GetMeshType() const noexcept
        { return mClass->GetMeshType(); }
        inline std::string GetSubMeshKey() const
        { return mSubMeshKey; }
        inline void SetSubMeshKey(std::string key) noexcept
        { mSubMeshKey = std::move(key); }

        void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const override;
        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment& env) const override;
        std::string GetShaderName(const Environment& env) const override;
        std::string GetGeometryId(const Environment& env) const override;
        bool Construct(const Environment& env, Geometry::CreateArgs& create) const override;
        bool Construct(const Environment& env, const InstancedDraw& draw, gfx::InstancedDraw::CreateArgs& args) const override;

        DrawCmd GetDrawCmd() const override;

        DrawPrimitive GetDrawPrimitive() const override
        { return DrawPrimitive::Triangles; }
        Type GetType() const override
        { return Type::Polygon; }

        Usage GetGeometryUsage() const override;

        size_t GetGeometryHash() const override;

        const DrawableClass* GetClass() const override
        { return mClass.get(); }

    private:
        std::shared_ptr<const PolygonMeshClass> mClass;
        std::string mSubMeshKey;
        mutable bool mError = false;
    };


} // namespace
