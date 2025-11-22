// Copyright (c) 2010-2014 Sami Väisänen, Ensisoft
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

#include "warnpush.h"
#  include <boost/spirit/include/classic.hpp>
#include "warnpop.h"

#if defined(__MSVC__)
// decorated name length exceeded. msvc 2015. 
#  pragma warning(disable: 4503) 
#endif

#include "base/wavefront.h"

using namespace boost::spirit::classic;

namespace wavefront
{
namespace detail
{

bool Parse(const std::string& str, Position& p)
{
    const auto ret = parse(std::begin(str), std::end(str),
        (str_p("v") >>
           real_p[assign(p.x)] >>
           real_p[assign(p.y)] >>
           real_p[assign(p.z)] >>
           !real_p[assign(p.w)]
           ), blank_p);
    return ret.hit;
}

bool Parse(const std::string& str, Normal& n)
{
    const auto ret = parse(std::begin(str), std::end(str),
        (str_p("vn") >>
           real_p[assign(n.x)] >>
           real_p[assign(n.y)] >>
           real_p[assign(n.z)]
           ), blank_p);
    return ret.hit;
}

bool Parse(const std::string& str, TexCoord& st)
{
    const auto ret = parse(std::begin(str), std::end(str),
        (str_p("vt") >>
           real_p[assign(st.x)] >>
           !real_p[assign(st.y)] >>
           !real_p[assign(st.z)]
           ), blank_p);
    return ret.hit;
}

bool Parse(const std::string& str, Face& f)
{
    auto beg = std::begin(str);

    auto ret = parse(beg, std::end(str), str_p("f"));
    if (!ret.hit)
        return false;
    beg = ret.stop;

    const auto end = std::end(str);

    // boost.spirit cannot backtrack on alternatives
    // for example if the parser is (v_p | vn_p)  and input  is "1//2"
    // it will halt on first / instead of backtracking to the start of the
    // alternative and trying another one.
    // so its imperative to make sure that the order of sub parsers solves
    // this problem
    static Vertex v = {0};

    static const auto v_p = (int_p[assign(v.pindex)]);
    static const auto vt_p = (int_p[assign(v.pindex)] >> "/" >> int_p[assign(v.tindex)]);
    static const auto vtn_p = (int_p[assign(v.pindex)] >> "/" >> int_p[assign(v.tindex)] >> "/" >> int_p[assign(v.nindex)]);
    static const auto vn_p = (int_p[assign(v.pindex)] >> "//" >> int_p[assign(v.nindex)]);

    while (beg != end)
    {
        beg = parse(beg, end, (*blank_p)).stop;
        if (beg == end)
            break;

        v = Vertex{0};

        auto ret = parse(beg, end, (vtn_p | vn_p | vt_p | v_p), blank_p);
        if (!ret.hit)
            return false;

        beg = ret.stop;

        f.vertices.push_back(v);
    }
    return !f.vertices.empty();
}

bool Parse(const std::string& str, MaterialKa& ka)
{
    const auto ret = parse(std::begin(str), std::end(str),
        (str_p("Ka") >>
           real_p[assign(ka.r)] >>
           real_p[assign(ka.g)] >>
           real_p[assign(ka.b)]
           ), blank_p);
    return ret.hit;
}

bool Parse(const std::string& str, MaterialKd& kd)
{
    const auto ret = parse(std::begin(str), std::end(str),
        (str_p("Kd") >>
           real_p[assign(kd.r)] >>
           real_p[assign(kd.g)] >>
           real_p[assign(kd.b)]
           ), blank_p);
    return ret.hit;
}

bool Parse(const std::string& str, MaterialKs& ks)
{
    const auto ret = parse(std::begin(str), std::end(str),
        (str_p("Ks") >>
           real_p[assign(ks.r)] >>
           real_p[assign(ks.g)] >>
           real_p[assign(ks.b)]
           ), blank_p);
    return ret.hit;
}

bool Parse(const std::string& str, SpecularExponent& ns)
{
    const auto ret = parse(std::begin(str), std::end(str),
        (str_p("Ns") >>
           real_p[assign(ns.exponent)]), blank_p);
    return ret.hit;
}

bool Parse(const std::string& str, Illumination& illumination)
{
    int value = 0;
    const auto ret = parse(std::begin(str), std::end(str),
        (str_p("illum") >> int_p[assign(value)]), blank_p);

    if (!ret.hit)
        return false;

    if (value < 0 || value > 2)
        return false;

    illumination.model = static_cast<Illumination::Model>(value);
    return true;
}

bool Parse(const std::string& str, std::string& s)
{
    const auto ret = parse(std::begin(str), std::end(str),
        ((str_p("g") |
          str_p("o") |
          str_p("mtllib") |
          str_p("usemtl") |
          str_p("newmtl") |
          str_p("map_Ka") |
          str_p("map_Kd")) >> blank_p >> (*print_p)[assign(s)]));
    return ret.hit;
}

} // detail

} // wavefront
