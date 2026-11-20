// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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

#include "config.h"

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/utility.h"
#include "data/json.h"
#include "graphics/device.h"
#include "graphics/texture.h"
#include "graphics/drawable.h"
#include "graphics/geometry.h"
#include "graphics/drawcmd.h"
#include "graphics/linebatch.h"
#include "graphics/guidegrid.h"
#include "graphics/polygon_mesh.h"
#include "graphics/curve.h"
#include "graphics/triangle_iterator.h"
#include "graphics/particle_engine.h"
#include "graphics/simple_shape.h"
#include "graphics/tool/polygon.h"
#include "graphics/vertex_algo.h"
#include "graphics/geometry_algo.h"
#include "graphics/effect_drawable.h"
#include "graphics/drawable_effect.h"
#include "test_device.cpp"

bool operator==(const gfx::Vec2& lhs, const gfx::Vec2& rhs)
{
    return real::equals(lhs.x, rhs.x) &&
           real::equals(lhs.y, rhs.y);
}

bool operator==(const gfx::Vec3& lhs, const gfx::Vec3& rhs)
{
    return real::equals(lhs.x, rhs.x) &&
           real::equals(lhs.y, rhs.y) &&
           real::equals(lhs.z, rhs.z);
}

bool operator==(const gfx::Vertex2D& lhs, const gfx::Vertex2D& rhs)
{
    return real::equals(lhs.aPosition.x, rhs.aPosition.x) &&
           real::equals(lhs.aPosition.y, rhs.aPosition.y) &&
           real::equals(lhs.aTexCoord.x, rhs.aTexCoord.x) &&
           real::equals(lhs.aTexCoord.y, rhs.aTexCoord.y);
}

void unit_test_vertex_stream()
{
    TEST_CASE(test::Type::Feature)

    // vertex stream with vector
    {
        std::vector<gfx::Vertex2D> verts;
        verts.resize(3);
        verts[0].aPosition = gfx::Vec2 {  1.0f,  2.0f };
        verts[0].aTexCoord = gfx::Vec2 {  0.5f,  0.5f };
        verts[1].aPosition = gfx::Vec2 { -1.0f, -2.0f };
        verts[1].aTexCoord = gfx::Vec2 {  1.0f,  1.0f };
        verts[2].aPosition = gfx::Vec2 {  0.0f,  0.0f };
        verts[2].aTexCoord = gfx::Vec2 { -0.5f, -0.5f };

        const gfx::VertexStream stream(gfx::GetVertexLayout<gfx::Vertex2D>(), verts);
        TEST_REQUIRE(stream.IsValid());
        TEST_REQUIRE(stream.GetCount() == 3);
        TEST_REQUIRE(stream.HasAttribute("aPosition"));
        TEST_REQUIRE(stream.HasAttribute("aTexCoord"));
        TEST_REQUIRE(stream.HasAttribute("aFoobar") == false);

        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(0) == verts[0]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(1) == verts[1]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(2) == verts[2]);

        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 0) == verts[0].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 1) == verts[1].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 2) == verts[2].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aTexCoord", 0) == verts[0].aTexCoord);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aTexCoord", 1) == verts[1].aTexCoord);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aTexCoord", 2) == verts[2].aTexCoord);

        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(0) == verts[0]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(1) == verts[1]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(2) == verts[2]);
    }

    // vertex stream with typed array
    {
        gfx::Vertex2D verts[3];
        verts[0].aPosition = gfx::Vec2 {  1.0f,  2.0f };
        verts[0].aTexCoord = gfx::Vec2 {  0.5f,  0.5f };
        verts[1].aPosition = gfx::Vec2 { -1.0f, -2.0f };
        verts[1].aTexCoord = gfx::Vec2 {  1.0f,  1.0f };
        verts[2].aPosition = gfx::Vec2 {  0.0f,  0.0f };
        verts[2].aTexCoord = gfx::Vec2 { -0.5f, -0.5f };

        const gfx::VertexStream stream(gfx::GetVertexLayout<gfx::Vertex2D>(), verts, 3);
        TEST_REQUIRE(stream.IsValid());
        TEST_REQUIRE(stream.GetCount() == 3);
        TEST_REQUIRE(stream.HasAttribute("aPosition"));
        TEST_REQUIRE(stream.HasAttribute("aTexCoord"));
        TEST_REQUIRE(stream.HasAttribute("aFoobar") == false);

        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(0) == verts[0]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(1) == verts[1]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(2) == verts[2]);

        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 0) == verts[0].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 1) == verts[1].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 2) == verts[2].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aTexCoord", 0) == verts[0].aTexCoord);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aTexCoord", 1) == verts[1].aTexCoord);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aTexCoord", 2) == verts[2].aTexCoord);

        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(0) == verts[0]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(1) == verts[1]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(2) == verts[2]);
    }

    // vertex stream with opaque void* array
    {
        gfx::Vertex2D verts[3];
        verts[0].aPosition = gfx::Vec2 {  1.0f,  2.0f };
        verts[0].aTexCoord = gfx::Vec2 {  0.5f,  0.5f };
        verts[1].aPosition = gfx::Vec2 { -1.0f, -2.0f };
        verts[1].aTexCoord = gfx::Vec2 {  1.0f,  1.0f };
        verts[2].aPosition = gfx::Vec2 {  0.0f,  0.0f };
        verts[2].aTexCoord = gfx::Vec2 { -0.5f, -0.5f };

        const gfx::VertexStream stream(gfx::GetVertexLayout<gfx::Vertex2D>(), (const void*)verts, sizeof(verts));
        TEST_REQUIRE(stream.IsValid());
        TEST_REQUIRE(stream.GetCount() == 3);
        TEST_REQUIRE(stream.HasAttribute("aPosition"));
        TEST_REQUIRE(stream.HasAttribute("aTexCoord"));
        TEST_REQUIRE(stream.HasAttribute("aFoobar") == false);

        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(0) == verts[0]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(1) == verts[1]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(2) == verts[2]);

        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 0) == verts[0].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 1) == verts[1].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 2) == verts[2].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aTexCoord", 0) == verts[0].aTexCoord);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aTexCoord", 1) == verts[1].aTexCoord);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aTexCoord", 2) == verts[2].aTexCoord);

        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(0) == verts[0]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(1) == verts[1]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(2) == verts[2]);
    }


    // serialize
    {
        std::vector<gfx::Vertex2D> src;
        src.resize(3);
        src[0].aPosition = gfx::Vec2 {  1.0f,  2.0f };
        src[1].aPosition = gfx::Vec2 { -1.0f, -2.0f };
        src[2].aPosition = gfx::Vec2 {  4.0f,  5.0f };

        const gfx::VertexStream src_stream(gfx::GetVertexLayout<gfx::Vertex2D>(), src);

        data::JsonObject json;
        src_stream.IntoJson(json);

        gfx::VertexBuffer buffer;
        TEST_REQUIRE(buffer.FromJson(json));
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::Vertex2D>());
        TEST_REQUIRE(buffer.GetCount() == 3);
        TEST_REQUIRE(*buffer.GetVertex<gfx::Vertex2D>(0) == src[0]);
        TEST_REQUIRE(*buffer.GetVertex<gfx::Vertex2D>(1) == src[1]);
        TEST_REQUIRE(*buffer.GetVertex<gfx::Vertex2D>(2) == src[2]);
    }

}


void unit_test_command_stream()
{
    TEST_CASE(test::Type::Feature)

    {
        std::vector<gfx::Geometry::DrawCommand> vector;

        {
            gfx::Geometry::DrawCommand cmd;
            cmd.type   = gfx::Geometry::DrawType::TriangleFan;
            cmd.offset = 123;
            cmd.count  = 321;
            vector.push_back(cmd);
        }

        {
            gfx::Geometry::DrawCommand cmd;
            cmd.type   = gfx::Geometry::DrawType::LineLoop;
            cmd.offset = 0;
            cmd.count  = 10;
            vector.push_back(cmd);
        }

        gfx::CommandStream stream(vector);
        TEST_REQUIRE(stream.GetCount() == 2);
        TEST_REQUIRE(stream.GetCommand(0).count  == 321);
        TEST_REQUIRE(stream.GetCommand(0).offset == 123);
        TEST_REQUIRE(stream.GetCommand(0).type   == gfx::Geometry::DrawType::TriangleFan);
        TEST_REQUIRE(stream.GetCommand(1).count  == 10);
        TEST_REQUIRE(stream.GetCommand(1).offset == 0);
        TEST_REQUIRE(stream.GetCommand(1).type   == gfx::Geometry::DrawType::LineLoop);

        data::JsonObject json;
        stream.IntoJson(json);

        gfx::CommandBuffer buffer;
        TEST_REQUIRE(buffer.FromJson(json));
        TEST_REQUIRE(buffer.GetCount() == 2);
        TEST_REQUIRE(buffer.GetCommand(0).count  == 321);
        TEST_REQUIRE(buffer.GetCommand(0).offset == 123);
        TEST_REQUIRE(buffer.GetCommand(0).type   == gfx::Geometry::DrawType::TriangleFan);
        TEST_REQUIRE(buffer.GetCommand(1).count  == 10);
        TEST_REQUIRE(buffer.GetCommand(1).offset == 0);
        TEST_REQUIRE(buffer.GetCommand(1).type   == gfx::Geometry::DrawType::LineLoop);
    }
}


void unit_test_wireframe_algo()
{
    TEST_CASE(test::Type::Feature)

    {
        std::vector<gfx::Vertex2D> verts;
        verts.resize(6);
        verts[0].aPosition = gfx::Vec2 { -1.0f,  1.0f };
        verts[1].aPosition = gfx::Vec2 { -1.0f, -1.0f };
        verts[2].aPosition = gfx::Vec2 {  1.0f, -1.0f };
        verts[3].aPosition = gfx::Vec2 { -1.0f,  1.0f };
        verts[4].aPosition = gfx::Vec2 {  1.0f, -1.0f };
        verts[5].aPosition = gfx::Vec2 {  1.0f,  1.0f };

        gfx::GeometryBuffer buffer;
        buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
        buffer.UploadVertices(verts.data(), verts.size() * sizeof(gfx::Vertex2D));
        buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 0, 3);
        buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 3, 3);

        gfx::GeometryBuffer wireframe;
        gfx::CreateWireframe(buffer, wireframe);
        TEST_REQUIRE(wireframe.GetVertexBytes() == 12 * sizeof(gfx::Vertex2D));
        TEST_REQUIRE(wireframe.GetNumDrawCmds() == 1);
        TEST_REQUIRE(wireframe.GetDrawCmd(0).type == gfx::Geometry::DrawType::Lines);
        TEST_REQUIRE(wireframe.GetDrawCmd(0).offset == 0);
        TEST_REQUIRE(wireframe.GetDrawCmd(0).count == std::numeric_limits<uint32_t>::max());

        const gfx::VertexStream stream(wireframe.GetLayout(),
                                 wireframe.GetVertexDataPtr(),
                                 wireframe.GetVertexBytes());
        TEST_REQUIRE(stream.GetCount() == 12);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 0) == verts[0].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 1) == verts[1].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 2) == verts[1].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 3) == verts[2].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 4) == verts[2].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 5) == verts[0].aPosition);

        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 6) == verts[0].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 7) == verts[2].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 8) == verts[2].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 9) == verts[5].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 10) == verts[5].aPosition);
        TEST_REQUIRE(*stream.GetAttribute<gfx::Vec2>("aPosition", 11) == verts[0].aPosition);
    }

    {
        std::vector<gfx::Vertex2D> verts;
        verts.resize(6);
        verts[0].aPosition = gfx::Vec2 {  0.0f,  0.0f };
        verts[1].aPosition = gfx::Vec2 { -1.0f,  1.0f };
        verts[2].aPosition = gfx::Vec2 { -1.0f, -1.0f };
        verts[3].aPosition = gfx::Vec2 {  1.0f, -1.0f };
        verts[4].aPosition = gfx::Vec2 {  1.0f,  1.0f };
        verts[5].aPosition = gfx::Vec2 { -1.0f,  1.0f };

        gfx::GeometryBuffer buffer;
        buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
        buffer.UploadVertices(verts.data(), verts.size() * sizeof(gfx::Vertex2D));
        buffer.AddDrawCmd(gfx::Geometry::DrawType::TriangleFan);

        gfx::GeometryBuffer wireframe;
        gfx::CreateWireframe(buffer, wireframe);
        TEST_REQUIRE(wireframe.GetVertexBytes() == 18 * sizeof(gfx::Vertex2D));
        TEST_REQUIRE(wireframe.GetNumDrawCmds() == 1);
        TEST_REQUIRE(wireframe.GetDrawCmd(0).type == gfx::Geometry::DrawType::Lines);
        TEST_REQUIRE(wireframe.GetDrawCmd(0).offset == 0);
        TEST_REQUIRE(wireframe.GetDrawCmd(0).count == std::numeric_limits<uint32_t>::max());

        const gfx::VertexStream stream(wireframe.GetLayout(),
                                       wireframe.GetVertexDataPtr(),
                                       wireframe.GetVertexBytes());
        TEST_REQUIRE(stream.GetCount() == 18);

        struct Line {
            gfx::Vec2 a;
            gfx::Vec2 b;
        };
        std::vector<Line> lines;

        auto AddLine = [&lines](const auto& v0, const auto& v1) {
            Line line;
            line.a = v0.aPosition;
            line.b = v1.aPosition;
            lines.push_back(line);
        };

        AddLine(verts[0], verts[1]);
        AddLine(verts[1], verts[2]);
        AddLine(verts[2], verts[0]);
        AddLine(verts[3], verts[2]);
        AddLine(verts[3], verts[0]);
        AddLine(verts[4], verts[3]);
        AddLine(verts[4], verts[0]);
        AddLine(verts[5], verts[4]);
        AddLine(verts[5], verts[0]);

        for (size_t i=0; i<lines.size(); ++i)
        {
            const auto& line = lines[i];
            const auto start = i * 2;
            const auto& p0 = *stream.GetAttribute<gfx::Vec2>("aPosition", start+0);
            const auto& p1 = *stream.GetAttribute<gfx::Vec2>("aPosition", start+1);
            TEST_REQUIRE((p0 == line.a && p1 == line.b) || (p1 == line.a && p0 == line.b));
        }
    }
}

void unit_test_triangle_algo()
{
    TEST_CASE(test::Type::Feature)

    // vertex interpolation
    {
        gfx::Vertex2D v0, v1;
        v0.aPosition = {3.0f, 5.0f};
        v0.aTexCoord = {-1.0f, -2.0f};

        v1.aPosition = {4.0f, 6.0f};
        v1.aTexCoord = {0.0f, 1.0f};

        const auto& layout = gfx::GetVertexLayout<gfx::Vertex2D>();
        gfx::VertexBuffer result(layout);
        gfx::InterpolateVertex(&v0, &v1, layout, result);

        TEST_REQUIRE(result.GetCount() == 1);

        const auto* vertex = result.GetVertex<gfx::Vertex2D>(0);
        TEST_REQUIRE(vertex->aPosition.x == 3.5f);
        TEST_REQUIRE(vertex->aPosition.y == 5.5f);
        TEST_REQUIRE(vertex->aTexCoord.x == -0.5f);
        TEST_REQUIRE(vertex->aTexCoord.y == -0.5f);
    }
    // vertex interpolation
    {
        gfx::Vertex2D v0, v1;
        v0.aPosition = { 0.0f, 0.0f};
        v1.aPosition = {-1.5f, 3.0f};

        const auto& layout = gfx::GetVertexLayout<gfx::Vertex2D>();
        gfx::VertexBuffer result(layout);
        gfx::InterpolateVertex(&v0, &v1, layout, result);

        TEST_REQUIRE(result.GetCount() == 1);
        const auto* vertex = result.GetVertex<gfx::Vertex2D>(0);
        TEST_REQUIRE(vertex->aPosition.x == -0.75f);
        TEST_REQUIRE(vertex->aPosition.y == 1.5f);
    }

    // triangle sub-div 1
    {
        gfx::Vertex2D v0, v1, v2;
        v0.aPosition = { 0.0f, 0.0f};
        v1.aPosition = {-1.5f, 3.0f};
        v2.aPosition = { 1.5f, 3.0f};

        const auto& layout = gfx::GetVertexLayout<gfx::Vertex2D>();
        gfx::VertexBuffer result(layout);
        gfx::VertexBuffer tmp(layout);

        gfx::SubdivideTriangle(&v0, &v1, &v2, layout, result, tmp,
            gfx::TessellationAlgo::MidpointSubdivision, 0, 1);

        const auto& verts = result.CopyBuffer<gfx::Vertex2D>();
        TEST_REQUIRE(verts.size() == 4 * 3);

        // todo check verts
        //TEST_REQUIRE(foo[0].aPosition.x == 0.0f);
        //TEST_REQUIRE(foo[0].aPosition.y == 0.0f);
    }

    {
        std::vector<gfx::Vertex2D> verts;
        verts.resize(6);
        verts[0].aPosition = gfx::Vec2 { -1.0f,  1.0f };
        verts[1].aPosition = gfx::Vec2 { -1.0f, -1.0f };
        verts[2].aPosition = gfx::Vec2 {  1.0f, -1.0f };
        verts[3].aPosition = gfx::Vec2 { -1.0f,  1.0f };
        verts[4].aPosition = gfx::Vec2 {  1.0f, -1.0f };
        verts[5].aPosition = gfx::Vec2 {  1.0f,  1.0f };

        gfx::GeometryBuffer buffer;
        buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
        buffer.UploadVertices(verts.data(), verts.size() * sizeof(gfx::Vertex2D));
        buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 0, 3);
        buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 3, 3);

        gfx::GeometryBuffer result;
        gfx::TessellateMesh(buffer, result, gfx::TessellationAlgo::MidpointSubdivision, 0);
        TEST_REQUIRE(result.GetVertexBytes() == 6 * sizeof(gfx::Vertex2D));
        TEST_REQUIRE(result.GetNumDrawCmds() == 1);
        TEST_REQUIRE(result.GetDrawCmd(0).type == gfx::Geometry::DrawType::Triangles);
        TEST_REQUIRE(result.GetDrawCmd(0).offset == 0);
        TEST_REQUIRE(result.GetDrawCmd(0).count == std::numeric_limits<uint32_t>::max());

        const gfx::VertexStream stream(result.GetLayout(),
                              result.GetVertexDataPtr(),
                              result.GetVertexBytes());
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(0)->aPosition == verts[0].aPosition);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(1)->aPosition == verts[1].aPosition);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(2)->aPosition == verts[2].aPosition);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(3)->aPosition == verts[3].aPosition);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(4)->aPosition == verts[4].aPosition);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(5)->aPosition == verts[5].aPosition);
    }

    {
        std::vector<gfx::Vertex2D> verts;
        verts.resize(6);
        verts[0].aPosition = gfx::Vec2 { -1.0f,  1.0f };
        verts[1].aPosition = gfx::Vec2 { -1.0f, -1.0f };
        verts[2].aPosition = gfx::Vec2 {  1.0f, -1.0f };
        verts[3].aPosition = gfx::Vec2 { -1.0f,  1.0f };
        verts[4].aPosition = gfx::Vec2 {  1.0f, -1.0f };
        verts[5].aPosition = gfx::Vec2 {  1.0f,  1.0f };

        gfx::GeometryBuffer buffer;
        buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
        buffer.UploadVertices(verts.data(), verts.size() * sizeof(gfx::Vertex2D));
        buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 0, 3);
        buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 3, 3);

        gfx::GeometryBuffer result;
        gfx::TessellateMesh(buffer, result, gfx::TessellationAlgo::MidpointSubdivision, 1);
        TEST_REQUIRE(result.GetVertexBytes() == 2 * 4 * 3 * sizeof(gfx::Vertex2D));
        TEST_REQUIRE(result.GetNumDrawCmds() == 1);
        TEST_REQUIRE(result.GetDrawCmd(0).type == gfx::Geometry::DrawType::Triangles);
        TEST_REQUIRE(result.GetDrawCmd(0).offset == 0);
        TEST_REQUIRE(result.GetDrawCmd(0).count == std::numeric_limits<uint32_t>::max());


        // todo
        //const gfx::VertexStream stream(result.GetLayout(), result.GetVertexDataPtr(), esult.GetVertexBytes());
        // TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(0)->aPosition == verts[0].aPosition);
        // TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(1)->aPosition == verts[1].aPosition);
        // TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(2)->aPosition == verts[2].aPosition);
        // TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(3)->aPosition == verts[3].aPosition);
        // TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(4)->aPosition == verts[4].aPosition);
        // TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(5)->aPosition == verts[5].aPosition);
    }
}

void unit_test_tangent_algo()
{
    TEST_CASE(test::Type::Feature)

    const auto right = gfx::Vec3 { 1.0f, 0.0f, 0.0f };
    const auto up = gfx::Vec3 {0.0f, 1.0f, 0.0f };

    // a quad, 2 separate vertices.
    {
        std::vector<gfx::Vertex3D> vertices;
        vertices.resize(6);
        vertices[0].aPosition = gfx::Vec3{-1.0f,  1.0f, 0.0f};
        vertices[0].aTexCoord = gfx::Vec2{ 0.0f, 0.0f};

        vertices[1].aPosition = gfx::Vec3{-1.0f, -1.0f, 0.0f};
        vertices[1].aTexCoord = gfx::Vec2{ 0.0f, 1.0f};

        vertices[2].aPosition = gfx::Vec3{ 1.0f, -1.0f, 0.0f};
        vertices[2].aTexCoord = gfx::Vec2{ 1.0f, 1.0f};

        vertices[3].aPosition = gfx::Vec3{-1.0f,  1.0f, 0.0f};
        vertices[3].aTexCoord = gfx::Vec2{ 0.0f, 0.0f};

        vertices[4].aPosition = gfx::Vec3{ 1.0f, -1.0f, 0.0f};
        vertices[4].aTexCoord = gfx::Vec2{ 1.0f, 1.0f};

        vertices[5].aPosition = gfx::Vec3{ 1.0f,  1.0f, 0.0f};
        vertices[5].aTexCoord = gfx::Vec2{ 1.0f, 0.0f};

        gfx::GeometryBuffer buffer;
        buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex3D>());
        buffer.SetVertexBuffer(std::move(vertices));
        buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 0, 3);
        buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 3, 3);

        TEST_REQUIRE(gfx::ComputeTangents(buffer));

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(0)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(1)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(2)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(3)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(4)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(5)->aTangent == right);

        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(0)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(1)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(2)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(3)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(4)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(5)->aBitangent == up);
    }

    // a quad, using index buffer
    {
        std::vector<gfx::Vertex3D> vertices;
        vertices.resize(4);
        vertices[0].aPosition = gfx::Vec3{ -1.0f,  1.0f, 0.0f };
        vertices[0].aTexCoord = gfx::Vec2{  0.0f, 0.0f};

        vertices[1].aPosition = gfx::Vec3{ -1.0f, -1.0f, 0.0f };
        vertices[1].aTexCoord = gfx::Vec2{  0.0f, 1.0f};

        vertices[2].aPosition = gfx::Vec3{ 1.0f, -1.0f, 0.0f };
        vertices[2].aTexCoord = gfx::Vec2{ 1.0f, 1.0f};

        vertices[3].aPosition = gfx::Vec3{ 1.0f,  1.0f, 0.0f };
        vertices[3].aTexCoord = gfx::Vec2{ 1.0f, 0.0f};

        std::vector<gfx::Index16> indices;
        indices.push_back(0);
        indices.push_back(1);
        indices.push_back(2);
        indices.push_back(0);
        indices.push_back(2);
        indices.push_back(3);

        gfx::GeometryBuffer buffer;
        buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex3D>());
        buffer.SetIndexBuffer(std::move(indices));
        buffer.SetVertexBuffer(std::move(vertices));
        buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 0, 3);
        buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 3, 3);

        TEST_REQUIRE(gfx::ComputeTangents(buffer));

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(0)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(1)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(2)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(3)->aTangent == right);

        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(0)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(1)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(2)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(3)->aBitangent == up);
    }

    // a triangle
    {
        std::vector<gfx::Vertex3D> vertices;
        vertices.resize(3);
        vertices[0].aPosition = gfx::Vec3{  0.0f,  1.0f, 0.0f }; // apex
        vertices[0].aTexCoord = gfx::Vec2{  0.5f, 0.0f};

        vertices[1].aPosition = gfx::Vec3{ -1.0f, -1.0f, 0.0f };
        vertices[1].aTexCoord = gfx::Vec2{  0.0f, 1.0f};

        vertices[2].aPosition = gfx::Vec3{ 1.0f, -1.0f, 0.0f };
        vertices[2].aTexCoord = gfx::Vec2{ 1.0f, 1.0f};

        gfx::GeometryBuffer buffer;
        buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex3D>());
        buffer.SetVertexBuffer(std::move(vertices));
        buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 0, 3);

        TEST_REQUIRE(gfx::ComputeTangents(buffer));

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(0)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(1)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(2)->aTangent == right);

        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(0)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(1)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(2)->aBitangent == up);
    }

    // triangle fan
    {
        std::vector<gfx::Vertex3D> vertices;
        vertices.resize(3);
        vertices[0].aPosition = gfx::Vec3{  0.0f,  1.0f, 0.0f }; // apex
        vertices[0].aTexCoord = gfx::Vec2{  0.5f, 0.0f};

        vertices[1].aPosition = gfx::Vec3{ -1.0f, -1.0f, 0.0f };
        vertices[1].aTexCoord = gfx::Vec2{  0.0f, 1.0f};

        vertices[2].aPosition = gfx::Vec3{ 1.0f, -1.0f, 0.0f };
        vertices[2].aTexCoord = gfx::Vec2{ 1.0f, 1.0f};

        gfx::GeometryBuffer buffer;
        buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex3D>());
        buffer.SetVertexBuffer(std::move(vertices));
        buffer.AddDrawCmd(gfx::Geometry::DrawType::TriangleFan, 0, 3);

        TEST_REQUIRE(gfx::ComputeTangents(buffer));

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(0)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(1)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(2)->aTangent == right);

        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(0)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(1)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(2)->aBitangent == up);
    }

    // triangle fan + 1
    {
        std::vector<gfx::Vertex3D> vertices;
        vertices.resize(4);
        vertices[0].aPosition = gfx::Vec3{  0.0f,  1.0f, 0.0f }; // apex
        vertices[0].aTexCoord = gfx::Vec2{  0.5f, 0.0f};

        vertices[1].aPosition = gfx::Vec3{ -1.0f, -1.0f, 0.0f };
        vertices[1].aTexCoord = gfx::Vec2{  0.0f, 1.0f};

        vertices[2].aPosition = gfx::Vec3{ 1.0f, -1.0f, 0.0f };
        vertices[2].aTexCoord = gfx::Vec2{ 1.0f, 1.0f};

        vertices[3].aPosition = gfx::Vec3{ 1.0f, 1.0f, 0.0f };
        vertices[3].aTexCoord = gfx::Vec2{ 1.0f, 0.0f};

        gfx::GeometryBuffer buffer;
        buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex3D>());
        buffer.SetVertexBuffer(std::move(vertices));
        buffer.AddDrawCmd(gfx::Geometry::DrawType::TriangleFan, 0, 4);

        TEST_REQUIRE(gfx::ComputeTangents(buffer));

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(0)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(1)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(2)->aTangent == right);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(3)->aTangent == right);

        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(0)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(1)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(2)->aBitangent == up);
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(3)->aBitangent == up);
    }
}

void unit_test_polygon_builder_json()
{
    TEST_CASE(test::Type::Feature)

    std::vector<gfx::Vertex2D> verts;
    gfx::Vertex2D v0;
    v0.aPosition.x = 1.0f;
    v0.aPosition.y = 2.0f;
    v0.aTexCoord.x = -1.0f;
    v0.aTexCoord.y = -0.5f;
    verts.push_back(v0);

    gfx::tool::PolygonBuilder2D builder;
    gfx::tool::PolygonBuilder2D::DrawCommand cmd;
    cmd.type = gfx::Geometry::DrawType::TriangleFan;
    cmd.offset = 1243;
    cmd.count = 555;
    builder.AddVertices(std::move(verts));
    builder.AddDrawCommand(cmd);

    // to/from json
    {
        data::JsonObject json;
        builder.IntoJson(json);

        gfx::tool::PolygonBuilder2D copy;
        TEST_REQUIRE(copy.FromJson(json));
        TEST_REQUIRE(copy.GetVertexCount() == 1);
        TEST_REQUIRE(copy.GetCommandCount() == 1);
        TEST_REQUIRE(copy.GetVertex(0) == v0);
        TEST_REQUIRE(copy.GetDrawCommand(0).type   == gfx::Geometry::DrawType::TriangleFan);
        TEST_REQUIRE(copy.GetDrawCommand(0).offset == 1243);
        TEST_REQUIRE(copy.GetDrawCommand(0).count  == 555);
        TEST_REQUIRE(copy.GetContentHash() == builder.GetContentHash());
    }
}

void unit_test_polygon_builder_build()
{
    TEST_CASE(test::Type::Feature)

    // some test vertices.
    std::vector<gfx::Vertex2D> verts;
    verts.resize(6);
    verts[0].aPosition.x = 0.0f;
    verts[1].aPosition.x = 1.0f;
    verts[2].aPosition.x = 2.0f;
    verts[3].aPosition.x = 3.0f;
    verts[4].aPosition.x = 4.0f;
    verts[5].aPosition.x = 5.0f;

    // test finding the right draw command.
    {
        gfx::tool::PolygonBuilder2D poly;
        poly.AddVertices(verts);

        gfx::Geometry::DrawCommand cmd;
        cmd.offset = 0;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);

        cmd.offset = 3;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);

        TEST_REQUIRE(poly.FindDrawCommand(0) == 0);
        TEST_REQUIRE(poly.FindDrawCommand(1) == 0);
        TEST_REQUIRE(poly.FindDrawCommand(2) == 0);
        TEST_REQUIRE(poly.FindDrawCommand(3) == 1);
        TEST_REQUIRE(poly.FindDrawCommand(4) == 1);
        TEST_REQUIRE(poly.FindDrawCommand(5) == 1);

        poly.ClearDrawCommands();
        cmd.offset = 0;
        cmd.count  = 6;
        poly.AddDrawCommand(cmd);
        TEST_REQUIRE(poly.FindDrawCommand(0) == 0);
        TEST_REQUIRE(poly.FindDrawCommand(1) == 0);
        TEST_REQUIRE(poly.FindDrawCommand(2) == 0);
        TEST_REQUIRE(poly.FindDrawCommand(3) == 0);
        TEST_REQUIRE(poly.FindDrawCommand(4) == 0);
        TEST_REQUIRE(poly.FindDrawCommand(5) == 0);

    }

    // test erase/insert with only one draw cmd
    {
        gfx::tool::PolygonBuilder2D poly;
        poly.AddVertices(verts);

        gfx::Geometry::DrawCommand cmd;
        cmd.offset = 0;
        cmd.count  = 6;
        poly.AddDrawCommand(cmd);

        gfx::Vertex2D v;
        v.aPosition.x = 6.0f;
        poly.InsertVertex(v, 0, 6);
        TEST_REQUIRE(poly.GetDrawCommand(0).offset == 0);
        TEST_REQUIRE(poly.GetDrawCommand(0).count == 7);

        v.aPosition.x = -1.0f;
        poly.InsertVertex(v, 0, 0);
        TEST_REQUIRE(poly.GetDrawCommand(0).offset == 0);
        TEST_REQUIRE(poly.GetDrawCommand(0).count == 8);
        TEST_REQUIRE(poly.GetVertex(0).aPosition.x == real::float32(-1.0f));
        TEST_REQUIRE(poly.GetVertex(1).aPosition.x == real::float32(0.0f));
        TEST_REQUIRE(poly.GetVertex(2).aPosition.x == real::float32(1.0f));
        TEST_REQUIRE(poly.GetVertex(3).aPosition.x == real::float32(2.0f));
        TEST_REQUIRE(poly.GetVertex(4).aPosition.x == real::float32(3.0f));
        TEST_REQUIRE(poly.GetVertex(5).aPosition.x == real::float32(4.0f));
        TEST_REQUIRE(poly.GetVertex(6).aPosition.x == real::float32(5.0f));
        TEST_REQUIRE(poly.GetVertex(7).aPosition.x == real::float32(6.0f));
    }

    // test erase/insert first draw command first index
    {
        gfx::tool::PolygonBuilder2D poly;
        poly.AddVertices(verts);

        gfx::Geometry::DrawCommand cmd;
        cmd.offset = 0;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);

        cmd.offset = 3;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);
        TEST_REQUIRE(poly.GetCommandCount() == 2);
        TEST_REQUIRE(poly.GetVertexCount() == 6);

        poly.EraseVertex(0);
        TEST_REQUIRE(poly.GetVertex(0).aPosition.x == real::float32(1.0f));
        TEST_REQUIRE(poly.GetVertex(1).aPosition.x == real::float32(2.0f));
        TEST_REQUIRE(poly.GetVertex(2).aPosition.x == real::float32(3.0f));
        TEST_REQUIRE(poly.GetVertex(3).aPosition.x == real::float32(4.0f));
        TEST_REQUIRE(poly.GetVertex(4).aPosition.x == real::float32(5.0f));
        TEST_REQUIRE(poly.GetDrawCommand(0).offset == 0);
        TEST_REQUIRE(poly.GetDrawCommand(0).count  == 2);
        TEST_REQUIRE(poly.GetDrawCommand(1).offset == 2);
        TEST_REQUIRE(poly.GetDrawCommand(1).count  == 3);

        poly.InsertVertex(verts[0], 0, 0);
        TEST_REQUIRE(poly.GetDrawCommand(0).offset == 0);
        TEST_REQUIRE(poly.GetDrawCommand(0).count  == 3);
        TEST_REQUIRE(poly.GetDrawCommand(1).offset == 3);
        TEST_REQUIRE(poly.GetDrawCommand(1).count  == 3);
        TEST_REQUIRE(poly.GetVertex(0).aPosition.x == real::float32(0.0f));
        TEST_REQUIRE(poly.GetVertex(1).aPosition.x == real::float32(1.0f));
        TEST_REQUIRE(poly.GetVertex(2).aPosition.x == real::float32(2.0f));
        TEST_REQUIRE(poly.GetVertex(3).aPosition.x == real::float32(3.0f));
        TEST_REQUIRE(poly.GetVertex(4).aPosition.x == real::float32(4.0f));
        TEST_REQUIRE(poly.GetVertex(5).aPosition.x == real::float32(5.0f));
    }

    // test erase/insert first draw command last index
    {
        gfx::tool::PolygonBuilder2D poly;
        poly.AddVertices(verts);

        gfx::Geometry::DrawCommand cmd;
        cmd.offset = 0;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);

        cmd.offset = 3;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);
        TEST_REQUIRE(poly.GetCommandCount() == 2);
        TEST_REQUIRE(poly.GetVertexCount() == 6);

        poly.EraseVertex(2);
        TEST_REQUIRE(poly.GetVertex(0).aPosition.x == real::float32(0.0f));
        TEST_REQUIRE(poly.GetVertex(1).aPosition.x == real::float32(1.0f));
        TEST_REQUIRE(poly.GetVertex(2).aPosition.x == real::float32(3.0f));
        TEST_REQUIRE(poly.GetVertex(3).aPosition.x == real::float32(4.0f));
        TEST_REQUIRE(poly.GetVertex(4).aPosition.x == real::float32(5.0f));
        TEST_REQUIRE(poly.GetDrawCommand(0).offset == 0);
        TEST_REQUIRE(poly.GetDrawCommand(0).count  == 2);
        TEST_REQUIRE(poly.GetDrawCommand(1).offset == 2);
        TEST_REQUIRE(poly.GetDrawCommand(1).count  == 3);

        poly.InsertVertex(verts[2], 0, 2);
        TEST_REQUIRE(poly.GetDrawCommand(0).offset == 0);
        TEST_REQUIRE(poly.GetDrawCommand(0).count  == 3);
        TEST_REQUIRE(poly.GetDrawCommand(1).offset == 3);
        TEST_REQUIRE(poly.GetDrawCommand(1).count  == 3);
        TEST_REQUIRE(poly.GetVertex(0).aPosition.x == real::float32(0.0f));
        TEST_REQUIRE(poly.GetVertex(1).aPosition.x == real::float32(1.0f));
        TEST_REQUIRE(poly.GetVertex(2).aPosition.x == real::float32(2.0f));
        TEST_REQUIRE(poly.GetVertex(3).aPosition.x == real::float32(3.0f));
        TEST_REQUIRE(poly.GetVertex(4).aPosition.x == real::float32(4.0f));
        TEST_REQUIRE(poly.GetVertex(5).aPosition.x == real::float32(5.0f));
    }

    // test erase/insert from/into second draw command.
    {
        gfx::tool::PolygonBuilder2D poly;
        poly.AddVertices(verts);

        gfx::Geometry::DrawCommand cmd;
        cmd.offset = 0;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);

        cmd.offset = 3;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);
        TEST_REQUIRE(poly.GetCommandCount() == 2);
        TEST_REQUIRE(poly.GetVertexCount() == 6);

        poly.EraseVertex(4);
        TEST_REQUIRE(poly.GetDrawCommand(0).offset == 0);
        TEST_REQUIRE(poly.GetDrawCommand(0).count  == 3);
        TEST_REQUIRE(poly.GetDrawCommand(1).offset == 3);
        TEST_REQUIRE(poly.GetDrawCommand(1).count  == 2);

        poly.InsertVertex(verts[4], 1, 1);
        TEST_REQUIRE(poly.GetDrawCommand(0).offset == 0);
        TEST_REQUIRE(poly.GetDrawCommand(0).count  == 3);
        TEST_REQUIRE(poly.GetDrawCommand(1).offset == 3);
        TEST_REQUIRE(poly.GetDrawCommand(1).count  == 3);
        TEST_REQUIRE(real::equals(poly.GetVertex(0).aPosition.x, 0.0f));
        TEST_REQUIRE(real::equals(poly.GetVertex(1).aPosition.x, 1.0f));
        TEST_REQUIRE(real::equals(poly.GetVertex(2).aPosition.x, 2.0f));
        TEST_REQUIRE(real::equals(poly.GetVertex(3).aPosition.x, 3.0f));
        TEST_REQUIRE(real::equals(poly.GetVertex(4).aPosition.x, 4.0f));
        TEST_REQUIRE(real::equals(poly.GetVertex(5).aPosition.x, 5.0f));
    }

    // erase whole command from the beginning
    {
        gfx::tool::PolygonBuilder2D poly;
        poly.AddVertices(verts);

        gfx::Geometry::DrawCommand cmd0;
        cmd0.offset = 0;
        cmd0.count  = 3;

        gfx::Geometry::DrawCommand cmd1;
        cmd1.offset = 3;
        cmd1.count  = 4; // fake number,

        poly.AddDrawCommand(cmd0);
        poly.AddDrawCommand(cmd1);

        poly.EraseCommand(0);
        TEST_REQUIRE(poly.GetCommandCount() == 1);
        TEST_REQUIRE(poly.GetVertexCount() == 3);
        const auto& cmd = poly.GetDrawCommand(0);
        TEST_REQUIRE(cmd.count == 4);
        TEST_REQUIRE(cmd.offset == 0);
    }

    // erase whole command from the beginning
    {
        std::vector<gfx::Vertex2D> verts;
        verts.resize(12);
        verts[0].aPosition.x = 1.0f;
        verts[1].aPosition.x = 1.0f;
        verts[2].aPosition.x = 1.0f;

        verts[3].aPosition.x = 2.0f;
        verts[4].aPosition.x = 2.0f;
        verts[5].aPosition.x = 2.0f;
        verts[6].aPosition.x = 2.0f;

        verts[7].aPosition.x = 3.0f;
        verts[8].aPosition.x = 3.0f;
        verts[9].aPosition.x = 3.0f;
        verts[10].aPosition.x = 3.0f;
        verts[11].aPosition.x = 3.0f;

        gfx::Geometry::DrawCommand cmd0;
        cmd0.offset = 0;
        cmd0.count  = 3;

        gfx::Geometry::DrawCommand cmd1;
        cmd1.offset = 3;
        cmd1.count  = 4; // fake number,

        gfx::Geometry::DrawCommand cmd2;
        cmd2.offset = 7;
        cmd2.count  = 5; // fake number,

        gfx::tool::PolygonBuilder2D poly;
        poly.AddVertices(verts);
        poly.AddDrawCommand(cmd0);
        poly.AddDrawCommand(cmd1);
        poly.AddDrawCommand(cmd2);

        poly.EraseCommand(1);
        TEST_REQUIRE(poly.GetCommandCount() == 2);
        TEST_REQUIRE(poly.GetVertexCount() == 8);
        {
            const auto& cmd = poly.GetDrawCommand(0);
            TEST_REQUIRE(cmd.count == 3);
            TEST_REQUIRE(cmd.offset == 0);
            TEST_REQUIRE(poly.GetVertex(0).aPosition.x == 1.0f);
            TEST_REQUIRE(poly.GetVertex(1).aPosition.x == 1.0f);
            TEST_REQUIRE(poly.GetVertex(2).aPosition.x == 1.0f);
        }
        {
            const auto& cmd = poly.GetDrawCommand(1);
            TEST_REQUIRE(cmd.count == 5);
            TEST_REQUIRE(cmd.offset == 3);
            TEST_REQUIRE(poly.GetVertex(3).aPosition.x == 3.0f);
            TEST_REQUIRE(poly.GetVertex(4).aPosition.x == 3.0f);
            TEST_REQUIRE(poly.GetVertex(5).aPosition.x == 3.0f);
            TEST_REQUIRE(poly.GetVertex(6).aPosition.x == 3.0f);
            TEST_REQUIRE(poly.GetVertex(7).aPosition.x == 3.0f);
        }
    }

}

void unit_test_particle_engine_data()
{
    TEST_CASE(test::Type::Feature)

    gfx::ParticleEngineClass::Params params;
    params.motion   = gfx::ParticleEngineClass::Motion::Projectile;
    params.mode     = gfx::ParticleEngineClass::SpawnPolicy::Continuous;
    params.boundary = gfx::ParticleEngineClass::BoundaryPolicy::Kill;
    params.num_particles = 500.0f;
    params.min_lifetime  = 2.0f;
    params.max_lifetime  = 5.0f;
    params.max_xpos      = 4.0f;
    params.max_ypos      = 2.0f;
    params.init_rect_xpos = 5.0f;
    params.init_rect_ypos = 6.0f;
    params.init_rect_width = 100.0f;
    params.init_rect_height = 200.0f;
    params.min_velocity = 4.0f;
    params.max_velocity = 5.5f;
    params.direction_sector_start_angle = 0.5f;
    params.direction_sector_size = 0.0f;
    params.min_point_size = 4.0f;
    params.max_point_size = 6.0f;
    params.min_alpha = 0.5f;
    params.max_alpha = 0.6f;
    params.rate_of_change_in_size_wrt_time = 1.0f;
    params.rate_of_change_in_size_wrt_dist = 2.0f;
    params.rate_of_change_in_alpha_wrt_dist = 3.0f;
    params.rate_of_change_in_alpha_wrt_time = 4.0f;
    params.gravity = glm::vec2{5.0f, 5.0f};
    gfx::ParticleEngineClass klass(params);

    // to/from json
    {
        data::JsonObject json;
        klass.IntoJson(json);
        gfx::ParticleEngineClass ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.GetId() == klass.GetId());
        TEST_REQUIRE(ret.GetHash() == klass.GetHash());

        const auto& p = ret.GetParams();
        TEST_REQUIRE(p.motion                           == gfx::ParticleEngineClass::Motion::Projectile);
        TEST_REQUIRE(p.mode                             == gfx::ParticleEngineClass::SpawnPolicy::Continuous);
        TEST_REQUIRE(p.boundary                         == gfx::ParticleEngineClass::BoundaryPolicy::Kill);
        TEST_REQUIRE(p.num_particles                    == real::float32(500.0f));
        TEST_REQUIRE(p.min_lifetime                     == real::float32(2.0f));
        TEST_REQUIRE(p.max_lifetime                     == real::float32(5.0f));
        TEST_REQUIRE(p.max_xpos                         == real::float32(4.0f));
        TEST_REQUIRE(p.max_ypos                         == real::float32(2.0f));
        TEST_REQUIRE(p.init_rect_xpos                   == real::float32(5.0f));
        TEST_REQUIRE(p.init_rect_ypos                   == real::float32(6.0f));
        TEST_REQUIRE(p.init_rect_width                  == real::float32(100.0f));
        TEST_REQUIRE(p.init_rect_height                 == real::float32(200.0f));
        TEST_REQUIRE(p.min_velocity                     == real::float32(4.0f));
        TEST_REQUIRE(p.max_velocity                     == real::float32(5.5f));
        TEST_REQUIRE(p.direction_sector_start_angle     == real::float32(0.5f));
        TEST_REQUIRE(p.direction_sector_size            == real::float32(0.0f));
        TEST_REQUIRE(p.min_point_size                   == real::float32(4.0f));
        TEST_REQUIRE(p.max_point_size                   == real::float32(6.0f));
        TEST_REQUIRE(p.min_alpha                        == real::float32(0.5f));
        TEST_REQUIRE(p.max_alpha                        == real::float32(0.6f));
        TEST_REQUIRE(p.rate_of_change_in_size_wrt_time  == real::float32(1.0f));
        TEST_REQUIRE(p.rate_of_change_in_size_wrt_dist  == real::float32(2.0f));
        TEST_REQUIRE(p.rate_of_change_in_alpha_wrt_dist == real::float32(3.0f));
        TEST_REQUIRE(p.rate_of_change_in_alpha_wrt_time == real::float32(4.0f));
        TEST_REQUIRE(p.gravity == glm::vec2(5.0f, 5.0f));
    }

    // test copy ctor/assignment
    {
        gfx::ParticleEngineClass copy(klass);
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());

        // test assignment
        copy = klass;
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());


    }
    // test virtual copy/clone
    {
        auto copy = klass.Copy();
        TEST_REQUIRE(copy->GetId() == klass.GetId());
        TEST_REQUIRE(copy->GetHash() == klass.GetHash());

        auto clone = klass.Clone();
        TEST_REQUIRE(clone->GetId() != klass.GetId());
        TEST_REQUIRE(clone->GetHash() != klass.GetHash());
    }
}

void unit_test_polygon_data()
{
    TEST_CASE(test::Type::Feature)

    gfx::Vertex2D verts[3];
    verts[0].aPosition = gfx::Vec2 {  1.0f,  2.0f };
    verts[0].aTexCoord = gfx::Vec2 {  0.5f,  0.5f };
    verts[1].aPosition = gfx::Vec2 { -1.0f, -2.0f };
    verts[1].aTexCoord = gfx::Vec2 {  1.0f,  1.0f };
    verts[2].aPosition = gfx::Vec2 {  0.0f,  0.0f };
    verts[2].aTexCoord = gfx::Vec2 { -0.5f, -0.5f };

    gfx::VertexBuffer buffer(gfx::GetVertexLayout<gfx::Vertex2D>());
    buffer.PushBack(&verts[0]);
    buffer.PushBack(&verts[1]);
    buffer.PushBack(&verts[2]);

    std::vector<uint8_t> vertex_flags;
    vertex_flags.resize(3);
    vertex_flags[0] = 0x1;
    vertex_flags[1] = 0x2;
    vertex_flags[2] = 0x3;

    gfx::PolygonMeshClass klass;
    klass.SetName("foo");
    klass.SetContentHash(0xffaabbee001177ff);
    klass.SetStatic(false);
    klass.SetVertexBuffer(std::move(buffer));
    klass.SetVertexFlagBuffer(std::move(vertex_flags));

    std::vector<gfx::Geometry::DrawCommand> cmds;
    cmds.resize(1);
    cmds[0].type   = gfx::Geometry::DrawType::TriangleFan;
    cmds[0].offset = 123;
    cmds[0].count  = 5;
    klass.SetCommandBuffer(std::move(cmds));

    klass.SetSubMeshDrawCmd("foo", gfx::PolygonMeshClass::SubMeshDrawCmd { 0, 10 });
    klass.SetSubMeshDrawCmd("bar", gfx::PolygonMeshClass::SubMeshDrawCmd { 10, 1 });

    // to/from json
    {
        data::JsonObject json;
        klass.IntoJson(json);

        gfx::PolygonMeshClass ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.HasInlineData());
        TEST_REQUIRE(ret.GetDrawCmdCount() == 1);
        TEST_REQUIRE(ret.GetVertexBufferSize() == sizeof(gfx::Vertex2D) * 3);
        TEST_REQUIRE(*ret.GetVertexLayout() == gfx::GetVertexLayout<gfx::Vertex2D>());
        TEST_REQUIRE(ret.GetDrawCmd(0)->type == gfx::Geometry::DrawType::TriangleFan);
        TEST_REQUIRE(ret.GetDrawCmd(0)->offset == 123);
        TEST_REQUIRE(ret.GetDrawCmd(0)->count == 5);

        const gfx::VertexStream stream(*ret.GetVertexLayout(),
                                        ret.GetVertexBufferPtr(),
                                        ret.GetVertexBufferSize());
        TEST_REQUIRE(stream.GetCount() == 3);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(0) == verts[0]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(1) == verts[1]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(2) == verts[2]);

        TEST_REQUIRE(ret.GetContentHash() == klass.GetContentHash());
        TEST_REQUIRE(ret.GetName() == klass.GetName());
        TEST_REQUIRE(ret.IsStatic() == klass.IsStatic());
        TEST_REQUIRE(ret.GetHash() == klass.GetHash());

        TEST_REQUIRE(ret.GetSubMeshDrawCmd("foo")->draw_cmd_start == 0);
        TEST_REQUIRE(ret.GetSubMeshDrawCmd("foo")->draw_cmd_count == 10);
        TEST_REQUIRE(ret.GetSubMeshDrawCmd("bar")->draw_cmd_start == 10);
        TEST_REQUIRE(ret.GetSubMeshDrawCmd("bar")->draw_cmd_count == 1);

        const auto* flags = ret.GetVertexFlagBufferPtr();
        TEST_REQUIRE(flags);
        TEST_REQUIRE(flags[0] == 0x1);
        TEST_REQUIRE(flags[1] == 0x2);
        TEST_REQUIRE(flags[2] == 0x3);
    }
}

void unit_test_curve_data()
{
    TEST_CASE(test::Type::Feature)

    gfx::CurveClass klass;
    klass.SetName("foo");
    klass.SetContentHash(0xffaabbee001177ff);
    klass.SetStatic(false);
    klass.SetCurveType(gfx::CurveClass::CurveType::PolylineSpline);
    klass.SetJointStyle(gfx::CurveClass::JointStyle::Round);
    klass.SetEndCapStyle(gfx::CurveClass::EndCapStyle::Butt);
    klass.AddPoint(gfx::CurvePoint({ 1.0f,  2.0f}, { 0.5f,  0.5f}, 0.25f));
    klass.AddPoint(gfx::CurvePoint({-1.0f, -2.0f}, { 1.0f,  1.0f}, 0.5f));
    klass.AddPoint(gfx::CurvePoint({ 0.0f,  0.0f}, {-0.5f, -0.5f}, 1.0f));

    // to/from json
    {
        data::JsonObject json;
        klass.IntoJson(json);

        gfx::CurveClass ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.GetName() == klass.GetName());
        TEST_REQUIRE(ret.GetId() == klass.GetId());
        TEST_REQUIRE(ret.GetContentHash() == klass.GetContentHash());
        TEST_REQUIRE(ret.IsStatic() == klass.IsStatic());
        TEST_REQUIRE(ret.GetCurveType() == gfx::CurveClass::CurveType::PolylineSpline);
        TEST_REQUIRE(ret.GetJointStyle() == gfx::CurveClass::JointStyle::Round);
        TEST_REQUIRE(ret.GetEndCapStyle() == gfx::CurveClass::EndCapStyle::Butt);
        TEST_REQUIRE(ret.GetHash() == klass.GetHash());

        TEST_REQUIRE(ret.GetPointCount() == 3);
        for (size_t i=0; i<ret.GetPointCount(); ++i)
        {
            const auto& expected = klass.GetPoint(i);
            const auto& actual = ret.GetPoint(i);
            TEST_REQUIRE(real::float32(actual.GetPosition().x) == real::float32(expected.GetPosition().x));
            TEST_REQUIRE(real::float32(actual.GetPosition().y) == real::float32(expected.GetPosition().y));
            TEST_REQUIRE(real::float32(actual.GetData().x) == real::float32(expected.GetData().x));
            TEST_REQUIRE(real::float32(actual.GetData().y) == real::float32(expected.GetData().y));
            TEST_REQUIRE(real::float32(actual.GetWidth()) == real::float32(expected.GetWidth()));
        }
    }

    // test copy ctor/assignment
    {
        gfx::CurveClass copy(klass);
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetPointCount() == 3);

        copy = klass;
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetPointCount() == 3);
    }

    // test virtual copy/clone
    {
        auto copy = klass.Copy();
        TEST_REQUIRE(copy->GetId() == klass.GetId());
        TEST_REQUIRE(copy->GetHash() == klass.GetHash());

        auto clone = klass.Clone();
        TEST_REQUIRE(clone->GetId() != klass.GetId());
        TEST_REQUIRE(clone->GetHash() != klass.GetHash());
    }
}

void unit_test_curve_catmull_rom_tessellation()
{
    TEST_CASE(test::Type::Feature)

    // straight line along the x axis with constant width.
    gfx::CurveClass klass;
    klass.SetCurveType(gfx::CurveClass::CurveType::CatmullRomSpline);
    klass.AddPoint(gfx::CurvePoint({0.0f, 0.0f}, {0.0f, 0.0f}, 0.2f));
    klass.AddPoint(gfx::CurvePoint({1.0f, 0.0f}, {0.0f, 0.0f}, 0.2f));
    klass.AddPoint(gfx::CurvePoint({2.0f, 0.0f}, {0.0f, 0.0f}, 0.2f));
    klass.AddPoint(gfx::CurvePoint({3.0f, 0.0f}, {0.0f, 0.0f}, 0.2f));

    // butt end caps
    {
        klass.SetEndCapStyle(gfx::CurveClass::EndCapStyle::Butt);

        gfx::GeometryBuffer buffer;
        TEST_REQUIRE(klass.Construct(&buffer));
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::TriangleStrip);
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::Vertex2D>());

        const gfx::VertexStream stream(buffer.GetLayout(),
                                       buffer.GetVertexDataPtr(),
                                       buffer.GetVertexBytes());
        TEST_REQUIRE(stream.GetCount() == (3 * 16 + 1) * 2);

        // the strip should be two parallel lines at +- half width
        for (size_t i=0; i<stream.GetCount(); ++i)
        {
            const auto* vertex = stream.GetVertex<gfx::Vertex2D>(i);
            TEST_REQUIRE(real::float32(std::abs(vertex->aPosition.y)) == real::float32(0.1f));
        }

        const auto* first = stream.GetVertex<gfx::Vertex2D>(0);
        const auto* last = stream.GetVertex<gfx::Vertex2D>(stream.GetCount() - 1);
        TEST_REQUIRE(real::float32(first->aPosition.x) == real::float32(0.0f));
        TEST_REQUIRE(real::float32(last->aPosition.x) == real::float32(3.0f));
        TEST_REQUIRE(real::float32(first->aTexCoord.x) == real::float32(0.0f));
        TEST_REQUIRE(real::float32(last->aTexCoord.x) == real::float32(1.0f));
    }

    // square end caps extend the curve ends by half width
    {
        klass.SetEndCapStyle(gfx::CurveClass::EndCapStyle::Square);

        gfx::GeometryBuffer buffer;
        TEST_REQUIRE(klass.Construct(&buffer));

        const gfx::VertexStream stream(buffer.GetLayout(),
                                       buffer.GetVertexDataPtr(),
                                       buffer.GetVertexBytes());
        const auto* first = stream.GetVertex<gfx::Vertex2D>(0);
        const auto* last = stream.GetVertex<gfx::Vertex2D>(stream.GetCount() - 1);
        TEST_REQUIRE(real::float32(first->aPosition.x) == real::float32(-0.1f));
        TEST_REQUIRE(real::float32(last->aPosition.x) == real::float32(3.1f));
    }

    // round end caps are half circles around the curve end points
    {
        klass.SetEndCapStyle(gfx::CurveClass::EndCapStyle::Round);

        gfx::GeometryBuffer buffer;
        TEST_REQUIRE(klass.Construct(&buffer));
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::TriangleStrip);

        const gfx::VertexStream stream(buffer.GetLayout(),
                                       buffer.GetVertexDataPtr(),
                                       buffer.GetVertexBytes());
        // curve vertices + tip and 7 vertex pairs per cap
        TEST_REQUIRE(stream.GetCount() == (3 * 16 + 1) * 2 + 2 * (1 + 7 * 2));

        // the strip starts and ends with the cap tips
        const auto* first = stream.GetVertex<gfx::Vertex2D>(0);
        const auto* last = stream.GetVertex<gfx::Vertex2D>(stream.GetCount() - 1);
        TEST_REQUIRE(real::float32(first->aPosition.x) == real::float32(-0.1f));
        TEST_REQUIRE(real::float32(first->aPosition.y) == real::float32(0.0f));
        TEST_REQUIRE(real::float32(last->aPosition.x) == real::float32(3.1f));
        TEST_REQUIRE(real::float32(last->aPosition.y) == real::float32(0.0f));
        TEST_REQUIRE(real::float32(first->aTexCoord.y) == real::float32(0.5f));
        TEST_REQUIRE(real::float32(last->aTexCoord.y) == real::float32(0.5f));

        // the cap vertices are on a half circle with the radius of half
        // the curve width around the curve end points and behind/ahead
        // of the curve end points.
        for (size_t i=0; i<1 + 7 * 2; ++i)
        {
            const auto& start = stream.GetVertex<gfx::Vertex2D>(i)->aPosition;
            const auto& end = stream.GetVertex<gfx::Vertex2D>(stream.GetCount() - 1 - i)->aPosition;
            TEST_REQUIRE(std::abs(glm::length(glm::vec2(start.x, start.y)) - 0.1f) < 0.0001f);
            TEST_REQUIRE(std::abs(glm::length(glm::vec2(end.x - 3.0f, end.y)) - 0.1f) < 0.0001f);
            TEST_REQUIRE(start.x < 0.0f);
            TEST_REQUIRE(end.x > 3.0f);
        }
    }

    // too few points produces no geometry
    {
        gfx::CurveClass klass;
        klass.AddPoint(gfx::CurvePoint({0.0f, 0.0f}, {0.0f, 0.0f}, 0.2f));
        klass.AddPoint(gfx::CurvePoint({1.0f, 0.0f}, {0.0f, 0.0f}, 0.2f));

        gfx::GeometryBuffer buffer;
        TEST_REQUIRE(!klass.Construct(&buffer));
        TEST_REQUIRE(buffer.GetVertexBytes() == 0);
    }

    // all triangles in the strip must have counter-clockwise winding
    // regardless of which way the curve bends.
    for (auto cap : {gfx::CurveClass::EndCapStyle::Butt,
                     gfx::CurveClass::EndCapStyle::Square,
                     gfx::CurveClass::EndCapStyle::Round})
    {
        gfx::CurveClass klass;
        klass.SetEndCapStyle(cap);
        klass.AddPoint(gfx::CurvePoint({0.0f, 0.0f}, {0.0f, 0.0f}, 0.1f));
        klass.AddPoint(gfx::CurvePoint({1.0f, 1.0f}, {0.0f, 0.0f}, 0.1f));
        klass.AddPoint(gfx::CurvePoint({2.0f, 0.0f}, {0.0f, 0.0f}, 0.1f));
        klass.AddPoint(gfx::CurvePoint({3.0f, -1.0f}, {0.0f, 0.0f}, 0.1f));
        klass.AddPoint(gfx::CurvePoint({4.0f, 0.0f}, {0.0f, 0.0f}, 0.1f));

        gfx::GeometryBuffer buffer;
        TEST_REQUIRE(klass.Construct(&buffer));

        const gfx::VertexStream stream(buffer.GetLayout(),
                                       buffer.GetVertexDataPtr(),
                                       buffer.GetVertexBytes());
        const gfx::TriangleIterator triangles(buffer.GetDrawCommands(), stream);
        TEST_REQUIRE(triangles.GetCount() == stream.GetCount() - 2);

        for (size_t i=0; i<triangles.GetCount(); ++i)
        {
            const auto& triangle = triangles.GetTriangle(i);
            const auto& v0 = stream.GetVertex<gfx::Vertex2D>(triangle.i0)->aPosition;
            const auto& v1 = stream.GetVertex<gfx::Vertex2D>(triangle.i1)->aPosition;
            const auto& v2 = stream.GetVertex<gfx::Vertex2D>(triangle.i2)->aPosition;
            const auto signed_area = (v1.x - v0.x) * (v2.y - v0.y) -
                                     (v2.x - v0.x) * (v1.y - v0.y);
            TEST_REQUIRE(signed_area > 0.0f);
        }
    }

    // coincident control points fail
    {
        gfx::CurveClass klass;
        klass.AddPoint(gfx::CurvePoint({0.0f, 0.0f}, {0.0f, 0.0f}, 0.2f));
        klass.AddPoint(gfx::CurvePoint({1.0f, 0.0f}, {0.0f, 0.0f}, 0.2f));
        klass.AddPoint(gfx::CurvePoint({1.0f, 0.0f}, {0.0f, 0.0f}, 0.2f));
        klass.AddPoint(gfx::CurvePoint({3.0f, 0.0f}, {0.0f, 0.0f}, 0.2f));

        gfx::GeometryBuffer buffer;
        TEST_REQUIRE(!klass.Construct(&buffer));
    }
}

namespace {
struct PolylineResult {
    std::vector<glm::vec2> positions;
    std::vector<glm::vec2> texcoords;
    // sum of the (absolute) triangle areas
    float area = 0.0f;
    // true if all triangles are counter-clockwise
    bool all_ccw = true;
    size_t triangle_count = 0;
};

bool TessellatePolyline(const std::vector<gfx::CurvePoint>& points,
                        gfx::CurveClass::JointStyle joint,
                        gfx::CurveClass::EndCapStyle cap,
                        PolylineResult* result)
{
    gfx::CurveClass klass;
    klass.SetCurveType(gfx::CurveClass::CurveType::PolylineSpline);
    klass.SetJointStyle(joint);
    klass.SetEndCapStyle(cap);
    for (const auto& point : points)
        klass.AddPoint(point);

    gfx::GeometryBuffer buffer;
    if (!klass.Construct(&buffer))
        return false;

    TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
    TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Triangles);
    TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::Vertex2D>());

    const gfx::VertexStream stream(buffer.GetLayout(),
                                   buffer.GetVertexDataPtr(),
                                   buffer.GetVertexBytes());
    TEST_REQUIRE(stream.GetCount() % 3 == 0);

    for (size_t i=0; i<stream.GetCount(); ++i)
    {
        const auto* vertex = stream.GetVertex<gfx::Vertex2D>(i);
        result->positions.push_back({vertex->aPosition.x, vertex->aPosition.y});
        result->texcoords.push_back({vertex->aTexCoord.x, vertex->aTexCoord.y});
    }
    result->triangle_count = stream.GetCount() / 3;
    for (size_t i=0; i<stream.GetCount(); i+=3)
    {
        const auto& v0 = result->positions[i+0];
        const auto& v1 = result->positions[i+1];
        const auto& v2 = result->positions[i+2];
        const auto signed_area = 0.5f * ((v1.x - v0.x) * (v2.y - v0.y) -
                                         (v2.x - v0.x) * (v1.y - v0.y));
        if (signed_area <= 0.0f)
            result->all_ccw = false;
        result->area += std::abs(signed_area);
    }
    return true;
}

bool Near(float a, float b, float eps = 0.0001f)
{
    return std::abs(a - b) < eps;
}
} // namespace

void unit_test_curve_polyline_tessellation()
{
    TEST_CASE(test::Type::Feature)

    using Joint = gfx::CurveClass::JointStyle;
    using Cap = gfx::CurveClass::EndCapStyle;
    using Point = gfx::CurvePoint;

    const auto P = [](float x, float y, float width) {
        return Point({x, y}, {0.0f, 0.0f}, width);
    };

    // a straight line with butt caps is a single quad
    {
        PolylineResult r;
        TEST_REQUIRE(TessellatePolyline({P(0.0f, 0.0f, 0.2f), P(1.0f, 0.0f, 0.2f)}, Joint::Miter, Cap::Butt, &r));
        TEST_REQUIRE(r.triangle_count == 2);
        TEST_REQUIRE(r.all_ccw);
        TEST_REQUIRE(Near(r.area, 0.2f));
        for (size_t i=0; i<r.positions.size(); ++i)
        {
            TEST_REQUIRE(Near(std::abs(r.positions[i].y), 0.1f));
            TEST_REQUIRE(r.positions[i].x >= 0.0f && r.positions[i].x <= 1.0f);
            // u goes from 0 to 1 along the line, v is 0 on the left and 1 on the right.
            TEST_REQUIRE(Near(r.texcoords[i].x, r.positions[i].x));
            TEST_REQUIRE(Near(r.texcoords[i].y, r.positions[i].y > 0.0f ? 0.0f : 1.0f));
        }
    }

    // square caps extend the line by half the width at both ends.
    {
        PolylineResult r;
        TEST_REQUIRE(TessellatePolyline({P(0.0f, 0.0f, 0.2f), P(1.0f, 0.0f, 0.2f)}, Joint::Miter, Cap::Square, &r));
        TEST_REQUIRE(r.all_ccw);
        TEST_REQUIRE(Near(r.area, 1.2f * 0.2f));
        float min_x = 100.0f, max_x = -100.0f;
        for (const auto& p : r.positions)
        {
            min_x = std::min(min_x, p.x);
            max_x = std::max(max_x, p.x);
        }
        TEST_REQUIRE(Near(min_x, -0.1f));
        TEST_REQUIRE(Near(max_x, 1.1f));
    }

    // round caps are half circles around the line ends.
    {
        PolylineResult r;
        TEST_REQUIRE(TessellatePolyline({P(0.0f, 0.0f, 0.2f), P(1.0f, 0.0f, 0.2f)}, Joint::Miter, Cap::Round, &r));
        TEST_REQUIRE(r.all_ccw);
        // 16 segments in a half circle at both ends
        TEST_REQUIRE(r.triangle_count == 2 + 2 * 16);
        // area of a regular polygon with 32 sides and radius of 0.1 is the area of two caps.
        const float circle = 0.5f * 0.1f * 0.1f * 32.0f * std::sin(2.0f * float(math::Pi) / 32.0f);
        TEST_REQUIRE(Near(r.area, 0.2f + circle));
        for (const auto& p : r.positions)
        {
            TEST_REQUIRE(p.x >= -0.1f - 0.0001f && p.x <= 1.1f + 0.0001f);
            if (p.x < 0.0f)
                TEST_REQUIRE(Near(glm::length(p), 0.1f));
            if (p.x > 1.0f)
                TEST_REQUIRE(Near(glm::length(p - glm::vec2(1.0f, 0.0f)), 0.1f));
        }
    }

    // the line direction and the draw order of the points does not matter
    // for the winding or area.
    {
        PolylineResult r;
        TEST_REQUIRE(TessellatePolyline({P(1.0f, 1.0f, 0.2f), P(0.0f, 0.0f, 0.2f)}, Joint::Miter, Cap::Butt, &r));
        TEST_REQUIRE(r.all_ccw);
        TEST_REQUIRE(Near(r.area, std::sqrt(2.0f) * 0.2f));
    }

    // a 90 degree left turn. the inner side of the turn is trimmed so the
    // segments don't overlap and the total area is the exact area of the
    // shape. (the horizontal bar (0 -> 1.1, 0.2 high) and the vertical bar (0.2 wide, 0.9 high))
    const std::vector<Point> left_turn = {P(0.0f, 0.0f, 0.2f), P(1.0f, 0.0f, 0.2f), P(1.0f, 1.0f, 0.2f)};
    // and the same turn in the other direction.
    const std::vector<Point> right_turn = {P(0.0f, 0.0f, 0.2f), P(1.0f, 0.0f, 0.2f), P(1.0f, -1.0f, 0.2f)};

    for (const auto& points : {left_turn, right_turn})
    {
        // bevel: the outer corner is cut off. (a triangle of 0.1 x 0.1)
        {
            PolylineResult r;
            TEST_REQUIRE(TessellatePolyline(points, Joint::Bevel, Cap::Butt, &r));
            TEST_REQUIRE(r.all_ccw);
            TEST_REQUIRE(Near(r.area, 0.4f - 0.005f));
        }
        // miter: the outer corner is a sharp corner.
        {
            PolylineResult r;
            TEST_REQUIRE(TessellatePolyline(points, Joint::Miter, Cap::Butt, &r));
            TEST_REQUIRE(r.all_ccw);
            TEST_REQUIRE(Near(r.area, 0.4f));
        }
        // round: the outer corner is a quarter circle around the corner point.
        {
            PolylineResult r;
            TEST_REQUIRE(TessellatePolyline(points, Joint::Round, Cap::Butt, &r));
            TEST_REQUIRE(r.all_ccw);
            const float sector = 0.5f * 0.1f * 0.1f * 8.0f * std::sin(float(math::Pi) * 0.5f / 8.0f);
            TEST_REQUIRE(Near(r.area, 0.4f - 0.01f + sector));
            // all the vertices around the corner are within the half width of the corner
            // or in the bar areas.
            for (const auto& p : r.positions)
            {
                const bool in_horizontal = p.x >= -0.0001f && p.x <= 1.1001f && std::abs(p.y) <= 0.1001f;
                const bool in_vertical = std::abs(p.x - 1.0f) <= 0.1001f && std::abs(p.y) <= 1.0001f;
                TEST_REQUIRE(in_horizontal || in_vertical);
            }
        }
    }

    // the miter joint is replaced with a bevel when the turn is so sharp
    // that the miter would extend too far.
    {
        PolylineResult miter, bevel;
        const std::vector<Point> sharp = {P(0.0f, 0.0f, 0.2f), P(1.0f, 0.0f, 0.2f), P(0.0f, 0.05f, 0.2f)};
        TEST_REQUIRE(TessellatePolyline(sharp, Joint::Miter, Cap::Butt, &miter));
        TEST_REQUIRE(TessellatePolyline(sharp, Joint::Bevel, Cap::Butt, &bevel));
        TEST_REQUIRE(miter.all_ccw);
        TEST_REQUIRE(Near(miter.area, bevel.area));
        // and none of the vertices are far from the joint
        for (const auto& p : miter.positions)
            TEST_REQUIRE(p.x <= 1.0f + 0.1001f);
    }

    // a gentle turn uses a proper miter.
    {
        PolylineResult miter, bevel;
        const std::vector<Point> gentle = {P(0.0f, 0.0f, 0.2f), P(1.0f, 0.0f, 0.2f), P(2.0f, 0.5f, 0.2f)};
        TEST_REQUIRE(TessellatePolyline(gentle, Joint::Miter, Cap::Butt, &miter));
        TEST_REQUIRE(TessellatePolyline(gentle, Joint::Bevel, Cap::Butt, &bevel));
        TEST_REQUIRE(miter.all_ccw);
        TEST_REQUIRE(miter.area > bevel.area);
    }

    // the line turns back on itself.
    {
        const std::vector<Point> reversal = {P(0.0f, 0.0f, 0.2f), P(1.0f, 0.0f, 0.2f), P(0.0f, 0.0f, 0.2f)};
        PolylineResult bevel, round;
        TEST_REQUIRE(TessellatePolyline(reversal, Joint::Bevel, Cap::Butt, &bevel));
        TEST_REQUIRE(TessellatePolyline(reversal, Joint::Round, Cap::Butt, &round));
        TEST_REQUIRE(bevel.all_ccw);
        TEST_REQUIRE(round.all_ccw);
        // a round joint makes a half circle around the tip.
        TEST_REQUIRE(round.triangle_count == bevel.triangle_count + 16);
        float max_x = -100.0f;
        for (const auto& p : round.positions)
            max_x = std::max(max_x, p.x);
        TEST_REQUIRE(Near(max_x, 1.1f));
    }

    // collinear points don't make a joint
    {
        PolylineResult line, split;
        TEST_REQUIRE(TessellatePolyline({P(0.0f, 0.0f, 0.2f), P(2.0f, 0.0f, 0.2f)}, Joint::Round, Cap::Butt, &line));
        TEST_REQUIRE(TessellatePolyline({P(0.0f, 0.0f, 0.2f), P(1.0f, 0.0f, 0.2f), P(2.0f, 0.0f, 0.2f)}, Joint::Round, Cap::Butt, &split));
        TEST_REQUIRE(split.all_ccw);
        TEST_REQUIRE(Near(line.area, split.area));
        TEST_REQUIRE(split.triangle_count == 4);
    }

    // coincident points are ignored.
    {
        PolylineResult r;
        TEST_REQUIRE(TessellatePolyline({P(0.0f, 0.0f, 0.2f), P(0.0f, 0.0f, 0.2f), P(1.0f, 0.0f, 0.2f), P(1.0f, 0.0f, 0.2f)},
                                        Joint::Miter, Cap::Butt, &r));
        TEST_REQUIRE(r.triangle_count == 2);
        TEST_REQUIRE(Near(r.area, 0.2f));
    }

    // the line width is interpolated between the points
    {
        PolylineResult r;
        TEST_REQUIRE(TessellatePolyline({P(0.0f, 0.0f, 0.2f), P(1.0f, 0.0f, 0.6f)}, Joint::Miter, Cap::Butt, &r));
        TEST_REQUIRE(r.all_ccw);
        TEST_REQUIRE(Near(r.area, 0.4f));
        for (size_t i=0; i<r.positions.size(); ++i)
        {
            const auto expected = r.positions[i].x < 0.5f ? 0.1f : 0.3f;
            TEST_REQUIRE(Near(std::abs(r.positions[i].y), expected));
        }
    }

    // the texture u coordinate follows the distance along the line
    {
        PolylineResult r;
        TEST_REQUIRE(TessellatePolyline({P(0.0f, 0.0f, 0.2f), P(1.0f, 0.0f, 0.2f), P(1.0f, 3.0f, 0.2f)},
                                        Joint::Bevel, Cap::Butt, &r));
        for (size_t i=0; i<r.positions.size(); ++i)
        {
            const auto& p = r.positions[i];
            const auto u = r.texcoords[i].x;
            TEST_REQUIRE(u >= -0.0001f && u <= 1.0001f);
            if (p.y < -0.0999f && p.x < 0.9f)
                TEST_REQUIRE(Near(u, p.x / 4.0f));
            if (p.y > 2.9999f)
                TEST_REQUIRE(Near(u, 1.0f));
        }
    }

    // all the triangles are counter-clockwise for all the combinations
    // of joints and caps and for all kinds of turns.
    for (auto joint : {Joint::Miter, Joint::Bevel, Joint::Round})
    {
        for (auto cap : {Cap::Butt, Cap::Square, Cap::Round})
        {
            const std::vector<Point> zigzag = {
                P(0.0f, 0.0f, 0.1f), P(1.0f, 1.0f, 0.2f), P(2.0f, 0.0f, 0.1f),
                P(3.0f, -1.0f, 0.3f), P(4.0f, 0.0f, 0.1f), P(4.0f, 2.0f, 0.1f),
                P(1.0f, 2.0f, 0.1f), P(1.0f, 2.1f, 0.1f), P(0.5f, 3.0f, 0.1f)
            };
            PolylineResult r;
            TEST_REQUIRE(TessellatePolyline(zigzag, joint, cap, &r));
            TEST_REQUIRE(r.all_ccw);
            TEST_REQUIRE(r.triangle_count > 0);
        }
    }

    // short segments that can't be trimmed (the width is large compared
    // to the segment length) still work.
    for (auto joint : {Joint::Miter, Joint::Bevel, Joint::Round})
    {
        const std::vector<Point> tight = {P(0.0f, 0.0f, 1.0f), P(0.1f, 0.0f, 1.0f), P(0.1f, 0.1f, 1.0f), P(0.2f, 0.1f, 1.0f)};
        PolylineResult r;
        TEST_REQUIRE(TessellatePolyline(tight, joint, Cap::Round, &r));
        TEST_REQUIRE(r.all_ccw);
    }

    // too few (distinct) points or zero width produces no geometry
    {
        PolylineResult r;
        TEST_REQUIRE(!TessellatePolyline({}, Joint::Miter, Cap::Butt, &r));
        TEST_REQUIRE(!TessellatePolyline({P(0.0f, 0.0f, 0.2f)}, Joint::Miter, Cap::Butt, &r));
        TEST_REQUIRE(!TessellatePolyline({P(1.0f, 1.0f, 0.2f), P(1.0f, 1.0f, 0.2f)}, Joint::Miter, Cap::Butt, &r));
        TEST_REQUIRE(!TessellatePolyline({P(0.0f, 0.0f, 0.0f), P(1.0f, 0.0f, 0.0f)}, Joint::Miter, Cap::Butt, &r));
    }

    // NormalizeCurve flips y and normalizes the curve points into the unit box.
    {
        gfx::CurveArgs args;
        args.curve_type = gfx::CurveType::PolylineSpline;
        args.joint_style = gfx::CurveJointStyle::Miter;
        args.end_cap_style = gfx::CurveEndCapStyle::Butt;

        auto Tessellate = [&args](const std::vector<Point>& points, std::vector<glm::vec2>* positions) {
            auto curve_args = args;
            curve_args.points = points;
            gfx::CurveInstance curve(std::move(curve_args));
            gfx::Drawable::Environment env;
            env.mesh_type = gfx::Drawable::MeshType::PaintMesh;
            auto geometry = curve.Construct(env);
            TEST_REQUIRE(!geometry.IsNull());
            const auto& buffer = geometry.GetGeometryBuffer();
            TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
            TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Triangles);
            const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
            float area = 0.0f;
            for (size_t i=0; i<stream.GetCount(); ++i)
            {
                const auto& pos = stream.GetVertex<gfx::Vertex2D>(i)->aPosition;
                positions->push_back({pos.x, pos.y});
            }
            for (size_t i=0; i<positions->size(); i+=3)
            {
                const auto& v0 = (*positions)[i+0];
                const auto& v1 = (*positions)[i+1];
                const auto& v2 = (*positions)[i+2];
                area += 0.5f * ((v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y));
            }
            return area;
        };
        const auto Bounds = [](const std::vector<glm::vec2>& positions) {
            glm::vec2 min = {100.0f, 100.0f};
            glm::vec2 max = {-100.0f, -100.0f};
            for (const auto& p : positions)
            {
                min = glm::min(min, p);
                max = glm::max(max, p);
            }
            return std::make_pair(min, max);
        };

        const std::vector<Point> points = {P(0.0f, 0.0f, 0.2f), P(100.0f, 0.0f, 0.2f), P(100.0f, 50.0f, 0.2f)};

        std::vector<glm::vec2> plain;
        const auto plain_area = Tessellate(points, &plain);
        TEST_REQUIRE(plain_area > 0.0f);

        // the tessellation is CCW also for curve points that have been flipped on y.
        {
            auto flipped_points = points;
            for (auto& p : flipped_points)
                p.SetPosition({p.GetPosition().x, -p.GetPosition().y});
            std::vector<glm::vec2> flipped;
            const auto flipped_area = Tessellate(flipped_points, &flipped);
            TEST_REQUIRE(Near(flipped_area, plain_area, 0.001f));
        }

        // normalization fits the (longer side of the) curve bounds into the unit
        // box [0, 1] x [-1, 0] and centers the shorter side. the bounds are computed
        // from the point positions and the point widths are left unchanged.
        for (bool flip : {false, true})
        {
            auto normalized_points = points;
            gfx::NormalizeCurve(normalized_points, flip);
            // the horizontal extent is 100 and the vertical extent is 50
            const auto scale = 100.0f;
            TEST_REQUIRE(Near(normalized_points[0].GetPosition().x, 0.0f));
            TEST_REQUIRE(Near(normalized_points[1].GetPosition().x, 1.0f));
            TEST_REQUIRE(Near(normalized_points[0].GetWidth(), 0.2f));
            // the orientation is preserved, y grows up unless flipped.
            const auto dy = normalized_points[2].GetPosition().y - normalized_points[0].GetPosition().y;
            TEST_REQUIRE(Near(dy, (flip ? -50.0f : 50.0f) / scale, 0.00001f));

            // the widths are not scaled so the line thickness (0.2) extends half of
            // the width outside the unit box at the outer side of the miter joint.
            // the curve is centered vertically, 0.5 high, between y = -0.75 and y = -0.25.
            std::vector<glm::vec2> normalized;
            const auto normalized_area = Tessellate(normalized_points, &normalized);
            TEST_REQUIRE(normalized_area > 0.0f);
            const auto [min, max] = Bounds(normalized);
            TEST_REQUIRE(Near(min.x, 0.0f) && Near(max.x, 1.1f));
            TEST_REQUIRE(Near(min.y, flip ? -0.75f : -0.85f));
            TEST_REQUIRE(Near(max.y, flip ? -0.15f : -0.25f));
        }

        // degenerate curves. no points does nothing and a zero size curve collapses to the center.
        {
            std::vector<Point> none;
            gfx::NormalizeCurve(none, true);
            TEST_REQUIRE(none.empty());

            std::vector<Point> single = {P(5.0f, 7.0f, 0.0f)};
            gfx::NormalizeCurve(single, false);
            TEST_REQUIRE(Near(single[0].GetPosition().x, 0.5f));
            TEST_REQUIRE(Near(single[0].GetPosition().y, -0.5f));
        }
    }
}

void unit_test_simple_shape_mesh()
{
    TEST_CASE(test::Type::Feature)

    // normal  paint mesh
    {
        auto klass = std::make_shared<gfx::RectangleClass>();
        gfx::RectangleClassInstance rect(klass);

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::PaintMesh;

        auto geometry = rect.Construct(env);
        TEST_REQUIRE(!geometry.IsNull());

        const auto& buffer = geometry.GetGeometryBuffer();
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::Vertex2D>());
        TEST_REQUIRE(buffer.GetVertexCount() == 6);
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Triangles);

        // rectangle solid mesh: two triangles, vertices in model space
        // triangle 0
        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(0)->aPosition == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(1)->aPosition == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(2)->aPosition == gfx::ToVec(1.0f, -1.0f));
        // triangle 1
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(3)->aPosition == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(4)->aPosition == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(5)->aPosition == gfx::ToVec(1.0f,  0.0f));

        // texture coordinates
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(0)->aTexCoord == gfx::ToVec(0.0f, 0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(1)->aTexCoord == gfx::ToVec(0.0f, 1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(2)->aTexCoord == gfx::ToVec(1.0f, 1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(3)->aTexCoord == gfx::ToVec(0.0f, 0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(4)->aTexCoord == gfx::ToVec(1.0f, 1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(5)->aTexCoord == gfx::ToVec(1.0f, 0.0f));
    }

    // wireframe mesh
    {
        auto klass = std::make_shared<gfx::RectangleClass>();
        gfx::RectangleClassInstance rect(klass);

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::Wireframe;

        auto geometry = rect.Construct(env);
        TEST_REQUIRE(!geometry.IsNull());

        const auto& buffer = geometry.GetGeometryBuffer();
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::Vertex2D>());
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Lines);

        // each triangle produces 3 lines (2 verts each), 2 triangles = 12 vertices
        TEST_REQUIRE(buffer.GetVertexCount() == 12);

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());

        // triangle 0: (0,0), (0,-1), (1,-1)
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>( 0)->aPosition == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>( 1)->aPosition == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>( 2)->aPosition == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>( 3)->aPosition == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>( 4)->aPosition == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>( 5)->aPosition == gfx::ToVec(0.0f,  0.0f));

        // triangle 1: (0,0), (1,-1), (1,0)
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>( 6)->aPosition == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>( 7)->aPosition == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>( 8)->aPosition == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>( 9)->aPosition == gfx::ToVec(1.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(10)->aPosition == gfx::ToVec(1.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(11)->aPosition == gfx::ToVec(0.0f,  0.0f));
    }

    // 2D debug mesh
    {
        auto klass = std::make_shared<gfx::RectangleClass>();
        gfx::RectangleClassInstance rect(klass);

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::DebugMesh;

        auto geometry = rect.Construct(env);
        TEST_REQUIRE(geometry.IsNull());
    }

    // 3D debug mesh
    {
        auto klass = std::make_shared<gfx::CubeClass>();
        gfx::SimpleShapeInstance cube(klass);

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::DebugMesh;
        env.mesh_flags.set(gfx::Drawable::MeshFlags::DebugNormals, true);

        auto geometry = cube.Construct(env);
        TEST_REQUIRE(!geometry.IsNull());

        const auto& buffer = geometry.GetGeometryBuffer();
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::Vertex3D>());
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Lines);

        // cube has 24 vertices (4 per face x 6 faces), normals-only debug mesh
        // produces 2 output vertices per input vertex: 24 * 2 = 48
        TEST_REQUIRE(buffer.GetVertexCount() == 48);

        // verify the front face normal lines.
        // front face vertices (indices 0-3) all have normal {0, 0, 1}.
        // each normal line: start = vertex position, end = position + normal * 0.2f
        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());

        // vertex 0: FrontTopLeft {-0.5, 0.5, 0.5}
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(0)->aPosition == gfx::ToVec(-0.5f,  0.5f, 0.5f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(1)->aPosition == gfx::ToVec(-0.5f,  0.5f, 0.7f));
        // vertex 1: FrontBotLeft {-0.5, -0.5, 0.5}
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(2)->aPosition == gfx::ToVec(-0.5f, -0.5f, 0.5f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(3)->aPosition == gfx::ToVec(-0.5f, -0.5f, 0.7f));
        // vertex 2: FrontBotRight {0.5, -0.5, 0.5}
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(4)->aPosition == gfx::ToVec( 0.5f, -0.5f, 0.5f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(5)->aPosition == gfx::ToVec( 0.5f, -0.5f, 0.7f));
        // vertex 3: FrontTopRight {0.5, 0.5, 0.5}
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(6)->aPosition == gfx::ToVec( 0.5f,  0.5f, 0.5f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex3D>(7)->aPosition == gfx::ToVec( 0.5f,  0.5f, 0.7f));
    }

    // effect paint mesh
    {
        auto klass = std::make_shared<gfx::RectangleClass>();

        gfx::DrawableEffect::MeshExplosion explosion;
        explosion.mesh_subdivision_count = 0;// should produce two triangles

        gfx::RectangleClassInstance rect(klass);
        rect.SetEffect(gfx::DrawableEffect(explosion));

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::PaintMesh;

        auto geometry = rect.Construct(env);
        TEST_REQUIRE(!geometry.IsNull());
        const auto& buffer = geometry.GetGeometryBuffer();

        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::ShardVertex2D>());
        const gfx::VertexStream stream(buffer.GetLayout(),  buffer.GetVertexBuffer());
        TEST_REQUIRE(stream.GetCount() == 6);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(0)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(1)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(2)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(3)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(4)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(5)->aShardIndex == 1);

    }

    // effect wireframe mesh
    {
        auto klass = std::make_shared<gfx::RectangleClass>();

        gfx::DrawableEffect::MeshExplosion explosion;
        explosion.mesh_subdivision_count = 0; // should produce two triangles

        gfx::RectangleClassInstance rect(klass);
        rect.SetEffect(gfx::DrawableEffect(explosion));

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::Wireframe;

        auto geometry = rect.Construct(env);
        TEST_REQUIRE(!geometry.IsNull());

        const auto& buffer = geometry.GetGeometryBuffer();
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::ShardVertex2D>());
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Lines);

        // each shard triangle produces 3 lines (2 verts each), 2 shards = 12 vertices
        TEST_REQUIRE(buffer.GetVertexCount() == 12);

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());

        // shard 0 triangle: (0,0), (0,-1), (1,-1) — all verts carry shard index 0
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 0)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 0)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 1)->aPosition   == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 1)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 2)->aPosition   == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 2)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 3)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 3)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 4)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 4)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 5)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 5)->aShardIndex == 0);

        // shard 1 triangle: (0,0), (1,-1), (1,0) — all verts carry shard index 1
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 6)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 6)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 7)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 7)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 8)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 8)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 9)->aPosition   == gfx::ToVec(1.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 9)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(10)->aPosition   == gfx::ToVec(1.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(10)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(11)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(11)->aShardIndex == 1);
    }
}

void unit_test_polygon_simple_render_mesh()
{
    TEST_CASE(test::Type::Feature)

    // Single triangle mesh: v0=(0,0), v1=(0,-1), v2=(1,-1)
    const gfx::Vertex2D verts[3] = {
        { {0.0f,  0.0f}, {0.0f, 0.0f} },
        { {0.0f, -1.0f}, {0.0f, 1.0f} },
        { {1.0f, -1.0f}, {1.0f, 1.0f} },
    };
    gfx::VertexBuffer vertex_buffer;
    vertex_buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    vertex_buffer.Resize(3);
    for (int i=0; i<3; ++i)
        vertex_buffer.SetVertex(verts[i], i);

    std::vector<gfx::Geometry::DrawCommand> cmds(1);
    cmds[0].type = gfx::Geometry::DrawType::Triangles;
    cmds[0].count = 3;

    auto klass = std::make_shared<gfx::PolygonMeshClass>();
    klass->SetMeshType(gfx::PolygonMeshClass::MeshType::Simple2DRenderMesh);
    klass->SetVertexBuffer(std::move(vertex_buffer));
    klass->SetCommandBuffer(std::move(cmds));

    // paint mesh
    {
        gfx::PolygonMeshInstance mesh(klass);

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::PaintMesh;

        auto geometry = mesh.Construct(env);
        TEST_REQUIRE(!geometry.IsNull());

        const auto& buffer = geometry.GetGeometryBuffer();
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::Vertex2D>());
        TEST_REQUIRE(buffer.GetVertexCount() == 3);
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Triangles);

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(0)->aPosition == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(1)->aPosition == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(2)->aPosition == gfx::ToVec(1.0f, -1.0f));
    }

    // wireframe mesh
    {
        gfx::PolygonMeshInstance mesh(klass);

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::Wireframe;

        auto geometry = mesh.Construct(env);
        TEST_REQUIRE(!geometry.IsNull());

        const auto& buffer = geometry.GetGeometryBuffer();
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::Vertex2D>());
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Lines);

        // 1 triangle x 3 lines x 2 verts = 6 vertices
        TEST_REQUIRE(buffer.GetVertexCount() == 6);

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        // triangle: (0,0), (0,-1), (1,-1)
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(0)->aPosition == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(1)->aPosition == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(2)->aPosition == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(3)->aPosition == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(4)->aPosition == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::Vertex2D>(5)->aPosition == gfx::ToVec(0.0f,  0.0f));
    }

    // effect paint mesh
    {
        gfx::PolygonMeshInstance mesh(klass);

        gfx::DrawableEffect::MeshExplosion explosion;
        explosion.mesh_subdivision_count = 0; // should produce one triangle (no subdivision)
        mesh.SetEffect(gfx::DrawableEffect(explosion));

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::PaintMesh;

        auto geometry = mesh.Construct(env);
        TEST_REQUIRE(!geometry.IsNull());

        const auto& buffer = geometry.GetGeometryBuffer();
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::ShardVertex2D>());
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Triangles);
        TEST_REQUIRE(buffer.GetVertexCount() == 3);

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(0)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(1)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(2)->aShardIndex == 0);
    }

    // effect wireframe mesh
    {
        gfx::PolygonMeshInstance mesh(klass);

        gfx::DrawableEffect::MeshExplosion explosion;
        explosion.mesh_subdivision_count = 0; // should produce one triangle (no subdivision)
        mesh.SetEffect(gfx::DrawableEffect(explosion));

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::Wireframe;

        auto geometry = mesh.Construct(env);
        TEST_REQUIRE(!geometry.IsNull());

        const auto& buffer = geometry.GetGeometryBuffer();
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::ShardVertex2D>());
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Lines);

        // 1 shard triangle x 3 lines x 2 verts = 6 vertices
        TEST_REQUIRE(buffer.GetVertexCount() == 6);

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        // shard 0 triangle: (0,0), (0,-1), (1,-1)
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(0)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(0)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(1)->aPosition   == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(1)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(2)->aPosition   == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(2)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(3)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(3)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(4)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(4)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(5)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(5)->aShardIndex == 0);
    }
}

void unit_test_polygon_simple_shard_mesh()
{
    TEST_CASE(test::Type::Feature)

    // Two-triangle shard mesh: triangle 0 has shard index 0, triangle 1 has shard index 1.
    const gfx::ShardVertex2D verts[6] = {
        // triangle 0, shard 0
        { {0.0f,  0.0f}, {0.0f, 0.0f}, 0 },
        { {0.0f, -1.0f}, {0.0f, 1.0f}, 0 },
        { {1.0f, -1.0f}, {1.0f, 1.0f}, 0 },
        // triangle 1, shard 1
        { {0.0f,  0.0f}, {0.0f, 0.0f}, 1 },
        { {1.0f, -1.0f}, {1.0f, 1.0f}, 1 },
        { {1.0f,  0.0f}, {1.0f, 0.0f}, 1 },
    };
    gfx::VertexBuffer vertex_buffer;
    vertex_buffer.SetVertexLayout(gfx::GetVertexLayout<gfx::ShardVertex2D>());
    vertex_buffer.Resize(6);
    for (int i=0; i<6; ++i)
        vertex_buffer.SetVertex(verts[i], i);

    std::vector<gfx::Geometry::DrawCommand> cmds(2);
    cmds[0].type   = gfx::Geometry::DrawType::Triangles;
    cmds[0].count  = 3;
    cmds[0].offset = 0;
    cmds[1].type   = gfx::Geometry::DrawType::Triangles;
    cmds[1].count  = 3;
    cmds[1].offset = 3;

    auto klass = std::make_shared<gfx::PolygonMeshClass>();
    klass->SetMeshType(gfx::PolygonMeshClass::MeshType::Simple2DShardEffectMesh);
    klass->SetVertexBuffer(std::move(vertex_buffer));
    klass->SetCommandBuffer(std::move(cmds));

    // paint mesh — inline shard mesh is returned as-is
    {
        gfx::PolygonMeshInstance mesh(klass);

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::PaintMesh;

        auto geometry = mesh.Construct(env);
        TEST_REQUIRE(!geometry.IsNull());

        const auto& buffer = geometry.GetGeometryBuffer();
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::ShardVertex2D>());
        TEST_REQUIRE(buffer.GetVertexCount() == 6);
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 2);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Triangles);
        TEST_REQUIRE(buffer.GetDrawCmd(1).type == gfx::DrawType::Triangles);

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        // triangle 0 — shard 0
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(0)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(0)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(1)->aPosition   == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(1)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(2)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(2)->aShardIndex == 0);
        // triangle 1 — shard 1
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(3)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(3)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(4)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(4)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(5)->aPosition   == gfx::ToVec(1.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(5)->aShardIndex == 1);
    }

    // wireframe mesh — each triangle becomes 3 line segments preserving shard index
    {
        gfx::PolygonMeshInstance mesh(klass);

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::Wireframe;

        auto geometry = mesh.Construct(env);
        TEST_REQUIRE(!geometry.IsNull());

        const auto& buffer = geometry.GetGeometryBuffer();
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::ShardVertex2D>());
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Lines);

        // 2 triangles x 3 lines x 2 verts = 12 vertices
        TEST_REQUIRE(buffer.GetVertexCount() == 12);

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        // shard 0 triangle: (0,0) -> (0,-1) -> (1,-1) -> (0,0)
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 0)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 0)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 1)->aPosition   == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 1)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 2)->aPosition   == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 2)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 3)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 3)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 4)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 4)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 5)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 5)->aShardIndex == 0);
        // shard 1 triangle: (0,0) -> (1,-1) -> (1,0) -> (0,0)
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 6)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 6)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 7)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 7)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 8)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 8)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 9)->aPosition   == gfx::ToVec(1.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 9)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(10)->aPosition   == gfx::ToVec(1.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(10)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(11)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(11)->aShardIndex == 1);
    }

    // effect paint mesh — ShardVertex2D input passes through ConstructShardEffectMesh unchanged;
    // the original shard geometry is returned with a shard data texture buffer added
    {
        gfx::PolygonMeshInstance mesh(klass);

        gfx::DrawableEffect::MeshExplosion explosion;
        explosion.mesh_subdivision_count = 0;
        mesh.SetEffect(gfx::DrawableEffect(explosion));

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::PaintMesh;

        auto geometry = mesh.Construct(env);
        TEST_REQUIRE(!geometry.IsNull());

        const auto& buffer = geometry.GetGeometryBuffer();
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::ShardVertex2D>());
        TEST_REQUIRE(buffer.GetVertexCount() == 6);
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 2);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Triangles);
        TEST_REQUIRE(buffer.GetDrawCmd(1).type == gfx::DrawType::Triangles);

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        // triangle 0 — shard 0
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(0)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(0)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(1)->aPosition   == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(1)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(2)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(2)->aShardIndex == 0);
        // triangle 1 — shard 1
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(3)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(3)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(4)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(4)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(5)->aPosition   == gfx::ToVec(1.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(5)->aShardIndex == 1);
    }

    // effect wireframe mesh — same passthrough geometry, then wireframed
    {
        gfx::PolygonMeshInstance mesh(klass);

        gfx::DrawableEffect::MeshExplosion explosion;
        explosion.mesh_subdivision_count = 0;
        mesh.SetEffect(gfx::DrawableEffect(explosion));

        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::Wireframe;

        auto geometry = mesh.Construct(env);
        TEST_REQUIRE(!geometry.IsNull());

        const auto& buffer = geometry.GetGeometryBuffer();
        TEST_REQUIRE(buffer.GetLayout() == gfx::GetVertexLayout<gfx::ShardVertex2D>());
        TEST_REQUIRE(buffer.GetNumDrawCmds() == 1);
        TEST_REQUIRE(buffer.GetDrawCmd(0).type == gfx::DrawType::Lines);

        // 2 shard triangles x 3 lines x 2 verts = 12 vertices
        TEST_REQUIRE(buffer.GetVertexCount() == 12);

        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        // shard 0 triangle: (0,0) -> (0,-1) -> (1,-1) -> (0,0)
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 0)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 0)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 1)->aPosition   == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 1)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 2)->aPosition   == gfx::ToVec(0.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 2)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 3)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 3)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 4)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 4)->aShardIndex == 0);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 5)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 5)->aShardIndex == 0);
        // shard 1 triangle: (0,0) -> (1,-1) -> (1,0) -> (0,0)
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 6)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 6)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 7)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 7)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 8)->aPosition   == gfx::ToVec(1.0f, -1.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 8)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 9)->aPosition   == gfx::ToVec(1.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>( 9)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(10)->aPosition   == gfx::ToVec(1.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(10)->aShardIndex == 1);
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(11)->aPosition   == gfx::ToVec(0.0f,  0.0f));
        TEST_REQUIRE(stream.GetVertex<gfx::ShardVertex2D>(11)->aShardIndex == 1);
    }
}

void unit_test_shader_id()
{
    TEST_CASE(test::Type::Feature)

    // all 2D simple shapes should use the same shader.
    {
        gfx::Drawable::Environment env;
        env.use_instancing = false;
        std::vector<std::string> list;

        list.push_back(gfx::Arrow().GetShaderId(env));
        list.push_back(gfx::ArrowCursor().GetShaderId(env));
        list.push_back(gfx::BlockCursor().GetShaderId(env));
        list.push_back(gfx::Capsule().GetShaderId(env));
        list.push_back(gfx::Circle().GetShaderId(env));
        list.push_back(gfx::IsoscelesTriangle().GetShaderId(env));
        list.push_back(gfx::Parallelogram().GetShaderId(env));
        list.push_back(gfx::Rectangle().GetShaderId(env));
        list.push_back(gfx::RightTriangle().GetShaderId(env));
        list.push_back(gfx::RoundRectangle().GetShaderId(env));
        list.push_back(gfx::Sector().GetShaderId(env));
        list.push_back(gfx::SemiCircle().GetShaderId(env));
        list.push_back(gfx::StaticLine().GetShaderId(env));
        list.push_back(gfx::Trapezoid().GetShaderId(env));

        list.push_back(gfx::SimpleShapeInstance(gfx::ArrowClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::ArrowCursorClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::BlockCursorClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::CapsuleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::CircleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::IsoscelesTriangleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::ParallelogramClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::RectangleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::RightTriangleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::RoundRectangleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::SectorClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::SemiCircleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::StaticLineClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::TrapezoidClass()).GetShaderId(env));
        TEST_REQUIRE(base::EqualElements(list));
    }

    // all 2D simple shapes with instancing should use the same shader
    {
        gfx::Drawable::Environment env;
        env.use_instancing = true;
        std::vector<std::string> list;

        list.push_back(gfx::Arrow().GetShaderId(env));
        list.push_back(gfx::ArrowCursor().GetShaderId(env));
        list.push_back(gfx::BlockCursor().GetShaderId(env));
        list.push_back(gfx::Capsule().GetShaderId(env));
        list.push_back(gfx::Circle().GetShaderId(env));
        list.push_back(gfx::IsoscelesTriangle().GetShaderId(env));
        list.push_back(gfx::Parallelogram().GetShaderId(env));
        list.push_back(gfx::Rectangle().GetShaderId(env));
        list.push_back(gfx::RightTriangle().GetShaderId(env));
        list.push_back(gfx::RoundRectangle().GetShaderId(env));
        list.push_back(gfx::Sector().GetShaderId(env));
        list.push_back(gfx::SemiCircle().GetShaderId(env));
        list.push_back(gfx::StaticLine().GetShaderId(env));
        list.push_back(gfx::Trapezoid().GetShaderId(env));

        list.push_back(gfx::SimpleShapeInstance(gfx::ArrowClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::ArrowCursorClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::BlockCursorClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::CapsuleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::CircleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::IsoscelesTriangleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::ParallelogramClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::RectangleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::RightTriangleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::RoundRectangleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::SectorClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::SemiCircleClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::StaticLineClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::TrapezoidClass()).GetShaderId(env));
        TEST_REQUIRE(base::EqualElements(list));
    }

    // 2D simple shaders should change when instancing or mesh changes.
    {
        gfx::Drawable::Environment env0;
        gfx::Drawable::Environment env1;

        env0.use_instancing = false;
        env1.use_instancing = false;
        TEST_REQUIRE(gfx::Arrow().GetShaderId(env0) == gfx::Arrow().GetShaderId(env1));

        env0.use_instancing = false;
        env1.use_instancing = true;
        TEST_REQUIRE(gfx::Arrow().GetShaderId(env0) != gfx::Arrow().GetShaderId(env1));

    }

    // other 2D "shapes" GuideGrid, and LineBatch should also use the same simple 2D shader
    {
        gfx::Drawable::Environment env0;
        gfx::Drawable::Environment env1;

        env0.use_instancing = false;
        env1.use_instancing = false;
        TEST_REQUIRE(gfx::Grid(10, 10).GetShaderId(env0) == gfx::Arrow().GetShaderId(env1));

        env0.use_instancing = false;
        env1.use_instancing = true;
        TEST_REQUIRE(gfx::Grid(10, 10).GetShaderId(env0) != gfx::Arrow().GetShaderId(env1));
    }

    {
        gfx::Drawable::Environment env0;
        gfx::Drawable::Environment env1;

        env0.use_instancing = false;
        env1.use_instancing = false;
        TEST_REQUIRE(gfx::LineBatch2D().GetShaderId(env0) == gfx::Arrow().GetShaderId(env1));

        env0.use_instancing = false;
        env1.use_instancing = true;
        TEST_REQUIRE(gfx::LineBatch2D().GetShaderId(env0) != gfx::Arrow().GetShaderId(env1));
    }
    // all 3D shapes should use same shader
    {
        gfx::Drawable::Environment env;
        env.use_instancing = false;
        std::vector<std::string> list;

        list.push_back(gfx::Cube().GetShaderId(env));
        list.push_back(gfx::Cone().GetShaderId(env));
        list.push_back(gfx::Cylinder().GetShaderId(env));
        list.push_back(gfx::Pyramid().GetShaderId(env));
        list.push_back(gfx::Sphere().GetShaderId(env));

        list.push_back(gfx::SimpleShapeInstance(gfx::CubeClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::ConeClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::CylinderClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::PyramidClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::SphereClass()).GetShaderId(env));
        TEST_REQUIRE(base::EqualElements(list));
    }

    // all 3D shapes with instancing should use same shader.
    {

        gfx::Drawable::Environment env;
        env.use_instancing = true;
        std::vector<std::string> list;

        list.push_back(gfx::Cube().GetShaderId(env));
        list.push_back(gfx::Cone().GetShaderId(env));
        list.push_back(gfx::Cylinder().GetShaderId(env));
        list.push_back(gfx::Pyramid().GetShaderId(env));
        list.push_back(gfx::Sphere().GetShaderId(env));

        list.push_back(gfx::SimpleShapeInstance(gfx::CubeClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::ConeClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::CylinderClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::PyramidClass()).GetShaderId(env));
        list.push_back(gfx::SimpleShapeInstance(gfx::SphereClass()).GetShaderId(env));
        TEST_REQUIRE(base::EqualElements(list));
    }

    // Wavefront should use the simple 3D non-instanced shader
    // todo: we can't test this right now because of the build config
    // and because of dependency to boost.spirit parser
    {
        //gfx::Drawable::Environment env;
        //env.use_instancing = false;
        //env.mesh_type = gfx::Drawable::MeshType::NormalRenderMesh;
        //TEST_REQUIRE(gfx::WavefrontMesh("foo").GetShaderId(env) == gfx::Cube().GetShaderId(env));
    }

    // 3D line batch should use the non-instanced 3D simple shader
    {
        gfx::Drawable::Environment env0;
        gfx::Drawable::Environment env1;

        env0.use_instancing = false;
        env1.use_instancing = false;
        TEST_REQUIRE(gfx::LineBatch3D().GetShaderId(env0) == gfx::Cube().GetShaderId(env1));

        env0.use_instancing = false;
        env1.use_instancing = true;
        TEST_REQUIRE(gfx::LineBatch3D().GetShaderId(env0) != gfx::Cube().GetShaderId(env1));
    }

    // check polygon mesh shader
    {
        gfx::PolygonMeshClass klass;
        klass.SetMeshType(gfx::PolygonMeshClass::MeshType::Simple2DRenderMesh);

        gfx::Drawable::Environment env;
        env.use_instancing = false;
        TEST_REQUIRE(gfx::PolygonMeshInstance(klass).GetShaderId(env) == gfx::Arrow().GetShaderId(env));

        env.use_instancing = true;
        TEST_REQUIRE(gfx::PolygonMeshInstance(klass).GetShaderId(env) == gfx::Arrow().GetShaderId(env));

        klass.SetMeshType(gfx::PolygonMeshClass::MeshType::Simple3DRenderMesh);

        env.use_instancing = false;
        TEST_REQUIRE(gfx::PolygonMeshInstance(klass).GetShaderId(env) == gfx::Cube().GetShaderId(env));

        klass.SetMeshType(gfx::PolygonMeshClass::MeshType::Model3DRenderMesh);
        TEST_REQUIRE(gfx::PolygonMeshInstance(klass).GetShaderId(env) != gfx::Cube().GetShaderId(env));

        klass.SetMeshType(gfx::PolygonMeshClass::MeshType::Dimetric2DRenderMesh);
        TEST_REQUIRE(gfx::PolygonMeshInstance(klass).GetShaderId(env) != gfx::Cube().GetShaderId(env));
    }
}

void unit_test_curve_aspect_ratio()
{
    TEST_CASE(test::Type::Feature)

    // the curve is tessellated for the aspect ratio of the model matrix and the inverse of
    // the aspect ratio is baked into the model matrix that goes to the vertex shader. The
    // line thickness should then be the same in all directions (a fraction of the
    // longer side of the node) and the curve points should still map to the node box.

    using Point = gfx::CurvePoint;

    // the same rules apply to a curve that has its own control points (gfx::Curve)
    // and to a curve that gets its control points from a curve class.
    for (const bool use_class : {false, true})
    {

    // tessellate the curve with the given model matrix and return the world space
    // bounds of the vertices when transformed with the model matrix given to the shader.
    const auto Render = [use_class](const std::vector<Point>& points, const glm::mat4& model_matrix) {
        std::unique_ptr<gfx::Drawable> curve;
        if (use_class)
        {
            gfx::CurveClass klass;
            klass.SetCurveType(gfx::CurveType::PolylineSpline);
            klass.SetJointStyle(gfx::CurveJointStyle::Miter);
            klass.SetEndCapStyle(gfx::CurveEndCapStyle::Butt);
            for (const auto& point : points)
                klass.AddPoint(point);
            curve = std::make_unique<gfx::CurveInstance>(klass);
        }
        else
        {
            gfx::CurveArgs args;
            args.curve_type = gfx::CurveType::PolylineSpline;
            args.joint_style = gfx::CurveJointStyle::Miter;
            args.end_cap_style = gfx::CurveEndCapStyle::Butt;
            args.points = points;
            curve = std::make_unique<gfx::CurveInstance>(std::move(args));
        }

        const glm::mat4 view_matrix(1.0f);
        const glm::mat4 proj_matrix(1.0f);
        gfx::Drawable::Environment env;
        env.mesh_type = gfx::Drawable::MeshType::PaintMesh;
        env.model_matrix = &model_matrix;
        env.view_matrix = &view_matrix;
        env.proj_matrix = &proj_matrix;

        auto geometry = curve->Construct(env);
        TEST_REQUIRE(!geometry.IsNull());

        TestDevice device;
        gfx::ProgramState program;
        gfx::Drawable::RasterState raster;
        gfx::DrawCall draw;
        TEST_REQUIRE(curve->ApplyDynamicState(env, draw, gfx::DrawGeometryHandle::Null, device, program, raster));
        glm::mat4 shader_matrix;
        TEST_REQUIRE(program.GetUniform("kModelViewMatrix", &shader_matrix));

        const auto& buffer = geometry.GetGeometryBuffer();
        const gfx::VertexStream stream(buffer.GetLayout(), buffer.GetVertexBuffer());
        glm::vec2 min = { 1.0e6f,  1.0e6f};
        glm::vec2 max = {-1.0e6f, -1.0e6f};
        for (size_t i=0; i<stream.GetCount(); ++i)
        {
            const auto& pos = stream.GetVertex<gfx::Vertex2D>(i)->aPosition;
            const auto world = shader_matrix * glm::vec4(pos.x, pos.y, 0.0f, 1.0f);
            TEST_REQUIRE(std::isfinite(world.x) && std::isfinite(world.y));
            min = glm::min(min, glm::vec2(world.x, world.y));
            max = glm::max(max, glm::vec2(world.x, world.y));
        }
        return std::make_pair(min, max);
    };

    const auto Near = [](float a, float b, float tolerance) {
        return std::abs(a - b) <= tolerance;
    };

    const std::vector<Point> horizontal = {Point(0.0f, -0.5f, 0.1f), Point(1.0f, -0.5f, 0.1f)};
    const std::vector<Point> vertical   = {Point(0.5f,  0.0f, 0.1f), Point(0.5f, -1.0f, 0.1f)};

    // these aspect ratios (4:1, 1:4, 1:1) are exactly on the bucket grid.
    // the thickness is 0.1 * max(width, height) in every direction.
    {
        const auto model = glm::scale(glm::mat4(1.0f), glm::vec3(400.0f, 100.0f, 1.0f));

        const auto [hmin, hmax] = Render(horizontal, model);
        TEST_REQUIRE(Near(hmin.x,   0.0f, 0.01f));
        TEST_REQUIRE(Near(hmax.x, 400.0f, 0.01f));
        TEST_REQUIRE(Near(hmin.y, -70.0f, 0.01f)); // center line at -50, thickness 40
        TEST_REQUIRE(Near(hmax.y, -30.0f, 0.01f));

        const auto [vmin, vmax] = Render(vertical, model);
        TEST_REQUIRE(Near(vmin.x, 180.0f, 0.01f)); // center line at 200, thickness 40
        TEST_REQUIRE(Near(vmax.x, 220.0f, 0.01f));
        TEST_REQUIRE(Near(vmin.y, -100.0f, 0.01f));
        TEST_REQUIRE(Near(vmax.y,    0.0f, 0.01f));
    }
    {
        const auto model = glm::scale(glm::mat4(1.0f), glm::vec3(100.0f, 400.0f, 1.0f));

        const auto [hmin, hmax] = Render(horizontal, model);
        TEST_REQUIRE(Near(hmin.x,    0.0f, 0.01f));
        TEST_REQUIRE(Near(hmax.x,  100.0f, 0.01f));
        TEST_REQUIRE(Near(hmin.y, -220.0f, 0.01f)); // center line at -200, thickness 40
        TEST_REQUIRE(Near(hmax.y, -180.0f, 0.01f));

        const auto [vmin, vmax] = Render(vertical, model);
        TEST_REQUIRE(Near(vmin.x,  30.0f, 0.01f)); // center line at 50, thickness 40
        TEST_REQUIRE(Near(vmax.x,  70.0f, 0.01f));
        TEST_REQUIRE(Near(vmin.y, -400.0f, 0.01f));
        TEST_REQUIRE(Near(vmax.y,    0.0f, 0.01f));
    }
    {
        const auto model = glm::scale(glm::mat4(1.0f), glm::vec3(100.0f, 100.0f, 1.0f));
        const auto [hmin, hmax] = Render(horizontal, model);
        TEST_REQUIRE(Near(hmin.y, -55.0f, 0.01f)); // thickness 10
        TEST_REQUIRE(Near(hmax.y, -45.0f, 0.01f));
        const auto [vmin, vmax] = Render(vertical, model);
        TEST_REQUIRE(Near(vmin.x, 45.0f, 0.01f));
        TEST_REQUIRE(Near(vmax.x, 55.0f, 0.01f));
    }

    // an aspect ratio that is not exactly on the bucket grid. The curve
    // still maps to the node box exactly and the thickness is off by
    // the quantization error (a few percent) at most.
    {
        const auto model = glm::scale(glm::mat4(1.0f), glm::vec3(300.0f, 100.0f, 1.0f));

        const auto [hmin, hmax] = Render(horizontal, model);
        TEST_REQUIRE(Near(hmin.x,   0.0f, 0.01f));
        TEST_REQUIRE(Near(hmax.x, 300.0f, 0.01f));
        TEST_REQUIRE(Near((hmin.y + hmax.y) * 0.5f, -50.0f, 0.01f));
        TEST_REQUIRE(Near(hmax.y - hmin.y, 30.0f, 30.0f * 0.03f));

        const auto [vmin, vmax] = Render(vertical, model);
        TEST_REQUIRE(Near((vmin.x + vmax.x) * 0.5f, 150.0f, 0.01f));
        TEST_REQUIRE(Near(vmax.x - vmin.x, 30.0f, 30.0f * 0.03f));
        TEST_REQUIRE(Near(vmin.y, -100.0f, 0.01f));
        TEST_REQUIRE(Near(vmax.y,    0.0f, 0.01f));
    }

    // the rotation doesn't change the aspect ratio of the model matrix
    // (the lengths of the axis vectors) so the center of the line must
    // still be on the rotated node box and the thickness should be isotropic.
    {
        const auto model = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
                           glm::scale(glm::mat4(1.0f), glm::vec3(400.0f, 100.0f, 1.0f));
        const auto [hmin, hmax] = Render(horizontal, model);
        // rotated 90 degrees: the line runs along y, thickness 40 along x.
        TEST_REQUIRE(Near(hmax.x - hmin.x, 40.0f, 0.01f));
        TEST_REQUIRE(Near(hmax.y - hmin.y, 400.0f, 0.01f));
    }

    // degenerate model matrices produce a valid (finite) geometry.
    {
        Render(horizontal, glm::scale(glm::mat4(1.0f), glm::vec3(0.0f, 100.0f, 1.0f)));
        Render(horizontal, glm::scale(glm::mat4(1.0f), glm::vec3(100.0f, 0.0f, 1.0f)));
    }
    }
}

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    unit_test_vertex_stream();
    unit_test_command_stream();
    unit_test_wireframe_algo();
    unit_test_triangle_algo();
    unit_test_tangent_algo();
    unit_test_polygon_builder_json();
    unit_test_polygon_builder_build();
    unit_test_particle_engine_data();
    unit_test_polygon_data();
    unit_test_curve_catmull_rom_tessellation();
    unit_test_curve_polyline_tessellation();
    unit_test_curve_aspect_ratio();
    unit_test_curve_data();
    unit_test_simple_shape_mesh();
    unit_test_polygon_simple_render_mesh();
    unit_test_polygon_simple_shard_mesh();
    unit_test_shader_id();
    return 0;
}
) // TEST_MAIN
