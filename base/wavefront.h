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

#pragma once

#include "config.h"

#include <string>
#include <vector>
#include <functional>
#include <map>

// Wavefront Advanced Visualizer .OBJ and .MTL data file parsers
// support for polygonal geometry only.

namespace wavefront
{
    template<int UniqueType>
    union vec4 {
        struct {
            float x, y, z, w;
        };
        struct {
            float r, g, b, a;
        };
    };

    template<int UnigueType>
    union vec3 {
        struct {
            float x, y, z;
        };
        struct {
            float r, g, b;
        };
    };

    template<int UniqueType>
    struct StringKey {
        std::string name;
    };

    // illumination model
    struct Illumination {
        enum class Model {
            // constant color illumination
            // outColor = Kd
            Constant = 0,

            // diffuse illumination using Lambertian shading. The color includes an ambient and diffuse shading
            // terms for each light source.
            // outColor = KaIa + Kd { SUM J=1...ls, (N * Lj)Ij }
            Diffuse  = 1,

            // diffuse and specular illumination model using Lambertian shading and Blinns' interpretation
            // of Phong's specular illumination model
            // outColor = KaIa + KD { SUM J=1...ls, (N*Lj)Ij } + Ks { SUM J01...ls, ((H*Hj)^Ns)Ij }
            DiffuseAndSpecular = 2
        };
        Model model = Model::Constant;
    };

    // specular exponent
    struct SpecularExponent {
        float exponent = 1.0f;
    };

    // a vertex defines indices to the position, normal and texture coordinate data.
    // the indices are all 1 based and if the vertex doesn't reference a given
    // attribute the index value is 0. Therefore, make sure to check against 0
    // and then subtract 1 when using the value as in index into the data.
    struct Vertex {
        std::size_t pindex = 0; // position
        std::size_t nindex = 0; // normal
        std::size_t tindex = 0; // texcoord
    };

    // a polygonal face with a list of vertices.
    struct Face {
        std::vector<Vertex> vertices;
    };

    // position specifies a geometric vertex and its xyz(w) coordinates
    using Position = vec4<0>;

    // normal specifies a normal vector and its xyz components
    using Normal = vec3<1>;

    // TexCoord specifies texture coordinates.
    // how many components are used depends on the texture type.
    using TexCoord = vec3<2>;

    // material ambient reflectivity property
    using MaterialKa = vec3<3>;

    // material diffuse reflectivity property
    using MaterialKd = vec3<4>;

    // material specular reflectivity
    using MaterialKs = vec3<5>;

    // set the material library name to look for material definitions
    // for the following material references.
    // todo: could be multiple filenames
    using MtlLib = StringKey<1>;

    // set the current material for the following element
    using UseMtl = StringKey<2>;

    // begin new material. material data will follow.
    using NewMtl = StringKey<3>;

    // set the current name for the following elements.
    // todo: could be multiple group names
    using GroupName = StringKey<4>;

    // set the current user defined name for the following elements.
    using ObjectName = StringKey<5>;

    // Ambient texture map name
    using AmbientTextureMap = StringKey<6>;

    // Diffuse texture map name.
    using DiffuseTextureMap = StringKey<7>;

    namespace detail {

        inline std::string SplitStringOnSpace(const std::string& line)
        {
            auto pos = line.find_first_of(' ');
            if (pos == std::string::npos)
                return "";
            return line.substr(0, pos);
        }

        bool Parse(const std::string& str, Position& p);
        bool Parse(const std::string& str, Normal& n);
        bool Parse(const std::string& str, TexCoord& st);
        bool Parse(const std::string& str, Face& f);
        bool Parse(const std::string& str, MaterialKa& ka);
        bool Parse(const std::string& str, MaterialKd& kd);
        bool Parse(const std::string& str, MaterialKs& ks);
        bool Parse(const std::string& str, Illumination& illumination);
        bool Parse(const std::string& str, SpecularExponent& exponent);
        bool Parse(const std::string& str, std::string& s);

        template<int UniqueType>
        bool Parse(const std::string& str, StringKey<UniqueType>& key)
        {
            return Parse(str, key.name);
        }

        template<typename Iterator>
        void ReadLine(Iterator& beg, Iterator end, std::string& line)
        {
            line.clear();
            while (beg != end)
            {
                auto next = *beg++;
                if (next == ' ' && line.empty())
                    continue;
                else if (next == '\n')
                    return;
                else if (next != '\r')
                    line += next;
            }
        }

        struct ParseActionImport {
            template<typename T, typename Importer>
            static void Invoke(const T& value, Importer& i)
            {
                i.Import(value);
            }
        };

        struct ParseActionBeginGroup {
            template<typename T, typename Importer>
            static void Invoke(const T& group, Importer& i)
            {
                i.BeginGroup(group);
            }
        };

        struct ParseActionBeginMaterial {
            template<typename T, typename Importer>
            static void Invoke(const T& mat, Importer& i)
            {
                i.BeginMaterial(mat);
            }
        };

        struct ParseActionSetAttribute {
            template<typename T, typename Importer>
            static void Invoke(const T& attr, Importer& i)
            {
                i.SetAttribute(attr);
            }
        };

        template<typename Importer, typename ImportAction, typename T>
        bool GenericParse(const std::string& line, Importer& imp)
        {
            T value {};
            if (!Parse(line, value))
                return false;
            ImportAction::Invoke(value, imp);
            return true;
        }

        template<typename Iterator, typename Importer>
        bool ParseLines(Iterator beg, Iterator end, Importer& importer,
            const std::map<std::string, std::function<bool(const std::string&, Importer&)>>& methods)
        {
            std::size_t lineno = 0;
            std::string line;

            while (beg != end)
            {
                ++lineno;
                detail::ReadLine(beg, end, line);
                if (line.empty())
                    continue;
                else if (line[0] == '#')
                    continue;

                const auto key = detail::SplitStringOnSpace(line);
                const auto pos = methods.find(key);
                if (pos == std::end(methods))
                {
                    if (!importer.OnUnknownIdentifier(line, lineno))
                        return false;
                    continue;
                }
                auto method = pos->second;
                if (!method(line, importer))
                {
                    if (!importer.OnParseError(line, lineno))
                        return false;
                }
            }
            return true;
        }

    } // detail

    // a simple base class that implements the importer's callbacks
    // in a derived class one can hide the ones that are not interesting.
    class ObjImporterBase
    {
    public:
        template<typename T>
        void Import(const T&) {}

        template<typename T>
        void BeginGroup(const T&) {}

        template<typename T>
        void SetAttribute(const T&) {}

        bool OnUnknownIdentifier(const std::string& line, std::size_t lineno)
        { return false; }

        bool OnParseError(const std::string& line, std::size_t lineno)
        { return false; }

    protected:
       ~ObjImporterBase() = default;
    };


    // parse the given input stream for  .obj model data.
    // this is a primitive line based parser and only realizes the syntax of .obj data.
    // there is no semantic validity checking of the parsed data.
    // returns true if parsing was successful, otherwise false.
    template<typename Iterator, typename Importer>
    bool ParseObj(Iterator beg, Iterator end, Importer& importer)
    {
        using ParseMethod = std::function<bool(const std::string&, Importer&)>;

        static const std::map<std::string, ParseMethod> methods {
            {"v",      detail::GenericParse<Importer, detail::ParseActionImport,       Position>},
            {"vn",     detail::GenericParse<Importer, detail::ParseActionImport,       Normal>},
            {"vt",     detail::GenericParse<Importer, detail::ParseActionImport,       TexCoord>},
            {"f",      detail::GenericParse<Importer, detail::ParseActionImport,       Face>},
            {"g",      detail::GenericParse<Importer, detail::ParseActionBeginGroup,   GroupName>},
            {"o",      detail::GenericParse<Importer, detail::ParseActionBeginGroup,   ObjectName>},
            {"mtllib", detail::GenericParse<Importer, detail::ParseActionSetAttribute, MtlLib>},
            {"usemtl", detail::GenericParse<Importer, detail::ParseActionSetAttribute, UseMtl>}
        };
        return detail::ParseLines(beg, end, importer, methods);
    }

    class MtlImporterBase
    {
    public:
        template<typename T>
        void Import(const T&) {}

        template<typename T>
        void BeginMaterial(const T&) {}

    protected:
       ~MtlImporterBase() = default;
    };

    // parse the given input stream for .mtl material data.
    // this is a primitive line based parser and only realizes the syntax of the .mtl data.
    // there is no semantic validity checking.
    // returns true if parsing was successful, otherwise false.
    template<typename Iterator, typename Importer>
    bool ParseMtl(Iterator beg, Iterator end, Importer& importer)
    {
        using ParseMethod = std::function<bool(const std::string&, Importer&)>;

        static const std::map<std::string, ParseMethod> methods {
            {"newmtl", detail::GenericParse<Importer, detail::ParseActionBeginMaterial, NewMtl>},
            {"Ka",     detail::GenericParse<Importer, detail::ParseActionImport, MaterialKa>},
            {"Kd",     detail::GenericParse<Importer, detail::ParseActionImport, MaterialKd>},
            {"Ks",     detail::GenericParse<Importer, detail::ParseActionImport, MaterialKs>},
            {"Ns",     detail::GenericParse<Importer, detail::ParseActionImport, SpecularExponent>},
            {"illum",  detail::GenericParse<Importer, detail::ParseActionImport, Illumination>},
            {"map_Ka", detail::GenericParse<Importer, detail::ParseActionImport, AmbientTextureMap>},
            {"map_Kd", detail::GenericParse<Importer, detail::ParseActionImport, DiffuseTextureMap>}
        };
        return detail::ParseLines(beg, end, importer, methods);
    }

} // wavefront

