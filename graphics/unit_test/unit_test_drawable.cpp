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
#include "data/json.h"
#include "graphics/drawable.h"
#include "graphics/geometry.h"

bool operator==(const gfx::Vec2& lhs, const gfx::Vec2& rhs)
{
    return real::equals(lhs.x, rhs.x) &&
           real::equals(lhs.y, rhs.y);
}

bool operator==(const gfx::Vertex2D& lhs, const gfx::Vertex2D& rhs)
{
    return real::equals(lhs.aPosition.x, rhs.aPosition.x) &&
           real::equals(lhs.aPosition.y, rhs.aPosition.y) &&
           real::equals(lhs.aTexCoord.x, rhs.aTexCoord.x) &&
           real::equals(lhs.aTexCoord.y, rhs.aTexCoord.y);
}

void unit_test_geometry()
{
    TEST_CASE(test::Type::Feature)

    // vertex stream
    {
        std::vector<gfx::Vertex2D> verts;
        verts.resize(3);
        verts[0].aPosition = gfx::Vec2 {  1.0f,  2.0f };
        verts[0].aTexCoord = gfx::Vec2 {  0.5f,  0.5f };
        verts[1].aPosition = gfx::Vec2 { -1.0f, -2.0f };
        verts[1].aTexCoord = gfx::Vec2 {  1.0f,  1.0f };
        verts[2].aPosition = gfx::Vec2 {  0.0f,  0.0f };
        verts[2].aTexCoord = gfx::Vec2 { -0.5f, -0.5f };

        gfx::VertexStream stream(gfx::GetVertexLayout<gfx::Vertex2D>(),
                                 verts.data(), verts.size() * sizeof(gfx::Vertex2D));
        TEST_REQUIRE(stream.IsValid());
        TEST_REQUIRE(stream.GetCount() == 3);
        TEST_REQUIRE(stream.HasAttribute("aPosition"));
        TEST_REQUIRE(stream.HasAttribute("aTexCoord"));
        TEST_REQUIRE(stream.HasAttribute("aFoobar") == false);
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
        buffer.UploadVertices(verts.data(), verts.size() * sizeof(gfx::Vertex2D), gfx::Geometry::Usage::Static);
        buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 0, 3);
        buffer.AddDrawCmd(gfx::Geometry::DrawType::Triangles, 3, 3);

        gfx::GeometryBuffer wireframe;
        gfx::CreateWireframe(buffer, wireframe);
        TEST_REQUIRE(wireframe.GetVertexBytes() == 12 * sizeof(gfx::Vertex2D));
        TEST_REQUIRE(wireframe.GetNumDrawCommands() == 1);
        TEST_REQUIRE(wireframe.GetDrawCmd(0).type == gfx::Geometry::DrawType::Lines);
        TEST_REQUIRE(wireframe.GetDrawCmd(0).offset == 0);
        TEST_REQUIRE(wireframe.GetDrawCmd(0).count == std::numeric_limits<size_t>::max());

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
        buffer.UploadVertices(verts.data(), verts.size() * sizeof(gfx::Vertex2D), gfx::Geometry::Usage::Static);
        buffer.AddDrawCmd(gfx::Geometry::DrawType::TriangleFan);

        gfx::GeometryBuffer wireframe;
        gfx::CreateWireframe(buffer, wireframe);
        TEST_REQUIRE(wireframe.GetVertexBytes() == 18 * sizeof(gfx::Vertex2D));
        TEST_REQUIRE(wireframe.GetNumDrawCommands() == 1);
        TEST_REQUIRE(wireframe.GetDrawCmd(0).type == gfx::Geometry::DrawType::Lines);
        TEST_REQUIRE(wireframe.GetDrawCmd(0).offset == 0);
        TEST_REQUIRE(wireframe.GetDrawCmd(0).count == std::numeric_limits<size_t>::max());

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

void unit_test_polygon_data()
{
    TEST_CASE(test::Type::Feature)

    std::vector<gfx::Vertex2D> verts;
    gfx::Vertex2D v0;
    v0.aPosition.x = 1.0f;
    v0.aPosition.y = 2.0f;
    v0.aTexCoord.x = -1.0f;
    v0.aTexCoord.y = -0.5f;
    verts.push_back(v0);

    gfx::PolygonMeshClass klass;
    gfx::PolygonMeshClass::DrawCommand cmd;
    cmd.type = gfx::PolygonMeshClass::DrawType::TriangleFan;
    cmd.offset = 1243;
    cmd.count = 555;
    klass.AddVertices(std::move(verts));
    klass.AddDrawCommand(cmd);

    // to/from json
    {
        data::JsonObject json;
        klass.IntoJson(json);
        gfx::PolygonMeshClass copy;
        TEST_REQUIRE(copy.FromJson(json));
        TEST_REQUIRE(copy.GetNumVertices() == 1);
        TEST_REQUIRE(copy.GetNumDrawCommands() == 1);
        TEST_REQUIRE(copy.GetVertex(0) == v0);
        TEST_REQUIRE(copy.GetDrawCommand(0).type == gfx::PolygonMeshClass::DrawType::TriangleFan);
        TEST_REQUIRE(copy.GetDrawCommand(0).offset == 1243);
        TEST_REQUIRE(copy.GetDrawCommand(0).count == 555);
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
    }
}

void unit_test_polygon_vertex_operations()
{
    TEST_CASE(test::Type::Feature)

    // some test vertices.
    std::vector<gfx::PolygonMeshClass::Vertex> verts;
    verts.resize(6);
    verts[0].aPosition.x = 0.0f;
    verts[1].aPosition.x = 1.0f;
    verts[2].aPosition.x = 2.0f;
    verts[3].aPosition.x = 3.0f;
    verts[4].aPosition.x = 4.0f;
    verts[5].aPosition.x = 5.0f;

    // test finding the right draw command.
    {
        gfx::PolygonMeshClass poly;
        poly.AddVertices(verts);

        gfx::PolygonMeshClass::DrawCommand cmd;
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
        gfx::PolygonMeshClass poly;
        poly.AddVertices(verts);

        gfx::PolygonMeshClass::DrawCommand cmd;
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
        gfx::PolygonMeshClass poly;
        poly.AddVertices(verts);

        gfx::PolygonMeshClass::DrawCommand cmd;
        cmd.offset = 0;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);

        cmd.offset = 3;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);
        TEST_REQUIRE(poly.GetNumDrawCommands() == 2);
        TEST_REQUIRE(poly.GetNumVertices() == 6);

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
        gfx::PolygonMeshClass poly;
        poly.AddVertices(verts);

        gfx::PolygonMeshClass::DrawCommand cmd;
        cmd.offset = 0;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);

        cmd.offset = 3;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);
        TEST_REQUIRE(poly.GetNumDrawCommands() == 2);
        TEST_REQUIRE(poly.GetNumVertices() == 6);

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
        gfx::PolygonMeshClass poly;
        poly.AddVertices(verts);

        gfx::PolygonMeshClass::DrawCommand cmd;
        cmd.offset = 0;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);

        cmd.offset = 3;
        cmd.count  = 3;
        poly.AddDrawCommand(cmd);
        TEST_REQUIRE(poly.GetNumDrawCommands() == 2);
        TEST_REQUIRE(poly.GetNumVertices() == 6);

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

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    unit_test_geometry();
    unit_test_polygon_data();
    unit_test_polygon_vertex_operations();
    unit_test_particle_engine_data();
    return 0;
}
) // TEST_MAIN