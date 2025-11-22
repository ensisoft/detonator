// Copyright (c) 2010-2013 Sami Väisänen, Ensisoft 
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

#include <vector>
#include <fstream>
#include <iterator>

#include "base/wavefront.h"
#include "base/test_float.h"
#include "base/test_minimal.h"

namespace wf = wavefront;

#define FLOAT_REQUIRE(x, y) TEST_REQUIRE(real::equals(x, y))

template<int i>
bool operator==(const wf::vec4<i>& lhs, const wf::vec4<i>& rhs)
{
    return 
        real::equals(lhs.x, rhs.x) &&
        real::equals(lhs.y, rhs.y) &&
        real::equals(lhs.z, rhs.z) &&
        real::equals(lhs.w, rhs.w);
}
template<int i>
bool operator==(const wf::vec3<i>& lhs, const wf::vec3<i>& rhs)
{
    return 
        real::equals(lhs.x, rhs.x) &&
        real::equals(lhs.y, rhs.y) &&
        real::equals(lhs.z, rhs.z);
}


template<typename T>
void clear(T& value)
{ 
    value = T{};
}


void unit_test_parse_normal()
{
    namespace d = wf::detail;    

    wf::Normal v;
    clear(v);
    TEST_REQUIRE(d::Parse("vn 0.0 0.5 0.67", v));
    TEST_REQUIRE((v == wf::Normal { 0.0, 0.5, 0.67 }));
    clear(v);
    TEST_REQUIRE(d::Parse("vn -1.0 -2.0 3.333", v));
    TEST_REQUIRE((v == wf::Normal {-1.0, -2.0, 3.333 }));
    clear(v);
    TEST_REQUIRE(d::Parse("vn  1.0  -1.0  -2.0", v));
    TEST_REQUIRE((v == wf::Normal {1.0, -1.0, -2.0 }));

    TEST_REQUIRE(!d::Parse("vn asdgasgasg", v));
    TEST_REQUIRE(!d::Parse("vn 1.0", v));
    TEST_REQUIRE(!d::Parse("", v));
    // TEST_REQUIRE(!d::parse("vn 6.0 4.0 5.0 6.0 7.0 8.0 10.0", v));
    TEST_REQUIRE(!d::Parse("vt 6.0 4.0 5.0", v));
}

void unit_test_parse_texcoord()
{
    namespace d = wf::detail;    

    wf::TexCoord v;
    clear(v);
    TEST_REQUIRE(d::Parse("vt 1.0", v));
    TEST_REQUIRE((v == wf::TexCoord {1.0, 0, 0}));
    clear(v);
    TEST_REQUIRE(d::Parse("vt 1.0 2.0", v));
    TEST_REQUIRE((v == wf::TexCoord {1.0, 2.0, 0}));
    clear(v);
    TEST_REQUIRE(d::Parse("vt 0.0 0.5 0.67", v));
    TEST_REQUIRE((v == wf::TexCoord { 0.0, 0.5, 0.67 }));
    clear(v);
    TEST_REQUIRE(d::Parse("vt -1.0 -2.0 3.333", v));
    TEST_REQUIRE((v == wf::TexCoord {-1.0, -2.0, 3.333 }));
    clear(v);
    TEST_REQUIRE(d::Parse("vt  1.0  -1.0  -2.0", v));
    TEST_REQUIRE((v == wf::TexCoord {1.0, -1.0, -2.0 }));

    TEST_REQUIRE(!d::Parse("vt asdgasgasg", v));

    TEST_REQUIRE(!d::Parse("", v));
    // TEST_REQUIRE(!d::parse("vt 6.0 4.0 5.0 6.0 7.0 8.0 10.0", v));
    TEST_REQUIRE(!d::Parse("vn 6.0 4.0 5.0", v));
}

void unit_test_parse_position()
{
    namespace d = wf::detail;    

    wf::Position v;
    clear(v);
    TEST_REQUIRE(d::Parse("v 0.0 0.5 0.67", v));
    TEST_REQUIRE((v == wf::Position { 0.0, 0.5, 0.67 }));
    clear(v);
    TEST_REQUIRE(d::Parse("v -1.0 -2.0 3.333", v));
    TEST_REQUIRE((v == wf::Position {-1.0, -2.0, 3.333 }));
    clear(v);
    TEST_REQUIRE(d::Parse("v  1.0  -1.0  -2.0", v));
    TEST_REQUIRE((v == wf::Position {1.0, -1.0, -2.0 }));
    clear(v);
    TEST_REQUIRE(d::Parse("v 1.0 -1.0 -2.0 -5.0", v));
    TEST_REQUIRE((v == wf::Position {1.0, -1.0, -2.0, -5.0 }));

    TEST_REQUIRE(!d::Parse("v asdgasgasg", v));
    TEST_REQUIRE(!d::Parse("v 1.0", v));
    TEST_REQUIRE(!d::Parse("", v));
    // TEST_REQUIRE(!d::parse("v 6.0 4.0 5.0 6.0 7.0 8.0 10.0", v));
    TEST_REQUIRE(!d::Parse("vt 6.0 4.0 5.0", v));
}

void unit_test_parse_face()
{
    namespace d = wf::detail;    

    wf::Face f;

    // single vertex

    // v
    TEST_REQUIRE(d::Parse("f 1", f));
    TEST_REQUIRE(f.vertices.size() == 1);
    TEST_REQUIRE(f.vertices[0].pindex == 1);
    TEST_REQUIRE(f.vertices[0].tindex == 0);
    TEST_REQUIRE(f.vertices[0].nindex == 0);

    // v/vt
    f.vertices.clear();
    TEST_REQUIRE(d::Parse("f 1/2", f));
    TEST_REQUIRE(f.vertices.size() == 1);
    TEST_REQUIRE(f.vertices[0].pindex == 1);
    TEST_REQUIRE(f.vertices[0].tindex == 2);
    TEST_REQUIRE(f.vertices[0].nindex == 0);


    // v//vn
    f.vertices.clear();
    TEST_REQUIRE(d::Parse("f 1//2", f));
    TEST_REQUIRE(f.vertices.size() == 1);
    TEST_REQUIRE(f.vertices[0].pindex == 1);
    TEST_REQUIRE(f.vertices[0].tindex == 0);
    TEST_REQUIRE(f.vertices[0].nindex == 2);

    // v/vt/vn
    f.vertices.clear();
    TEST_REQUIRE(d::Parse("f 1/2/3", f));
    TEST_REQUIRE(f.vertices.size() == 1);
    TEST_REQUIRE(f.vertices[0].pindex == 1);
    TEST_REQUIRE(f.vertices[0].tindex == 2);
    TEST_REQUIRE(f.vertices[0].nindex == 3);

    // multiple vertices


    f.vertices.clear();
    // v
    TEST_REQUIRE(d::Parse("f 1 2", f));
    TEST_REQUIRE(f.vertices.size() == 2);
    TEST_REQUIRE(f.vertices[0].pindex == 1);
    TEST_REQUIRE(f.vertices[0].tindex == 0);
    TEST_REQUIRE(f.vertices[0].nindex == 0);
    TEST_REQUIRE(f.vertices[1].pindex == 2);
    TEST_REQUIRE(f.vertices[1].tindex == 0);
    TEST_REQUIRE(f.vertices[1].nindex == 0);

    // v/vt
    f.vertices.clear();
    TEST_REQUIRE(d::Parse("f 1/2 3/4", f));
    TEST_REQUIRE(f.vertices.size() == 2);
    TEST_REQUIRE(f.vertices[0].pindex == 1);
    TEST_REQUIRE(f.vertices[0].tindex == 2);
    TEST_REQUIRE(f.vertices[0].nindex == 0);
    TEST_REQUIRE(f.vertices[1].pindex == 3);
    TEST_REQUIRE(f.vertices[1].tindex == 4);
    TEST_REQUIRE(f.vertices[1].nindex == 0);


    // v//vn
    f.vertices.clear();
    TEST_REQUIRE(d::Parse("f 1//2 3//4", f));
    TEST_REQUIRE(f.vertices.size() == 2);
    TEST_REQUIRE(f.vertices[0].pindex == 1);
    TEST_REQUIRE(f.vertices[0].tindex == 0);
    TEST_REQUIRE(f.vertices[0].nindex == 2);
    TEST_REQUIRE(f.vertices[1].pindex == 3);
    TEST_REQUIRE(f.vertices[1].tindex == 0);
    TEST_REQUIRE(f.vertices[1].nindex == 4);

    // v/vt/vn
    f.vertices.clear();
    TEST_REQUIRE(d::Parse("f 1/2/3 4/5/6", f));
    TEST_REQUIRE(f.vertices.size() == 2);
    TEST_REQUIRE(f.vertices[0].pindex == 1);
    TEST_REQUIRE(f.vertices[0].tindex == 2);
    TEST_REQUIRE(f.vertices[0].nindex == 3);
    TEST_REQUIRE(f.vertices[1].pindex == 4);
    TEST_REQUIRE(f.vertices[1].tindex == 5);
    TEST_REQUIRE(f.vertices[1].nindex == 6);

    TEST_REQUIRE(!d::Parse("", f));
    TEST_REQUIRE(!d::Parse("a 1 2 3 ", f));
    TEST_REQUIRE(!d::Parse("f 1///234", f));
    TEST_REQUIRE(!d::Parse("f 1.5", f));
    TEST_REQUIRE(!d::Parse("f 1 b 3", f));
    TEST_REQUIRE(!d::Parse("f //1", f));
    // TEST_REQUIRE(!d::parse("f 1 1//3", f)); this should actually fail, the vertex notation should be consistent int the data
}

void unit_test_parse_ka()
{
    namespace d = wf::detail;    

    wf::MaterialKa v;
    clear(v);
    TEST_REQUIRE(d::Parse("Ka 0.0 0.5 0.67", v));
    TEST_REQUIRE((v == wf::MaterialKa { 0.0, 0.5, 0.67 }));
    clear(v);
    TEST_REQUIRE(d::Parse("Ka -1.0 -2.0 3.333", v));
    TEST_REQUIRE((v == wf::MaterialKa {-1.0, -2.0, 3.333 }));
    clear(v);
    TEST_REQUIRE(d::Parse("Ka  1.0  -1.0  -2.0", v));
    TEST_REQUIRE((v == wf::MaterialKa {1.0, -1.0, -2.0 }));

    TEST_REQUIRE(!d::Parse("Ka asdgasgasg", v));
    TEST_REQUIRE(!d::Parse("Ka 1.0", v));
    TEST_REQUIRE(!d::Parse("", v));
    // TEST_REQUIRE(!d::parse("vn 6.0 4.0 5.0 6.0 7.0 8.0 10.0", v));
    TEST_REQUIRE(!d::Parse("Kn 6.0 4.0 5.0", v));
}

void unit_test_parse_kd()
{
    namespace d = wf::detail;    

    wf::MaterialKd v;
    clear(v);
    TEST_REQUIRE(d::Parse("Kd 0.0 0.5 0.67", v));
    TEST_REQUIRE((v == wf::MaterialKd { 0.0, 0.5, 0.67 }));
    clear(v);
    TEST_REQUIRE(d::Parse("Kd -1.0 -2.0 3.333", v));
    TEST_REQUIRE((v == wf::MaterialKd {-1.0, -2.0, 3.333 }));
    clear(v);
    TEST_REQUIRE(d::Parse("Kd  1.0  -1.0  -2.0", v));
    TEST_REQUIRE((v == wf::MaterialKd {1.0, -1.0, -2.0 }));

    TEST_REQUIRE(!d::Parse("Kd asdgasgasg", v));
    TEST_REQUIRE(!d::Parse("Kd 1.0", v));
    TEST_REQUIRE(!d::Parse("", v));
    // TEST_REQUIRE(!d::parse("vn 6.0 4.0 5.0 6.0 7.0 8.0 10.0", v));
    TEST_REQUIRE(!d::Parse("Ka 6.0 4.0 5.0", v));
}

void unit_test_parse_ks()
{
    namespace d = wf::detail;    

    wf::MaterialKs v;
    clear(v);
    TEST_REQUIRE(d::Parse("Ks 0.0 0.5 0.67", v));
    TEST_REQUIRE((v == wf::MaterialKs { 0.0, 0.5, 0.67 }));
    clear(v);
    TEST_REQUIRE(d::Parse("Ks -1.0 -2.0 3.333", v));
    TEST_REQUIRE((v == wf::MaterialKs {-1.0, -2.0, 3.333 }));
    clear(v);
    TEST_REQUIRE(d::Parse("Ks  1.0  -1.0  -2.0", v));
    TEST_REQUIRE((v == wf::MaterialKs {1.0, -1.0, -2.0 }));

    TEST_REQUIRE(!d::Parse("Ks asdgasgasg", v));
    TEST_REQUIRE(!d::Parse("Ks 1.0", v));
    TEST_REQUIRE(!d::Parse("", v));
    // TEST_REQUIRE(!d::parse("vn 6.0 4.0 5.0 6.0 7.0 8.0 10.0", v));
    TEST_REQUIRE(!d::Parse("Kd 6.0 4.0 5.0", v));
}

void unit_test_parse_illum()
{
    namespace d = wf::detail;

    wf::Illumination i;
    TEST_REQUIRE(d::Parse("illum 0", i));
    TEST_REQUIRE(i.model == wf::Illumination::Model::Constant);

    TEST_REQUIRE(d::Parse("illum 1", i));
    TEST_REQUIRE(i.model == wf::Illumination::Model::Diffuse);

    TEST_REQUIRE(d::Parse("illum 2", i));
    TEST_REQUIRE(i.model == wf::Illumination::Model::DiffuseAndSpecular);

    TEST_REQUIRE(!d::Parse("illum", i));
    TEST_REQUIRE(!d::Parse("", i));
    // TEST_REQUIRE(!d::parse("illum 5", i));
}

void unit_test_parse_ns()
{
    namespace d = wf::detail;

    wf::SpecularExponent n;
    TEST_REQUIRE(d::Parse("Ns 0.0", n));
    FLOAT_REQUIRE(n.exponent, 0.0);
    TEST_REQUIRE(d::Parse("Ns 4.0", n));
    FLOAT_REQUIRE(n.exponent, 4.0);
}

void unit_test_parse_primitive()
{
    unit_test_parse_normal();
    unit_test_parse_position();
    unit_test_parse_texcoord();
    unit_test_parse_face();
    unit_test_parse_ka();
    unit_test_parse_kd();
    unit_test_parse_ks();
    unit_test_parse_illum();
    unit_test_parse_ns();
}

void unit_test_parse_obj_success()
{
    struct checker : public wavefront::ObjImporterBase
    {
        std::size_t positions = 0;
        std::size_t normals = 0;
        std::size_t texcoords = 0;
        std::size_t faces = 0;

        void Import(const wf::Position& p) { ++positions; }
        void Import(const wf::Normal& n) { ++normals; }
        void Import(const wf::TexCoord& t) { ++texcoords; }
        void Import(const wf::Face& f) { ++faces; }

        bool OnParseError(const std::string& line, std::size_t lineno)
        {
            TEST_FAIL("parse failure");
            return false;
        }
        bool OnUnknownIdentifier(const std::string& line, std::size_t lineno)
        {
            TEST_FAIL("parse failure");
            return false;
        }
    };

    const char data[] = 
        "# Blender v2.65 (sub 0) OBJ File: ''\n"
        "# www.blender.org\n"
        "mtllib test.mtl\n"
        "o test\n"
        "v -0.392859 -0.188482 0.064474\n"
        "v -0.392859 -0.188482 0.064474\n"
        "v -0.392859 -0.188482 0.064474\n"
        "v -0.392859 -0.188482 0.064474\n"                
        "vn -0.392130 -0.184177 0.052850\n"
        "vn -0.392130 -0.184177 0.052850\n"        
        "vn -0.392130 -0.184177 0.052850\n"
        "vn -0.392130 -0.184177 0.052850\n"                
        "vt -0.391565 -0.193230\n"
        "vt -0.391565 -0.193230\n"
        "vt -0.391565 -0.193230\n"
        "vt -0.391565 -0.193230\n"                        
        "f 1 2 3\n"
        "f 2 4 5\n"
        "f 4 5 5 5 6\n";

    checker c;
    TEST_REQUIRE(wavefront::ParseObj(data, data + strlen(data), c));
    TEST_REQUIRE(c.faces == 3);
    TEST_REQUIRE(c.normals == 4);
    TEST_REQUIRE(c.positions == 4);
    TEST_REQUIRE(c.normals == 4);

}

void unit_test_parse_obj_failure()
{
    struct checker : public wavefront::ObjImporterBase
    {
        std::string line;
        std::size_t lineno;
        bool OnParseError(const std::string& line, std::size_t lineno)
        {
            this->line   = line;
            this->lineno = lineno;
            return false;
        }
        bool OnUnknownIdentifier(const std::string& line, std::size_t lineno)
        {
            TEST_FAIL("parse failure");
            return false;
        }
    };

    const char data[] = 
        "# Blender v2.65 (sub 0) OBJ File: ''\n"
        "# www.blender.org\n"
        "mtllib test.mtl\n"
        "o test\n"
        "v -0.392859 -0.188482 0.064474\n"
        "v -0.392859 -0.188482 0.064474\n"
        "v asgas\n"
        "v -0.392859 -0.188482 0.064474\n"
        "v -0.392859 -0.188482 0.064474\n"                
        "vn -0.392130 -0.184177 0.052850\n"
        "vn -0.392130 -0.184177 0.052850\n"        
        "vn -0.392130 -0.184177 0.052850\n"
        "vn -0.392130 -0.184177 0.052850\n"                
        "vt -0.391565 -0.193230\n"
        "vt -0.391565 -0.193230\n"
        "vt -0.391565 -0.193230\n"
        "vt -0.391565 -0.193230\n"                        
        "f 1 2 3\n"
        "f 2 4 5\n"
        "f 4 5 5 5 6\n";
    checker c;
    TEST_REQUIRE(!wavefront::ParseObj(data, data + strlen(data), c));
    TEST_REQUIRE(c.lineno == 7);
    TEST_REQUIRE(c.line == "v asgas");
}

void unit_test_parse_mtl_success()
{
    struct checker : public wf::MtlImporterBase
    {
        void BeginMaterial(const wf::NewMtl&) { ++materials; }
        void Import(const wf::MaterialKa&) { ++ka; };
        void Import(const wf::MaterialKs&) { ++ks; };
        void Import(const wf::MaterialKd&) { ++kd; };
        void Import(const wf::SpecularExponent&) { ++ns; };
        void Import(const wf::Illumination&) {++illum; }
        void Import(const wf::AmbientTextureMap&) { ++mapka; }
        void Import(const wf::DiffuseTextureMap&) { ++mapkd; }

        std::size_t materials = 0;
        std::size_t ka = 0;
        std::size_t kd = 0;
        std::size_t ks = 0;
        std::size_t ns = 0;
        std::size_t illum = 0;
        std::size_t mapkd = 0;
        std::size_t mapka = 0;

        bool OnParseError(const std::string& line, std::size_t lineno) const
        { return false; }

        bool OnUnknownIdentifier(const std::string& line, std::size_t lineno) const
        { return false; }
    };

    const char data[] =
        "# Blender MTL File: 'None'\n"
        "# Material Count: 5\n"
        "\n"
        "newmtl bennettzombie_arm\n"
        "Ns 0.000000\n"
        "Ka 0.000000 0.000000 0.000000\n"
        "Kd 0.640000 0.640000 0.640000\n"
        "Ks 0.000000 0.000000 0.000000\n"
        "illum 2\n"
        "map_Kd bennetzombie_arm.png\n"
        "\n"
        "newmtl bennettzombie_body\n"
        "Ns 0.000000\n"
        "Ka 0.000000 0.000000 0.000000\n"
        "Kd 0.640000 0.640000 0.640000\n"
        "Ks 0.000000 0.000000 0.000000\n"
        "illum 2\n"
        "map_Kd bennetzombie_body.png\n";

    checker c;
    TEST_REQUIRE(wf::ParseMtl(data, data + strlen(data), c));
    TEST_REQUIRE(c.materials == 2);
    TEST_REQUIRE(c.ns == 2);
    TEST_REQUIRE(c.mapkd == 2);
    TEST_REQUIRE(c.ka == 2);
    TEST_REQUIRE(c.kd == 2);
    TEST_REQUIRE(c.ks == 2);

}

void unit_test_parse_bennet()
{
    std::string file = __FILE__;
    file.resize(file.size()-3);
    file.append("obj");

    std::ifstream in(file);
    TEST_REQUIRE(in.is_open());

    struct checker : public wf::ObjImporterBase
    {
        bool OnUnknownIdentifier(const std::string&, std::size_t) const
        { return true; }

        bool OnParseError(const std::string&, std::size_t) const
        { return false; }

        void Import(const wf::Position&) { ++num_positions; }
        void Import(const wf::TexCoord&) { ++num_texcoords; }
        void Import(const wf::Normal&) { ++num_normals; }
        void Import(const wf::Face&) { ++num_faces; }

        std::size_t num_normals   = 0;
        std::size_t num_texcoords = 0;
        std::size_t num_positions = 0;
        std::size_t num_faces     = 0;
    };

    checker c;
    TEST_REQUIRE(wf::ParseObj(std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>(), c));

    // cat bennet.obj | grep "v " | wc -l

    TEST_REQUIRE(c.num_positions == 9495);
    TEST_REQUIRE(c.num_texcoords == 3003);
    TEST_REQUIRE(c.num_normals == 8060);
    TEST_REQUIRE(c.num_faces == 4798);

}

EXPORT_TEST_MAIN(
int test_main(int, char*[])
{
    unit_test_parse_primitive();
    unit_test_parse_obj_success();
    unit_test_parse_obj_failure();
    unit_test_parse_mtl_success();
    unit_test_parse_bennet();
    return 0;
}
) // TEST_MAIN

